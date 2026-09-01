/**
 * @file pit8253.h
 * @brief Intel 8253/8254 Programmable Interval Timer.
 *
 * Ported from the proven, chip-accurate Py86 emulator (pit8253.py) to C++23.
 * Channel 0 (the DOS system timer) asserts IRQ0 on terminal count / OUT
 * falling edge through the PIC, which is how a DOS game's delay counters get
 * decremented by its own timer ISR — the behaviour that fixes tdx appearing
 * "stuck" on one instruction line while the game busy-waits on a counter.
 *
 * The PIT is ticked by *guest instruction count* (like Py86's every-2nd-step
 * cadence), not wall clock, because the Unicorn CPU runs at full host speed.
 */
#ifndef DOS_PIT8253_H
#define DOS_PIT8253_H

#include <cstdint>

#include "pic8259.h"

class pit8253
{
public:
    explicit pit8253(pic8259 &pic) : m_pic(pic) {}

    struct counter
    {
        uint16_t count = 0;
        uint16_t latch = 0;
        uint16_t reload = 0;
        unsigned mode = 0;
        bool bcd = false;
        bool lsb_toggle = true;
        unsigned rw_mode = 3;
        bool running = false;
        bool out = true;
        bool gate = true;
        bool triggered = false;
        bool latched = false;
    };

    void write(unsigned port, uint8_t value)
    {
        port &= 3u;
        if (port == 3u)
        {
            write_control(value);
            return;
        }
        counter &c = m_counters[port];
        bool should_load = false;
        if (c.rw_mode == 1u)
        {
            c.reload = value;
            should_load = true;
            c.lsb_toggle = true;
        }
        else if (c.rw_mode == 2u)
        {
            c.reload = (uint16_t)(value << 8);
            should_load = true;
            c.lsb_toggle = true;
        }
        else
        { /* RW=3: LSB then MSB */
            if (c.lsb_toggle)
            {
                c.reload = (uint16_t)((c.reload & 0xFF00u) | value);
            }
            else
            {
                c.reload = (uint16_t)((c.reload & 0x00FFu) | ((unsigned)value << 8));
            }
            c.lsb_toggle = !c.lsb_toggle;
            should_load = c.lsb_toggle;
        }
        if (should_load)
        {
            c.count = c.reload;
            if ((c.mode != 1u) && (c.mode != 5u))
            {
                c.running = true;
            }
            c.triggered = false;
            if (c.mode == 0u)
            {
                c.out = false;
            }
        }
    }

    uint8_t read(unsigned port)
    {
        port &= 3u;
        counter &c = m_counters[port];
        const uint16_t val = c.latched ? c.latch : c.count;
        uint8_t ret = 0;
        if (c.rw_mode == 1u)
        {
            ret = (uint8_t)(val & 0xFFu);
            c.latched = false;
            c.latch = 0;
            return ret;
        }
        if (c.rw_mode == 2u)
        {
            ret = (uint8_t)((val >> 8) & 0xFFu);
            c.latched = false;
            c.latch = 0;
            return ret;
        }
        if (c.lsb_toggle)
        {
            ret = (uint8_t)(val & 0xFFu);
        }
        else
        {
            ret = (uint8_t)((val >> 8) & 0xFFu);
            c.latched = false;
            c.latch = 0;
        }
        c.lsb_toggle = !c.lsb_toggle;
        return ret;
    }

    /* Advance all counters by one PIT clock tick (~1.193182 MHz). */
    void tick()
    {
        for (unsigned i = 0u; i < 3u; i++)
        {
            counter &c = m_counters[i];
            if ((!c.running) || ((!c.gate) && (c.mode != 1u) && (c.mode != 5u)))
            {
                continue;
            }
            const bool prev_out = c.out;
            if (c.bcd)
            {
                c.count = bcd_decrement(c.count);
            }
            else
            {
                c.count = (uint16_t)(c.count - 1u);
            }
            if (c.count == 0u)
            {
                terminal_count(i, c);
            }
            if (c.mode == 3u)
            {
                const uint16_t half = (uint16_t)(((unsigned)c.reload + 1u) / 2u);
                if (c.count == half)
                {
                    c.out = !c.out;
                    if ((i == 0u) && prev_out && (!c.out))
                    {
                        m_pic.assert_irq(0);
                    }
                }
            }
        }
    }

    void set_gate(unsigned ch, bool state)
    {
        if (ch > 2u)
        {
            return;
        }
        counter &c = m_counters[ch];
        const bool prev = c.gate;
        c.gate = state;
        if (state && (!prev) && ((c.mode == 1u) || (c.mode == 5u)))
        {
            c.triggered = true;
            c.count = c.reload;
            c.running = true;
            if (c.mode == 1u)
            {
                c.out = false;
            }
        }
    }

    const counter &channel(unsigned ch) const { return m_counters[ch & 3u]; }

    /* tdx runs no BIOS POST, so ch0 would never get programmed. Apply the
     * state every booted DOS machine has: ch0 = mode 3 square wave, reload 0
     * (=65536 ticks -> the classic ~18.2 Hz IRQ0 system tick), counting. */
    void pc_boot_state()
    {
        for (auto &c : m_counters)
        {
            c = counter{};
        }
        counter &c0 = m_counters[0];
        c0.mode = 3;
        c0.rw_mode = 3;
        c0.reload = 0;
        c0.count = 0;
        c0.running = true;
        c0.out = true;
        c0.gate = true;
    }

private:
    void write_control(uint8_t val)
    {
        const unsigned idx = (val >> 6) & 3u;
        if (idx == 3u)
        {
            return;
        }
        counter &c = m_counters[idx];
        if ((val & 0x30u) == 0u)
        { /* latch command */
            c.latch = c.count;
            c.latched = true;
            return;
        }
        c.rw_mode = (val >> 4) & 3u;
        unsigned mode = (val >> 1) & 7u;
        if (mode > 5u)
        {
            mode -= 4u;
        }
        c.mode = mode;
        c.bcd = (val & 1u) != 0;
        c.lsb_toggle = true;
        c.running = false;
        c.out = true;
        c.triggered = false;
        c.latched = false;
        c.reload = 0;
        c.count = 0;
    }

    void terminal_count(unsigned ch, counter &c)
    {
        switch (c.mode)
        {
        case 0:
            c.out = true;
            if (ch == 0u)
            {
                m_pic.deassert_irq(0);
                m_pic.assert_irq(0);
            }
            break;
        case 1:
            if (c.triggered)
            {
                c.out = true;
                c.running = false;
            }
            break;
        case 2:
            c.out = false;
            c.count = (c.reload != 0u) ? c.reload : (uint16_t)0x10000u;
            if (ch == 0u)
            {
                m_pic.deassert_irq(0);
                m_pic.assert_irq(0);
            }
            c.out = true;
            break;
        case 3:
        {
            const bool prev_out = c.out;
            c.out = !c.out;
            c.count = c.reload;
            if ((ch == 0u) && prev_out && (!c.out))
            {
                m_pic.deassert_irq(0);
                m_pic.assert_irq(0);
            }
            return;
        }
        case 4:
            c.out = false;
            c.running = false;
            c.count = c.reload;
            c.out = true;
            break;
        case 5:
            if (c.triggered)
            {
                c.out = false;
                c.count = c.reload;
                c.out = true;
                c.triggered = false;
            }
            break;
        default:
            break;
        }
        if ((c.mode == 2u) || (c.mode == 3u))
        {
            c.count = (c.reload != 0u) ? c.reload : c.count;
        }
    }

    static uint16_t bcd_decrement(uint16_t value)
    {
        unsigned n0 = value & 0x0Fu;
        unsigned n1 = (value >> 4) & 0x0Fu;
        unsigned n2 = (value >> 8) & 0x0Fu;
        unsigned n3 = (value >> 12) & 0x0Fu;
        if (n0 == 0u)
        {
            n0 = 9u;
            if (n1 == 0u)
            {
                n1 = 9u;
                if (n2 == 0u)
                {
                    n2 = 9u;
                    n3 = (n3 == 0u) ? 9u : (n3 - 1u);
                }
                else
                {
                    n2--;
                }
            }
            else
            {
                n1--;
            }
        }
        else
        {
            n0--;
        }
        return (uint16_t)((n3 << 12) | (n2 << 8) | (n1 << 4) | n0);
    }

    pic8259 &m_pic;
    counter m_counters[3]{};
};

#endif /* DOS_PIT8253_H */
