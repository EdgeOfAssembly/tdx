/**
 * @file pc.cpp
 * @brief INT 10h (tty/cursor/scroll), INT 13h AH=00/02, INT 16h, INT 1Ah.
 *
 * With rom_bios(), INT 10h AH=0Eh may be served here while IVT[10h] is still
 * the 24APR81 VIDEO_IO at F000:F065; AH=00 and hooked INT 10h stay in ROS.
 */
#include "iron86/pc.h"
#include "iron86/chs_lba.h"
#include "iron86/crtc.h"
#include "iron86/flopfs_pack.h"
#include "iron86/log.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace iron86
{

#ifndef NDEBUG
/** @brief PIC 20h/21h trace; compiled out of release (-DNDEBUG). */
static void dbg_pic(const char *what, uint16_t port, uint8_t val, const pic8259 &p)
{
    fd_printf(STDERR_FILENO,
              "PIC %s p=%02X v=%02X imr=%02X irr=%02X isr=%02X step=%u init=%u\n", what,
              static_cast<unsigned>(port), val, p.imr, p.irr, p.isr,
              static_cast<unsigned>(p.icw_step), static_cast<unsigned>(p.initialized));
    log_trace("PIC %s p=%02X v=%02X imr=%02X irr=%02X isr=%02X step=%u init=%u\n", what,
              static_cast<unsigned>(port), val, p.imr, p.irr, p.isr,
              static_cast<unsigned>(p.icw_step), static_cast<unsigned>(p.initialized));
}
#endif

static void text_fill(cpu &c, unsigned r0, unsigned c0, unsigned r1, unsigned c1, uint8_t ch,
                      uint8_t at)
{
    if (r1 > 24u)
    {
        r1 = 24u;
    }
    if (c1 > 79u)
    {
        c1 = 79u;
    }
    if ((r0 > r1) || (c0 > c1))
    {
        return;
    }
    unsigned r = r0;
    for (; r <= r1; r++)
    {
        unsigned col = c0;
        for (; col <= c1; col++)
        {
            const uint32_t off = (r * 80u + col) * 2u;
            c.mem_write8(0xB8000u + off, ch);
            c.mem_write8(0xB8000u + off + 1u, at);
        }
    }
}

static void text_scroll_up(cpu &c, unsigned n, unsigned r0, unsigned c0, unsigned r1, unsigned c1,
                           uint8_t at)
{
    if (r1 > 24u)
    {
        r1 = 24u;
    }
    if (c1 > 79u)
    {
        c1 = 79u;
    }
    if ((r0 > r1) || (c0 > c1))
    {
        return;
    }
    const unsigned h = r1 - r0 + 1u;
    if ((n == 0u) || (n >= h))
    {
        text_fill(c, r0, c0, r1, c1, static_cast<uint8_t>(' '), at);
        return;
    }
    unsigned r = r0;
    for (; r + n <= r1; r++)
    {
        unsigned col = c0;
        for (; col <= c1; col++)
        {
            const uint32_t d = (r * 80u + col) * 2u;
            const uint32_t s = ((r + n) * 80u + col) * 2u;
            c.mem_write8(0xB8000u + d, c.mem_read8(0xB8000u + s));
            c.mem_write8(0xB8000u + d + 1u, c.mem_read8(0xB8000u + s + 1u));
        }
    }
    text_fill(c, r1 - n + 1u, c0, r1, c1, static_cast<uint8_t>(' '), at);
}

const std::vector<uint8_t> *pc::floppy_media(uint8_t drv) const
{
    if ((drv & 0x80u) != 0)
    {
        return nullptr;
    }
    const uint8_t unit = static_cast<uint8_t>(drv & 3u);
    if (unit > 1u)
    {
        return nullptr;
    }
    if (floppy_[unit].size() < 512u)
    {
        return nullptr;
    }
    return &floppy_[unit];
}

void pc::sync_fdd_dip()
{
    uint8_t dip = hw_.ppi.dip;
    if (dip == 0)
    {
        dip = 0x2D;
    }
    dip = static_cast<uint8_t>(dip & static_cast<uint8_t>(~0xC0u));
    if (floppy_[1].size() >= 512u)
    {
        /* SW1 bits 6–7: 01 = 2 floppy drives (0x2D | 0x40 = 0x6D). */
        dip = static_cast<uint8_t>(dip | 0x40u);
    }
    hw_.ppi.dip = dip;
}

void pc::set_mda(bool on)
{
    uint8_t dip = hw_.ppi.dip;
    if (dip == 0)
    {
        dip = 0x2D;
    }
    dip = static_cast<uint8_t>(dip & static_cast<uint8_t>(~0x30u));
    dip = static_cast<uint8_t>(dip | (on ? 0x30u : 0x20u));
    hw_.ppi.dip = dip;
}

bool pc::attach_floppy(const uint8_t *img, size_t n)
{
    return attach_floppy(0, img, n);
}

bool pc::attach_floppy(uint8_t unit, const uint8_t *img, size_t n)
{
    if ((unit > 1u) || (img == nullptr) || (n < 512u))
    {
        return false;
    }
    floppy_[unit].assign(img, img + n);
    sync_fdd_dip();
    return true;
}

bool pc::attach_floppy_path(uint8_t unit, const char *path)
{
    struct stat st{};
    if ((unit > 1u) || (path == nullptr) || (path[0] == '\0'))
    {
        return false;
    }
    if (stat(path, &st) != 0)
    {
        return false;
    }
    if (S_ISDIR(st.st_mode))
    {
        std::vector<uint8_t> img;
        if (!flopfs_pack_dir(path, &img))
        {
            return false;
        }
        return attach_floppy(unit, img.data(), img.size());
    }
    if (!S_ISREG(st.st_mode))
    {
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    const std::vector<uint8_t> img((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
    return attach_floppy(unit, img.data(), img.size());
}

bool pc::attach_floppy_b(const uint8_t *img, size_t n)
{
    return attach_floppy(1, img, n);
}

bool pc::attach_hdc_path(const char *path, uint32_t cylinders, uint32_t heads, uint32_t spt)
{
    if ((path == nullptr) || (path[0] == '\0'))
    {
        return false;
    }
    struct stat st{};
    if (stat(path, &st) != 0)
    {
        return false;
    }
    if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode))
    {
        log_event("iron86: refuse hdc block/char device %s (host disk)\n", path);
        return false;
    }
    if (!S_ISREG(st.st_mode) || (st.st_size < 512))
    {
        return false;
    }
    if (!hdc_.attach(path, cylinders, heads, spt))
    {
        return false;
    }
    hdc_.bind_bus(&c, &hw_.dma);
    hdc_path_ = path;
    hdc_bytes_ = static_cast<size_t>(st.st_size);
    return true;
}

bool pc::load_hdc_rom(const uint8_t *data, size_t n)
{
    if ((data == nullptr) || (n < 3u) || (data[0] != 0x55u) || (data[1] != 0xAAu))
    {
        return false;
    }
    uint32_t bytes = static_cast<uint32_t>(data[2]) * 512u;
    if ((bytes == 0u) || (bytes > n))
    {
        bytes = static_cast<uint32_t>(n);
    }
    if (bytes > 8192u)
    {
        bytes = 8192u;
    }
    uint32_t i = 0;
    for (; i < bytes; i++)
    {
        c.mem_write8(k_hdc_ros_phys + i, data[i]);
    }
    hdc_rom_loaded_ = true;
    return true;
}

bool pc::load_hdc_rom_path(const char *path)
{
    if ((path == nullptr) || (path[0] == '\0'))
    {
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    const std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
    return load_hdc_rom(rom.data(), rom.size());
}

void pc::hdc_sync_irq()
{
    if (hdc_.irq_line())
    {
        pic_assert(k_hdc_irq);
    }
    else
    {
        pic_deassert(k_hdc_irq);
    }
}

bool pc::load_floppy(const uint8_t *img, size_t n)
{
    if (!attach_floppy(0, img, n))
    {
        return false;
    }
    c.reset();
    for (size_t i = 0; i < 512u; i++)
    {
        c.mem_write8(0x7C00u + static_cast<uint32_t>(i), floppy_[0][i]);
    }
    return true;
}

void pc::boot()
{
    t0_ = std::chrono::steady_clock::now();
    if (floppy_[0].size() >= 512u)
    {
        size_t i = 0;
        for (i = 0; i < 512u; i++)
        {
            c.mem_write8(0x7C00u + static_cast<uint32_t>(i), floppy_[0][i]);
        }
    }
    c.set_bios([this](cpu &, uint8_t v) { return bios_int(v); });
    c.set_cs(0);
    c.set_ds(0);
    c.set_es(0);
    c.set_ss(0);
    c.set_sp(0x7C00);
    c.set_ip(0x7C00);
    c.set_dx(0); /* DL = drive 0 */
    for (uint32_t i = 0; i < 80u * 25u; i++)
    {
        c.mem_write8(0xB8000u + i * 2u, static_cast<uint8_t>(' '));
        c.mem_write8(0xB8000u + i * 2u + 1u, 0x07);
    }
    cur_x_ = 0;
    cur_y_ = 0;
    video_mode_ = 0x03;
}

uint8_t pc::ascii_make(uint8_t ch)
{
    static const uint8_t letters[26] = {0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
                                        0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13,
                                        0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C};
    if ((ch == '\n') || (ch == '\r'))
    {
        return 0x1C;
    }
    if ((ch == '\b') || (ch == 0x7F))
    {
        return 0x0E;
    }
    if (ch == '\t')
    {
        return 0x0F;
    }
    if (ch == 0x1B)
    {
        return 0x01;
    }
    if (ch == ' ')
    {
        return 0x39;
    }
    if ((ch >= 'A') && (ch <= 'Z'))
    {
        ch = static_cast<uint8_t>(ch - 'A' + 'a');
    }
    if ((ch >= 'a') && (ch <= 'z'))
    {
        return letters[ch - 'a'];
    }
    if ((ch >= '1') && (ch <= '9'))
    {
        return static_cast<uint8_t>(0x02u + (ch - '1'));
    }
    if (ch == '0')
    {
        return 0x0B;
    }
    if ((ch == ';') || (ch == ':'))
    {
        return 0x27;
    }
    if ((ch == '.') || (ch == '>'))
    {
        return 0x34;
    }
    if ((ch == ',') || (ch == '<'))
    {
        return 0x33;
    }
    if ((ch == '/') || (ch == '?'))
    {
        return 0x35;
    }
    if ((ch == '-') || (ch == '_'))
    {
        return 0x0C;
    }
    if ((ch == '=') || (ch == '+'))
    {
        return 0x0D;
    }
    if ((ch == '\\') || (ch == '|'))
    {
        return 0x2B;
    }
    return 0;
}

static bool ascii_needs_shift(uint8_t ch)
{
    /* Letters stay unshifted (BIOS INT 9 → lowercase; COMMAND.COM upcases). */
    switch (ch)
    {
    case ':':
    case '"':
    case '_':
    case '+':
    case '<':
    case '>':
    case '?':
    case '{':
    case '}':
    case '|':
    case '~':
        return true;
    default:
        return false;
    }
}

void pc::queue_scan(uint8_t sc)
{
    if (hw_.ppi.kbd_ready != 0)
    {
        scanq_.push_back(sc);
        return;
    }
    hw_.ppi.kbd_data = sc;
    hw_.ppi.kbd_ready = 1;
    hw_.ppi.kbd_irq_pend = 1;
    kbd_irq_delay_ = k_xt_kbd_irq_delay;
    pic_deassert(1);
}

void pc::type_scan(uint8_t make)
{
    const uint8_t m = static_cast<uint8_t>(make & 0x7Fu);
    /* INT 16h is stolen under ROM BIOS; PPI-only inject never unblocks AH=00. */
    kbd_.push_back(0);
    kbd_scan_.push_back(m);
    waiting_key_ = false;
    queue_scan(m);
    /* INT 9 wrap (M1TP CGAGRPH) treats make+break in one poll as no key.
     * Same pump as type_keys so the hook sees the make first. */
    int n = 0;
    for (n = 0; (n < 1200) && (!c.halted()); n++)
    {
        if (!c.step())
        {
            break;
        }
    }
    queue_scan(static_cast<uint8_t>(m | 0x80u));
}

void pc::feed_ascii(uint8_t ch)
{
    kbd_.push_back(ch);
    waiting_key_ = false;
    const uint8_t mk = ascii_make(ch);
    if (mk == 0)
    {
        return;
    }
    if (ascii_needs_shift(ch))
    {
        queue_scan(0x2A);
    }
    queue_scan(mk);
    queue_scan(static_cast<uint8_t>(mk | 0x80u));
    if (ascii_needs_shift(ch))
    {
        queue_scan(0xAA);
    }
}

void pc::type_keys(const char *s)
{
    if (s == nullptr)
    {
        return;
    }
    auto pump = [this]() {
        int n = 0;
        for (n = 0; (n < 1200) && (!c.halted()); n++)
        {
            if (!c.step())
            {
                break;
            }
        }
    };
    for (const char *p = s; *p != '\0'; p++)
    {
        const uint8_t ch = static_cast<uint8_t>(*p);
        const uint8_t mk = ascii_make(ch);
        const bool shift = ascii_needs_shift(ch);
        kbd_.push_back(ch);
        if (mk != 0)
        {
            /* Pump BIOS INT 9 between make/break so Shift is actually held. */
            if (shift)
            {
                queue_scan(0x2A);
                pump();
            }
            queue_scan(mk);
            pump();
            queue_scan(static_cast<uint8_t>(mk | 0x80u));
            pump();
            if (shift)
            {
                queue_scan(0xAA);
                pump();
            }
        }
    }
    if (!kbd_.empty())
    {
        waiting_key_ = false;
    }
}

void floppy_geom(size_t n, uint32_t *spt, uint32_t *heads)
{
    static const struct
    {
        size_t bytes;
        uint32_t spt;
        uint32_t heads;
    } k_ibm[] = {
        {k_floppy_bytes_160k, 8u, 1u},
        {k_floppy_bytes_180k, 9u, 1u},
        {k_floppy_bytes_320k, 8u, 2u},
        {k_floppy_bytes_360k, 9u, 2u},
        {k_floppy_bytes_720k, 9u, 2u},
        {k_floppy_bytes_1200k, 15u, 2u},
        {k_floppy_bytes_1440k, 18u, 2u},
    };
    if ((spt == nullptr) || (heads == nullptr))
    {
        return;
    }
    size_t best_i = 0;
    size_t best_d = static_cast<size_t>(-1);
    size_t i = 0;
    for (; i < (sizeof(k_ibm) / sizeof(k_ibm[0])); i++)
    {
        if (k_ibm[i].bytes == n)
        {
            *spt = k_ibm[i].spt;
            *heads = k_ibm[i].heads;
            return;
        }
        const size_t d = (n > k_ibm[i].bytes) ? (n - k_ibm[i].bytes) : (k_ibm[i].bytes - n);
        if (d < best_d)
        {
            best_d = d;
            best_i = i;
        }
    }
    *spt = k_ibm[best_i].spt;
    *heads = k_ibm[best_i].heads;
}

uint32_t pc::chs_lba(uint8_t cyl, uint8_t head, uint8_t sec, uint8_t unit) const
{
    /* sec is 1-based. Size picks IBM floppy geom (exact then nearest). */
    if (sec == 0)
    {
        return UINT32_MAX;
    }
    uint32_t spt = 9;
    uint32_t nh = 2;
    if (unit < 2u)
    {
        floppy_geom(floppy_[unit].size(), &spt, &nh);
    }
    return iron86_chs_to_lba(static_cast<uint32_t>(cyl), nh, static_cast<uint32_t>(head), spt,
                             static_cast<uint32_t>(sec));
}

bool pc::bios_int(uint8_t vector)
{
    if (c.rom_bios())
    {
        /* 5150 8K ROM owns video/timer. INT 13h stays here (ROM is 40-track
         * 360K; FlopFS 720K B: uses cyl 40–79). INT 16h too: tdxctl type
         * fills iron86's key queue, which ROM INT 16h never reads.
         * INT 8 / INT 1Ah always fall through to ROS. */
        if (vector == 0x13)
        {
            return int13();
        }
        if (vector == 0x16)
        {
            return int16();
        }
        if (vector == 0x10)
        {
            /* 24APR81 VIDEO_IO is F000:F065 (vector table at F000:FF13).
             * Steal only AH=0Eh teletype while the IVT still points there
             * and BDA 40:49 is 80-column CGA text. AH=00 mode-set, graphics
             * teletype, and a hooked INT 10h stay in ROS. */
            if ((c.mem_read16(0x40u) != 0xF065u) || (c.mem_read16(0x42u) != 0xF000u))
            {
                return false;
            }
            if (static_cast<uint8_t>(c.ax() >> 8) != 0x0Eu)
            {
                return false;
            }
            const uint8_t mode = c.mem_read8(0x449u);
            if ((mode != 2u) && (mode != 3u))
            {
                return false;
            }
            const uint8_t page = static_cast<uint8_t>((c.bx() >> 8) & 7u);
            const uint32_t bda = 0x450u + static_cast<uint32_t>(page) * 2u;
            const uint16_t cr = c.mem_read16(bda);
            cur_x_ = static_cast<uint8_t>(cr);
            cur_y_ = static_cast<uint8_t>(cr >> 8);
            if (cur_x_ > 79u)
            {
                cur_x_ = 79;
            }
            if (cur_y_ > 24u)
            {
                cur_y_ = 24;
            }
            const bool handled = int10();
            c.mem_write16(bda, static_cast<uint16_t>(
                                   (static_cast<uint16_t>(cur_y_) << 8) | cur_x_));
            return handled;
        }
        return false;
    }
    if (vector == 0x10)
    {
        return int10();
    }
    if (vector == 0x13)
    {
        return int13();
    }
    if (vector == 0x16)
    {
        return int16();
    }
    if (vector == 0x1A)
    {
        return int1a();
    }
    return false;
}

uint32_t pc::ticks_18hz() const
{
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0_)
                        .count();
    /* BIOS INT 1Ah: ~18.2 ticks/s. */
    return static_cast<uint32_t>((static_cast<uint64_t>(ms) * 182u) / 10000u);
}

bool pc::int1a()
{
    const uint8_t ah = static_cast<uint8_t>(c.ax() >> 8);
    const auto ms = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0_)
            .count());
    if (ah == 0xFF)
    {
        /* FloppyOS DEBUG stamp: milliseconds since boot, AL=86h signature. */
        c.set_cx(static_cast<uint16_t>(ms >> 16));
        c.set_dx(static_cast<uint16_t>(ms));
        c.set_ax(0xFF86);
        c.set_cf(false);
        return true;
    }
    if (ah == 0x00)
    {
        const uint32_t t = ticks_18hz();
        c.set_cx(static_cast<uint16_t>(t >> 16));
        c.set_dx(static_cast<uint16_t>(t));
        c.set_ax(static_cast<uint16_t>(c.ax() & 0xFF00u)); /* AL=0, not midnight */
        c.set_cf(false);
        return true;
    }
    c.set_cf(false);
    return true;
}

void pc::tty_scroll()
{
    for (uint32_t i = 0; i < 24u * 80u; i++)
    {
        const uint32_t d = 0xB8000u + i * 2u;
        const uint32_t s = 0xB8000u + (i + 80u) * 2u;
        c.mem_write8(d, c.mem_read8(s));
        c.mem_write8(d + 1u, c.mem_read8(s + 1u));
    }
    for (uint32_t i = 24u * 80u; i < 25u * 80u; i++)
    {
        c.mem_write8(0xB8000u + i * 2u, static_cast<uint8_t>(' '));
        c.mem_write8(0xB8000u + i * 2u + 1u, 0x07);
    }
}

void pc::tty_cell(uint8_t ch)
{
    if (ch == '\r')
    {
        cur_x_ = 0;
        tty_.push_back('\r');
        return;
    }
    if (ch == '\n')
    {
        cur_y_ = static_cast<uint8_t>(cur_y_ + 1u);
        if (cur_y_ >= 25u)
        {
            cur_y_ = 24;
            tty_scroll();
        }
        tty_.push_back('\n');
        return;
    }
    if (ch == 8)
    {
        if (cur_x_ > 0)
        {
            cur_x_ = static_cast<uint8_t>(cur_x_ - 1u);
        }
        return;
    }
    if (ch < 32)
    {
        return;
    }
    const uint32_t off = (static_cast<uint32_t>(cur_y_) * 80u + cur_x_) * 2u;
    c.mem_write8(0xB8000u + off, ch);
    c.mem_write8(0xB8000u + off + 1u, 0x07);
    tty_.push_back(static_cast<char>(ch));
    cur_x_ = static_cast<uint8_t>(cur_x_ + 1u);
    if (cur_x_ >= 80u)
    {
        cur_x_ = 0;
        cur_y_ = static_cast<uint8_t>(cur_y_ + 1u);
        if (cur_y_ >= 25u)
        {
            cur_y_ = 24;
            tty_scroll();
        }
    }
}

bool pc::int10()
{
    const uint8_t ah = static_cast<uint8_t>(c.ax() >> 8);
    const uint8_t al = static_cast<uint8_t>(c.ax());
    if (ah == 0x00)
    {
        video_mode_ = static_cast<uint8_t>(al & 0x7Fu);
        cur_x_ = 0;
        cur_y_ = 0;
        text_fill(c, 0, 0, 24, 79, static_cast<uint8_t>(' '), 0x07);
        c.set_cf(false);
        return true;
    }
    if ((ah == 0x06) || (ah == 0x07)) /* scroll; AL=0 clears the window */
    {
        const uint8_t at = static_cast<uint8_t>(c.bx() >> 8); /* BH */
        const uint8_t cl = static_cast<uint8_t>(c.cx());
        const uint8_t ch = static_cast<uint8_t>(c.cx() >> 8);
        const uint8_t dl = static_cast<uint8_t>(c.dx());
        const uint8_t dh = static_cast<uint8_t>(c.dx() >> 8);
        if ((al == 0) || (ah == 0x06))
        {
            text_scroll_up(c, al, ch, cl, dh, dl, at);
        }
        else
        {
            /* AH=07 scroll down: treat as fill when AL=0 (already handled). */
            text_scroll_up(c, al, ch, cl, dh, dl, at);
        }
        c.set_cf(false);
        return true;
    }
    if (ah == 0x02) /* set cursor DH=row DL=col */
    {
        cur_y_ = static_cast<uint8_t>(c.dx() >> 8);
        cur_x_ = static_cast<uint8_t>(c.dx());
        if (cur_x_ > 79u)
        {
            cur_x_ = 79;
        }
        if (cur_y_ > 24u)
        {
            cur_y_ = 24;
        }
        c.set_cf(false);
        return true;
    }
    if (ah == 0x03) /* get cursor → DH=row DL=col */
    {
        c.set_dx(static_cast<uint16_t>((static_cast<uint16_t>(cur_y_) << 8) | cur_x_));
        c.set_cx(0x0607);
        c.set_cf(false);
        return true;
    }
    if ((ah == 0x09) || (ah == 0x0A)) /* write char at cursor, do not move */
    {
        const uint8_t attr = (ah == 0x09) ? static_cast<uint8_t>(c.bx()) : 0x07;
        uint16_t n = c.cx();
        if (n == 0)
        {
            n = 1;
        }
        uint32_t x = cur_x_;
        const uint32_t y = cur_y_;
        for (uint16_t i = 0; i < n; i++)
        {
            if (x > 79u)
            {
                break;
            }
            const uint32_t off = (y * 80u + x) * 2u;
            c.mem_write8(0xB8000u + off, al);
            if (ah == 0x09)
            {
                c.mem_write8(0xB8000u + off + 1u, attr);
            }
            x++;
        }
        c.set_cf(false);
        return true;
    }
    if (ah == 0x0E)
    {
        tty_cell(al);
        c.set_cf(false);
        return true;
    }
    c.set_cf(false);
    return true;
}

static const char *int13_ah_name(uint8_t ah)
{
    switch (ah)
    {
    case 0x00:
        return "reset";
    case 0x01:
        return "status";
    case 0x02:
        return "read";
    case 0x03:
        return "write";
    case 0x04:
        return "verify";
    case 0x05:
        return "format";
    case 0x08:
        return "params";
    case 0x09:
        return "init";
    case 0x0C:
        return "seek";
    case 0x10:
        return "ready";
    case 0x15:
        return "dasd";
    case 0x16:
        return "change";
    default:
        return "?";
    }
}

bool pc::int13()
{
    const uint8_t ah = static_cast<uint8_t>(c.ax() >> 8);
    const uint8_t al = static_cast<uint8_t>(c.ax());
    const uint8_t ch = static_cast<uint8_t>(c.cx() >> 8);
    const uint8_t cl = static_cast<uint8_t>(c.cx());
    const uint8_t dh = static_cast<uint8_t>(c.dx() >> 8);
    const uint8_t dl = static_cast<uint8_t>(c.dx());
    if (log_active())
    {
        const uint32_t cyl =
            static_cast<uint32_t>(ch) | (static_cast<uint32_t>(cl & 0xC0u) << 2);
        const uint32_t sec = static_cast<uint32_t>(cl & 0x3Fu);
        log_trace("INT13 AH=%02X %s AL=%02X CHS=%u/%u/%u DL=%02X ES:BX=%04X:%04X%s\n", ah,
                  int13_ah_name(ah), al, cyl, static_cast<unsigned>(dh), sec, dl, c.es(),
                  c.bx(), ((dl & 0x80u) != 0) ? " pass" : " handled");
    }
    if ((dl & 0x80u) != 0)
    {
        /* HDD: Xebec ROS / option ROM. Do not map C: onto A:. */
        return false;
    }
    if (ah == 0x00)
    {
        c.set_ax(static_cast<uint16_t>(c.ax() & 0x00FFu)); /* AH=0 */
        c.set_cf(false);
        return true;
    }
    if (ah == 0x02)
    {
        const uint8_t nsec = (al == 0) ? 1 : al;
        const uint8_t sec = static_cast<uint8_t>(cl & 0x3Fu);
        const std::vector<uint8_t> *media = floppy_media(dl);
        const uint32_t lba = chs_lba(ch, dh, sec, dl);
        const uint32_t off = lba * 512u;
        const uint32_t bytes = static_cast<uint32_t>(nsec) * 512u;
        if ((media == nullptr) || (lba == UINT32_MAX) || (off + bytes > media->size()))
        {
            c.set_ax(0x0400);
            c.set_cf(true);
            return true;
        }
        uint32_t dst = cpu::phys(c.es(), c.bx());
        for (uint32_t i = 0; i < bytes; i++)
        {
            c.mem_write8(dst + i, (*media)[off + i]);
        }
        c.set_ax(nsec); /* AH=0 AL=count */
        c.set_cf(false);
        return true;
    }
    if (ah == 0x03)
    {
        const uint8_t nsec = (al == 0) ? 1 : al;
        const uint8_t sec = static_cast<uint8_t>(cl & 0x3Fu);
        const uint8_t unit = static_cast<uint8_t>(dl & 3u);
        if ((unit > 1u) || (floppy_[unit].size() < 512u))
        {
            c.set_ax(0x0400);
            c.set_cf(true);
            return true;
        }
        const uint32_t lba = chs_lba(ch, dh, sec, dl);
        const uint32_t off = lba * 512u;
        const uint32_t bytes = static_cast<uint32_t>(nsec) * 512u;
        if ((lba == UINT32_MAX) || (off + bytes > floppy_[unit].size()))
        {
            c.set_ax(0x0400);
            c.set_cf(true);
            return true;
        }
        uint32_t src = cpu::phys(c.es(), c.bx());
        for (uint32_t i = 0; i < bytes; i++)
        {
            floppy_[unit][off + i] = c.mem_read8(src + i);
        }
        c.set_ax(nsec);
        c.set_cf(false);
        return true;
    }
    if (ah == 0x08)
    {
        const uint8_t unit = static_cast<uint8_t>(dl & 3u);
        if ((unit > 1u) || (floppy_[unit].size() < 512u))
        {
            c.set_ax(0x0100);
            c.set_cf(true);
            return true;
        }
        const size_t n = floppy_[unit].size();
        uint32_t spt32 = 9;
        uint32_t heads32 = 2;
        floppy_geom(n, &spt32, &heads32);
        const uint8_t spt = static_cast<uint8_t>(spt32);
        const uint8_t heads = static_cast<uint8_t>(heads32);
        const uint32_t denom = spt32 * heads32;
        uint16_t cyls = 0;
        if (denom != 0u)
        {
            cyls = static_cast<uint16_t>((n / 512u) / denom);
        }
        const uint8_t maxc = (cyls > 1u) ? static_cast<uint8_t>(cyls - 1u) : 0;
        uint8_t ndrv = 1;
        if (floppy_[1].size() >= 512u)
        {
            ndrv = 2;
        }
        c.set_ax(0);
        c.set_bx(0);
        c.set_cx(static_cast<uint16_t>((static_cast<uint16_t>(maxc) << 8) | spt));
        c.set_dx(static_cast<uint16_t>((static_cast<uint16_t>(heads - 1u) << 8) | ndrv));
        c.set_cf(false);
        return true;
    }
    c.set_ax(0x0100);
    c.set_cf(true);
    return true;
}

bool pc::int16()
{
    const uint8_t ah = static_cast<uint8_t>(c.ax() >> 8);
    if (ah == 0x01)
    {
        if (kbd_.empty())
        {
            c.set_zf(true);
            return true;
        }
        c.set_zf(false);
        uint16_t axv = kbd_.front();
        if (!kbd_scan_.empty())
        {
            axv = static_cast<uint16_t>(axv | (static_cast<uint16_t>(kbd_scan_.front()) << 8));
        }
        c.set_ax(axv);
        return true;
    }
    if (ah == 0x00)
    {
        if (kbd_.empty())
        {
            waiting_key_ = true;
            c.set_ip(static_cast<uint16_t>(c.ip() - 2u)); /* rewind CD 16 */
            return true;
        }
        waiting_key_ = false;
        const uint8_t ch = kbd_.front();
        kbd_.pop_front();
        uint8_t scan = 0;
        if (!kbd_scan_.empty())
        {
            scan = kbd_scan_.front();
            kbd_scan_.pop_front();
        }
        c.set_zf(false);
        c.set_ax(static_cast<uint16_t>(ch) | (static_cast<uint16_t>(scan) << 8));
        return true;
    }
    /* Other INT 16: no key. */
    c.set_zf(true);
    return true;
}

template <bool Log>
void pc::bind_io()
{
    c.set_io(
        [this](uint16_t p) -> uint8_t {
            const uint8_t v = in_port(p);
            if constexpr (Log)
            {
                if (((p >= 0x320u) && (p <= 0x323u)) || (p == 0x213u))
                {
                    log_trace("IN  %03X=%02X\n", static_cast<unsigned>(p), v);
                }
            }
            return v;
        },
        [this](uint16_t p, uint8_t v) {
            if constexpr (Log)
            {
                if (((p >= 0x320u) && (p <= 0x323u)) || (p == 0x213u))
                {
                    log_trace("OUT %03X,%02X\n", static_cast<unsigned>(p), v);
                }
            }
            out_port(p, v);
        });
}

template void pc::bind_io<false>();
template void pc::bind_io<true>();

void pc::wire_pc_hw(bool trace_io)
{
    hw_ = {};
    crtc_reset(&mda_, 1);
    crtc_reset(&cga_, 0);
    hw_.ppi.control = 0x99;
    hw_.ppi.dip = 0x2D; /* CGA 80×25, 64K planar, 1 FDD, IPL (not Py86 MDA 0x3D) */
    sync_fdd_dip();
    /* Py86 ppi8255 / PCBIOS.ASM TEST.11: planar 64K (SW1 bits 3–2) plus
     * I/O-channel nibble*32K. 1981 BIOS max is 544K (nibble 0xF), not 640K
     * (SW2 bit 5 is ignored until the 27-OCT-82 BIOS). 0x6 was 256K. */
    hw_.ppi.io_nibble = 0x0F;
    hw_.pic.imr = 0xFF;
    hw_.dma.mask = 0x0F; /* 8237 reset: all channels masked */
    hw_.pit.ch[0].gate = 1; /* PC: ch0 GATE tied high */
    hw_.pic.vector_base = 8;
    hw_.pic.read_isr = 1;
    for (uint8_t i = 0; i < 3u; i++)
    {
        hw_.pit.ch[i].out_pin = 1;
        hw_.pit.ch[i].rw_mode = 3;
        hw_.pit.ch[i].lsb_toggle = 1;
        hw_.pit.ch[i].gate = 1;
    }
    hw_.dma.mask = 0x0F;
    hdc_.bind_bus(&c, &hw_.dma);
    hw_.fdc.msr = 0x80;
    {
        uint32_t spt = 9;
        uint32_t heads = 2;
        floppy_geom(floppy_[0].size(), &spt, &heads);
        hw_.fdc.spt = static_cast<uint8_t>(spt);
        hw_.fdc.heads = static_cast<uint8_t>(heads);
    }
    if (trace_io)
    {
        bind_io<true>();
    }
    else
    {
        bind_io<false>();
    }
    c.set_pc_hooks(
        this,
        [](void *p) { static_cast<pc *>(p)->after_insn(); },
        [](void *p, cpu &, uint8_t v) { return static_cast<pc *>(p)->bios_int(v); });
}

void pc::enable_fast_post()
{
    /* Warm boot: BDA 40:72 and BX = 1234h (ROS mov [72],bx / cmp at E1AE).
     * CS:IP = F000:E199 — after 64K fill, before 8259 ICW. jz E1F2 then
     * skips expansion STGTST. Do not start at E1F2 (that skips ICW). */
    c.mem_write8(0x472u, 0x34);
    c.mem_write8(0x473u, 0x12);
    c.set_bx(0x1234);
    c.set_cs(0xF000);
    c.set_ip(0xE199);
    c.mem_write8(0x475u, 1); /* one HDD; ROS may raise this */
    /* 10JAN86 programs DRAM-refresh DMA at E13C–E14C *before* E199.
     * Without ch0 unmasked, POST ECA9 waits forever on IN 00h. */
    hw_.dma.mode[0] = 0x58;
    hw_.dma.curr_addr[0] = 0;
    hw_.dma.base_addr[0] = 0;
    hw_.dma.curr_count[0] = 0xFFFF;
    hw_.dma.base_count[0] = 0xFFFF;
    hw_.dma.command = 0;
    hw_.dma.mask = 0x0E;
}

void pc::enable_audio(bool on)
{
    audio_enabled_ = on;
}

void pc::speaker_rising()
{
    if (!audio_enabled_)
    {
        return;
    }
    const uint16_t reload = hw_.pit.ch[2].reload;
    const unsigned hz =
        (reload != 0) ? static_cast<unsigned>(1193182u / static_cast<unsigned>(reload)) : 0u;
    (void)hz;
    beep_count_++;
    (void)std::fputc('\a', stderr);
}

void pc::pic_assert(uint8_t irq)
{
    if (irq > 7u)
    {
        return;
    }
    const uint8_t bit = static_cast<uint8_t>(1u << irq);
    if (hw_.pic.trigger_mode == 0)
    {
        if ((hw_.pic.irq_lines & bit) == 0)
        {
            hw_.pic.irr = static_cast<uint8_t>(hw_.pic.irr | bit);
        }
        hw_.pic.irq_lines = static_cast<uint8_t>(hw_.pic.irq_lines | bit);
    }
    else
    {
        hw_.pic.irr = static_cast<uint8_t>(hw_.pic.irr | bit);
        hw_.pic.irq_lines = static_cast<uint8_t>(hw_.pic.irq_lines | bit);
    }
}

void pc::pic_deassert(uint8_t irq)
{
    if (irq > 7u)
    {
        return;
    }
    const uint8_t bit = static_cast<uint8_t>(1u << irq);
    hw_.pic.irq_lines = static_cast<uint8_t>(hw_.pic.irq_lines & static_cast<uint8_t>(~bit));
    if (hw_.pic.trigger_mode != 0)
    {
        hw_.pic.irr = static_cast<uint8_t>(hw_.pic.irr & static_cast<uint8_t>(~bit));
    }
}

void pc::pic_write_cmd(uint8_t v)
{
    if ((v & 0x10u) != 0)
    {
        hw_.pic.icw_step = 1;
        hw_.pic.initialized = 0;
        hw_.pic.imr = 0; /* Py86 018aa8919: ICW1 clears IMR */
        hw_.pic.isr = 0;
        hw_.pic.irr = 0;
        hw_.pic.irq_lines = 0;
        hw_.pic.trigger_mode = static_cast<uint8_t>((v & 0x08u) != 0 ? 1 : 0);
        hw_.pic.icw4_needed = static_cast<uint8_t>((v & 0x01u) != 0 ? 1 : 0);
        hw_.pic.single_mode = static_cast<uint8_t>((v & 0x02u) != 0 ? 1 : 0);
        return;
    }
    if ((v & 0x20u) != 0)
    {
        if ((v & 0x40u) != 0)
        {
            const uint8_t bit = static_cast<uint8_t>(1u << (v & 7u));
            hw_.pic.isr = static_cast<uint8_t>(hw_.pic.isr & static_cast<uint8_t>(~bit));
        }
        else if (hw_.pic.isr != 0)
        {
            uint8_t irq = 0;
            while ((irq < 8u) && ((hw_.pic.isr & static_cast<uint8_t>(1u << irq)) == 0))
            {
                irq++;
            }
            if (irq < 8u)
            {
                hw_.pic.isr =
                    static_cast<uint8_t>(hw_.pic.isr & static_cast<uint8_t>(~(1u << irq)));
            }
        }
        return;
    }
    if ((v & 0x08u) != 0)
    {
        if ((v & 0x02u) != 0)
        {
            hw_.pic.read_isr = static_cast<uint8_t>((v & 0x01u) != 0 ? 1 : 0);
        }
    }
}

void pc::pic_write_data(uint8_t v)
{
    if (hw_.pic.initialized == 0)
    {
        if (hw_.pic.icw_step == 1)
        {
            hw_.pic.vector_base = static_cast<uint8_t>(v & 0xF8u);
            hw_.pic.icw_step = 2;
            return;
        }
        if (hw_.pic.icw_step == 2)
        {
            if (hw_.pic.icw4_needed != 0)
            {
                if (hw_.pic.single_mode != 0)
                {
                    hw_.pic.auto_eoi = static_cast<uint8_t>((v & 0x02u) != 0 ? 1 : 0);
                    hw_.pic.initialized = 1;
                    hw_.pic.icw_step = 0;
                }
                else
                {
                    hw_.pic.icw_step = 3;
                }
            }
            else
            {
                hw_.pic.initialized = 1;
                hw_.pic.icw_step = 0;
            }
            return;
        }
        if (hw_.pic.icw_step == 3)
        {
            hw_.pic.auto_eoi = static_cast<uint8_t>((v & 0x02u) != 0 ? 1 : 0);
            hw_.pic.initialized = 1;
            hw_.pic.icw_step = 0;
            return;
        }
        /* icw_step==0: OCW1 even if ICW never finished (5160 TST6). */
    }
    hw_.pic.imr = v;
    pic_holdoff_ = 1;
}

void pc::ppi_on_port_b()
{
    ppi8255 &p = hw_.ppi;
    const uint8_t prev = p.last_b;
    const uint8_t now = p.port_b;
    hw_.pit.ch[2].gate = static_cast<uint8_t>((now & 0x01u) != 0 ? 1 : 0);
    const bool was_audible = ((prev & 0x03u) == 0x03u);
    const bool now_audible = ((now & 0x03u) == 0x03u);
    if (now_audible && !was_audible)
    {
        speaker_rising();
    }
    const bool clock_now_high = (now & 0x40u) != 0;
    const bool kbd_en = (now & 0x80u) == 0;
    /* Clock low = XT kbd reset. BAT 0xAA is presented when clock is high and
     * PB7 enables the interface (F9E3: 08h → C8h → 48h). Same-write PB7 ack
     * on C8h used to clear AA before POST IN 60h (error 301). */
    if ((now & 0x40u) == 0)
    {
        kbd_bat_ = 1;
    }
    if ((kbd_bat_ != 0) && kbd_en && clock_now_high)
    {
        p.kbd_data = 0xAA;
        p.kbd_ready = 1;
        p.kbd_irq_pend = 1;
        kbd_irq_delay_ = k_xt_kbd_irq_delay;
        kbd_bat_ = 0;
#ifndef NDEBUG
        fd_printf(STDERR_FILENO, "KBD BAT AA\n");
        log_trace("KBD BAT AA\n");
#endif
        pic_deassert(1);
        ppi_tick();
    }
    const bool kbd_sel = kbd_en;
    if (kbd_sel && clock_now_high && (p.kbd_irq_pend != 0))
    {
        ppi_tick();
    }
    const bool pb7_was = (prev & 0x80u) != 0;
    const bool pb7_now = (now & 0x80u) != 0;
    if (!pb7_was && pb7_now)
    {
        /* Keyboard ack: 5150 INT 9 OR AL,80h / OUT 61h. Port A goes idle so
         * POST's stuck-key IN 60h (E3FE) reads 0, not leftover BAT 0xAA. */
        p.kbd_ready = 0;
        p.kbd_irq_pend = 0;
        p.kbd_data = 0;
        kbd_irq_delay_ = 0;
        pic_deassert(1);
    }
    if (pb7_was && !pb7_now)
    {
        /* PB7 falling: present next queued scancode (make/break). */
        if (!scanq_.empty())
        {
            p.kbd_data = scanq_.front();
            scanq_.pop_front();
            p.kbd_ready = 1;
            p.kbd_irq_pend = 1;
            kbd_irq_delay_ = k_xt_kbd_irq_delay;
        }
    }
}

void pc::ppi_tick()
{
    ppi8255 &p = hw_.ppi;
    /* Keyboard only: speaker GATE/DATA are OUT 61h + pit_tick(ch2). */
    if (scanq_.empty() && (p.kbd_irq_pend == 0))
    {
        return;
    }
    /* PB6+PB7=0: XT clock enable. scanq drain still needs that (real kbd).
     * IRQ1 for an already-latched byte does not: tdxctl injects sit in
     * kbd_data with PB6 often clear (M1TP CGAGRPH hooks INT 9). */
    const bool kbd_sel = ((p.port_b & 0x80u) == 0) && ((p.port_b & 0x40u) != 0);
    if ((p.kbd_ready == 0) && (!scanq_.empty()) && kbd_sel)
    {
        p.kbd_data = scanq_.front();
        scanq_.pop_front();
        p.kbd_ready = 1;
        p.kbd_irq_pend = 1;
        kbd_irq_delay_ = k_xt_kbd_irq_delay;
        pic_deassert(1);
    }
    if ((p.kbd_irq_pend == 0) || ((p.port_b & 0x80u) != 0))
    {
        return;
    }
    if (kbd_irq_delay_ != 0)
    {
        kbd_irq_delay_ = static_cast<uint8_t>(kbd_irq_delay_ - 1u);
        return;
    }
    if ((hw_.pic.imr & 0x02u) == 0)
    {
        pic_deassert(1);
        pic_assert(1);
        /* Keep kbd_irq_pend until PB7 ack. F9E3 does OUT 21h,FDh then
         * MOV [046B],0; if we fire IRQ1 on the OUT, dummy sets 046B and
         * the MOV clears it → 301 / F1. */
    }
}

void pc::pit_write(uint16_t port, uint8_t v)
{
    const uint16_t p = static_cast<uint16_t>(port & 3u);
    if (p == 3u)
    {
        const uint8_t idx = static_cast<uint8_t>((v >> 6) & 3u);
        if (idx == 3u)
        {
            return;
        }
        pit_counter &ch = hw_.pit.ch[idx];
        if ((v & 0x30u) == 0)
        {
            ch.latch = ch.count;
            ch.latched = 1;
            return;
        }
        ch.rw_mode = static_cast<uint8_t>((v >> 4) & 3u);
        uint8_t mode = static_cast<uint8_t>((v >> 1) & 7u);
        if (mode > 5u)
        {
            mode = static_cast<uint8_t>(mode - 4u);
        }
        ch.mode = mode;
        ch.bcd = static_cast<uint8_t>(v & 1u);
        ch.lsb_toggle = 1;
        ch.running = 0;
        ch.out_pin = 1;
        ch.latched = 0;
        ch.reload = 0;
        ch.count = 0;
        return;
    }
    pit_counter &ch = hw_.pit.ch[p];
    bool should_load = false;
    if (ch.rw_mode == 1u)
    {
        ch.reload = v;
        should_load = true;
        ch.lsb_toggle = 1;
    }
    else if (ch.rw_mode == 2u)
    {
        ch.reload = static_cast<uint16_t>(static_cast<uint16_t>(v) << 8);
        should_load = true;
        ch.lsb_toggle = 1;
    }
    else
    {
        if (ch.lsb_toggle != 0)
        {
            ch.reload = static_cast<uint16_t>((ch.reload & 0xFF00u) | v);
        }
        else
        {
            ch.reload = static_cast<uint16_t>((ch.reload & 0x00FFu) | (static_cast<uint16_t>(v) << 8));
        }
        ch.lsb_toggle = static_cast<uint8_t>(ch.lsb_toggle ^ 1u);
        should_load = ch.lsb_toggle != 0;
    }
    if (should_load)
    {
        ch.count = ch.reload;
        if ((ch.mode != 1u) && (ch.mode != 5u))
        {
            ch.running = 1;
        }
        if (ch.mode == 0)
        {
            ch.out_pin = 0;
        }
    }
}

uint8_t pc::pit_read(uint16_t port)
{
    const uint16_t p = static_cast<uint16_t>(port & 3u);
    if (p == 3u)
    {
        return 0xFF;
    }
    pit_counter &ch = hw_.pit.ch[p];
    const uint16_t val = (ch.latched != 0) ? ch.latch : ch.count;
    uint8_t ret = 0;
    if (ch.rw_mode == 1u)
    {
        ret = static_cast<uint8_t>(val);
        ch.latched = 0;
        return ret;
    }
    if (ch.rw_mode == 2u)
    {
        ret = static_cast<uint8_t>(val >> 8);
        ch.latched = 0;
        return ret;
    }
    if (ch.lsb_toggle != 0)
    {
        ret = static_cast<uint8_t>(val);
    }
    else
    {
        ret = static_cast<uint8_t>(val >> 8);
        ch.latched = 0;
    }
    ch.lsb_toggle = static_cast<uint8_t>(ch.lsb_toggle ^ 1u);
    return ret;
}

void pc::wall_clock_irq0()
{
    /* Do not latch IRQ0 until the guest has programmed 8253 ch0. tdx pit_poll
     * used to pulse IRQ0 during 5160 POST; unmask-all then took the dummy
     * ISR which wrote IMR=01 and failed TST6 (CLI+HLT at E38C). */
    if (hw_.pit.ch[0].running == 0)
    {
        return;
    }
    pic_deassert(0);
    pic_assert(0);
}

void pc::pit_tick()
{
    for (uint8_t i = 0; i < 3u; i++)
    {
        pit_counter &ch = hw_.pit.ch[i];
        if ((ch.running == 0) || (ch.gate == 0))
        {
            continue;
        }
        const uint8_t prev_out = ch.out_pin;
        ch.count = static_cast<uint16_t>(ch.count - 1u);
        if (ch.count == 0)
        {
            /* BIOS ch0 reload 0 = 18.2 Hz. Instruction-cadence IRQ0 is far
             * faster than a real 8088 on release tdx, so TANK.COM's INT 1Ch
             * watchdog (cdd budget ~56 ticks) hits FATAL ERROR: 3. Pulse IRQ0
             * from wall_clock_irq0 (~55 ms) instead. Small reloads still fire
             * here (iron86 tests, PC speaker). */
            const bool irq0_insn = ((i == 0) && (ch.reload != 0));
            if (ch.mode == 0)
            {
                ch.out_pin = 1;
                if (irq0_insn)
                {
                    pic_deassert(0);
                    pic_assert(0);
                }
            }
            else if (ch.mode == 2)
            {
                ch.count = (ch.reload != 0) ? ch.reload : 0;
                if (irq0_insn)
                {
                    pic_deassert(0);
                    pic_assert(0);
                }
            }
            else if (ch.mode == 3)
            {
                ch.out_pin = static_cast<uint8_t>(ch.out_pin ^ 1u);
                ch.count = ch.reload;
                if (irq0_insn && (prev_out != 0) && (ch.out_pin == 0))
                {
                    pic_deassert(0);
                    pic_assert(0);
                }
            }
            else
            {
                ch.running = 0;
            }
        }
        else if ((ch.mode == 3) && (ch.reload != 0))
        {
            const uint16_t half = static_cast<uint16_t>((ch.reload + 1u) / 2u);
            if (ch.count == half)
            {
                ch.out_pin = static_cast<uint8_t>(ch.out_pin ^ 1u);
                if ((i == 0) && (ch.reload != 0) && (prev_out != 0) && (ch.out_pin == 0))
                {
                    pic_deassert(0);
                    pic_assert(0);
                }
            }
        }
    }
}

void pc::dma_out(uint16_t port, uint8_t v)
{
    const uint16_t p = static_cast<uint16_t>(port & 0x0Fu);
    if (p == 0x0Cu)
    {
        hw_.dma.byte_ptr = 0;
        return;
    }
    if (p == 0x0Du)
    {
        hw_.dma = {};
        hw_.dma.mask = 0x0F;
        return;
    }
    if (p == 0x08u)
    {
        hw_.dma.command = v;
        return;
    }
    if (p == 0x0Au)
    {
        const uint8_t ch = static_cast<uint8_t>(v & 3u);
        const uint8_t bit = static_cast<uint8_t>(1u << ch);
        if ((v & 0x04u) != 0)
        {
            hw_.dma.mask = static_cast<uint8_t>(hw_.dma.mask | bit);
        }
        else
        {
            hw_.dma.mask = static_cast<uint8_t>(hw_.dma.mask & static_cast<uint8_t>(~bit));
        }
        return;
    }
    if (p == 0x0Bu)
    {
        hw_.dma.mode[v & 3u] = v;
        return;
    }
    if (p == 0x0Eu)
    {
        hw_.dma.mask = 0;
        return;
    }
    if (p == 0x0Fu)
    {
        hw_.dma.mask = static_cast<uint8_t>(v & 0x0Fu);
        return;
    }
    if (p < 8u)
    {
        const uint8_t ch = static_cast<uint8_t>((p >> 1) & 3u);
        const bool count_reg = (p & 1u) != 0;
        uint16_t *dst = count_reg ? &hw_.dma.curr_count[ch] : &hw_.dma.curr_addr[ch];
        uint16_t *base = count_reg ? &hw_.dma.base_count[ch] : &hw_.dma.base_addr[ch];
        if (hw_.dma.byte_ptr == 0)
        {
            *dst = static_cast<uint16_t>((*dst & 0xFF00u) | v);
        }
        else
        {
            *dst = static_cast<uint16_t>((*dst & 0x00FFu) | (static_cast<uint16_t>(v) << 8));
        }
        *base = *dst;
        hw_.dma.byte_ptr = static_cast<uint8_t>(hw_.dma.byte_ptr ^ 1u);
    }
}

void pc::dma_tick()
{
    /* IBM PC DRAM refresh is 8237 ch0 auto-init. 5160 POST ECA9 waits for
     * IN 00h (ch0 current address) to change; without this it spins forever. */
    if ((hw_.dma.mask & 1u) != 0)
    {
        return;
    }
    if ((hw_.dma.command & 0x04u) != 0)
    {
        return;
    }
    hw_.dma.curr_addr[0] = static_cast<uint16_t>(hw_.dma.curr_addr[0] + 1u);
    if (hw_.dma.curr_count[0] == 0)
    {
        hw_.dma.status = static_cast<uint8_t>(hw_.dma.status | 1u);
        if ((hw_.dma.mode[0] & 0x10u) != 0)
        {
            hw_.dma.curr_addr[0] = hw_.dma.base_addr[0];
            hw_.dma.curr_count[0] = hw_.dma.base_count[0];
        }
    }
    else
    {
        hw_.dma.curr_count[0] = static_cast<uint16_t>(hw_.dma.curr_count[0] - 1u);
    }
}

uint8_t pc::dma_in(uint16_t port)
{
    const uint16_t p = static_cast<uint16_t>(port & 0x0Fu);
    if (p == 0x08u)
    {
        return hw_.dma.status;
    }
    if (p == 0x0Au)
    {
        return hw_.dma.mask;
    }
    if (p >= 8u)
    {
        return 0xFF;
    }
    const uint8_t ch = static_cast<uint8_t>((p >> 1) & 3u);
    const uint16_t val = ((p & 1u) != 0) ? hw_.dma.curr_count[ch] : hw_.dma.curr_addr[ch];
    uint8_t ret = 0;
    if (hw_.dma.byte_ptr == 0)
    {
        ret = static_cast<uint8_t>(val);
    }
    else
    {
        ret = static_cast<uint8_t>(val >> 8);
    }
    hw_.dma.byte_ptr = static_cast<uint8_t>(hw_.dma.byte_ptr ^ 1u);
    return ret;
}

void pc::after_insn()
{
    hw_.pit_div = static_cast<uint8_t>(hw_.pit_div + 1u);
    if ((hw_.pit_div & 3u) == 0)
    {
        /* BDA 40:49: 7 = MDA, else CGA. IN 3BA/3DA still tick in crtc_read_status. */
        if (c.mem_read8(0x449u) == 7u)
        {
            crtc_tick(&mda_);
        }
        else
        {
            crtc_tick(&cga_);
        }
    }
    if ((hw_.pit_div & 1u) == 0)
    {
        pit_tick();
    }
    dma_tick();
    /* Skip when scanq empty, no IRQ1 pending, and speaker GATE+DATA off. */
    if ((!scanq_.empty()) || (hw_.ppi.kbd_irq_pend != 0) || ((hw_.ppi.port_b & 0x03u) != 0))
    {
        ppi_tick();
    }
    if ((c.flags() & k_flag_if) == 0)
    {
        return;
    }
    if (pic_holdoff_ != 0)
    {
        pic_holdoff_ = 0;
        return;
    }
    const uint8_t pending = static_cast<uint8_t>(hw_.pic.irr & static_cast<uint8_t>(~hw_.pic.imr));
    if (pending == 0)
    {
        return;
    }
    uint8_t irq = 0;
    while ((irq < 8u) && ((pending & static_cast<uint8_t>(1u << irq)) == 0))
    {
        irq++;
    }
    if (irq >= 8u)
    {
        return;
    }
    hw_.pic.irr = static_cast<uint8_t>(hw_.pic.irr & static_cast<uint8_t>(~(1u << irq)));
    hw_.pic.isr = static_cast<uint8_t>(hw_.pic.isr | static_cast<uint8_t>(1u << irq));
    if (hw_.pic.auto_eoi != 0)
    {
        hw_.pic.isr = static_cast<uint8_t>(hw_.pic.isr & static_cast<uint8_t>(~(1u << irq)));
    }
    c.raise_intr(static_cast<uint8_t>(hw_.pic.vector_base + irq));
}



void pc::fdc_dma_to_mem(const uint8_t *data, size_t n)
{
    if ((data == nullptr) || (n == 0))
    {
        hw_.fdc.dma_more = 0;
        return;
    }
    const uint32_t phys =
        (static_cast<uint32_t>(hw_.dma.page[2] & 0x0Fu) << 16) | hw_.dma.curr_addr[2];
    uint32_t remain = static_cast<uint32_t>(hw_.dma.curr_count[2]) + 1u;
    if (remain == 0)
    {
        remain = 0x10000u;
    }
    const size_t ncopy = (n < remain) ? n : static_cast<size_t>(remain);
    size_t i = 0;
    for (i = 0; i < ncopy; i++)
    {
        c.mem_write8((phys + static_cast<uint32_t>(i)) & 0xFFFFFu, data[i]);
    }
    hw_.dma.curr_addr[2] =
        static_cast<uint16_t>(hw_.dma.curr_addr[2] + static_cast<uint16_t>(ncopy));
    const uint16_t nc = static_cast<uint16_t>(hw_.dma.curr_count[2] - static_cast<uint16_t>(ncopy));
    hw_.dma.curr_count[2] = nc;
    if ((ncopy > 0) && (nc == 0xFFFFu))
    {
        hw_.dma.status = static_cast<uint8_t>(hw_.dma.status | 0x04u);
        hw_.fdc.dma_more = 0;
    }
    else
    {
        hw_.fdc.dma_more = (remain > ncopy) ? 1 : 0;
    }
}

void pc::fdc_write_dor(uint8_t v)
{
    const uint8_t prev = hw_.fdc.dor;
    hw_.fdc.dor = v;
    hw_.fdc.sel = static_cast<uint8_t>(v & 3u); /* bits 0–1 drive select */
    /* Py86: bits 4–7 motor enables (bit 4 = A, bit 5 = B). Kept in DOR. */
    if ((v & 0x04u) == 0)
    {
        hw_.fdc.cmd_n = 0;
        hw_.fdc.res_n = 0;
        hw_.fdc.res_i = 0;
        hw_.fdc.phase = 0;
        hw_.fdc.msr = 0x80;
        hw_.fdc.pending_ri = 0;
        pic_deassert(6);
        return;
    }
    if (((prev & 0x04u) == 0) && ((v & 0x04u) != 0))
    {
        hw_.fdc.cmd_n = 0;
        hw_.fdc.res_n = 0;
        hw_.fdc.res_i = 0;
        hw_.fdc.phase = 0;
        hw_.fdc.msr = 0x80;
        hw_.fdc.pending_ri = 4;
        hw_.fdc.st0 = 0xC0;
        pic_deassert(6);
        pic_assert(6);
    }
}

void pc::fdc_write_cmd(uint8_t v)
{
    if (hw_.fdc.phase != 0)
    {
        return;
    }
    if (hw_.fdc.cmd_n >= 16u)
    {
        return;
    }
    hw_.fdc.cmd[hw_.fdc.cmd_n] = v;
    hw_.fdc.cmd_n = static_cast<uint8_t>(hw_.fdc.cmd_n + 1u);
    const uint8_t cmd = static_cast<uint8_t>(hw_.fdc.cmd[0] & 0x1Fu);
    uint8_t need = 1;
    switch (cmd)
    {
    case 0x02:
    case 0x05:
    case 0x06:
    case 0x09:
    case 0x0C:
    case 0x11:
    case 0x19:
    case 0x1D:
        need = 9;
        break;
    case 0x03:
    case 0x0F:
        need = 3;
        break;
    case 0x04:
    case 0x07:
    case 0x0A:
    case 0x12:
        need = 2;
        break;
    case 0x0D:
        need = 6;
        break;
    case 0x13:
        need = 4;
        break;
    default:
        need = 1;
        break;
    }
    if (hw_.fdc.cmd_n == need)
    {
        fdc_exec();
    }
}

uint8_t pc::fdc_read_res()
{
    if ((hw_.fdc.phase != 1) || (hw_.fdc.res_i >= hw_.fdc.res_n))
    {
        return 0;
    }
    const uint8_t v = hw_.fdc.res[hw_.fdc.res_i];
    hw_.fdc.res_i = static_cast<uint8_t>(hw_.fdc.res_i + 1u);
    if (hw_.fdc.res_i >= hw_.fdc.res_n)
    {
        hw_.fdc.msr = 0x80;
        hw_.fdc.phase = 0;
        pic_deassert(6);
    }
    return v;
}

void pc::fdc_exec()
{
    const uint8_t cmd = static_cast<uint8_t>(hw_.fdc.cmd[0] & 0x1Fu);
    const uint8_t drv = (hw_.fdc.cmd_n > 1u) ? static_cast<uint8_t>(hw_.fdc.cmd[1] & 3u) : hw_.fdc.sel;
    const std::vector<uint8_t> *media = floppy_media(drv);
    const bool have = media != nullptr;
    hw_.fdc.msr = 0x10;
    hw_.fdc.cmd_n = 0;

    if (cmd == 0x03)
    {
        hw_.fdc.msr = 0x80;
        hw_.fdc.phase = 0;
        return;
    }
    if (cmd == 0x07)
    {
        hw_.fdc.cyl[drv] = 0;
        hw_.fdc.st0 = have ? static_cast<uint8_t>(0x28u | drv) : static_cast<uint8_t>(0x70u | drv);
        hw_.fdc.msr = 0x80;
        hw_.fdc.phase = 0;
        pic_deassert(6);
        pic_assert(6);
        return;
    }
    if (cmd == 0x0F)
    {
        const uint8_t cyl = hw_.fdc.cmd[2];
        hw_.fdc.cyl[drv] = cyl;
        hw_.fdc.st0 = have ? static_cast<uint8_t>(0x28u | drv) : static_cast<uint8_t>(0x70u | drv);
        hw_.fdc.msr = 0x80;
        hw_.fdc.phase = 0;
        pic_deassert(6);
        pic_assert(6);
        return;
    }
    if (cmd == 0x08)
    {
        if (hw_.fdc.pending_ri > 0)
        {
            const uint8_t slot = static_cast<uint8_t>(4u - hw_.fdc.pending_ri);
            hw_.fdc.pending_ri = static_cast<uint8_t>(hw_.fdc.pending_ri - 1u);
            hw_.fdc.res[0] = static_cast<uint8_t>(0xC0u | (slot & 3u));
            hw_.fdc.res[1] = 0;
        }
        else
        {
            hw_.fdc.res[0] = hw_.fdc.st0;
            hw_.fdc.res[1] = hw_.fdc.cyl[hw_.fdc.st0 & 3u];
            pic_deassert(6);
        }
        hw_.fdc.res_n = 2;
        hw_.fdc.res_i = 0;
        hw_.fdc.phase = 1;
        hw_.fdc.msr = 0xD0;
        pic_assert(6);
        return;
    }
    if (cmd == 0x04)
    {
        const uint8_t head = static_cast<uint8_t>((hw_.fdc.cmd[1] >> 2) & 1u);
        uint8_t st3 = static_cast<uint8_t>(drv | (head << 2));
        if (have)
        {
            st3 = static_cast<uint8_t>(st3 | 0x28u);
            if (hw_.fdc.cyl[drv] == 0)
            {
                st3 = static_cast<uint8_t>(st3 | 0x10u);
            }
        }
        hw_.fdc.res[0] = st3;
        hw_.fdc.res_n = 1;
        hw_.fdc.res_i = 0;
        hw_.fdc.phase = 1;
        hw_.fdc.msr = 0xD0;
        pic_assert(6);
        return;
    }
    if ((cmd == 0x06) || (cmd == 0x0C) || (cmd == 0x02))
    {
        const uint8_t cyl = hw_.fdc.cmd[2];
        const uint8_t head = static_cast<uint8_t>((hw_.fdc.cmd[1] >> 2) & 1u);
        uint8_t sec = hw_.fdc.cmd[4];
        const uint8_t n = hw_.fdc.cmd[5];
        uint8_t eot = hw_.fdc.cmd[6];
        uint8_t spt = 9;
        if (media != nullptr)
        {
            uint32_t spt32 = 9;
            uint32_t heads32 = 2;
            floppy_geom(media->size(), &spt32, &heads32);
            spt = static_cast<uint8_t>(spt32);
        }
        if (spt > eot)
        {
            eot = spt;
        }
        const uint32_t sec_sz = (n < 8u) ? (128u << n) : 512u;
        hw_.fdc.st0 = static_cast<uint8_t>(drv);
        hw_.fdc.st1 = 0;
        hw_.fdc.st2 = 0;
        if (!have)
        {
            hw_.fdc.st0 = static_cast<uint8_t>(0x40u | drv);
            hw_.fdc.st1 = 0x04;
        }
        else
        {
            uint8_t k = 0;
            for (k = 0; k < 64u; k++)
            {
                const uint32_t lba = chs_lba(cyl, head, sec, drv);
                const uint32_t off = lba * sec_sz;
                if ((lba == 0xFFFFFFFFu) || ((off + sec_sz) > media->size()))
                {
                    hw_.fdc.st0 = static_cast<uint8_t>(0x40u | drv);
                    hw_.fdc.st1 = static_cast<uint8_t>(hw_.fdc.st1 | 0x04u);
                    break;
                }
                fdc_dma_to_mem(media->data() + off, sec_sz);
                if (sec >= eot)
                {
                    break;
                }
                if (hw_.fdc.dma_more == 0)
                {
                    break;
                }
                sec = static_cast<uint8_t>(sec + 1u);
            }
        }
        hw_.fdc.res[0] = hw_.fdc.st0;
        hw_.fdc.res[1] = hw_.fdc.st1;
        hw_.fdc.res[2] = hw_.fdc.st2;
        hw_.fdc.res[3] = cyl;
        hw_.fdc.res[4] = head;
        hw_.fdc.res[5] = sec;
        hw_.fdc.res[6] = n;
        hw_.fdc.res_n = 7;
        hw_.fdc.res_i = 0;
        hw_.fdc.phase = 1;
        hw_.fdc.msr = 0xD0;
        pic_deassert(6);
        pic_assert(6);
        return;
    }
    if ((cmd == 0x05) || (cmd == 0x09))
    {
        hw_.fdc.st0 = have ? drv : static_cast<uint8_t>(0x40u | drv);
        hw_.fdc.st1 = have ? 0 : 0x04;
        hw_.fdc.st2 = 0;
        hw_.fdc.res[0] = hw_.fdc.st0;
        hw_.fdc.res[1] = hw_.fdc.st1;
        hw_.fdc.res[2] = hw_.fdc.st2;
        hw_.fdc.res[3] = hw_.fdc.cmd[2];
        hw_.fdc.res[4] = static_cast<uint8_t>((hw_.fdc.cmd[1] >> 2) & 1u);
        hw_.fdc.res[5] = hw_.fdc.cmd[4];
        hw_.fdc.res[6] = hw_.fdc.cmd[5];
        hw_.fdc.res_n = 7;
        hw_.fdc.res_i = 0;
        hw_.fdc.phase = 1;
        hw_.fdc.msr = 0xD0;
        pic_assert(6);
        return;
    }
    hw_.fdc.res[0] = 0x80;
    hw_.fdc.res_n = 1;
    hw_.fdc.res_i = 0;
    hw_.fdc.phase = 1;
    hw_.fdc.msr = 0xD0;
    pic_assert(6);
}

uint8_t pc::in_port(uint16_t port)
{
    const uint16_t p = port;
    if ((p == 0x20u) || (p == 0x21u))
    {
        if (p == 0x20u)
        {
            const uint8_t v = (hw_.pic.read_isr != 0) ? hw_.pic.isr : hw_.pic.irr;
#ifndef NDEBUG
            dbg_pic("IN", p, v, hw_.pic);
#endif
            return v;
        }
#ifndef NDEBUG
        dbg_pic("IN", p, hw_.pic.imr, hw_.pic);
#endif
        return hw_.pic.imr;
    }
    if ((p >= 0x40u) && (p <= 0x43u))
    {
        return pit_read(p);
    }
    if (p <= 0x0Fu)
    {
        return dma_in(p);
    }
    if ((p >= 0x60u) && (p <= 0x63u))
    {
        if (p == 0x60u)
        {
            if ((hw_.ppi.port_b & 0x80u) != 0)
            {
                return hw_.ppi.dip;
            }
            /* 5150 PPI Port A is a latch. BIOS INT 9 (F000:E987) does
             * IN 60h then pulses PB7 (OR 80h / OUT 61h / restore) to ack.
             * Clearing ready on the read broke wrap-INT9 games (M1TP
             * CGAGRPH: IN 60h, then CALL FAR the ROM handler). */
#ifndef NDEBUG
            if (c.rom_bios())
            {
                fd_printf(STDERR_FILENO, "KBD IN 60=%02X ready=%u\n", hw_.ppi.kbd_data,
                          static_cast<unsigned>(hw_.ppi.kbd_ready));
                log_trace("KBD IN 60=%02X ready=%u\n", hw_.ppi.kbd_data,
                          static_cast<unsigned>(hw_.ppi.kbd_ready));
            }
#endif
            return hw_.ppi.kbd_data;
        }
        if (p == 0x61u)
        {
            return hw_.ppi.port_b;
        }
        if (p == 0x62u)
        {
            /* 5160: PC0–3 are SW1 low/high nibble selected by PB2 (E211–E228). */
            const uint8_t sw = (hw_.ppi.dip != 0) ? hw_.ppi.dip : 0x2Du;
            uint8_t nibble = sw & 0x0Fu;
            if ((hw_.ppi.port_b & 0x04u) != 0)
            {
                nibble = static_cast<uint8_t>((sw >> 4) & 0x0Fu);
            }
            uint8_t port_c = nibble;
            if (hw_.pit.ch[2].out_pin != 0)
            {
                port_c = static_cast<uint8_t>(port_c | 0x20u);
            }
            return port_c;
        }
        return hw_.ppi.control;
    }
    if ((p >= 0x3B0u) && (p <= 0x3BFu))
    {
        if (p == 0x3B4u)
        {
            return mda_.index;
        }
        if (p == 0x3B5u)
        {
            return crtc_read_data(&mda_);
        }
        if (p == 0x3B8u)
        {
            return crtc_read_ctrl(&mda_);
        }
        if (p == 0x3BAu)
        {
            return crtc_read_status(&mda_);
        }
        if (p == 0x3BCu)
        {
            return mda_.lpt_data;
        }
        if (p == 0x3BDu)
        {
            return mda_.lpt_stat;
        }
        if (p == 0x3BEu)
        {
            return mda_.lpt_ctrl;
        }
        return 0xFFu;
    }
    if ((p >= 0x3D0u) && (p <= 0x3DFu))
    {
        if (p == 0x3D4u)
        {
            return cga_.index;
        }
        if (p == 0x3D5u)
        {
            return crtc_read_data(&cga_);
        }
        if (p == 0x3D8u)
        {
            return crtc_read_ctrl(&cga_);
        }
        if (p == 0x3DAu)
        {
            return crtc_read_status(&cga_);
        }
        return 0xFFu;
    }
    if ((p >= 0x3F0u) && (p <= 0x3F7u))
    {
        if (p == 0x3F2u)
        {
            return hw_.fdc.dor;
        }
        if (p == 0x3F4u)
        {
            return hw_.fdc.msr;
        }
        if (p == 0x3F5u)
        {
            return fdc_read_res();
        }
        return 0;
    }
    if (p == 0x81u)
    {
        return hw_.dma.page[2];
    }
    if (p == 0x82u)
    {
        return hw_.dma.page[3];
    }
    if (p == 0x83u)
    {
        return hw_.dma.page[1];
    }
    if (p == 0x87u)
    {
        return hw_.dma.page[0];
    }
    if (hdc_.attached() &&
        (((p >= k_hdc_port_data) && (p <= k_hdc_port_mask)) || (p == k_hdc_port_probe)))
    {
        const uint8_t v = hdc_.read_port(p);
        hdc_sync_irq();
        return v;
    }
    return 0xFF;
}

void pc::out_port(uint16_t port, uint8_t v)
{
    const uint16_t p = port;
    if (p == 0x20u)
    {
        pic_write_cmd(v);
#ifndef NDEBUG
        dbg_pic("OUT", p, v, hw_.pic);
#endif
        return;
    }
    if (p == 0x21u)
    {
        pic_write_data(v);
#ifndef NDEBUG
        dbg_pic("OUT", p, v, hw_.pic);
#endif
        return;
    }
    if ((p >= 0x40u) && (p <= 0x43u))
    {
        pit_write(p, v);
        return;
    }
    if (p <= 0x0Fu)
    {
        dma_out(p, v);
        return;
    }
    if ((p >= 0x60u) && (p <= 0x63u))
    {
        if (p == 0x63u)
        {
            hw_.ppi.control = v;
            return;
        }
        if (p == 0x60u)
        {
            hw_.ppi.port_a = v;
            return;
        }
        if (p == 0x61u)
        {
            hw_.ppi.last_b = hw_.ppi.port_b;
            hw_.ppi.port_b = v;
            ppi_on_port_b();
            return;
        }
        hw_.ppi.port_c = v;
        return;
    }
    if ((p >= 0x3B0u) && (p <= 0x3BFu))
    {
        if (p == 0x3B4u)
        {
            crtc_write_index(&mda_, v);
            return;
        }
        if (p == 0x3B5u)
        {
            crtc_write_data(&mda_, v);
            return;
        }
        if (p == 0x3B8u)
        {
            crtc_write_ctrl(&mda_, v);
            return;
        }
        if (p == 0x3BCu)
        {
            mda_.lpt_data = v;
            return;
        }
        if (p == 0x3BEu)
        {
            mda_.lpt_ctrl = v;
            return;
        }
        return;
    }
    if ((p >= 0x3D0u) && (p <= 0x3DFu))
    {
        if (p == 0x3D4u)
        {
            crtc_write_index(&cga_, v);
            return;
        }
        if (p == 0x3D5u)
        {
            crtc_write_data(&cga_, v);
            return;
        }
        if (p == 0x3D8u)
        {
            crtc_write_ctrl(&cga_, v);
            return;
        }
        return;
    }
    if (p == 0x3F2u)
    {
        fdc_write_dor(v);
        return;
    }
    if (p == 0x3F5u)
    {
        fdc_write_cmd(v);
        return;
    }
    if (p == 0x81u)
    {
        hw_.dma.page[2] = v;
        return;
    }
    if (p == 0x82u)
    {
        hw_.dma.page[3] = v;
        return;
    }
    if (p == 0x83u)
    {
        hw_.dma.page[1] = v;
        return;
    }
    if (p == 0x87u)
    {
        hw_.dma.page[0] = v;
        return;
    }
    if (hdc_.attached() &&
        (((p >= k_hdc_port_data) && (p <= k_hdc_port_mask)) || (p == k_hdc_port_probe)))
    {
        hdc_.write_port(p, v);
        hdc_sync_irq();
    }
}

} // namespace iron86
