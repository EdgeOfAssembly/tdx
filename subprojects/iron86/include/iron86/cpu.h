/**
 * @file cpu.h
 * @brief Real-mode 8086 CPU (Py86 `Intel8086` port). Hardware only — no DOS.
 */
#ifndef IRON86_CPU_H
#define IRON86_CPU_H

#include "iron86/ea.h"

#include <cstddef>
#include <cstdint>
#include <functional>
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
    void set_cx(uint16_t v) { cx_ = v; }
    void set_dx(uint16_t v) { dx_ = v; }
    void set_cs(uint16_t v) { cs_ = v; }
    void set_ds(uint16_t v) { ds_ = v; }
    void set_ss(uint16_t v) { ss_ = v; }
    void set_es(uint16_t v) { es_ = v; }
    void set_ip(uint16_t v) { ip_ = v; }
    void set_sp(uint16_t v) { sp_ = v; }
    void set_bx(uint16_t v) { bx_ = v; }
    void set_si(uint16_t v) { si_ = v; }
    void set_di(uint16_t v) { di_ = v; }
    void set_bp(uint16_t v) { bp_ = v; }
    void set_flags(uint16_t v) { flags_ = v; }
    void set_cf(bool v) { set_flag(k_flag_cf, v); }
    void set_zf(bool v) { set_flag(k_flag_zf, v); }
    uint8_t last_op() const { return last_op_; }

    /**
     * @brief Execute one instruction (prefixes included).
     * @return false if still halted with no wake-up.
     */
    bool step();

    /**
     * @brief Load IBM 5150 8K BIOS the way Py86 `load_bios_5150_8k` does.
     *
     * Image at FE000h (high 8K of the F000 block). Five-byte reset vector
     * from file offset -16 copied to FFFF0h (JMP F000:E05B on 5700051).
     * Does not touch the IVT. Leaves CS=FFFF IP=0000 (8086 reset).
     *
     * @return false if @p data is null or empty.
     */
    bool load_bios_5150_8k(const uint8_t *data, size_t n);

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

    /**
     * @brief Optional BIOS intercept. Return true if the INT was handled
     *        (IP already past CD nn; set CF in FLAGS as the BIOS would).
     */
    void set_bios(std::function<bool(cpu &, uint8_t)> fn);

    /** @brief Optional port I/O (PPI/PIT/PIC). Default IN=FFh, OUT=nop. */
    void set_io(std::function<uint8_t(uint16_t)> in, std::function<void(uint16_t, uint8_t)> out);

    /** @brief Called after each instruction (PIT ticks). */
    void set_after_step(std::function<void()> fn);

    /**
     * @brief Latch a hardware interrupt vector (PIC INTA result).
     * Delivered at end of step if IF=1.
     */
    void raise_intr(uint8_t vector);

    /** @brief Latch IRQ0 (vector 08h). */
    void raise_irq0();

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
    void set_logic_flags(uint32_t res, unsigned size);
    uint16_t sreg(uint8_t n) const;
    void set_sreg(uint8_t n, uint16_t v);

    uint16_t data_seg() const;
    uint16_t fetch_imm(bool word, bool sign_ext);
    uint16_t alu_arith(uint16_t dst, uint16_t src, unsigned size, uint8_t op, uint16_t cin,
                       bool write);
    void op_alu_rm(uint8_t opc);
    void op_alu_imm(bool word, bool sign_ext);
    void op_grp3(bool word);
    void op_shift(bool word, bool via_cl);
    void op_ff();
    void op_incdec8();
    void one_string(uint8_t op);
    void op_string(uint8_t op);
    void op_loop(uint8_t kind);
    void op_les_lds(bool les);
    uint8_t in8(uint16_t port);
    void out8(uint16_t port, uint8_t v);

    using handler = void (cpu::*)();
    static void fill_dispatch();

    void op_unimpl();
    void op_nop();
    void op_hlt();
    void op_pre_es();
    void op_pre_cs();
    void op_pre_ss();
    void op_pre_ds();
    void op_daa();
    void op_das();
    void op_aaa();
    void op_aas();
    void op_lock();
    void op_repne();
    void op_repe();
    void op_alu();
    void op_push_sr();
    void op_pop_sr();
    void op_inc_r();
    void op_dec_r();
    void op_push_r();
    void op_pop_r();
    void op_jcc();
    void op_mov_i8();
    void op_mov_i16();
    void op_xchg_ax();
    void op_test8();
    void op_test16();
    void op_xchg8();
    void op_xchg16();
    void op_grp80();
    void op_grp81();
    void op_grp83();
    void op_mov_rm8_r8();
    void op_mov_rm16_r16();
    void op_mov_r8_rm8();
    void op_mov_r16_rm16();
    void op_mov_rm_sr();
    void op_lea();
    void op_mov_sr_rm();
    void op_pop_rm();
    void op_cbw();
    void op_cwd();
    void op_call_far();
    void op_wait();
    void op_pushf();
    void op_popf();
    void op_sahf();
    void op_lahf();
    void op_mov_al_m();
    void op_mov_ax_m();
    void op_mov_m_al();
    void op_mov_m_ax();
    void op_str();
    void op_test_al();
    void op_test_ax();
    void op_retn_imm();
    void op_retn();
    void op_les();
    void op_lds();
    void op_mov_rm8_i();
    void op_mov_rm16_i();
    void op_retf_imm();
    void op_retf();
    void op_int3();
    void op_int();
    void op_into();
    void op_iret();
    void op_shift_d0();
    void op_shift_d1();
    void op_shift_d2();
    void op_shift_d3();
    void op_aam();
    void op_aad();
    void op_xlat();
    void op_esc();
    void op_loop_op();
    void op_in_i8();
    void op_in_i16();
    void op_out_i8();
    void op_out_i16();
    void op_call_rel();
    void op_jmp_rel16();
    void op_jmp_far();
    void op_jmp_rel8();
    void op_in_dx8();
    void op_in_dx16();
    void op_out_dx8();
    void op_out_dx16();
    void op_cmc();
    void op_grp3_8();
    void op_grp3_16();
    void op_clc();
    void op_stc();
    void op_cli();
    void op_sti();
    void op_cld();
    void op_std();
    void op_fe();
    void op_ff_op();

    static handler dispatch_[256];
    static bool dispatch_ready_;
    bool prefix_more_ = false;

    std::function<bool(cpu &, uint8_t)> bios_{};
    std::function<uint8_t(uint16_t)> io_in_{};
    std::function<void(uint16_t, uint8_t)> io_out_{};
    std::function<void()> after_step_{};
    bool intr_pending_ = false;
    uint8_t intr_vec_ = 0;
    uint16_t seg_ov_ = 0xFFFF; /**< 0xFFFF = none */
    uint8_t rep_ = 0;
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
    uint8_t last_op_ = 0;
};

} // namespace iron86

#endif /* IRON86_CPU_H */
