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

Boot FloppyOS with `./iron86 --floppy-a ../floppyos/build/floppyos.img`.
`--floppy` is an alias for `--floppy-a`. `--floppy-b PATH` mounts drive B:
(image file or a host directory packed as FlopFS: 360K if it fits, else 720K
80/2/9).
Genuine IBM 5150 8K BIOS (Py86 `load_bios_5150_8k`): image at `FE000`,
reset vector at `FFFF0`, CPU `FFFF:0000`. `--bios` also accepts a **40K**
5160 concat (U19 8K + U18 32K) or a **64K** 5160 image at `F0000`
(`cat U19 U18`). Do not commit the ROM.

Xebec / IBM XT Fixed Disk Adapter (`--hdc PATH`, `--hdc-rom` optional):
ports **320h–323h**, IRQ **5**, DMA **3**, IBM CHS DCB, option ROM at **C8000**.
Type-1 geometry 306×4×17. INT 13h with DL bit 7 set is not stolen (ROS owns C:).
Prep / FORMAT C: notes: `tests/hdc-format-c.sh`.

```text
./iron86 --bios /mnt/RetroCodeMess/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN
```

Version **0.19** — `--log FILE` (append event log; default off), `--lock-mem`
(mlock guest RAM + HDC image; default off), HDC port traces under `--log`
(`if constexpr`, no extra branches without `--log`). Unbuffered
`write(2)` diagnostics. INI (`--config`, default `iron86.ini` in cwd), `--preset ibm-pc`
(default) / `ibm-xt` (`ibm-at`/`8286` not yet), `--hdc PATH` plumbing, floppy CHS
auto-detect (160K/180K/320K/360K/720K/1.2M/1.44M; no 2.88M on PC/XT/AT),
8088-style prefetch queue (`--prefetch-bytes`, default 4), dual floppy A:/B:
(`--floppy-a` / `--floppy-b`, directory packs 360K or 720K FlopFS), 544K PPI I/O
nibble (Py86 `0x0F`, 1981 BIOS max), PC speaker BEL (PIT ch2 + PPI 61h bits 0+1;
`--no-audio` only). Fast-post (BDA `RESET_FLAG=1234h`) is the `--bios` default;
`--no-fast-post` for cold. `--until-prompt` and `--exec-map FILE` stay.

```text
make -s -j$(nproc)
make -s test
./iron86 -h
./iron86 path/to/tiny.com
```
