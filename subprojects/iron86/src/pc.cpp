/**
 * @file pc.cpp
 * @brief INT 10h AH=0Eh, INT 13h AH=00/02, INT 16h AH=00/01, INT 1Ah ticks.
 */
#include "iron86/pc.h"

namespace iron86
{

bool pc::load_floppy(const uint8_t *img, size_t n)
{
    if ((img == nullptr) || (n < 512u))
    {
        return false;
    }
    floppy_.assign(img, img + n);
    c.reset();
    for (size_t i = 0; i < 512u; i++)
    {
        c.mem_write8(0x7C00u + static_cast<uint32_t>(i), floppy_[i]);
    }
    return true;
}

void pc::boot()
{
    t0_ = std::chrono::steady_clock::now();
    c.set_bios([this](cpu &, uint8_t v) { return bios_int(v); });
    c.set_cs(0);
    c.set_ds(0);
    c.set_es(0);
    c.set_ss(0);
    c.set_sp(0x7C00);
    c.set_ip(0x7C00);
    c.set_dx(0); /* DL = drive 0 */
}

void pc::type_keys(const char *s)
{
    if (s == nullptr)
    {
        return;
    }
    for (const char *p = s; *p != '\0'; p++)
    {
        kbd_.push_back(static_cast<uint8_t>(*p));
    }
}

uint32_t pc::chs_lba(uint8_t cyl, uint8_t head, uint8_t sec) const
{
    /* 360K: 2 heads, 9 spt, 40 cyl. sec is 1-based. */
    if (sec == 0)
    {
        return UINT32_MAX;
    }
    return (static_cast<uint32_t>(cyl) * 2u + head) * 9u + (static_cast<uint32_t>(sec) - 1u);
}

bool pc::bios_int(uint8_t vector)
{
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

bool pc::int10()
{
    const uint8_t ah = static_cast<uint8_t>(c.ax() >> 8);
    const uint8_t al = static_cast<uint8_t>(c.ax());
    if (ah == 0x0E)
    {
        if (al == '\r')
        {
            tty_.push_back('\r');
        }
        else if (al == '\n')
        {
            tty_.push_back('\n');
        }
        else if (al >= 32)
        {
            tty_.push_back(static_cast<char>(al));
        }
        c.set_cf(false);
        return true;
    }
    c.set_cf(false);
    return true;
}

bool pc::int13()
{
    const uint8_t ah = static_cast<uint8_t>(c.ax() >> 8);
    const uint8_t al = static_cast<uint8_t>(c.ax());
    const uint8_t ch = static_cast<uint8_t>(c.cx() >> 8);
    const uint8_t cl = static_cast<uint8_t>(c.cx());
    const uint8_t dh = static_cast<uint8_t>(c.dx() >> 8);
    const uint8_t dl = static_cast<uint8_t>(c.dx());
    (void)dl;
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
        const uint32_t lba = chs_lba(ch, dh, sec);
        const uint32_t off = lba * 512u;
        const uint32_t bytes = static_cast<uint32_t>(nsec) * 512u;
        if ((lba == UINT32_MAX) || (off + bytes > floppy_.size()))
        {
            c.set_ax(0x0400);
            c.set_cf(true);
            return true;
        }
        uint32_t dst = cpu::phys(c.es(), c.bx());
        for (uint32_t i = 0; i < bytes; i++)
        {
            c.mem_write8(dst + i, floppy_[off + i]);
        }
        c.set_ax(nsec); /* AH=0 AL=count */
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
        c.set_ax(kbd_.front()); /* AL=ascii, AH=0 scan */
        return true;
    }
    if (ah == 0x00)
    {
        if (kbd_.empty())
        {
            c.set_zf(true);
            c.set_ax(0);
            return true;
        }
        const uint8_t ch = kbd_.front();
        kbd_.pop_front();
        c.set_zf(false);
        c.set_ax(ch);
        return true;
    }
    /* Other INT 16: no key. */
    c.set_zf(true);
    return true;
}

} // namespace iron86
