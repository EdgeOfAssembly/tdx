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
    REQUIRE(std::string(TDX_VERSION_STRING) == "0.27");
}

TEST_CASE("prefetch-bytes default 6 (8086); 0 is off")
{
    auto a = parse({"game.exe"});
    REQUIRE(a.prefetch_bytes == 6u);
    auto b = parse({"--prefetch-bytes", "0", "game.exe"});
    REQUIRE(b.prefetch_bytes == 0u);
    auto c = parse({"game.exe", "--prefetch-bytes=6"});
    REQUIRE(c.prefetch_bytes == 6u);
    auto d = parse({"--prefetch-bytes", "2000", "--no-ui"});
    REQUIRE(d.prefetch_bytes == 1024u);
    REQUIRE(d.no_ui);
}

TEST_CASE("prefetch-bytes missing value is usage error")
{
    char *argv[] = {const_cast<char *>("tdx"), const_cast<char *>("--prefetch-bytes")};
    tdx_cli c{};
    REQUIRE_FALSE(tdx_cli_parse(2, argv, &c));
    REQUIRE(c.usage_error);
}

TEST_CASE("bios is default with floppy-a; --no-bios opts out")
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
    auto e = parse({"--floppy-a", "a.img"});
    REQUIRE_FALSE(e.bios.empty());
    REQUIRE_FALSE(e.no_bios);
    auto f = parse({"--floppy-a", "a.img", "--no-bios"});
    REQUIRE(f.no_bios);
    REQUIRE(f.bios.empty());
}

TEST_CASE("floppy is opt-in iron86 boot")
{
    auto a = parse({"game.exe"});
    REQUIRE(a.floppy.empty());
    auto b = parse({"--floppy", "disk.img", "--no-bios"});
    REQUIRE(b.floppy == "disk.img");
    REQUIRE(b.bios.empty());
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
    auto d = parse({"--floppy", "a.img", "--no-bios"});
    REQUIRE(d.floppy == "a.img");
    REQUIRE(d.floppy_b.empty());
    REQUIRE(d.bios.empty());
}

TEST_CASE("in-process game window is opt-in")
{
    auto a = parse({"game.exe"});
    REQUIRE_FALSE(a.game);
    auto b = parse({"--game", "game.exe"});
    REQUIRE(b.game);
}

TEST_CASE("fast-post is default; --no-fast-post is cold POST")
{
    auto a = parse({"--bios", "rom.bin"});
    REQUIRE_FALSE(a.no_fast_post);
    auto b = parse({"--bios", "rom.bin", "--no-fast-post"});
    REQUIRE(b.no_fast_post);
    REQUIRE(b.bios == "rom.bin");
    auto c = parse({"--no-fast-post", "--floppy-a", "a.img"});
    REQUIRE(c.no_fast_post);
}

TEST_CASE("preset ibm-pc/5150 vs ibm-xt/5160; --hdc implies xt")
{
    auto a = parse({"--preset", "ibm-pc", "--floppy-a", "a.img"});
    REQUIRE(a.preset == iron86::machine_preset::ibm_pc);
    REQUIRE(a.preset_set);
    REQUIRE(a.prefetch_bytes == 6u);
    auto b = parse({"--preset=5160", "--floppy-a", "a.img"});
    REQUIRE(b.preset == iron86::machine_preset::ibm_xt);
    auto c = parse({"--preset", "5150", "game.exe"});
    REQUIRE(c.preset == iron86::machine_preset::ibm_pc);
    auto d = parse({"--hdc", "disk.img", "--bios", "rom.bin"});
    REQUIRE(d.preset == iron86::machine_preset::ibm_xt);
    REQUIRE(d.preset_set);
    auto e = parse({"--preset", "ibm-xt", "--prefetch-bytes", "4", "--floppy-a", "a.img"});
    REQUIRE(e.prefetch_bytes == 4u);
    REQUIRE(e.prefetch_set);
}

TEST_CASE("hdc is opt-in and order-independent")
{
    auto a = parse({"--bios", "rom.bin"});
    REQUIRE(a.hdc.empty());
    auto b = parse({"--bios", "rom.bin", "--hdc", "disk.img", "--hdc-rom", "ros.bin"});
    REQUIRE(b.hdc == "disk.img");
    REQUIRE(b.hdc_rom == "ros.bin");
    auto c = parse({"--hdc=disk.img", "--bios=rom.bin", "--no-ui"});
    REQUIRE(c.hdc == "disk.img");
    REQUIRE(c.bios == "rom.bin");
}

TEST_CASE("mda is opt-in CGA default")
{
    auto a = parse({"--bios", "rom.bin"});
    REQUIRE_FALSE(a.mda);
    auto b = parse({"--bios", "rom.bin", "--mda"});
    REQUIRE(b.mda);
    REQUIRE(b.bios == "rom.bin");
}

TEST_CASE("no-8087 is opt-out; NPX default on")
{
    auto a = parse({"--bios", "rom.bin"});
    REQUIRE_FALSE(a.no_8087);
    auto b = parse({"--bios", "rom.bin", "--no-8087"});
    REQUIRE(b.no_8087);
    REQUIRE(b.bios == "rom.bin");
    auto c = parse({"--no-8087", "--floppy-a", "a.img"});
    REQUIRE(c.no_8087);
}
