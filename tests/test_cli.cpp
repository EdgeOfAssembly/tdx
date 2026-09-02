/**
 * @file test_cli.cpp
 * @brief CLI contract: help/version flags, order independence.
 */

#include "tdx/tdx_cli.h"
#include "tdx/tdx_version.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

static tdx_cli parse(std::vector<const char *> args)
{
    tdx_cli c{};
    args.insert(args.begin(), "tdx");
    REQUIRE(tdx_cli_parse((int)args.size(), const_cast<char **>(args.data()), &c));
    return c;
}

TEST_CASE("no args is help")
{
    char *argv[] = {const_cast<char *>("tdx"), nullptr};
    tdx_cli c{};
    REQUIRE(tdx_cli_parse(1, argv, &c));
    REQUIRE(c.help);
}

TEST_CASE("version flags")
{
    auto a = parse({"-v"});
    REQUIRE(a.version);
    auto b = parse({"--version"});
    REQUIRE(b.version);
}

TEST_CASE("options and input may be interleaved")
{
    auto a = parse({"--no-ui", "game.exe", "--no-sock"});
    REQUIRE(a.no_ui);
    REQUIRE(a.no_sock);
    REQUIRE(a.input == "game.exe");
    auto b = parse({"game.exe", "--no-ui"});
    REQUIRE(b.no_ui);
    REQUIRE(b.input == "game.exe");
}

TEST_CASE("version string matches header")
{
    REQUIRE(std::string(TDX_VERSION_STRING) == "0.12");
}

TEST_CASE("bios is opt-in iron86 5150 POST")
{
    auto a = parse({"game.exe"});
    REQUIRE(a.bios.empty());
    auto b = parse({"--bios", "rom.bin"});
    REQUIRE(b.bios == "rom.bin");
    auto c = parse({"--bios=rom.bin", "--no-ui"});
    REQUIRE(c.bios == "rom.bin");
    REQUIRE(c.no_ui);
    auto d = parse({"--bios", "rom.bin", "--floppy", "disk.img"});
    REQUIRE(d.bios == "rom.bin");
    REQUIRE(d.floppy == "disk.img");
}

TEST_CASE("floppy is opt-in iron86 boot")
{
    auto a = parse({"game.exe"});
    REQUIRE(a.floppy.empty());
    auto b = parse({"--floppy", "disk.img"});
    REQUIRE(b.floppy == "disk.img");
    auto c = parse({"--floppy=disk.img", "--no-ui"});
    REQUIRE(c.floppy == "disk.img");
    REQUIRE(c.no_ui);
    auto d = parse({"--uc-floppy", "disk.img"});
    REQUIRE(d.uc_floppy == "disk.img");
    REQUIRE(d.floppy.empty());
}

TEST_CASE("floppy-a and floppy-b")
{
    auto a = parse({"--floppy-a", "a.img", "--floppy-b", "b.img"});
    REQUIRE(a.floppy == "a.img");
    REQUIRE(a.floppy_b == "b.img");
    auto b = parse({"--floppy-a=a.img", "--floppy-b=b.img", "--no-ui"});
    REQUIRE(b.floppy == "a.img");
    REQUIRE(b.floppy_b == "b.img");
    REQUIRE(b.no_ui);
    auto c = parse({"--bios", "rom.bin", "--floppy-b", "/mnt/bushido/bushido"});
    REQUIRE(c.bios == "rom.bin");
    REQUIRE(c.floppy.empty());
    REQUIRE(c.floppy_b == "/mnt/bushido/bushido");
    auto d = parse({"--floppy", "a.img"});
    REQUIRE(d.floppy == "a.img");
    REQUIRE(d.floppy_b.empty());
}

TEST_CASE("in-process game window is opt-in")
{
    auto a = parse({"game.exe"});
    REQUIRE_FALSE(a.game);
    auto b = parse({"--game", "game.exe"});
    REQUIRE(b.game);
}
