/**
 * @file dos_cga.c
 * @brief CGA 320×200 2bpp, mode-6 1bpp, and old-CGA composite decode.
 */

#include "dos/dos_cga.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

REX_C_DEF int dos_cga_decode(const uint8_t *vram, uint8_t *px, size_t px_size)
{
    int y = 0;
    int x = 0;

    if ((vram == NULL) || (px == NULL) || (px_size < (size_t)DOS_CGA_PIXELS))
    {
        return -1;
    }
    for (y = 0; y < DOS_CGA_HEIGHT; y++)
    {
        const uint8_t *src = vram + ((y & 1) ? 0x2000 : 0) + (size_t)(y / 2) * 80u;
        for (x = 0; x < DOS_CGA_WIDTH; x++)
        {
            const int byte_i = x / 4;
            const int shift = 6 - (x % 4) * 2;
            px[(size_t)y * (size_t)DOS_CGA_WIDTH + (size_t)x] =
                (uint8_t)((src[byte_i] >> shift) & 3);
        }
    }
    return 0;
}

REX_C_DEF int dos_cga_encode(const uint8_t *px, size_t px_size, uint8_t *vram, size_t vram_size)
{
    int y = 0;
    int x = 0;

    if ((px == NULL) || (vram == NULL) || (px_size < (size_t)DOS_CGA_PIXELS) ||
        (vram_size < (size_t)DOS_CGA_VRAM))
    {
        return -1;
    }
    memset(vram, 0, (size_t)DOS_CGA_VRAM);
    for (y = 0; y < DOS_CGA_HEIGHT; y++)
    {
        uint8_t *dst = vram + ((y & 1) ? 0x2000 : 0) + (size_t)(y / 2) * 80u;
        for (x = 0; x < DOS_CGA_WIDTH; x++)
        {
            const int byte_i = x / 4;
            const int shift = 6 - (x % 4) * 2;
            const uint8_t c =
                (uint8_t)(px[(size_t)y * (size_t)DOS_CGA_WIDTH + (size_t)x] & 3u);
            dst[byte_i] = (uint8_t)(dst[byte_i] | (uint8_t)(c << shift));
        }
    }
    return 0;
}

REX_C_DEF int dos_cga_decode_hires(const uint8_t *vram, uint8_t *px, size_t px_size)
{
    int y = 0;
    int x = 0;

    if ((vram == NULL) || (px == NULL) || (px_size < (size_t)DOS_CGA_HIRES_PIXELS))
    {
        return -1;
    }
    for (y = 0; y < DOS_CGA_HEIGHT; y++)
    {
        const uint8_t *src = vram + ((y & 1) ? 0x2000 : 0) + (size_t)(y / 2) * 80u;
        for (x = 0; x < DOS_CGA_HIRES_WIDTH; x++)
        {
            const int byte_i = x / 8;
            const int shift = 7 - (x % 8);
            px[(size_t)y * (size_t)DOS_CGA_HIRES_WIDTH + (size_t)x] =
                (uint8_t)((src[byte_i] >> shift) & 1);
        }
    }
    return 0;
}

/*
 * Old-CGA NTSC: Reenigne filter as in 86Box vid_cga_comp.c (DOSBox patch).
 * Gold proof: games/SCREEN.CGA (Dragon Wars intro) → blue dragon, red warrior.
 */
static const unsigned char k_chroma_mux[256] = {
    2,   2,   2,   2,   114, 174, 4,   3,   2,   1,   133, 135, 2,   113, 150, 4,
    133, 2,   1,   99,  151, 152, 2,   1,   3,   2,   96,  136, 151, 152, 151, 152,
    2,   56,  62,  4,   111, 250, 118, 4,   0,   51,  207, 137, 1,   171, 209, 5,
    140, 50,  54,  100, 133, 202, 57,  4,   2,   50,  153, 149, 128, 198, 198, 135,
    32,  1,   36,  81,  147, 158, 1,   42,  33,  1,   210, 254, 34,  109, 169, 77,
    177, 2,   0,   165, 189, 154, 3,   44,  33,  0,   91,  197, 178, 142, 144, 192,
    4,   2,   61,  67,  117, 151, 112, 83,  4,   0,   249, 255, 3,   107, 249, 117,
    147, 1,   50,  162, 143, 141, 52,  54,  3,   0,   145, 206, 124, 123, 192, 193,
    72,  78,  2,   0,   159, 208, 4,   0,   53,  58,  164, 159, 37,  159, 171, 1,
    248, 117, 4,   98,  212, 218, 5,   2,   54,  59,  93,  121, 176, 181, 134, 130,
    1,   61,  31,  0,   160, 255, 34,  1,   1,   58,  197, 166, 0,   177, 194, 2,
    162, 111, 34,  96,  205, 253, 32,  1,   1,   57,  123, 125, 119, 188, 150, 112,
    78,  4,   0,   75,  166, 180, 20,  38,  78,  1,   143, 246, 42,  113, 156, 37,
    252, 4,   1,   188, 175, 129, 1,   37,  118, 4,   88,  249, 202, 150, 145, 200,
    61,  59,  60,  60,  228, 252, 117, 77,  60,  58,  248, 251, 81,  212, 254, 107,
    198, 59,  58,  169, 250, 251, 81,  80,  100, 58,  154, 250, 251, 252, 252, 252};
static const double k_intensity[4] = {77.175381, 88.654656, 166.564623, 174.228438};

static int g_comp_table[1024];
static int g_ri, g_rq, g_gi, g_gq, g_bi, g_bq, g_sharp;
static uint8_t g_inited_mode, g_inited_col;
static int g_inited;

static void cga_comp_setup(uint8_t cgamode, uint8_t cgacol)
{
    const double tau = 6.28318531;
    double min_v = 0;
    double max_v = 0;
    double mode_contrast = 0;
    double mode_brightness = 0;
    double mode_hue = 0;
    double mode_sat = 0;
    double i = 0;
    double q = 0;
    double a = 0;
    double c = 0;
    double s = 0;
    double r = 0;
    double iq_i = 0;
    double iq_q = 0;
    unsigned x = 0;

    if ((g_inited != 0) && (g_inited_mode == cgamode) && (g_inited_col == cgacol))
    {
        return;
    }
    min_v = (double)k_chroma_mux[0] + k_intensity[0];
    max_v = (double)k_chroma_mux[255] + k_intensity[3];
    mode_contrast = 256.0 / (max_v - min_v);
    mode_brightness = -min_v * mode_contrast;
    mode_hue = ((cgamode & 3u) == 1u) ? 14.0 : 4.0;
    mode_sat = 2.9;
    for (x = 0; x < 1024u; x++)
    {
        const int phase = (int)(x & 3u);
        const int right = (int)((x >> 2) & 15u);
        const int left = (int)((x >> 6) & 15u);
        const double ch =
            (double)k_chroma_mux[(unsigned)(((left & 7) << 5) | ((right & 7) << 2) | phase)];
        const double inten = k_intensity[(left >> 3) | ((right >> 2) & 2)];
        g_comp_table[x] = (int)((ch + inten) * mode_contrast + mode_brightness);
    }
    i = (double)(g_comp_table[6 * 68] - g_comp_table[6 * 68 + 2]);
    q = (double)(g_comp_table[6 * 68 + 1] - g_comp_table[6 * 68 + 3]);
    a = tau * (33.0 + 90.0 + mode_hue) / 360.0;
    c = cos(a);
    s = sin(a);
    r = 256.0 * mode_sat / sqrt(i * i + q * q);
    iq_i = -(i * c + q * s) * r;
    iq_q = (q * c - i * s) * r;
    g_ri = (int)(0.9563 * iq_i + 0.6210 * iq_q);
    g_rq = (int)(-0.9563 * iq_q + 0.6210 * iq_i);
    g_gi = (int)(-0.2721 * iq_i + -0.6474 * iq_q);
    g_gq = (int)(0.2721 * iq_q + -0.6474 * iq_i);
    g_bi = (int)(-1.1069 * iq_i + 1.7046 * iq_q);
    g_bq = (int)(1.1069 * iq_q + 1.7046 * iq_i);
    g_sharp = 0;
    g_inited_mode = cgamode;
    g_inited_col = cgacol;
    g_inited = 1;
}

static uint8_t byte_clamp(int v)
{
    v >>= 13;
    if (v < 0)
    {
        return 0;
    }
    if (v > 255)
    {
        return 255;
    }
    return (uint8_t)v;
}

/** @brief RGBI samples (low 4 bits) → ARGB8888, length n (multiple of 4). */
static void cga_comp_line(const uint8_t *rgbi, int n, uint8_t border, uint32_t *out)
{
    int temp[660];
    int atemp[660];
    int btemp[660];
    int *o = temp;
    int *ap = NULL;
    int *bp = NULL;
    int *ip = NULL;
    int x = 0;
    int blocks = 0;

    if (n > 640)
    {
        n = 640;
    }
    blocks = n / 4;
    for (x = 0; x < 4; x++)
    {
        *o++ = g_comp_table[border * 68 + ((x + 3) & 3)];
    }
    *o++ = g_comp_table[(border << 6) | ((rgbi[0] & 15) << 2) | 3];
    for (x = 0; x < n - 1; x++)
    {
        *o++ = g_comp_table[((rgbi[x] & 15) << 6) | ((rgbi[x + 1] & 15) << 2) | (x & 3)];
    }
    *o++ = g_comp_table[((rgbi[n - 1] & 15) << 6) | (border << 2) | 3];
    for (x = 0; x < 5; x++)
    {
        *o++ = g_comp_table[border * 68 + (x & 3)];
    }
    ip = temp + 4;
    ap = atemp + 1;
    bp = btemp + 1;
    for (x = -1; x < n + 1; x++)
    {
        ap[x] = ip[-4] - ((ip[-2] - ip[0] + ip[2]) << 1) + ip[4];
        bp[x] = (ip[-3] - ip[-1] + ip[1] - ip[3]) << 1;
        ip++;
    }
    ip = temp + 5;
    ip[-1] = (ip[-1] << 3) - ap[-1];
    ip[0] = (ip[0] << 3) - ap[0];
    for (x = 0; x < blocks; x++)
    {
        int k = 0;
        const int iq[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        (void)iq;
        for (k = 0; k < 4; k++)
        {
            const int I = (k == 0) ? ap[0] : ((k == 1) ? -bp[0] : ((k == 2) ? -ap[0] : bp[0]));
            const int Q = (k == 0) ? bp[0] : ((k == 1) ? ap[0] : ((k == 2) ? -bp[0] : -ap[0]));
            int aa = 0;
            int bb = 0;
            int cc = 0;
            int dd = 0;
            int y = 0;
            int rr = 0;
            int gg = 0;
            int bl = 0;
            ip[1] = (ip[1] << 3) - ap[1];
            aa = ap[0];
            bb = bp[0];
            (void)aa;
            (void)bb;
            cc = ip[0] + ip[0];
            dd = ip[-1] + ip[1];
            y = ((cc + dd) << 8) + g_sharp * (cc - dd);
            rr = y + g_ri * I + g_rq * Q;
            gg = y + g_gi * I + g_gq * Q;
            bl = y + g_bi * I + g_bq * Q;
            ip++;
            ap++;
            bp++;
            *out++ = 0xFF000000u | ((uint32_t)byte_clamp(rr) << 16) |
                     ((uint32_t)byte_clamp(gg) << 8) | (uint32_t)byte_clamp(bl);
        }
    }
}

static void cga_rgbi4(uint8_t cga3d9, uint8_t pal[4])
{
    const unsigned hi = (cga3d9 & 0x10u) ? 8u : 0u;
    pal[0] = (uint8_t)(cga3d9 & 0x0Fu);
    if ((cga3d9 & 0x20u) != 0u)
    {
        pal[1] = (uint8_t)(3u + hi);
        pal[2] = (uint8_t)(5u + hi);
        pal[3] = (uint8_t)(7u + hi);
    }
    else
    {
        pal[1] = (uint8_t)(2u + hi);
        pal[2] = (uint8_t)(4u + hi);
        pal[3] = (uint8_t)(6u + hi);
    }
}

REX_C_DEF int dos_cga_composite_argb(const uint8_t *vram, uint32_t *argb, size_t n_pixels,
                                     uint8_t cga3d8, uint8_t cga3d9)
{
    uint8_t rgbi[640];
    uint32_t line[640];
    int y = 0;
    int x = 0;
    const uint8_t fg = (cga3d9 != 0) ? (uint8_t)(cga3d9 & 0x0Fu) : 0x0Fu;

    if ((vram == NULL) || (argb == NULL) || (n_pixels < (size_t)DOS_CGA_HIRES_PIXELS))
    {
        return -1;
    }
    if (cga3d8 == 0)
    {
        cga3d8 = 0x1E;
    }
    cga_comp_setup(cga3d8, cga3d9);
    for (y = 0; y < DOS_CGA_HEIGHT; y++)
    {
        const uint8_t *src = vram + ((y & 1) ? 0x2000 : 0) + (size_t)(y / 2) * 80u;
        for (x = 0; x < DOS_CGA_HIRES_WIDTH; x++)
        {
            const int bit = (src[x / 8] >> (7 - (x % 8))) & 1;
            rgbi[x] = bit ? fg : 0;
        }
        cga_comp_line(rgbi, 640, (uint8_t)(cga3d9 & 0x0Fu), line);
        memcpy(argb + (size_t)y * 640u, line, 640u * sizeof(uint32_t));
    }
    return 0;
}

REX_C_DEF int dos_cga_composite_argb320(const uint8_t *vram, uint32_t *argb, size_t n_pixels,
                                        uint8_t cga3d8, uint8_t cga3d9)
{
    uint8_t pal[4];
    uint8_t rgbi[640];
    uint32_t line[640];
    int y = 0;
    int x = 0;

    if ((vram == NULL) || (argb == NULL) || (n_pixels < (size_t)DOS_CGA_PIXELS))
    {
        return -1;
    }
    if (cga3d8 == 0)
    {
        cga3d8 = 0x2A;
    }
    if (cga3d9 == 0)
    {
        cga3d9 = 0x30;
    }
    cga_rgbi4(cga3d9, pal);
    cga_comp_setup(cga3d8, cga3d9);
    for (y = 0; y < DOS_CGA_HEIGHT; y++)
    {
        const uint8_t *src = vram + ((y & 1) ? 0x2000 : 0) + (size_t)(y / 2) * 80u;
        uint32_t *dst = argb + (size_t)y * (size_t)DOS_CGA_WIDTH;
        for (x = 0; x < DOS_CGA_WIDTH; x++)
        {
            const int shift = 6 - 2 * (x % 4);
            const uint8_t pix = (uint8_t)((src[x / 4] >> shift) & 3u);
            rgbi[x * 2] = pal[pix];
            rgbi[x * 2 + 1] = pal[pix];
        }
        cga_comp_line(rgbi, 640, (uint8_t)(cga3d9 & 0x0Fu), line);
        for (x = 0; x < DOS_CGA_WIDTH; x++)
        {
            const uint32_t a = line[x * 2];
            const uint32_t b = line[x * 2 + 1];
            const unsigned r = (((a >> 16) & 255u) + ((b >> 16) & 255u)) / 2u;
            const unsigned g = (((a >> 8) & 255u) + ((b >> 8) & 255u)) / 2u;
            const unsigned bl = ((a & 255u) + (b & 255u)) / 2u;
            dst[x] = 0xFF000000u | (r << 16) | (g << 8) | bl;
        }
    }
    return 0;
}
