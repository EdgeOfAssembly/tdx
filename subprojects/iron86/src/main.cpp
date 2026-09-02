/**
 * @file main.cpp
 * @brief iron86 CLI — load a .COM and run until HLT.
 */
#include "iron86/cpu.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static const char *k_version = "0.1";

static void usage(FILE *out)
{
    std::fputs(
        "Usage: iron86 [options] [file.com]\n"
        "\n"
        "  Chip-level 8086 (C++23, Py86 port). Hardware only — no DOS.\n"
        "  Loads .COM at 1000:0100 and runs until HLT.\n"
        "\n"
        "Options:\n"
        "  -h, --help       Show this help and exit\n"
        "  -v, --version    Show version and exit\n"
        "\n"
        "iron86 0.1\n",
        out);
}

int main(int argc, char **argv)
{
    const char *path = nullptr;
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
        if (a[0] == '-')
        {
            std::fprintf(stderr, "iron86: unknown option %s\n", a);
            usage(stderr);
            return 2;
        }
        if (path != nullptr)
        {
            std::fprintf(stderr, "iron86: extra operand %s\n", a);
            return 2;
        }
        path = a;
    }
    if (path == nullptr)
    {
        usage(stdout);
        return 0;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        std::fprintf(stderr, "iron86: cannot open %s\n", path);
        return 1;
    }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    iron86::cpu c;
    /* INT 20h / INT 21h → HLT stub at F000:0000 so a DOS COM can stop. */
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
