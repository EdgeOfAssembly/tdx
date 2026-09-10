/**
 * @file test_fpu.cpp
 * @brief 8087 NPX: FNINIT, FLD/FSTP, FADD, FSQRT, FSTCW, --no-8087.
 */
#include "iron86/cpu.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

int g_fail = 0;

void expect(bool c, const char *msg)
{
    if (!c)
    {
        std::fprintf(stderr, "FAIL %s\n", msg);
        g_fail++;
    }
}

void expect_eq(uint16_t a, uint16_t b, const char *msg)
{
    if (a != b)
    {
        std::fprintf(stderr, "FAIL %s got %04x want %04x\n", msg, a, b);
        g_fail++;
    }
}

void run(iron86::cpu &c, int n = 32)
{
    int i = 0;
    for (i = 0; i < n; i++)
    {
        if (!c.step())
        {
            break;
        }
        if (c.halted())
        {
            break;
        }
    }
}

} // namespace

int main()
{
    {
        iron86::cpu c;
        expect(c.npx().enabled(), "npx default on");
        expect_eq(c.npx().control(), iron86::k_fpu_cw_init, "cw init");
    }
    {
        iron86::cpu c;
        const uint8_t prog[] = {0xDB, 0xE3, 0xF4}; /* FNINIT */
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect(c.halted(), "fninit hlt");
        expect_eq(c.npx().control(), iron86::k_fpu_cw_init, "fninit cw");
    }
    {
        iron86::cpu c;
        const uint8_t prog[] = {0xD9, 0xE8, 0xD9, 0xFA, 0xF4}; /* FLD1 FSQRT */
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect(c.halted(), "fld1 sqrt hlt");
        expect(std::fabsl(c.npx().st(0) - 1.0L) < 1.0e-6L, "sqrt 1");
    }
    {
        iron86::cpu c;
        float four = 4.0f;
        uint32_t bits = 0;
        std::memcpy(&bits, &four, 4);
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0200), static_cast<uint16_t>(bits));
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0202), static_cast<uint16_t>(bits >> 16));
        const uint8_t prog[] = {
            0xDB, 0xE3,                   /* FNINIT */
            0xD9, 0x06, 0x00, 0x02,       /* FLD dword [0200] */
            0xD9, 0xFA,                   /* FSQRT */
            0xD9, 0x1E, 0x04, 0x02,       /* FSTP dword [0204] */
            0xF4,
        };
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        const uint32_t out = static_cast<uint32_t>(c.mem_read16(iron86::cpu::phys(0x1000, 0x0204))) |
                             (static_cast<uint32_t>(c.mem_read16(iron86::cpu::phys(0x1000, 0x0206)))
                              << 16);
        float got = 0.0f;
        std::memcpy(&got, &out, 4);
        expect(c.halted(), "sqrt4 hlt");
        expect(std::fabs(got - 2.0f) < 1.0e-5f, "sqrt 4 = 2");
    }
    {
        iron86::cpu c;
        float a = 1.5f;
        float b = 2.5f;
        uint32_t ba = 0;
        uint32_t bb = 0;
        std::memcpy(&ba, &a, 4);
        std::memcpy(&bb, &b, 4);
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0200), static_cast<uint16_t>(ba));
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0202), static_cast<uint16_t>(ba >> 16));
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0204), static_cast<uint16_t>(bb));
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0206), static_cast<uint16_t>(bb >> 16));
        const uint8_t prog[] = {
            0xDB, 0xE3,
            0xD9, 0x06, 0x00, 0x02, /* FLD a */
            0xD8, 0x06, 0x04, 0x02, /* FADD b */
            0xD9, 0x1E, 0x08, 0x02, /* FSTP */
            0xF4,
        };
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        const uint32_t out = static_cast<uint32_t>(c.mem_read16(iron86::cpu::phys(0x1000, 0x0208))) |
                             (static_cast<uint32_t>(c.mem_read16(iron86::cpu::phys(0x1000, 0x020A)))
                              << 16);
        float got = 0.0f;
        std::memcpy(&got, &out, 4);
        expect(std::fabs(got - 4.0f) < 1.0e-5f, "1.5+2.5=4");
    }
    {
        iron86::cpu c;
        const uint8_t prog[] = {0xDB, 0xE3, 0xD9, 0x3E, 0x00, 0x02, 0xF4}; /* FSTCW [0200] */
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.mem_read16(iron86::cpu::phys(0x1000, 0x0200)), iron86::k_fpu_cw_init, "fstcw");
    }
    {
        iron86::cpu c;
        c.set_npx(false);
        expect(!c.npx().enabled(), "npx off");
        const uint8_t prog[] = {0xD9, 0xE8, 0xF4}; /* FLD1 ignored */
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect(std::fabsl(c.npx().st(0)) < 1.0e-12L, "disabled no fld1");
    }
    if (g_fail != 0)
    {
        std::fprintf(stderr, "%d failed\n", g_fail);
        return 1;
    }
    std::puts("ok");
    return 0;
}
