/**
 * @file main.cpp
 * @brief iron86 CLI — .COM until HLT, or --floppy IMAGE boot at 0000:7C00.
 */
#include "iron86/cpu.h"
#include "iron86/pc.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static const char *k_version = "0.9";

static void usage(FILE *out)
{
    std::fputs(
        "Usage: iron86 [options] [file.com]\n"
        "       iron86 --bios BIOS.BIN [--floppy IMAGE] [--keys STRING]\n"
        "       iron86 --floppy IMAGE [--keys STRING]\n"
        "\n"
        "  Chip-level 8086 (C++23, Py86 port). Hardware only — no DOS.\n"
        "  .COM loads at 1000:0100. --floppy without --bios: 0000:7C00.\n"
        "  --bios: IBM 5150 8K at FE000, reset FFFF:0000 (Py86 load_bios_5150_8k).\n"
        "  PPI/PIT/PIC/DMA/FDC (PyFloppy uPD765) for 1981 POST + INT 19h.\n"
        "  Fast-post (BDA 1234h) is default. PC speaker BEL is default.\n"
        "  Options and operands may be interleaved.\n"
        "\n"
        "Options:\n"
        "  -h, --help         Show this help and exit\n"
        "  -v, --version      Show version and exit\n"
        "  --bios FILE        Load 5150 8K BIOS (default: none)\n"
        "  --no-fast-post     Cold POST (do not set RESET_FLAG=1234h)\n"
        "  --no-audio         Disable PC speaker BEL (default: on)\n"
        "  --floppy IMAGE     360K (or larger) floppy image\n"
        "  --keys STRING      Type STRING as INT 16h keys (default: none)\n"
        "\n",
        out);
    std::fprintf(out, "iron86 %s\n", k_version);
}

static std::vector<uint8_t> slurp(const char *path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return {};
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

int main(int argc, char **argv)
{
    const char *path = nullptr;
    const char *floppy = nullptr;
    const char *keys = nullptr;
    const char *bios = nullptr;
    bool no_fast_post = false;
    bool no_audio = false;
    int i = 1;
    for (i = 1; i < argc; i++)
    {
        const char *a = argv[i];
        if ((std::strcmp(a, "-h") == 0) || (std::strcmp(a, "--help") == 0))
        {
            usage(stdout);
            return 0;
        }
        if ((std::strcmp(a, "-v") == 0) || (std::strcmp(a, "--version") == 0))
        {
            std::printf("iron86 %s\n", k_version);
            return 0;
        }
        if (std::strcmp(a, "--floppy") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fputs("iron86: --floppy needs IMAGE\n", stderr);
                return 2;
            }
            floppy = argv[++i];
            continue;
        }
        if (std::strcmp(a, "--bios") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fputs("iron86: --bios needs FILE\n", stderr);
                return 2;
            }
            bios = argv[++i];
            continue;
        }
        if (std::strcmp(a, "--no-fast-post") == 0)
        {
            no_fast_post = true;
            continue;
        }
        if (std::strcmp(a, "--no-audio") == 0)
        {
            no_audio = true;
            continue;
        }
        if (std::strcmp(a, "--keys") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fputs("iron86: --keys needs STRING\n", stderr);
                return 2;
            }
            keys = argv[++i];
            continue;
        }
        if (a[0] == '-')
        {
            std::fprintf(stderr, "iron86: unknown option %s\n", a);
            usage(stderr);
            return 2;
        }
        path = a;
    }
    if ((path == nullptr) && (floppy == nullptr) && (bios == nullptr))
    {
        usage(stdout);
        return 0;
    }

    if (bios != nullptr)
    {
        const std::vector<uint8_t> rom = slurp(bios);
        if (rom.empty())
        {
            std::fprintf(stderr, "iron86: cannot read BIOS %s\n", bios);
            return 1;
        }
        iron86::pc p;
        if (no_audio)
        {
            p.enable_audio(false);
        }
        if (floppy != nullptr)
        {
            const std::vector<uint8_t> img = slurp(floppy);
            if (img.size() < 512u)
            {
                std::fprintf(stderr, "iron86: cannot read floppy %s\n", floppy);
                return 1;
            }
            if (!p.load_floppy(img.data(), img.size()))
            {
                return 1;
            }
            if (keys != nullptr)
            {
                p.type_keys(keys);
            }
        }
        if (!p.c.load_bios_5150_8k(rom.data(), rom.size()))
        {
            return 1;
        }
        p.wire_pc_hw();
        if (!no_fast_post)
        {
            p.enable_fast_post();
        }
        std::fprintf(stderr, "5150 8K BIOS %zu bytes at FE000, reset FFFF:0000%s\n", rom.size(),
                     no_fast_post ? "" : " fast-post");
        uint64_t n = 0;
        while ((!p.c.halted()) && (n < 50000000ull))
        {
            if (!p.c.step())
            {
                break;
            }
            n++;
        }
        {
            char line[81];
            uint8_t row = 0;
            size_t col = 0;
            for (row = 0; row < 5u; row++)
            {
                for (col = 0; col < 80u; col++)
                {
                    const uint8_t ch =
                        p.c.mem_read8(0xB8000u + (static_cast<uint32_t>(row) * 80u +
                                                  static_cast<uint32_t>(col)) *
                                                     2u);
                    line[col] = ((ch >= 32u) && (ch < 127u)) ? static_cast<char>(ch) : ' ';
                }
                line[80] = '\0';
                std::fprintf(stderr, "B800[%u]: %s\n", row, line);
            }
        }
        std::fputs(p.tty().c_str(), stdout);
        if (!p.tty().empty() && (p.tty().back() != '\n'))
        {
            std::fputc('\n', stdout);
        }
        std::fprintf(stderr, "halted AX=%04X CS:IP=%04X:%04X last=%02X steps=%llu\n", p.c.ax(),
                     p.c.cs(), p.c.ip(), p.c.last_op(), static_cast<unsigned long long>(n));
        return ((p.c.cs() == 0xF000) || (p.c.cs() < 0x2000)) ? 0 : 1;
    }

    if (floppy != nullptr)
    {
        const std::vector<uint8_t> img = slurp(floppy);
        if (img.size() < 512u)
        {
            std::fprintf(stderr, "iron86: cannot read floppy %s\n", floppy);
            return 1;
        }
        iron86::pc p;
        if (no_audio)
        {
            p.enable_audio(false);
        }
        if (!p.load_floppy(img.data(), img.size()))
        {
            return 1;
        }
        if (keys != nullptr)
        {
            p.type_keys(keys);
        }
        p.boot();
        uint64_t n = 0;
        while ((!p.c.halted()) && (n < 20000000ull))
        {
            if (!p.c.step())
            {
                break;
            }
            n++;
            if ((n & 0x3FFFu) == 0)
            {
                const std::string &t = p.tty();
                if ((t.find("FCB OK") != std::string::npos) ||
                    (t.find("FCB FAIL") != std::string::npos))
                {
                    break;
                }
                if ((keys == nullptr) && (t.find("A>") != std::string::npos) && (n > 80000ull))
                {
                    /* Prompt is up; no --keys, skip the idle AH=0A spin. */
                    break;
                }
            }
        }
        std::fputs(p.tty().c_str(), stdout);
        if (!p.tty().empty() && (p.tty().back() != '\n'))
        {
            std::fputc('\n', stdout);
        }
        std::fprintf(stderr, "halted AX=%04X CS:IP=%04X:%04X last=%02X steps=%llu\n", p.c.ax(),
                     p.c.cs(), p.c.ip(), p.c.last_op(), static_cast<unsigned long long>(n));
        const bool ok = (p.tty().find("A>") != std::string::npos) ||
                        (p.tty().find("FCB OK") != std::string::npos);
        return ok ? 0 : 1;
    }

    const std::vector<uint8_t> buf = slurp(path);
    if (buf.empty())
    {
        std::fprintf(stderr, "iron86: cannot open %s\n", path);
        return 1;
    }
    iron86::cpu c;
    c.mem_write8(iron86::cpu::phys(0xF000, 0x0000), 0xF4);
    c.set_ivt(0x20, 0xF000, 0x0000);
    c.set_ivt(0x21, 0xF000, 0x0000);
    c.load_com(buf.data(), buf.size(), 0x1000);
    uint64_t n = 0;
    while ((!c.halted()) && (n < 1000000ull))
    {
        (void)c.step();
        n++;
    }
    std::fprintf(stderr, "halted AX=%04X CS:IP=%04X:%04X steps=%llu\n", c.ax(), c.cs(), c.ip(),
                 static_cast<unsigned long long>(n));
    return c.halted() ? 0 : 1;
}
