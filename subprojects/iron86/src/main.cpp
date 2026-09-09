/**
 * @file main.cpp
 * @brief iron86 CLI — .COM until HLT, or --floppy-a PATH boot at 0000:7C00.
 */
#include "iron86/config.h"
#include "iron86/cpu.h"
#include "iron86/hdc.h"
#include "iron86/pc.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <vector>

static const char *k_version = "0.17";

static void usage(FILE *out)
{
    std::fputs(
        "Usage: iron86 [options] [file.com]\n"
        "       iron86 --bios BIOS.BIN [--floppy-a PATH] [--floppy-b PATH] [--keys STRING]\n"
        "       iron86 --floppy-a PATH [--floppy-b PATH] [--keys STRING]\n"
        "\n"
        "  Chip-level 8086 (C++23, Py86 port). Hardware only — no DOS.\n"
        "  .COM loads at 1000:0100. --floppy-a without --bios: 0000:7C00.\n"
        "  --bios: 8K 5150 at FE000; 40K U19+U18 or 64K 5160 at F0000.\n"
        "  PPI/PIT/PIC/DMA/FDC (PyFloppy uPD765) for 1981 POST + INT 19h.\n"
        "  --hdc: Xebec S1410 @ 320h IRQ5 DMA3; IBM ROS at C8000 if found.\n"
        "  Fast-post (BDA 1234h) is default. PC speaker BEL is default.\n"
        "  Floppy PATH is a 160K/180K/320K/360K/720K/1.2M/1.44M image\n"
        "  (CHS from exact size, else nearest) or a directory packed as FlopFS\n"
        "  (360K if it fits, else 720K 80/2/9). No 2.88M on PC/XT/AT.\n"
        "  CLI overrides INI overrides preset. Options and operands may be interleaved.\n"
        "\n"
        "Options:\n"
        "  -h, --help           Show this help and exit\n"
        "  -v, --version        Show version and exit\n"
        "  --config PATH        INI file (default: iron86.ini in cwd if present)\n"
        "  --preset NAME        ibm-pc (default), ibm-xt; ibm-at/8286 not yet\n"
        "  --bios FILE          8K@FE000, 40K U19+U18, or 64K 5160@F0000\n"
        "  --no-fast-post       Cold POST (do not set RESET_FLAG=1234h)\n"
        "  --no-audio           Disable PC speaker BEL (default: on)\n"
        "  --mda                MDA 80×25 (B000, DIP 0x3D). Default CGA\n"
        "  --floppy-a PATH      Drive 0: image or directory (pack FlopFS)\n"
        "  --floppy-b PATH      Drive 1: image or directory (pack FlopFS)\n"
        "  --floppy PATH        Alias for --floppy-a\n"
        "  --hdc PATH           Raw HDD image (Xebec Type-1 306x4x17)\n"
        "  --hdc-rom FILE       Xebec option ROM at C8000 (IBM 5000059)\n"
        "  --keys STRING        Type STRING as INT 16h keys (default: none)\n"
        "  --until-prompt       Stop at FloppyOS/DOS A> (boot bench; default off)\n"
        "  --exec-map FILE      1 MiB executed-opcode map (same linear addrs, else 0)\n"
        "  --prefetch-bytes N   BIU prefetch queue (default: 4; 0=off, 6=8086; max 1024)\n"
        "\n",
        out);
    std::fprintf(out, "iron86 %s\n", k_version);
}

/**
 * @brief Match --name or --name=VAL. On bare --name, consume the next argv.
 * @return 1 matched, 0 no match, -1 missing/empty value.
 */
static int take_path_opt(const char *a, const char *name, const char **val, int *i, int argc,
                         char **argv)
{
    const size_t n = std::strlen(name);
    if (std::strncmp(a, name, n) != 0)
    {
        return 0;
    }
    if (a[n] == '=')
    {
        *val = a + n + 1;
        return ((*val)[0] == '\0') ? -1 : 1;
    }
    if (a[n] != '\0')
    {
        return 0;
    }
    if (*i + 1 >= argc)
    {
        return -1;
    }
    *val = argv[++(*i)];
    return 1;
}

/**
 * @brief Match --name or --name=VAL as an unsigned decimal.
 * @return 1 matched, 0 no match, -1 missing/empty/non-numeric.
 */
static int take_u32_opt(const char *a, const char *name, unsigned long *val, int *i, int argc,
                        char **argv)
{
    const size_t n = std::strlen(name);
    const char *s = nullptr;
    char *end = nullptr;
    if (std::strncmp(a, name, n) != 0)
    {
        return 0;
    }
    if (a[n] == '=')
    {
        s = a + n + 1;
    }
    else if (a[n] == '\0')
    {
        if (*i + 1 >= argc)
        {
            return -1;
        }
        s = argv[++(*i)];
    }
    else
    {
        return 0;
    }
    if ((s == nullptr) || (s[0] == '\0') || (s[0] == '-'))
    {
        return -1;
    }
    *val = std::strtoul(s, &end, 10);
    if ((end == s) || (*end != '\0'))
    {
        return -1;
    }
    return 1;
}

static unsigned clamp_prefetch(unsigned long v)
{
    if (v > iron86::k_prefetch_max)
    {
        std::fprintf(stderr, "iron86: --prefetch-bytes %lu clamped to %u\n", v,
                     iron86::k_prefetch_max);
        return iron86::k_prefetch_max;
    }
    return static_cast<unsigned>(v);
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

static bool mount_floppy(iron86::pc *p, uint8_t unit, const char *path)
{
    if ((p == nullptr) || (path == nullptr))
    {
        return false;
    }
    if (!p->attach_floppy_path(unit, path))
    {
        std::fprintf(stderr, "iron86: cannot attach floppy %c: %s\n",
                     (unit == 0) ? 'A' : 'B', path);
        return false;
    }
    return true;
}

/** @brief True if B800 80×25 text contains "A>". */
static bool b800_has_prompt(const iron86::cpu &c)
{
    uint32_t i = 0;
    for (; i + 1u < 80u * 25u; i++)
    {
        if ((c.mem_read8(0xB8000u + i * 2u) == static_cast<uint8_t>('A')) &&
            (c.mem_read8(0xB8000u + (i + 1u) * 2u) == static_cast<uint8_t>('>')))
        {
            return true;
        }
    }
    return false;
}

static const char *find_default_xebec_ros()
{
    static const char *const k_cands[] = {
        iron86::k_hdc_ros_default,
        "Py86/ROM/IBM/XT/FixedDiskAdapter/IBM_XEBEC_5000059_1982.BIN",
        "../Py86/ROM/IBM/XT/FixedDiskAdapter/IBM_XEBEC_5000059_1982.BIN",
        "../../Py86/ROM/IBM/XT/FixedDiskAdapter/IBM_XEBEC_5000059_1982.BIN",
    };
    const char *env = std::getenv("IRON86_XEBEC_ROM");
    struct stat st{};
    if ((env != nullptr) && (env[0] != '\0') && (stat(env, &st) == 0) && S_ISREG(st.st_mode))
    {
        return env;
    }
    size_t i = 0;
    for (; i < (sizeof(k_cands) / sizeof(k_cands[0])); i++)
    {
        if ((stat(k_cands[i], &st) == 0) && S_ISREG(st.st_mode))
        {
            return k_cands[i];
        }
    }
    return nullptr;
}

static bool mount_hdc(iron86::pc *p, const char *path, uint32_t cyl, uint32_t heads, uint32_t spt)
{
    if ((p == nullptr) || (path == nullptr) || (path[0] == '\0'))
    {
        return true;
    }
    if (!p->attach_hdc_path(path, cyl, heads, spt))
    {
        std::fprintf(stderr, "iron86: cannot attach hdc %s\n", path);
        return false;
    }
    return true;
}

static bool mount_hdc_rom(iron86::pc *p, const char *explicit_rom)
{
    if (p == nullptr)
    {
        return true;
    }
    if ((explicit_rom != nullptr) && (explicit_rom[0] != '\0'))
    {
        if (!p->load_hdc_rom_path(explicit_rom))
        {
            std::fprintf(stderr, "iron86: cannot load hdc-rom %s\n", explicit_rom);
            return false;
        }
        std::fprintf(stderr, "Xebec ROS %s at C8000\n", explicit_rom);
        return true;
    }
    const char *ros = find_default_xebec_ros();
    if (ros == nullptr)
    {
        std::fputs("iron86: warning: no Xebec ROS (set --hdc-rom or IRON86_XEBEC_ROM)\n",
                   stderr);
        return true;
    }
    if (!p->load_hdc_rom_path(ros))
    {
        std::fprintf(stderr, "iron86: warning: failed to map Xebec ROS %s\n", ros);
        return true;
    }
    std::fprintf(stderr, "Xebec ROS %s at C8000\n", ros);
    return true;
}

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        usage(stdout);
        return 0;
    }
    const char *path = nullptr;
    const char *keys = nullptr;
    const char *exec_map = nullptr;
    const char *config_path = nullptr;
    const char *hdc_rom = nullptr;
    iron86::machine_config cli{};
    bool no_fast_post = false;
    bool no_audio = false;
    bool mda = false;
    bool until_prompt = false;
    int i = 1;
    for (i = 1; i < argc; i++)
    {
        const char *a = argv[i];
        int t = 0;
        const char *opt = nullptr;
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
        t = take_path_opt(a, "--config", &config_path, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                std::fputs("iron86: --config needs PATH\n", stderr);
                return 2;
            }
            continue;
        }
        t = take_path_opt(a, "--preset", &opt, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                std::fputs("iron86: --preset needs NAME\n", stderr);
                return 2;
            }
            {
                iron86::machine_preset p = iron86::machine_preset::ibm_pc;
                const int r = iron86::parse_preset_name(opt, &p);
                if (r < 0)
                {
                    std::fprintf(stderr, "iron86: unknown preset %s\n", opt);
                    return 2;
                }
                if (r > 0)
                {
                    std::fprintf(stderr, "iron86: preset %s is not yet implemented\n",
                                 iron86::machine_preset_name(p));
                    return 2;
                }
                cli.preset = p;
                cli.preset_set = true;
            }
            continue;
        }
        t = take_path_opt(a, "--floppy-a", &opt, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                std::fputs("iron86: --floppy-a needs PATH\n", stderr);
                return 2;
            }
            cli.floppy_a = opt;
            continue;
        }
        t = take_path_opt(a, "--floppy-b", &opt, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                std::fputs("iron86: --floppy-b needs PATH\n", stderr);
                return 2;
            }
            cli.floppy_b = opt;
            continue;
        }
        t = take_path_opt(a, "--floppy", &opt, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                std::fputs("iron86: --floppy needs PATH\n", stderr);
                return 2;
            }
            cli.floppy_a = opt;
            continue;
        }
        t = take_path_opt(a, "--hdc-rom", &opt, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                std::fputs("iron86: --hdc-rom needs FILE\n", stderr);
                return 2;
            }
            hdc_rom = opt;
            continue;
        }
        t = take_path_opt(a, "--hdc", &opt, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                std::fputs("iron86: --hdc needs PATH\n", stderr);
                return 2;
            }
            cli.hdc_image = opt;
            continue;
        }
        t = take_path_opt(a, "--bios", &opt, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                std::fputs("iron86: --bios needs FILE\n", stderr);
                return 2;
            }
            cli.bios_rom = opt;
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
        if (std::strcmp(a, "--mda") == 0)
        {
            mda = true;
            continue;
        }
        if (std::strcmp(a, "--until-prompt") == 0)
        {
            until_prompt = true;
            continue;
        }
        t = take_path_opt(a, "--exec-map", &exec_map, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                std::fputs("iron86: --exec-map needs FILE\n", stderr);
                return 2;
            }
            continue;
        }
        t = take_path_opt(a, "--keys", &keys, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                std::fputs("iron86: --keys needs STRING\n", stderr);
                return 2;
            }
            continue;
        }
        {
            unsigned long pv = 0;
            t = take_u32_opt(a, "--prefetch-bytes", &pv, &i, argc, argv);
            if (t != 0)
            {
                if (t < 0)
                {
                    std::fputs("iron86: --prefetch-bytes needs N\n", stderr);
                    return 2;
                }
                cli.prefetch_bytes = clamp_prefetch(pv);
                cli.prefetch_set = true;
                continue;
            }
        }
        if (a[0] == '-')
        {
            std::fprintf(stderr, "iron86: unknown option %s\n", a);
            usage(stderr);
            return 2;
        }
        path = a;
    }

    iron86::machine_config cfg{};
    {
        std::string err;
        if (!iron86::merge_machine_config(config_path, cli, &cfg, &err))
        {
            std::fprintf(stderr, "iron86: %s\n", err.c_str());
            return 2;
        }
    }
    const char *floppy_a = cfg.floppy_a.empty() ? nullptr : cfg.floppy_a.c_str();
    const char *floppy_b = cfg.floppy_b.empty() ? nullptr : cfg.floppy_b.c_str();
    const char *bios = cfg.bios_rom.empty() ? nullptr : cfg.bios_rom.c_str();
    const char *hdc = cfg.hdc_image.empty() ? nullptr : cfg.hdc_image.c_str();
    const unsigned prefetch_bytes = clamp_prefetch(cfg.prefetch_bytes);
    /* A .COM operand is exclusive: cwd iron86.ini must not steal the run. */
    if (path != nullptr)
    {
        floppy_a = nullptr;
        floppy_b = nullptr;
        bios = nullptr;
        hdc = nullptr;
    }

    if ((path == nullptr) && (floppy_a == nullptr) && (floppy_b == nullptr) && (bios == nullptr))
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
        p.c.set_prefetch_bytes(prefetch_bytes);
        if ((exec_map != nullptr) && !p.c.open_exec_map(exec_map))
        {
            std::fprintf(stderr, "iron86: cannot create exec-map %s\n", exec_map);
            return 1;
        }
        if (no_audio)
        {
            p.enable_audio(false);
        }
        if ((floppy_a != nullptr) && !mount_floppy(&p, 0, floppy_a))
        {
            return 1;
        }
        if ((floppy_b != nullptr) && !mount_floppy(&p, 1, floppy_b))
        {
            return 1;
        }
        if (!mount_hdc(&p, hdc, cfg.hdc_cylinders, cfg.hdc_heads, cfg.hdc_spt))
        {
            return 1;
        }
        if (!p.c.load_bios(rom.data(), rom.size()))
        {
            std::fprintf(stderr,
                         "iron86: unsupported BIOS size %zu (need 8K, 40K U19+U18, or 64K)\n",
                         rom.size());
            return 1;
        }
        if ((hdc != nullptr) && !mount_hdc_rom(&p, hdc_rom))
        {
            return 1;
        }
        p.wire_pc_hw();
        if (mda)
        {
            p.set_mda(true);
        }
        if (!no_fast_post)
        {
            p.enable_fast_post();
        }
        if (keys != nullptr)
        {
            /* Queue only — type_keys() pumps steps and eats keys during POST. */
            const char *k = keys;
            for (; *k != 0; ++k)
            {
                p.feed_ascii(static_cast<uint8_t>(*k));
            }
        }
        if (rom.size() == 65536u)
        {
            std::fprintf(stderr, "5160 64K BIOS %zu bytes at F0000, reset FFFF:0000%s\n",
                         rom.size(), no_fast_post ? "" : " fast-post");
        }
        else if (rom.size() == 40960u)
        {
            std::fprintf(stderr, "5160 40K BIOS (U19+U18) at F0000/F8000%s\n",
                         no_fast_post ? "" : " fast-post");
        }
        else
        {
            std::fprintf(stderr, "5150 8K BIOS %zu bytes at FE000, reset FFFF:0000%s\n",
                         rom.size(), no_fast_post ? "" : " fast-post");
        }
        uint64_t n = 0;
        while ((!p.c.halted()) && (n < 150000000ull))
        {
            if (!p.c.step())
            {
                break;
            }
            n++;
            if (until_prompt && ((n & 8191ull) == 0ull) && b800_has_prompt(p.c))
            {
                break;
            }
        }
        {
            char line[81];
            uint8_t row = 0;
            size_t col = 0;
            for (row = 0; row < 25u; row++)
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

    if ((floppy_a != nullptr) || (floppy_b != nullptr))
    {
        iron86::pc p;
        p.c.set_prefetch_bytes(prefetch_bytes);
        if ((exec_map != nullptr) && !p.c.open_exec_map(exec_map))
        {
            std::fprintf(stderr, "iron86: cannot create exec-map %s\n", exec_map);
            return 1;
        }
        if (no_audio)
        {
            p.enable_audio(false);
        }
        if (floppy_a == nullptr)
        {
            std::fputs("iron86: --floppy-b needs --floppy-a or --bios to boot\n", stderr);
            return 2;
        }
        if (!mount_floppy(&p, 0, floppy_a))
        {
            return 1;
        }
        if ((floppy_b != nullptr) && !mount_floppy(&p, 1, floppy_b))
        {
            return 1;
        }
        if (!mount_hdc(&p, hdc, cfg.hdc_cylinders, cfg.hdc_heads, cfg.hdc_spt))
        {
            return 1;
        }
        if ((hdc != nullptr) && !mount_hdc_rom(&p, hdc_rom))
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
            if (until_prompt && ((n & 8191ull) == 0ull) && b800_has_prompt(p.c))
            {
                break;
            }
            if ((n & 0x3FFFu) == 0)
            {
                const std::string &t = p.tty();
                if (t.find("FCB FAIL") != std::string::npos)
                {
                    break;
                }
                if (keys != nullptr)
                {
                    const bool want_mz = std::strstr(keys, "MZFAR") != nullptr;
                    const bool want_if = std::strstr(keys, "IFLAG") != nullptr;
                    const bool fcb = t.find("FCB OK") != std::string::npos;
                    const bool mz = t.find("MZFAR OK") != std::string::npos;
                    const bool iff = t.find("IF OK") != std::string::npos;
                    if (fcb && (!want_mz || mz) && (!want_if || iff))
                    {
                        break;
                    }
                }
                else if ((t.find("A>") != std::string::npos) && (n > 80000ull))
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
        {
            char line[81];
            uint8_t row = 0;
            size_t col = 0;
            for (row = 0; row < 25u; row++)
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
    c.set_prefetch_bytes(prefetch_bytes);
    if ((exec_map != nullptr) && !c.open_exec_map(exec_map))
    {
        std::fprintf(stderr, "iron86: cannot create exec-map %s\n", exec_map);
        return 1;
    }
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
