/**
 * @file main.cpp
 * @brief iron86 CLI — .COM until HLT, or --floppy-a PATH boot at 0000:7C00.
 */
#include "iron86/config.h"
#include "iron86/cpu.h"
#include "iron86/hdc.h"
#include "iron86/log.h"
#include "iron86/pc.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <ctime>
#include <unistd.h>
#include <vector>

using iron86::fd_printf;
using iron86::fd_puts;
using iron86::fd_write;
using iron86::log_event;

static const char *k_version = "0.22";

static void usage(int fd)
{
    fd_puts(fd,
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
            "  --preset NAME        ibm-pc/5150 (floppy), ibm-xt/5160 (Xebec); ibm-at not yet\n"
            "  --bios FILE          8K@FE000, 40K U19+U18, or 64K 5160@F0000\n"
            "  --no-fast-post       Cold POST (do not set RESET_FLAG=1234h)\n"
            "  --no-audio           Disable PC speaker BEL (default: on)\n"
            "  --mda                MDA 80×25 (B000, DIP 0x3D). Default CGA\n"
            "  --floppy-a PATH      Drive 0: image or directory (pack FlopFS)\n"
            "  --floppy-b PATH      Drive 1: image or directory (pack FlopFS)\n"
            "  --floppy PATH        Alias for --floppy-a\n"
            "  --hdc PATH           Raw HDD image (Xebec Type-1 306x4x17)\n"
            "  --hdc-rom FILE       Xebec option ROM at C8000 (IBM 5000059)\n"
            "  --log FILE           Append event log (HDC/floppy/BIOS/ROS/INT13); default off\n"
            "  --lock-mem           mlock guest RAM + HDC image (default off)\n"
            "  --keys STRING        Type STRING as INT 16h keys (default: none)\n"
            "  --until-prompt       Stop at FloppyOS/DOS A> (boot bench; default off)\n"
            "  --exec-map FILE      1 MiB executed-opcode map (same linear addrs, else 0)\n"
            "  --prefetch-bytes N   BIU prefetch queue (default: 6=8086; 4=8088; 0=off; max 1024)\n"
            "\n");
    fd_printf(fd, "iron86 %s\n", k_version);
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
        fd_printf(STDERR_FILENO, "iron86: --prefetch-bytes %lu clamped to %u\n", v,
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
        log_event("iron86: cannot attach floppy %c: %s\n", (unit == 0) ? 'A' : 'B', path);
        return false;
    }
    const size_t nb = p->floppy_bytes(unit);
    uint32_t spt = 0;
    uint32_t heads = 0;
    iron86::floppy_geom(nb, &spt, &heads);
    const uint32_t denom = 512u * spt * heads;
    const uint32_t cyl = (denom == 0u) ? 0u : static_cast<uint32_t>(nb / denom);
    const uint32_t lba = static_cast<uint32_t>(nb / 512u);
    char hs[32];
    iron86::format_human_size(nb, hs, sizeof(hs));
    log_event("FD%u  path=%s  CHS=%u/%u/%u  LBA=%u  size=%s  bytes=%zu  DOS=%c:\n",
              static_cast<unsigned>(unit), path, cyl, heads, spt, lba, hs, nb,
              (unit == 0) ? 'A' : 'B');
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

/** @brief One write(2) of 25 CGA text rows (no FILE* buffering). */
static void dump_b800(const iron86::cpu &c)
{
    char buf[2304];
    size_t o = 0;
    uint8_t row = 0;
    for (; row < 25u; row++)
    {
        const int n = std::snprintf(buf + o, sizeof(buf) - o, "B800[%u]: ",
                                    static_cast<unsigned>(row));
        if ((n < 0) || (static_cast<size_t>(n) >= (sizeof(buf) - o)))
        {
            break;
        }
        o += static_cast<size_t>(n);
        size_t col = 0;
        for (; col < 80u; col++)
        {
            if ((o + 1u) >= sizeof(buf))
            {
                break;
            }
            const uint8_t ch =
                c.mem_read8(0xB8000u + (static_cast<uint32_t>(row) * 80u +
                                        static_cast<uint32_t>(col)) *
                                           2u);
            buf[o++] = ((ch >= 32u) && (ch < 127u)) ? static_cast<char>(ch) : ' ';
        }
        if ((o + 1u) >= sizeof(buf))
        {
            break;
        }
        buf[o++] = '\n';
    }
    fd_write(STDERR_FILENO, buf, o);
}

static void emit_tty(const std::string &t)
{
    if (t.empty())
    {
        return;
    }
    fd_write(STDOUT_FILENO, t.data(), t.size());
    if (t.back() != '\n')
    {
        fd_write(STDOUT_FILENO, "\n", 1);
    }
}

static void maybe_lock_mem(iron86::cpu *c, iron86::pc *p, bool want)
{
    if (!want || (c == nullptr))
    {
        return;
    }
    (void)iron86::lock_region(c->ram(), iron86::cpu::ram_size(), "ram");
    if ((p != nullptr) && p->hdc().attached())
    {
        (void)iron86::lock_region(p->hdc().image_data(), p->hdc().image().size(), "hdc");
    }
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
        log_event("iron86: cannot attach hdc %s\n", path);
        return false;
    }
    const uint32_t lba = cyl * heads * spt;
    const uint64_t bytes = static_cast<uint64_t>(lba) * 512ull;
    char hs[32];
    iron86::format_human_size(bytes, hs, sizeof(hs));
    const size_t file_n = p->hdc_bytes();
    if (file_n == static_cast<size_t>(bytes))
    {
        log_event("HDC0  path=%s  CHS=%u/%u/%u  LBA=%u  size=%s  bytes=%llu  "
                  "BIOS_DL=80h  DOS=C: after FDISK\n",
                  path, cyl, heads, spt, lba, hs, static_cast<unsigned long long>(bytes));
    }
    else
    {
        log_event("HDC0  path=%s  CHS=%u/%u/%u  LBA=%u  size=%s  bytes=%llu  file=%zu  "
                  "BIOS_DL=80h  DOS=C: after FDISK\n",
                  path, cyl, heads, spt, lba, hs, static_cast<unsigned long long>(bytes),
                  file_n);
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
            log_event("iron86: cannot load hdc-rom %s\n", explicit_rom);
            return false;
        }
        {
            struct stat st{};
            char hs[32];
            const uint64_t nb =
                ((stat(explicit_rom, &st) == 0) && S_ISREG(st.st_mode))
                    ? static_cast<uint64_t>(st.st_size)
                    : 0ull;
            iron86::format_human_size(nb, hs, sizeof(hs));
            log_event("ROS  path=%s  size=%s  bytes=%llu  map=C8000\n", explicit_rom, hs,
                      static_cast<unsigned long long>(nb));
        }
        return true;
    }
    const char *ros = find_default_xebec_ros();
    if (ros == nullptr)
    {
        log_event("iron86: warning: no Xebec ROS (set --hdc-rom or IRON86_XEBEC_ROM)\n");
        return true;
    }
    if (!p->load_hdc_rom_path(ros))
    {
        log_event("iron86: warning: failed to map Xebec ROS %s\n", ros);
        return true;
    }
    {
        struct stat st{};
        char hs[32];
        const uint64_t nb =
            ((stat(ros, &st) == 0) && S_ISREG(st.st_mode)) ? static_cast<uint64_t>(st.st_size)
                                                           : 0ull;
        iron86::format_human_size(nb, hs, sizeof(hs));
        log_event("ROS  path=%s  size=%s  bytes=%llu  map=C8000\n", ros, hs,
                  static_cast<unsigned long long>(nb));
    }
    return true;
}

int main(int argc, char **argv)
{
    iron86::log_init_stdio();
    if (argc <= 1)
    {
        usage(STDOUT_FILENO);
        return 0;
    }
    const char *path = nullptr;
    const char *keys = nullptr;
    const char *exec_map = nullptr;
    const char *config_path = nullptr;
    const char *hdc_rom = nullptr;
    const char *log_path = nullptr;
    iron86::machine_config cli{};
    bool no_fast_post = false;
    bool no_audio = false;
    bool mda = false;
    bool until_prompt = false;
    bool lock_mem = false;
    int i = 1;
    for (i = 1; i < argc; i++)
    {
        const char *a = argv[i];
        int t = 0;
        const char *opt = nullptr;
        if ((std::strcmp(a, "-h") == 0) || (std::strcmp(a, "--help") == 0))
        {
            usage(STDOUT_FILENO);
            return 0;
        }
        if ((std::strcmp(a, "-v") == 0) || (std::strcmp(a, "--version") == 0))
        {
            fd_printf(STDOUT_FILENO, "iron86 %s\n", k_version);
            return 0;
        }
        t = take_path_opt(a, "--config", &config_path, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                fd_puts(STDERR_FILENO, "iron86: --config needs PATH\n");
                return 2;
            }
            continue;
        }
        t = take_path_opt(a, "--preset", &opt, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                fd_puts(STDERR_FILENO, "iron86: --preset needs NAME\n");
                return 2;
            }
            {
                iron86::machine_preset p = iron86::machine_preset::ibm_pc;
                const int r = iron86::parse_preset_name(opt, &p);
                if (r < 0)
                {
                    fd_printf(STDERR_FILENO, "iron86: unknown preset %s\n", opt);
                    return 2;
                }
                if (r > 0)
                {
                    fd_printf(STDERR_FILENO, "iron86: preset %s is not yet implemented\n",
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
                fd_puts(STDERR_FILENO, "iron86: --floppy-a needs PATH\n");
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
                fd_puts(STDERR_FILENO, "iron86: --floppy-b needs PATH\n");
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
                fd_puts(STDERR_FILENO, "iron86: --floppy needs PATH\n");
                return 2;
            }
            cli.floppy_a = opt;
            continue;
        }
        t = take_path_opt(a, "--log", &log_path, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                fd_puts(STDERR_FILENO, "iron86: --log needs FILE\n");
                return 2;
            }
            continue;
        }
        t = take_path_opt(a, "--hdc-rom", &opt, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                fd_puts(STDERR_FILENO, "iron86: --hdc-rom needs FILE\n");
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
                fd_puts(STDERR_FILENO, "iron86: --hdc needs PATH\n");
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
                fd_puts(STDERR_FILENO, "iron86: --bios needs FILE\n");
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
        if (std::strcmp(a, "--lock-mem") == 0)
        {
            lock_mem = true;
            continue;
        }
        t = take_path_opt(a, "--exec-map", &exec_map, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                fd_puts(STDERR_FILENO, "iron86: --exec-map needs FILE\n");
                return 2;
            }
            continue;
        }
        t = take_path_opt(a, "--keys", &keys, &i, argc, argv);
        if (t != 0)
        {
            if (t < 0)
            {
                fd_puts(STDERR_FILENO, "iron86: --keys needs STRING\n");
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
                    fd_puts(STDERR_FILENO, "iron86: --prefetch-bytes needs N\n");
                    return 2;
                }
                cli.prefetch_bytes = clamp_prefetch(pv);
                cli.prefetch_set = true;
                continue;
            }
        }
        if (a[0] == '-')
        {
            fd_printf(STDERR_FILENO, "iron86: unknown option %s\n", a);
            usage(STDERR_FILENO);
            return 2;
        }
        path = a;
    }

    if (log_path != nullptr)
    {
        if (!iron86::log_open(log_path))
        {
            fd_printf(STDERR_FILENO, "iron86: cannot open log %s\n", log_path);
            return 1;
        }
        (void)std::atexit(iron86::log_close);
    }

    iron86::machine_config cfg{};
    {
        std::string err;
        if (!iron86::merge_machine_config(config_path, cli, &cfg, &err))
        {
            fd_printf(STDERR_FILENO, "iron86: %s\n", err.c_str());
            return 2;
        }
    }
    if (!cli.hdc_image.empty() && !cli.preset_set)
    {
        cfg.preset = iron86::machine_preset::ibm_xt;
        cfg.preset_set = true;
    }
    if (cli.preset_set && cfg.bios_rom.empty() && (path == nullptr))
    {
        cfg.bios_rom = iron86::find_default_bios_rom(cfg.preset);
        if (cfg.bios_rom.empty())
        {
            fd_printf(STDERR_FILENO, "iron86: --preset %s needs --bios (no default ROM found)\n",
                      iron86::machine_preset_name(cfg.preset));
            return 2;
        }
    }
    if (!cfg.hdc_image.empty() && (cfg.preset == iron86::machine_preset::ibm_pc))
    {
        fd_puts(STDERR_FILENO,
                "iron86: warning: ibm-pc/5150 BIOS does not scan C8000; HDD needs --preset ibm-xt\n");
    }
    const char *floppy_a = cfg.floppy_a.empty() ? nullptr : cfg.floppy_a.c_str();
    const char *floppy_b = cfg.floppy_b.empty() ? nullptr : cfg.floppy_b.c_str();
    const char *bios = cfg.bios_rom.empty() ? nullptr : cfg.bios_rom.c_str();
    const char *hdc = cfg.hdc_image.empty() ? nullptr : cfg.hdc_image.c_str();
    const unsigned prefetch_bytes = clamp_prefetch(cfg.prefetch_bytes);
    const bool cli_hdc = !cli.hdc_image.empty();
    /* A .COM operand ignores INI floppy/BIOS so cwd iron86.ini cannot steal
     * tiny.com tests. CLI --hdc still attaches (hdcprobe). */
    if (path != nullptr)
    {
        floppy_a = nullptr;
        floppy_b = nullptr;
        bios = nullptr;
        if (!cli_hdc)
        {
            hdc = nullptr;
        }
    }

    if ((path == nullptr) && (floppy_a == nullptr) && (floppy_b == nullptr) && (bios == nullptr))
    {
        usage(STDOUT_FILENO);
        return 0;
    }

    if (bios != nullptr)
    {
        const std::vector<uint8_t> rom = slurp(bios);
        if (rom.empty())
        {
            fd_printf(STDERR_FILENO, "iron86: cannot read BIOS %s\n", bios);
            return 1;
        }
        iron86::pc p;
        p.c.set_prefetch_bytes(prefetch_bytes);
        if ((exec_map != nullptr) && !p.c.open_exec_map(exec_map))
        {
            fd_printf(STDERR_FILENO, "iron86: cannot create exec-map %s\n", exec_map);
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
            fd_printf(STDERR_FILENO,
                         "iron86: unsupported BIOS size %zu (need 8K, 40K U19+U18, or 64K)\n",
                         rom.size());
            return 1;
        }
        if ((hdc != nullptr) && !mount_hdc_rom(&p, hdc_rom))
        {
            return 1;
        }
        p.wire_pc_hw(iron86::log_active());
        maybe_lock_mem(&p.c, &p, lock_mem);
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
        {
            char hs[32];
            iron86::format_human_size(rom.size(), hs, sizeof(hs));
            const char *fast = no_fast_post ? "" : " fast-post";
            if (rom.size() == 65536u)
            {
                log_event("BIOS  path=%s  size=%s  bytes=%zu  map=F0000  5160 64K%s\n", bios, hs,
                          rom.size(), fast);
            }
            else if (rom.size() == 40960u)
            {
                log_event("BIOS  path=%s  size=%s  bytes=%zu  map=F0000/F8000  5160 40K%s\n",
                          bios, hs, rom.size(), fast);
            }
            else
            {
                log_event("BIOS  path=%s  size=%s  bytes=%zu  map=FE000  5150 8K%s\n", bios, hs,
                          rom.size(), fast);
            }
        }
        uint64_t n = 0;
        struct timespec irq0_last{};
        (void)clock_gettime(CLOCK_MONOTONIC, &irq0_last);
        while ((!p.c.halted()) && (n < 150000000ull))
        {
            if (!p.c.step())
            {
                break;
            }
            n++;
            if ((n & 4095ull) == 0ull)
            {
                struct timespec now{};
                uint64_t dt = 0;
                (void)clock_gettime(CLOCK_MONOTONIC, &now);
                dt = (static_cast<uint64_t>(now.tv_sec) - static_cast<uint64_t>(irq0_last.tv_sec)) *
                         1000000000ull +
                     static_cast<uint64_t>(now.tv_nsec) -
                     static_cast<uint64_t>(irq0_last.tv_nsec);
                if (dt >= 55000000ull)
                {
                    p.wall_clock_irq0();
                    irq0_last = now;
                }
            }
            if (until_prompt && ((n & 8191ull) == 0ull) && b800_has_prompt(p.c))
            {
                break;
            }
        }
        dump_b800(p.c);
        emit_tty(p.tty());
        fd_printf(STDERR_FILENO, "halted AX=%04X CS:IP=%04X:%04X last=%02X steps=%llu\n", p.c.ax(),
                  p.c.cs(), p.c.ip(), p.c.last_op(), static_cast<unsigned long long>(n));
        return ((p.c.cs() == 0xF000) || (p.c.cs() < 0x2000)) ? 0 : 1;
    }

    if ((floppy_a != nullptr) || (floppy_b != nullptr))
    {
        iron86::pc p;
        p.c.set_prefetch_bytes(prefetch_bytes);
        if ((exec_map != nullptr) && !p.c.open_exec_map(exec_map))
        {
            fd_printf(STDERR_FILENO, "iron86: cannot create exec-map %s\n", exec_map);
            return 1;
        }
        if (no_audio)
        {
            p.enable_audio(false);
        }
        if (floppy_a == nullptr)
        {
            fd_puts(STDERR_FILENO, "iron86: --floppy-b needs --floppy-a or --bios to boot\n");
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
        maybe_lock_mem(&p.c, &p, lock_mem);
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
        emit_tty(p.tty());
        fd_printf(STDERR_FILENO, "halted AX=%04X CS:IP=%04X:%04X last=%02X steps=%llu\n", p.c.ax(),
                  p.c.cs(), p.c.ip(), p.c.last_op(), static_cast<unsigned long long>(n));
        dump_b800(p.c);
        const bool ok = (p.tty().find("A>") != std::string::npos) ||
                        (p.tty().find("FCB OK") != std::string::npos);
        return ok ? 0 : 1;
    }

    const std::vector<uint8_t> buf = slurp(path);
    if (buf.empty())
    {
        fd_printf(STDERR_FILENO, "iron86: cannot open %s\n", path);
        return 1;
    }
    if (hdc != nullptr)
    {
        iron86::pc p;
        p.c.set_prefetch_bytes(prefetch_bytes);
        if ((exec_map != nullptr) && !p.c.open_exec_map(exec_map))
        {
            fd_printf(STDERR_FILENO, "iron86: cannot create exec-map %s\n", exec_map);
            return 1;
        }
        if (!mount_hdc(&p, hdc, cfg.hdc_cylinders, cfg.hdc_heads, cfg.hdc_spt))
        {
            return 1;
        }
        p.wire_pc_hw(iron86::log_active());
        maybe_lock_mem(&p.c, &p, lock_mem);
        p.c.mem_write8(iron86::cpu::phys(0xF000, 0x0000), 0xF4);
        p.c.set_ivt(0x20, 0xF000, 0x0000);
        p.c.set_ivt(0x21, 0xF000, 0x0000);
        p.c.load_com(buf.data(), buf.size(), 0x1000);
        uint64_t n = 0;
        while ((!p.c.halted()) && (n < 1000000ull))
        {
            (void)p.c.step();
            n++;
        }
        fd_printf(STDERR_FILENO, "halted AX=%04X CS:IP=%04X:%04X steps=%llu\n", p.c.ax(), p.c.cs(),
                     p.c.ip(), static_cast<unsigned long long>(n));
        return p.c.halted() ? 0 : 1;
    }
    iron86::cpu c;
    c.set_prefetch_bytes(prefetch_bytes);
    maybe_lock_mem(&c, nullptr, lock_mem);
    if ((exec_map != nullptr) && !c.open_exec_map(exec_map))
    {
        fd_printf(STDERR_FILENO, "iron86: cannot create exec-map %s\n", exec_map);
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
    fd_printf(STDERR_FILENO, "halted AX=%04X CS:IP=%04X:%04X steps=%llu\n", c.ax(), c.cs(), c.ip(),
                 static_cast<unsigned long long>(n));
    return c.halted() ? 0 : 1;
}
