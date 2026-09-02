/**
 * @file cpu.cpp
 * @brief 8086 step loop — 256-entry dispatch table (Py86 `handlers[op]`).
 */
#include "iron86/cpu.h"

#include <algorithm>
#include <cstring>

namespace iron86
{

cpu::cpu()
{
    fill_dispatch();
    reset();
}

void cpu::reset()
{
    if (!mem_)
    {
        mem_ = std::make_unique<uint8_t[]>(k_mem_size);
    }
    std::memset(mem_.get(), 0, k_mem_size);
    ax_ = 0;
    cx_ = 0;
    dx_ = 0;
    bx_ = 0;
    sp_ = 0xFFFE;
    bp_ = 0;
    si_ = 0;
    di_ = 0;
    cs_ = 0xFFFF;
    ds_ = 0;
    ss_ = 0;
    es_ = 0;
    ip_ = 0;
    flags_ = k_flags_reset;
    halted_ = false;
    last_op_ = 0;
    seg_ov_ = 0xFFFF;
    rep_ = 0;
    mr_ = {};
    intr_pending_ = false;
    intr_vec_ = 0;
}

uint32_t cpu::phys(uint16_t seg, uint16_t off)
{
    return ((static_cast<uint32_t>(seg) << 4) + off) & 0xFFFFFu;
}

uint8_t cpu::mem_read8(uint32_t lin) const
{
    return mem_[lin & 0xFFFFFu];
}

uint16_t cpu::mem_read16(uint32_t lin) const
{
    const uint32_t a = lin & 0xFFFFFu;
    const uint32_t b = (lin + 1u) & 0xFFFFFu;
    return static_cast<uint16_t>(mem_[a] | (static_cast<uint16_t>(mem_[b]) << 8));
}

void cpu::mem_write8(uint32_t lin, uint8_t v)
{
    mem_[lin & 0xFFFFFu] = v;
}

void cpu::mem_write16(uint32_t lin, uint16_t v)
{
    mem_[lin & 0xFFFFFu] = static_cast<uint8_t>(v);
    mem_[(lin + 1u) & 0xFFFFFu] = static_cast<uint8_t>(v >> 8);
}

uint8_t cpu::fetch8()
{
    const uint8_t b = mem_read8(phys(cs_, ip_));
    ip_ = static_cast<uint16_t>(ip_ + 1u);
    return b;
}

uint16_t cpu::fetch16()
{
    const uint16_t lo = fetch8();
    const uint16_t hi = fetch8();
    return static_cast<uint16_t>(lo | (hi << 8));
}

void cpu::push16(uint16_t v)
{
    sp_ = static_cast<uint16_t>(sp_ - 2u);
    mem_write16(phys(ss_, sp_), v);
}

uint16_t cpu::pop16()
{
    const uint16_t v = mem_read16(phys(ss_, sp_));
    sp_ = static_cast<uint16_t>(sp_ + 2u);
    return v;
}

void cpu::do_int(uint8_t vector)
{
    if (bios_ && bios_(*this, vector))
    {
        return;
    }
    push16(flags_);
    push16(cs_);
    push16(ip_);
    flags_ = static_cast<uint16_t>(flags_ & static_cast<uint16_t>(~k_flag_if));
    flags_ = static_cast<uint16_t>(flags_ & static_cast<uint16_t>(~k_flag_tf));
    const uint32_t ivt = static_cast<uint32_t>(vector) * 4u;
    ip_ = mem_read16(ivt);
    cs_ = mem_read16(ivt + 2u);
}

void cpu::set_bios(std::function<bool(cpu &, uint8_t)> fn)
{
    bios_ = std::move(fn);
}

void cpu::set_io(std::function<uint8_t(uint16_t)> in, std::function<void(uint16_t, uint8_t)> out)
{
    io_in_ = std::move(in);
    io_out_ = std::move(out);
}

void cpu::set_after_step(std::function<void()> fn)
{
    after_step_ = std::move(fn);
}

void cpu::raise_intr(uint8_t vector)
{
    intr_vec_ = vector;
    intr_pending_ = true;
}

void cpu::raise_irq0()
{
    raise_intr(8);
}

void cpu::set_logic_flags(uint32_t res, unsigned size)
{
    update_zsp(res, size);
    set_flag(k_flag_cf, false);
    set_flag(k_flag_of, false);
}

uint16_t cpu::sreg(uint8_t n) const
{
    switch (n & 3u)
    {
    case 0:
        return es_;
    case 1:
        return cs_;
    case 2:
        return ss_;
    default:
        return ds_;
    }
}

void cpu::set_sreg(uint8_t n, uint16_t v)
{
    switch (n & 3u)
    {
    case 0:
        es_ = v;
        break;
    case 1:
        cs_ = v;
        break;
    case 2:
        ss_ = v;
        break;
    default:
        ds_ = v;
        break;
    }
}

void cpu::set_ivt(uint8_t vector, uint16_t handler_seg, uint16_t handler_off)
{
    const uint32_t ivt = static_cast<uint32_t>(vector) * 4u;
    mem_write16(ivt, handler_off);
    mem_write16(ivt + 2u, handler_seg);
}

bool cpu::load_bios_5150_8k(const uint8_t *data, size_t n)
{
    if ((data == nullptr) || (n == 0))
    {
        return false;
    }
    if (!mem_)
    {
        mem_ = std::make_unique<uint8_t[]>(k_mem_size);
        std::memset(mem_.get(), 0, k_mem_size);
    }
    const size_t copy = std::min(n, static_cast<size_t>(8192));
    std::memcpy(mem_.get() + 0xFE000u, data, copy);
    /* Py86: file[-16:-11] → FFFF0 (EA 5B E0 00 F0 on 5700051). */
    if (n >= 16u)
    {
        std::memcpy(mem_.get() + 0xFFFF0u, data + (n - 16u), 5u);
    }
    else if (n >= 5u)
    {
        std::memcpy(mem_.get() + 0xFFFF0u, data + (n - 5u), 5u);
    }
    cs_ = 0xFFFF;
    ip_ = 0;
    ds_ = 0;
    es_ = 0;
    ss_ = 0;
    sp_ = 0xFFFE;
    flags_ = k_flags_reset;
    halted_ = false;
    last_op_ = 0;
    bios_ = {}; /* real BIOS owns INT 10/13/16 */
    return true;
}

void cpu::load_com(const uint8_t *bytes, size_t n, uint16_t cs)
{
    const size_t cap = 0x10000u - 0x100u;
    const size_t copy = std::min(n, cap);
    cs_ = cs;
    ds_ = cs;
    es_ = cs;
    ss_ = cs;
    sp_ = 0xFFFE;
    ip_ = 0x0100;
    halted_ = false;
    if ((bytes != nullptr) && (copy > 0))
    {
        const uint32_t dst = phys(cs, 0x0100);
        std::memcpy(mem_.get() + dst, bytes, copy);
    }
}

void cpu::set_flag(uint16_t bit, bool v)
{
    if (v)
    {
        flags_ = static_cast<uint16_t>(flags_ | bit);
    }
    else
    {
        flags_ = static_cast<uint16_t>(flags_ & static_cast<uint16_t>(~bit));
    }
}

void cpu::update_zsp(uint32_t res, unsigned size)
{
    const uint32_t mask = (size == 16u) ? 0xFFFFu : 0xFFu;
    const uint32_t sign = (size == 16u) ? 0x8000u : 0x80u;
    res &= mask;
    set_flag(k_flag_zf, res == 0u);
    set_flag(k_flag_sf, (res & sign) != 0u);
    uint8_t p = static_cast<uint8_t>(res);
    p = static_cast<uint8_t>(p ^ static_cast<uint8_t>(p >> 4));
    p = static_cast<uint8_t>(p ^ static_cast<uint8_t>(p >> 2));
    p = static_cast<uint8_t>(p ^ static_cast<uint8_t>(p >> 1));
    set_flag(k_flag_pf, (p & 1u) == 0u);
}

void cpu::set_add_flags(uint32_t res, uint32_t op1, uint32_t op2, unsigned size)
{
    const uint32_t mask = (size == 16u) ? 0xFFFFu : 0xFFu;
    const uint32_t sign = (size == 16u) ? 0x8000u : 0x80u;
    res &= mask;
    op1 &= mask;
    op2 &= mask;
    update_zsp(res, size);
    set_flag(k_flag_cf, (op1 + op2) > mask);
    set_flag(k_flag_of, ((op1 ^ op2 ^ sign) & (res ^ op1) & sign) != 0u);
    set_flag(k_flag_af, ((op1 ^ op2 ^ res) & 0x10u) != 0u);
}

void cpu::set_sub_flags(uint32_t res, uint32_t op1, uint32_t op2, unsigned size)
{
    const uint32_t mask = (size == 16u) ? 0xFFFFu : 0xFFu;
    const uint32_t sign = (size == 16u) ? 0x8000u : 0x80u;
    res &= mask;
    op1 &= mask;
    op2 &= mask;
    update_zsp(res, size);
    set_flag(k_flag_cf, op1 < op2);
    set_flag(k_flag_of, ((op1 ^ op2) & (op1 ^ res) & sign) != 0u);
    set_flag(k_flag_af, ((op1 ^ op2 ^ res) & 0x10u) != 0u);
}

void cpu::inc_r16(uint8_t r)
{
    const uint16_t v = gpr16(r);
    const uint16_t res = static_cast<uint16_t>(v + 1u);
    set_gpr16(r, res);
    update_zsp(res, 16u);
    set_flag(k_flag_of, v == 0x7FFFu);
    set_flag(k_flag_af, (v & 0x000Fu) == 0x000Fu);
}

void cpu::dec_r16(uint8_t r)
{
    const uint16_t v = gpr16(r);
    const uint16_t res = static_cast<uint16_t>(v - 1u);
    set_gpr16(r, res);
    update_zsp(res, 16u);
    set_flag(k_flag_of, v == 0x8000u);
    set_flag(k_flag_af, (v & 0x000Fu) == 0u);
}

bool cpu::cond_cc(uint8_t cc) const
{
    const bool cf = (flags_ & k_flag_cf) != 0;
    const bool zf = (flags_ & k_flag_zf) != 0;
    const bool sf = (flags_ & k_flag_sf) != 0;
    const bool of = (flags_ & k_flag_of) != 0;
    const bool pf = (flags_ & k_flag_pf) != 0;
    switch (cc & 0x0Fu)
    {
    case 0x0:
        return of;
    case 0x1:
        return !of;
    case 0x2:
        return cf;
    case 0x3:
        return !cf;
    case 0x4:
        return zf;
    case 0x5:
        return !zf;
    case 0x6:
        return cf || zf;
    case 0x7:
        return !cf && !zf;
    case 0x8:
        return sf;
    case 0x9:
        return !sf;
    case 0xA:
        return pf;
    case 0xB:
        return !pf;
    case 0xC:
        return sf != of;
    case 0xD:
        return sf == of;
    case 0xE:
        return zf || (sf != of);
    default:
        return !zf && (sf == of);
    }
}

void cpu::jcc8(uint8_t cc)
{
    const int8_t rel = static_cast<int8_t>(fetch8());
    if (cond_cc(cc))
    {
        ip_ = static_cast<uint16_t>(static_cast<int32_t>(ip_) + rel);
    }
}

uint16_t cpu::data_seg() const
{
    return (seg_ov_ != 0xFFFFu) ? seg_ov_ : ds_;
}

uint16_t cpu::fetch_imm(bool word, bool sign_ext)
{
    if (!word)
    {
        return fetch8();
    }
    if (!sign_ext)
    {
        return fetch16();
    }
    const uint8_t b = fetch8();
    return static_cast<uint16_t>((b & 0x80u) ? (0xFF00u | b) : b);
}

uint16_t cpu::alu_arith(uint16_t dst, uint16_t src, unsigned size, uint8_t op, uint16_t cin,
                        bool write)
{
    const uint32_t mask = (size == 16u) ? 0xFFFFu : 0xFFu;
    dst = static_cast<uint16_t>(dst & mask);
    src = static_cast<uint16_t>(src & mask);
    uint16_t res = 0;
    switch (op)
    {
    case 0: /* ADD */
    case 2: /* ADC */
    {
        const uint32_t full = static_cast<uint32_t>(dst) + src + cin;
        res = static_cast<uint16_t>(full & mask);
        if (write)
        {
            set_add_flags(res, dst, static_cast<uint32_t>(src) + cin, size);
            set_flag(k_flag_cf, full > mask);
            const uint32_t sign = (size == 16u) ? 0x8000u : 0x80u;
            set_flag(k_flag_of, ((dst ^ res) & (src ^ res) & sign) != 0u);
        }
        break;
    }
    case 1: /* OR */
        res = static_cast<uint16_t>((dst | src) & mask);
        if (write)
        {
            set_logic_flags(res, size);
        }
        break;
    case 3: /* SBB */
    case 5: /* SUB */
    case 7: /* CMP */
    {
        const uint32_t full = static_cast<uint32_t>(dst) - src - cin;
        res = static_cast<uint16_t>(full & mask);
        if (write || (op == 7u))
        {
            set_sub_flags(res, dst, static_cast<uint32_t>(src) + cin, size);
            set_flag(k_flag_cf, full > mask);
            const uint32_t sign = (size == 16u) ? 0x8000u : 0x80u;
            set_flag(k_flag_of, ((dst ^ src) & (dst ^ res) & sign) != 0u);
        }
        break;
    }
    case 4: /* AND */
        res = static_cast<uint16_t>((dst & src) & mask);
        if (write)
        {
            set_logic_flags(res, size);
        }
        break;
    case 6: /* XOR */
        res = static_cast<uint16_t>((dst ^ src) & mask);
        if (write)
        {
            set_logic_flags(res, size);
        }
        break;
    default:
        halted_ = true;
        break;
    }
    return res;
}

void cpu::op_alu_rm(uint8_t opc)
{
    const uint8_t kind = static_cast<uint8_t>((opc >> 3) & 7u);
    const uint8_t form = static_cast<uint8_t>(opc & 7u);
    const bool word = (form & 1u) != 0u;
    const unsigned size = word ? 16u : 8u;
    const uint16_t cin =
        ((kind == 2u) || (kind == 3u)) ? static_cast<uint16_t>((flags_ & k_flag_cf) != 0) : 0;
    const bool store = (kind != 7u);

    if (form <= 1u)
    {
        decode_modrm();
        if (word)
        {
            const uint16_t dst = get_rm16();
            const uint16_t src = gpr16(mr_.reg);
            const uint16_t res = alu_arith(dst, src, size, kind, cin, true);
            if (store)
            {
                set_rm16(res);
            }
        }
        else
        {
            const uint8_t dst = get_rm8();
            const uint8_t src = get_reg8(mr_.reg);
            const uint16_t res = alu_arith(dst, src, size, kind, cin, true);
            if (store)
            {
                set_rm8(static_cast<uint8_t>(res));
            }
        }
        return;
    }
    if (form <= 3u)
    {
        decode_modrm();
        if (word)
        {
            const uint16_t dst = gpr16(mr_.reg);
            const uint16_t src = get_rm16();
            const uint16_t res = alu_arith(dst, src, size, kind, cin, true);
            if (store)
            {
                set_gpr16(mr_.reg, res);
            }
        }
        else
        {
            const uint8_t dst = get_reg8(mr_.reg);
            const uint8_t src = get_rm8();
            const uint16_t res = alu_arith(dst, src, size, kind, cin, true);
            if (store)
            {
                set_reg8(mr_.reg, static_cast<uint8_t>(res));
            }
        }
        return;
    }
    /* form 4/5: AL/AX, imm */
    const uint16_t imm = word ? fetch16() : fetch8();
    if (word)
    {
        const uint16_t res = alu_arith(ax_, imm, 16u, kind, cin, true);
        if (store)
        {
            ax_ = res;
        }
    }
    else
    {
        const uint16_t res = alu_arith(static_cast<uint8_t>(ax_), imm, 8u, kind, cin, true);
        if (store)
        {
            ax_ = static_cast<uint16_t>((ax_ & 0xFF00u) | (res & 0xFFu));
        }
    }
}

void cpu::op_alu_imm(bool word, bool sign_ext)
{
    decode_modrm();
    const uint16_t imm = fetch_imm(word, sign_ext);
    const unsigned size = word ? 16u : 8u;
    const uint8_t kind = mr_.reg;
    const uint16_t cin =
        ((kind == 2u) || (kind == 3u)) ? static_cast<uint16_t>((flags_ & k_flag_cf) != 0) : 0;
    const bool store = (kind != 7u);
    if (word)
    {
        const uint16_t dst = get_rm16();
        const uint16_t res = alu_arith(dst, imm, size, kind, cin, true);
        if (store)
        {
            set_rm16(res);
        }
    }
    else
    {
        const uint8_t dst = get_rm8();
        const uint16_t res = alu_arith(dst, imm, size, kind, cin, true);
        if (store)
        {
            set_rm8(static_cast<uint8_t>(res));
        }
    }
}

void cpu::op_grp3(bool word)
{
    decode_modrm();
    const unsigned size = word ? 16u : 8u;
    const uint32_t mask = word ? 0xFFFFu : 0xFFu;
    switch (mr_.reg)
    {
    case 0:
    case 1: /* TEST r/m, imm ( /1 is alias on 8086 ) */
    {
        const uint16_t imm = word ? fetch16() : fetch8();
        const uint16_t v = word ? get_rm16() : get_rm8();
        set_logic_flags(static_cast<uint16_t>(v & imm), size);
        return;
    }
    case 2: /* NOT */
        if (word)
        {
            set_rm16(static_cast<uint16_t>(~get_rm16()));
        }
        else
        {
            set_rm8(static_cast<uint8_t>(~get_rm8()));
        }
        return;
    case 3: /* NEG */
    {
        const uint16_t v = word ? get_rm16() : get_rm8();
        const uint16_t res = alu_arith(0, v, size, 5u, 0, true);
        if (word)
        {
            set_rm16(res);
        }
        else
        {
            set_rm8(static_cast<uint8_t>(res));
        }
        set_flag(k_flag_cf, v != 0);
        return;
    }
    case 4: /* MUL */
    case 5: /* IMUL (one-operand; 8086 IMUL flags ≈ MUL) */
        if (word)
        {
            const uint32_t prod = static_cast<uint32_t>(ax_) * get_rm16();
            ax_ = static_cast<uint16_t>(prod);
            dx_ = static_cast<uint16_t>(prod >> 16);
            const bool wide = dx_ != 0;
            set_flag(k_flag_cf, wide);
            set_flag(k_flag_of, wide);
        }
        else
        {
            const uint16_t prod =
                static_cast<uint16_t>(static_cast<uint16_t>(ax_ & 0xFFu) * get_rm8());
            ax_ = prod;
            const bool wide = (prod >> 8) != 0;
            set_flag(k_flag_cf, wide);
            set_flag(k_flag_of, wide);
        }
        return;
    case 6: /* DIV */
    case 7: /* IDIV */
    {
        const uint16_t v = word ? get_rm16() : get_rm8();
        if (v == 0)
        {
            do_int(0);
            return;
        }
        if (word)
        {
            const uint32_t num = (static_cast<uint32_t>(dx_) << 16) | ax_;
            if (mr_.reg == 7u)
            {
                const int32_t n = static_cast<int32_t>(num);
                const int32_t d = static_cast<int16_t>(v);
                const int32_t q = n / d;
                const int32_t r = n % d;
                if ((q > 32767) || (q < -32768))
                {
                    do_int(0);
                    return;
                }
                ax_ = static_cast<uint16_t>(q);
                dx_ = static_cast<uint16_t>(r);
            }
            else
            {
                const uint32_t q = num / v;
                const uint32_t r = num % v;
                if (q > 0xFFFFu)
                {
                    do_int(0);
                    return;
                }
                ax_ = static_cast<uint16_t>(q);
                dx_ = static_cast<uint16_t>(r);
            }
        }
        else
        {
            if (mr_.reg == 7u)
            {
                const int32_t n = static_cast<int16_t>(ax_);
                const int32_t d = static_cast<int8_t>(static_cast<uint8_t>(v));
                const int32_t q = n / d;
                const int32_t r = n % d;
                if ((q > 127) || (q < -128))
                {
                    do_int(0);
                    return;
                }
                ax_ = static_cast<uint16_t>((static_cast<uint16_t>(static_cast<uint8_t>(r)) << 8) |
                                            static_cast<uint8_t>(q));
            }
            else
            {
                const uint16_t q = static_cast<uint16_t>(ax_ / v);
                const uint16_t r = static_cast<uint16_t>(ax_ % v);
                if (q > 0xFFu)
                {
                    do_int(0);
                    return;
                }
                ax_ = static_cast<uint16_t>((r << 8) | (q & mask));
            }
        }
        return;
    }
    default:
        halted_ = true;
        return;
    }
}

void cpu::op_shift(bool word, bool via_cl)
{
    decode_modrm();
    uint8_t count = via_cl ? static_cast<uint8_t>(cx_) : 1u;
    count = static_cast<uint8_t>(count & 0x1Fu);
    if (count == 0)
    {
        return;
    }
    const unsigned size = word ? 16u : 8u;
    const uint32_t mask = word ? 0xFFFFu : 0xFFu;
    uint32_t val = word ? get_rm16() : get_rm8();
    const uint32_t orig_msb = (val >> (size - 1u)) & 1u;
    uint32_t new_cf = (flags_ & k_flag_cf) != 0 ? 1u : 0u;
    const uint8_t kind = mr_.reg;
    for (uint8_t i = 0; i < count; i++)
    {
        if (kind == 0u) /* ROL */
        {
            new_cf = (val >> (size - 1u)) & 1u;
            val = ((val << 1) | new_cf) & mask;
        }
        else if (kind == 1u) /* ROR */
        {
            new_cf = val & 1u;
            val = ((val >> 1) | (new_cf << (size - 1u))) & mask;
        }
        else if (kind == 2u) /* RCL */
        {
            const uint32_t old_cf = (flags_ & k_flag_cf) != 0 ? 1u : 0u;
            new_cf = (val >> (size - 1u)) & 1u;
            val = ((val << 1) | old_cf) & mask;
            set_flag(k_flag_cf, new_cf != 0);
        }
        else if (kind == 3u) /* RCR */
        {
            const uint32_t old_cf = (flags_ & k_flag_cf) != 0 ? 1u : 0u;
            new_cf = val & 1u;
            val = ((val >> 1) | (old_cf << (size - 1u))) & mask;
            set_flag(k_flag_cf, new_cf != 0);
        }
        else if (kind == 4u) /* SHL/SAL */
        {
            new_cf = (val >> (size - 1u)) & 1u;
            val = (val << 1) & mask;
        }
        else if (kind == 5u) /* SHR */
        {
            new_cf = val & 1u;
            val = (val >> 1) & mask;
        }
        else if (kind == 7u) /* SAR */
        {
            new_cf = val & 1u;
            const uint32_t sign = val & (1u << (size - 1u));
            val = ((val >> 1) | sign) & mask;
        }
        else
        {
            halted_ = true;
            return;
        }
        set_flag(k_flag_cf, new_cf != 0);
    }
    if (word)
    {
        set_rm16(static_cast<uint16_t>(val));
    }
    else
    {
        set_rm8(static_cast<uint8_t>(val));
    }
    set_flag(k_flag_cf, new_cf != 0);
    if ((kind == 4u) || (kind == 5u) || (kind == 7u))
    {
        update_zsp(val, size);
    }
    if (count == 1u)
    {
        if ((kind == 0u) || (kind == 2u) || (kind == 4u))
        {
            const uint32_t msb = (val >> (size - 1u)) & 1u;
            set_flag(k_flag_of, (msb ^ new_cf) != 0);
        }
        else if ((kind == 1u) || (kind == 3u))
        {
            const uint32_t msb = (val >> (size - 1u)) & 1u;
            const uint32_t next = (val >> (size - 2u)) & 1u;
            set_flag(k_flag_of, msb != next);
        }
        else if (kind == 5u)
        {
            set_flag(k_flag_of, orig_msb != 0);
        }
        else if (kind == 7u)
        {
            set_flag(k_flag_of, false);
        }
    }
}

void cpu::op_incdec8()
{
    decode_modrm();
    if (mr_.reg == 0u)
    {
        const uint8_t v = get_rm8();
        const uint8_t res = static_cast<uint8_t>(v + 1u);
        set_rm8(res);
        update_zsp(res, 8u);
        set_flag(k_flag_of, v == 0x7Fu);
        set_flag(k_flag_af, (v & 0x0Fu) == 0x0Fu);
        return;
    }
    if (mr_.reg == 1u)
    {
        const uint8_t v = get_rm8();
        const uint8_t res = static_cast<uint8_t>(v - 1u);
        set_rm8(res);
        update_zsp(res, 8u);
        set_flag(k_flag_of, v == 0x80u);
        set_flag(k_flag_af, (v & 0x0Fu) == 0u);
        return;
    }
    halted_ = true;
}

void cpu::op_ff()
{
    decode_modrm();
    switch (mr_.reg)
    {
    case 0: /* INC r/m16 */
    {
        const uint16_t v = get_rm16();
        const uint16_t res = static_cast<uint16_t>(v + 1u);
        set_rm16(res);
        update_zsp(res, 16u);
        set_flag(k_flag_of, v == 0x7FFFu);
        set_flag(k_flag_af, (v & 0x000Fu) == 0x000Fu);
        return;
    }
    case 1: /* DEC r/m16 */
    {
        const uint16_t v = get_rm16();
        const uint16_t res = static_cast<uint16_t>(v - 1u);
        set_rm16(res);
        update_zsp(res, 16u);
        set_flag(k_flag_of, v == 0x8000u);
        set_flag(k_flag_af, (v & 0x000Fu) == 0u);
        return;
    }
    case 2: /* CALL near r/m16 */
    {
        const uint16_t target = get_rm16();
        push16(ip_);
        ip_ = target;
        return;
    }
    case 3: /* CALL far m16:16 */
    {
        if (mr_.mod == 3u)
        {
            halted_ = true;
            return;
        }
        const uint32_t lin = rm_lin();
        const uint16_t off = mem_read16(lin);
        const uint16_t seg = mem_read16(lin + 2u);
        push16(cs_);
        push16(ip_);
        ip_ = off;
        cs_ = seg;
        return;
    }
    case 4: /* JMP near r/m16 */
        ip_ = get_rm16();
        return;
    case 5: /* JMP far m16:16 */
    {
        if (mr_.mod == 3u)
        {
            halted_ = true;
            return;
        }
        const uint32_t lin = rm_lin();
        const uint16_t off = mem_read16(lin);
        const uint16_t seg = mem_read16(lin + 2u);
        ip_ = off;
        cs_ = seg;
        return;
    }
    case 6: /* PUSH r/m16 */
        push16(get_rm16());
        return;
    default:
        halted_ = true;
        return;
    }
}

void cpu::one_string(uint8_t op)
{
    const int16_t dir = ((flags_ & k_flag_df) != 0) ? -1 : 1;
    const uint16_t src_seg = data_seg();
    switch (op)
    {
    case 0xA4: /* MOVSB */
        mem_write8(phys(es_, di_), mem_read8(phys(src_seg, si_)));
        si_ = static_cast<uint16_t>(si_ + dir);
        di_ = static_cast<uint16_t>(di_ + dir);
        break;
    case 0xA5: /* MOVSW */
        mem_write16(phys(es_, di_), mem_read16(phys(src_seg, si_)));
        si_ = static_cast<uint16_t>(si_ + static_cast<int16_t>(dir * 2));
        di_ = static_cast<uint16_t>(di_ + static_cast<int16_t>(dir * 2));
        break;
    case 0xA6: /* CMPSB */
    {
        const uint8_t a = mem_read8(phys(src_seg, si_));
        const uint8_t b = mem_read8(phys(es_, di_));
        set_sub_flags(static_cast<uint8_t>(a - b), a, b, 8u);
        si_ = static_cast<uint16_t>(si_ + dir);
        di_ = static_cast<uint16_t>(di_ + dir);
        break;
    }
    case 0xA7: /* CMPSW */
    {
        const uint16_t a = mem_read16(phys(src_seg, si_));
        const uint16_t b = mem_read16(phys(es_, di_));
        set_sub_flags(static_cast<uint16_t>(a - b), a, b, 16u);
        si_ = static_cast<uint16_t>(si_ + static_cast<int16_t>(dir * 2));
        di_ = static_cast<uint16_t>(di_ + static_cast<int16_t>(dir * 2));
        break;
    }
    case 0xAA: /* STOSB */
        mem_write8(phys(es_, di_), static_cast<uint8_t>(ax_));
        di_ = static_cast<uint16_t>(di_ + dir);
        break;
    case 0xAB: /* STOSW */
        mem_write16(phys(es_, di_), ax_);
        di_ = static_cast<uint16_t>(di_ + static_cast<int16_t>(dir * 2));
        break;
    case 0xAC: /* LODSB */
        ax_ = static_cast<uint16_t>((ax_ & 0xFF00u) | mem_read8(phys(src_seg, si_)));
        si_ = static_cast<uint16_t>(si_ + dir);
        break;
    case 0xAD: /* LODSW */
        ax_ = mem_read16(phys(src_seg, si_));
        si_ = static_cast<uint16_t>(si_ + static_cast<int16_t>(dir * 2));
        break;
    case 0xAE: /* SCASB */
    {
        const uint8_t b = mem_read8(phys(es_, di_));
        const uint8_t a = static_cast<uint8_t>(ax_);
        set_sub_flags(static_cast<uint8_t>(a - b), a, b, 8u);
        di_ = static_cast<uint16_t>(di_ + dir);
        break;
    }
    case 0xAF: /* SCASW */
    {
        const uint16_t b = mem_read16(phys(es_, di_));
        set_sub_flags(static_cast<uint16_t>(ax_ - b), ax_, b, 16u);
        di_ = static_cast<uint16_t>(di_ + static_cast<int16_t>(dir * 2));
        break;
    }
    default:
        halted_ = true;
        break;
    }
}

void cpu::op_string(uint8_t op)
{
    const bool is_scan = (op == 0xA6) || (op == 0xA7) || (op == 0xAE) || (op == 0xAF);
    if (rep_ == 0)
    {
        one_string(op);
        return;
    }
    while (cx_ != 0)
    {
        one_string(op);
        cx_ = static_cast<uint16_t>(cx_ - 1u);
        if (halted_)
        {
            break;
        }
        if (is_scan)
        {
            const bool zf = (flags_ & k_flag_zf) != 0;
            if ((rep_ == 0xF2) && zf)
            {
                break;
            }
            if ((rep_ == 0xF3) && !zf)
            {
                break;
            }
        }
    }
}

void cpu::op_loop(uint8_t kind)
{
    const int8_t rel = static_cast<int8_t>(fetch8());
    if (kind == 3u) /* JCXZ */
    {
        if (cx_ == 0)
        {
            ip_ = static_cast<uint16_t>(static_cast<int32_t>(ip_) + rel);
        }
        return;
    }
    cx_ = static_cast<uint16_t>(cx_ - 1u);
    const bool zf = (flags_ & k_flag_zf) != 0;
    bool take = cx_ != 0;
    if (kind == 0u) /* LOOPNE */
    {
        take = take && !zf;
    }
    else if (kind == 1u) /* LOOPE */
    {
        take = take && zf;
    }
    if (take)
    {
        ip_ = static_cast<uint16_t>(static_cast<int32_t>(ip_) + rel);
    }
}

void cpu::op_les_lds(bool les)
{
    decode_modrm();
    if (mr_.mod == 3u)
    {
        halted_ = true;
        return;
    }
    const uint32_t lin = rm_lin();
    const uint16_t off = mem_read16(lin);
    const uint16_t seg = mem_read16(lin + 2u);
    set_gpr16(mr_.reg, off);
    if (les)
    {
        es_ = seg;
    }
    else
    {
        ds_ = seg;
    }
}

uint8_t cpu::in8(uint16_t port)
{
    if (io_in_)
    {
        return io_in_(port);
    }
    return 0xFF;
}

void cpu::out8(uint16_t port, uint8_t v)
{
    if (io_out_)
    {
        io_out_(port, v);
    }
}

cpu::handler cpu::dispatch_[256]{};
bool cpu::dispatch_ready_ = false;

void cpu::op_unimpl() { halted_ = true; }
void cpu::op_nop() {}
void cpu::op_hlt() { halted_ = true; }
void cpu::op_pre_es() { seg_ov_ = es_; prefix_more_ = true; }
void cpu::op_pre_cs() { seg_ov_ = cs_; prefix_more_ = true; }
void cpu::op_pre_ss() { seg_ov_ = ss_; prefix_more_ = true; }
void cpu::op_pre_ds() { seg_ov_ = ds_; prefix_more_ = true; }
void cpu::op_daa()
{
    /* 8086/Py86: low-nibble +6 sets AF; CF from old AL>99h or old CF; then Z/S/P. */
    const uint8_t old_al = static_cast<uint8_t>(ax_);
    uint8_t al = old_al;
    const bool old_af = (flags_ & k_flag_af) != 0;
    const bool old_cf = (flags_ & k_flag_cf) != 0;
    if (((al & 0x0Fu) > 9u) || old_af)
    {
        al = static_cast<uint8_t>(al + 6u);
        set_flag(k_flag_af, true);
    }
    else
    {
        set_flag(k_flag_af, false);
    }
    if ((old_al > 0x99u) || old_cf)
    {
        al = static_cast<uint8_t>(al + 0x60u);
        set_flag(k_flag_cf, true);
    }
    else
    {
        set_flag(k_flag_cf, false);
    }
    ax_ = static_cast<uint16_t>((ax_ & 0xFF00u) | al);
    update_zsp(al, 8u);
}
void cpu::op_das()
{
    const uint8_t old_al = static_cast<uint8_t>(ax_);
    uint8_t al = old_al;
    const bool old_af = (flags_ & k_flag_af) != 0;
    const bool old_cf = (flags_ & k_flag_cf) != 0;
    if (((al & 0x0Fu) > 9u) || old_af)
    {
        al = static_cast<uint8_t>(al - 6u);
        set_flag(k_flag_af, true);
    }
    else
    {
        set_flag(k_flag_af, false);
    }
    if ((old_al > 0x99u) || old_cf)
    {
        al = static_cast<uint8_t>(al - 0x60u);
        set_flag(k_flag_cf, true);
    }
    else
    {
        set_flag(k_flag_cf, false);
    }
    ax_ = static_cast<uint16_t>((ax_ & 0xFF00u) | al);
    update_zsp(al, 8u);
}
void cpu::op_aaa()
{
    uint8_t al = static_cast<uint8_t>(ax_);
    uint8_t ah = static_cast<uint8_t>(ax_ >> 8);
    if (((al & 0x0Fu) > 9u) || ((flags_ & k_flag_af) != 0))
    {
        al = static_cast<uint8_t>(al + 6u);
        ah = static_cast<uint8_t>(ah + 1u);
        set_flag(k_flag_af, true);
        set_flag(k_flag_cf, true);
    }
    else
    {
        set_flag(k_flag_af, false);
        set_flag(k_flag_cf, false);
    }
    al = static_cast<uint8_t>(al & 0x0Fu);
    ax_ = static_cast<uint16_t>((static_cast<uint16_t>(ah) << 8) | al);
}
void cpu::op_aas()
{
    uint8_t al = static_cast<uint8_t>(ax_);
    uint8_t ah = static_cast<uint8_t>(ax_ >> 8);
    if (((al & 0x0Fu) > 9u) || ((flags_ & k_flag_af) != 0))
    {
        al = static_cast<uint8_t>(al - 6u);
        ah = static_cast<uint8_t>(ah - 1u);
        set_flag(k_flag_af, true);
        set_flag(k_flag_cf, true);
    }
    else
    {
        set_flag(k_flag_af, false);
        set_flag(k_flag_cf, false);
    }
    al = static_cast<uint8_t>(al & 0x0Fu);
    ax_ = static_cast<uint16_t>((static_cast<uint16_t>(ah) << 8) | al);
}
void cpu::op_lock() { prefix_more_ = true; }
void cpu::op_repne() { rep_ = 0xF2; prefix_more_ = true; }
void cpu::op_repe() { rep_ = 0xF3; prefix_more_ = true; }
void cpu::op_alu() { op_alu_rm(last_op_); }
void cpu::op_push_sr() { push16(sreg(static_cast<uint8_t>(last_op_ >> 3))); }
void cpu::op_pop_sr() { set_sreg(static_cast<uint8_t>(last_op_ >> 3), pop16()); }
void cpu::op_inc_r() { inc_r16(static_cast<uint8_t>(last_op_ & 7u)); }
void cpu::op_dec_r() { dec_r16(static_cast<uint8_t>(last_op_ & 7u)); }
void cpu::op_push_r() { push16(gpr16(static_cast<uint8_t>(last_op_ & 7u))); }
void cpu::op_pop_r() { set_gpr16(static_cast<uint8_t>(last_op_ & 7u), pop16()); }
void cpu::op_jcc() { jcc8(last_op_); }
void cpu::op_mov_i8() { set_reg8(static_cast<uint8_t>(last_op_ - 0xB0), fetch8()); }
void cpu::op_mov_i16() { set_gpr16(static_cast<uint8_t>(last_op_ - 0xB8), fetch16()); }
void cpu::op_xchg_ax()
{
    const uint8_t r = static_cast<uint8_t>(last_op_ - 0x90);
    const uint16_t t = ax_;
    ax_ = gpr16(r);
    set_gpr16(r, t);
}
void cpu::op_test8()
{
    decode_modrm();
    set_logic_flags(static_cast<uint8_t>(get_rm8() & get_reg8(mr_.reg)), 8u);
}
void cpu::op_test16()
{
    decode_modrm();
    set_logic_flags(static_cast<uint16_t>(get_rm16() & gpr16(mr_.reg)), 16u);
}
void cpu::op_xchg8()
{
    decode_modrm();
    const uint8_t a = get_rm8();
    const uint8_t b = get_reg8(mr_.reg);
    set_rm8(b);
    set_reg8(mr_.reg, a);
}
void cpu::op_xchg16()
{
    decode_modrm();
    const uint16_t a = get_rm16();
    const uint16_t b = gpr16(mr_.reg);
    set_rm16(b);
    set_gpr16(mr_.reg, a);
}
void cpu::op_grp80() { op_alu_imm(false, false); }
void cpu::op_grp81() { op_alu_imm(true, false); }
void cpu::op_grp83() { op_alu_imm(true, true); }
void cpu::op_mov_rm8_r8()
{
    decode_modrm();
    set_rm8(get_reg8(mr_.reg));
}
void cpu::op_mov_rm16_r16()
{
    decode_modrm();
    set_rm16(gpr16(mr_.reg));
}
void cpu::op_mov_r8_rm8()
{
    decode_modrm();
    set_reg8(mr_.reg, get_rm8());
}
void cpu::op_mov_r16_rm16()
{
    decode_modrm();
    set_gpr16(mr_.reg, get_rm16());
}
void cpu::op_mov_rm_sr()
{
    decode_modrm();
    set_rm16(sreg(mr_.reg));
}
void cpu::op_lea()
{
    decode_modrm();
    set_gpr16(mr_.reg, (mr_.mod == 3u) ? gpr16(mr_.rm) : mr_.ea);
}
void cpu::op_mov_sr_rm()
{
    decode_modrm();
    set_sreg(mr_.reg, get_rm16());
}
void cpu::op_pop_rm()
{
    decode_modrm();
    set_rm16(pop16());
}
void cpu::op_cbw() { ax_ = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(ax_))); }
void cpu::op_cwd() { dx_ = ((ax_ & 0x8000u) != 0) ? 0xFFFFu : 0; }
void cpu::op_call_far()
{
    const uint16_t off = fetch16();
    const uint16_t seg = fetch16();
    push16(cs_);
    push16(ip_);
    ip_ = off;
    cs_ = seg;
}
void cpu::op_wait() {} /* no 8087: WAIT is NOP */
void cpu::op_pushf() { push16(flags_); }
void cpu::op_popf() { flags_ = pop16(); }
void cpu::op_sahf()
{
    flags_ = static_cast<uint16_t>((flags_ & 0xFF00u) | (static_cast<uint8_t>(ax_ >> 8) & 0xD5u) |
                                   0x02u);
}
void cpu::op_lahf()
{
    ax_ = static_cast<uint16_t>((ax_ & 0x00FFu) | (static_cast<uint16_t>(flags_ & 0x00FFu) << 8));
}
void cpu::op_mov_al_m()
{
    const uint16_t off = fetch16();
    ax_ = static_cast<uint16_t>((ax_ & 0xFF00u) | mem_read8(phys(data_seg(), off)));
}
void cpu::op_mov_ax_m()
{
    const uint16_t off = fetch16();
    ax_ = mem_read16(phys(data_seg(), off));
}
void cpu::op_mov_m_al()
{
    const uint16_t off = fetch16();
    mem_write8(phys(data_seg(), off), static_cast<uint8_t>(ax_));
}
void cpu::op_mov_m_ax()
{
    const uint16_t off = fetch16();
    mem_write16(phys(data_seg(), off), ax_);
}
void cpu::op_str() { op_string(last_op_); }
void cpu::op_test_al()
{
    set_logic_flags(static_cast<uint8_t>(static_cast<uint8_t>(ax_) & fetch8()), 8u);
}
void cpu::op_test_ax() { set_logic_flags(static_cast<uint16_t>(ax_ & fetch16()), 16u); }
void cpu::op_retn_imm()
{
    const uint16_t n = fetch16();
    ip_ = pop16();
    sp_ = static_cast<uint16_t>(sp_ + n);
}
void cpu::op_retn() { ip_ = pop16(); }
void cpu::op_les() { op_les_lds(true); }
void cpu::op_lds() { op_les_lds(false); }
void cpu::op_mov_rm8_i()
{
    decode_modrm();
    set_rm8(fetch8());
}
void cpu::op_mov_rm16_i()
{
    decode_modrm();
    set_rm16(fetch16());
}
void cpu::op_retf_imm()
{
    const uint16_t n = fetch16();
    ip_ = pop16();
    cs_ = pop16();
    sp_ = static_cast<uint16_t>(sp_ + n);
}
void cpu::op_retf()
{
    ip_ = pop16();
    cs_ = pop16();
}
void cpu::op_int3() { do_int(3); }
void cpu::op_int() { do_int(fetch8()); }
void cpu::op_into()
{
    if ((flags_ & k_flag_of) != 0)
    {
        do_int(4);
    }
}
void cpu::op_iret()
{
    ip_ = pop16();
    cs_ = pop16();
    flags_ = pop16();
}
void cpu::op_shift_d0() { op_shift(false, false); }
void cpu::op_shift_d1() { op_shift(true, false); }
void cpu::op_shift_d2() { op_shift(false, true); }
void cpu::op_shift_d3() { op_shift(true, true); }
void cpu::op_aam()
{
    const uint8_t base = fetch8();
    if (base == 0)
    {
        do_int(0);
        return;
    }
    const uint8_t al = static_cast<uint8_t>(ax_);
    const uint8_t ah = static_cast<uint8_t>(al / base);
    const uint8_t r = static_cast<uint8_t>(al % base);
    ax_ = static_cast<uint16_t>((static_cast<uint16_t>(ah) << 8) | r);
    update_zsp(r, 8u);
    set_flag(k_flag_cf, false);
    set_flag(k_flag_of, false);
}
void cpu::op_aad()
{
    const uint8_t base = fetch8();
    const uint8_t al = static_cast<uint8_t>(ax_);
    const uint8_t ah = static_cast<uint8_t>(ax_ >> 8);
    const uint8_t res = static_cast<uint8_t>(al + (ah * base));
    ax_ = res;
    update_zsp(res, 8u);
    set_flag(k_flag_cf, false);
    set_flag(k_flag_of, false);
}
void cpu::op_xlat()
{
    ax_ = static_cast<uint16_t>(
        (ax_ & 0xFF00u) |
        mem_read8(phys(data_seg(), static_cast<uint16_t>(bx_ + static_cast<uint8_t>(ax_)))));
}
void cpu::op_esc() { decode_modrm(); } /* no 8087: consume ModR/M (+disp), NOP */
void cpu::op_loop_op() { op_loop(static_cast<uint8_t>(last_op_ - 0xE0)); }
void cpu::op_in_i8() { ax_ = static_cast<uint16_t>((ax_ & 0xFF00u) | in8(fetch8())); }
void cpu::op_in_i16()
{
    const uint8_t p = fetch8();
    ax_ = static_cast<uint16_t>(in8(p) | (static_cast<uint16_t>(in8(static_cast<uint16_t>(p + 1u))) << 8));
}
void cpu::op_out_i8() { out8(fetch8(), static_cast<uint8_t>(ax_)); }
void cpu::op_out_i16()
{
    const uint8_t p = fetch8();
    out8(p, static_cast<uint8_t>(ax_));
    out8(static_cast<uint16_t>(p + 1u), static_cast<uint8_t>(ax_ >> 8));
}
void cpu::op_call_rel()
{
    const int16_t rel = static_cast<int16_t>(fetch16());
    push16(ip_);
    ip_ = static_cast<uint16_t>(static_cast<int32_t>(ip_) + rel);
}
void cpu::op_jmp_rel16()
{
    const int16_t rel = static_cast<int16_t>(fetch16());
    ip_ = static_cast<uint16_t>(static_cast<int32_t>(ip_) + rel);
}
void cpu::op_jmp_far()
{
    const uint16_t off = fetch16();
    const uint16_t seg = fetch16();
    ip_ = off;
    cs_ = seg;
}
void cpu::op_jmp_rel8()
{
    const int8_t rel = static_cast<int8_t>(fetch8());
    ip_ = static_cast<uint16_t>(static_cast<int32_t>(ip_) + rel);
}
void cpu::op_in_dx8() { ax_ = static_cast<uint16_t>((ax_ & 0xFF00u) | in8(dx_)); }
void cpu::op_in_dx16()
{
    ax_ = static_cast<uint16_t>(in8(dx_) | (static_cast<uint16_t>(in8(static_cast<uint16_t>(dx_ + 1u))) << 8));
}
void cpu::op_out_dx8() { out8(dx_, static_cast<uint8_t>(ax_)); }
void cpu::op_out_dx16()
{
    out8(dx_, static_cast<uint8_t>(ax_));
    out8(static_cast<uint16_t>(dx_ + 1u), static_cast<uint8_t>(ax_ >> 8));
}
void cpu::op_cmc() { flags_ = static_cast<uint16_t>(flags_ ^ k_flag_cf); }
void cpu::op_grp3_8() { op_grp3(false); }
void cpu::op_grp3_16() { op_grp3(true); }
void cpu::op_clc() { set_flag(k_flag_cf, false); }
void cpu::op_stc() { set_flag(k_flag_cf, true); }
void cpu::op_cli() { flags_ = static_cast<uint16_t>(flags_ & static_cast<uint16_t>(~k_flag_if)); }
void cpu::op_sti() { flags_ = static_cast<uint16_t>(flags_ | k_flag_if); }
void cpu::op_cld() { flags_ = static_cast<uint16_t>(flags_ & static_cast<uint16_t>(~k_flag_df)); }
void cpu::op_std() { flags_ = static_cast<uint16_t>(flags_ | k_flag_df); }
void cpu::op_fe() { op_incdec8(); }
void cpu::op_ff_op() { op_ff(); }

void cpu::fill_dispatch()
{
    if (dispatch_ready_)
    {
        return;
    }
    for (unsigned i = 0; i < 256u; i++)
    {
        dispatch_[i] = &cpu::op_unimpl;
    }
    for (unsigned op = 0; op < 0x40u; op++)
    {
        if ((op & 7u) <= 5u)
        {
            dispatch_[op] = &cpu::op_alu;
        }
    }
    dispatch_[0x06] = &cpu::op_push_sr;
    dispatch_[0x07] = &cpu::op_pop_sr;
    dispatch_[0x0E] = &cpu::op_push_sr;
    dispatch_[0x0F] = &cpu::op_pop_sr;
    dispatch_[0x16] = &cpu::op_push_sr;
    dispatch_[0x17] = &cpu::op_pop_sr;
    dispatch_[0x1E] = &cpu::op_push_sr;
    dispatch_[0x1F] = &cpu::op_pop_sr;
    dispatch_[0x26] = &cpu::op_pre_es;
    dispatch_[0x27] = &cpu::op_daa;
    dispatch_[0x2E] = &cpu::op_pre_cs;
    dispatch_[0x2F] = &cpu::op_das;
    dispatch_[0x36] = &cpu::op_pre_ss;
    dispatch_[0x37] = &cpu::op_aaa;
    dispatch_[0x3E] = &cpu::op_pre_ds;
    dispatch_[0x3F] = &cpu::op_aas;
    for (unsigned i = 0; i < 8u; i++)
    {
        dispatch_[0x40u + i] = &cpu::op_inc_r;
        dispatch_[0x48u + i] = &cpu::op_dec_r;
        dispatch_[0x50u + i] = &cpu::op_push_r;
        dispatch_[0x58u + i] = &cpu::op_pop_r;
        dispatch_[0x70u + i] = &cpu::op_jcc;
        dispatch_[0x78u + i] = &cpu::op_jcc;
        dispatch_[0xB0u + i] = &cpu::op_mov_i8;
        dispatch_[0xB8u + i] = &cpu::op_mov_i16;
        dispatch_[0x90u + i] = &cpu::op_xchg_ax;
    }
    dispatch_[0x90] = &cpu::op_nop;
    dispatch_[0x80] = &cpu::op_grp80;
    dispatch_[0x82] = &cpu::op_grp80;
    dispatch_[0x81] = &cpu::op_grp81;
    dispatch_[0x83] = &cpu::op_grp83;
    dispatch_[0x84] = &cpu::op_test8;
    dispatch_[0x85] = &cpu::op_test16;
    dispatch_[0x86] = &cpu::op_xchg8;
    dispatch_[0x87] = &cpu::op_xchg16;
    dispatch_[0x88] = &cpu::op_mov_rm8_r8;
    dispatch_[0x89] = &cpu::op_mov_rm16_r16;
    dispatch_[0x8A] = &cpu::op_mov_r8_rm8;
    dispatch_[0x8B] = &cpu::op_mov_r16_rm16;
    dispatch_[0x8C] = &cpu::op_mov_rm_sr;
    dispatch_[0x8D] = &cpu::op_lea;
    dispatch_[0x8E] = &cpu::op_mov_sr_rm;
    dispatch_[0x8F] = &cpu::op_pop_rm;
    dispatch_[0x98] = &cpu::op_cbw;
    dispatch_[0x99] = &cpu::op_cwd;
    dispatch_[0x9A] = &cpu::op_call_far;
    dispatch_[0x9B] = &cpu::op_wait;
    dispatch_[0x9C] = &cpu::op_pushf;
    dispatch_[0x9D] = &cpu::op_popf;
    dispatch_[0x9E] = &cpu::op_sahf;
    dispatch_[0x9F] = &cpu::op_lahf;
    dispatch_[0xA0] = &cpu::op_mov_al_m;
    dispatch_[0xA1] = &cpu::op_mov_ax_m;
    dispatch_[0xA2] = &cpu::op_mov_m_al;
    dispatch_[0xA3] = &cpu::op_mov_m_ax;
    dispatch_[0xA4] = &cpu::op_str;
    dispatch_[0xA5] = &cpu::op_str;
    dispatch_[0xA6] = &cpu::op_str;
    dispatch_[0xA7] = &cpu::op_str;
    dispatch_[0xAA] = &cpu::op_str;
    dispatch_[0xAB] = &cpu::op_str;
    dispatch_[0xAC] = &cpu::op_str;
    dispatch_[0xAD] = &cpu::op_str;
    dispatch_[0xAE] = &cpu::op_str;
    dispatch_[0xAF] = &cpu::op_str;
    dispatch_[0xA8] = &cpu::op_test_al;
    dispatch_[0xA9] = &cpu::op_test_ax;
    dispatch_[0xC2] = &cpu::op_retn_imm;
    dispatch_[0xC3] = &cpu::op_retn;
    dispatch_[0xC4] = &cpu::op_les;
    dispatch_[0xC5] = &cpu::op_lds;
    dispatch_[0xC6] = &cpu::op_mov_rm8_i;
    dispatch_[0xC7] = &cpu::op_mov_rm16_i;
    dispatch_[0xCA] = &cpu::op_retf_imm;
    dispatch_[0xCB] = &cpu::op_retf;
    dispatch_[0xCC] = &cpu::op_int3;
    dispatch_[0xCD] = &cpu::op_int;
    dispatch_[0xCE] = &cpu::op_into;
    dispatch_[0xCF] = &cpu::op_iret;
    dispatch_[0xD0] = &cpu::op_shift_d0;
    dispatch_[0xD1] = &cpu::op_shift_d1;
    dispatch_[0xD2] = &cpu::op_shift_d2;
    dispatch_[0xD3] = &cpu::op_shift_d3;
    dispatch_[0xD4] = &cpu::op_aam;
    dispatch_[0xD5] = &cpu::op_aad;
    dispatch_[0xD7] = &cpu::op_xlat;
    for (unsigned op = 0xD8u; op < 0xE0u; op++)
    {
        dispatch_[op] = &cpu::op_esc;
    }
    dispatch_[0xE0] = &cpu::op_loop_op;
    dispatch_[0xE1] = &cpu::op_loop_op;
    dispatch_[0xE2] = &cpu::op_loop_op;
    dispatch_[0xE3] = &cpu::op_loop_op;
    dispatch_[0xE4] = &cpu::op_in_i8;
    dispatch_[0xE5] = &cpu::op_in_i16;
    dispatch_[0xE6] = &cpu::op_out_i8;
    dispatch_[0xE7] = &cpu::op_out_i16;
    dispatch_[0xE8] = &cpu::op_call_rel;
    dispatch_[0xE9] = &cpu::op_jmp_rel16;
    dispatch_[0xEA] = &cpu::op_jmp_far;
    dispatch_[0xEB] = &cpu::op_jmp_rel8;
    dispatch_[0xEC] = &cpu::op_in_dx8;
    dispatch_[0xED] = &cpu::op_in_dx16;
    dispatch_[0xEE] = &cpu::op_out_dx8;
    dispatch_[0xEF] = &cpu::op_out_dx16;
    dispatch_[0xF0] = &cpu::op_lock;
    dispatch_[0xF1] = &cpu::op_nop;
    dispatch_[0xF2] = &cpu::op_repne;
    dispatch_[0xF3] = &cpu::op_repe;
    dispatch_[0xF4] = &cpu::op_hlt;
    dispatch_[0xF5] = &cpu::op_cmc;
    dispatch_[0xF6] = &cpu::op_grp3_8;
    dispatch_[0xF7] = &cpu::op_grp3_16;
    dispatch_[0xF8] = &cpu::op_clc;
    dispatch_[0xF9] = &cpu::op_stc;
    dispatch_[0xFA] = &cpu::op_cli;
    dispatch_[0xFB] = &cpu::op_sti;
    dispatch_[0xFC] = &cpu::op_cld;
    dispatch_[0xFD] = &cpu::op_std;
    dispatch_[0xFE] = &cpu::op_fe;
    dispatch_[0xFF] = &cpu::op_ff_op;
    dispatch_ready_ = true;
}

bool cpu::step()
{
    if (halted_)
    {
        return false;
    }

    seg_ov_ = 0xFFFF;
    rep_ = 0;
    do
    {
        prefix_more_ = false;
        last_op_ = fetch8();
        (this->*dispatch_[last_op_])();
    } while (prefix_more_ && !halted_);

    if (after_step_)
    {
        after_step_();
    }
    if ((!halted_) && intr_pending_ && ((flags_ & k_flag_if) != 0))
    {
        intr_pending_ = false;
        do_int(intr_vec_);
    }

    return (last_op_ == 0xF4) || !halted_;
}

} // namespace iron86
