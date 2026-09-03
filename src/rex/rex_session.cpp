/**
 * @file rex_session.cpp
 * @brief Opaque session + C ABI (load, step, step-over, run, mem, symbols).
 */

#include "rex/rex.h"

#include "dos/dos_cga.h"
#include "dos/dos_machine.h"
#include "rex/rex_disasm.h"
#include "rex/rex_log.h"
#include "tdx/tdx_version.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <unordered_map>

struct rex_session
{
    std::unique_ptr<dos_machine> dos;
    rex_arch arch = REX_ARCH_I8086;
    std::unordered_map<uint64_t, std::string> syms;
    std::string load_path;
    std::string load_cwd;
    bool floppy = false;
    bool floppy_uc = false;
    bool bios = false;
    std::string floppy_a;
    std::string floppy_b;
    std::string guest;
    int ui_cmd = 0;
    uint32_t run_delay_ms = 0;
};

namespace
{
constexpr uint32_t k_run_delay_step_ms = 5;
constexpr uint32_t k_run_delay_max_ms = 200;
} // namespace

const char *rex_version(void)
{
    return REX_VERSION_STRING;
}

static std::string path_basename(const std::string &p)
{
    std::string s = p;
    while ((!s.empty()) && ((s.back() == '/') || (s.back() == '\\')))
    {
        s.pop_back();
    }
    const auto n = s.find_last_of("/\\");
    if (n != std::string::npos)
    {
        s = s.substr(n + 1);
    }
    return s.empty() ? std::string("-") : s;
}

static void refresh_guest(rex_session *s)
{
    if (s == nullptr)
    {
        return;
    }
    if ((s->dos) && (!s->dos->exec_name.empty()))
    {
        s->guest = s->dos->exec_name;
        return;
    }
    s->guest.clear();
}

const char *rex_session_guest(const rex_session *s)
{
    if (s == nullptr)
    {
        return "";
    }
    if ((s->dos) && (!s->dos->exec_name.empty()))
    {
        return s->dos->exec_name.c_str();
    }
    return s->guest.c_str();
}

const char *rex_status_str(rex_status st)
{
    switch (st)
    {
    case REX_OK:
        return "ok";
    case REX_ERR_ARG:
        return "bad argument";
    case REX_ERR_IO:
        return "i/o error";
    case REX_ERR_FMT:
        return "bad format";
    case REX_ERR_CPU:
        return "cpu error";
    case REX_ERR_MEM:
        return "memory error";
    case REX_ERR_NOSYS:
        return "not implemented";
    case REX_ERR_TIMEOUT:
        return "timeout";
    case REX_ERR_BUSY:
        return "busy";
    case REX_ERR_SOCK:
        return "socket error";
    default:
        return "unknown";
    }
}

rex_session *rex_session_create(void)
{
    return new (std::nothrow) rex_session();
}

void rex_session_destroy(rex_session *s)
{
    delete s;
}

rex_status rex_session_load(rex_session *s, const char *path, const char *cwd)
{
    if ((s == nullptr) || (path == nullptr))
    {
        return REX_ERR_ARG;
    }
    s->dos = std::make_unique<dos_machine>();
    s->arch = REX_ARCH_I8086;
    s->load_path = path;
    s->load_cwd = (cwd != nullptr) ? cwd : "";
    s->dos->exec_name = path_basename(path);
    refresh_guest(s);
    return s->dos->load_path(path, cwd);
}

rex_status rex_session_load_floppy(rex_session *s, const char *image)
{
    if ((s == nullptr) || (image == nullptr))
    {
        return REX_ERR_ARG;
    }
    s->dos = std::make_unique<dos_machine>();
    s->arch = REX_ARCH_I8086;
    s->load_path = image;
    s->load_cwd.clear();
    s->floppy = true;
    s->floppy_uc = false;
    s->floppy_a = image;
    s->dos->exec_name.clear();
    refresh_guest(s);
    return s->dos->load_floppy(image);
}

rex_status rex_session_load_floppy_uc(rex_session *s, const char *image)
{
    if ((s == nullptr) || (image == nullptr))
    {
        return REX_ERR_ARG;
    }
    s->dos = std::make_unique<dos_machine>();
    s->arch = REX_ARCH_I8086;
    s->load_path = image;
    s->load_cwd.clear();
    s->floppy = false;
    s->floppy_uc = true;
    s->dos->exec_name.clear();
    refresh_guest(s);
    return s->dos->load_floppy_uc(image);
}

rex_status rex_session_load_bios(rex_session *s, const char *path)
{
    if ((s == nullptr) || (path == nullptr))
    {
        return REX_ERR_ARG;
    }
    s->dos = std::make_unique<dos_machine>();
    s->arch = REX_ARCH_I8086;
    s->load_path = path;
    s->load_cwd.clear();
    s->floppy = false;
    s->floppy_uc = false;
    s->bios = true;
    s->dos->exec_name.clear();
    refresh_guest(s);
    return s->dos->load_bios_5150(path);
}

rex_status rex_session_attach_floppy(rex_session *s, const char *image)
{
    if ((s == nullptr) || (image == nullptr) || (!s->dos))
    {
        return REX_ERR_ARG;
    }
    s->floppy_a = image;
    refresh_guest(s);
    return s->dos->attach_floppy_image(image);
}

rex_status rex_session_attach_floppy_b(rex_session *s, const char *image)
{
    if ((s == nullptr) || (image == nullptr) || (!s->dos))
    {
        return REX_ERR_ARG;
    }
    s->floppy_b = image;
    refresh_guest(s);
    return s->dos->attach_floppy_image_b(image);
}

rex_status rex_session_reset(rex_session *s)
{
    if ((s == nullptr) || s->load_path.empty() || (!s->dos))
    {
        return REX_ERR_ARG;
    }
    const auto bps = s->dos->bps;
    const auto bp_by_id = s->dos->bp_by_id;
    const uint32_t next_id = s->dos->next_bp_id;
    const char *cwd = s->load_cwd.empty() ? nullptr : s->load_cwd.c_str();
    rex_status st =
        s->bios ? s->dos->load_bios_5150(s->load_path.c_str())
                : (s->floppy_uc ? s->dos->load_floppy_uc(s->load_path.c_str())
                                : (s->floppy ? s->dos->load_floppy(s->load_path.c_str())
                                             : s->dos->load_path(s->load_path.c_str(), cwd)));
    if (st != REX_OK)
    {
        return st;
    }
    if (s->bios && (!s->floppy_a.empty()))
    {
        st = s->dos->attach_floppy_image(s->floppy_a.c_str());
        if (st != REX_OK)
        {
            return st;
        }
    }
    if (!s->floppy_b.empty())
    {
        st = s->dos->attach_floppy_image_b(s->floppy_b.c_str());
        if (st != REX_OK)
        {
            return st;
        }
    }
    s->dos->bps = bps;
    s->dos->bp_by_id = bp_by_id;
    s->dos->next_bp_id = next_id;
    rex_logf(REX_LOG_INFO, "reset %s entry linear=0x%llx", s->load_path.c_str(),
             (unsigned long long)s->dos->entry_linear);
    return REX_OK;
}

static dos_machine *need_dos(rex_session *s)
{
    if ((s == nullptr) || (!s->dos))
    {
        return nullptr;
    }
    return s->dos.get();
}

static const dos_machine *need_dos_c(const rex_session *s)
{
    if ((s == nullptr) || (!s->dos))
    {
        return nullptr;
    }
    return s->dos.get();
}

rex_status rex_session_step(rex_session *s)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->vcr_forward(true);
}

void rex_session_vcr_seed(rex_session *s)
{
    dos_machine *m = need_dos(s);
    if (m != nullptr)
    {
        m->vcr_seed();
    }
}

rex_status rex_session_vcr_forward(rex_session *s, bool step_into)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->vcr_forward(step_into);
}

rex_status rex_session_vcr_back(rex_session *s)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->vcr_back();
}

rex_status rex_session_vcr_home(rex_session *s)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->vcr_home();
}

rex_status rex_session_vcr_end(rex_session *s)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->vcr_end();
}

rex_status rex_session_step_over(rex_session *s, uint64_t max_insns)
{
    dos_machine *m = need_dos(s);
    rex_insn ins{};
    size_t n = 0;
    uint64_t fall = 0;

    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    if (dos_machine_disasm(m, UINT64_MAX, &ins, 1, &n) != REX_OK || (n == 0))
    {
        return m->step_one();
    }
    if (!(ins.is_call || ins.is_int || ins.is_rep || ins.is_loop))
    {
        return m->step_one();
    }
    fall = ins.linear + ins.size;
    return m->run_until(fall, (max_insns == 0) ? 10000000ull : max_insns, true);
}

rex_status rex_session_run(rex_session *s, uint64_t max_insns)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    m->pit_poll();
    return m->run_until(0, (max_insns == 0) ? 50000000ull : max_insns, false);
}

rex_status rex_session_request_stop(rex_session *s)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    m->stop_req = true;
    return REX_OK;
}

void rex_session_post_ui_cmd(rex_session *s, int cmd)
{
    if (s != nullptr)
    {
        s->ui_cmd = cmd;
    }
}

int rex_session_take_ui_cmd(rex_session *s)
{
    int cmd = 0;
    if (s == nullptr)
    {
        return 0;
    }
    cmd = s->ui_cmd;
    s->ui_cmd = 0;
    return cmd;
}

uint32_t rex_session_run_delay_ms(const rex_session *s)
{
    return (s != nullptr) ? s->run_delay_ms : 0;
}

void rex_session_set_run_delay_ms(rex_session *s, uint32_t ms)
{
    if (s == nullptr)
    {
        return;
    }
    if (ms > k_run_delay_max_ms)
    {
        ms = k_run_delay_max_ms;
    }
    s->run_delay_ms = ms;
}

uint32_t rex_session_nudge_run_delay(rex_session *s, int dir)
{
    uint32_t ms = 0;
    if (s == nullptr)
    {
        return 0;
    }
    ms = s->run_delay_ms;
    if (dir > 0)
    {
        if (ms > k_run_delay_max_ms - k_run_delay_step_ms)
        {
            ms = k_run_delay_max_ms;
        }
        else
        {
            ms += k_run_delay_step_ms;
        }
    }
    else if (dir < 0)
    {
        if (ms <= k_run_delay_step_ms)
        {
            ms = 0;
        }
        else
        {
            ms -= k_run_delay_step_ms;
        }
    }
    s->run_delay_ms = ms;
    return ms;
}

rex_stop rex_session_stop_reason(const rex_session *s)
{
    const dos_machine *m = need_dos_c(s);
    return (m != nullptr) ? m->last_stop : REX_STOP_NONE;
}

bool rex_session_halted(const rex_session *s)
{
    const dos_machine *m = need_dos_c(s);
    return (m != nullptr) && m->halted;
}

int rex_session_exit_code(const rex_session *s)
{
    const dos_machine *m = need_dos_c(s);
    return (m != nullptr) ? m->exit_code : 0;
}

rex_arch rex_session_arch(const rex_session *s)
{
    return (s != nullptr) ? s->arch : REX_ARCH_NONE;
}

rex_status rex_session_get_regs_i8086(const rex_session *s, rex_regs_i8086 *out)
{
    const dos_machine *m = need_dos_c(s);
    if ((m == nullptr) || (out == nullptr))
    {
        return REX_ERR_ARG;
    }
    m->get_regs(out);
    return REX_OK;
}

rex_status rex_session_set_regs_i8086(rex_session *s, const rex_regs_i8086 *in)
{
    dos_machine *m = need_dos(s);
    if ((m == nullptr) || (in == nullptr))
    {
        return REX_ERR_ARG;
    }
    m->set_regs(in);
    return REX_OK;
}

rex_status rex_session_read_mem(const rex_session *s, uint64_t linear, void *dst, size_t n)
{
    const dos_machine *m = need_dos_c(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->read_mem(linear, dst, n);
}

rex_status rex_session_write_mem(rex_session *s, uint64_t linear, const void *src, size_t n)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->write_mem(linear, src, n);
}

rex_status rex_session_disasm(const rex_session *s, uint64_t linear, rex_insn *out, size_t cap,
                              size_t *wrote)
{
    const dos_machine *m = need_dos_c(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return dos_machine_disasm(m, linear, out, cap, wrote);
}

rex_status rex_bp_add_linear(rex_session *s, uint64_t linear, uint32_t *id)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->bp_add(linear, id, 0, 0);
}

rex_status rex_bp_add_segoff(rex_session *s, uint16_t seg, uint16_t off, uint32_t *id)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->bp_add(rex_segoff_to_linear(seg, off), id, seg, off);
}

rex_status rex_bp_del(rex_session *s, uint32_t id)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->bp_del(id);
}

rex_status rex_bp_del_linear(rex_session *s, uint64_t linear)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    auto it = m->bps.find(linear);
    if (it == m->bps.end())
    {
        return REX_ERR_ARG;
    }
    return m->bp_del(it->second);
}

rex_status rex_bp_clear(rex_session *s)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    m->bp_clear();
    return REX_OK;
}

size_t rex_bp_count(const rex_session *s)
{
    const dos_machine *m = need_dos_c(s);
    return (m != nullptr) ? m->bps.size() : 0;
}

bool rex_bp_at(const rex_session *s, uint64_t linear)
{
    const dos_machine *m = need_dos_c(s);
    size_t i = 0;
    if (m == nullptr)
    {
        return false;
    }
    if (m->bps.find(linear) != m->bps.end())
    {
        return true;
    }
    for (i = 0; i < m->range_bps.size(); i++)
    {
        if ((linear >= m->range_bps[i].lo) && (linear <= m->range_bps[i].hi))
        {
            return true;
        }
    }
    return false;
}

rex_status rex_bp_int(rex_session *s, uint8_t intno)
{
    return rex_bp_int_hits(s, intno, 0);
}

rex_status rex_bp_int_hits(rex_session *s, uint8_t intno, uint32_t hits)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    m->int_bps[intno] = hits;
    return REX_OK;
}

rex_status rex_bp_int_del(rex_session *s, uint8_t intno)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    m->int_bps.erase(intno);
    return REX_OK;
}

size_t rex_int_bp_list(const rex_session *s, rex_int_bp *out, size_t cap)
{
    const dos_machine *m = need_dos_c(s);
    size_t n = 0;
    if ((m == nullptr) || (out == nullptr) || (cap == 0))
    {
        return 0;
    }
    for (const auto &kv : m->int_bps)
    {
        if (n >= cap)
        {
            break;
        }
        out[n].intno = kv.first;
        out[n].remain = kv.second;
        n++;
    }
    return n;
}

rex_status rex_bp_insn(rex_session *s, const char *pat, uint32_t hits, uint32_t *id)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->bp_insn_add(pat, hits, id);
}

size_t rex_insn_bp_list(const rex_session *s, rex_insn_bp *out, size_t cap)
{
    const dos_machine *m = need_dos_c(s);
    size_t n = 0;
    if ((m == nullptr) || (out == nullptr) || (cap == 0))
    {
        return 0;
    }
    for (const auto &e : m->insn_bps)
    {
        if (n >= cap)
        {
            break;
        }
        out[n].id = e.id;
        out[n].remain = e.remain;
        std::snprintf(out[n].text, sizeof(out[n].text), "%s", e.needle.c_str());
        n++;
    }
    return n;
}

rex_status rex_bp_set_hits(rex_session *s, uint32_t id, uint32_t hits)
{
    dos_machine *m = need_dos(s);
    size_t i = 0;
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    if (m->bp_by_id.find(id) != m->bp_by_id.end())
    {
        m->bp_remain[id] = hits;
        return REX_OK;
    }
    for (i = 0; i < m->insn_bps.size(); i++)
    {
        if (m->insn_bps[i].id == id)
        {
            m->insn_bps[i].remain = hits;
            return REX_OK;
        }
    }
    for (i = 0; i < m->range_bps.size(); i++)
    {
        if (m->range_bps[i].id == id)
        {
            m->range_bps[i].remain = hits;
            return REX_OK;
        }
    }
    for (i = 0; i < m->mem_bps.size(); i++)
    {
        if (m->mem_bps[i].id == id)
        {
            m->mem_bps[i].remain = hits;
            return REX_OK;
        }
    }
    return REX_ERR_ARG;
}

rex_status rex_bp_add_range(rex_session *s, uint64_t lo, uint64_t hi, uint32_t hits, uint32_t *id)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->bp_range_add(lo, hi, 0, 0, 0, 0, hits, id);
}

rex_status rex_bp_add_segoff_range(rex_session *s, uint16_t seg0, uint16_t off0, uint16_t seg1,
                                   uint16_t off1, uint32_t hits, uint32_t *id)
{
    dos_machine *m = need_dos(s);
    const uint64_t lo = rex_segoff_to_linear(seg0, off0);
    const uint64_t hi = rex_segoff_to_linear(seg1, off1);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->bp_range_add(lo, hi, seg0, off0, seg1, off1, hits, id);
}

size_t rex_range_bp_list(const rex_session *s, rex_range_bp *out, size_t cap)
{
    const dos_machine *m = need_dos_c(s);
    size_t n = 0;
    if ((m == nullptr) || (out == nullptr) || (cap == 0))
    {
        return 0;
    }
    for (const auto &e : m->range_bps)
    {
        if (n >= cap)
        {
            break;
        }
        out[n].id = e.id;
        out[n].remain = e.remain;
        out[n].seg0 = e.seg0;
        out[n].off0 = e.off0;
        out[n].seg1 = e.seg1;
        out[n].off1 = e.off1;
        out[n].lo = e.lo;
        out[n].hi = e.hi;
        n++;
    }
    return n;
}

rex_status rex_bp_add_write(rex_session *s, uint64_t lo, uint64_t hi, uint32_t hits, uint32_t *id)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->bp_mem_add(lo, hi, 0, 0, 0, 0, hits, id);
}

rex_status rex_bp_add_segoff_write(rex_session *s, uint16_t seg0, uint16_t off0, uint16_t seg1,
                                   uint16_t off1, uint32_t hits, uint32_t *id)
{
    dos_machine *m = need_dos(s);
    const uint64_t lo = rex_segoff_to_linear(seg0, off0);
    const uint64_t hi = rex_segoff_to_linear(seg1, off1);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    return m->bp_mem_add(lo, hi, seg0, off0, seg1, off1, hits, id);
}

size_t rex_mem_bp_list(const rex_session *s, rex_range_bp *out, size_t cap)
{
    const dos_machine *m = need_dos_c(s);
    size_t n = 0;
    if ((m == nullptr) || (out == nullptr) || (cap == 0))
    {
        return 0;
    }
    for (const auto &e : m->mem_bps)
    {
        if (n >= cap)
        {
            break;
        }
        out[n].id = e.id;
        out[n].remain = e.remain;
        out[n].seg0 = e.seg0;
        out[n].off0 = e.off0;
        out[n].seg1 = e.seg1;
        out[n].off1 = e.off1;
        out[n].lo = e.lo;
        out[n].hi = e.hi;
        n++;
    }
    return n;
}

size_t rex_bp_list(const rex_session *s, rex_bp *out, size_t cap)
{
    const dos_machine *m = need_dos_c(s);
    size_t n = 0;
    if ((m == nullptr) || (out == nullptr) || (cap == 0))
    {
        return 0;
    }
    for (const auto &kv : m->bp_by_id)
    {
        rex_bp b{};
        uint32_t so = 0;
        if (n >= cap)
        {
            break;
        }
        b.id = kv.first;
        b.linear = kv.second;
        auto it = m->bp_segoff.find(b.id);
        so = (it != m->bp_segoff.end()) ? it->second : 0;
        b.seg = (uint16_t)(so >> 16);
        b.off = (uint16_t)(so & 0xFFFFu);
        if ((b.seg == 0) && (b.off == 0) && (b.linear != 0))
        {
            b.seg = (uint16_t)(b.linear >> 4);
            b.off = (uint16_t)(b.linear - ((uint32_t)b.seg << 4));
        }
        out[n++] = b;
    }
    return n;
}

rex_status rex_symbols_load(rex_session *s, const char *path)
{
    std::ifstream in;
    std::string line;
    if ((s == nullptr) || (path == nullptr))
    {
        return REX_ERR_ARG;
    }
    in.open(path);
    if (!in)
    {
        return REX_ERR_IO;
    }
    while (std::getline(in, line))
    {
        if (line.empty() || (line[0] == '#'))
        {
            continue;
        }
        std::string addr;
        std::string name;
        const auto tab = line.find('\t');
        const auto sp = line.find(' ');
        const auto sep = (tab != std::string::npos) ? tab : sp;
        if (sep == std::string::npos)
        {
            continue;
        }
        addr = line.substr(0, sep);
        name = line.substr(sep + 1);
        while ((!name.empty()) && ((name[0] == ' ') || (name[0] == '\t')))
        {
            name.erase(name.begin());
        }
        {
            unsigned seg = 0;
            unsigned off = 0;
            unsigned long lin = 0;
            if (std::sscanf(addr.c_str(), "%x:%x", &seg, &off) == 2)
            {
                s->syms[rex_segoff_to_linear((uint16_t)seg, (uint16_t)off)] = name;
            }
            else if (std::sscanf(addr.c_str(), "%lx", &lin) == 1)
            {
                s->syms[lin] = name;
            }
        }
    }
    rex_logf(REX_LOG_INFO, "symbols loaded %zu from %s", s->syms.size(), path);
    return REX_OK;
}

const char *rex_symbols_lookup(const rex_session *s, uint64_t linear)
{
    if (s == nullptr)
    {
        return nullptr;
    }
    auto it = s->syms.find(linear);
    if (it == s->syms.end())
    {
        return nullptr;
    }
    return it->second.c_str();
}

rex_status rex_session_push_key(rex_session *s, uint8_t ascii, uint8_t scan)
{
    dos_machine *m = need_dos(s);
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    m->push_key(ascii, scan);
    return REX_OK;
}

uint8_t rex_session_video_mode(const rex_session *s)
{
    const dos_machine *m = need_dos_c(s);
    return (m != nullptr) ? m->video_mode : 0;
}

rex_status rex_session_cga_decode(const rex_session *s, uint8_t *px, size_t px_size)
{
    const dos_machine *m = need_dos_c(s);
    uint8_t vram[DOS_CGA_VRAM];
    if (m == nullptr)
    {
        return REX_ERR_ARG;
    }
    if (m->read_mem(0xB8000, vram, sizeof(vram)) != REX_OK)
    {
        return REX_ERR_MEM;
    }
    if (dos_cga_decode(vram, px, px_size) != 0)
    {
        return REX_ERR_ARG;
    }
    return REX_OK;
}

const char *rex_session_dos_cwd(const rex_session *s)
{
    const dos_machine *m = need_dos_c(s);
    return (m != nullptr) ? m->dos_cwd.c_str() : "";
}

uint64_t rex_session_entry_linear(const rex_session *s)
{
    const dos_machine *m = need_dos_c(s);
    return (m != nullptr) ? m->entry_linear : 0;
}

const char *rex_session_con_out(const rex_session *s)
{
    const dos_machine *m = need_dos_c(s);
    return (m != nullptr) ? m->con_out.c_str() : "";
}
