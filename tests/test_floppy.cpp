/**
 * @file test_floppy.cpp
 * @brief Opt-in iron86 floppy boot inside rex_session (Unicorn path untouched).
 */
#include "rex/rex.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <fstream>
#include <string>

TEST_CASE("iron86 floppy boot reaches FloppyOS banner")
{
    const char *img = "subprojects/floppyos/build/floppyos.img";
    {
        std::ifstream in(img, std::ios::binary);
        if (!in)
        {
            SKIP("FloppyOS image not built");
        }
    }
    rex_session *s = rex_session_create();
    REQUIRE(s != nullptr);
    REQUIRE(rex_session_load_floppy(s, img) == REX_OK);
    rex_regs_i8086 r{};
    rex_session_get_regs_i8086(s, &r);
    REQUIRE(r.cs == 0);
    REQUIRE(r.ip == 0x7C00);
    REQUIRE(rex_session_run(s, 200000) == REX_OK);
    const char *con = rex_session_con_out(s);
    REQUIRE(con != nullptr);
    const std::string t(con);
    REQUIRE(t.find("FloppyOS") != std::string::npos);
    rex_session_destroy(s);
}

TEST_CASE("attach floppy B after A")
{
    const char *img = "subprojects/floppyos/build/floppyos.img";
    {
        std::ifstream in(img, std::ios::binary);
        if (!in)
        {
            SKIP("FloppyOS image not built");
        }
    }
    rex_session *s = rex_session_create();
    REQUIRE(s != nullptr);
    REQUIRE(rex_session_load_floppy(s, img) == REX_OK);
    REQUIRE(rex_session_attach_floppy_b(s, img) == REX_OK);
    rex_session_destroy(s);
}

TEST_CASE("BIOS 40:6C advances while COMMAND waits for a key")
{
    const char *img = "subprojects/floppyos/build/floppyos.img";
    const char *rom = "/mnt/TurboDebugger/Py86/ROM/IBM/PC/5150/"
                      "BIOS_IBM5150_24APR81_5700051_U33.BIN";
    {
        std::ifstream in(img, std::ios::binary);
        if (!in)
        {
            SKIP("FloppyOS image not built");
        }
    }
    {
        std::ifstream in(rom, std::ios::binary);
        if (!in)
        {
            SKIP("5150 BIOS ROM not mounted");
        }
    }
    rex_session *s = rex_session_create();
    REQUIRE(s != nullptr);
    REQUIRE(rex_session_load_bios(s, rom) == REX_OK);
    REQUIRE(rex_session_attach_floppy(s, img) == REX_OK);
    REQUIRE(rex_session_run(s, 4000000ull) == REX_OK);
    /* ROM INT 10h writes B800, not iron86 tty_. */
    bool saw_prompt = false;
    unsigned row = 0;
    for (row = 0; row < 25u; row++)
    {
        char line[81];
        unsigned col = 0;
        for (col = 0; col < 80u; col++)
        {
            uint8_t ch = 0;
            const uint64_t at = 0xB8000ull + (static_cast<uint64_t>(row) * 80u + col) * 2u;
            REQUIRE(rex_session_read_mem(s, at, &ch, 1) == REX_OK);
            line[col] = ((ch >= 32u) && (ch < 127u)) ? static_cast<char>(ch) : ' ';
        }
        line[80] = '\0';
        if (std::string(line).find("A>") != std::string::npos)
        {
            saw_prompt = true;
            break;
        }
    }
    uint8_t ticks0[4] = {0, 0, 0, 0};
    uint8_t ticks1[4] = {0, 0, 0, 0};
    REQUIRE(rex_session_read_mem(s, 0x46Cull, ticks0, 4) == REX_OK);
    /* reload-0 needs ~131k insns/IRQ0; also wall_clock_irq0 once per run. */
    REQUIRE(rex_session_run(s, 200000ull) == REX_OK);
    REQUIRE(rex_session_read_mem(s, 0x46Cull, ticks1, 4) == REX_OK);
    const uint32_t t0 = (uint32_t)ticks0[0] | ((uint32_t)ticks0[1] << 8) |
                        ((uint32_t)ticks0[2] << 16) | ((uint32_t)ticks0[3] << 24);
    const uint32_t t1 = (uint32_t)ticks1[0] | ((uint32_t)ticks1[1] << 8) |
                        ((uint32_t)ticks1[2] << 16) | ((uint32_t)ticks1[3] << 24);
    rex_session_destroy(s);
    s = nullptr;
    REQUIRE(saw_prompt);
    REQUIRE(t1 > t0);
}

TEST_CASE("attach floppy B packs a host directory")
{
    const char *img = "subprojects/floppyos/build/floppyos.img";
    const char *dir = "/mnt/bushido/bushido";
    {
        std::ifstream in(img, std::ios::binary);
        if (!in)
        {
            SKIP("FloppyOS image not built");
        }
    }
    {
        std::ifstream exe(std::string(dir) + "/BUSHIDO.EXE", std::ios::binary);
        if (!exe)
        {
            SKIP("Bushido directory not mounted");
        }
    }
    rex_session *s = rex_session_create();
    REQUIRE(s != nullptr);
    REQUIRE(rex_session_load_floppy(s, img) == REX_OK);
    REQUIRE(rex_session_attach_floppy_b(s, dir) == REX_OK);
    rex_session_destroy(s);
}
