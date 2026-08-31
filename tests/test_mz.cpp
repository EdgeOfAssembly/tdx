/**
 * @file test_mz.cpp
 * @brief MZ header parser tests.
 */

#include "dos/mz_parse.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

TEST_CASE("mz_parse rejects short buffer")
{
    uint8_t buf[8]{};
    mz_info info{};
    REQUIRE(mz_parse_header(buf, sizeof(buf), 8, &info) == 1);
}

TEST_CASE("mz_parse COM-looking bytes are not MZ")
{
    uint8_t buf[32];
    mz_info info{};
    std::memset(buf, 0x90, sizeof(buf));
    REQUIRE(mz_parse_header(buf, sizeof(buf), 32, &info) == 1);
    REQUIRE(info.is_mz == 0);
}

TEST_CASE("mz_parse reads a minimal MZ header")
{
    uint8_t buf[32]{};
    mz_info info{};
    /* MZ, last=0, pages=1, relocs=0, header_paras=2 (32 bytes), min=0 max=ffff
       ss=0 sp=0 checksum=0 ip=0 cs=0 reloc_off=28 overlay=0 */
    buf[0] = 'M';
    buf[1] = 'Z';
    buf[4] = 1; /* pages */
    buf[8] = 2; /* header paras */
    buf[12] = 0xFF;
    buf[13] = 0xFF;
    buf[24] = 28;
    REQUIRE(mz_parse_header(buf, sizeof(buf), 32, &info) == 0);
    REQUIRE(info.is_mz == 1);
    REQUIRE(info.header_paras == 2);
    REQUIRE(info.header_bytes == 32);
}

TEST_CASE("mz_parse_relocs reads one entry")
{
    std::vector<uint8_t> file(36, 0);
    mz_info info{};
    mz_reloc rel[4]{};
    size_t n = 0;
    file[0] = 'M';
    file[1] = 'Z';
    file[4] = 1;
    file[6] = 1; /* reloc count */
    file[8] = 2;
    file[12] = 0xFF;
    file[13] = 0xFF;
    file[24] = 28;
    file[28] = 0x10; /* off */
    file[29] = 0x00;
    file[30] = 0x01; /* seg */
    file[31] = 0x00;
    REQUIRE(mz_parse_header(file.data(), file.size(), (uint32_t)file.size(), &info) == 0);
    REQUIRE(mz_parse_relocs(file.data(), file.size(), &info, rel, 4, &n) == 0);
    REQUIRE(n == 1);
    REQUIRE(rel[0].off == 0x10);
    REQUIRE(rel[0].seg == 0x01);
}
