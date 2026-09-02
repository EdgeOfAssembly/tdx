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
    for (uint32_t i = 0; i < 80u * 25u; i++)
    {
        c.mem_write8(0xB8000u + i * 2u, static_cast<uint8_t>(' '));
        c.mem_write8(0xB8000u + i * 2u + 1u, 0x07);
    }
    cur_x_ = 0;
    cur_y_ = 0;
    video_mode_ = 0x03;
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
    hw_.ppi.io_nibble = 0;
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
    if ((p.kbd_irq_pend == 0) || ((p.port_b & 0x80u) != 0) || ((p.port_b & 0x40u) == 0))
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
    }
}

} // namespace iron86
