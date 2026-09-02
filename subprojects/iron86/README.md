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
| Py86 | Python reference (slow, accurate) |

Version **0.1** — registers, 1 MiB RAM, a handful of opcodes, `HLT` / `INT`.

```text
make -s -j$(nproc)
make -s test
./iron86 -h
./iron86 path/to/tiny.com
```
