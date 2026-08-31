/**
 * @file test_step.cpp
 * @brief Step, step-over CALL, step-over LOOP on fixture COMs.
 */

#include "rex/rex.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

static void step_until_halt(rex_session *s, int max_steps)
{
    int i = 0;
    for (i = 0; (i < max_steps) && (!rex_session_halted(s)); i++)
    {
        REQUIRE(rex_session_step(s) == REX_OK);
    }
    REQUIRE(rex_session_halted(s));
}

TEST_CASE("PSP word at offset 2 is 640K conventional end")
{
    rex_session *s = rex_session_create();
    uint8_t b[2] = {0, 0};
    uint16_t end = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/tiny.com", nullptr) == REX_OK);
    REQUIRE(rex_session_read_mem(s, 0x10002ull, b, 2) == REX_OK);
    end = (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
    REQUIRE(end == 0xA000);
    rex_session_destroy(s);
}

TEST_CASE("far.com run executes lcall helper then INT20")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/far.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ax == 0xAABB);
    REQUIRE(rex_session_halted(s));
    rex_session_destroy(s);
}

TEST_CASE("reset after halt restores entry and clears halted")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/tiny.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 1000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_reset(s) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ip == 0x0100);
    REQUIRE(r.ax == 0);
    REQUIRE_FALSE(rex_session_halted(s));
    rex_session_destroy(s);
}

TEST_CASE("reset rewinds tiny.com to entry")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/tiny.com", nullptr) == REX_OK);
    REQUIRE(rex_session_step(s) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ax == 1);
    REQUIRE(rex_session_reset(s) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ip == 0x0100);
    REQUIRE(r.ax == 0);
    REQUIRE_FALSE(rex_session_halted(s));
    rex_session_destroy(s);
}

TEST_CASE("INT3 padding does not stop run")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/int3pad.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 1000) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(rex_session_halted(s));
    REQUIRE(r.ax == 1);
    rex_session_destroy(s);
}

TEST_CASE("INT16 waitkey consumes pushed key as exit code")
{
    rex_session *s = rex_session_create();
    REQUIRE(rex_session_load(s, "tests/fixtures/waitkey.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 1000) == REX_OK);
    REQUIRE_FALSE(rex_session_halted(s));
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_WAIT_KEY);
    REQUIRE(rex_session_push_key(s, (uint8_t)'Q', 0x10) == REX_OK);
    REQUIRE(rex_session_run(s, 1000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_exit_code(s) == (int)'Q');
    rex_session_destroy(s);
}

TEST_CASE("INT21 FCB open TINY.COM succeeds")
{
    rex_session *s = rex_session_create();
    REQUIRE(rex_session_load(s, "tests/fixtures/fcbopen.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 1000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_exit_code(s) == 0);
    rex_session_destroy(s);
}

TEST_CASE("INT21 AH=4A BX=FFFF reports max and sets CF")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/setblock.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 1000) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE((r.flags & 1u) != 0); /* CF */
    REQUIRE(r.bx != 0xFFFF);
    REQUIRE(r.bx > 0);
    rex_session_destroy(s);
}

TEST_CASE("tiny.com steps to AX=4 then INT20 halt")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(s != nullptr);
    REQUIRE(rex_session_load(s, "tests/fixtures/tiny.com", nullptr) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ip == 0x0100);
    step_until_halt(s, 16);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ax == 4);
    rex_session_destroy(s);
}

TEST_CASE("over.com F8 on CALL skips helper body")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    rex_insn ins{};
    size_t n = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/over.com", nullptr) == REX_OK);
    REQUIRE(rex_session_step(s) == REX_OK); /* xor ax,ax */
    rex_session_disasm(s, UINT64_MAX, &ins, 1, &n);
    REQUIRE(n == 1);
    REQUIRE(ins.is_call);
    REQUIRE(rex_session_step_over(s, 0) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ax == 2);
    REQUIRE_FALSE(rex_session_halted(s));
    rex_session_destroy(s);
}

TEST_CASE("far.com F7 into lcall lands at helper offset 0 not linear low bits")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/far.com", nullptr) == REX_OK);
    REQUIRE(rex_session_step(s) == REX_OK); /* xor ax,ax */
    REQUIRE(rex_session_step(s) == REX_OK); /* lcall */
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.cs == 0x1000);
    REQUIRE(r.ip >= 0x0100);
    REQUIRE(r.ip < 0x0180);
    REQUIRE(rex_session_step(s) == REX_OK); /* mov ax, AABB */
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ax == 0xAABB);
    rex_session_destroy(s);
}

TEST_CASE("loop.com F8 on LOOP runs remaining iterations once")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    rex_insn ins{};
    size_t n = 0;
    int i = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/loop.com", nullptr) == REX_OK);
    REQUIRE(rex_session_step(s) == REX_OK); /* xor */
    REQUIRE(rex_session_step(s) == REX_OK); /* mov cx,5 */
    REQUIRE(rex_session_step(s) == REX_OK); /* inc ax */
    rex_session_disasm(s, UINT64_MAX, &ins, 1, &n);
    REQUIRE(ins.is_loop);
    REQUIRE(rex_session_step_over(s, 0) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ax == 5);
    REQUIRE(r.cx == 0);
    (void)i;
    rex_session_destroy(s);
}
