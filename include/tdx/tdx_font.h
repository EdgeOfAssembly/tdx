/**
 * @file tdx_font.h
 * @brief CP850 8×16 glyph atlas for the CPU window.
 */
#ifndef TDX_FONT_H
#define TDX_FONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 16 row bytes, MSB = leftmost pixel. */
const uint8_t *tdx_font_glyph(uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif /* TDX_FONT_H */
