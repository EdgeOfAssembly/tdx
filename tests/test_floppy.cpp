/**
 * @file test_floppy.cpp
 * @brief Opt-in iron86 floppy boot inside rex_session (Unicorn path untouched).
 */
#include "rex/rex.h"

#include <catch2/catch_test_macros.hpp>

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
