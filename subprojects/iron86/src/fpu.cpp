/**
 * @file fpu.cpp
 * @brief Intel 8087 NPX — 80-bit stack, ESC D8–DF (not 287/387).
 */
#include "iron86/fpu.h"
#include "iron86/cpu.h"

#include <cmath>
#include <cstring>

namespace iron86
{

namespace
{

constexpr uint16_t k_sw_ie = 0x0001;
constexpr uint16_t k_sw_c0 = 0x0100;
constexpr uint16_t k_sw_c2 = 0x0400;
constexpr uint16_t k_sw_c3 = 0x4000;

uint32_t lin_of(const modrm &mr)
{
    return cpu::phys(mr.seg, mr.ea);
}

uint16_t load_u16(cpu &c, uint32_t lin)
{
    return c.mem_read16(lin);
}

uint32_t load_u32(cpu &c, uint32_t lin)
{
    return static_cast<uint32_t>(c.mem_read16(lin)) |
           (static_cast<uint32_t>(c.mem_read16(lin + 2u)) << 16);
}

uint64_t load_u64(cpu &c, uint32_t lin)
{
    return static_cast<uint64_t>(load_u32(c, lin)) |
           (static_cast<uint64_t>(load_u32(c, lin + 4u)) << 32);
}

void store_u16(cpu &c, uint32_t lin, uint16_t v)
{
    c.mem_write16(lin, v);
}

void store_u32(cpu &c, uint32_t lin, uint32_t v)
{
    c.mem_write16(lin, static_cast<uint16_t>(v));
    c.mem_write16(lin + 2u, static_cast<uint16_t>(v >> 16));
}

void store_u64(cpu &c, uint32_t lin, uint64_t v)
{
    store_u32(c, lin, static_cast<uint32_t>(v));
    store_u32(c, lin + 4u, static_cast<uint32_t>(v >> 32));
}

long double from_f32(uint32_t bits)
{
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(f));
    return static_cast<long double>(f);
}

uint32_t to_f32(long double v)
{
    float f = static_cast<float>(v);
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(f));
    return bits;
}

long double from_f64(uint64_t bits)
{
    double d = 0.0;
    std::memcpy(&d, &bits, sizeof(d));
    return static_cast<long double>(d);
}

uint64_t to_f64(long double v)
{
    double d = static_cast<double>(v);
    uint64_t bits = 0;
    std::memcpy(&bits, &d, sizeof(d));
    return bits;
}

long double from_f80(cpu &c, uint32_t lin)
{
    unsigned char raw[16]{};
    unsigned i = 0;
    for (i = 0; i < 10u; i++)
    {
        raw[i] = c.mem_read8(lin + i);
    }
    long double v = 0.0L;
    std::memcpy(&v, raw, 10);
    return v;
}

void to_f80(cpu &c, uint32_t lin, long double v)
{
    unsigned char raw[16]{};
    std::memcpy(raw, &v, 10);
    unsigned i = 0;
    for (i = 0; i < 10u; i++)
    {
        c.mem_write8(lin + i, raw[i]);
    }
}

} // namespace

void fpu::reset()
{
    enabled_ = true;
    busy_ = false;
    cw_ = k_fpu_cw_init;
    sw_ = 0;
    tag_ = k_fpu_tag_empty;
    unsigned i = 0;
    for (i = 0; i < 8u; i++)
    {
        stack_[i] = 0.0L;
    }
}

void fpu::wait()
{
    busy_ = false;
}

unsigned fpu::phys_reg(unsigned sti) const
{
    return (top() + (sti & 7u)) & 7u;
}

long double fpu::st_phys(unsigned p) const
{
    return stack_[p & 7u];
}

void fpu::set_st_phys(unsigned p, long double v)
{
    stack_[p & 7u] = v;
}

long double fpu::st(unsigned i) const
{
    if (i > 7u)
    {
        return 0.0L;
    }
    const unsigned p = phys_reg(i);
    const unsigned sh = (p * 2u) & 15u;
    if (((tag_ >> sh) & 3u) == 3u)
    {
        return 0.0L;
    }
    return st_phys(p);
}

void fpu::set_ex(uint16_t bit)
{
    sw_ = static_cast<uint16_t>(sw_ | bit);
}

void fpu::set_c0c2c3(int cmp)
{
    sw_ = static_cast<uint16_t>(sw_ & static_cast<uint16_t>(~(k_sw_c0 | k_sw_c2 | k_sw_c3)));
    if (cmp < 0)
    {
        sw_ = static_cast<uint16_t>(sw_ | k_sw_c0);
    }
    else if (cmp == 0)
    {
        sw_ = static_cast<uint16_t>(sw_ | k_sw_c3);
    }
}

void fpu::push(long double v)
{
    const unsigned t = (top() - 1u) & 7u;
    sw_ = static_cast<uint16_t>((sw_ & static_cast<uint16_t>(~0x3800u)) | static_cast<uint16_t>(t << 11));
    stack_[t] = v;
    const unsigned sh = (t * 2u) & 15u;
    tag_ = static_cast<uint16_t>(tag_ & static_cast<uint16_t>(~(3u << sh)));
}

long double fpu::pop()
{
    const unsigned t = top();
    const unsigned sh = (t * 2u) & 15u;
    long double v = 0.0L;
    if (((tag_ >> sh) & 3u) == 3u)
    {
        set_ex(k_sw_ie);
    }
    else
    {
        v = stack_[t];
    }
    tag_ = static_cast<uint16_t>(tag_ | static_cast<uint16_t>(3u << sh));
    const unsigned nt = (t + 1u) & 7u;
    sw_ = static_cast<uint16_t>((sw_ & static_cast<uint16_t>(~0x3800u)) | static_cast<uint16_t>(nt << 11));
    return v;
}

long double fpu::load_mem(cpu &c, const modrm &mr, unsigned bytes)
{
    const uint32_t lin = lin_of(mr);
    if (bytes == 4u)
    {
        return from_f32(load_u32(c, lin));
    }
    if (bytes == 8u)
    {
        return from_f64(load_u64(c, lin));
    }
    if (bytes == 10u)
    {
        return from_f80(c, lin);
    }
    if (bytes == 2u)
    {
        return static_cast<long double>(static_cast<int16_t>(load_u16(c, lin)));
    }
    if (bytes == 4u + 16u) /* unused */
    {
        return 0.0L;
    }
    return 0.0L;
}

void fpu::store_mem(cpu &c, const modrm &mr, unsigned bytes, long double v, bool do_pop)
{
    const uint32_t lin = lin_of(mr);
    if (bytes == 4u)
    {
        store_u32(c, lin, to_f32(v));
    }
    else if (bytes == 8u)
    {
        store_u64(c, lin, to_f64(v));
    }
    else if (bytes == 10u)
    {
        to_f80(c, lin, v);
    }
    else if (bytes == 2u)
    {
        store_u16(c, lin, static_cast<uint16_t>(static_cast<int16_t>(v)));
    }
    if (do_pop)
    {
        (void)pop();
    }
}

void fpu::esc(cpu &c, uint8_t opc, const modrm &mr)
{
    busy_ = false;
    if (!enabled_)
    {
        return;
    }
    const uint8_t op = static_cast<uint8_t>(opc & 7u);
    const bool mem = (mr.mod != 3u);
    const uint8_t r = mr.reg;
    const uint8_t rm = mr.rm;

    if ((opc == 0xDBu) && (mr.mod == 3u) && (mr.reg == 4u) && (mr.rm == 3u))
    {
        /* FNINIT DB E3 — keep enabled_ */
        const bool on = enabled_;
        reset();
        enabled_ = on;
        return;
    }

    /* D9 misc register */
    if ((opc == 0xD9u) && (mr.mod == 3u) && (mr.reg >= 4u))
    {
        const uint8_t m = static_cast<uint8_t>((mr.reg << 3) | mr.rm);
        if (m == 0x20u) /* E0 FCHS */
        {
            set_st_phys(phys_reg(0), -st(0));
            return;
        }
        if (m == 0x21u) /* E1 FABS */
        {
            set_st_phys(phys_reg(0), std::fabsl(st(0)));
            return;
        }
        if (m == 0x28u) /* E8 FLD1 */
        {
            push(1.0L);
            return;
        }
        if (m == 0x2Bu) /* EB FLDPI */
        {
            push(3.14159265358979323846L);
            return;
        }
        if (m == 0x2Eu) /* EE FLDZ */
        {
            push(0.0L);
            return;
        }
        if (m == 0x3Au) /* FA FSQRT */
        {
            const long double x = st(0);
            if (x < 0.0L)
            {
                set_ex(k_sw_ie);
            }
            else
            {
                set_st_phys(phys_reg(0), std::sqrtl(x));
            }
            return;
        }
        set_ex(k_sw_ie);
        return;
    }

    if ((opc == 0xD9u) && (mr.mod == 3u) && (mr.reg == 0u))
    {
        push(st(rm)); /* FLD ST(i) */
        return;
    }
    if ((opc == 0xD9u) && (mr.mod == 3u) && (mr.reg == 1u))
    {
        /* FXCH ST(i) */
        const unsigned a = phys_reg(0);
        const unsigned b = phys_reg(rm);
        const long double t = stack_[a];
        stack_[a] = stack_[b];
        stack_[b] = t;
        return;
    }

    if (!mem)
    {
        /* D8 ST,ST(i) arithmetic */
        if (opc == 0xD8u)
        {
            long double a = st(0);
            long double b = st(rm);
            if (r == 0)
            {
                set_st_phys(phys_reg(0), a + b);
            }
            else if (r == 1)
            {
                set_st_phys(phys_reg(0), a * b);
            }
            else if (r == 2)
            {
                set_c0c2c3((a < b) ? -1 : (a > b) ? 1 : 0);
            }
            else if (r == 3)
            {
                set_c0c2c3((a < b) ? -1 : (a > b) ? 1 : 0);
                (void)pop();
            }
            else if (r == 4)
            {
                set_st_phys(phys_reg(0), a - b);
            }
            else if (r == 5)
            {
                set_st_phys(phys_reg(0), b - a);
            }
            else if (r == 6)
            {
                set_st_phys(phys_reg(0), (b == 0.0L) ? a : (a / b));
            }
            else if (r == 7)
            {
                set_st_phys(phys_reg(0), (a == 0.0L) ? b : (b / a));
            }
            return;
        }
        if ((opc == 0xDDu) && (r == 2u))
        {
            set_st_phys(phys_reg(rm), st(0)); /* FST ST(i) */
            return;
        }
        if ((opc == 0xDDu) && (r == 3u))
        {
            set_st_phys(phys_reg(rm), st(0));
            (void)pop();
            return;
        }
        set_ex(k_sw_ie);
        return;
    }

    /* memory */
    if (opc == 0xD9u)
    {
        if (r == 0)
        {
            push(load_mem(c, mr, 4));
        }
        else if (r == 2)
        {
            store_mem(c, mr, 4, st(0), false);
        }
        else if (r == 3)
        {
            store_mem(c, mr, 4, st(0), true);
        }
        else if (r == 5)
        {
            cw_ = load_u16(c, lin_of(mr)); /* FLDCW */
        }
        else if (r == 7)
        {
            store_u16(c, lin_of(mr), cw_); /* FSTCW */
        }
        else
        {
            set_ex(k_sw_ie);
        }
        return;
    }
    if (opc == 0xDDu)
    {
        if (r == 0)
        {
            push(load_mem(c, mr, 8));
        }
        else if (r == 2)
        {
            store_mem(c, mr, 8, st(0), false);
        }
        else if (r == 3)
        {
            store_mem(c, mr, 8, st(0), true);
        }
        else
        {
            set_ex(k_sw_ie);
        }
        return;
    }
    if (opc == 0xDBu)
    {
        if (r == 0)
        {
            push(static_cast<long double>(static_cast<int32_t>(load_u32(c, lin_of(mr)))));
        }
        else if (r == 3)
        {
            store_u32(c, lin_of(mr), static_cast<uint32_t>(static_cast<int32_t>(st(0))));
            (void)pop();
        }
        else if (r == 5)
        {
            push(load_mem(c, mr, 10));
        }
        else if (r == 7)
        {
            store_mem(c, mr, 10, st(0), true);
        }
        else
        {
            set_ex(k_sw_ie);
        }
        return;
    }
    if (opc == 0xDFu)
    {
        if (r == 0)
        {
            push(static_cast<long double>(static_cast<int16_t>(load_u16(c, lin_of(mr)))));
        }
        else if (r == 3)
        {
            store_u16(c, lin_of(mr), static_cast<uint16_t>(static_cast<int16_t>(st(0))));
            (void)pop();
        }
        else if (r == 7)
        {
            store_u16(c, lin_of(mr), sw_); /* FSTSW m16 */
        }
        else
        {
            set_ex(k_sw_ie);
        }
        return;
    }
    if (opc == 0xD8u)
    {
        const long double m = load_mem(c, mr, 4);
        const long double a = st(0);
        if (r == 0)
        {
            set_st_phys(phys_reg(0), a + m);
        }
        else if (r == 1)
        {
            set_st_phys(phys_reg(0), a * m);
        }
        else if (r == 2)
        {
            set_c0c2c3((a < m) ? -1 : (a > m) ? 1 : 0);
        }
        else if (r == 3)
        {
            set_c0c2c3((a < m) ? -1 : (a > m) ? 1 : 0);
            (void)pop();
        }
        else if (r == 4)
        {
            set_st_phys(phys_reg(0), a - m);
        }
        else if (r == 5)
        {
            set_st_phys(phys_reg(0), m - a);
        }
        else if (r == 6)
        {
            set_st_phys(phys_reg(0), (m == 0.0L) ? a : (a / m));
        }
        else if (r == 7)
        {
            set_st_phys(phys_reg(0), (a == 0.0L) ? m : (m / a));
        }
        return;
    }
    (void)op;
    set_ex(k_sw_ie);
}

} // namespace iron86
