/**
 * @file tdx_ibm_font.h
 * @brief IBM 5150 fonts: BIOS CRT_CHAR_GEN 8×8 and MDA/CGA card ROM 5788005.
 *
 * Card ROM is 8K (AM9264): MDA 8×14 in the first 4K, CGA 8×8 thick in the
 * last 2K. Load from disk at runtime; do not push the BIN to GitHub.
 */
#ifndef TDX_IBM_FONT_H
#define TDX_IBM_FONT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** True if 1024-byte blob matches IBM CRT_CHAR_GEN (char 0x01 = 7E 81 …). */
int tdx_ibm_font_looks_cga8(const uint8_t *p, size_t n);

/**
 * @brief Load IBM 5788005 8K MDA/CGA character ROM.
 * @param path File, or NULL to search ROM/ and $TDX_MDA_FONT.
 * @return 1 on success (8192-byte file).
 */
int tdx_ibm_font_load_5788005(const char *path);

int tdx_ibm_font_mda_loaded(void);

/** MDA scanline 0..13 for CP437 @p ch (8 bits, MSB left). */
uint8_t tdx_ibm_font_mda_row(uint8_t ch, int row);

/** CGA thick 8×8 from the same ROM (256 glyphs), or 0 if not loaded. */
int tdx_ibm_font_cga8_loaded(void);
uint8_t tdx_ibm_font_cga8_row(uint8_t ch, int row);

#ifdef __cplusplus
}
#endif

#endif /* TDX_IBM_FONT_H */
