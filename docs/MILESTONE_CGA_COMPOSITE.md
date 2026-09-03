# Milestone: CGA composite works (Dragon Wars)

**Date:** 2026-09-04  
**Commit:** `a66b0aa` FEATURE v1 Dragon Wars 720K B: CGA composite and INT 21 LSEEK  
**Tag:** `milestone/cga-composite-works`  
**Branch:** `milestone/bios-flopfs-dir`

## What “works” means

1. **Virtual floppy B:** host directory `games/DRGNWARS` packed as FlopFS
   **720K** (80/2/9) because ~708K does not fit 360K.
2. **FloppyOS** loads `DRAGON.COM` from B:, opens `DATA1` (`AH=3D`),
   **`AH=42` LSEEK**, `AH=3F` read — intro lands in B800.
3. **tdxview** default is **old-CGA NTSC** (Reenigne / 86Box `vid_cga_comp.c`),
   320×200 mode 4 (PCBIOS.ASM M7 `3D8=2Ah`, burst on). `--no-composite` = RGBI.
4. **Proof:** `tdxctl dump cga` → 16384 bytes **byte-identical** to the DOSBox
   dump `games/SCREEN.CGA`. tdxview shows blue dragon, red warrior, Interplay.

## Bugs that blocked it

| Symptom | Cause | Fix |
|---------|--------|-----|
| `Fatal error : Out of memory` | `AH=4A` coalesce marked owned MCB `'Z'` | Keep `'M'` on owned blocks |
| 256K “not enough” | PPI I/O nibble `0x06` | `0x0F` → **544K** (24-APR-81 BIOS max) |
| Intro never in B800 (`EE`/`00`) | No **`AH=42` LSEEK** | FloppyOS `dos_lseek` |
| Grey/wrong colors | 4-bit lookup table | Reenigne Composite_Process |
| 640-wide stretch | Mode 4 decoded as 640×200 | Stay **320×200**, scale 2 |

## Not in this milestone

- 1.2M / 1.44M floppy packer or CHS auto-detect
- Recursive subdirs on virtual floppies (32 8.3 names, one directory)
- Trees larger than 720K (next: **XT HDD as C:**)
- FDC Write / Format
- `AH=40` write (saves / `dragon -s` persist)

## How to replay

See `docs/HOWTO_DRAGON_WARS.md`.
