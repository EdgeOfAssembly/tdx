/**
 * @file test_ibm_font.cpp
 * @brief IBM CRT_CHAR_GEN 8×8 detector (no BIOS blob in-tree).
 */
#include "tdx/tdx_ibm_font.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

TEST_CASE("IBM CGA 8x8 signature is char 0x01 smile")
{
    std::vector<uint8_t> blob(1024, 0);
    const uint8_t smile[8] = {0x7E, 0x81, 0xA5, 0x81, 0xBD, 0x99, 0x81, 0x7E};
    std::memcpy(blob.data() + 8, smile, 8);
    REQUIRE(tdx_ibm_font_looks_cga8(blob.data(), blob.size()) == 1);
    blob[0] = 1;
    REQUIRE(tdx_ibm_font_looks_cga8(blob.data(), blob.size()) == 0);
    REQUIRE(tdx_ibm_font_looks_cga8(blob.data(), 16) == 0);
}
