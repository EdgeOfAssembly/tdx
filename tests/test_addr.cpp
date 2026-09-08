/**
 * @file test_addr.cpp
 * @brief rex_parse_addr: seg:off, linear hex, canonical split for setip.
 */

#include "rex/rex_addr.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("parse seg:off")
{
    uint64_t lin = 0;
    uint16_t seg = 0;
    uint16_t off = 0;
    REQUIRE(rex_parse_addr("0FDB:A360", &lin, &seg, &off));
    REQUIRE(seg == 0x0FDB);
    REQUIRE(off == 0xA360);
    REQUIRE(lin == rex_segoff_to_linear(0x0FDB, 0xA360));
}

TEST_CASE("parse linear hex fills canonical CS:IP")
{
    uint64_t lin = 0;
    uint16_t seg = 0;
    uint16_t off = 0;
    REQUIRE(rex_parse_addr("0xFE199", &lin, &seg, &off));
    REQUIRE(lin == 0xFE199u);
    REQUIRE(seg == 0xFE19);
    REQUIRE(off == 0x0009);
    REQUIRE(rex_segoff_to_linear(seg, off) == 0xFE199u);
}

TEST_CASE("parse linear FE199h and $FE199")
{
    uint64_t lin = 0;
    uint16_t seg = 0;
    uint16_t off = 0;
    REQUIRE(rex_parse_addr("FE199h", &lin, &seg, &off));
    REQUIRE(lin == 0xFE199u);
    REQUIRE(rex_parse_addr("$FE199", &lin, &seg, &off));
    REQUIRE(lin == 0xFE199u);
    REQUIRE(rex_parse_addr("linear FE199", &lin, &seg, &off));
    REQUIRE(lin == 0xFE199u);
}

TEST_CASE("parse empty fails")
{
    uint64_t lin = 0;
    REQUIRE_FALSE(rex_parse_addr("", &lin, nullptr, nullptr));
    REQUIRE_FALSE(rex_parse_addr("not-an-addr", &lin, nullptr, nullptr));
}
