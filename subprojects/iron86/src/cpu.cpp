/**
 * @file cpu.cpp
 * @brief 8086 step loop — opcode subset ported from Py86 intel8086.py.
 */
#include "iron86/cpu.h"

#include <algorithm>
#include <cstring>

namespace iron86
{

cpu::cpu()
{
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
    mr_ = {};
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
    push16(flags_);
    push16(cs_);
    push16(ip_);
    flags_ = static_cast<uint16_t>(flags_ & static_cast<uint16_t>(~k_flag_if));
    flags_ = static_cast<uint16_t>(flags_ & static_cast<uint16_t>(~k_flag_tf));
    const uint32_t ivt = static_cast<uint32_t>(vector) * 4u;
    ip_ = mem_read16(ivt);
    cs_ = mem_read16(ivt + 2u);
}

void cpu::set_ivt(uint8_t vector, uint16_t handler_seg, uint16_t handler_off)
{
    const uint32_t ivt = static_cast<uint32_t>(vector) * 4u;
    mem_write16(ivt, handler_off);
    mem_write16(ivt + 2u, handler_seg);
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

bool cpu::step()
{
    if (halted_)
    {
        return false;
    }

    const uint8_t op = fetch8();

    if ((op >= 0x40) && (op <= 0x4F))
    {
        if (op < 0x48)
        {
            inc_r16(static_cast<uint8_t>(op - 0x40));
        }
        else
        {
            dec_r16(static_cast<uint8_t>(op - 0x48));
        }
        return true;
    }
    if ((op >= 0x50) && (op <= 0x57))
    {
        push16(gpr16(static_cast<uint8_t>(op - 0x50)));
        return true;
    }
    if ((op >= 0x58) && (op <= 0x5F))
    {
        set_gpr16(static_cast<uint8_t>(op - 0x58), pop16());
        return true;
    }
    if ((op >= 0x70) && (op <= 0x7F))
    {
        jcc8(op);
        return true;
    }
    if ((op >= 0xB8) && (op <= 0xBF))
    {
        set_gpr16(static_cast<uint8_t>(op - 0xB8), fetch16());
        return true;
    }
    if ((op >= 0xB0) && (op <= 0xB7))
    {
        set_reg8(static_cast<uint8_t>(op - 0xB0), fetch8());
        return true;
    }

    switch (op)
    {
    case 0x04:
    {
        const uint8_t imm = fetch8();
        const uint8_t al = static_cast<uint8_t>(ax_);
        const uint8_t res = static_cast<uint8_t>(al + imm);
        ax_ = static_cast<uint16_t>((ax_ & 0xFF00u) | res);
        set_add_flags(res, al, imm, 8u);
        return true;
    }
    case 0x05:
    {
        const uint16_t imm = fetch16();
        const uint16_t v = ax_;
        const uint16_t res = static_cast<uint16_t>(v + imm);
        ax_ = res;
        set_add_flags(res, v, imm, 16u);
        return true;
    }
    case 0x3C:
    {
        const uint8_t imm = fetch8();
        const uint8_t al = static_cast<uint8_t>(ax_);
        set_sub_flags(static_cast<uint8_t>(al - imm), al, imm, 8u);
        return true;
    }
    case 0x3D:
    {
        const uint16_t imm = fetch16();
        set_sub_flags(static_cast<uint16_t>(ax_ - imm), ax_, imm, 16u);
        return true;
    }
    case 0x88:
        decode_modrm();
        set_rm8(get_reg8(mr_.reg));
        return true;
    case 0x89:
        decode_modrm();
        set_rm16(gpr16(mr_.reg));
        return true;
    case 0x8A:
        decode_modrm();
        set_reg8(mr_.reg, get_rm8());
        return true;
    case 0x8B:
        decode_modrm();
        set_gpr16(mr_.reg, get_rm16());
        return true;
    case 0x90:
        return true;
    case 0xC3:
        ip_ = pop16();
        return true;
    case 0xCD:
        do_int(fetch8());
        return true;
    case 0xCF:
        ip_ = pop16();
        cs_ = pop16();
        flags_ = pop16();
        return true;
    case 0xE8:
    {
        const int16_t rel = static_cast<int16_t>(fetch16());
        push16(ip_);
        ip_ = static_cast<uint16_t>(static_cast<int32_t>(ip_) + rel);
        return true;
    }
    case 0xE9:
    {
        const int16_t rel = static_cast<int16_t>(fetch16());
        ip_ = static_cast<uint16_t>(static_cast<int32_t>(ip_) + rel);
        return true;
    }
    case 0xEB:
    {
        const int8_t rel = static_cast<int8_t>(fetch8());
        ip_ = static_cast<uint16_t>(static_cast<int32_t>(ip_) + rel);
        return true;
    }
    case 0xF4:
        halted_ = true;
        return true;
    case 0xFA:
        flags_ = static_cast<uint16_t>(flags_ & static_cast<uint16_t>(~k_flag_if));
        return true;
    case 0xFB:
        flags_ = static_cast<uint16_t>(flags_ | k_flag_if);
        return true;
    default:
        halted_ = true;
        return false;
    }
}

} // namespace iron86
