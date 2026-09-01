/**
 * @file test_fault.cpp
 * @brief Invalid opcode stops as FAULT (not halted) and listing stays non-empty.
 */

#include "rex/rex.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>

TEST_CASE("invalid F1 ICEBP is a CPU fault, not terminated")
{
    rex_session *s = rex_session_create();
    rex_regs_i8086 r{};
    rex_insn ins[8];
    size_t n = 0;
    std::memset(ins, 0, sizeof(ins));
    REQUIRE(rex_session_load(s, "tests/fixtures/ud.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 10000) == REX_ERR_CPU);
    REQUIRE(rex_session_stop_reason(s) == REX_STOP_FAULT);
    REQUIRE(!rex_session_halted(s));
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.ip == 0x0101);
    REQUIRE(rex_session_disasm(s, UINT64_MAX, ins, 8, &n) == REX_OK);
    REQUIRE(n > 0);
    REQUIRE(ins[0].size >= 1);
    REQUIRE(ins[0].bytes[0] == 0xF1);
    rex_session_destroy(s);
}
