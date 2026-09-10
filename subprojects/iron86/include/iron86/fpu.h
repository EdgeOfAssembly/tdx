/**
 * @file fpu.h
 * @brief Intel 8087 numeric coprocessor (NPX) — 80-bit stack, ESC D8–DF.
 *
 * Default **on**. CLI `--no-8087` disables: ESC still consumes ModR/M, WAIT is
 * NOP, EQUIP_FLAG bit 1 / SW1-2 stay clear. Not 287/387 (`FSTSW AX`, `FSIN`).
 */
#ifndef IRON86_FPU_H
#define IRON86_FPU_H

#include "iron86/ea.h"

#include <cstdint>

namespace iron86
{

class cpu;

/** @brief FNINIT control word: all exceptions masked, round nearest, 64-bit prec. */
inline constexpr uint16_t k_fpu_cw_init = 0x037F;
/** @brief Tag word: eight empty registers (11b each). */
inline constexpr uint16_t k_fpu_tag_empty = 0xFFFF;

/**
 * @brief 8087 register stack and control/status.
 *
 * The 8086 still decodes ModR/M and forms the EA; @ref esc performs the
 * numeric op and memory load/store through @p cpu. Ops complete immediately
 * (`busy()` is false after @ref esc). @ref wait is then a no-op. No host
 * thread — F9/VCR stay deterministic.
 */
class fpu
{
public:
    fpu() { reset(); }

    /** @brief FNINIT-equivalent: CW=037Fh, SW=0, all tags empty, enabled. */
    void reset();

    void set_enabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }

    /** @brief 8087 BUSY. v1: always false after @ref esc. */
    bool busy() const { return busy_; }

    /** @brief WAIT: return when !busy (v1 immediate). */
    void wait();

    /**
     * @brief Execute one ESC after the CPU has fetched opcode + ModR/M.
     *
     * @param c     Host 8086 (memory + @c last_op already D8–DF).
     * @param opc   Opcode byte D8–DF.
     * @param mr    Decoded ModR/M (mod=3 → ST(rm); else linear EA in mr).
     */
    void esc(cpu &c, uint8_t opc, const modrm &mr);

    uint16_t control() const { return cw_; }
    uint16_t status() const { return sw_; }
    uint16_t tags() const { return tag_; }

    /**
     * @brief ST(i) as host long double (x86_64 gcc 80-bit). Empty → 0.
     * @param i 0 = TOS … 7.
     */
    long double st(unsigned i) const;

    /** @brief TOS index 0–7 (status bits 11–13). */
    unsigned top() const { return (static_cast<unsigned>(sw_ >> 11) & 7u); }

private:
    bool enabled_ = true;
    bool busy_ = false;
    uint16_t cw_ = k_fpu_cw_init;
    uint16_t sw_ = 0;
    uint16_t tag_ = k_fpu_tag_empty;
    long double stack_[8]{};

    unsigned phys_reg(unsigned sti) const;
    void push(long double v);
    long double pop();
    long double st_phys(unsigned p) const;
    void set_st_phys(unsigned p, long double v);
    void set_c0c2c3(int cmp);
    void set_ex(uint16_t bit);
    long double load_mem(cpu &c, const modrm &mr, unsigned bytes);
    void store_mem(cpu &c, const modrm &mr, unsigned bytes, long double v, bool pop);
};

} // namespace iron86

#endif /* IRON86_FPU_H */
