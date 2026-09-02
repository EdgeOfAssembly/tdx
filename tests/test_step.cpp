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

TEST_CASE("VCR back undoes mov ax,1 on tiny.com")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/tiny.com", nullptr) == REX_OK);
    REQUIRE(rex_session_vcr_forward(s, true) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ax == 1);
    REQUIRE(rex_session_vcr_back(s) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ax == 0);
    REQUIRE(r.ip == 0x0100);
    REQUIRE(rex_session_vcr_forward(s, true) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ax == 1);
    rex_session_destroy(s);
}

TEST_CASE("VCR over CALL then home returns to entry")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    REQUIRE(rex_session_load(s, "tests/fixtures/over.com", nullptr) == REX_OK);
    REQUIRE(rex_session_vcr_forward(s, true) == REX_OK); /* xor ax,ax */
    REQUIRE(rex_session_vcr_forward(s, false) == REX_OK); /* call helper — over */
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ax == 2);
    REQUIRE(rex_session_vcr_home(s) == REX_OK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ip == 0x0100);
    REQUIRE(r.ax == 0);
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

TEST_CASE("INT21 FCB open zeros leftover seq pointers then AH=14 reads byte 0")
{
    rex_session *s = rex_session_create();
    REQUIRE(rex_session_load(s, "tests/fixtures/fcbread.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_exit_code(s) == 0);
    rex_session_destroy(s);
}

TEST_CASE("INT21 FCB open always sets recsize 128 (MS-DOS 1.25)")
{
    rex_session *s = rex_session_create();
    REQUIRE(rex_session_load(s, "tests/fixtures/fcbrecsz.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_exit_code(s) == 0);
    rex_session_destroy(s);
}

TEST_CASE("INT21 AH=21 does not bump RR; AH=27 does")
{
    rex_session *s = rex_session_create();
    REQUIRE(rex_session_load(s, "tests/fixtures/fcbah21.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
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

TEST_CASE("int16spin.com INT16 AH=01/00 spin survives repeated run/reset/keys")
{
    /* Regression: the original Bushido crash was an ASan SEGV inside
     * handle_int16 (dos_int.cpp) after the program looped on INT 16h.
     * Drive the wait/check-key paths across resets and key injections;
     * the machine must stay alive and CS/IP must stay in the fixture. */
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    int round = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/int16spin.com", nullptr) == REX_OK);
    for (round = 0; round < 8; round++)
    {
        /* Run a burst; the fixture blocks on INT16 AH=00 (wait_key). */
        (void)rex_session_run(s, 4000);
        (void)rex_session_request_stop(s);
        rex_session_get_regs_i8086(s, &r);
        REQUIRE(r.cs == 0x1000);           /* COM image base segment */
        REQUIRE(r.ip >= 0x0100);
        REQUIRE(r.ip < 0x0120);            /* tight spin loop window */
        /* Inject two keys (consumed by AH=01 peek + AH=00 read). */
        rex_session_push_key(s, 'x', 0x2D);
        rex_session_push_key(s, 'a', 0x1E);
        /* Reload in place (Ctrl-F2 / tdxctl reset). */
        REQUIRE(rex_session_reset(s) == REX_OK);
        rex_session_get_regs_i8086(s, &r);
        REQUIRE(r.cs == 0x1000);
        REQUIRE(r.ip == 0x0100);           /* back at entry after reset */
    }
    rex_session_destroy(s);
}

TEST_CASE("int16spin.com disasm buffer is not overrun at cap boundary")
{
    /* Regression: requesting more insns than remain in a tiny image must stop
     * at the image edge, never write past the caller's buffer. */
    rex_session *s = rex_session_create();
    rex_insn ins[8];
    size_t n = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/int16spin.com", nullptr) == REX_OK);
    REQUIRE(rex_session_disasm(s, UINT64_MAX, ins, 8, &n) == REX_OK);
    REQUIRE(n <= 8);                        /* never past the caller's cap */
    REQUIRE(n > 0);
    rex_session_destroy(s);
}
