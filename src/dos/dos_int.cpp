/**
 * @file dos_int.cpp
 * @brief Tiny DOS 5 / CGA BIOS interrupt handlers for the Unicorn machine.
 */

#include "dos/dos_machine.h"

#include "rex/rex_log.h"

#include <unicorn/unicorn.h>
#include <unicorn/x86.h>

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

void dos_machine::write_dos_string(const char *text)
{
    if (text == nullptr)
    {
        return;
    }
    con_out.append(text);
}

std::string dos_machine::dos_to_host_path(const char *dos_path) const
{
    std::string s = (dos_path != nullptr) ? dos_path : "";
    size_t i = 0;
    if ((s.size() >= 2) && (s[1] == ':'))
    {
        s = s.substr(2);
    }
    if ((!s.empty()) && ((s[0] == '\\') || (s[0] == '/')))
    {
        s = s.substr(1);
    }
    for (i = 0; i < s.size(); i++)
    {
        if (s[i] == '\\')
        {
            s[i] = '/';
        }
    }
    if (s.empty())
    {
        return dos_cwd;
    }
    if (s[0] == '/')
    {
        return s;
    }
    return dos_cwd + "/" + s;
}

int dos_machine::alloc_handle(void)
{
    int i = 5;
    for (i = 5; i < DOS_MAX_FILES; i++)
    {
        if (!files[i].used)
        {
            files[i].used = true;
            files[i].fp = nullptr;
            return i;
        }
    }
    return -1;
}

void dos_machine::close_handle(int fd)
{
    if ((fd < 0) || (fd >= DOS_MAX_FILES))
    {
        return;
    }
    if ((files[fd].fp != nullptr) && (fd >= 5))
    {
        fclose(files[fd].fp);
        files[fd].fp = nullptr;
    }
    files[fd].used = false;
}

static void rewind_software_int(dos_machine *m, uint16_t nbytes)
{
    const uint16_t ip = m->reg16(UC_X86_REG_IP);
    m->set_reg16(UC_X86_REG_IP, (uint16_t)(ip - nbytes));
    uc_emu_stop(m->uc);
}

static void handle_int21(dos_machine *m)
{
    const uint16_t ax = m->reg16(UC_X86_REG_AX);
    const uint8_t ah = (uint8_t)(ax >> 8);
    const uint8_t al = (uint8_t)(ax & 0xFFu);
    const uint16_t bx = m->reg16(UC_X86_REG_BX);
    const uint16_t cx = m->reg16(UC_X86_REG_CX);
    const uint16_t dx = m->reg16(UC_X86_REG_DX);
    const uint16_t ds = m->reg16(UC_X86_REG_DS);
    const uint16_t es = m->reg16(UC_X86_REG_ES);
    const uint16_t si = m->reg16(UC_X86_REG_SI);

    switch (ah)
    {
    case 0x00:
        m->halted = true;
        m->exit_code = 0;
        uc_emu_stop(m->uc);
        break;
    case 0x02:
    {
        char ch[2] = {(char)(dx & 0xFFu), 0};
        m->write_dos_string(ch);
        break;
    }
    case 0x06:
        if (al == 0xFFu)
        {
            if (m->kbd.empty())
            {
                m->set_zf(true);
            }
            else
            {
                const dos_kbd_ev ev = m->kbd.front();
                m->kbd.pop_front();
                m->set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | ev.ascii));
                m->set_zf(false);
            }
        }
        else
        {
            char ch[2] = {(char)al, 0};
            m->write_dos_string(ch);
        }
        break;
    case 0x07:
    case 0x08:
    case 0x01:
        if (m->kbd.empty())
        {
            m->wait_key = true;
            rewind_software_int(m, 2);
        }
        else
        {
            const dos_kbd_ev ev = m->kbd.front();
            m->kbd.pop_front();
            m->set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | ev.ascii));
        }
        break;
    case 0x09:
    {
        const uint8_t *p = m->ptr_segoff(ds, dx);
        char buf[256];
        size_t n = 0;
        while ((n + 1u) < sizeof(buf) && (p[n] != '$') && (p[n] != 0))
        {
            buf[n] = (char)p[n];
            n++;
        }
        buf[n] = 0;
        m->write_dos_string(buf);
        break;
    }
    case 0x0B:
        m->set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | (m->kbd.empty() ? 0x00u : 0xFFu)));
        break;
    case 0x0C:
        m->kbd.clear();
        m->set_reg16(UC_X86_REG_AX, (uint16_t)(ax & 0xFF00u));
        break;
    case 0x19:
        m->set_reg16(UC_X86_REG_AX, (uint16_t)(ax & 0xFF00u)); /* drive A=0, report C=2 */
        m->set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | 2u));
        break;
    case 0x1A:
        m->dta = rex_segoff_to_linear(ds, dx);
        break;
    case 0x25:
    {
        const uint32_t ivt = (uint32_t)al * 4u;
        const uint16_t off = dx;
        const uint16_t seg = ds;
        m->ram[ivt] = (uint8_t)(off & 0xFFu);
        m->ram[ivt + 1] = (uint8_t)(off >> 8);
        m->ram[ivt + 2] = (uint8_t)(seg & 0xFFu);
        m->ram[ivt + 3] = (uint8_t)(seg >> 8);
        break;
    }
    case 0x2A:
    {
        const time_t t = time(nullptr);
        const struct tm *tm = localtime(&t);
        uint16_t cxv = 2026;
        uint16_t dxv = 0;
        if (tm != nullptr)
        {
            cxv = (uint16_t)(tm->tm_year + 1900);
            dxv = (uint16_t)(((tm->tm_mon + 1) << 8) | tm->tm_mday);
        }
        m->set_reg16(UC_X86_REG_CX, cxv);
        m->set_reg16(UC_X86_REG_DX, dxv);
        break;
    }
    case 0x2C:
    {
        const time_t t = time(nullptr);
        const struct tm *tm = localtime(&t);
        uint16_t cxv = 0;
        uint16_t dxv = 0;
        if (tm != nullptr)
        {
            cxv = (uint16_t)((tm->tm_hour << 8) | tm->tm_min);
            dxv = (uint16_t)((tm->tm_sec << 8));
        }
        m->set_reg16(UC_X86_REG_CX, cxv);
        m->set_reg16(UC_X86_REG_DX, dxv);
        break;
    }
    case 0x2F:
        m->set_reg16(UC_X86_REG_ES, (uint16_t)(m->dta >> 4));
        m->set_reg16(UC_X86_REG_BX, (uint16_t)(m->dta & 0xF));
        break;
    case 0x30:
        m->set_reg16(UC_X86_REG_AX, 0x0005); /* DOS 5.00 */
        m->set_reg16(UC_X86_REG_BX, 0);
        m->set_reg16(UC_X86_REG_CX, 0);
        break;
    case 0x33:
        m->set_reg16(UC_X86_REG_DX, 0);
        break;
    case 0x35:
    {
        const uint32_t ivt = (uint32_t)al * 4u;
        const uint16_t off = (uint16_t)(m->ram[ivt] | ((uint16_t)m->ram[ivt + 1] << 8));
        const uint16_t seg = (uint16_t)(m->ram[ivt + 2] | ((uint16_t)m->ram[ivt + 3] << 8));
        m->set_reg16(UC_X86_REG_BX, off);
        m->set_reg16(UC_X86_REG_ES, seg);
        break;
    }
    case 0x3C:
    case 0x3D:
    {
        char dpath[128];
        size_t n = 0;
        const uint8_t *p = m->ptr_segoff(ds, dx);
        const int fd = m->alloc_handle();
        const char *mode = (ah == 0x3C) ? "w+b" : ((al == 0) ? "rb" : ((al == 1) ? "wb" : "r+b"));
        FILE *fp = nullptr;
        std::string host;
        while ((n + 1u) < sizeof(dpath) && (p[n] != 0))
        {
            dpath[n] = (char)p[n];
            n++;
        }
        dpath[n] = 0;
        host = m->dos_to_host_path(dpath);
        if (fd < 0)
        {
            m->set_cf(true);
            m->set_reg16(UC_X86_REG_AX, 4);
            break;
        }
        fp = fopen(host.c_str(), mode);
        if ((fp == nullptr) && (ah == 0x3D) && (al != 0))
        {
            fp = fopen(host.c_str(), "w+b");
        }
        if (fp == nullptr)
        {
            m->close_handle(fd);
            m->set_cf(true);
            m->set_reg16(UC_X86_REG_AX, 2);
            rex_logf(REX_LOG_INFO, "INT21 open fail %s", host.c_str());
            break;
        }
        m->files[fd].fp = fp;
        m->set_cf(false);
        m->set_reg16(UC_X86_REG_AX, (uint16_t)fd);
        rex_logf(REX_LOG_DEBUG, "INT21 open %s -> %d", host.c_str(), fd);
        break;
    }
    case 0x3E:
        if ((bx < DOS_MAX_FILES) && m->files[bx].used)
        {
            m->close_handle((int)bx);
            m->set_cf(false);
        }
        else
        {
            m->set_cf(true);
            m->set_reg16(UC_X86_REG_AX, 6);
        }
        break;
    case 0x3F:
    {
        if ((bx >= DOS_MAX_FILES) || (!m->files[bx].used) || (m->files[bx].fp == nullptr))
        {
            m->set_cf(true);
            m->set_reg16(UC_X86_REG_AX, 6);
            break;
        }
        {
            std::vector<uint8_t> tmp(cx);
            const size_t got = fread(tmp.data(), 1, (size_t)cx, m->files[bx].fp);
            if (got > 0)
            {
                std::memcpy(m->ptr_segoff(ds, dx), tmp.data(), got);
            }
            m->set_cf(false);
            m->set_reg16(UC_X86_REG_AX, (uint16_t)got);
        }
        break;
    }
    case 0x40:
    {
        const uint8_t *src = m->ptr_segoff(ds, dx);
        if ((bx == 1) || (bx == 2))
        {
            m->con_out.append(reinterpret_cast<const char *>(src), (size_t)cx);
            m->set_cf(false);
            m->set_reg16(UC_X86_REG_AX, cx);
            break;
        }
        if ((bx >= DOS_MAX_FILES) || (!m->files[bx].used) || (m->files[bx].fp == nullptr))
        {
            m->set_cf(true);
            m->set_reg16(UC_X86_REG_AX, 6);
            break;
        }
        {
            const size_t put = fwrite(src, 1, (size_t)cx, m->files[bx].fp);
            m->set_cf(false);
            m->set_reg16(UC_X86_REG_AX, (uint16_t)put);
        }
        break;
    }
    case 0x41:
    {
        char dpath[128];
        size_t n = 0;
        const uint8_t *p = m->ptr_segoff(ds, dx);
        while ((n + 1u) < sizeof(dpath) && p[n])
        {
            dpath[n] = (char)p[n];
            n++;
        }
        dpath[n] = 0;
        if (remove(m->dos_to_host_path(dpath).c_str()) == 0)
        {
            m->set_cf(false);
        }
        else
        {
            m->set_cf(true);
            m->set_reg16(UC_X86_REG_AX, 2);
        }
        break;
    }
    case 0x42:
    {
        if ((bx >= DOS_MAX_FILES) || (m->files[bx].fp == nullptr))
        {
            m->set_cf(true);
            m->set_reg16(UC_X86_REG_AX, 6);
            break;
        }
        {
            const int whence = (al == 0) ? SEEK_SET : ((al == 1) ? SEEK_CUR : SEEK_END);
            const long off = (long)(((uint32_t)cx << 16) | dx);
            if (fseek(m->files[bx].fp, off, whence) != 0)
            {
                m->set_cf(true);
                m->set_reg16(UC_X86_REG_AX, 25);
                break;
            }
            {
                const long pos = ftell(m->files[bx].fp);
                m->set_cf(false);
                m->set_reg16(UC_X86_REG_AX, (uint16_t)(pos & 0xFFFF));
                m->set_reg16(UC_X86_REG_DX, (uint16_t)((pos >> 16) & 0xFFFF));
            }
        }
        break;
    }
    case 0x43:
    case 0x44:
        m->set_cf(false);
        m->set_reg16(UC_X86_REG_AX, 0);
        m->set_reg16(UC_X86_REG_DX, 0x80); /* char device-ish */
        break;
    case 0x47:
    {
        uint8_t *dst = m->ptr_segoff(ds, si);
        dst[0] = 0; /* cwd = root */
        m->set_cf(false);
        break;
    }
    case 0x48:
        if ((uint32_t)m->alloc_bump + (uint32_t)bx < 0x9000u)
        {
            m->set_cf(false);
            m->set_reg16(UC_X86_REG_AX, (uint16_t)m->alloc_bump);
            m->alloc_bump += bx;
        }
        else
        {
            m->set_cf(true);
            m->set_reg16(UC_X86_REG_AX, 8);
            m->set_reg16(UC_X86_REG_BX, 0x1000);
        }
        break;
    case 0x49:
        m->set_cf(false);
        break;
    case 0x4A:
        m->set_cf(false);
        (void)es;
        break;
    case 0x4C:
        m->halted = true;
        m->exit_code = (int)al;
        uc_emu_stop(m->uc);
        rex_logf(REX_LOG_INFO, "DOS exit %d", m->exit_code);
        break;
    case 0x4D:
        m->set_reg16(UC_X86_REG_AX, (uint16_t)m->exit_code);
        break;
    case 0x4E:
    case 0x4F:
        m->set_cf(true);
        m->set_reg16(UC_X86_REG_AX, 18); /* no more files */
        break;
    default:
        rex_logf(REX_LOG_DEBUG, "INT21 AH=%02X stub", ah);
        m->set_cf(false);
        break;
    }
}

static void handle_int10(dos_machine *m)
{
    const uint16_t ax = m->reg16(UC_X86_REG_AX);
    const uint8_t ah = (uint8_t)(ax >> 8);
    const uint8_t al = (uint8_t)(ax & 0xFFu);
    const uint16_t dx = m->reg16(UC_X86_REG_DX);

    switch (ah)
    {
    case 0x00:
        m->video_mode = al;
        m->ram[0x449] = al;
        if ((al == 0x04) || (al == 0x05) || (al == 0x06) || (al == 0x0D) || (al == 0x13))
        {
            std::memset(m->ram + 0xB8000, 0, 0x8000);
            m->video_dirty = true;
        }
        rex_logf(REX_LOG_INFO, "INT10 set mode %02X", al);
        break;
    case 0x02:
        m->cursor_x = (uint16_t)(dx & 0xFFu);
        m->cursor_y = (uint16_t)(dx >> 8);
        break;
    case 0x03:
        m->set_reg16(UC_X86_REG_DX, (uint16_t)((m->cursor_y << 8) | (m->cursor_x & 0xFFu)));
        m->set_reg16(UC_X86_REG_CX, 0x0607);
        break;
    case 0x0E:
    {
        char ch[2] = {(char)al, 0};
        m->write_dos_string(ch);
        break;
    }
    case 0x0F:
        m->set_reg16(UC_X86_REG_AX, (uint16_t)((80u << 8) | m->video_mode));
        m->set_reg16(UC_X86_REG_BX, 0);
        break;
    default:
        break;
    }
}

static void handle_int16(dos_machine *m)
{
    const uint16_t ax = m->reg16(UC_X86_REG_AX);
    const uint8_t ah = (uint8_t)(ax >> 8);

    if (ah == 0x00)
    {
        if (m->kbd.empty())
        {
            m->wait_key = true;
            rewind_software_int(m, 2);
            return;
        }
        {
            const dos_kbd_ev ev = m->kbd.front();
            m->kbd.pop_front();
            m->set_reg16(UC_X86_REG_AX, (uint16_t)((ev.scan << 8) | ev.ascii));
        }
        return;
    }
    if (ah == 0x01)
    {
        if (m->kbd.empty())
        {
            m->set_zf(true);
        }
        else
        {
            const dos_kbd_ev ev = m->kbd.front();
            m->set_zf(false);
            m->set_reg16(UC_X86_REG_AX, (uint16_t)((ev.scan << 8) | ev.ascii));
        }
        return;
    }
    if (ah == 0x02)
    {
        m->set_reg16(UC_X86_REG_AX, (uint16_t)(ax & 0xFF00u));
    }
}

void dos_machine::handle_intr(uint32_t intno)
{
    switch (intno)
    {
    case 0x03:
        at_break = true;
        last_stop = REX_STOP_BREAK;
        uc_emu_stop(uc);
        break;
    case 0x10:
        handle_int10(this);
        break;
    case 0x11:
        set_reg16(UC_X86_REG_AX, 0x0021); /* CGA + 80x25 + 2 floppies-ish */
        break;
    case 0x12:
        set_reg16(UC_X86_REG_AX, 640);
        break;
    case 0x13:
        set_cf(true);
        set_reg16(UC_X86_REG_AX, 0x0100);
        break;
    case 0x15:
        set_cf(true);
        break;
    case 0x16:
        handle_int16(this);
        break;
    case 0x1A:
        set_reg16(UC_X86_REG_CX, 0);
        set_reg16(UC_X86_REG_DX, (uint16_t)(clock() & 0xFFFF));
        set_cf(false);
        break;
    case 0x1C:
        break;
    case 0x20:
        halted = true;
        exit_code = 0;
        uc_emu_stop(uc);
        break;
    case 0x21:
        handle_int21(this);
        break;
    case 0x23:
    case 0x24:
        break;
    case 0x33:
        set_reg16(UC_X86_REG_AX, 0); /* no mouse */
        break;
    default:
        rex_logf(REX_LOG_DEBUG, "unhandled INT %02X", intno);
        break;
    }
}
