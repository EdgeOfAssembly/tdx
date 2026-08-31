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
        "\n"
        "  Turbo Debugger X — native Linux debugger for DOS EXE/COM\n"
        "  (librex core; Unicorn 8086 + Capstone). Options and the input\n"
        "  path may appear in any order.\n"
        "\n"
        "Options:\n"
        "  -h, --help            Show this help and exit\n"
        "  -v, --version         Show version and exit\n"
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
        else if (std::strcmp(a, "--no-sock") == 0)
        {
            out->no_sock = true;
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
