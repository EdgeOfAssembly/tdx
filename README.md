# TDX — Turbo Debugger X

Native **x86_64 Linux** debugger with a Borland Turbo Debugger–like CPU view.
It is **not** a source port of `TD.EXE` (Borland never shipped TD sources here).
The reusable core is **librex**: load a target, breakpoints, step, step-over,
memory, symbols, agent socket. First backend: **DOS MZ EXE / .COM** on Unicorn
8086 + Capstone.

| Name | What |
|------|------|
| **tdx** | Main driver: load EXE/COM, CPU TUI, step/over/run, socket |
| **tdxview** | CGA user-screen; polls `tdx` over the UNIX socket (own process / Xmux) |
| **librex** | C ABI (`include/rex/rex.h`) for other emulators later (Z80, 6502, M68K, …) |
| **tdxctl** | Agent client for the UNIX socket |

## Build

```text
make -s V=0 -j$(nproc)
make -s test
make -s verify    # CBMC on the MZ parser after tests
```

Needs: gcc/g++, SDL2, capstone, unicorn, Catch2 (`source ~/.local/share/test-frameworks/env.sh`), nasm, optional CBMC and Ghidra 12.

## Run

```text
tdx -h
tdx -v
tdx tests/fixtures/tiny.com --no-ui --no-sock
tdx /mnt/bushido/bushido/BUSHIDO.EXE --cwd /mnt/bushido/bushido
tdx --bios BIOS.BIN --floppy-a floppyos.img --floppy-b /mnt/bushido/bushido
tdx --bios BIOS.BIN --floppy-a floppyos.img --floppy-b games/DRGNWARS   # 720K FlopFS if needed
tdxview --sock /tmp/tdx.sock          # second process: CGA window
```

SPECTATOR — **two Xmux sessions** (CPU and game do not share one DISPLAY):

```text
scripts/tdx-xmux.sh /mnt/bushido/bushido/BUSHIDO.EXE /mnt/bushido/bushido
# SPECTATOR: xmux attach tdx --no-reconnect
# SPECTATOR: xmux attach tdx-game --no-reconnect
```

`tdx` is the driver. `tdxview` only paints whatever framebuffer `tdx` exposes (`cga` on the socket). Optional `--game` still embeds CGA in the tdx process.

Keys (CPU window — VCR tape, CGA follows):

- **F1** help dialog (toggle)
- **↓ / F8** one unit **over** CALL, INT, REP, LOOP (loop/rep runs to completion)
- **F7** one insn **into** CALL
- **↑** reverse one unit (registers + RAM, so the game screen rewinds)
- **PgDn / PgUp** 14 units forward / back
- **Home / End** start / end of the tape
- Jcc/JMP follow **live flags** (not skipped)
- **F9** run/pause (reseeds the tape); **F2** breakpoint; **Ctrl-F2** reset; **Alt-X** quit

Game window: letters, arrows, Enter, Space → INT 16 (and start F9).

**Agents (no Xmux):** keep-alive UNIX sockets.

```text
tdxctl shot                 # CPU BMP, stdout = versioned path
tdxctl --view shot          # CGA window
tdxctl --ctl                # stdin pipeline (KEY/SHOT/nav)
scripts/tdx-start.sh GAME.EXE
```

Sockets: `/tmp/tdx.sock` (CPU) and `/tmp/tdxview.sock` (game). Screenshot names
match Xmux: `stem-YYYYMMDDTHHMMSS.mmm.bmp`.

Reload after halt is **in-process** (`tdxctl reset` or Ctrl-F2). Never `xmux run` of
`tdx` — that blocks until the SDL window exits.

Agent:

```text
tdxctl --sock /tmp/tdx.sock regs
tdxctl --sock /tmp/tdx.sock step
tdxctl --sock /tmp/tdx.sock over
tdxctl --sock /tmp/tdx.sock bp 1010:001A
tdxctl --sock /tmp/tdx.sock shot
```

## Ghidra symbols

```text
scripts/ghidra_export.sh guest.exe
tdx guest.exe --symbols guest.sym
# or: tdx guest.exe --ghidra
```

## Original TD.EXE (DOSBox Staging)

This laptop has **dosbox-staging**. To compare with 1992 TD:

```text
scripts/td-original.sh /mnt/bushido/bushido/BUSHIDO.EXE
```

Local `BORLANDC/` is a Borland C++ 3.1 reference tree (copyrighted; **not** in git).

## Layout

```text
include/rex/     C ABI (stable)
include/dos/     MZ + CGA + Unicorn machine
include/tdx/     CLI / UI
src/             implementations (keep files under 5k LOC)
tests/           Catch2 + nasm COM fixtures
scripts/         tdxctl, ghidra_export, td-original
docs/            architecture + agent protocol
```
