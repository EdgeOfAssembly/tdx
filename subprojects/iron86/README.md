# iron86

Chip-level **8086 + IBM PC** emulator in C++23, ported from [Py86](https://github.com/EdgeOfAssembly/RetroCodeMess)
(`intel8086.py` and friends). Iron vs Python: same machine, native speed.

**Not DOS.** iron86 is CPU + platform hardware (PIT, PIC, PPI, FDC later).
Guest OS is IBM DOS, MS-DOS, or FloppyOS. tdx/tdxview stay the debugger.

| Piece | Role |
|-------|------|
| tdx / tdxview | Debugger + CGA view |
| **iron86** | Switchable machine backend (replaces Unicorn later) |
| FloppyOS | Guest DOS (FCB lives here) |
| Py86 | Python reference only (too slow for FloppyOS loops) |

**Policy:** once iron86 is proven against Py86 on the same COM/kernel snippets,
FloppyOS testing uses **iron86 exclusively**. Py86 stays the golden reference
for opcode/timing disputes, not the daily boot.

Opcode dispatch is a **256-entry handler table** (same idea as Py86
`handlers[op]`), not a per-instruction `if`/`switch` forest. Prefixes
(LOCK/REP/seg) are table entries that request another fetch.

Boot FloppyOS with `./iron86 --floppy ../floppyos/build/floppyos.img`.
Genuine IBM 5150 8K BIOS (Py86 `load_bios_5150_8k`): image at `FE000`,
reset vector at `FFFF0`, CPU `FFFF:0000`. Do not commit the ROM.

```text
./iron86 --bios /mnt/RetroCodeMess/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN
```

Version **0.7** — 5150 BIOS load matching Py86, plus packed PPI/PIT/PIC/DMA
for authentic POST (IMR echo, timer 1, DMA wrap, CGA DIP 0x2D). Fast-post
(BDA `RESET_FLAG=1234h`) is the `--bios` default; `--no-fast-post` for cold.

```text
make -s -j$(nproc)
make -s test
./iron86 -h
./iron86 path/to/tiny.com
```
