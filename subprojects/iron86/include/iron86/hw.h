/**
 * @file hw.h
 * @brief Packed IBM PC chipset state (8255/8253/8259/8237 + CRTC ports).
 *
 * Layout matches Py86 (`ppi8255.py`, `pit8253.py`, `pic8259.py`, `dma8237.py`)
 * as used by the 24-APR-1981 5150 BIOS POST. Packed so on-disk dumps and
 * `offsetof` checks stay stable.
 *
 * @note DIP default 0x2D is CGA 80×25 (tdxview B800). Py86 later used 0x3D
 *       (MDA) for its mono display; do not copy that here.
 */
#ifndef IRON86_HW_H
#define IRON86_HW_H

#include <cstddef>
#include <cstdint>

namespace iron86
{

#pragma pack(push, 1)

/** @brief Intel 8255 PPI (5150 ports 60h–63h). */
struct ppi8255
{
    uint8_t port_a;
    uint8_t port_b;
    uint8_t port_c;
    uint8_t control;
    uint8_t dip;          /**< SW1; 0x2D = CGA 80×25, 64K, 1 FDD, IPL. */
    uint8_t io_nibble;    /**< Port C low nibble: I/O-channel RAM ×32K. */
    uint8_t kbd_data;
    uint8_t last_b;
    uint8_t kbd_ready;
    uint8_t kbd_irq_pend;
};

/** @brief One 8253 counter channel. */
struct pit_counter
{
    uint16_t count;
    uint16_t latch;
    uint16_t reload;
    uint8_t mode;
    uint8_t rw_mode;
    uint8_t lsb_toggle;
    uint8_t running;
    uint8_t out_pin;
    uint8_t latched;
    uint8_t bcd;
    uint8_t gate;
};

/** @brief Intel 8253 PIT (ports 40h–43h). */
struct pit8253
{
    pit_counter ch[3];
};

/** @brief Intel 8259A PIC (ports 20h–21h). */
struct pic8259
{
    uint8_t irr;
    uint8_t isr;
    uint8_t imr;
    uint8_t vector_base;
    uint8_t icw_step;
    uint8_t initialized;
    uint8_t auto_eoi;
    uint8_t read_isr;
    uint8_t trigger_mode;
    uint8_t irq_lines;
    uint8_t icw4_needed;
    uint8_t single_mode;
};

/** @brief Intel 8237A DMA (ports 00h–0Fh), POST wrap-test subset. */
struct dma8237
{
    uint16_t curr_addr[4];
    uint16_t curr_count[4];
    uint16_t base_addr[4];
    uint16_t base_count[4];
    uint8_t mode[4];
    uint8_t page[4];
    uint8_t mask;
    uint8_t command;
    uint8_t status;
    uint8_t byte_ptr;
};

/** @brief MDA/CGA CRTC ports used by 1981 POST (3Bx / 3Dx). */
struct crtc_video
{
    uint8_t index;
    uint8_t regs[18];
    uint8_t mode_3b8;
    uint8_t mode_3d8;
    uint8_t status;
};

/**
 * @brief All POST-visible motherboard chips in one packed blob.
 *
 * @note pit_div is the Py86 “tick PIT every other instruction” divider.
 */
struct pc_hw
{
    ppi8255 ppi;
    pit8253 pit;
    pic8259 pic;
    dma8237 dma;
    crtc_video vid;
    uint8_t pit_div;
};

#pragma pack(pop)

static_assert(sizeof(ppi8255) == 10, "ppi8255 packed");
static_assert(sizeof(pit_counter) == 14, "pit_counter packed");
static_assert(sizeof(pit8253) == 42, "pit8253 packed");
static_assert(sizeof(pic8259) == 12, "pic8259 packed");
static_assert(sizeof(dma8237) == 44, "dma8237 packed");
static_assert(sizeof(crtc_video) == 22, "crtc_video packed");
static_assert(sizeof(pc_hw) == 131, "pc_hw packed");
static_assert(offsetof(pc_hw, pic) == 52, "pc_hw.pic offset");
static_assert(offsetof(pc_hw, dma) == 64, "pc_hw.dma offset");

} // namespace iron86

#endif /* IRON86_HW_H */
