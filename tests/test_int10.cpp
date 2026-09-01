/**
 * @file test_int10.cpp
 * @brief INT 10 text writes and BPINT 10 (stop before handler).
 */

#include "rex/rex.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("INT 10 AH=09 writes char+attr at B800:0000")
{
    rex_session *s = rex_session_create();
    uint8_t cell[2] = {0, 0};
    uint8_t rest[2] = {0, 0};
    REQUIRE(rex_session_load(s, "tests/fixtures/int10.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_video_mode(s) == 0x03);
    REQUIRE(rex_session_read_mem(s, 0xB8000ull, cell, 2) == REX_OK);
    REQUIRE(cell[0] == (uint8_t)'A');
    REQUIRE(cell[1] == 0x07);
    REQUIRE(rex_session_read_mem(s, 0xB8000ull + 10ull * 2ull, rest, 2) == REX_OK);
    REQUIRE(rest[0] == (uint8_t)' ');
    REQUIRE(rest[1] == 0x07);
    rex_session_destroy(s);
}

TEST_CASE("BPINT 10 stops on the CD 10 before the handler")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/int10.com", nullptr) == REX_OK);
    REQUIRE(rex_bp_int(s, 0x10) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_BREAK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ip == 0x0103);
    REQUIRE(r.ax == 0x0003);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_BREAK);
    REQUIRE(r.ip == 0x010B);
    rex_session_destroy(s);
}
