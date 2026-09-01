/**
 * @file dos_cga.c
 * @brief CGA 320×200 2bpp even/odd-bank encode and decode.
 */

#include "dos/dos_cga.h"

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
