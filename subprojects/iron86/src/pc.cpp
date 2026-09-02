/**
 * @file pc.cpp
 * @brief INT 10h AH=0Eh, INT 13h AH=00/02, INT 16h AH=00/01, INT 1Ah ticks.
 */
#include "iron86/pc.h"

#include <cstdio>

namespace iron86
{

bool pc::attach_floppy(const uint8_t *img, size_t n)
{
    if ((img == nullptr) || (n < 512u))
    {
        return false;
    }
    floppy_.assign(img, img + n);
    return true;
}

bool pc::load_floppy(const uint8_t *img, size_t n)
{
    if (!attach_floppy(img, n))
    {
        return false;
    }
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
    return 0;
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
    pic_deassert(1);
}

void pc::type_scan(uint8_t make)
{
    const uint8_t m = static_cast<uint8_t>(make & 0x7Fu);
    queue_scan(m);
    queue_scan(static_cast<uint8_t>(m | 0x80u));
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
        const uint8_t mk = ascii_make(static_cast<uint8_t>(*p));
        if (mk != 0)
        {
            type_scan(mk);
        }
    }
    if (!kbd_.empty())
    {
        waiting_key_ = false;
    }
}

uint32_t pc::chs_lba(uint8_t cyl, uint8_t head, uint8_t sec) const
{
    /* 360K: 2 heads, 9 spt, 40 cyl. sec is 1-based. */
    if (sec == 0)
    {
        return UINT32_MAX;
    }
    const uint32_t spt = (hw_.fdc.spt != 0) ? hw_.fdc.spt : 9u;
    const uint32_t nh = (hw_.fdc.heads != 0) ? hw_.fdc.heads : 2u;
    if (sec == 0)
    {
        return 0xFFFFFFFFu;
    }
    return (static_cast<uint32_t>(cyl) * nh + head) * spt + (static_cast<uint32_t>(sec) - 1u);
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
            waiting_key_ = true;
            c.set_ip(static_cast<uint16_t>(c.ip() - 2u)); /* rewind CD 16 */
            return true;
        }
        waiting_key_ = false;
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

void pc::wire_pc_hw()
{
    hw_ = {};
    hw_.ppi.control = 0x99;
    hw_.ppi.dip = 0x2D; /* CGA 80×25, 64K planar, 1 FDD, IPL (not Py86 MDA 0x3D) */
    hw_.ppi.io_nibble = 0x06; /* 64K + 6*32K = 256K (Py86 io_channel_size_nibble) */
    hw_.pic.imr = 0xFF;
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
    hw_.fdc.msr = 0x80;
    hw_.fdc.spt = 9;
    hw_.fdc.heads = 2;
    if ((floppy_.size() >= 512u) && (floppy_.size() < 300000u))
    {
        hw_.fdc.spt = 8;
        hw_.fdc.heads = 1;
    }
    c.set_io([this](uint16_t p) { return in_port(p); },
             [this](uint16_t p, uint8_t v) { out_port(p, v); });
    c.set_after_step([this]() { after_insn(); });
}

void pc::enable_fast_post()
{
    /* Py86 enable_fast_post: BDA 0040:0072 = 1234h. */
    c.mem_write8(0x472u, 0x34);
    c.mem_write8(0x473u, 0x12);
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
        }
        return;
    }
    hw_.pic.imr = v;
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
    const bool clock_was_low = (prev & 0x40u) == 0;
    const bool clock_now_high = (now & 0x40u) != 0;
    const bool clock_released = clock_was_low && clock_now_high;
    if (clock_released)
    {
        p.kbd_data = 0xAA;
        p.kbd_ready = 1;
        p.kbd_irq_pend = 1;
        pic_deassert(1);
    }
    const bool kbd_sel = (now & 0x80u) == 0;
    if (kbd_sel && clock_now_high && (p.kbd_irq_pend != 0))
    {
        ppi_tick();
    }
    if (clock_now_high && ((prev & 0x80u) == 0) && ((now & 0x80u) != 0))
    {
        pic_deassert(1);
    }
}

void pc::ppi_tick()
{
    ppi8255 &p = hw_.ppi;
    const bool kbd_sel = ((p.port_b & 0x80u) == 0) && ((p.port_b & 0x40u) != 0);
    if ((p.kbd_ready == 0) && (!scanq_.empty()) && kbd_sel)
    {
        p.kbd_data = scanq_.front();
        scanq_.pop_front();
        p.kbd_ready = 1;
        p.kbd_irq_pend = 1;
        pic_deassert(1);
    }
    if ((p.kbd_irq_pend == 0) || (!kbd_sel))
    {
        return;
    }
    if ((hw_.pic.imr & 0x02u) == 0)
    {
        pic_deassert(1);
        pic_assert(1);
        p.kbd_irq_pend = 0;
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
            if (ch.mode == 0)
            {
                ch.out_pin = 1;
                if (i == 0)
                {
                    pic_deassert(0);
                    pic_assert(0);
                }
            }
            else if (ch.mode == 2)
            {
                ch.count = (ch.reload != 0) ? ch.reload : 0;
                if (i == 0)
                {
                    pic_deassert(0);
                    pic_assert(0);
                }
            }
            else if (ch.mode == 3)
            {
                ch.out_pin = static_cast<uint8_t>(ch.out_pin ^ 1u);
                ch.count = ch.reload;
                if ((i == 0) && (prev_out != 0) && (ch.out_pin == 0))
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
                if ((i == 0) && (prev_out != 0) && (ch.out_pin == 0))
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
    if ((hw_.pit_div & 1u) == 0)
    {
        pit_tick();
    }
    ppi_tick();
    if ((c.flags() & k_flag_if) == 0)
    {
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

bool pc::fdc_drive() const
{
    return floppy_.size() >= 512u;
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
    hw_.fdc.sel = static_cast<uint8_t>(v & 3u);
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
    const bool have = fdc_drive();
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
        if (hw_.fdc.spt > eot)
        {
            eot = hw_.fdc.spt;
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
                const uint32_t lba = chs_lba(cyl, head, sec);
                const uint32_t off = lba * sec_sz;
                if ((lba == 0xFFFFFFFFu) || ((off + sec_sz) > floppy_.size()))
                {
                    hw_.fdc.st0 = static_cast<uint8_t>(0x40u | drv);
                    hw_.fdc.st1 = static_cast<uint8_t>(hw_.fdc.st1 | 0x04u);
                    break;
                }
                fdc_dma_to_mem(floppy_.data() + off, sec_sz);
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
            return (hw_.pic.read_isr != 0) ? hw_.pic.isr : hw_.pic.irr;
        }
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
            const uint8_t sc = hw_.ppi.kbd_data;
            if (hw_.ppi.kbd_ready != 0)
            {
                hw_.ppi.kbd_ready = 0;
                pic_deassert(1);
            }
            return sc;
        }
        if (p == 0x61u)
        {
            return hw_.ppi.port_b;
        }
        if (p == 0x62u)
        {
            uint8_t port_c = static_cast<uint8_t>(hw_.ppi.io_nibble & 0x0Fu);
            if (hw_.pit.ch[2].out_pin != 0)
            {
                port_c = static_cast<uint8_t>(port_c | 0x20u);
            }
            return port_c;
        }
        return hw_.ppi.control;
    }
    if ((p == 0x3BAu) || (p == 0x3DAu))
    {
        /* POST CRT line test: bit3 video enable, bit0 HSYNC must go on then off. */
        hw_.vid.status = static_cast<uint8_t>(hw_.vid.status ^ 0x09u);
        return hw_.vid.status;
    }
    if (p == 0x3B8u)
    {
        return hw_.vid.mode_3b8;
    }
    if (p == 0x3D8u)
    {
        return hw_.vid.mode_3d8;
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
    return 0xFF;
}

void pc::out_port(uint16_t port, uint8_t v)
{
    const uint16_t p = port;
    if (p == 0x20u)
    {
        pic_write_cmd(v);
        return;
    }
    if (p == 0x21u)
    {
        pic_write_data(v);
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
    if ((p == 0x3B4u) || (p == 0x3D4u))
    {
        hw_.vid.index = static_cast<uint8_t>(v & 0x1Fu);
        return;
    }
    if ((p == 0x3B5u) || (p == 0x3D5u))
    {
        if (hw_.vid.index < 18u)
        {
            hw_.vid.regs[hw_.vid.index] = v;
        }
        return;
    }
    if (p == 0x3B8u)
    {
        hw_.vid.mode_3b8 = v;
        return;
    }
    if (p == 0x3D8u)
    {
        hw_.vid.mode_3d8 = v;
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
    }
}

} // namespace iron86
