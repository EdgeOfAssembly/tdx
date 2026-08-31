/**
 * @file test_cga.cpp
 * @brief CGA pack/unpack round-trip.
 */

#include "dos/dos_cga.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

TEST_CASE("cga encode/decode round trip")
{
    std::vector<uint8_t> px((size_t)DOS_CGA_PIXELS);
    std::vector<uint8_t> vram((size_t)DOS_CGA_VRAM);
    std::vector<uint8_t> out((size_t)DOS_CGA_PIXELS);
    size_t i = 0;
    for (i = 0; i < px.size(); i++)
    {
        px[i] = (uint8_t)(i & 3u);
    }
    REQUIRE(dos_cga_encode(px.data(), px.size(), vram.data(), vram.size()) == 0);
    REQUIRE(dos_cga_decode(vram.data(), out.data(), out.size()) == 0);
    REQUIRE(out == px);
}

TEST_CASE("cga decode rejects null")
{
    uint8_t px[4]{};
    REQUIRE(dos_cga_decode(nullptr, px, sizeof(px)) == -1);
}
