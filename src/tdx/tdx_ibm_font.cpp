/**
 * @file tdx_ibm_font.cpp
 * @brief IBM CRT_CHAR_GEN detect + 5788005 MDA/CGA card ROM loader.
 *
 * Layout (foone/mdafont, vcfed, MAME 5788005.u33):
 *   0x0000  MDA rows 0–7   (256×8)
 *   0x0800  MDA rows 8–13  (256×8, last 2 unused)
 *   0x1000  CGA thin 8×8
 *   0x1800  CGA thick 8×8
 */
#include "tdx/tdx_ibm_font.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint8_t g_mda[256][14];
static uint8_t g_cga8[256][8];
static int g_mda_ok = 0;
static int g_cga8_ok = 0;

int tdx_ibm_font_looks_cga8(const uint8_t *p, size_t n)
{
    static const uint8_t k_smile[8] = {0x7E, 0x81, 0xA5, 0x81, 0xBD, 0x99, 0x81, 0x7E};
    size_t i = 0;
    if ((p == nullptr) || (n < 1024u))
    {
        return 0;
    }
    for (i = 0; i < 8u; i++)
    {
        if (p[i] != 0)
        {
            return 0;
        }
    }
    for (i = 0; i < 8u; i++)
    {
        if (p[8u + i] != k_smile[i])
        {
            return 0;
        }
    }
    return 1;
}

static int load_path(const char *path)
{
    FILE *fp = nullptr;
    uint8_t rom[8192];
    unsigned ch = 0;
    int r = 0;
    if (path == nullptr)
    {
        return 0;
    }
    fp = std::fopen(path, "rb");
    if (fp == nullptr)
    {
        return 0;
    }
    if (std::fread(rom, 1, 8192, fp) != 8192)
    {
        std::fclose(fp);
        return 0;
    }
    std::fclose(fp);
    for (ch = 0; ch < 256u; ch++)
    {
        for (r = 0; r < 8; r++)
        {
            g_mda[ch][r] = rom[ch * 8u + (unsigned)r];
        }
        for (r = 0; r < 6; r++)
        {
            g_mda[ch][8 + r] = rom[0x800u + ch * 8u + (unsigned)r];
        }
        for (r = 0; r < 8; r++)
        {
            g_cga8[ch][r] = rom[0x1800u + ch * 8u + (unsigned)r];
        }
    }
    g_mda_ok = 1;
    g_cga8_ok = 1;
    return 1;
}

int tdx_ibm_font_load_5788005(const char *path)
{
    const char *env = std::getenv("TDX_MDA_FONT");
    if ((path != nullptr) && (path[0] != '\0'))
    {
        return load_path(path);
    }
    if ((env != nullptr) && (env[0] != '\0'))
    {
        return load_path(env);
    }
    if (load_path("ROM/IBM_5788005_AM9264_1981_CGA_MDA_CARD.BIN"))
    {
        return 1;
    }
    return load_path("/mnt/TurboDebugger/ROM/IBM_5788005_AM9264_1981_CGA_MDA_CARD.BIN");
}

int tdx_ibm_font_mda_loaded(void)
{
    return g_mda_ok;
}

uint8_t tdx_ibm_font_mda_row(uint8_t ch, int row)
{
    if ((g_mda_ok == 0) || (row < 0) || (row > 13))
    {
        return 0;
    }
    return g_mda[ch][row];
}

int tdx_ibm_font_cga8_loaded(void)
{
    return g_cga8_ok;
}

uint8_t tdx_ibm_font_cga8_row(uint8_t ch, int row)
{
    if ((g_cga8_ok == 0) || (row < 0) || (row > 7))
    {
        return 0;
    }
    return g_cga8[ch][row];
}
