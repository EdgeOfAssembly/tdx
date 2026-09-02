/**
 * @file pc.h
 * @brief Tiny IBM PC: 360K floppy + INT 10h/13h/16h + INT 1Ah ticks.
 */
#ifndef IRON86_PC_H
#define IRON86_PC_H

#include "iron86/cpu.h"
#include "iron86/hw.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace iron86
{

class pc
{
public:
    cpu c;

    /** @brief Load a 360K (or any) image; boot sector copied to 0000:7C00. */
    bool load_floppy(const uint8_t *img, size_t n);

    /** @brief CS:IP = 0000:7C00, DL = 00h, BIOS hooks installed. */
    void boot();

    /**
     * @brief Queue ASCII as INT 16h keys (AH=00/01). Used to type at A>.
     * @param[in] s Bytes to type (CR = 0x0D).
     */
    void type_keys(const char *s);

    /** @brief True when the INT 16h type-ahead buffer is empty. */
    bool keys_empty() const { return kbd_.empty(); }

    /** @brief INT 16h AH=00 with empty buffer: IP rewound, tdx should wait. */
    bool waiting_key() const { return waiting_key_; }
    void clear_wait_key() { waiting_key_ = false; }

    const std::string &tty() const { return tty_; }

    /** @brief BIOS video mode (INT 10h AH=00). Default 03h text. */
    uint8_t video_mode() const { return video_mode_; }

    bool bios_int(uint8_t vector);

    /**
     * @brief Wire PPI/PIT/PIC/DMA I/O (Py86). Call after load_bios_5150_8k.
     */
    void wire_pc_hw();

    /**
     * @brief BDA RESET_FLAG=1234h (Py86 --fast-post). Skips long STGTST.
     */
    void enable_fast_post();

private:
    bool int10();
    bool int13();
    bool int16();
    bool int1a();
    uint32_t chs_lba(uint8_t cyl, uint8_t head, uint8_t sec) const;
    uint32_t ticks_18hz() const;

    void tty_cell(uint8_t ch);
    void tty_scroll();

    std::vector<uint8_t> floppy_;
    std::string tty_;
    std::deque<uint8_t> kbd_;
    std::chrono::steady_clock::time_point t0_{};
    uint8_t video_mode_ = 0x03;
    uint8_t cur_x_ = 0;
    uint8_t cur_y_ = 0;
    bool waiting_key_ = false;

    pc_hw hw_{};

    uint8_t in_port(uint16_t port);
    void out_port(uint16_t port, uint8_t v);
    void after_insn();
    void pit_tick();
    void ppi_tick();
    void ppi_on_port_b();
    void pic_assert(uint8_t irq);
    void pic_deassert(uint8_t irq);
    void pic_write_cmd(uint8_t v);
    void pic_write_data(uint8_t v);
    uint8_t dma_in(uint16_t port);
    void dma_out(uint16_t port, uint8_t v);
    void pit_write(uint16_t port, uint8_t v);
    uint8_t pit_read(uint16_t port);
};

} // namespace iron86

#endif
