/**
 * @file crtc.h
 * @brief Packed Motorola 6845 CRTC as used on IBM MDA (3Bx) and CGA (3Dx).
 *
 * 99% IBM PC subset: R0–R15, status bits for POST TEST.10, cursor/start
 * address. Light pen (R16/R17 strobe) is not implemented.
 *
 * Match Py86 `mda.py` + OA MDA manual pages 9–12. Not cycle-exact.
 */
#ifndef IRON86_CRTC_H
#define IRON86_CRTC_H

#include <cstdint>

namespace iron86
{

#pragma pack(push, 1)

/**
 * @brief One 6845 + adapter mode/status (MDA or CGA).
 *
 * @note is_mda: 1 → 3BA bits (HSYNC bit0, video bit3); 0 → 3DA (retrace bit0,
 *       vsync bit3).
 */
struct mc6845
{
    uint8_t index;
    uint8_t regs[18];
    uint8_t ctrl;
    uint8_t h_pos;
    uint8_t v_row;
    uint8_t scan;
    uint8_t status;
    uint8_t primed; /**< MDA: high-res bit 0 of 3B8 seen. */
    uint8_t is_mda;
    uint8_t lpt_data;
    uint8_t lpt_stat;
    uint8_t lpt_ctrl;
};

#pragma pack(pop)

static_assert(sizeof(mc6845) == 29, "mc6845 packed");

/**
 * @brief Reset to power-on (regs 0, MDA primed=0, LPT not-busy 0xDF).
 */
void crtc_reset(mc6845 *c, uint8_t is_mda);

void crtc_write_index(mc6845 *c, uint8_t v);
void crtc_write_data(mc6845 *c, uint8_t v);
uint8_t crtc_read_data(const mc6845 *c);
void crtc_write_ctrl(mc6845 *c, uint8_t v);
uint8_t crtc_read_ctrl(const mc6845 *c);

/**
 * @brief Advance one character clock and return 3BA/3DA status.
 *
 * If R0 is 0 (unprogrammed), fall back to Py86-style toggling so POST
 * TEST.10 still sees on→off.
 */
uint8_t crtc_read_status(mc6845 *c);

void crtc_tick(mc6845 *c);

uint16_t crtc_cursor(const mc6845 *c);
uint16_t crtc_start(const mc6845 *c);

} // namespace iron86

#endif /* IRON86_CRTC_H */
