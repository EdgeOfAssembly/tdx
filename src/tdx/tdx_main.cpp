/**
 * @file tdx_main.cpp
 * @brief tdx CLI entry: load DOS EXE/COM, optional SDL UI + agent socket.
 */

#include "rex/rex.h"
#include "rex/rex_log.h"
#include "rex/rex_sock.h"
#include "tdx/tdx_cli.h"
#include "tdx/tdx_ui.h"
#include "tdx/tdx_version.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <csignal>
#include <unistd.h>

static int print_headless(rex_session *s)
{
    rex_regs_i8086 r{};
    rex_insn ins[8];
    size_t n = 0;
    size_t i = 0;
    rex_session_get_regs_i8086(s, &r);
    std::printf("CS:IP %04X:%04X  AX=%04X BX=%04X CX=%04X DX=%04X\n", r.cs, r.ip, r.ax, r.bx, r.cx,
                r.dx);
    std::printf("SS:SP %04X:%04X  DS=%04X ES=%04X FLAGS=%04X stop=%d halted=%d\n", r.ss, r.sp, r.ds,
                r.es, r.flags, (int)rex_session_stop_reason(s), (int)rex_session_halted(s));
    rex_session_disasm(s, UINT64_MAX, ins, 8, &n);
    for (i = 0; i < n; i++)
    {
        std::printf("  %04X:%04X  %s\n", ins[i].seg, ins[i].off, ins[i].text);
    }
    {
        const char *con = rex_session_con_out(s);
        if ((con != nullptr) && (con[0] != '\0'))
        {
            std::printf("CON: %s\n", con);
        }
    }
    return 0;
}

static int maybe_ghidra(const tdx_cli *cli)
{
    std::string cmd;
    if ((cli == nullptr) || (!cli->ghidra) || cli->input.empty())
    {
        return 0;
    }
    cmd = "sh scripts/ghidra_export.sh \"";
    cmd += cli->input;
    cmd += "\"";
    rex_logf(REX_LOG_INFO, "ghidra: %s", cmd.c_str());
    return std::system(cmd.c_str());
}

int main(int argc, char **argv)
{
    tdx_cli cli{};
    (void)std::signal(SIGPIPE, SIG_IGN);
    rex_session *s = nullptr;
    rex_sock *sk = nullptr;
    FILE *logfp = nullptr;
    int rc = 0;
    const char *cwd = nullptr;

    if (!tdx_cli_parse(argc, argv, &cli) || cli.usage_error)
    {
        tdx_print_usage(stderr);
        return 2;
    }
    if (cli.help)
    {
        tdx_print_usage(stdout);
        return 0;
    }
    if (cli.version)
    {
        std::printf("tdx %s\n", TDX_VERSION_STRING);
        return 0;
    }
    if (cli.input.empty() && cli.floppy.empty() && cli.uc_floppy.empty() && cli.bios.empty())
    {
        tdx_print_usage(stderr);
        return 2;
    }
    if (cli.verbose)
    {
        rex_log_set_level(REX_LOG_DEBUG);
    }
    if (!cli.log_file.empty())
    {
        logfp = std::fopen(cli.log_file.c_str(), "w");
        if (logfp == nullptr)
        {
            std::fprintf(stderr, "tdx: cannot write %s\n", cli.log_file.c_str());
            return 2;
        }
        rex_log_set_file(logfp);
    }

    s = rex_session_create();
    assert(s != nullptr);
    cwd = cli.cwd.empty() ? nullptr : cli.cwd.c_str();
    if (!cli.bios.empty())
    {
        rc = (int)rex_session_load_bios(s, cli.bios.c_str());
        if ((rc == (int)REX_OK) && (!cli.floppy.empty()))
        {
            rc = (int)rex_session_attach_floppy(s, cli.floppy.c_str());
        }
        if ((rc == (int)REX_OK) && (!cli.floppy_b.empty()))
        {
            rc = (int)rex_session_attach_floppy_b(s, cli.floppy_b.c_str());
        }
        if ((rc == (int)REX_OK) && cli.mda)
        {
            rex_session_set_mda(s, 1);
        }
    }
    else if (!cli.uc_floppy.empty())
    {
        rc = (int)rex_session_load_floppy_uc(s, cli.uc_floppy.c_str());
    }
    else if (!cli.floppy.empty())
    {
        rc = (int)rex_session_load_floppy(s, cli.floppy.c_str());
        if ((rc == (int)REX_OK) && (!cli.floppy_b.empty()))
        {
            rc = (int)rex_session_attach_floppy_b(s, cli.floppy_b.c_str());
        }
    }
    else
    {
        rc = (int)rex_session_load(s, cli.input.c_str(), cwd);
    }
    if ((rc == (int)REX_OK) && !cli.exec_map.empty())
    {
        rc = (int)rex_session_set_exec_map(s, cli.exec_map.c_str());
    }
    if (rc != (int)REX_OK)
    {
        std::fprintf(stderr, "tdx: load failed: %s\n", rex_status_str((rex_status)rc));
        rex_session_destroy(s);
        rex_log_close_file();
        return 1;
    }
    maybe_ghidra(&cli);
    if (!cli.symbols.empty())
    {
        rex_symbols_load(s, cli.symbols.c_str());
    }
    else
    {
        /* default: <stem>.sym next to the binary if present */
        std::string stem = cli.input;
        const auto dot = stem.rfind('.');
        if (dot != std::string::npos)
        {
            stem = stem.substr(0, dot) + ".sym";
            rex_symbols_load(s, stem.c_str()); /* ignore missing */
        }
    }

    if (!cli.no_sock)
    {
        sk = rex_sock_listen(cli.sock_path.c_str());
        if (sk == nullptr)
        {
            std::fprintf(stderr, "tdx: warning: cannot bind %s\n", cli.sock_path.c_str());
        }
    }

    if (cli.run)
    {
        rex_session_run(s, 0);
    }

    if (cli.no_ui)
    {
        print_headless(s);
        if (sk != nullptr)
        {
            while ((!rex_sock_quit_requested(sk)) && (!rex_session_halted(s)) &&
                   (rex_session_stop_reason(s) != REX_STOP_FAULT))
            {
                rex_sock_poll(sk, s);
                if (cli.run)
                {
                    rex_session_run(s, 20000);
                }
                else
                {
                    usleep(20000);
                }
            }
            print_headless(s);
        }
        {
            const int code = rex_session_halted(s) ? rex_session_exit_code(s) : 0;
            rex_sock_close(sk);
            rex_session_destroy(s);
            rex_log_close_file();
            return code;
        }
    }

    rc = tdx_ui_run(s, sk, &cli);
    rex_sock_close(sk);
    rex_session_destroy(s);
    rex_log_close_file();
    return rc;
}
