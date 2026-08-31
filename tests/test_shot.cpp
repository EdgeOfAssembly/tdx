/**
 * @file test_shot.cpp
 * @brief Xmux-style versioned screenshot path.
 */

#include "tdx/tdx_shot.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <regex>
#include <string>
#include <unistd.h>

TEST_CASE("versioned path inserts timestamp before extension")
{
    const std::string p = tdx_shot_versioned_path("/tmp/tdx-cpu.bmp");
    const std::regex re(R"(/tmp/tdx-cpu-\d{8}T\d{6}\.\d{3}\.bmp)");
    REQUIRE(std::regex_match(p, re));
}

TEST_CASE("empty path defaults under /tmp")
{
    const std::string p = tdx_shot_versioned_path("");
    REQUIRE(p.find("/tmp/") == 0);
    REQUIRE(p.find(".bmp") != std::string::npos);
}

TEST_CASE("existing file is not overwritten")
{
    const std::string first = tdx_shot_versioned_path("/tmp/tdx-coll.bmp");
    FILE *fp = std::fopen(first.c_str(), "wb");
    REQUIRE(fp != nullptr);
    std::fclose(fp);
    const std::string second = tdx_shot_versioned_path("/tmp/tdx-coll.bmp");
    unlink(first.c_str());
    REQUIRE(second != first);
}
