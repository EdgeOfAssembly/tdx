# iron86 TODO

Gates 1/4 (kernel stack is FloppyOS): DAA/WAIT/ESC.
Gates 2/5 done: 256K PPI nibble (`io_nibble=0x06`), speaker `--no-audio`.
PPI I/O nibble is now **0x0F = 544K** (24-APR-81 BIOS max).

## CPU / machine ladder

| Name | Chip | IBM box | Status |
|------|------|---------|--------|
| 8086 | 8086 prefetch **6** (IBM shipped **8088** queue 4) | **5150 PC** (`ibm-pc` / `5150`) | floppy only; 8K BIOS, no C8000 scan |
| 8086 | 8086 + Xebec | **5160 XT** (`ibm-xt` / `5160`) | 64K BIOS scans C8000; `DIR C:` proven `ff83f30` |
| 8286 | 80286 | **5170 AT** (`ibm-at` / `8286`) — **not XT** | reserved, not yet |
| 8386 | 80386 | later | |
| 8486 | 80486 | later | |
| 8586 | Pentium | later | |
| AMD K5 | K5 | later | |
| 8686 | Pentium Pro | later | |

IBM 5150/5160 **factory CPU is 8088**. iron86/tdx default both presets to **8086 prefetch 6** (`--prefetch-bytes 4` for stock 8088). Xebec is S1410 SASI @ 320h, IRQ5, DMA3. Do **not** call the 286 an XT. 5162 XT/286 is not the 286 target.

## Config (INI)

Precedence: **CLI overrides INI overrides preset**. No JSON/XML.

- [x] INI parser (`include/iron86/config.h`, `src/config.cpp`)
- [x] `--config PATH` (default `iron86.ini` in cwd if that file exists)
- [x] `--preset ibm-pc` / `5150` (floppy) / `ibm-xt` / `5160` (Xebec); `--hdc` implies xt
- [x] `--preset ibm-at` / `8286` reserved: stderr **not yet**, exit 2
- [x] `--hdc PATH` + Xebec engine (not a stub): 320h–323h, ROS C8000, Type-1 306×4×17
- [x] `--log FILE`, `--lock-mem`, `--no-fast-post`, `--hdc-rom FILE`

```ini
[machine]
preset = ibm-pc

[bios]
rom = /path/to/IBM5150.BIN

[floppy]
a = disk.img
b =

[hdc]
image = hdc.img
; ibm-pc/xt: ST-412-ish 306 cyl × 4 heads × 17 spt ≈ 10 MB
cylinders = 306
heads = 4
spt = 17

[cpu]
prefetch_bytes = 6
```

## Floppy CHS (IBM PC compatible)

Exact byte sizes first, then nearest-bucket fallback. **No 2.88M** on 5150 / 5160 / 5170.

| Size | Bytes | Geom (cyl/heads/spt) |
|------|------:|----------------------|
| 160K | 163840 | 40/1/8 |
| 180K | 184320 | 40/1/9 |
| 320K | 327680 | 40/2/8 |
| 360K | 368640 | 40/2/9 |
| 720K | 737280 | 80/2/9 |
| 1.2M | 1228800 | 80/2/**15** |
| 1.44M | 1474560 | 80/2/18 |
| 2.88M | — | skip (not this era) |

INT 13 AH=08 cylinder count is `size / (512 * spt * heads)`, not a hardcoded 40/80 when the image is short.

- [x] **Raw image CHS auto-detect** — table above; per-unit SPT/heads
- [x] Floppy **B:** (second 360K, INT 13 DL=1, `attach_floppy` unit 1 / `--floppy-b`)
- [x] **720K** FlopFS pack when a host dir does not fit 360K (`--floppy-b`)
- [x] FDC **Write Data** DMA-from-mem (persist image)
- [x] FDC **Format** / **Scan** / **Read ID**
- [x] **Dir pack: smallest floppy that fits** — recursive 8.3 flatten,
  360K → 720K → 1.2M → 1.44M; root dirents grow with file count.

## Xebec HDD (5160 / `ibm-xt`)

8086 **does** support HDD; the missing piece is the **controller + BIOS**, not the ISA. 5150 8K BIOS has no HDD INT 13 — use 5160 BIOS or the IBM Xebec option ROM (`IBM_XEBEC_5000059_1982.BIN`) at C800:0. INT 13 with **DL & 0x80** must not map onto floppy (intercept returns false so ROS can run).

- [x] `--hdc PATH` refuse `/dev/sd*` (host disks)
- [x] INT 13 DL&0x80 fall through (do not treat HDD as A:)
- [x] Xebec command engine (`hdc.cpp` / `hdc.h`): 320h–323h, IRQ5, DMA3, IBM CHS DCB (0-based sector), opcodes 00/01/03/04/05/06/08/0A/0B/0C/0E/0F + class-7 stubs, persist to hdc.img, ROS at C8000, 8K/40K/64K `--bios`
- [x] 5160 POST **301/F1** (`fe9e529` IRQ1 delay + Port A idle) and **8259 nested IRQ** (`ff83f30`) so DOS does not stack-overflow
- [x] **DIR C:** on `ibm-xt` + DOS 3.31 + Type-1 image (empty FAT12 root). Shot `re/tdx-tank/tdx-xt-dirc-20260910T212706.431.bmp`
- [x] **FORMAT C: copy+reboot** — `ECHO HELLO>C:HI.TXT` then kill tdx, restart same image, `DIR C:` still shows `HI.TXT` 7 bytes (`b16bc2a6…` persist). Burst `tdxctl type` of a long line can still stack-overflow; type per-char or pump in `feed_ascii`.
- [x] **FORMAT C: as letter C:** guest FDISK + FORMAT C: on blank Type-1 (`hdc-type1-fmt.img`, FAT12 `IBM  3.3`, `FORMAT.COM` 19683). Shot `re/tdx-tank/tdx-cold-dirc-20260910T230506.357.bmp` (also after cold POST).
- [x] **Cold POST dummy-stack smash** — BIOS F000 is write-protected (real XT ROM). No more CS=`4B20` zeros after 640 KB OK.
- [x] **Cold POST to A>** — `--no-fast-post` 5160 10JAN86: 640 KB OK, EQUIP_FLAG=402Dh, date prompt, `DIR C:` still `FORMAT.COM` 19683. SW1 mux is PB3 (`0.22`). STGTST is slow (~3 min). Fast-post remains the play path.
- [x] **XT HDD C:** auto-size from a host dir — `--hdc DIR` packs Type-1 FAT12 (`DIR.img`)

## Bottom of queue (after those)

- [ ] Full 8237 HOLD/HLDA / DRAM refresh tick
- [x] **CGA BIOS CRT_CHAR_GEN 8×8** (`F000:FA6E`, runtime from guest, tdxview 8×16 double-row)
- [x] **MDA 8K 5788005** + tdxview B000 (`tdx --mda`). BIN gitignored, not pushed.
- [x] CGA **3D9** palette (OUT 3D9 + BDA 40:66); **40-col** modes 0/1 in tdxview
- [x] **mode 6** 640×200 1bpp + composite in tdxview (Dragon Wars)
- [x] **mode 4** 320×200 old-CGA NTSC (Reenigne/86Box); gold `games/SCREEN.CGA`
- [x] PPI I/O nibble **0x0F = 544K** (24-APR-81 BIOS max; not 640K / not EMS)
- [x] 8087 NPX default on (`--no-8087`)
- [x] **`--exec-map FILE`** — 1 MiB executed-opcode map (same linear addrs as RAM)
- [ ] MDA **blink**; exact 720×350 timing
- [ ] **Hercules** 720×348 gfx (not MDA — HGC is MDA text + extra graphics)
- [ ] Cassette BASIC ROM map

## Game-MD5 presets (last)

After **8286 / 5170 AT** exists: pin CPU + MHz + RAM + BIOS class + video + audio + floppy max + HDD from a game image MD5. Not before 286.

- [ ] Game-MD5 machine presets

## WILLNOTFIX / park

- [ ] ~~CGA light pen~~ **WILLNOTFIX** (no light pen hardware here)
- [ ] ~~Intel 80186~~ **WILLNOTFIX** — not in the ISA plan (no 80186 box in the ladder). Parked; do not add 186 opcodes or a preset.
- [ ] iron86 dual MDA+CGA in one guest — last; tdx/tdxview is the dual-head
- [ ] CGA snow / wait-states (cycle-exact 6845) — unless the composite-artifact game needs it

See also repo `docs/TODO.md`.
