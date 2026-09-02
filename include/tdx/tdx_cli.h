/**
 * @file tdx_cli.h
 * @brief TDX command-line options (order-independent).
 */
#ifndef TDX_CLI_H
#define TDX_CLI_H

#include <cstdio>
#include <string>

struct tdx_cli
{
    bool help = false;
    bool version = false;
    bool usage_error = false;
    bool no_ui = false;
    bool game = false; /**< In-process CGA window (default off; use tdxview). */
    bool no_sock = false;
    bool run = false;
    bool ghidra = false;
    bool verbose = false;
    std::string sock_path = "/tmp/tdx.sock";
    std::string log_file;
    std::string symbols;
    std::string input;
    std::string cwd;
    std::string floppy;    /**< A: image or dir (--floppy / --floppy-a). */
    std::string floppy_b;  /**< B: image or dir (--floppy-b). */
    std::string uc_floppy; /**< Same image on Unicorn (A/B vs iron86). */
    std::string bios;      /**< IBM 5150 8K BIOS (iron86, Py86 mapping). */
    int scale = 2;
};

bool tdx_cli_parse(int argc, char **argv, tdx_cli *out);
void tdx_print_usage(FILE *fp);

#endif /* TDX_CLI_H */
