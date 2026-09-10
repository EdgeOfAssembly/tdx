/**
 * @file test_config.cpp
 * @brief INI parse, preset names, CLI-over-INI merge.
 */
#include "iron86/config.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static int g_fail = 0;

static void expect(bool ok, const char *msg)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL %s\n", msg);
        g_fail = 1;
    }
}

static void expect_eq_u(unsigned got, unsigned want, const char *msg)
{
    if (got != want)
    {
        std::fprintf(stderr, "FAIL %s got=%u want=%u\n", msg, got, want);
        g_fail = 1;
    }
}

static void expect_streq(const std::string &got, const char *want, const char *msg)
{
    if (got != want)
    {
        std::fprintf(stderr, "FAIL %s got='%s' want='%s'\n", msg, got.c_str(), want);
        g_fail = 1;
    }
}

int main()
{
    {
        iron86::machine_preset p = iron86::machine_preset::ibm_at;
        expect(iron86::parse_preset_name("ibm-pc", &p) == 0, "parse ibm-pc");
        expect(p == iron86::machine_preset::ibm_pc, "id ibm-pc");
        expect(iron86::parse_preset_name("IBM-XT", &p) == 0, "parse ibm-xt");
        expect(p == iron86::machine_preset::ibm_xt, "id ibm-xt");
        expect(iron86::parse_preset_name("5150", &p) == 0, "parse 5150");
        expect(p == iron86::machine_preset::ibm_pc, "id 5150");
        expect(iron86::parse_preset_name("5160", &p) == 0, "parse 5160");
        expect(p == iron86::machine_preset::ibm_xt, "id 5160");
        expect(iron86::parse_preset_name("ibm-5150", &p) == 0, "parse ibm-5150");
        expect(iron86::parse_preset_name("ibm-5160", &p) == 0, "parse ibm-5160");
        expect(iron86::parse_preset_name("ibm-at", &p) == 1, "parse ibm-at reserved");
        expect(iron86::preset_is_reserved(p), "ibm-at reserved");
        expect(iron86::parse_preset_name("8286", &p) == 1, "parse 8286 reserved");
        expect(std::strcmp(iron86::machine_preset_name(p), "8286") == 0, "name 8286");
        expect(iron86::parse_preset_name("nope", &p) == -1, "unknown preset");
        expect(iron86::parse_preset_name(nullptr, &p) == -1, "null preset");
        {
            iron86::machine_config cfg{};
            iron86::apply_preset(iron86::machine_preset::ibm_xt, &cfg);
            expect_eq_u(cfg.prefetch_bytes, 6u, "xt preset prefetch 8086");
            iron86::machine_config pc{};
            iron86::apply_preset(iron86::machine_preset::ibm_pc, &pc);
            expect_eq_u(pc.prefetch_bytes, 6u, "pc preset prefetch 8086");
        }
    }
    {
        iron86::machine_config cfg{};
        std::string err;
        const char *ini =
            "[machine]\n"
            "preset = ibm-xt ; xt + xebec\n"
            "\n"
            "[bios]\n"
            "rom = /tmp/IBM5150.BIN\n"
            "\n"
            "[floppy]\n"
            "a = disk.img\n"
            "b =\n"
            "\n"
            "[hdc]\n"
            "image = hdc.img\n"
            "; ibm-pc/xt: ST-412-ish\n"
            "cylinders = 306\n"
            "heads = 4\n"
            "spt = 17\n"
            "\n"
            "[cpu]\n"
            "prefetch_bytes = 6\n"
            "unknown_key = ignored\n";
        expect(iron86::parse_ini_text(ini, &cfg, &err), "parse goal ini");
        expect(cfg.preset == iron86::machine_preset::ibm_xt, "ini preset xt");
        expect_streq(cfg.bios_rom, "/tmp/IBM5150.BIN", "ini bios");
        expect_streq(cfg.floppy_a, "disk.img", "ini floppy a");
        expect(cfg.floppy_b.empty(), "ini floppy b empty");
        expect_streq(cfg.hdc_image, "hdc.img", "ini hdc");
        expect_eq_u(cfg.hdc_cylinders, 306u, "ini cyl");
        expect_eq_u(cfg.hdc_heads, 4u, "ini heads");
        expect_eq_u(cfg.hdc_spt, 17u, "ini spt");
        expect_eq_u(cfg.prefetch_bytes, 6u, "ini prefetch");
        expect(cfg.prefetch_set, "ini prefetch set");
    }
    {
        iron86::machine_config cfg{};
        std::string err;
        expect(!iron86::parse_ini_text("preset = ibm-pc\n", &cfg, &err), "key outside section");
        expect(!iron86::parse_ini_text("[machine]\npreset = z80\n", &cfg, &err), "bad preset");
        expect(!iron86::parse_ini_text("[hdc]\ncylinders = -1\n", &cfg, &err), "neg cyl");
        expect(!iron86::parse_ini_text("[hdc]\nheads = x\n", &cfg, &err), "bad heads");
        expect(!iron86::parse_ini_text("[]\n", &cfg, &err), "empty section");
    }
    {
        /* CLI overrides INI overrides preset. */
        const char *path = "/tmp/iron86-test.ini";
        {
            std::ofstream f(path);
            f << "[machine]\npreset = ibm-xt\n"
              << "[bios]\nrom = /ini/bios.bin\n"
              << "[floppy]\na = /ini/a.img\n"
              << "[cpu]\nprefetch_bytes = 6\n"
              << "[hdc]\nimage = /ini/hdc.img\ncylinders = 305\n";
        }
        iron86::machine_config cli{};
        cli.bios_rom = "/cli/bios.bin";
        cli.prefetch_bytes = 0;
        cli.prefetch_set = true;
        iron86::machine_config out{};
        std::string err;
        expect(iron86::merge_machine_config(path, cli, &out, &err), "merge cli over ini");
        expect(out.preset == iron86::machine_preset::ibm_xt, "merge keeps ini preset");
        expect_streq(out.bios_rom, "/cli/bios.bin", "cli bios wins");
        expect_streq(out.floppy_a, "/ini/a.img", "ini floppy kept");
        expect_streq(out.hdc_image, "/ini/hdc.img", "ini hdc kept");
        expect_eq_u(out.prefetch_bytes, 0u, "cli prefetch 0 wins");
        expect_eq_u(out.hdc_cylinders, 305u, "ini cyl kept");
    }
    {
        const char *path = "/tmp/iron86-test-at.ini";
        {
            std::ofstream f(path);
            f << "[machine]\npreset = ibm-at\n";
        }
        iron86::machine_config cli{};
        iron86::machine_config out{};
        std::string err;
        expect(!iron86::merge_machine_config(path, cli, &out, &err), "ini ibm-at fails");
        expect(err.find("not yet") != std::string::npos, "ini ibm-at not yet");
        cli.preset = iron86::machine_preset::ibm_pc;
        cli.preset_set = true;
        err.clear();
        expect(iron86::merge_machine_config(path, cli, &out, &err), "cli ibm-pc overrides ini at");
        expect(out.preset == iron86::machine_preset::ibm_pc, "cli preset pc");
    }
    {
        iron86::machine_config cli{};
        cli.preset = iron86::machine_preset::cpu_8286;
        cli.preset_set = true;
        iron86::machine_config out{};
        std::string err;
        expect(!iron86::merge_machine_config("/tmp/iron86-test.ini", cli, &out, &err),
               "cli 8286 fails");
        expect(err.find("not yet") != std::string::npos, "cli 8286 not yet");
    }
    {
        iron86::machine_config cli{};
        iron86::machine_config out{};
        std::string err;
        expect(!iron86::merge_machine_config("/no/such/iron86.ini", cli, &out, &err),
               "missing --config");
        expect(err.find("cannot read") != std::string::npos, "missing file err");
    }
    {
        iron86::machine_config cli{};
        iron86::machine_config out{};
        std::string err;
        expect(iron86::merge_machine_config("/tmp/iron86-test.ini", cli, &out, &err),
               "default-like merge");
        expect(out.preset == iron86::machine_preset::ibm_xt, "file preset");
        expect_eq_u(out.prefetch_bytes, 6u, "file prefetch");
    }
    {
        /* cwd default iron86.ini when config_path is NULL. */
        char old[512];
        expect(getcwd(old, sizeof(old)) != nullptr, "getcwd");
        const char *dir = "/tmp/iron86-ini-cwd";
        (void)mkdir(dir, 0755);
        expect(chdir(dir) == 0, "chdir tmp");
        {
            std::ofstream f("iron86.ini");
            f << "[floppy]\na = cwd.img\n[cpu]\nprefetch_bytes = 4\n";
        }
        iron86::machine_config cli{};
        iron86::machine_config out{};
        std::string err;
        expect(iron86::merge_machine_config(nullptr, cli, &out, &err), "cwd ini");
        expect_streq(out.floppy_a, "cwd.img", "cwd floppy");
        expect(out.preset == iron86::machine_preset::ibm_pc, "cwd default ibm-pc");
        expect(chdir(old) == 0, "chdir back");
    }
    {
        iron86::machine_config cfg{};
        std::string err;
        const char *quoted = "[bios]\nrom = \"/tmp/path with space.bin\"\n";
        expect(iron86::parse_ini_text(quoted, &cfg, &err), "quoted path");
        expect_streq(cfg.bios_rom, "/tmp/path with space.bin", "quoted value");
    }

    if (g_fail != 0)
    {
        return 1;
    }
    std::puts("iron86 config tests ok");
    return 0;
}
