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

TEST_CASE("rex_version is 0.6")
{
    REQUIRE(std::string(rex_version()) == "0.6");
}
