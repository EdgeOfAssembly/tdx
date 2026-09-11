# iron86 next session (keyboard + BIOS rebuild)

Checkpoint: `checkpoint/20260911T000421Z-pre-iron86-refactor` (see `git log -1`).

## Tomorrow (order)

1. **Assemble IBM 5160 BIOS with UASM** and boot it.
   - **Assemblable source (yes):** `/mnt/linux/docs/research/IBM-PC-BIOS/IBM PC XT/XT BIOS V3/`
     (`POST.ASM`, **`KEYBRD.ASM` TITLE 01/10/86**, `VIDEO.ASM`, `DSKETTE.ASM`, `HEADER.ASM`, `FILL.ASM`, `XTLINK`). Originally IBM MASM 2.0; we use **UASM** (`uasm`, 8086).
   - **Not assemblable (listings only):** `docs/bios/xt5160_10jan86_64k.ndisasm.lst` + `labels.md` (ndisasm of the **binary** we already boot).
   - **Golden ROM:** `tests/images/hdc-proof/xt5160_10jan86_64k.bin` (SHA256 in `docs/bios/README.md`). Compare UASM output to this; then `tdx`/`tdxview`/`iron86 --preset ibm-xt --bios that.bin`.
2. **Team documents KEYBRD.ASM** — INT 9 (IRQ1, IN 60h, PB7 ack), INT 16 AH=00/01/02/10/11/12, ring 40:1A/1C/1E, make vs break, AL=0 extended prefix. Annotate CS:IP `F000:xxxx`.
3. **One last standalone iron86 keyboard pass** using that document (XT make/break timing vs ROM INT 9). tdx+tdxview already boots DOS 3.31 + `DIR` with current iron86 (`tdxctl type`).

## Keyboard (current)

- **Default with IBM ROM:** do **not** steal INT 16. SDL/tdxctl inject **XT make + delayed break** → IRQ1 → ROM INT 9 → BDA 40:1A → ROM INT 16.
- **Steal INT 16** only if `!rom_bios()` (FloppyOS) or `--steal-int16`.
- v8 added passthru in the **wrong** `bios_int` branch; v11 (`b9f9f50`) / v11 follow-up (`b9f9f50`+`FIXUP v11 actually passthru`) fixes the `rom_bios()` block. `--log` must show `INT16 passthru` not steal.
- Idle sleep only after DOS paints (`new date` / `A>`), and **not** while `scanq_` / IRQ1 pending (v10).
- CON CHRIN is INT 16 **AH=10h** (MS-DOS 3.3+). C++ steal must alias 10/11 if used.

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
