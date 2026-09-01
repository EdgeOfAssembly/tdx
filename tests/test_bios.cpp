/**
 * @file test_bios.cpp
 * @brief CGA 3DAh retrace toggle and INT 21 AH=29 FCB parse.
 */

#include "rex/rex.h"

#include <catch2/catch_test_macros.hpp>

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
