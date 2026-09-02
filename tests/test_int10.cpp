/**
 * @file test_int10.cpp
 * @brief INT 10 text writes and BPINT 10 (stop before handler).
 */

#include "dos/dos_cga.h"
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

TEST_CASE("INT 10 mode 04 AH=0F reports 40 columns")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/int10m4.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_video_mode(s) == 0x04);
    rex_session_get_regs_i8086(s, &r);
    /* AH = column count (40 decimal = 28h), AL = mode. */
    REQUIRE(r.ax == 0x2804);
    rex_session_destroy(s);
}

TEST_CASE("blank regen is spaces+07 and CGA decode succeeds")
{
    rex_session *s = rex_session_create();
    uint8_t cell[2] = {0, 0};
    uint8_t px[DOS_CGA_PIXELS];
    REQUIRE(rex_session_load(s, "tests/fixtures/tiny.com", nullptr) == REX_OK);
    REQUIRE(rex_session_read_mem(s, 0xB8000ull, cell, 2) == REX_OK);
    REQUIRE(cell[0] == (uint8_t)' ');
    REQUIRE(cell[1] == 0x07);
    REQUIRE(rex_session_cga_decode(s, px, sizeof(px)) == REX_OK);
    rex_session_destroy(s);
}

TEST_CASE("INT 10 AH=0B sets BDA CRT_PALETTE 0040:0066")
{
    rex_session *s = rex_session_create();
    uint8_t pal = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/int10pal.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_read_mem(s, 0x466ull, &pal, 1) == REX_OK);
    /* Mode 4 default 30h, AH=0B BL=0Dh → 3Dh. */
    REQUIRE(pal == 0x3D);
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

TEST_CASE("BPINT 10 once stops only on the first INT 10")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    rex_int_bp ib[4]{};
    REQUIRE(rex_session_load(s, "tests/fixtures/int10.com", nullptr) == REX_OK);
    REQUIRE(rex_bp_int_hits(s, 0x10, 1) == REX_OK);
    REQUIRE(rex_int_bp_list(s, ib, 4) == 1);
    REQUIRE(ib[0].intno == 0x10);
    REQUIRE(ib[0].remain == 1);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_BREAK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ip == 0x0103);
    REQUIRE(rex_int_bp_list(s, ib, 4) == 0);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    rex_session_destroy(s);
}

TEST_CASE("bpinsn int 10 once matches Capstone text and auto-clears")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    rex_insn_bp ib[4]{};
    uint32_t id = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/int10.com", nullptr) == REX_OK);
    REQUIRE(rex_bp_insn(s, "int 10", 1, &id) == REX_OK);
    REQUIRE(id != 0);
    REQUIRE(rex_insn_bp_list(s, ib, 4) == 1);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_BREAK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ip == 0x0103);
    REQUIRE(rex_insn_bp_list(s, ib, 4) == 0);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    rex_session_destroy(s);
}
