/**
 * @file cpu.h
 * @brief Real-mode 8086 CPU (Py86 `Intel8086` port). Hardware only — no DOS.
 */
#ifndef IRON86_CPU_H
#define IRON86_CPU_H

#include "iron86/ea.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>

namespace iron86
{

inline constexpr uint32_t k_mem_size = 0x100000u;
/** @brief Prefetch queue cap; @ref cpu::set_prefetch_bytes clamps here. */
inline constexpr unsigned k_prefetch_max = 1024u;
/** @brief Default BIU queue: 8088 / IBM 5150 (6 = 8086). */
inline constexpr unsigned k_prefetch_default = 4u;
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
    ~cpu();
    cpu(const cpu &) = delete;
    cpu &operator=(const cpu &) = delete;

    /** @brief Power-on reset. */
    void reset();

    /**
     * @brief 1 MiB executed-opcode map at the same linear addresses as RAM.
     *
     * File is created/truncated to 1048576 bytes and mmap'd. Each instruction
     * fetch writes those bytes into the file at CS:IP (20-bit wrap). Unexecuted
     * addresses stay 0 — a binary listing of what actually ran.
     *
     * @return false if PATH cannot be created or mmap'd.
     */
    bool open_exec_map(const char *path);
    /** @brief msync + munmap. Safe if no map is open. */
    void close_exec_map();
    /** @brief Pointer to the 1 MiB map, or null. */
    const uint8_t *exec_map() const { return exec_map_; }

    /**
     * @brief Linear address (seg<<4)+off, 20-bit wrap.
     */
    static uint32_t phys(uint16_t seg, uint16_t off)
    {
        return ((static_cast<uint32_t>(seg) << 4) + off) & 0xFFFFFu;
    }

    uint8_t mem_read8(uint32_t lin) const
    {
        return ram_[lin & 0xFFFFFu];
    }
    uint16_t mem_read16(uint32_t lin) const
    {
        const uint32_t a = lin & 0xFFFFFu;
        const uint32_t b = (lin + 1u) & 0xFFFFFu;
        return static_cast<uint16_t>(ram_[a] | (static_cast<uint16_t>(ram_[b]) << 8));
    }
    void mem_write8(uint32_t lin, uint8_t v)
    {
        ram_[lin & 0xFFFFFu] = v;
    }
    void mem_write16(uint32_t lin, uint16_t v)
    {
        ram_[lin & 0xFFFFFu] = static_cast<uint8_t>(v);
        ram_[(lin + 1u) & 0xFFFFFu] = static_cast<uint8_t>(v >> 8);
    }

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
     * Sets rom_bios(); does not clear the BIOS intercept — pc::bios_int
     * still services INT 13h (720K CHS). 5150 ROM INT 13 is 40-track only.
     *
     * @return false if @p data is null or empty.
     */
    bool load_bios_5150_8k(const uint8_t *data, size_t n);

    /**
     * @brief True after a successful load_bios_5150_8k.
     */
    bool rom_bios() const
    {
        return rom_bios_;
    }

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

    /**
     * @brief Hot-path hooks without std::function (boot bench).
     * @p after is called after each insn; @p bios like set_bios.
     */
    void set_pc_hooks(void *ctx, void (*after)(void *),
                      bool (*bios)(void *, cpu &, uint8_t));

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

    /**
     * @brief Set 8088-style prefetch queue size (BIU).
     *
     * @param[in] n Bytes kept ahead of the EU. 0 disables the queue (every
     *              fetch reads RAM). Values above @c k_prefetch_max are
     *              clamped to 1024. Default is 4 (8088 / IBM 5150); 6 is 8086.
     *
     * @note 8086/8088 do not snoop data writes. A byte already in the queue
     *       is executed even if RAM was patched (self-modifying code). A
     *       CS:IP discontinuity (jmp/call/ret/int/iret, or debugger
     *       set_cs/set_ip) flushes and refills from RAM. mem_write8/16 must
     *       not flush.
     */
    void set_prefetch_bytes(unsigned n);

    /**
     * @brief Current prefetch queue size (0 = off).
     */
    unsigned prefetch_bytes() const
    {
        return prefetch_n_;
    }

private:
    /**
     * @brief Fill the prefetch queue from RAM at CS:IP + q_len_ (IP 16-bit wrap).
     *
     * Remaining bytes are exactly RAM[CS:IP], RAM[CS:IP+1], … for q_len_
     * bytes (IP wraps at 64K; linear address wraps at 1 MiB).
     */
    void fill_prefetch()
    {
        const uint16_t n = prefetch_n_;
        while (q_len_ < n)
        {
            const uint16_t off = static_cast<uint16_t>(ip_ + q_len_);
            const uint32_t lin = ((static_cast<uint32_t>(cs_) << 4) + off) & 0xFFFFFu;
            const uint16_t slot = static_cast<uint16_t>(
                (static_cast<uint32_t>(q_head_) + static_cast<uint32_t>(q_len_)) &
                (k_prefetch_max - 1u));
            q_[slot] = ram_[lin];
            q_len_ = static_cast<uint16_t>(q_len_ + 1u);
        }
        q_cs_ = cs_;
        q_ip_ = ip_;
    }

    /**
     * @brief Fetch one instruction byte at CS:IP (20-bit wrap; IP wraps at 64K).
     *
     * With prefetch_n_ == 0 this is a direct RAM load. Otherwise the BIU
     * queue supplies the byte; a CS:IP mismatch flushes (no write-snoop).
     */
    uint8_t fetch8()
    {
        if (prefetch_n_ == 0) [[unlikely]]
        {
            const uint32_t lin = ((static_cast<uint32_t>(cs_) << 4) + ip_) & 0xFFFFFu;
            const uint8_t b = ram_[lin];
            if (exec_map_ != nullptr) [[unlikely]]
            {
                exec_map_[lin] = b;
            }
            ++ip_;
            return b;
        }
        if ((cs_ != q_cs_) || (ip_ != q_ip_))
        {
            q_len_ = 0;
            q_head_ = 0;
        }
        if (q_len_ == 0)
        {
            fill_prefetch();
        }
        const uint8_t b = q_[q_head_];
        q_head_ = static_cast<uint16_t>((static_cast<uint32_t>(q_head_) + 1u) &
                                        (k_prefetch_max - 1u));
        q_len_ = static_cast<uint16_t>(q_len_ - 1u);
        const uint32_t lin = ((static_cast<uint32_t>(cs_) << 4) + ip_) & 0xFFFFFu;
        if (exec_map_ != nullptr) [[unlikely]]
        {
            exec_map_[lin] = b;
        }
        ip_ = static_cast<uint16_t>(ip_ + 1u);
        q_ip_ = ip_;
        q_cs_ = cs_;
        fill_prefetch();
        return b;
    }

    /**
     * @brief Fetch one instruction word (LE). Split at 1 MiB wrap and IP=FFFF.
     *
     * A contiguous 16-bit RAM load is only valid when both bytes are the next
     * two CS:IP bytes. Linear FFFFFh wraps to 00000h; IP FFFFh wraps to 0000h
     * in the same CS — those are not the same address. Prefetch uses two
     * fetch8 calls so IP wrap stays correct.
     */
    uint16_t fetch16()
    {
        if (prefetch_n_ != 0)
        {
            const uint16_t lo = fetch8();
            const uint16_t hi = fetch8();
            return static_cast<uint16_t>(lo | (hi << 8));
        }
        const uint32_t lin = ((static_cast<uint32_t>(cs_) << 4) + ip_) & 0xFFFFFu;
        if ((lin != 0xFFFFFu) && (ip_ != 0xFFFFu))
        {
            uint16_t v = 0;
            std::memcpy(&v, ram_ + lin, sizeof(v));
            if (exec_map_ != nullptr) [[unlikely]]
            {
                exec_map_[lin] = static_cast<uint8_t>(v);
                exec_map_[lin + 1u] = static_cast<uint8_t>(v >> 8);
            }
            ip_ = static_cast<uint16_t>(ip_ + 2u);
            return v;
        }
        const uint16_t lo = fetch8();
        const uint16_t hi = fetch8();
        return static_cast<uint16_t>(lo | (hi << 8));
    }
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
    void *hook_ctx_ = nullptr;
    void (*after_fn_)(void *) = nullptr;
    bool (*bios_fn_)(void *, cpu &, uint8_t) = nullptr;
    bool intr_pending_ = false;
    uint8_t intr_vec_ = 0;
    uint16_t seg_ov_ = 0xFFFF; /**< 0xFFFF = none */
    uint8_t rep_ = 0;
    std::unique_ptr<uint8_t[]> mem_;
    uint8_t *ram_ = nullptr; /**< mem_.get(); hot path avoids unique_ptr[] */
    uint8_t *exec_map_ = nullptr;
    int exec_map_fd_ = -1;
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
    bool rom_bios_ = false;
    uint16_t prefetch_n_ = static_cast<uint16_t>(k_prefetch_default);
    uint16_t q_len_ = 0;
    uint16_t q_head_ = 0;
    uint16_t q_cs_ = 0;
    uint16_t q_ip_ = 0;
    uint8_t q_[k_prefetch_max]{};
};

} // namespace iron86

#endif /* IRON86_CPU_H */
