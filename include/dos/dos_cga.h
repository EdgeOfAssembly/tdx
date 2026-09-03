/**
 * @file dos_cga.h
 * @brief CGA mode 4/5 (320×200×4) pack/unpack from B800:0000 layout.
 */
#ifndef DOS_CGA_H
#define DOS_CGA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* See mz_parse.h for why: dual linkage when compiled as C++ (one-gulp). */
#ifndef REX_C_DEF
#  ifdef __cplusplus
#    define REX_C_DEF extern "C"
#  else
#    define REX_C_DEF
#  endif
#endif

enum
{
    DOS_CGA_WIDTH = 320,
    DOS_CGA_HEIGHT = 200,
    DOS_CGA_PIXELS = 320 * 200,
    DOS_CGA_VRAM = 16384,
    DOS_CGA_HIRES_WIDTH = 640,
    DOS_CGA_HIRES_PIXELS = 640 * 200
};

/**
 * @brief Decode interleaved CGA VRAM into linear indices 0..3.
 *
 * Even scanlines at @p vram+0, odd at @p vram+0x2000, 80 bytes/line, 2 bits/pixel.
 *
 * @param[in]  vram      At least 16 KiB (B800:0000).
 * @param[out] px        320*200 bytes.
 * @param[in]  px_size   Must be >= DOS_CGA_PIXELS.
 *
 * @return 0 on success, -1 on bad args.
 */
int dos_cga_decode(const uint8_t *vram, uint8_t *px, size_t px_size);

/**
 * @brief Inverse of @c dos_cga_decode (for tests).
 */
int dos_cga_encode(const uint8_t *px, size_t px_size, uint8_t *vram, size_t vram_size);

/**
 * @brief Decode CGA mode 6 (640×200 1bpp) into 0/1 bytes.
 *
 * Same even/odd 8K banks as mode 4, 80 bytes/line, 1 bit/pixel MSB first.
 *
 * @param[in]  vram     At least 16 KiB (B800:0000).
 * @param[out] px       640*200 bytes, each 0 or 1.
 * @param[in]  px_size  Must be >= DOS_CGA_HIRES_PIXELS.
 *
 * @return 0 on success, -1 on bad args.
 */
int dos_cga_decode_hires(const uint8_t *vram, uint8_t *px, size_t px_size);

/**
 * @brief Old-CGA NTSC (Reenigne / 86Box Composite_Process) from mode-6 VRAM.
 *
 * @param cga3d8  Mode-set (PCBIOS M7; mode 6 = 1Eh). 0 → 1Eh.
 * @param cga3d9  Color-select. 0 → 0Fh (white fg).
 */
int dos_cga_composite_argb(const uint8_t *vram, uint32_t *argb, size_t n_pixels,
                           uint8_t cga3d8, uint8_t cga3d9);

/**
 * @brief Old-CGA NTSC from mode 4/5 VRAM (320×200 2bpp).
 *
 * PCBIOS M7 mode 4 = 3D8 2Ah (burst on). Gold: games/SCREEN.CGA (Dragon Wars).
 *
 * @param cga3d8  0 → 2Ah. @param cga3d9  0 → 30h (pal1 + intensity).
 */
int dos_cga_composite_argb320(const uint8_t *vram, uint32_t *argb, size_t n_pixels,
                              uint8_t cga3d8, uint8_t cga3d9);

/**
 * @brief Map CGA color-select (port 3D9h / BDA 0040:0066) to four ARGB colors.
 *
 * Index 0 is the background (bits 0–3). Bit 4 is palette intensity, bit 5
 * selects cyan/magenta/white vs green/red/brown. Default BIOS mode-4 value
 * 30h matches the historical tdx cyan/magenta/white table.
 *
 * Header-inline so tdxview (no dos_cga.c) and tdx share one mapping.
 *
 * @param[in]  color_select  3D9h register.
 * @param[out] out           Four ARGB8888 colors; must not be NULL.
 */
static inline void dos_cga_palette_argb(uint8_t color_select, uint32_t out[4])
{
    static const uint32_t irgb[16] = {
        0xFF000000u, 0xFF0000A8u, 0xFF00A800u, 0xFF00A8A8u, 0xFFA80000u, 0xFFA800A8u,
        0xFFA85400u, 0xFFA8A8A8u, 0xFF545454u, 0xFF5454FCu, 0xFF54FC54u, 0xFF54FCFCu,
        0xFFFC5454u, 0xFFFC54FCu, 0xFFFCFC54u, 0xFFFCFCFCu};
    const unsigned hi = (color_select & 0x10u) ? 8u : 0u;

    if (out == NULL)
    {
        return;
    }
    out[0] = irgb[color_select & 0x0Fu];
    if ((color_select & 0x20u) != 0u)
    {
        out[1] = irgb[3u + hi];
        out[2] = irgb[5u + hi];
        out[3] = irgb[7u + hi];
    }
    else
    {
        out[1] = irgb[2u + hi];
        out[2] = irgb[4u + hi];
        out[3] = irgb[6u + hi];
    }
    if ((color_select & 0x3Fu) == 0x30u)
    {
        out[1] = 0xFF55FFFFu;
        out[2] = 0xFFFF55FFu;
        out[3] = 0xFFFFFFFFu;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* DOS_CGA_H */
