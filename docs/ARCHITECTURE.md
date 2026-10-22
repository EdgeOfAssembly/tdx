# TDX architecture

```text
  tdx (SDL2 CPU TUI)                             tdxview (SDL2 CGA)
       │                                              │
       ├── rex_session  (step / over / run / bp)      │
       │        └── dos_machine                       │
       │              ├── iron86 (recommended)        │
       │              └── Unicorn 8086 (deprecated)   │
       └── rex_sock  UNIX JSON  ──────────────── cga / key / step
```

`tdx` is the only emulator. `tdxview` is a framebuffer client (polls `{"cmd":"cga"}`).

**Recommended emulator — iron86:** C++23 8086 + IBM PC machine, installed as
**system libiron86** (`pkg-config iron86`). Use `--floppy-a` / `--bios`. Linked
into tdx via `-liron86` (no sibling `IRON86_SRC` embed). Development tree:
`/mnt/iron86`. Guest OS is FloppyOS (`/mnt/FloppyOS`) / MS-DOS.

**Unicorn (deprecated / not recommended):** still used for positional MZ EXE /
`.COM` loads (`USE_UNICORN=1` default). LGPL, embeddable 16-bit step. Build with
`USE_UNICORN=0` to omit the dependency; those APIs return `REX_ERR_NOSYS`.
Capstone 6 disassembles. SDL2 draws a TD-like text UI (not BGI — TD is text
mode; the *guest* may be CGA).

**Step over:** if the current insn is CALL, INT, REP, or LOOP, run until the
fall-through linear address (LOOP = remaining iterations). Inner breakpoints
still fire.

**CGA window:** BIOS mode 04h/05h VRAM at `B800:0000` (even/odd banks) decoded
to 320×200×4. Writes into `0xB8000–0xBFFFF` mark the surface dirty.

**DOS memory:** PSP word at offset 2 is `A000h` (640 KiB), matching DOS 5 with
`max_alloc=FFFFh`. BASCOM reads that before INT 21; a tight image-sized block
makes it RETF to PSP:0000 (INT 20) without SETBLOCK.

**FCB I/O:** INT 21 AH=0Fh/10h/14h/16h/21h/27h (open/close/seq/create/random).
Packed `dos_fcb` (`#pragma pack(push,1)`). Open matches MS-DOS 1.25 IBM:
EXTENT=0, recsize always 128 (set recsize after open). RR is 3 bytes if
recsize≥64. AH=21 does not increment RR; AH=27 sets RR to last+1.

**IBM BDA:** packed `ibm_bda` at linear `0x400` (0040:0000) for mem size, video
mode/cols, CRT palette, timer ticks. No BIOS Parameter Block (boot-sector BPB)
or INT 1Eh diskette table in this tree yet.

**Reset:** Ctrl-F2 / `{"cmd":"reset"}` reloads the same image in-process (keep
breakpoints). Never `xmux run` of the SDL GUI — it blocks until the window
exits.

**Layers (CPU is not DOS):** `tdx`/`tdxview` are the debugger and CGA viewer
(session + UNIX sockets). The machine backend is CPU + platform hardware
(iron86 preferred; Unicorn optional for lone EXE). `rex.h` stays arch-agnostic.
FCB / INT 21 is an **OS personality** (`dos_int.cpp`), not a CPU feature.
Long term the hardware emulator should boot IBM DOS, MS-DOS, or FloppyOS;
an optional thin host DOS shim can still load a lone EXE. Do not put FCB
inside the CPU core.

**Other platforms:** `rex_arch` already has Z80 / 6502 / M68K slots. A future
backend replaces `dos_machine` and keeps `rex_session_*`.
