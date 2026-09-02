/**
 * @file dos_int.cpp
 * @brief Tiny DOS 5 / CGA BIOS interrupt handlers for the Unicorn machine.
 */

#include "dos/dos_machine.h"
#include "dos/dos_fcb.h"
#include "dos/ibm_bda.h"

#include "rex/rex_log.h"

#include <unicorn/unicorn.h>
#include <unicorn/x86.h>

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
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
    if (files[fd].used && (files[fd].fp != nullptr) && (fd >= 5))
    {
        fclose(files[fd].fp);
    }
    files[fd].fp = nullptr;
    files[fd].used = false;
}

static void rewind_software_int(dos_machine *m, uint16_t nbytes)
{
    const uint16_t ip = m->reg16(UC_X86_REG_IP);
    m->set_reg16(UC_X86_REG_IP, (uint16_t)(ip - nbytes));
    uc_emu_stop(m->uc);
}

static void fcb_to_name(const uint8_t *fcb, char *out, size_t n)
{
    char name[9];
    char ext[4];
    size_t i = 0;
    size_t k = 0;
    assert((fcb != nullptr) && (out != nullptr) && (n > 0));
    for (i = 0; (i < 8) && (k < sizeof(name) - 1); i++)
    {
        const char c = (char)fcb[1 + i];
        if ((c == ' ') || (c == '\0'))
        {
            break;
        }
        name[k++] = c;
    }
    name[k] = 0;
    k = 0;
    for (i = 0; (i < 3) && (k < sizeof(ext) - 1); i++)
    {
        const char c = (char)fcb[9 + i];
        if ((c == ' ') || (c == '\0'))
        {
            break;
        }
        ext[k++] = c;
    }
    ext[k] = 0;
    if (ext[0] != 0)
    {
        std::snprintf(out, n, "%s.%s", name, ext);
    }
    else
    {
        std::snprintf(out, n, "%s", name);
    }
}

static FILE *fopen_ci(const std::string &cwd, const char *name, const char *mode)
{
    FILE *fp = nullptr;
    DIR *dir = nullptr;
    std::string exact;
    char want[16];
    size_t i = 0;
    if ((name == nullptr) || (mode == nullptr) || (name[0] == '\0'))
    {
        return nullptr;
    }
    exact = cwd + "/" + name;
    fp = std::fopen(exact.c_str(), mode);
    if (fp != nullptr)
    {
        return fp;
    }
    for (i = 0; (name[i] != 0) && (i + 1u < sizeof(want)); i++)
    {
        want[i] = (char)std::toupper((unsigned char)name[i]);
    }
    want[i] = 0;
    dir = opendir(cwd.c_str());
    if (dir == nullptr)
    {
        return nullptr;
    }
    for (;;)
    {
        const struct dirent *de = readdir(dir);
        char have[256];
        if (de == nullptr)
        {
            break;
        }
        for (i = 0; (de->d_name[i] != 0) && (i + 1u < sizeof(have)); i++)
        {
            have[i] = (char)std::toupper((unsigned char)de->d_name[i]);
        }
        have[i] = 0;
        if (std::strcmp(have, want) == 0)
        {
            exact = cwd + "/" + de->d_name;
            fp = std::fopen(exact.c_str(), mode);
            break;
        }
    }
    closedir(dir);
    return fp;
}

static dos_fcb *as_fcb(uint8_t *p)
{
    return (dos_fcb *)p;
}

static const dos_fcb *as_fcb_c(const uint8_t *p)
{
    return (const dos_fcb *)p;
}

static uint16_t fcb_recsize(const uint8_t *fcb)
{
    const uint16_t sz = as_fcb_c(fcb)->recsiz;
    return (sz == 0) ? 128u : sz;
}

static uint32_t fcb_random_rec(const uint8_t *fcb)
{
    /* MS-DOS 1.25 SETUP: if recsize >= 64, ignore RR MSB (3-byte field). */
    const dos_fcb *f = as_fcb_c(fcb);
    uint32_t rec = (uint32_t)f->rr[0] | ((uint32_t)f->rr[1] << 8) | ((uint32_t)f->rr[2] << 16);
    if (fcb_recsize(fcb) < 64u)
    {
        rec |= ((uint32_t)f->rr_hi << 24);
    }
    return rec;
}

static void fcb_set_rr(uint8_t *fcb, uint32_t rec)
{
    dos_fcb *f = as_fcb(fcb);
    f->rr[0] = (uint8_t)(rec & 0xFFu);
    f->rr[1] = (uint8_t)((rec >> 8) & 0xFFu);
    f->rr[2] = (uint8_t)((rec >> 16) & 0xFFu);
    if (fcb_recsize(fcb) < 64u)
    {
        f->rr_hi = (uint8_t)((rec >> 24) & 0xFFu);
    }
}

static void fcb_setnrex(uint8_t *fcb, uint32_t rec)
{
    /* MS-DOS 1.25 SETNREX: NR = rec % 128, EXTENT = rec / 128. */
    dos_fcb *f = as_fcb(fcb);
    f->nr = (uint8_t)(rec % 128u);
    f->extent = (uint16_t)(rec / 128u);
}

static int fcb_handle(const uint8_t *fcb)
{
    const dos_fcb *f = as_fcb_c(fcb);
    if ((uint8_t)(f->firclus & 0xFFu) != 0xFCu)
    {
        return -1;
    }
    return (int)f->devid;
}

static void fcb_set_handle(uint8_t *fcb, int fd)
{
    dos_fcb *f = as_fcb(fcb);
    f->devid = (uint8_t)fd;
    f->firclus = (uint16_t)((f->firclus & 0xFF00u) | 0x00FCu);
}

static uint8_t fcb_read_records(dos_machine *m, uint8_t *fcb, uint16_t nrec, bool sequential,
                                bool advance_rr)
{
    const int fd = fcb_handle(fcb);
    const uint16_t recsize = fcb_recsize(fcb);
    uint32_t rec = 0;
    uint16_t got_recs = 0;
    if ((fd < 0) || (fd >= DOS_MAX_FILES) || (m->files[fd].fp == nullptr) || (nrec == 0))
    {
        rex_logf(REX_LOG_INFO, "FCB read no-handle fd=%d nrec=%u recsize=%u", fd, (unsigned)nrec,
                 (unsigned)recsize);
        return 1;
    }
    if (sequential)
    {
        const dos_fcb *f = as_fcb_c(fcb);
        rec = ((uint32_t)f->extent * 128u) + (uint32_t)f->nr;
    }
    else
    {
        rec = fcb_random_rec(fcb);
    }
    if (std::fseek(m->files[fd].fp, (long)rec * (long)recsize, SEEK_SET) != 0)
    {
        return 1;
    }
    {
        std::vector<uint8_t> tmp((size_t)nrec * (size_t)recsize);
        const size_t want = tmp.size();
        const size_t got = std::fread(tmp.data(), 1, want, m->files[fd].fp);
        if (got > 0)
        {
            if (m->dta >= m->ram_size)
            {
                rex_logf(REX_LOG_ERROR, "FCB read DTA 0x%X past RAM", m->dta);
                return 1;
            }
            {
                const size_t room = (size_t)(m->ram_size - m->dta);
                const size_t n = (got > room) ? room : got;
                std::memcpy(m->ram + m->dta, tmp.data(), n);
                if (n < got)
                {
                    rex_logf(REX_LOG_ERROR, "FCB read truncated %zu -> %zu @ DTA 0x%X", got, n,
                             m->dta);
                }
            }
        }
        rex_logf(REX_LOG_DEBUG,
                 "FCB read dta=0x%X rec=%u recsize=%u nrec=%u got=%zu seq=%d head=%02X%02X%02X%02X",
                 m->dta, rec, (unsigned)recsize, (unsigned)nrec, got, sequential ? 1 : 0,
                 (got > 0) ? tmp[0] : 0, (got > 1) ? tmp[1] : 0, (got > 2) ? tmp[2] : 0,
                 (got > 3) ? tmp[3] : 0);
        got_recs = (uint16_t)(got / recsize);
        if (got_recs > 0)
        {
            if (sequential)
            {
                fcb_setnrex(fcb, rec + got_recs);
            }
            else
            {
                /* AH=21: RR = last record read (no increment). AH=27: RR = last+1. */
                const uint32_t last = rec + (uint32_t)got_recs - 1u;
                const uint32_t store = advance_rr ? (last + 1u) : last;
                fcb_set_rr(fcb, store);
                fcb_setnrex(fcb, store);
            }
        }
        if (got == 0)
        {
            return 1;
        }
        if (got < want)
        {
            return 3;
        }
    }
    (void)got_recs;
    return 0;
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

    rex_logf((ah == 0x00 || ah == 0x0F || ah == 0x16 || ah == 0x3D || ah == 0x48 || ah == 0x4A ||
              ah == 0x4C)
                 ? REX_LOG_INFO
                 : REX_LOG_DEBUG,
             "INT21 AH=%02X AX=%04X BX=%04X CX=%04X DX=%04X DS=%04X ES=%04X @%04X:%04X", ah, ax, bx,
             cx, dx, ds, es, m->reg16(UC_X86_REG_CS), m->reg16(UC_X86_REG_IP));

    switch (ah)
    {
    case 0x00:
        rex_logf(REX_LOG_INFO, "INT21 AH=00 terminate");
        m->halted = true;
        m->exit_code = 0;
        m->last_stop = REX_STOP_HALTED;
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
    case 0x0F:
    case 0x16:
    {
        uint8_t *fcb = m->ptr_segoff(ds, dx);
        char nbuf[16];
        const int fd = m->alloc_handle();
        FILE *fp = nullptr;
        struct stat st{};
        fcb_to_name(fcb, nbuf, sizeof(nbuf));
        if (fd < 0)
        {
            m->set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | 0xFFu));
            break;
        }
        fp = fopen_ci(m->dos_cwd, nbuf, (ah == 0x16) ? "w+b" : "rb");
        if ((fp == nullptr) && (ah == 0x0F))
        {
            fp = fopen_ci(m->dos_cwd, nbuf, "r+b");
        }
        if (fp == nullptr)
        {
            m->close_handle(fd);
            rex_logf(REX_LOG_INFO, "INT21 FCB open fail %s", nbuf);
            m->set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | 0xFFu));
            break;
        }
        m->files[fd].fp = fp;
        fcb_set_handle(fcb, fd);
        {
            dos_fcb *f = as_fcb(fcb);
            /* MS-DOS 1.25 OPEN (IBM ZEROEXT): zero EXTENT, recsize always 128.
             * Programs set recsize after open (HEX2BIN: MOV [FCB+14], BUFSIZ).
             * We also zero NR/RR so leftover BSS does not seek past EOF. */
            f->extent = 0;
            f->recsiz = 128;
            f->nr = 0;
            f->rr[0] = 0;
            f->rr[1] = 0;
            f->rr[2] = 0;
            f->rr_hi = 0;
            if (fstat(fileno(fp), &st) == 0)
            {
                f->filsiz = (uint32_t)st.st_size;
            }
        }
        rex_logf(REX_LOG_INFO, "INT21 FCB %s %s -> %d", (ah == 0x16) ? "create" : "open", nbuf, fd);
        m->set_reg16(UC_X86_REG_AX, (uint16_t)(ax & 0xFF00u));
        break;
    }
    case 0x10:
    {
        uint8_t *fcb = m->ptr_segoff(ds, dx);
        const int fd = fcb_handle(fcb);
        if (fd >= 0)
        {
            m->close_handle(fd);
            as_fcb(fcb)->firclus = (uint16_t)(as_fcb(fcb)->firclus & 0xFF00u);
        }
        m->set_reg16(UC_X86_REG_AX, (uint16_t)(ax & 0xFF00u));
        break;
    }
    case 0x14:
    case 0x21:
    case 0x27:
    {
        uint8_t *fcb = m->ptr_segoff(ds, dx);
        const uint16_t nrec = (ah == 0x27) ? ((cx == 0) ? 1u : cx) : 1u;
        const uint8_t alv = fcb_read_records(m, fcb, nrec, (ah == 0x14), (ah == 0x27));
        m->set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | alv));
        if (ah == 0x27)
        {
            m->set_reg16(UC_X86_REG_CX, (alv == 0) ? nrec : 0);
        }
        break;
    }
    case 0x19:
        m->set_reg16(UC_X86_REG_AX, (uint16_t)(ax & 0xFF00u)); /* drive A=0, report C=2 */
        m->set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | 2u));
        break;
    case 0x29:
    {
        /* Parse ASCIIZ/command-line name at DS:SI into FCB at ES:DI. */
        uint16_t psi = si;
        uint16_t pdi = m->reg16(UC_X86_REG_DI);
        uint8_t *fcb = m->ptr_segoff(es, pdi);
        uint8_t al_out = 0;
        int ni = 0;
        int ei = 0;
        bool in_ext = false;
        std::memset(fcb, 0, 16);
        std::memset(fcb + 1, ' ', 11);
        for (;;)
        {
            const uint8_t ch = *m->ptr_segoff(ds, psi);
            if ((ch != (uint8_t)' ') && (ch != (uint8_t)'\t'))
            {
                break;
            }
            psi++;
        }
        {
            const uint8_t a = *m->ptr_segoff(ds, psi);
            const uint8_t b = *m->ptr_segoff(ds, (uint16_t)(psi + 1u));
            if ((((a >= 'A') && (a <= 'Z')) || ((a >= 'a') && (a <= 'z'))) && (b == (uint8_t)':'))
            {
                fcb[0] = (uint8_t)(std::toupper((unsigned char)a) - 'A' + 1);
                psi = (uint16_t)(psi + 2u);
            }
        }
        for (;;)
        {
            const uint8_t ch = *m->ptr_segoff(ds, psi);
            if ((ch == 0) || (ch == (uint8_t)' ') || (ch == (uint8_t)'\t') || (ch == (uint8_t)'\r') ||
                (ch == (uint8_t)'\n'))
            {
                break;
            }
            if (ch == (uint8_t)'.')
            {
                in_ext = true;
                ei = 0;
                psi++;
                continue;
            }
            if (ch == (uint8_t)'*')
            {
                al_out = 1;
                if (!in_ext)
                {
                    while (ni < 8)
                    {
                        fcb[1 + ni++] = (uint8_t)'?';
                    }
                }
                else
                {
                    while (ei < 3)
                    {
                        fcb[9 + ei++] = (uint8_t)'?';
                    }
                }
                psi++;
                continue;
            }
            if (ch == (uint8_t)'?')
            {
                al_out = 1;
            }
            {
                const uint8_t up = (uint8_t)std::toupper((unsigned char)ch);
                if (!in_ext)
                {
                    if (ni < 8)
                    {
                        fcb[1 + ni++] = up;
                    }
                }
                else if (ei < 3)
                {
                    fcb[9 + ei++] = up;
                }
            }
            psi++;
        }
        m->set_reg16(UC_X86_REG_SI, psi);
        m->set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | al_out));
        break;
    }
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
    case 0x0D:
        m->set_cf(false);
        break;
    case 0x0E:
        m->set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | 3u)); /* AL = last drive */
        m->set_cf(false);
        break;
    case 0x36:
        m->set_cf(false);
        m->set_reg16(UC_X86_REG_AX, 8);    /* sectors/cluster */
        m->set_reg16(UC_X86_REG_BX, 0x1000);
        m->set_reg16(UC_X86_REG_CX, 512);
        m->set_reg16(UC_X86_REG_DX, 0x1000);
        break;
    case 0x38:
        m->set_cf(false);
        m->set_reg16(UC_X86_REG_BX, 1); /* USA */
        break;
    case 0x48:
    {
        const uint32_t room =
            (m->alloc_bump < 0xA000u) ? (0xA000u - (uint32_t)m->alloc_bump) : 0u;
        if ((bx != 0u) && ((uint32_t)bx <= room))
        {
            m->set_cf(false);
            m->set_reg16(UC_X86_REG_AX, (uint16_t)m->alloc_bump);
            m->alloc_bump = (uint32_t)m->alloc_bump + (uint32_t)bx;
        }
        else
        {
            m->set_cf(true);
            m->set_reg16(UC_X86_REG_AX, 8);
            m->set_reg16(UC_X86_REG_BX, (uint16_t)room);
        }
        break;
    }
    case 0x49:
        m->set_cf(false);
        break;
    case 0x4A:
    {
        /* DOS: BX=FFFF probes max; CF=1 and BX=available. Success only if BX fits. */
        const uint32_t max_paras =
            (es < 0xA000u) ? (0xA000u - (uint32_t)es) : 16u;
        if ((uint32_t)bx > max_paras)
        {
            m->set_cf(true);
            m->set_reg16(UC_X86_REG_AX, 8);
            m->set_reg16(UC_X86_REG_BX, (uint16_t)max_paras);
        }
        else
        {
            m->set_cf(false);
            if (es == (uint16_t)DOS_PSP_SEG)
            {
                const uint16_t end = (uint16_t)(es + bx);
                m->ram[0x10002] = (uint8_t)(end & 0xFFu);
                m->ram[0x10003] = (uint8_t)(end >> 8);
                m->alloc_bump = end;
            }
        }
        break;
    }
    case 0x50:
        m->set_cf(false);
        break;
    case 0x51:
    case 0x62:
        m->set_reg16(UC_X86_REG_BX, (uint16_t)DOS_PSP_SEG);
        m->set_cf(false);
        break;
    case 0x59:
        m->set_reg16(UC_X86_REG_AX, 8);
        m->set_reg16(UC_X86_REG_BX, 0);
        m->set_cf(false);
        break;
    case 0x4C:
        m->halted = true;
        m->exit_code = (int)al;
        m->last_stop = REX_STOP_HALTED;
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

/** PCBIOS: alpha modes are 0–3 and 7 (MDA). 4–6 are CGA graphics. */
static bool video_is_alpha(uint8_t mode)
{
    return (mode < 0x04u) || (mode == 0x07u);
}

static void tty_putc(dos_machine *m, uint8_t ch)
{
    uint32_t o = 0;
    if (ch == 0x0Du)
    {
        m->cursor_x = 0;
        return;
    }
    if (ch == 0x0Au)
    {
        if (m->cursor_y < 24u)
        {
            m->cursor_y++;
        }
        return;
    }
    if (ch == 0x08u)
    {
        if (m->cursor_x > 0u)
        {
            m->cursor_x--;
        }
        return;
    }
    o = 0xB8000u + ((uint32_t)m->cursor_y * 80u + (uint32_t)m->cursor_x) * 2u;
    if (o + 1u < m->ram_size)
    {
        m->ram[o] = ch;
        m->ram[o + 1u] = 0x07;
    }
    m->cursor_x++;
    if (m->cursor_x >= 80u)
    {
        m->cursor_x = 0;
        if (m->cursor_y < 24u)
        {
            m->cursor_y++;
        }
    }
    m->video_dirty = true;
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
        {
            ibm_bda *bda = ibm_bda_at(m->ram);
            const uint8_t cols =
                ((al == 0x00) || (al == 0x01) || (al == 0x04) || (al == 0x05)) ? 40u : 80u;
            if (bda != nullptr)
            {
                bda->video_mode = al;
                bda->video_cols = cols;
            }
        }
        m->cursor_x = 0;
        m->cursor_y = 0;
        m->blank_regen();
        if ((al == 0x04u) || (al == 0x05u))
        {
            /* BIOS mode 4/5: palette 1 + intensity, black background. */
            m->cga_3d9 = 0x30;
            {
                ibm_bda *bda = ibm_bda_at(m->ram);
                if (bda != nullptr)
                {
                    bda->crt_palette = 0x30;
                }
            }
        }
        rex_logf(REX_LOG_INFO, "INT10 set mode %02X", al);
        break;
    case 0x0B:
    {
        /* SET PALETTE / BACKGROUND — Bushido hit-flash uses this. */
        const uint16_t bx = m->reg16(UC_X86_REG_BX);
        const uint8_t bh = (uint8_t)(bx >> 8);
        const uint8_t bl = (uint8_t)(bx & 0xFFu);
        uint8_t r = m->cga_3d9;
        if (bh == 0u)
        {
            r = (uint8_t)((r & 0xF0u) | (bl & 0x0Fu));
        }
        else
        {
            r = (uint8_t)((r & (uint8_t)~0x20u) | (uint8_t)((bl & 1u) << 5));
        }
        m->cga_3d9 = r;
        {
            ibm_bda *bda = ibm_bda_at(m->ram);
            if (bda != nullptr)
            {
                bda->crt_palette = r;
            }
        }
        m->video_dirty = true;
        break;
    }
    case 0x02:
        m->cursor_x = (uint16_t)(dx & 0xFFu);
        m->cursor_y = (uint16_t)(dx >> 8);
        break;
    case 0x03:
        m->set_reg16(UC_X86_REG_DX, (uint16_t)((m->cursor_y << 8) | (m->cursor_x & 0xFFu)));
        m->set_reg16(UC_X86_REG_CX, 0x0607);
        break;
    case 0x05:
        /* Select page. CGA graphics is page 0 only; text pages later. */
        break;
    case 0x08:
    {
        const uint32_t o = 0xB8000u + ((uint32_t)m->cursor_y * 80u + (uint32_t)m->cursor_x) * 2u;
        uint8_t ch = 0;
        uint8_t at = 0x07;
        if (o + 1u < m->ram_size)
        {
            ch = m->ram[o];
            at = m->ram[o + 1u];
        }
        m->set_reg16(UC_X86_REG_AX, (uint16_t)(((uint16_t)at << 8) | ch));
        break;
    }
    case 0x09:
    case 0x0A:
    {
        const uint16_t cx = m->reg16(UC_X86_REG_CX);
        const uint8_t attr = (uint8_t)(m->reg16(UC_X86_REG_BX) & 0xFFu);
        uint16_t n = (cx == 0) ? 1u : cx;
        uint16_t x = m->cursor_x;
        uint16_t y = m->cursor_y;
        if (!video_is_alpha(m->video_mode))
        {
            /* PCBIOS GRAPHICS_WRITE: ROM 8×8 into the CGA bitmap. Do not
             * store text cells on top of mode 4/5/6 VRAM. */
            break;
        }
        if (n > 2000u)
        {
            n = 2000u;
        }
        while (n-- > 0)
        {
            const uint32_t o = 0xB8000u + ((uint32_t)y * 80u + (uint32_t)x) * 2u;
            if (o + 1u < m->ram_size)
            {
                m->ram[o] = al;
                if (ah == 0x09)
                {
                    m->ram[o + 1u] = attr;
                }
            }
            x++;
            if (x >= 80u)
            {
                x = 0;
                y++;
                if (y >= 25u)
                {
                    break;
                }
            }
        }
        m->video_dirty = true;
        break;
    }
    case 0x0E:
    {
        char ch[2] = {(char)al, 0};
        m->write_dos_string(ch);
        if (video_is_alpha(m->video_mode))
        {
            tty_putc(m, al);
        }
        break;
    }
    case 0x0F:
    {
        const uint8_t mode = m->video_mode;
        const uint8_t cols =
            ((mode == 0x00) || (mode == 0x01) || (mode == 0x04) || (mode == 0x05)) ? 40u : 80u;
        m->set_reg16(UC_X86_REG_AX, (uint16_t)(((uint16_t)cols << 8) | mode));
        m->set_reg16(UC_X86_REG_BX, 0);
        break;
    }
    case 0x06:
    case 0x07:
        /* AL=0 → fill window. Graphics: wipe CGA. Text: space + BH attribute. */
        if (al == 0)
        {
            if (!video_is_alpha(m->video_mode))
            {
                std::memset(m->ram + 0xB8000, 0, 0x8000);
            }
            else
            {
                const uint8_t attr = (uint8_t)(m->reg16(UC_X86_REG_BX) >> 8);
                uint32_t i = 0;
                for (i = 0; i < 80u * 25u; i++)
                {
                    m->ram[0xB8000u + i * 2u] = (uint8_t)' ';
                    m->ram[0xB8000u + i * 2u + 1u] = attr;
                }
            }
            m->video_dirty = true;
        }
        break;
    default:
        break;
    }
}

static void handle_int16(dos_machine *m)
{
    const uint16_t ax = m->reg16(UC_X86_REG_AX);
    const uint8_t ah = (uint8_t)(ax >> 8);

    if ((ah == 0x00) || (ah == 0x10))
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
    if ((ah == 0x01) || (ah == 0x11))
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
    {
        auto it = int_bps.find((uint8_t)intno);
        const bool armed = (it != int_bps.end());
        if (armed && (!skip_int_bp))
        {
            const uint16_t ip = reg16(UC_X86_REG_IP);
            set_reg16(UC_X86_REG_IP, (uint16_t)(ip - 2u));
            at_break = true;
            skip_int_bp = true;
            last_stop = REX_STOP_BREAK;
            if (it->second == 1u)
            {
                int_bps.erase(it);
            }
            else if (it->second > 1u)
            {
                it->second--;
            }
            uc_emu_stop(uc);
            rex_logf(REX_LOG_INFO, "BPINT %02X AX=%04X @ %04X:%04X", (unsigned)intno,
                     reg16(UC_X86_REG_AX), reg16(UC_X86_REG_CS), (uint16_t)(ip - 2u));
            return;
        }
        skip_int_bp = false;
    }
    switch (intno)
    {
    case 0x03:
        /* Guest INT3. User BPs are CODE-hook linear addresses, not 0xCC patches.
         * BASCOM pads with CC; stopping here aborts F9 in the padding. */
        rex_logf(REX_LOG_DEBUG, "INT3 padding @ %04X:%04X", reg16(UC_X86_REG_CS),
                 reg16(UC_X86_REG_IP));
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
    {
        /* BIOS time: AH=00 returns the 18.2 Hz dword at 0040:006C, not C clock(). */
        const uint16_t ax = reg16(UC_X86_REG_AX);
        const uint8_t ah = (uint8_t)(ax >> 8);
        if (ah == 0x00u)
        {
            ibm_bda *bda = ibm_bda_at(ram);
            const uint32_t ticks = (bda != nullptr) ? bda->timer_ticks : 0;
            const uint8_t midnight = (bda != nullptr) ? bda->timer_overflow : 0;
            if (bda != nullptr)
            {
                bda->timer_overflow = 0;
            }
            set_reg16(UC_X86_REG_CX, (uint16_t)(ticks >> 16));
            set_reg16(UC_X86_REG_DX, (uint16_t)ticks);
            set_reg16(UC_X86_REG_AX, (uint16_t)((ax & 0xFF00u) | midnight));
            set_cf(false);
        }
        else if (ah == 0x01u)
        {
            ibm_bda *bda = ibm_bda_at(ram);
            const uint16_t cx = reg16(UC_X86_REG_CX);
            const uint16_t dx = reg16(UC_X86_REG_DX);
            if (bda != nullptr)
            {
                bda->timer_ticks = ((uint32_t)cx << 16) | (uint32_t)dx;
                bda->timer_overflow = 0;
            }
            set_cf(false);
        }
        else
        {
            set_cf(true);
        }
        break;
    }
    case 0x1C:
        break;
    case 0x20:
        rex_logf(REX_LOG_INFO, "INT20 terminate from %04X:%04X", reg16(UC_X86_REG_CS),
                 (uint16_t)(reg16(UC_X86_REG_IP) - 2u));
        halted = true;
        exit_code = 0;
        last_stop = REX_STOP_HALTED;
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
