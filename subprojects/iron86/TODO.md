# iron86 TODO

Gates 1/4 (kernel stack is FloppyOS): DAA/WAIT/ESC.
Gates 2/5 done: 256K PPI nibble (`io_nibble=0x06`), speaker `--no-audio`.
PPI I/O nibble is now **0x0F = 544K** (24-APR-81 BIOS max).

## CPU / machine ladder

| Name | Chip | IBM box | Status |
|------|------|---------|--------|
| 8086 | 8086/8088 | **5150 PC** (default `--preset ibm-pc`) | current |
| 8086 | 8088 + Xebec | **5160 XT** (`--preset ibm-xt`) | same CPU as 5150; HDC next |
| 8286 | 80286 | **5170 AT** (`ibm-at` / `8286`) — **not XT** | reserved, not yet |
| 8386 | 80386 | later | |
| 8486 | 80486 | later | |
| 8586 | Pentium | later | |
| AMD K5 | K5 | later | |
| 8686 | Pentium Pro | later | |

5160 XT is still an **8088** (same CPU class as the 5150) plus the IBM Fixed Disk Adapter (Xebec S1410 SASI @ 320h, IRQ5, DMA3). Do **not** call the 286 an XT. 5162 XT/286 is not the 286 target.

## Config (INI)

Precedence: **CLI overrides INI overrides preset**. No JSON/XML.

- [x] INI parser (`include/iron86/config.h`, `src/config.cpp`)
- [x] `--config PATH` (default `iron86.ini` in cwd if that file exists)
- [x] `--preset ibm-pc` (default) / `ibm-xt` (8086 + Xebec-capable)
- [x] `--preset ibm-at` / `8286` reserved: stderr **not yet**, exit 2
- [x] `--hdc PATH` plumbing (store path; Xebec command engine is next)

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
prefetch_bytes = 4
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
- [ ] FDC **Write Data** DMA-from-mem (persist image)
- [ ] FDC **Format** / **Scan** / **Read ID**
- [ ] **Dir pack: smallest floppy that fits** — recursive tree size, then
  360K → 720K → 1.2M → 1.44M. Today: flat dir, 32 names, 360/720 only.

## Xebec HDD (5160 / `ibm-xt`)

8086 **does** support HDD; the missing piece is the **controller + BIOS**, not the ISA. 5150 8K BIOS has no HDD INT 13 — use 5160 BIOS or the IBM Xebec option ROM (`IBM_XEBEC_5000059_1982.BIN`) at C800:0. INT 13 with **DL & 0x80** must not map onto floppy (intercept returns false so ROS can run).

- [x] `--hdc PATH` store path (stub `attach_hdc_path`)
- [x] INT 13 DL&0x80 fall through (do not treat HDD as A:)
- [x] Xebec command engine (`hdc.cpp` / `hdc.h`): 320h–323h, IRQ5, DMA3, IBM CHS DCB (0-based sector), opcodes 00/01/03/04/05/06/08/0A/0B/0C/0E/0F + class-7 stubs, persist to hdc.img, ROS at C8000, 8K/40K/64K `--bios`
- [ ] **FORMAT C: proof** (mandatory): `dd if=/dev/zero of=hdc.img bs=512 count=20808` (306×4×17), boot MS-DOS 3.31 from A: (`ibm-pc` or `ibm-xt` + `--hdc`), FDISK, `FORMAT C:`, copy a file, reboot, `DIR C:` still shows it — harness: `tests/hdc-format-c.sh`
- [ ] **XT HDD C:** auto-size from a host dir larger than 1.44M (or any floppy)

## Bottom of queue (after those)

- [ ] Full 8237 HOLD/HLDA / DRAM refresh tick
- [x] **CGA BIOS CRT_CHAR_GEN 8×8** (`F000:FA6E`, runtime from guest, tdxview 8×16 double-row)
- [x] **MDA 8K 5788005** + tdxview B000 (`tdx --mda`). BIN gitignored, not pushed.
- [ ] CGA **3D9** palette; **40-col** modes 0/1
- [x] **mode 6** 640×200 1bpp + composite in tdxview (Dragon Wars)
- [x] **mode 4** 320×200 old-CGA NTSC (Reenigne/86Box); gold `games/SCREEN.CGA`
- [x] PPI I/O nibble **0x0F = 544K** (24-APR-81 BIOS max; not 640K / not EMS)
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
