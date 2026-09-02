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

**Not ready yet.** Need at least: full ALU + 80/81/83, FF-group, LES/LDS,
REP MOVS/STOS, shifts D0–D3, MUL/DIV, prefixes, then PIC/PIT/PPI/FDC + MDA
to boot FloppyOS to `A>`. Confirm with `make test` plus a FloppyOS image
run that matches Py86’s `A>` (once, not every commit).

Version **0.2** — ModR/M, PUSH/POP, INC/DEC r16, CALL/RET, ADD/CMP acc,
Jcc rel8.

```text
make -s -j$(nproc)
make -s test
./iron86 -h
./iron86 path/to/tiny.com
```
