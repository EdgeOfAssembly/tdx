/**
 * @file pc.h
 * @brief Tiny IBM PC: 360K floppy + INT 10h/13h/16h + INT 1Ah ticks.
 */
#ifndef IRON86_PC_H
#define IRON86_PC_H

#include "iron86/cpu.h"

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

    const std::string &tty() const { return tty_; }

    bool bios_int(uint8_t vector);

private:
    bool int10();
    bool int13();
    bool int16();
    bool int1a();
    uint32_t chs_lba(uint8_t cyl, uint8_t head, uint8_t sec) const;
    uint32_t ticks_18hz() const;

    std::vector<uint8_t> floppy_;
    std::string tty_;
    std::deque<uint8_t> kbd_;
    std::chrono::steady_clock::time_point t0_{};
};

} // namespace iron86

#endif
