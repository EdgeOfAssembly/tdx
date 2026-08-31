# TDX — Turbo Debugger X

Native **x86_64 Linux** debugger with a Borland Turbo Debugger–like CPU view.
It is **not** a source port of `TD.EXE` (Borland never shipped TD sources here).
The reusable core is **librex**: load a target, breakpoints, step, step-over,
memory, symbols, agent socket. First backend: **DOS MZ EXE / .COM** on Unicorn
8086 + Capstone.

| Name | What |
|------|------|
| **tdx** | CLI / SDL2 UI binary |
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
```

SPECTATOR (two SDL windows: CPU + CGA user screen):

```text
xmux prune
xmux start tdx --geometry 1920x800 --gl nvidia --no-attach -- \
  ./tdx /mnt/bushido/bushido/BUSHIDO.EXE --cwd /mnt/bushido/bushido
# SPECTATOR: xmux attach tdx --no-reconnect
```

Keys: **F7** trace, **F8** step over (CALL / INT / REP / LOOP), **F9** run/pause,
**F2** breakpoint at CS:IP, **Alt-X** quit.

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
