# How to run the Dragon Wars CGA-composite demo

Proven 2026-09-04: FloppyOS on A:, `games/DRGNWARS` packed as **720K** B:,
`INT 21 AH=42` LSEEK + `AH=3F` load `DATA1`, tdxview **old-CGA NTSC**
(Reenigne/86Box). Guest B800 matched the DOSBox dump `games/SCREEN.CGA`
(16384 bytes). Git: `a66b0aa`, tag `milestone/cga-composite-works`.

Do **not** put tdx/tdxview under Xmux. One pair only.

## Paths

| What | Path |
|------|------|
| Repo | `/mnt/TurboDebugger` |
| BIOS (local, **not** on GitHub) | `Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN` |
| FloppyOS A: | `subprojects/floppyos/build/floppyos.img` |
| Game dir B: | `games/DRGNWARS` |
| Gold VRAM dump | `games/SCREEN.CGA` |
| Gold screenshot | `dragon_wars.png` (repo root, not committed) |

If the BIOS path is missing, the same 8K file may live under
`/mnt/RetroCodeMess/Py86/ROM/IBM/PC/5150/`.

## Build

```sh
cd /mnt/TurboDebugger
source ~/.local/share/test-frameworks/env.sh
make -s tdx tdxview
make -s -C subprojects/floppyos image
```

## Start (host display)

```sh
cd /mnt/TurboDebugger
./scripts/tdx-kill.sh

BIOS=Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN
test -f "$BIOS" || BIOS=/mnt/RetroCodeMess/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN

./tdx --bios "$BIOS" \
  --floppy-a subprojects/floppyos/build/floppyos.img \
  --floppy-b games/DRGNWARS \
  --sock /tmp/tdx.sock &

./tdxview --sock /tmp/tdx.sock --listen /tmp/tdxview.sock --scale 2 &
```

CPU window = **tdx**. Game window = **tdxview** (composite **on** by default;
`--no-composite` is RGBI magenta/cyan).

## Drive it (or type yourself)

```sh
python3 scripts/tdxctl.py delay 0
python3 scripts/tdxctl.py unpause
```

Wait until FloppyOS shows **`A>`** (POST + INT 19). Then:

```sh
python3 scripts/tdxctl.py key B
python3 scripts/tdxctl.py key :
python3 scripts/tdxctl.py key Enter
# wait for B>
python3 scripts/tdxctl.py key d
python3 scripts/tdxctl.py key r
python3 scripts/tdxctl.py key a
python3 scripts/tdxctl.py key g
python3 scripts/tdxctl.py key o
python3 scripts/tdxctl.py key n
python3 scripts/tdxctl.py key Enter
```

Do **not** send Space/Enter after `dragon` — that skips the intro.

After ~15–20 s at delay 0 the title should match `dragon_wars.png`
(blue dragon, red warrior, Interplay). Dump VRAM:

```sh
python3 scripts/tdxctl.py dump cga SCREEN.CGA
cmp SCREEN.CGA games/SCREEN.CGA
python3 scripts/tdxctl.py --view shot /tmp/tdx-game.bmp
```

## Notes

- `dragon -s` is setup only (writes video choice into `DRAGON.COM`). This copy
  already has composite (`[0x102]=2` → INT 10 mode 4, 3D8=`2Ah`).
- Keyboard: US XT colon is Shift+`;`. Agents should use `tdxctl key :`.
- Always `scripts/tdx-kill.sh` before starting another pair.
