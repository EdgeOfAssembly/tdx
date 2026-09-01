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
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
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
    m->prev_code_linear = m->last_code_linear;
    m->last_code_linear = address;
    m->code_ring[m->code_ring_i % 8u] = address;
    m->code_ring_i++;
    if (m->stop_req)
    {
        uc_emu_stop(uc);
        m->last_stop = REX_STOP_REQUEST;
        return;
    }
    /* 8086 cadence: tick the 8253 once per 2 guest instructions. */
    m->tick_pit(1);
    /* Instruction-boundary INTR check (real hardware samples INTR between
     * instructions) — but only when not in the one-instruction inhibit window
     * after STI / POP SS / MOV SS,reg (8086 semantics). A pending, unmasked IRQ
     * with IF=1 stops this burst so run_until can deliver it at a clean
     * boundary before re-entering Unicorn. Do NOT touch last_stop/stop_req:
     * this is an internal micro-stop, transparent to the debugger UI. */
    if (m->intr_inhibit != 0u)
    {
        m->intr_inhibit--; /* the previous instruction armed it; consume here */
    }
    else if ((m->intr_depth == 0u) && (m->pic.pending()) &&
             ((m->reg16(UC_X86_REG_FLAGS) & 0x0200u) != 0u))
    {
        uc_emu_stop(uc);
        return;
    }
    /* Track STI/POP-SS/MOV-SS inhibit for the *next* instruction, and IRET that
     * unwinds a hardware ISR (restores IF and leaves intr_depth). */
    if ((m->ram != nullptr) && (address + 1u < m->ram_size))
    {
        const uint8_t op = m->ram[address];
        if (op == 0xCFu /* IRET */ && (m->intr_depth != 0u))
        {
            m->intr_depth--; /* after this IRET the ISR returns, IF restored */
        }
        else if (op == 0xFBu /* STI */ || op == 0x17u /* POP SS */ ||
                 (op == 0x8Eu && ((m->ram[address + 1u] >> 3) & 7u) == 2u) /* MOV SS,r/m */)
        {
            m->intr_inhibit = 1u;
        }
    }
    if (m->bps.find(address) == m->bps.end())
    {
        return;
    }
    if (address == m->run_ignore_bp)
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
    assert(m != nullptr);
    if ((address >= 0xB8000ull) && (address < 0xC0000ull))
    {
        m->video_dirty = true;
    }
    if ((m->vcr_rec) && (m->ram != nullptr) && (size > 0))
    {
        int i = 0;
        for (i = 0; i < size; i++)
        {
            const uint64_t a = address + (uint64_t)i;
            vcr_delta d{};
            if (a >= m->ram_size)
            {
                break;
            }
            d.lin = (uint32_t)a;
            d.oldv = m->ram[a];
            d.newv = (uint8_t)((value >> (8 * i)) & 0xFFu);
            m->vcr_pending.push_back(d);
        }
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
        /* Fault, not program exit — keep last_stop as FAULT so the UI does
         * not paint "terminated" (that string is INT 20 / AH=4C only). */
        m->last_stop = REX_STOP_FAULT;
        uc_emu_stop(uc);
    }
    return false;
}

uint32_t on_in(uc_engine *uc, uint32_t port, int size, void *user)
{
    dos_machine *m = static_cast<dos_machine *>(user);
    (void)uc;
    (void)size;
    if (m != nullptr)
    {
        if ((port >= 0x40u) && (port <= 0x43u))
        {
            return m->pit.read(port);
        }
        if (port == 0x20u)
        {
            return m->pic.read_command();
        }
        if (port == 0x21u)
        {
            return m->pic.read_data();
        }
        /* CGA CRT status 03DAh: bit0 display-enable, bit3 vertical retrace.
         * Toggle every IN so "wait for retrace / wait for display" loops
         * complete in two reads (0xFF stuck high hangs those loops). */
        if ((port == 0x3DAu) || (port == 0x3BAu))
        {
            m->cga_3da ^= 0x09u;
            return m->cga_3da;
        }
    }
    return 0xFFu;
}

void on_out(uc_engine *uc, uint32_t port, int size, uint32_t value, void *user)
{
    dos_machine *m = static_cast<dos_machine *>(user);
    (void)uc;
    (void)size;
    if (m == nullptr)
    {
        (void)value;
        return;
    }
    if ((port == 0x3D8u) || (port == 0x3D9u) || (port == 0x3D4u))
    {
        m->video_dirty = true;
    }
    else if ((port >= 0x40u) && (port <= 0x43u))
    {
        m->pit.write(port, (uint8_t)value);
    }
    else if (port == 0x20u)
    {
        m->pic.write_command((uint8_t)value);
    }
    else if (port == 0x21u)
    {
        m->pic.write_data((uint8_t)value);
    }
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
    uint32_t lin = 0;
    if ((eip >= base) && ((eip - base) <= 0xFFFFu))
    {
        ip = (uint16_t)(eip - base);
        lin = eip;
    }
    else
    {
        ip = (uint16_t)eip;
        lin = base + (uint32_t)ip;
    }
    uc_reg_write(uc, UC_X86_REG_IP, &ip);
    uc_reg_write(uc, UC_X86_REG_EIP, &lin);
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
    video_mode = 0x03;
    blank_regen();

    /* Plant a minimal BIOS default INT 08h (timer tick) + INT 1Ch (user tick)
     * handler, as a real BIOS ROM would. tdx skips POST, so without this the
     * IVT is all zeros and a hardware IRQ would vector to 0000:0000. The stub
     * lives in the BIOS ROM hole at F000:0000 (linear 0xF0000). 9FC0:0000 is
     * EBDA / top of conventional RAM — Bushido reuses that region.
     *
     * INT 08h handler:
     *   inc dword [0040:006C]     ; 18.2 Hz tick dword
     *   (rollover sets 0070h)
     *   int 1Ch                    ; chain the user tick
     *   mov al,20h ; out 20h,al    ; non-specific EOI to the 8259 PIC
     *   iret
     * INT 1Ch handler: iret
     */
    {
        const uint16_t seg = 0xF000;             /* BIOS ROM hole, not conventional RAM */
        uint8_t *p = ram + ((uint32_t)seg << 4);
        size_t i = 0;
        /* ---- INT 08h (IF stays 0 until IRET; DS must be 0000 for BDA) ---- */
        p[i++] = 0x50;                           /* push ax */
        p[i++] = 0x1E;                           /* push ds */
        p[i++] = 0x33; p[i++] = 0xC0;            /* xor ax,ax */
        p[i++] = 0x8E; p[i++] = 0xD8;            /* mov ds,ax */
        p[i++] = 0xFF; p[i++] = 0x06; p[i++] = 0x6C; p[i++] = 0x04; /* inc word [046C] */
        p[i++] = 0x75; p[i++] = 0x04;            /* jnz +4 */
        p[i++] = 0xFF; p[i++] = 0x06; p[i++] = 0x6E; p[i++] = 0x04; /* inc word [046E] */
        p[i++] = 0x1F;                           /* pop ds */
        p[i++] = 0x58;                           /* pop ax */
        p[i++] = 0xCD; p[i++] = 0x1C;            /* int 1Ch */
        p[i++] = 0xB0; p[i++] = 0x20;            /* mov al,20h */
        p[i++] = 0xE6; p[i++] = 0x20;            /* out 20h,al (EOI) */
        p[i++] = 0xCF;                           /* iret */
        const uint16_t off08 = 0;
        const uint16_t off1c = (uint16_t)i;      /* ---- INT 1Ch ---- */
        p[i++] = 0xCF;                           /* iret */
        /* IVT[08] -> seg:off08, IVT[1C] -> seg:off1c */
        ram[0x08u * 4 + 0] = (uint8_t)(off08);
        ram[0x08u * 4 + 1] = (uint8_t)(off08 >> 8);
        ram[0x08u * 4 + 2] = (uint8_t)(seg);
        ram[0x08u * 4 + 3] = (uint8_t)(seg >> 8);
        ram[0x1Cu * 4 + 0] = (uint8_t)(off1c);
        ram[0x1Cu * 4 + 1] = (uint8_t)(off1c >> 8);
        ram[0x1Cu * 4 + 2] = (uint8_t)(seg);
        ram[0x1Cu * 4 + 3] = (uint8_t)(seg >> 8);
    }

    e = uc_hook_add(uc, &hh, UC_HOOK_INTR, (void *)on_intr, this, 1, 0);
    hh_intr = hh;
    e = uc_hook_add(uc, &hh, UC_HOOK_CODE, (void *)on_code, this, 0, k_ram - 1);
    (void)e;
    e = uc_hook_add(uc, &hh, UC_HOOK_MEM_WRITE, (void *)on_write, this, 0, k_ram - 1);
    hh_memw = hh;
    e = uc_hook_add(uc, &hh, UC_HOOK_MEM_UNMAPPED, (void *)on_unmapped, this, 1, 0);
    hh_unmapped = hh;
    e = uc_hook_add(uc, &hh, UC_HOOK_INSN, (void *)on_in, this, 1, 0, UC_X86_INS_IN);
    e = uc_hook_add(uc, &hh, UC_HOOK_INSN, (void *)on_out, this, 1, 0, UC_X86_INS_OUT);
    (void)e;

    /* tdx runs no BIOS POST: put the emulated 8259 PIC + 8253 PIT in the state
     * every booted DOS machine has, so programs (and their timer ISRs) see a
     * live IRQ0 system tick without needing to reprogram the chipset. */
    pic.pc_boot_state();
    pit.pc_boot_state();
    pit_ticks_acc = 0;
    intr_inhibit = 0;
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
    {
        int fi = 0;
        for (fi = 0; fi < DOS_MAX_FILES; fi++)
        {
            close_handle(fi);
        }
        kbd.clear();
        con_out.clear();
        halted = false;
        wait_key = false;
        stop_req = false;
    }
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
        image_base = 0x10100;
        image_bytes = (uint32_t)file.size();
        /* DOS gives a COM the rest of conventional memory (max_alloc style). */
        mem_end = DOS_MEM_END_PARA;
        build_psp(ram + 0x10000, (uint16_t)mem_end, DOS_ENV_SEG);
        cs = DOS_PSP_SEG;
        ip = 0x0100;
        ss = DOS_PSP_SEG;
        sp = 0xFFFE;
        alloc_bump = DOS_MEM_END_PARA;
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
        image_base = load_lin;
        image_bytes = info.image_size;
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
        /* DOS 5: one MCB from PSP through min(image+max_alloc, 640 KiB).
         * BASCOM reads PSP[2] before INT 21; a tight image-sized block makes
         * it RETF to PSP:0000 (INT 20) without ever calling SETBLOCK. */
        {
            const uint32_t image_end =
                (uint32_t)DOS_LOAD_SEG + ((info.image_size + 15u) / 16u) + (uint32_t)info.min_alloc;
            if ((info.max_alloc == 0xFFFFu) ||
                ((image_end + (uint32_t)info.max_alloc) >= (uint32_t)DOS_MEM_END_PARA))
            {
                mem_end = DOS_MEM_END_PARA;
            }
            else
            {
                mem_end = image_end + (uint32_t)info.max_alloc;
            }
            if (mem_end < (image_end + 1u))
            {
                mem_end = image_end + 1u;
            }
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
    skip_int_bp = false;
    last_stop = REX_STOP_NONE;
    video_mode = 0x03;
    blank_regen();
    sync_ip_from_eip();
    rebuild_decode();
    vcr_seed();
    return REX_OK;
}

void dos_machine::blank_regen(void)
{
    uint32_t i = 0;
    if (ram == nullptr)
    {
        return;
    }
    /* PCBIOS SET_MODE: graphics regen is zeros; alpha is ' ' + attribute 07. */
    if ((video_mode == 0x04) || (video_mode == 0x05) || (video_mode == 0x06) ||
        (video_mode == 0x0D) || (video_mode == 0x13))
    {
        std::memset(ram + 0xB8000, 0, 0x8000);
    }
    else
    {
        for (i = 0; i < 80u * 25u; i++)
        {
            ram[0xB8000u + i * 2u] = (uint8_t)' ';
            ram[0xB8000u + i * 2u + 1u] = 0x07;
        }
    }
    video_dirty = true;
}

uint64_t dos_machine::linear_ip() const
{
    const uint32_t eip = eip32();
    const uint16_t cs = reg16(UC_X86_REG_CS);
    const uint32_t base = (uint32_t)cs << 4;
    if ((eip >= base) && ((eip - base) <= 0xFFFFu))
    {
        return (uint64_t)eip;
    }
    return (uint64_t)base + (uint64_t)(eip & 0xFFFFu);
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
        const uint64_t lin = linear_ip();
        const uint32_t base = (uint32_t)out->cs << 4;
        out->ip = (lin >= base) ? (uint16_t)(lin - base) : (uint16_t)lin;
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
    /* A pending hardware IRQ makes the code hook stop the 1-insn execution
     * before it runs, leaving IP frozen. Deliver the interrupt first so the
     * step actually executes the (ISR entry) instruction it lands on. */
    if (irq_pending_if_on())
    {
        deliver_pending_irq();
        last_stop = REX_STOP_STEP;
        return REX_OK;
    }
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
    {
        const uint64_t start_lin = linear_ip();
        if (skip_bp || until_valid)
        {
            run_ignore_bp = start_lin;
        }
        else
        {
            run_ignore_bp = UINT64_MAX;
        }
    }
    while ((left > 0) && (!halted) && (!at_break) && (!wait_key) && (!stop_req))
    {
        const uint64_t lin = linear_ip();
        const size_t chunk = (left > 50000ull) ? 50000u : (size_t)left;
        uc_err e = UC_ERR_OK;
        /* Deliver a pending hardware IRQ (timer/keyboard) to the guest through
         * its IVT at this instruction boundary — exactly like real 8086
         * hardware checks INTR between instructions. The burst below runs the
         * ISR and its IRET, restoring the stack and returning here. */
        if (irq_pending_if_on())
        {
            deliver_pending_irq();
            continue; /* recompute lin; we just vectored into the ISR */
        }
        if (lin != run_ignore_bp)
        {
            run_ignore_bp = UINT64_MAX;
        }
        if ((bps.find(lin) != bps.end()) && (lin != run_ignore_bp) && (!skip_bp))
        {
            at_break = true;
            skip_bp = true;
            last_stop = REX_STOP_BREAK;
            break;
        }
        skip_bp = false;
        e = uc_emu_start(uc, lin, until, 0, chunk);
        sync_ip_from_eip();
        /* Unicorn IRET may not hit on_code with opcode CF (prefix / EIP). If
         * we have left the hardware ISR, drop intr_depth so IRQ0 can fire
         * again — otherwise the guest timer countdown freezes. */
        if (intr_depth != 0u)
        {
            const uint16_t isr_off =
                (uint16_t)(ram[0x20] | ((uint16_t)ram[0x21] << 8));
            const uint16_t isr_seg =
                (uint16_t)(ram[0x22] | ((uint16_t)ram[0x23] << 8));
            const uint32_t isr_lin = ((uint32_t)isr_seg << 4) + (uint32_t)isr_off;
            const uint64_t now = linear_ip();
            if ((now < isr_lin) || (now > (uint64_t)isr_lin + 0x80ull))
            {
                intr_depth = 0;
            }
        }
        left -= (left < 50000ull) ? left : 50000ull;
        if (until_valid && (linear_ip() == until_linear))
        {
            run_ignore_bp = UINT64_MAX;
            last_stop = REX_STOP_STEP;
            return REX_OK;
        }
        if ((e != UC_ERR_OK) && (!halted) && (!at_break) && (!wait_key))
        {
            const uint64_t flin = linear_ip();
            const uint16_t fcs = reg16(UC_X86_REG_CS);
            const uint32_t fbase = (uint32_t)fcs << 4;
            const uint16_t fip = (uint16_t)((flin >= fbase) ? (flin - fbase) : flin);
            uint8_t b[4] = {0, 0, 0, 0};
            (void)read_mem(flin, b, sizeof(b));
            rex_logf(REX_LOG_ERROR,
                     "run: %s @ %04X:%04X lin=0x%llX prev=0x%llX  %02X %02X %02X %02X",
                     uc_strerror(e), fcs, fip, (unsigned long long)flin,
                     (unsigned long long)prev_code_linear, b[0], b[1], b[2], b[3]);
            {
                unsigned k = 0;
                for (k = 0; k < 8u; k++)
                {
                    const uint64_t a = code_ring[(code_ring_i + k) % 8u];
                    rex_logf(REX_LOG_ERROR, "  ring[%u]=0x%llX", k, (unsigned long long)a);
                }
            }
            last_stop = REX_STOP_FAULT;
            return REX_ERR_CPU;
        }
        (void)e;
    }
    run_ignore_bp = UINT64_MAX;
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

rex_status dos_machine::bp_add(uint64_t linear, uint32_t *id, uint16_t seg, uint16_t off)
{
    const uint32_t nid = next_bp_id++;
    bps[linear] = nid;
    bp_by_id[nid] = linear;
    bp_segoff[nid] = ((uint32_t)seg << 16) | (uint32_t)off;
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
    bp_segoff.erase(id);
    return REX_OK;
}

void dos_machine::bp_clear(void)
{
    bps.clear();
    bp_by_id.clear();
    bp_segoff.clear();
}

void dos_machine::vcr_seed(void)
{
    vcr_frame f{};
    vcr_pending.clear();
    vcr_tape.clear();
    get_regs(&f.regs);
    f.video_mode = video_mode;
    f.halted = halted;
    vcr_tape.push_back(std::move(f));
    vcr_pos = 0;
    vcr_rec = false;
}

rex_status dos_machine::vcr_back(void)
{
    size_t i = 0;
    if ((vcr_pos == 0) || vcr_tape.empty())
    {
        return REX_OK;
    }
    for (i = 0; i < vcr_tape[vcr_pos].undos.size(); i++)
    {
        const vcr_delta &d = vcr_tape[vcr_pos].undos[i];
        if ((ram != nullptr) && ((size_t)d.lin < ram_size))
        {
            ram[d.lin] = d.oldv;
        }
    }
    vcr_pos--;
    set_regs(&vcr_tape[vcr_pos].regs);
    video_mode = vcr_tape[vcr_pos].video_mode;
    halted = vcr_tape[vcr_pos].halted;
    wait_key = false;
    video_dirty = true;
    last_stop = REX_STOP_STEP;
    return REX_OK;
}

rex_status dos_machine::vcr_end(void)
{
    while ((vcr_pos + 1u) < vcr_tape.size())
    {
        size_t i = 0;
        vcr_pos++;
        for (i = 0; i < vcr_tape[vcr_pos].undos.size(); i++)
        {
            const vcr_delta &d = vcr_tape[vcr_pos].undos[i];
            if ((ram != nullptr) && ((size_t)d.lin < ram_size))
            {
                ram[d.lin] = d.newv;
            }
        }
        set_regs(&vcr_tape[vcr_pos].regs);
        video_mode = vcr_tape[vcr_pos].video_mode;
        halted = vcr_tape[vcr_pos].halted;
    }
    video_dirty = true;
    last_stop = REX_STOP_STEP;
    return REX_OK;
}

rex_status dos_machine::vcr_home(void)
{
    while (vcr_pos > 0)
    {
        const rex_status st = vcr_back();
        if (st != REX_OK)
        {
            return st;
        }
    }
    return REX_OK;
}

rex_status dos_machine::vcr_forward(bool step_into)
{
    vcr_frame f{};
    rex_status st = REX_OK;
    constexpr size_t k_cap = 8192;

    if (vcr_tape.empty())
    {
        vcr_seed();
    }
    if ((vcr_pos + 1u) < vcr_tape.size())
    {
        size_t i = 0;
        vcr_pos++;
        for (i = 0; i < vcr_tape[vcr_pos].undos.size(); i++)
        {
            const vcr_delta &d = vcr_tape[vcr_pos].undos[i];
            if ((ram != nullptr) && ((size_t)d.lin < ram_size))
            {
                ram[d.lin] = d.newv;
            }
        }
        set_regs(&vcr_tape[vcr_pos].regs);
        video_mode = vcr_tape[vcr_pos].video_mode;
        halted = vcr_tape[vcr_pos].halted;
        video_dirty = true;
        last_stop = REX_STOP_STEP;
        return REX_OK;
    }

    vcr_pending.clear();
    vcr_rec = true;
    if (step_into)
    {
        st = step_one();
    }
    else
    {
        rex_insn ins{};
        size_t n = 0;
        if ((dos_machine_disasm(this, UINT64_MAX, &ins, 1, &n) != REX_OK) || (n == 0))
        {
            st = step_one();
        }
        else if (!(ins.is_call || ins.is_int || ins.is_rep || ins.is_loop))
        {
            st = step_one();
        }
        else
        {
            st = run_until(ins.linear + ins.size, 10000000ull, true);
        }
    }
    vcr_rec = false;
    get_regs(&f.regs);
    f.video_mode = video_mode;
    f.halted = halted;
    f.undos = std::move(vcr_pending);
    vcr_pending.clear();
    vcr_tape.push_back(std::move(f));
    vcr_pos = vcr_tape.size() - 1u;
    while (vcr_tape.size() > k_cap)
    {
        vcr_tape.pop_front();
        if (vcr_pos > 0)
        {
            vcr_pos--;
        }
    }
    video_dirty = true;
    return st;
}

void dos_machine::pit_poll(void)
{
    /* Wall-clock tick advance (BIOS 046Ch dword + cadence for the tdx UI).
     * The real work that unstucks DOS games is done by deliver_pending_irq()
     * via the emulated 8259 PIC + 8253 PIT below, ticked by guest instruction
     * count. Here we only keep the BIOS data-area tick fresh for programs that
     * read it directly. */
    struct timespec now{};
    uint64_t ns = 0;
    uint32_t t = 0;
    if (ram == nullptr)
    {
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &now);
    ns = (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
    if (pit_last_ns == 0)
    {
        pit_last_ns = ns;
        return;
    }
    if (ns - pit_last_ns < 55000000ull)
    {
        return;
    }
    pit_last_ns = ns;
    t = (uint32_t)ram[0x46C] | ((uint32_t)ram[0x46D] << 8) | ((uint32_t)ram[0x46E] << 16) |
        ((uint32_t)ram[0x46F] << 24);
    t += 1u;
    ram[0x46C] = (uint8_t)(t);
    ram[0x46D] = (uint8_t)(t >> 8);
    ram[0x46E] = (uint8_t)(t >> 16);
    ram[0x46F] = (uint8_t)(t >> 24);
}

/* Advance the 8253 PIT by guest instruction count (Py86 cadence: one PIT tick
 * per 2 guest insns). Terminal count on ch0 asserts IRQ0 through the 8259 PIC.
 * Unicorn runs at full host speed, so wall-clock pacing (the old pit_poll)
 * could not make timer-driven delay loops expire; instruction-cadence can. */
void dos_machine::tick_pit(size_t insns)
{
    pit_ticks_acc += (uint32_t)insns;
    while (pit_ticks_acc >= 2u)
    {
        pit_ticks_acc -= 2u;
        pit.tick();
    }
}

/* True when the guest can take a maskable interrupt right now: an unmasked IRQ
 * is pending in the PIC, the guest has IF=1, and we are not in the one-
 * instruction inhibit window after STI / POP SS / MOV SS (8086 semantics). */
bool dos_machine::irq_pending_if_on(void) const
{
    if ((uc == nullptr) || (ram == nullptr) || halted)
    {
        return false;
    }
    if (intr_depth != 0u)
    {
        return false; /* already inside a hardware ISR — no nesting */
    }
    if (intr_inhibit != 0u)
    {
        return false;
    }
    const uint16_t flags = reg16(UC_X86_REG_FLAGS);
    if ((flags & 0x0200u) == 0u)
    {
        return false;
    }
    if (!pic.pending())
    {
        return false;
    }
    /* Never vector through a null IVT entry: before the game installs its own
     * ISR the table is all zeros (we skip BIOS POST), so delivering now would
     * jump to 0000:0000. Real hardware would go to a BIOS stub; we emulate one
     * (init_cpu plants IVT[08] = iret) so pending holds a sane fallback. */
    const uint8_t v = pic.peek_vector();
    if (v == 0xFFu)
    {
        return false;
    }
    const uint16_t fseg = (uint16_t)(ram[((uint32_t)v * 4u + 2u) & 0xFFFFF] |
                                     ((uint16_t)ram[((uint32_t)v * 4u + 3u) & 0xFFFFF] << 8));
    const uint16_t foff = (uint16_t)(ram[((uint32_t)v * 4u) & 0xFFFFF] |
                                     ((uint16_t)ram[((uint32_t)v * 4u + 1u) & 0xFFFFF] << 8));
    if ((fseg == 0u) && (foff == 0u))
    {
        return false; /* null vector: hold the IRQ until the game hooks it */
    }
    return true;
}

/* Perform a CPU INTA cycle against the PIC and dispatch the interrupt the way
 * real 8086 hardware does: push FLAGS/CS/IP, clear IF+TF, load CS:IP from the
 * IVT. The matching guest IRET pops the frame, keeping the stack balanced. */
void dos_machine::deliver_pending_irq(void)
{
    if (!irq_pending_if_on())
    {
        return;
    }
    const uint8_t v = pic.peek_vector();
    if (v == 0xFFu)
    {
        return;
    }
    const uint16_t seg = (uint16_t)(ram[((uint32_t)v * 4u + 2u) & 0xFFFFF] |
                                    ((uint16_t)ram[((uint32_t)v * 4u + 3u) & 0xFFFFF] << 8));
    const uint16_t off = (uint16_t)(ram[((uint32_t)v * 4u) & 0xFFFFF] |
                                    ((uint16_t)ram[((uint32_t)v * 4u + 1u) & 0xFFFFF] << 8));
    /* Default BIOS stub at F000:0000: do the tick in C. Jumping into Unicorn
     * IRET after a hooked INT 1Ch left CS stuck executing 00 00. */
    if ((v == 0x08u) && (seg == 0xF000u) && (off == 0u))
    {
        uint32_t t = 0;
        (void)pic.ack_vector();
        t = (uint32_t)ram[0x46C] | ((uint32_t)ram[0x46D] << 8) | ((uint32_t)ram[0x46E] << 16) |
            ((uint32_t)ram[0x46F] << 24);
        t += 1u;
        ram[0x46C] = (uint8_t)(t);
        ram[0x46D] = (uint8_t)(t >> 8);
        ram[0x46E] = (uint8_t)(t >> 16);
        ram[0x46F] = (uint8_t)(t >> 24);
        pic.write_command(0x20); /* EOI */
        return;
    }
    sync_ip_from_eip();
    const uint16_t ss = reg16(UC_X86_REG_SS);
    uint16_t sp = reg16(UC_X86_REG_SP);
    const uint16_t cs = reg16(UC_X86_REG_CS);
    const uint32_t csbase = (uint32_t)cs << 4;
    const uint64_t rlin = linear_ip();
    const uint16_t ip = (uint16_t)((rlin >= csbase) ? (rlin - csbase) : rlin);
    const uint16_t flags = reg16(UC_X86_REG_FLAGS);

    sp = (uint16_t)(sp - 6u);
    const uint32_t sp_lin = (((uint32_t)ss << 4) + (uint32_t)sp);
    if ((sp_lin + 6u) > ram_size)
    {
        return;
    }
    /* Frame fits: commit to the INTA cycle and consume the request. */
    (void)pic.ack_vector();

    /* 8086 pushes FLAGS, CS, IP — lowest address holds the return IP. */
    ram[sp_lin + 0] = (uint8_t)(ip);
    ram[sp_lin + 1] = (uint8_t)(ip >> 8);
    ram[sp_lin + 2] = (uint8_t)(cs);
    ram[sp_lin + 3] = (uint8_t)(cs >> 8);
    ram[sp_lin + 4] = (uint8_t)(flags);
    ram[sp_lin + 5] = (uint8_t)(flags >> 8);
    set_reg16(UC_X86_REG_SP, sp);
    set_reg16(UC_X86_REG_CS, seg);
    set_reg16(UC_X86_REG_IP, off);
    set_reg16(UC_X86_REG_FLAGS, (uint16_t)(flags & ~0x0300u));
    intr_depth++; /* inside a hardware ISR until its IRET returns */
    rex_logf(REX_LOG_INFO, "IRQ%u frame %04X:%04X lin=0x%llX -> %04X:%04X sp=%04X depth=%u",
             (unsigned)(v - 8), cs, ip, (unsigned long long)rlin, seg, off, sp, intr_depth);
}

void dos_machine::rebuild_decode(void)
{
    std::vector<rex_insn> tmp;
    size_t wrote = 0;
    size_t i = 0;
    uint16_t seg = 0;
    uint16_t off = 0;
    decode.clear();
    if ((ram == nullptr) || (image_bytes == 0) || (image_base + image_bytes > ram_size))
    {
        return;
    }
    tmp.resize(65536);
    seg = (uint16_t)(image_base >> 4);
    off = (uint16_t)(image_base - ((uint32_t)seg << 4));
    if (rex_disasm_block(REX_ARCH_I8086, image_base, seg, off, ram + image_base, image_bytes,
                         tmp.data(), tmp.size(), &wrote) != REX_OK)
    {
        rex_logf(REX_LOG_ERROR, "decode map empty (Capstone)");
        return;
    }
    for (i = 0; i < wrote; i++)
    {
        decode[tmp[i].linear] = tmp[i];
    }
    rex_logf(REX_LOG_INFO, "decode map %zu insns @ %llX+%u (Capstone once)", wrote,
             (unsigned long long)image_base, image_bytes);
}

rex_status dos_machine_disasm(const dos_machine *m, uint64_t linear, rex_insn *out, size_t cap,
                              size_t *wrote)
{
    uint16_t cs = 0;
    uint32_t base = 0;
    uint64_t lin = linear;
    size_t n = 0;

    if ((m == nullptr) || (out == nullptr) || (cap == 0))
    {
        return REX_ERR_ARG;
    }
    if (wrote != nullptr)
    {
        *wrote = 0;
    }
    cs = m->reg16(UC_X86_REG_CS);
    base = (uint32_t)cs << 4;
    if (lin == UINT64_MAX)
    {
        lin = m->linear_ip();
    }
    while (n < cap)
    {
        auto it = m->decode.find(lin);
        rex_insn one{};
        if (it == m->decode.end())
        {
            uint8_t buf[32];
            size_t w = 0;
            uint16_t seg = cs;
            uint16_t off = 0;
            if ((lin >= base) && (lin < base + 0x10000u))
            {
                off = (uint16_t)(lin - base);
            }
            else
            {
                seg = (uint16_t)(lin >> 4);
                off = (uint16_t)(lin - ((uint32_t)seg << 4));
            }
            if (m->read_mem(lin, buf, sizeof(buf)) != REX_OK)
            {
                break;
            }
            if (rex_disasm_block(REX_ARCH_I8086, lin, seg, off, buf, sizeof(buf), &one, 1, &w) !=
                    REX_OK ||
                (w == 0) || (one.size == 0))
            {
                /* Capstone hole (e.g. FF /7). Keep a one-byte db so the CPU
                 * pane never goes blank at a fault IP. */
                std::memset(&one, 0, sizeof(one));
                one.linear = lin;
                one.seg = seg;
                one.off = off;
                one.size = 1;
                one.bytes[0] = buf[0];
                std::snprintf(one.text, sizeof(one.text), "db %02X", buf[0]);
            }
            const_cast<dos_machine *>(m)->decode[lin] = one;
            it = m->decode.find(lin);
        }
        out[n] = it->second;
        if ((out[n].linear >= base) && (out[n].linear < base + 0x10000u))
        {
            out[n].seg = cs;
            out[n].off = (uint16_t)(out[n].linear - base);
        }
        lin = out[n].linear + (uint64_t)out[n].size;
        n++;
        if (it->second.size == 0)
        {
            break;
        }
    }
    if (wrote != nullptr)
    {
        *wrote = n;
    }
    return (n > 0) ? REX_OK : REX_ERR_CPU;
}
