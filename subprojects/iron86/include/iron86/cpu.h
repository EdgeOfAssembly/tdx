/**
 * @file cpu.h
 * @brief Real-mode 8086 CPU (Py86 `Intel8086` port). Hardware only — no DOS.
 */
#ifndef IRON86_CPU_H
#define IRON86_CPU_H

#include "iron86/ea.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace iron86
{

inline constexpr uint32_t k_mem_size = 0x100000u;
inline constexpr uint16_t k_flag_cf = 0x0001;
inline constexpr uint16_t k_flag_pf = 0x0004;
inline constexpr uint16_t k_flag_af = 0x0010;
inline constexpr uint16_t k_flag_zf = 0x0040;
inline constexpr uint16_t k_flag_sf = 0x0080;
inline constexpr uint16_t k_flag_tf = 0x0100;
inline constexpr uint16_t k_flag_if = 0x0200;
inline constexpr uint16_t k_flag_df = 0x0400;
inline constexpr uint16_t k_flag_of = 0x0800;
/** 8086 power-on FLAGS (reserved 1s): F002h. */
inline constexpr uint16_t k_flags_reset = 0xF002;

/**
 * @brief 8086 real-mode CPU with 1 MiB physical memory.
 *
 * Reset state matches the 8086 datasheet / Py86: CS=FFFF, IP=0000, SP=FFFE,
 * FLAGS=F002.
 */
class cpu
{
public:
    cpu();

    /** @brief Power-on reset. */
    void reset();

    /**
     * @brief Linear address (seg<<4)+off, 20-bit wrap.
     */
    static uint32_t phys(uint16_t seg, uint16_t off);

    uint8_t mem_read8(uint32_t lin) const;
    uint16_t mem_read16(uint32_t lin) const;
    void mem_write8(uint32_t lin, uint8_t v);
    void mem_write16(uint32_t lin, uint16_t v);

    uint16_t ax() const { return ax_; }
    uint16_t cx() const { return cx_; }
    uint16_t dx() const { return dx_; }
    uint16_t bx() const { return bx_; }
    uint16_t sp() const { return sp_; }
    uint16_t bp() const { return bp_; }
    uint16_t si() const { return si_; }
    uint16_t di() const { return di_; }
    uint16_t cs() const { return cs_; }
    uint16_t ds() const { return ds_; }
    uint16_t ss() const { return ss_; }
    uint16_t es() const { return es_; }
    uint16_t ip() const { return ip_; }
    uint16_t flags() const { return flags_; }
    bool halted() const { return halted_; }

    void set_ax(uint16_t v) { ax_ = v; }
    void set_cs(uint16_t v) { cs_ = v; }
    void set_ds(uint16_t v) { ds_ = v; }
    void set_ss(uint16_t v) { ss_ = v; }
    void set_es(uint16_t v) { es_ = v; }
    void set_ip(uint16_t v) { ip_ = v; }
    void set_sp(uint16_t v) { sp_ = v; }

    /**
     * @brief Execute one instruction (prefixes included).
     * @return false if still halted with no wake-up.
     */
    bool step();

    /**
     * @brief Load a .COM image at CS:0100 (PSP-style). Does not build a PSP.
     * @param[in] bytes Image bytes.
     * @param[in] n     Length (capped at 64 KiB - 0x100).
     * @param[in] cs    Load segment (default 0x1000).
     */
    void load_com(const uint8_t *bytes, size_t n, uint16_t cs = 0x1000);

    /**
     * @brief Point IVT[vector] at @p handler_seg:@p handler_off.
     */
    void set_ivt(uint8_t vector, uint16_t handler_seg, uint16_t handler_off);

private:
    uint8_t fetch8();
    uint16_t fetch16();
    void push16(uint16_t v);
    uint16_t pop16();
    void do_int(uint8_t vector);

    void decode_modrm();
    uint16_t rm_base(uint8_t rm) const;
    uint32_t rm_lin() const;
    uint16_t gpr16(uint8_t n) const;
    void set_gpr16(uint8_t n, uint16_t v);
    uint8_t get_reg8(uint8_t n) const;
    void set_reg8(uint8_t n, uint8_t v);
    uint8_t get_rm8() const;
    void set_rm8(uint8_t v);
    uint16_t get_rm16() const;
    void set_rm16(uint16_t v);

    void set_flag(uint16_t bit, bool v);
    void update_zsp(uint32_t res, unsigned size);
    void set_add_flags(uint32_t res, uint32_t op1, uint32_t op2, unsigned size);
    void set_sub_flags(uint32_t res, uint32_t op1, uint32_t op2, unsigned size);
    void inc_r16(uint8_t r);
    void dec_r16(uint8_t r);
    bool cond_cc(uint8_t cc) const;
    void jcc8(uint8_t cc);

    std::unique_ptr<uint8_t[]> mem_;
    modrm mr_{};
    uint16_t ax_ = 0;
    uint16_t cx_ = 0;
    uint16_t dx_ = 0;
    uint16_t bx_ = 0;
    uint16_t sp_ = 0xFFFE;
    uint16_t bp_ = 0;
    uint16_t si_ = 0;
    uint16_t di_ = 0;
    uint16_t cs_ = 0xFFFF;
    uint16_t ds_ = 0;
    uint16_t ss_ = 0;
    uint16_t es_ = 0;
    uint16_t ip_ = 0;
    uint16_t flags_ = k_flags_reset;
    bool halted_ = false;
};

} // namespace iron86

#endif /* IRON86_CPU_H */
