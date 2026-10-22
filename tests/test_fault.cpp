/**
 * @file test_fault.cpp
 * @brief F1 is a NOP on iron86 (not Unicorn #UD). INT 20 still terminates.
 */

#include "rex/rex.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>

TEST_CASE("F1 NOP then INT 20 halts; IP is not stuck on ICEBP")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    rex_insn ins[8];
    size_t n = 0;
    std::memset(ins, 0, sizeof(ins));
    /* ud.com is NOP, F1, INT 20. iron86 executes F1 as NOP; it must not
     * report REX_ERR_CPU or freeze the IP on the F1 byte. */
    REQUIRE(rex_session_load(s, "tests/fixtures/ud.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_OK);
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_HALTED);
    REQUIRE(rex_session_stop_reason(s) != REX_STOP_FAULT);
    REQUIRE(rex_session_halted(s));
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ip != 0x0101);
    REQUIRE(rex_session_disasm(s, 0x10101ull, ins, 8, &n) == REX_OK);
    REQUIRE(n > 0);
    REQUIRE(ins[0].size >= 1);
    REQUIRE(ins[0].bytes[0] == 0xF1);
    rex_session_destroy(s);
}
