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
    REQUIRE(std::string(TDX_VERSION_STRING) == "0.6");
}

TEST_CASE("in-process game window is opt-in")
{
    auto a = parse({"game.exe"});
    REQUIRE_FALSE(a.game);
    auto b = parse({"--game", "game.exe"});
    REQUIRE(b.game);
}
