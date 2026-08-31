/**
 * @file dos_machine.cpp
 * @brief Unicorn 8086 real-mode CPU, MZ/COM load, step/run, breakpoints.
 */

#include "dos/dos_machine.h"

#include "rex/rex_disasm.h"
#include "rex/rex_log.h"

#include <unicorn/unicorn.h>
#include <unicorn/x86.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include <stdlib.h>

namespace
{
constexpr uint64_t k_ram = static_cast<uint64_t>(DOS_RAM_SIZE);

void on_intr(uc_engine *uc, uint32_t intno, void *user)
{
    dos_machine *m = static_cast<dos_machine *>(user);
    (void)uc;
    assert(m != nullptr);
    m->handle_intr(intno);
}

void on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user)
{
    dos_machine *m = static_cast<dos_machine *>(user);
    (void)size;
    assert(m != nullptr);
    if (m->stop_req)
    {
        uc_emu_stop(uc);
        m->last_stop = REX_STOP_REQUEST;
        return;
    }
    if (m->bps.find(address) == m->bps.end())
    {
        return;
    }
    if (m->skip_bp)
    {
        m->skip_bp = false;
        return;
    }
    m->at_break = true;
    m->skip_bp = true; /* next continue executes this insn */
    m->last_stop = REX_STOP_BREAK;
    uc_emu_stop(uc);
}

void on_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                void *user)
{
    dos_machine *m = static_cast<dos_machine *>(user);
    (void)uc;
    (void)type;
    (void)size;
    (void)value;
    assert(m != nullptr);
    if ((address >= 0xB8000ull) && (address < 0xC0000ull))
    {
        m->video_dirty = true;
    }
}

bool on_unmapped(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                   void *user)
{
    dos_machine *m = static_cast<dos_machine *>(user);
    (void)uc;
    (void)type;
    (void)size;
    (void)value;
    rex_logf(REX_LOG_ERROR, "unmapped mem access @ 0x%llx", (unsigned long long)address);
    if (m != nullptr)
    {
        m->halted = true;
        m->last_stop = REX_STOP_FAULT;
    }
    return false;
}

uint32_t on_in(uc_engine *uc, uint32_t port, int size, void *user)
{
    (void)uc;
    (void)port;
    (void)size;
    (void)user;
    return 0xFFu;
}

void on_out(uc_engine *uc, uint32_t port, int size, uint32_t value, void *user)
{
    dos_machine *m = static_cast<dos_machine *>(user);
    (void)uc;
    (void)size;
    if ((m != nullptr) && ((port == 0x3D8u) || (port == 0x3D9u) || (port == 0x3D4u)))
    {
        m->video_dirty = true;
    }
    (void)value;
}

void build_psp(uint8_t *psp, uint16_t mem_end_para, uint16_t env_seg)
{
    assert(psp != nullptr);
    std::memset(psp, 0, 256);
    psp[0] = 0xCD;
    psp[1] = 0x20;
    psp[2] = (uint8_t)(mem_end_para & 0xFFu);
    psp[3] = (uint8_t)(mem_end_para >> 8);
    psp[0x2C] = (uint8_t)(env_seg & 0xFFu);
    psp[0x2D] = (uint8_t)(env_seg >> 8);
    psp[0x80] = 0; /* empty command tail */
    psp[0x81] = 0x0D;
}

void build_env(uint8_t *env)
{
    static const char k_env[] = "PATH=C:\\";
    assert(env != nullptr);
    std::memset(env, 0, 256);
    std::memcpy(env, k_env, sizeof(k_env)); /* includes NUL */
    env[sizeof(k_env)] = 0;
    env[sizeof(k_env) + 1] = 1;
    env[sizeof(k_env) + 2] = 0;
    std::memcpy(env + sizeof(k_env) + 3, "C:\\TDX.COM", 11);
}
} // namespace

dos_machine::~dos_machine()
{
    int i = 0;
    if (uc != nullptr)
    {
        uc_close(uc);
        uc = nullptr;
    }
    if (ram != nullptr)
    {
        std::free(ram);
        ram = nullptr;
    }
    for (i = 0; i < DOS_MAX_FILES; i++)
    {
        close_handle(i);
    }
}

uint16_t dos_machine::reg16(int uc_reg) const
{
    uint16_t v = 0;
    assert(uc != nullptr);
    uc_reg_read(uc, uc_reg, &v);
    return v;
}

void dos_machine::set_reg16(int uc_reg, uint16_t v)
{
    assert(uc != nullptr);
    uc_reg_write(uc, uc_reg, &v);
    /* Unicorn 16-bit stores a linear EIP; keep it in sync with CS:IP. */
    if ((uc_reg == UC_X86_REG_IP) || (uc_reg == UC_X86_REG_CS))
    {
        const uint16_t cs = (uc_reg == UC_X86_REG_CS) ? v : reg16(UC_X86_REG_CS);
        const uint16_t ip = (uc_reg == UC_X86_REG_IP) ? v : reg16(UC_X86_REG_IP);
        uint32_t eip = ((uint32_t)cs << 4) + (uint32_t)ip;
        uc_reg_write(uc, UC_X86_REG_EIP, &eip);
    }
}

uint32_t dos_machine::eip32(void) const
{
    uint32_t eip = 0;
    assert(uc != nullptr);
    uc_reg_read(uc, UC_X86_REG_EIP, &eip);
    return eip;
}

void dos_machine::sync_ip_from_eip(void)
{
    const uint32_t eip = eip32();
    const uint16_t cs = reg16(UC_X86_REG_CS);
    const uint32_t base = (uint32_t)cs << 4;
    uint16_t ip = 0;
    if (eip >= base)
    {
        ip = (uint16_t)(eip - base);
    }
    else
    {
        ip = (uint16_t)eip;
    }
    /* Write IP only (do not recompute EIP from the stale offset). */
    uc_reg_write(uc, UC_X86_REG_IP, &ip);
}

void dos_machine::set_cf(bool carry)
{
    uint16_t f = reg16(UC_X86_REG_FLAGS);
    if (carry)
    {
        f = (uint16_t)(f | 1u);
    }
    else
    {
        f = (uint16_t)(f & ~1u);
    }
    set_reg16(UC_X86_REG_FLAGS, f);
}

void dos_machine::set_zf(bool zero)
{
    uint16_t f = reg16(UC_X86_REG_FLAGS);
    if (zero)
    {
        f = (uint16_t)(f | 0x40u);
    }
    else
    {
        f = (uint16_t)(f & ~0x40u);
    }
    set_reg16(UC_X86_REG_FLAGS, f);
}

uint8_t *dos_machine::ptr_segoff(uint16_t seg, uint16_t off)
{
    const uint32_t lin = rex_segoff_to_linear(seg, off);
    assert(ram != nullptr);
    if (lin >= ram_size)
    {
        return ram;
    }
    return ram + lin;
}

rex_status dos_machine::init_cpu()
{
    uc_err e = UC_ERR_OK;
    uc_hook hh = 0;
    uint8_t *aligned = nullptr;

    if (ram != nullptr)
    {
        std::free(ram);
        ram = nullptr;
    }
    if (uc != nullptr)
    {
        uc_close(uc);
        uc = nullptr;
    }
    ram_size = (size_t)k_ram;
    aligned = static_cast<uint8_t *>(aligned_alloc(4096, ram_size));
    if (aligned == nullptr)
    {
        return REX_ERR_MEM;
    }
    ram = aligned;
    std::memset(ram, 0, ram_size);

    e = uc_open(UC_ARCH_X86, UC_MODE_16, &uc);
    if (e != UC_ERR_OK)
    {
        rex_logf(REX_LOG_ERROR, "uc_open: %s", uc_strerror(e));
        return REX_ERR_CPU;
    }
    e = uc_mem_map_ptr(uc, 0, ram_size, UC_PROT_ALL, ram);
    if (e != UC_ERR_OK)
    {
        rex_logf(REX_LOG_ERROR, "uc_mem_map_ptr: %s", uc_strerror(e));
        return REX_ERR_MEM;
    }

    ram[0x413] = 0x80; /* 640 KiB in BDA */
    ram[0x414] = 0x02;
    ram[0x449] = 0x03; /* text 80x25 */
    ram[0x44A] = 80;
    ram[0x44B] = 0;

    e = uc_hook_add(uc, &hh, UC_HOOK_INTR, (void *)on_intr, this, 1, 0);
    hh_intr = hh;
    e = uc_hook_add(uc, &hh, UC_HOOK_CODE, (void *)on_code, this, 0, k_ram - 1);
    (void)e;
    e = uc_hook_add(uc, &hh, UC_HOOK_MEM_WRITE, (void *)on_write, this, 0xB8000, 0xBFFFF);
    hh_memw = hh;
    e = uc_hook_add(uc, &hh, UC_HOOK_MEM_UNMAPPED, (void *)on_unmapped, this, 1, 0);
    hh_unmapped = hh;
    e = uc_hook_add(uc, &hh, UC_HOOK_INSN, (void *)on_in, this, 1, 0, UC_X86_INS_IN);
    e = uc_hook_add(uc, &hh, UC_HOOK_INSN, (void *)on_out, this, 1, 0, UC_X86_INS_OUT);
    (void)e;
    return REX_OK;
}

rex_status dos_machine::load_path(const char *path, const char *cwd)
{
    std::ifstream in;
    std::vector<uint8_t> file;
    mz_info info{};
    int pr = 0;
    uint16_t cs = 0;
    uint16_t ip = 0;
    uint16_t ss = 0;
    uint16_t sp = 0;
    uint16_t ds = DOS_PSP_SEG;
    uint16_t flags = 0x0202;
    uint16_t ax = 0;
    uint32_t mem_end = 0;

    assert(path != nullptr);
    if (init_cpu() != REX_OK)
    {
        return REX_ERR_CPU;
    }
    image_path = path;
    if ((cwd != nullptr) && (cwd[0] != '\0'))
    {
        dos_cwd = cwd;
    }
    else
    {
        const char *slash = std::strrchr(path, '/');
        dos_cwd = (slash != nullptr) ? std::string(path, slash) : std::string(".");
    }

    in.open(path, std::ios::binary);
    if (!in)
    {
        rex_logf(REX_LOG_ERROR, "cannot open %s", path);
        return REX_ERR_IO;
    }
    file.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (file.empty())
    {
        return REX_ERR_FMT;
    }

    pr = mz_parse_header(file.data(), file.size(), (uint32_t)file.size(), &info);
    is_com = (pr == 1);
    if (pr < 0)
    {
        return REX_ERR_FMT;
    }

    build_env(ram + ((uint32_t)DOS_ENV_SEG << 4));
    files[1].used = true; /* stdout dummy */
    files[2].used = true;

    if (is_com)
    {
        if (file.size() > 0xFF00u)
        {
            return REX_ERR_FMT;
        }
        std::memcpy(ram + 0x10100, file.data(), file.size());
        mem_end = DOS_PSP_SEG + 0x1000;
        build_psp(ram + 0x10000, (uint16_t)mem_end, DOS_ENV_SEG);
        cs = DOS_PSP_SEG;
        ip = 0x0100;
        ss = DOS_PSP_SEG;
        sp = 0xFFFE;
        alloc_bump = DOS_PSP_SEG + 0x1000;
        entry_linear = 0x10100;
        rex_logf(REX_LOG_INFO, "loaded COM %s size=%zu entry=%04X:%04X", path, file.size(), cs, ip);
    }
    else
    {
        mz_reloc rels[DOS_MAX_RELOCS];
        size_t nrel = 0;
        uint32_t load_lin = (uint32_t)DOS_LOAD_SEG << 4;
        size_t i = 0;
        if (mz_parse_relocs(file.data(), file.size(), &info, rels, DOS_MAX_RELOCS, &nrel) != 0)
        {
            return REX_ERR_FMT;
        }
        if ((load_lin + info.image_size) > ram_size)
        {
            return REX_ERR_MEM;
        }
        std::memcpy(ram + load_lin, file.data() + info.header_bytes, info.image_size);
        for (i = 0; i < nrel; i++)
        {
            const uint32_t at = load_lin + rex_segoff_to_linear(rels[i].seg, rels[i].off);
            uint16_t w = 0;
            if ((at + 2u) > ram_size)
            {
                return REX_ERR_FMT;
            }
            w = (uint16_t)(ram[at] | ((uint16_t)ram[at + 1] << 8));
            w = (uint16_t)(w + DOS_LOAD_SEG);
            ram[at] = (uint8_t)(w & 0xFFu);
            ram[at + 1] = (uint8_t)(w >> 8);
        }
        cs = (uint16_t)(info.cs + DOS_LOAD_SEG);
        ip = info.ip;
        ss = (uint16_t)(info.ss + DOS_LOAD_SEG);
        sp = info.sp;
        mem_end = DOS_LOAD_SEG + ((info.image_size + 15u) / 16u) + info.min_alloc + 1u;
        if (mem_end < (DOS_LOAD_SEG + 0x1000u))
        {
            mem_end = DOS_LOAD_SEG + 0x1000u;
        }
        build_psp(ram + 0x10000, (uint16_t)mem_end, DOS_ENV_SEG);
        alloc_bump = mem_end;
        entry_linear = rex_segoff_to_linear(cs, ip);
        rex_logf(REX_LOG_INFO, "loaded MZ %s image=%u relocs=%zu entry=%04X:%04X ss:sp=%04X:%04X",
                 path, info.image_size, nrel, cs, ip, ss, sp);
    }

    set_reg16(UC_X86_REG_CS, cs);
    set_reg16(UC_X86_REG_IP, ip);
    set_reg16(UC_X86_REG_SS, ss);
    set_reg16(UC_X86_REG_SP, sp);
    set_reg16(UC_X86_REG_DS, ds);
    set_reg16(UC_X86_REG_ES, ds);
    set_reg16(UC_X86_REG_AX, ax);
    set_reg16(UC_X86_REG_BX, 0);
    set_reg16(UC_X86_REG_CX, 0);
    set_reg16(UC_X86_REG_DX, 0);
    set_reg16(UC_X86_REG_SI, 0);
    set_reg16(UC_X86_REG_DI, 0);
    set_reg16(UC_X86_REG_BP, 0);
    set_reg16(UC_X86_REG_FLAGS, flags);
    halted = false;
    wait_key = false;
    at_break = false;
    skip_bp = false;
    last_stop = REX_STOP_NONE;
    video_mode = 0x03;
    return REX_OK;
}

uint64_t dos_machine::linear_ip() const
{
    return (uint64_t)eip32();
}

void dos_machine::get_regs(rex_regs_i8086 *out) const
{
    assert(out != nullptr);
    out->ax = reg16(UC_X86_REG_AX);
    out->bx = reg16(UC_X86_REG_BX);
    out->cx = reg16(UC_X86_REG_CX);
    out->dx = reg16(UC_X86_REG_DX);
    out->si = reg16(UC_X86_REG_SI);
    out->di = reg16(UC_X86_REG_DI);
    out->bp = reg16(UC_X86_REG_BP);
    out->sp = reg16(UC_X86_REG_SP);
    out->cs = reg16(UC_X86_REG_CS);
    out->ds = reg16(UC_X86_REG_DS);
    out->es = reg16(UC_X86_REG_ES);
    out->ss = reg16(UC_X86_REG_SS);
    {
        const uint32_t eip = eip32();
        const uint32_t base = (uint32_t)out->cs << 4;
        out->ip = (eip >= base) ? (uint16_t)(eip - base) : (uint16_t)eip;
    }
    out->flags = reg16(UC_X86_REG_FLAGS);
}

void dos_machine::set_regs(const rex_regs_i8086 *in)
{
    assert(in != nullptr);
    set_reg16(UC_X86_REG_AX, in->ax);
    set_reg16(UC_X86_REG_BX, in->bx);
    set_reg16(UC_X86_REG_CX, in->cx);
    set_reg16(UC_X86_REG_DX, in->dx);
    set_reg16(UC_X86_REG_SI, in->si);
    set_reg16(UC_X86_REG_DI, in->di);
    set_reg16(UC_X86_REG_BP, in->bp);
    set_reg16(UC_X86_REG_SP, in->sp);
    set_reg16(UC_X86_REG_CS, in->cs);
    set_reg16(UC_X86_REG_DS, in->ds);
    set_reg16(UC_X86_REG_ES, in->es);
    set_reg16(UC_X86_REG_SS, in->ss);
    set_reg16(UC_X86_REG_IP, in->ip);
    set_reg16(UC_X86_REG_FLAGS, in->flags);
}

rex_status dos_machine::read_mem(uint64_t linear, void *dst, size_t n) const
{
    if ((dst == nullptr) || (ram == nullptr))
    {
        return REX_ERR_ARG;
    }
    if ((linear + n) > ram_size)
    {
        return REX_ERR_MEM;
    }
    std::memcpy(dst, ram + linear, n);
    return REX_OK;
}

rex_status dos_machine::write_mem(uint64_t linear, const void *src, size_t n)
{
    if ((src == nullptr) || (ram == nullptr))
    {
        return REX_ERR_ARG;
    }
    if ((linear + n) > ram_size)
    {
        return REX_ERR_MEM;
    }
    std::memcpy(ram + linear, src, n);
    if ((linear < 0xC0000ull) && ((linear + n) > 0xB8000ull))
    {
        video_dirty = true;
    }
    return REX_OK;
}

rex_status dos_machine::step_one()
{
    const uint64_t lin = linear_ip();
    uc_err e = UC_ERR_OK;

    if (halted)
    {
        last_stop = REX_STOP_HALTED;
        return REX_OK;
    }
    if ((bps.find(lin) != bps.end()) && (!skip_bp))
    {
        at_break = true;
        skip_bp = true;
        last_stop = REX_STOP_BREAK;
        return REX_OK;
    }
    at_break = false;
    wait_key = false;
    e = uc_emu_start(uc, lin, 0, 0, 1);
    sync_ip_from_eip();
    if (!at_break)
    {
        skip_bp = false;
    }
    if (halted)
    {
        last_stop = REX_STOP_HALTED;
        return REX_OK;
    }
    if (wait_key)
    {
        last_stop = REX_STOP_WAIT_KEY;
        return REX_OK;
    }
    if (at_break)
    {
        last_stop = REX_STOP_BREAK;
        return REX_OK;
    }
    if (e != UC_ERR_OK)
    {
        rex_logf(REX_LOG_ERROR, "step: %s @ 0x%llx", uc_strerror(e), (unsigned long long)lin);
        last_stop = REX_STOP_FAULT;
        return REX_ERR_CPU;
    }
    last_stop = REX_STOP_STEP;
    if (bps.find(linear_ip()) != bps.end())
    {
        at_break = true;
        skip_bp = true;
        last_stop = REX_STOP_BREAK;
    }
    return REX_OK;
}

rex_status dos_machine::run_until(uint64_t until_linear, uint64_t max_insns, bool until_valid)
{
    uint64_t left = (max_insns == 0) ? 10000000ull : max_insns;
    const uint64_t until = until_valid ? until_linear : 0;

    if (halted)
    {
        last_stop = REX_STOP_HALTED;
        return REX_OK;
    }
    stop_req = false;
    at_break = false;
    while ((left > 0) && (!halted) && (!at_break) && (!wait_key) && (!stop_req))
    {
        const uint64_t lin = linear_ip();
        const size_t chunk = (left > 50000ull) ? 50000u : (size_t)left;
        uc_err e = UC_ERR_OK;
        if ((bps.find(lin) != bps.end()) && (!skip_bp))
        {
            at_break = true;
            skip_bp = true;
            last_stop = REX_STOP_BREAK;
            break;
        }
        e = uc_emu_start(uc, lin, until, 0, chunk);
        sync_ip_from_eip();
        left -= (left < 50000ull) ? left : 50000ull;
        if (until_valid && (linear_ip() == until_linear))
        {
            last_stop = REX_STOP_STEP;
            return REX_OK;
        }
        if ((e != UC_ERR_OK) && (!halted) && (!at_break) && (!wait_key))
        {
            rex_logf(REX_LOG_ERROR, "run: %s", uc_strerror(e));
            last_stop = REX_STOP_FAULT;
            return REX_ERR_CPU;
        }
        (void)e;
    }
    if (halted)
    {
        last_stop = REX_STOP_HALTED;
    }
    else if (wait_key)
    {
        last_stop = REX_STOP_WAIT_KEY;
    }
    else if (at_break)
    {
        last_stop = REX_STOP_BREAK;
    }
    else if (stop_req)
    {
        last_stop = REX_STOP_REQUEST;
    }
    else if (left == 0)
    {
        last_stop = REX_STOP_TIMEOUT;
    }
    return REX_OK;
}

void dos_machine::push_key(uint8_t ascii, uint8_t scan)
{
    dos_kbd_ev ev{};
    ev.ascii = ascii;
    ev.scan = (scan != 0) ? scan : ascii;
    kbd.push_back(ev);
    wait_key = false;
}

rex_status dos_machine::bp_add(uint64_t linear, uint32_t *id)
{
    const uint32_t nid = next_bp_id++;
    bps[linear] = nid;
    bp_by_id[nid] = linear;
    if (id != nullptr)
    {
        *id = nid;
    }
    return REX_OK;
}

rex_status dos_machine::bp_del(uint32_t id)
{
    auto it = bp_by_id.find(id);
    if (it == bp_by_id.end())
    {
        return REX_ERR_ARG;
    }
    bps.erase(it->second);
    bp_by_id.erase(it);
    return REX_OK;
}

void dos_machine::bp_clear(void)
{
    bps.clear();
    bp_by_id.clear();
}

rex_status dos_machine_disasm(const dos_machine *m, uint64_t linear, rex_insn *out, size_t cap,
                              size_t *wrote)
{
    uint8_t buf[64];
    uint16_t seg = 0;
    uint16_t off = 0;
    uint64_t lin = linear;

    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    if (lin == UINT64_MAX)
    {
        seg = m->reg16(UC_X86_REG_CS);
        off = m->reg16(UC_X86_REG_IP);
        lin = rex_segoff_to_linear(seg, off);
    }
    else
    {
        seg = (uint16_t)((lin >> 4) & 0xF000); /* not unique; keep 0 if unknown */
        off = (uint16_t)(lin & 0xF);
        seg = (uint16_t)(lin >> 4);
        off = (uint16_t)(lin - ((uint32_t)seg << 4));
        /* Prefer CS-relative when in the same paragraph window. */
        {
            const uint16_t cs = m->reg16(UC_X86_REG_CS);
            const uint32_t base = (uint32_t)cs << 4;
            if ((lin >= base) && (lin < base + 0x10000u))
            {
                seg = cs;
                off = (uint16_t)(lin - base);
            }
        }
    }
    if (m->read_mem(lin, buf, sizeof(buf)) != REX_OK)
    {
        return REX_ERR_MEM;
    }
    return rex_disasm_block(REX_ARCH_I8086, lin, seg, off, buf, sizeof(buf), out, cap, wrote);
}
