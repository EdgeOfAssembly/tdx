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
    std::memset(mem_, 0, sizeof(mem_));
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
        std::memcpy(mem_ + dst, bytes, copy);
    }
}

bool cpu::step()
{
    if (halted_)
    {
        return false;
    }

    const uint8_t op = fetch8();
    if (op == 0x90)
    {
        return true;
    }
    if (op == 0xF4)
    {
        halted_ = true;
        return true;
    }
    if (op == 0xFA)
    {
        flags_ = static_cast<uint16_t>(flags_ & static_cast<uint16_t>(~k_flag_if));
        return true;
    }
    if (op == 0xFB)
    {
        flags_ = static_cast<uint16_t>(flags_ | k_flag_if);
        return true;
    }
    if (op == 0xEB)
    {
        const int8_t rel = static_cast<int8_t>(fetch8());
        ip_ = static_cast<uint16_t>(static_cast<int32_t>(ip_) + rel);
        return true;
    }
    if (op == 0xCD)
    {
        do_int(fetch8());
        return true;
    }
    if (op == 0xCF)
    {
        ip_ = pop16();
        cs_ = pop16();
        flags_ = pop16();
        return true;
    }
    if ((op >= 0xB8) && (op <= 0xBF))
    {
        const uint16_t imm = fetch16();
        switch (op)
        {
        case 0xB8:
            ax_ = imm;
            break;
        case 0xB9:
            cx_ = imm;
            break;
        case 0xBA:
            dx_ = imm;
            break;
        case 0xBB:
            bx_ = imm;
            break;
        case 0xBC:
            sp_ = imm;
            break;
        case 0xBD:
            bp_ = imm;
            break;
        case 0xBE:
            si_ = imm;
            break;
        default:
            di_ = imm;
            break;
        }
        return true;
    }
    if ((op >= 0xB0) && (op <= 0xB7))
    {
        const uint8_t imm = fetch8();
        switch (op)
        {
        case 0xB0:
            ax_ = static_cast<uint16_t>((ax_ & 0xFF00u) | imm);
            break;
        case 0xB1:
            cx_ = static_cast<uint16_t>((cx_ & 0xFF00u) | imm);
            break;
        case 0xB2:
            dx_ = static_cast<uint16_t>((dx_ & 0xFF00u) | imm);
            break;
        case 0xB3:
            bx_ = static_cast<uint16_t>((bx_ & 0xFF00u) | imm);
            break;
        case 0xB4:
            ax_ = static_cast<uint16_t>((ax_ & 0x00FFu) | (static_cast<uint16_t>(imm) << 8));
            break;
        case 0xB5:
            cx_ = static_cast<uint16_t>((cx_ & 0x00FFu) | (static_cast<uint16_t>(imm) << 8));
            break;
        case 0xB6:
            dx_ = static_cast<uint16_t>((dx_ & 0x00FFu) | (static_cast<uint16_t>(imm) << 8));
            break;
        default:
            bx_ = static_cast<uint16_t>((bx_ & 0x00FFu) | (static_cast<uint16_t>(imm) << 8));
            break;
        }
        return true;
    }
    if (op == 0x40)
    {
        ax_ = static_cast<uint16_t>(ax_ + 1u);
        return true;
    }
    /* Unimplemented: treat as HLT so tests fail closed rather than runaway. */
    halted_ = true;
    return false;
}

} // namespace iron86
