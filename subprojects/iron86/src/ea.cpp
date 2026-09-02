/**
 * @file ea.cpp
 * @brief ModR/M decode and r/m operand access (Py86 intel8086.py).
 */
#include "iron86/cpu.h"

namespace iron86
{

void cpu::decode_modrm()
{
    const uint8_t b = fetch8();
    mr_.mod = static_cast<uint8_t>((b >> 6) & 3u);
    mr_.reg = static_cast<uint8_t>((b >> 3) & 7u);
    mr_.rm = static_cast<uint8_t>(b & 7u);
    mr_.ea = 0;
    mr_.seg = ds_;

    if (mr_.mod == 3u)
    {
        return;
    }

    /* mod=00 rm=110: direct disp16, default DS (not BP/SS). */
    if ((mr_.mod == 0u) && (mr_.rm == 6u))
    {
        mr_.ea = fetch16();
        mr_.seg = (seg_ov_ != 0xFFFFu) ? seg_ov_ : ds_;
        return;
    }

    uint16_t base = rm_base(mr_.rm);
    if (mr_.mod == 1u)
    {
        const int8_t d8 = static_cast<int8_t>(fetch8());
        base = static_cast<uint16_t>(static_cast<int32_t>(base) + d8);
    }
    else if (mr_.mod == 2u)
    {
        base = static_cast<uint16_t>(base + fetch16());
    }
    mr_.ea = base;

    /* BP-based forms (rm 2/3, or rm 6 with disp) default to SS. */
    if ((mr_.rm == 2u) || (mr_.rm == 3u) || (mr_.rm == 6u))
    {
        mr_.seg = ss_;
    }
    if (seg_ov_ != 0xFFFFu)
    {
        mr_.seg = seg_ov_;
    }
}

uint16_t cpu::rm_base(uint8_t rm) const
{
    switch (rm & 7u)
    {
    case 0:
        return static_cast<uint16_t>(bx_ + si_);
    case 1:
        return static_cast<uint16_t>(bx_ + di_);
    case 2:
        return static_cast<uint16_t>(bp_ + si_);
    case 3:
        return static_cast<uint16_t>(bp_ + di_);
    case 4:
        return si_;
    case 5:
        return di_;
    case 6:
        return bp_;
    default:
        return bx_;
    }
}

uint32_t cpu::rm_lin() const
{
    return phys(mr_.seg, mr_.ea);
}

uint16_t cpu::gpr16(uint8_t n) const
{
    switch (n & 7u)
    {
    case 0:
        return ax_;
    case 1:
        return cx_;
    case 2:
        return dx_;
    case 3:
        return bx_;
    case 4:
        return sp_;
    case 5:
        return bp_;
    case 6:
        return si_;
    default:
        return di_;
    }
}

void cpu::set_gpr16(uint8_t n, uint16_t v)
{
    switch (n & 7u)
    {
    case 0:
        ax_ = v;
        break;
    case 1:
        cx_ = v;
        break;
    case 2:
        dx_ = v;
        break;
    case 3:
        bx_ = v;
        break;
    case 4:
        sp_ = v;
        break;
    case 5:
        bp_ = v;
        break;
    case 6:
        si_ = v;
        break;
    default:
        di_ = v;
        break;
    }
}

uint8_t cpu::get_reg8(uint8_t n) const
{
    n = static_cast<uint8_t>(n & 7u);
    if (n < 4u)
    {
        return static_cast<uint8_t>(gpr16(n));
    }
    return static_cast<uint8_t>(gpr16(static_cast<uint8_t>(n - 4u)) >> 8);
}

void cpu::set_reg8(uint8_t n, uint8_t v)
{
    n = static_cast<uint8_t>(n & 7u);
    if (n < 4u)
    {
        const uint16_t w = gpr16(n);
        set_gpr16(n, static_cast<uint16_t>((w & 0xFF00u) | v));
        return;
    }
    const uint8_t r = static_cast<uint8_t>(n - 4u);
    const uint16_t w = gpr16(r);
    set_gpr16(r, static_cast<uint16_t>((w & 0x00FFu) | (static_cast<uint16_t>(v) << 8)));
}

uint8_t cpu::get_rm8() const
{
    if (mr_.mod == 3u)
    {
        return get_reg8(mr_.rm);
    }
    return mem_read8(rm_lin());
}

void cpu::set_rm8(uint8_t v)
{
    if (mr_.mod == 3u)
    {
        set_reg8(mr_.rm, v);
        return;
    }
    mem_write8(rm_lin(), v);
}

uint16_t cpu::get_rm16() const
{
    if (mr_.mod == 3u)
    {
        return gpr16(mr_.rm);
    }
    return mem_read16(rm_lin());
}

void cpu::set_rm16(uint16_t v)
{
    if (mr_.mod == 3u)
    {
        set_gpr16(mr_.rm, v);
        return;
    }
    mem_write16(rm_lin(), v);
}

} // namespace iron86
