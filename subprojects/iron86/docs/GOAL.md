# GOAL: iron86 0.17 — INI, floppy CHS, Xebec HDD, ibm-pc preset

Checkpoint: `checkpoint/20260909T214451Z-pre-xebec-ini-floppy` @ `ac7a48a`.

**Accuracy / authenticity:** IBM 5150/5160 era only for this slice. No fake INT 13 HDD. Xebec is the IBM XT Fixed Disk Adapter (S1410 SASI @ 320h, IRQ5, DMA3) per OCR + Py86 `hdc_xebec.py`. 8086 CPU **does** support HDD; missing piece is the **controller + BIOS**, not the ISA.

## Non-goals (this slice)

- 80286 / `ibm-at` opcodes or protected mode (prepare TODO + `--preset` stub only)
- 80186 (WILLNOTFIX — park at bottom of TODO.md)
- 2.88M floppy (no 5150/5160/5170 BIOS)
- WD1002 (docs only; later preset)
- Game-MD5 presets (TODO bottom, much later)
- Separate 8088-only CPU core (4-byte prefetch already exists as `--prefetch-bytes 4`)

## CPU / machine names (docs + TODO)

| Name | Chip | IBM box |
|------|------|---------|
| 8086 | 8086/8088 | 5150 PC (default), 5160 XT (same CPU + Xebec) |
| 8286 | 80286 | **5170 AT** (not XT) |
| 8386 | 80386 | later |
| 8486 | 80486 | later |
| 8586 | Pentium | later |
| AMD K5 | K5 | later |
| 8686 | Pentium Pro | later |

5160 XT = 8088 + Xebec. Do **not** call 286 “XT”.

## Deliverables

### A. Docs in-tree (done copy; keep in git)

- OCR: `subprojects/iron86/docs/xebec/` (S1410 Aug 82, S1410A)
- OCR: `subprojects/iron86/docs/wd/` (later; not this HDC)
- PDFs: `Py86/docs/datasheets/xebec/` and `.../wd/`
- Rewrite `TODO.md`: CPU ladder, 80186 never, INI, presets, floppy table, Xebec proof, MD5 presets last

### B. CLI 0.17 — help = no-args = `-h`

Sync `iron86 -h` / no-args with **0.17** (bump from 0.16):

- `--config PATH` (INI; default `iron86.ini` in cwd if present)
- `--preset NAME` (`ibm-pc` default; `ibm-xt` = same 8086 + Xebec-capable; `ibm-at`/`8286` **reserved**, error “not yet”)
- `--hdc PATH` raw 512-byte image
- `--floppy-a/b` unchanged
- `--prefetch-bytes`, `--until-prompt`, `--exec-map` stay
- CLI flags override INI override preset

INI (no JSON/XML):

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

### C. Floppy CHS auto-detect (IBM PC compatible)

| Size | Geom | Notes |
|------|------|--------|
| 160K | 40/1/8 | |
| 180K | 40/1/9 | |
| 320K | 40/2/8 | |
| 360K | 40/2/9 | |
| 720K | 80/2/9 | |
| 1.2M | 80/2/**15** | AT; still detect by file size |
| 1.44M | 80/2/18 | |
| 2.88M | skip | not this era |

Match **exact** byte sizes first; do not bucket 180K as 360K. Tests: unit geom + help grep. FDC write persist if FORMAT A: is in path; FORMAT C: is Xebec not FDC.

### D. Xebec HDC (authentic)

Ports **320h–323h**, IRQ **5**, DMA **3**. Command set from **S1410 OCR** + Py86 `hdc_xebec.py` (86Box `hdc_st506_xt.c` only as cross-check). Image: flat 512 B sectors, LBA = `((cyl * heads) + head) * spt + sector0`.

BIOS: **5160 XT BIOS** for INT 13 HDD **or** IBM Xebec option ROM `IBM_XEBEC_5000059_1982.BIN` if 5150 + ROM scan. Prefer loading option ROM at C8000 if that is how IBM did the adapter. Do **not** invent a fake C++ INT 13 that bypasses the controller unless ROS cannot run — if so, document as temporary and still implement the ports.

### E. Proof (mandatory)

1. `dd if=/dev/zero of=hdc.img bs=512 count=20808` (306×4×17) or documented CHS.
2. Boot **MS-DOS 3.31** from floppy A: (`ibm-pc` or `ibm-xt` + `--hdc hdc.img`).
3. `FDISK` create DOS partition if required, reboot if required.
4. `FORMAT C:` → FAT16.
5. Copy a small file to C:, reboot, `DIR C:` still shows it.
6. Evidence: commands + exit 0 + DIR listing path. DOSBox for this proof: **`--exit`** and `timeout` so CHECK-style hangs cannot wedge.

If 5150 8K BIOS cannot see HDD: use **5160 BIOS** for the proof (still 8088/8086 class). State which ROM in the log.

### F. Tests / formal

- `make -C subprojects/iron86 test` exit 0 (geom, INI parse, Xebec read/write unit, CLI `-h`/`-v`/no-args)
- `make -C subprojects/iron86 verify` (add CBMC or CrossHair on CHS/LBA and INI parse if feasible; else `verify` depends on `test` and a bounded CBMC on `lba = f(c,h,s)` with unwind)
- tdx `make test` still green if iron86 API changes
- Catch2/existing iron86 snippet tests

### G. Quality

C++23, gcc, Allman, Doxygen on public headers, ctags if public symbols. CLI: no-args = usage; `-h`/`--help`; `-v`/`--version` from 0.17; never `-v` = verbose. Flag polarity: default-on only `--no-*`.

### H. Git

FEATURE series, e.g.

1. `FEATURE 1/4 v1 iron86 docs Xebec OCR + TODO`
2. `FEATURE 2/4 v1 iron86 INI and ibm-pc preset`
3. `FEATURE 3/4 v1 Floppy CHS auto-detect`
4. `FEATURE 4/4 v1 Xebec HDC and FORMAT C proof`

Checkpoint already created. Push `mnt` + origin. Dual-write memory after **cross-agent review**.

## Agent split

| Agent | Work |
|-------|------|
| explore | S1410 OCR command bytes; Py86 hdc; iron86 FDC/CLI |
| implement INI+CLI+TODO | B, A leftover |
| implement floppy | C |
| implement Xebec | D, E |
| review × N | each slice |
| orchestrator | merge, `make test`/`verify`, FORMAT proof, memory, commit, push |

## Done

- [ ] `iron86` and `iron86 -h` identical usage, version 0.17
- [ ] INI + `--preset ibm-pc`
- [ ] Floppy sizes in table C
- [ ] Xebec + FORMAT C: proof log
- [ ] 80186 at TODO bottom WILLNOTFIX
- [ ] `make test` + `make verify` green
- [ ] Reviews closed
- [ ] pmem + MEMORY.md + commit + push
EOF
