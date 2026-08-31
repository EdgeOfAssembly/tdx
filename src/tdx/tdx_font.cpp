/**
 * @file tdx_font.cpp
 * @brief CP850 8×16 console font (generated).
 */

#include "tdx/tdx_font.h"

#include "tdx/tdx_font_data.inc"

const uint8_t *tdx_font_glyph(uint8_t ch)
{
    return TDX_FONT_8X16[ch];
}
