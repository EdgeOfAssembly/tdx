/**
 * @file tdx_shot.h
 * @brief Xmux-style auto-versioned screenshot paths.
 */
#ifndef TDX_SHOT_H
#define TDX_SHOT_H

#include <string>

/**
 * @brief Insert @c -YYYYMMDDTHHMMSS.mmm before the extension.
 *
 * Same-millisecond collisions get @c -2, @c -3, … like Xmux.
 * Empty @p path becomes @c /tmp/tdx.bmp. Unknown extensions stay on the stem
 * and default to @c .bmp (SDL_SaveBMP).
 */
std::string tdx_shot_versioned_path(const std::string &path);

#endif /* TDX_SHOT_H */
