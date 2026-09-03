/**
 * @file test_cga.cpp
 * @brief CGA pack/unpack round-trip.
 */

#include "dos/dos_cga.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <fstream>
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

TEST_CASE("CGA 3D9h 30h is black/cyan/magenta/white")
{
    uint32_t pal[4] = {1, 1, 1, 1};
    dos_cga_palette_argb(0x30, pal);
    REQUIRE(pal[0] == 0xFF000000u);
    REQUIRE(pal[1] == 0xFF55FFFFu);
    REQUIRE(pal[2] == 0xFFFF55FFu);
    REQUIRE(pal[3] == 0xFFFFFFFFu);
}

TEST_CASE("CGA 3D9h background nibble paints index 0")
{
    uint32_t pal[4] = {};
    dos_cga_palette_argb(0x35, pal); /* intensity + pal1 + magenta bg */
    REQUIRE(pal[0] == 0xFFA800A8u);
    REQUIRE(pal[1] == 0xFF54FCFCu);
    REQUIRE(pal[2] == 0xFFFC54FCu);
}

TEST_CASE("mode 6 1bpp decode even/odd banks")
{
    std::vector<uint8_t> vram((size_t)DOS_CGA_VRAM, 0);
    std::vector<uint8_t> px((size_t)DOS_CGA_HIRES_PIXELS, 0xFF);
    vram[0] = 0x80;          /* even y=0 x=0 */
    vram[0x2000] = 0x01;     /* odd  y=1 x=7 */
    REQUIRE(dos_cga_decode_hires(vram.data(), px.data(), px.size()) == 0);
    REQUIRE(px[0] == 1);
    REQUIRE(px[1] == 0);
    REQUIRE(px[(size_t)DOS_CGA_HIRES_WIDTH + 7u] == 1);
    REQUIRE(dos_cga_decode_hires(nullptr, px.data(), px.size()) == -1);
}

TEST_CASE("mode 6 composite empty VRAM is dark")
{
    std::vector<uint8_t> vram((size_t)DOS_CGA_VRAM, 0);
    std::vector<uint32_t> argb((size_t)DOS_CGA_HIRES_PIXELS, 1);
    REQUIRE(dos_cga_composite_argb(vram.data(), argb.data(), argb.size(), 0x1E, 0x00) == 0);
    REQUIRE((argb[100] & 0xFFu) < 48u);
    REQUIRE(((argb[100] >> 16) & 0xFFu) < 48u);
    REQUIRE(dos_cga_composite_argb(nullptr, argb.data(), argb.size(), 0x1E, 0x0F) == -1);
}

TEST_CASE("mode 4 composite Dragon Wars gold dump is blue dragon red warrior")
{
    std::ifstream in("games/SCREEN.CGA", std::ios::binary);
    if (!in)
    {
        SKIP("games/SCREEN.CGA (DOSBox gold dump) not present");
    }
    std::vector<uint8_t> vram((size_t)DOS_CGA_VRAM, 0);
    in.read(reinterpret_cast<char *>(vram.data()), (std::streamsize)vram.size());
    REQUIRE(in.gcount() >= 8000);
    std::vector<uint32_t> argb((size_t)DOS_CGA_PIXELS, 0);
    REQUIRE(dos_cga_composite_argb320(vram.data(), argb.data(), argb.size(), 0x2A, 0x30) == 0);
    auto mean = [&](int x0, int x1, int y0, int y1) {
        unsigned r = 0, g = 0, b = 0, n = 0;
        for (int y = y0; y < y1; y++)
        {
            for (int x = x0; x < x1; x++)
            {
                const uint32_t p = argb[(size_t)y * 320u + (size_t)x];
                r += (p >> 16) & 255u;
                g += (p >> 8) & 255u;
                b += p & 255u;
                n++;
            }
        }
        return std::array<unsigned, 3>{r / n, g / n, b / n};
    };
    const auto dragon = mean(40, 120, 10, 80);
    const auto warrior = mean(160, 250, 50, 140);
    REQUIRE(dragon[2] > dragon[0]);  /* B > R */
    REQUIRE(warrior[0] > warrior[2]); /* R > B */
    REQUIRE(dos_cga_composite_argb320(nullptr, argb.data(), argb.size(), 0x2A, 0x30) == -1);
}
