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
    int ui_cmd = 0;
};

const char *rex_version(void)
{
    return REX_VERSION_STRING;
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
    return s->dos->load_path(path, cwd);
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
    const rex_status st = s->dos->load_path(s->load_path.c_str(), cwd);
    if (st != REX_OK)
    {
        return st;
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
    return m->step_one();
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
    return m->bp_add(linear, id);
}

rex_status rex_bp_add_segoff(rex_session *s, uint16_t seg, uint16_t off, uint32_t *id)
{
    return rex_bp_add_linear(s, rex_segoff_to_linear(seg, off), id);
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
    return (m != nullptr) && (m->bps.find(linear) != m->bps.end());
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
