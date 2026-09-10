/**
 * @file tdx_cli.h
 * @brief TDX command-line options (order-independent).
 */
#ifndef TDX_CLI_H
#define TDX_CLI_H

#include "iron86/config.h"

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
    std::string bios;      /**< IBM 5150 8K or 5160 40K/64K BIOS. Default with --floppy-a. */
    bool no_bios = false;  /**< --no-bios: iron86 C++ INT 10/13/16 only. */
    bool no_fast_post = false; /**< --no-fast-post: cold POST from FFFF:0000. */
    std::string hdc;       /**< Raw HDD image (Xebec Type-1). Regular file only. */
    std::string hdc_rom;   /**< Xebec option ROM at C8000. */
    std::string exec_map;  /**< 1 MiB executed-opcode map (iron86). */
    bool mda = false;      /**< SW1 MDA 80×25 (B000, mode 7). Default CGA. */
    int scale = 2;
    unsigned prefetch_bytes = 6; /**< iron86 BIU queue; 0=off. Default 8086. */
    bool prefetch_set = false;
    iron86::machine_preset preset = iron86::machine_preset::ibm_pc;
    bool preset_set = false;
};

bool tdx_cli_parse(int argc, char **argv, tdx_cli *out);
void tdx_print_usage(FILE *fp);

#endif /* TDX_CLI_H */
