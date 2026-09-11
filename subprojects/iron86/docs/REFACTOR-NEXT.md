# iron86 refactor (next session)

Checkpoint: `checkpoint/20260911T000421Z-pre-iron86-refactor` @ `b9f9f50` (moved forward after this note).

## Keyboard (do this first)

- **Default with IBM ROM:** do **not** steal INT 16. SDL/tdxctl inject **XT make + delayed break** → IRQ1 → ROM INT 9 → BDA 40:1A → ROM INT 16.
- **Steal INT 16** only if `!rom_bios()` (FloppyOS) or `--steal-int16`.
- v8 added passthru in the **wrong** `bios_int` branch; v11 (`b9f9f50`) fixes the `rom_bios()` block. `--log` must show `INT16 passthru` not steal.
- Idle sleep only after DOS paints (`new date` / `A>`), and **not** while `scanq_` / IRQ1 pending (v10).
- CON CHRIN is INT 16 **AH=10h** (MS-DOS 3.3+). C++ steal must alias 10/11 if used.

## Display

- Standalone SDL2 (`display_sdl.cpp`): IBM 5788005, 4:3 `--scale`, `--headless`.
- tdx **links** iron86; no child `--headless`.
- Do not `clock_gettime` per instruction. Present ~60 Hz. Vsync off. Redraw on EXPOSE.

## Split (pc.cpp ~2.5k)

Candidate TUs: `ppi.cpp` / `pic.cpp` / `fdc.cpp` / `int16` vs ROM. Keep `fpu.cpp` as the chip pattern. Do not dump 8087 into `cpu.cpp`.

## Leave

tdx F9 slice speed; FloppyOS←MS-DOS source; 286; Hercules; TTY text mode.
