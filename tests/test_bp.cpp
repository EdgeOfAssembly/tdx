/**
 * @file test_bp.cpp
 * @brief Execution breakpoints on tiny.com.
 */

#include "rex/rex.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("breakpoint at second insn stops before executing it")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    rex_insn ins[4];
    size_t n = 0;
    uint32_t id = 0;
    uint64_t lin = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/tiny.com", nullptr) == REX_OK);
    rex_session_disasm(s, UINT64_MAX, ins, 4, &n);
    REQUIRE(n >= 2);
    lin = ins[1].linear;
    REQUIRE(rex_bp_add_linear(s, lin, &id) == REX_OK);
    REQUIRE(rex_bp_count(s) == 1);
    REQUIRE(rex_session_run(s, 1000) == REX_OK);
    {
        const rex_stop st = rex_session_stop_reason(s);
        rex_session_get_regs_i8086(s, &r);
        const uint64_t ip_lin = rex_segoff_to_linear(r.cs, r.ip);
        rex_session_destroy(s);
        s = nullptr;
        REQUIRE(st == REX_STOP_BREAK);
        REQUIRE(ip_lin == lin);
        REQUIRE(r.ax == 1); /* MOV AX,1 executed; INC not yet */
    }
}

TEST_CASE("bp segoff is preserved in rex_bp_list")
{
    rex_session *s = rex_session_create();
    rex_bp b[4]{};
    uint32_t id = 0;
    size_t n = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/tiny.com", nullptr) == REX_OK);
    REQUIRE(rex_bp_add_segoff(s, 0x1000, 0x0103, &id) == REX_OK);
    n = rex_bp_list(s, b, 4);
    REQUIRE(n == 1);
    REQUIRE(b[0].id == id);
    REQUIRE(b[0].seg == 0x1000);
    REQUIRE(b[0].off == 0x0103);
    rex_session_destroy(s);
}

TEST_CASE("range BP stops on first insn in [lo,hi] inclusive")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    rex_insn ins[4];
    rex_range_bp rb[4]{};
    size_t n = 0;
    uint32_t id = 0;
    uint64_t lo = 0;
    uint64_t hi = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/tiny.com", nullptr) == REX_OK);
    rex_session_disasm(s, UINT64_MAX, ins, 4, &n);
    REQUIRE(n >= 3);
    lo = ins[1].linear;
    hi = ins[2].linear;
    REQUIRE(rex_bp_add_segoff_range(s, ins[1].seg, ins[1].off, ins[2].seg, ins[2].off, 0, &id) ==
            REX_OK);
    REQUIRE(rex_range_bp_list(s, rb, 4) == 1);
    REQUIRE(rb[0].id == id);
    REQUIRE(rb[0].lo == lo);
    REQUIRE(rb[0].hi == hi);
    REQUIRE(rex_bp_at(s, lo));
    REQUIRE(rex_session_run(s, 1000) == REX_OK);
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_BREAK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(rex_segoff_to_linear(r.cs, r.ip) == lo);
    REQUIRE(r.ax == 1); /* MOV AX,1 done; INC in range not executed */
    rex_session_destroy(s);
}

TEST_CASE("range BP once auto-clears after first hit")
{
    rex_session *s = rex_session_create();
    rex_insn ins[4];
    rex_range_bp rb[4]{};
    size_t n = 0;
    uint32_t id = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/tiny.com", nullptr) == REX_OK);
    rex_session_disasm(s, UINT64_MAX, ins, 4, &n);
    REQUIRE(n >= 2);
    REQUIRE(rex_bp_add_range(s, ins[1].linear, ins[1].linear, 1, &id) == REX_OK);
    REQUIRE(rex_session_run(s, 1000) == REX_OK);
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_BREAK);
    REQUIRE(rex_range_bp_list(s, rb, 4) == 0);
    REQUIRE(rex_session_run(s, 1000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    rex_session_destroy(s);
}

TEST_CASE("BPM write-watch stops on guest store to DS:0200")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    rex_range_bp mb[4]{};
    uint32_t id = 0;
    uint8_t cell = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/memw.com", nullptr) == REX_OK);
    REQUIRE(rex_bp_add_segoff_write(s, 0x1000, 0x0200, 0x1000, 0x0200, 0, &id) == REX_OK);
    REQUIRE(rex_mem_bp_list(s, mb, 4) == 1);
    REQUIRE(mb[0].id == id);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_BREAK);
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.cs == 0x1000);
    REQUIRE(rex_session_read_mem(s, rex_segoff_to_linear(0x1000, 0x0200), &cell, 1) == REX_OK);
    REQUIRE(cell == 0xAA);
    rex_session_destroy(s);
}

TEST_CASE("BPM once auto-clears after the first guest write")
{
    rex_session *s = rex_session_create();
    rex_range_bp mb[4]{};
    uint32_t id = 0;
    REQUIRE(rex_session_load(s, "tests/fixtures/memw.com", nullptr) == REX_OK);
    REQUIRE(rex_bp_add_segoff_write(s, 0x1000, 0x0200, 0x1000, 0x0200, 1, &id) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_BREAK);
    REQUIRE(rex_mem_bp_list(s, mb, 4) == 0);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    rex_session_destroy(s);
}

TEST_CASE("rex_version is 0.11")
{
    REQUIRE(std::string(rex_version()) == "0.11");
}

TEST_CASE("F9 run delay nudges in 5ms steps and will not go below 0")
{
    rex_session *s = rex_session_create();
    REQUIRE(rex_session_run_delay_ms(s) == 0);
    rex_session_set_run_delay_ms(s, 5);
    REQUIRE(rex_session_run_delay_ms(s) == 5);
    REQUIRE(rex_session_nudge_run_delay(s, 1) == 10);
    REQUIRE(rex_session_nudge_run_delay(s, -1) == 5);
    REQUIRE(rex_session_nudge_run_delay(s, -1) == 0);
    REQUIRE(rex_session_nudge_run_delay(s, -1) == 0);
    rex_session_set_run_delay_ms(s, 9999);
    REQUIRE(rex_session_run_delay_ms(s) == 200);
    REQUIRE(rex_session_nudge_run_delay(s, 1) == 200);
    rex_session_set_run_delay_ms(s, 3);
    REQUIRE(rex_session_nudge_run_delay(s, -1) == 0);
    rex_session_destroy(s);
}
