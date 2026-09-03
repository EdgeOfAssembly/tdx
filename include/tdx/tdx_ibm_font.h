/**
 * @file tdx_ibm_font.h
 * @brief IBM 5150 BIOS CRT_CHAR_GEN 8×8 (F000:FA6E, 128 glyphs).
 *
 * Not the MDA card's 8K ROM (separate adapter dump, still TODO). Do not
 * commit BIOS bytes; load at runtime from guest memory.
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

#ifdef __cplusplus
}
#endif

#endif /* TDX_IBM_FONT_H */
