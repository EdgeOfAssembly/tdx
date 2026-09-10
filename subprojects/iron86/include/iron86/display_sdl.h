/**
 * @file display_sdl.h
 * @brief Standalone iron86 SDL2 window (text + CGA graphics). Not used by tdx.
 *
 * tdx links the CPU/PC library and owns its own windows. This module is
 * linked only into the `iron86` CLI. `--headless` / `--until-prompt` skip it.
 */
#ifndef IRON86_DISPLAY_SDL_H
#define IRON86_DISPLAY_SDL_H

#include "iron86/cpu.h"
#include "iron86/pc.h"

namespace iron86
{

/**
 * @brief Open a 4:3 letterboxed window. @p scale is integer 1..8 (default 2).
 * @return false if SDL is missing or init failed (caller stays headless).
 */
bool display_sdl_open(int scale);

/** @brief Blit B800/B000 text or CGA bitmap from guest RAM. */
void display_sdl_present(cpu &c, bool mda);

/**
 * @brief Pump SDL events; inject keys via @p p (no extra policy).
 * @return false if the user closed the window or pressed Alt+F4 / Esc+Shift.
 */
bool display_sdl_poll(pc &p);

void display_sdl_close();

} // namespace iron86

#endif /* IRON86_DISPLAY_SDL_H */
