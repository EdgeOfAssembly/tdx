/**
 * @file test_bios.cpp
 * @brief CGA 3DAh retrace toggle and INT 21 AH=29 FCB parse.
 */

#include "dos/dos_fcb.h"
#include "dos/ibm_bda.h"
#include "dos/mz_parse.h"
#include "rex/rex.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

TEST_CASE("CGA port 3DAh toggles so wait-retrace loops can exit")
{
    rex_session *s = rex_session_create();
    REQUIRE(rex_session_load(s, "tests/fixtures/cga3da.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_exit_code(s) == 0);
    rex_session_destroy(s);
}

TEST_CASE("INT 1A AH=00 returns BDA 0040:006C ticks")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/int1a.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_exit_code(s) == 0);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.dx == 0x1234);
    REQUIRE(r.cx == 0x0005);
    rex_session_destroy(s);
}

TEST_CASE("OUT 3D9h writes CGA color-select to BDA 0040:0066")
{
    rex_session *s = rex_session_create();
    uint8_t pal = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/out3d9.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_read_mem(s, 0x466ull, &pal, 1) == REX_OK);
    REQUIRE(pal == 0x35);
    rex_session_destroy(s);
}

TEST_CASE("INT 21 AH=29 fills FCB name FOO.BAR")
{
    rex_session *s = rex_session_create();
    uint8_t fcb[12] = {};
    REQUIRE(rex_session_load(s, "tests/fixtures/parsefcb.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    /* COM CS=1000h, fcb label after 'FOO.BAR',0 at IP 0x113+8 = 0x11B. */
    REQUIRE(rex_session_read_mem(s, 0x1011Bull, fcb, 12) == REX_OK);
    REQUIRE(fcb[0] == 0);
    REQUIRE(fcb[1] == (uint8_t)'F');
    REQUIRE(fcb[2] == (uint8_t)'O');
    REQUIRE(fcb[3] == (uint8_t)'O');
    REQUIRE(fcb[4] == (uint8_t)' ');
    REQUIRE(fcb[9] == (uint8_t)'B');
    REQUIRE(fcb[10] == (uint8_t)'A');
    REQUIRE(fcb[11] == (uint8_t)'R');
    rex_session_destroy(s);
}

TEST_CASE("packed DOS FCB / IBM BDA / MZ on-disk sizes")
{
    REQUIRE(sizeof(dos_fcb) == 37u);
    REQUIRE(offsetof(dos_fcb, recsiz) == 0x0Eu);
    REQUIRE(offsetof(dos_fcb, nr) == 0x20u);
    REQUIRE(offsetof(ibm_bda, mem_kb) == 0x13u);
    REQUIRE(offsetof(ibm_bda, video_mode) == 0x49u);
    REQUIRE(offsetof(ibm_bda, crt_palette) == 0x66u);
    REQUIRE(offsetof(ibm_bda, timer_ticks) == 0x6Cu);
    REQUIRE(sizeof(mz_exe_hdr) == 28u);
    REQUIRE(sizeof(mz_reloc) == 4u);
}
