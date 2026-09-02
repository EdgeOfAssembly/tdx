/**
 * @file test_crtc.cpp
 * @brief Packed MC6845: sizeof 29, MDA/CGA 3BA/3DA bits, unprogrammed POST.
 */
#include "iron86/crtc.h"

#include <cstdint>
#include <cstdio>

static int g_fail = 0;

static void expect(bool ok, const char *msg)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL %s\n", msg);
        g_fail = 1;
    }
}

static void expect_eq(uint16_t got, uint16_t want, const char *msg)
{
    if (got != want)
    {
        std::fprintf(stderr, "FAIL %s got=%04X want=%04X\n", msg, got, want);
        g_fail = 1;
    }
}

static void wr_reg(iron86::mc6845 *c, uint8_t idx, uint8_t v)
{
    iron86::crtc_write_index(c, idx);
    iron86::crtc_write_data(c, v);
}

/** @brief IBM MDA 80×25 CRTC subset used by the unit tests (R4 left 0). */
static void program_ibm_mda_regs(iron86::mc6845 *c)
{
    iron86::crtc_write_ctrl(c, 0x09);
    wr_reg(c, 0, 0x61);
    wr_reg(c, 1, 0x50);
    wr_reg(c, 2, 0x52);
    wr_reg(c, 3, 0x0F);
    wr_reg(c, 6, 0x19);
    wr_reg(c, 9, 0x0D);
}

/**
 * @brief Poll status until bit0 and bit3 have each been 0 and 1.
 * @return true if all four states appeared within @p max_reads.
 */
static bool seen_bit0_bit3(iron86::mc6845 *c, int max_reads)
{
    bool b0_lo = false;
    bool b0_hi = false;
    bool b3_lo = false;
    bool b3_hi = false;
    for (int i = 0; i < max_reads; i++)
    {
        const uint8_t s = iron86::crtc_read_status(c);
        if ((s & 1u) == 0)
        {
            b0_lo = true;
        }
        else
        {
            b0_hi = true;
        }
        if ((s & 8u) == 0)
        {
            b3_lo = true;
        }
        else
        {
            b3_hi = true;
        }
        if (b0_lo && b0_hi && b3_lo && b3_hi)
        {
            return true;
        }
    }
    return false;
}

int main()
{
    expect(sizeof(iron86::mc6845) == 29u, "sizeof mc6845");

    {
        iron86::mc6845 c{};
        iron86::crtc_reset(&c, 1);
        expect_eq(c.is_mda, 1, "reset is_mda");
        expect_eq(c.primed, 0, "reset primed");
        expect_eq(c.lpt_stat, 0xDF, "reset lpt_stat");
        expect_eq(c.regs[0], 0, "reset R0");
        expect_eq(c.ctrl, 0, "reset ctrl");
        iron86::crtc_write_ctrl(&c, 0x00); /* not 01h: must not hang */
        expect_eq(c.primed, 0, "ctrl 00 primed");
        iron86::crtc_write_ctrl(&c, 0x09);
        expect_eq(c.primed, 1, "ctrl 09 primed");
        expect_eq(iron86::crtc_read_ctrl(&c), 0x09, "read ctrl");
    }

    {
        iron86::mc6845 c{};
        iron86::crtc_reset(&c, 0);
        iron86::crtc_write_ctrl(&c, 0x09);
        expect_eq(c.primed, 0, "cga ctrl no primed");
        iron86::crtc_write_index(&c, 0x25);
        expect_eq(c.index, 0x05, "index 5 bits");
        wr_reg(&c, 16, 0x12);
        wr_reg(&c, 17, 0x34);
        iron86::crtc_write_index(&c, 16);
        expect_eq(iron86::crtc_read_data(&c), 0x12, "R16 stored");
        iron86::crtc_write_index(&c, 17);
        expect_eq(iron86::crtc_read_data(&c), 0x34, "R17 stored");
        wr_reg(&c, 12, 0x01);
        wr_reg(&c, 13, 0x23);
        wr_reg(&c, 14, 0x04);
        wr_reg(&c, 15, 0x56);
        expect_eq(iron86::crtc_start(&c), 0x0123, "start R12/R13");
        expect_eq(iron86::crtc_cursor(&c), 0x0456, "cursor R14/R15");
        iron86::crtc_write_index(&c, 18);
        iron86::crtc_write_data(&c, 0xFF);
        expect_eq(c.regs[0], 0, "R18 ignored");
    }

    {
        iron86::mc6845 c{};
        iron86::crtc_reset(&c, 1);
        program_ibm_mda_regs(&c);
        expect(seen_bit0_bit3(&c, 4096), "mda programmed bit0/bit3");
    }

    {
        iron86::mc6845 c{};
        iron86::crtc_reset(&c, 0);
        program_ibm_mda_regs(&c);
        /* CGA vsync (bit3) waits until v_row >= R6: 25 rows × 14 scans × 98. */
        expect(seen_bit0_bit3(&c, 100000), "cga programmed bit0/bit3");
    }

    {
        iron86::mc6845 mda{};
        iron86::mc6845 cga{};
        iron86::crtc_reset(&mda, 1);
        iron86::crtc_reset(&cga, 0);
        expect(seen_bit0_bit3(&mda, 16), "mda unprogrammed R0=0 toggle");
        expect(seen_bit0_bit3(&cga, 16), "cga unprogrammed R0=0 toggle");
    }

    if (g_fail != 0)
    {
        return 1;
    }
    std::puts("iron86 crtc tests ok");
    return 0;
}
