/**
 * @file tdx_cli.cpp
 * @brief Order-independent CLI for tdx (cli-design: -h/-v, --no-* for defaults).
 */

#include "tdx/tdx_cli.h"

#include "tdx/tdx_version.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

void tdx_print_usage(FILE *fp)
{
    std::fputs(
        "Usage: tdx [options] [file.exe|file.com]\n"
        "       tdx --floppy-a image.img [options]\n"
        "       tdx --uc-floppy image.img [options]\n"
        "       tdx --bios BIOS.BIN [--floppy-a A] [--floppy-b B] [options]\n"
        "\n"
        "  Turbo Debugger X — native Linux debugger for DOS EXE/COM\n"
        "  (librex core; Unicorn 8086 default; --floppy-a uses iron86).\n"
        "  Options and the input path may appear in any order.\n"
        "\n"
        "Options:\n"
        "  -h, --help            Show this help and exit\n"
        "  -v, --version         Show version and exit\n"
        "      --floppy-a PATH   A: raw image or host directory (FlopFS pack)\n"
        "      --floppy PATH     Alias for --floppy-a\n"
        "      --floppy-b PATH   B: image or host dir (FlopFS 360K, or 720K if needed)\n"
        "      --uc-floppy IMAGE Same boot on Unicorn (vs iron86 --floppy-a)\n"
        "      --bios FILE       IBM 5150 8K BIOS on iron86 (FFFF:0000, Py86 map)\n"
        "                        with --floppy-a: PyFloppy uPD765 A: for INT 19h\n"
        "      --mda             MDA 80×25 (B000, DIP 0x3D). Default CGA B800\n"
        "      --no-ui           Headless (no SDL windows)\n"
        "      --game            Also open CGA in this process (default: use tdxview)\n"
        "      --no-sock         Do not listen on the agent UNIX socket\n"
        "      --sock PATH       Agent socket (default: /tmp/tdx.sock)\n"
        "      --log-file PATH   Also write logs to PATH\n"
        "      --symbols PATH    Load TSV/MAP symbols (seg:off\\tname)\n"
        "      --ghidra          Run Ghidra headless export then load symbols\n"
        "      --cwd PATH        DOS current directory for INT 21 (default: file dir)\n"
        "      --run             After load, run until break/halt (headless-friendly)\n"
        "      --verbose         Log every stepped instruction to stderr\n"
        "      --exec-map FILE   1 MiB executed-opcode map (iron86; same linear addrs)\n"
        "      --scale N         CPU window integer scale (default: 2)\n"
        "\n"
        "Keys (CPU window): F7 trace, F8 step over, F9 run/pause, F2 breakpoint,\n"
        "  Ctrl-F2 reset program, Alt-X quit.\n"
        "  CGA: tdxview (listens /tmp/tdxview.sock). Agent: tdxctl / tdxctl --view\n"
        "  (UNIX keep-alive sockets; Xmux not required).\n"
        "\n"
        "tdx " TDX_VERSION_STRING "\n",
        fp);
}

bool tdx_cli_parse(int argc, char **argv, tdx_cli *out)
{
    int i = 1;
    if (out == nullptr)
    {
        return false;
    }
    *out = tdx_cli{};
    if (argc <= 1)
    {
        out->help = true;
        return true;
    }
    for (i = 1; i < argc; i++)
    {
        const char *a = argv[i];
        if ((std::strcmp(a, "-h") == 0) || (std::strcmp(a, "--help") == 0))
        {
            out->help = true;
        }
        else if ((std::strcmp(a, "-v") == 0) || (std::strcmp(a, "--version") == 0))
        {
            out->version = true;
        }
        else if (std::strcmp(a, "--no-ui") == 0)
        {
            out->no_ui = true;
        }
        else if (std::strcmp(a, "--game") == 0)
        {
            out->game = true;
        }
        else if (std::strcmp(a, "--mda") == 0)
        {
            out->mda = true;
        }
        else if ((std::strcmp(a, "--floppy-a") == 0) || (std::strcmp(a, "--floppy") == 0))
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->floppy = argv[++i];
        }
        else if (std::strncmp(a, "--floppy-a=", 11) == 0)
        {
            out->floppy = a + 11;
        }
        else if (std::strncmp(a, "--floppy=", 9) == 0)
        {
            out->floppy = a + 9;
        }
        else if (std::strcmp(a, "--floppy-b") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->floppy_b = argv[++i];
        }
        else if (std::strncmp(a, "--floppy-b=", 11) == 0)
        {
            out->floppy_b = a + 11;
        }
        else if (std::strcmp(a, "--uc-floppy") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->uc_floppy = argv[++i];
        }
        else if (std::strncmp(a, "--uc-floppy=", 12) == 0)
        {
            out->uc_floppy = a + 12;
        }
        else if (std::strcmp(a, "--bios") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->bios = argv[++i];
        }
        else if (std::strncmp(a, "--bios=", 7) == 0)
        {
            out->bios = a + 7;
        }
        else if (std::strcmp(a, "--no-sock") == 0)
        {
            out->no_sock = true;
        }
        else if (std::strcmp(a, "--exec-map") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->exec_map = argv[++i];
        }
        else if (std::strncmp(a, "--exec-map=", 11) == 0)
        {
            out->exec_map = a + 11;
        }
        else if (std::strcmp(a, "--run") == 0)
        {
            out->run = true;
        }
        else if (std::strcmp(a, "--ghidra") == 0)
        {
            out->ghidra = true;
        }
        else if (std::strcmp(a, "--verbose") == 0)
        {
            out->verbose = true;
        }
        else if (std::strcmp(a, "--sock") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->sock_path = argv[++i];
        }
        else if (std::strncmp(a, "--sock=", 7) == 0)
        {
            out->sock_path = a + 7;
        }
        else if (std::strcmp(a, "--log-file") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->log_file = argv[++i];
        }
        else if (std::strncmp(a, "--log-file=", 11) == 0)
        {
            out->log_file = a + 11;
        }
        else if (std::strcmp(a, "--symbols") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->symbols = argv[++i];
        }
        else if (std::strncmp(a, "--symbols=", 10) == 0)
        {
            out->symbols = a + 10;
        }
        else if (std::strcmp(a, "--cwd") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->cwd = argv[++i];
        }
        else if (std::strncmp(a, "--cwd=", 6) == 0)
        {
            out->cwd = a + 6;
        }
        else if (std::strcmp(a, "--scale") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->scale = std::atoi(argv[++i]);
            if (out->scale < 1)
            {
                out->scale = 1;
            }
        }
        else if (a[0] == '-')
        {
            std::fprintf(stderr, "tdx: unknown option %s\n", a);
            out->usage_error = true;
            return false;
        }
        else
        {
            if (!out->input.empty())
            {
                std::fprintf(stderr, "tdx: extra operand %s\n", a);
                out->usage_error = true;
                return false;
            }
            out->input = a;
        }
    }
    return true;
}
