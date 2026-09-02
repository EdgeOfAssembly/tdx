/**
 * @file crtc.cpp
 * @brief Motorola 6845 CRTC (IBM MDA/CGA subset). Py86 mda.py + OA MDA p.9–12.
 */
#include "iron86/crtc.h"

#include <cstdint>
#include <cstring>

namespace iron86
{

namespace
{

/** @brief Py86 3BA fallback: bit0 high for 2 of every 4 character clocks. */
uint8_t fallback_hsync(uint8_t counter)
{
    return ((counter & 0x03u) < 2u) ? 1u : 0u;
}

/** @brief Py86 3BA fallback: bit3 high for 5 of every 8 character clocks. */
uint8_t fallback_video(uint8_t counter)
{
    return ((counter & 0x07u) < 5u) ? 0x08u : 0u;
}

/**
 * @brief Active display: character in [0, R1) and row in [0, R6).
 *
 * R1=0 or R6=0 (unprogrammed displayed counts) is never inside the window.
 */
bool in_display(const mc6845 *c)
{
    return (c->h_pos < c->regs[1]) && (c->v_row < c->regs[6]);
}

} // namespace

void crtc_reset(mc6845 *c, uint8_t is_mda)
{
    std::memset(c, 0, sizeof(*c));
    c->is_mda = is_mda;
    c->lpt_stat = 0xDFu; /* printer ready: not busy, selected, no error */
}

void crtc_write_index(mc6845 *c, uint8_t v)
{
    c->index = static_cast<uint8_t>(v & 0x1Fu);
}

void crtc_write_data(mc6845 *c, uint8_t v)
{
    if (c->index < 18u)
    {
        c->regs[c->index] = v;
    }
}

uint8_t crtc_read_data(const mc6845 *c)
{
    if (c->index < 18u)
    {
        return c->regs[c->index];
    }
    return 0;
}

void crtc_write_ctrl(mc6845 *c, uint8_t v)
{
    c->ctrl = v;
    /* Real MDA hangs the CPU if the first 3B8 write is not 01h. Py86 only
     * warns; we set primed and keep running. */
    if ((c->is_mda != 0) && ((v & 1u) != 0))
    {
        c->primed = 1;
    }
}

uint8_t crtc_read_ctrl(const mc6845 *c)
{
    return c->ctrl;
}

void crtc_tick(mc6845 *c)
{
    c->h_pos = static_cast<uint8_t>(c->h_pos + 1u);
    if ((c->regs[0] > 0) && (c->h_pos > c->regs[0]))
    {
        c->h_pos = 0;
        c->scan = static_cast<uint8_t>(c->scan + 1u);
        if ((c->regs[9] > 0) && (c->scan > c->regs[9]))
        {
            c->scan = 0;
            c->v_row = static_cast<uint8_t>(c->v_row + 1u);
            if ((c->regs[4] > 0) && (c->v_row > c->regs[4]))
            {
                c->v_row = 0;
            }
        }
    }
}

uint8_t crtc_read_status(mc6845 *c)
{
    crtc_tick(c);

    /* POST TEST.10 (and the 3BA/3DA smoke test) run before CRTC init. XOR
     * 0x09 so bit0 and bit3 each go on then off on consecutive reads. */
    if (c->regs[0] == 0)
    {
        c->status = static_cast<uint8_t>(c->status ^ 0x09u);
        return c->status;
    }

    uint8_t bit0 = 0;
    uint8_t bit3 = 0;
    if (c->is_mda != 0)
    {
        const uint8_t r2 = c->regs[2];
        const uint8_t r3 = c->regs[3];
        if (r2 != 0)
        {
            const uint16_t hs_end = static_cast<uint16_t>(r2) + r3;
            bit0 = ((c->h_pos >= r2) && (c->h_pos < hs_end)) ? 1u : 0u;
        }
        else
        {
            bit0 = fallback_hsync(c->h_pos);
        }
        if (((c->ctrl & 0x09u) == 0x09u) && in_display(c))
        {
            bit3 = 0x08u;
        }
    }
    else
    {
        bit0 = in_display(c) ? 0u : 1u;
        if (c->regs[6] != 0)
        {
            bit3 = (c->v_row >= c->regs[6]) ? 0x08u : 0u;
        }
        else
        {
            bit3 = fallback_video(c->h_pos);
        }
    }

    c->status = static_cast<uint8_t>(bit0 | bit3);
    return c->status;
}

uint16_t crtc_cursor(const mc6845 *c)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(c->regs[14]) << 8) | c->regs[15]);
}

uint16_t crtc_start(const mc6845 *c)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(c->regs[12]) << 8) | c->regs[13]);
}

} // namespace iron86
