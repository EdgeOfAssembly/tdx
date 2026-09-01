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
    DOS_CGA_VRAM = 16384
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

#ifdef __cplusplus
}
#endif

#endif /* DOS_CGA_H */
