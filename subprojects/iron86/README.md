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
Confirm with `make test` plus that image run to `A>` (once, not every commit).

Version **0.5** — dispatch table; JMP FAR fetch-before-assign; INT 10/13/16;
enough integer 8086 to reach FloppyOS `A>`.

```text
make -s -j$(nproc)
make -s test
./iron86 -h
./iron86 path/to/tiny.com
```
