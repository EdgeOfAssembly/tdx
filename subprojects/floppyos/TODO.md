# FloppyOS — Master TODO & Roadmap

**Project:** Optimized, enhanced, **100% MS-DOS-compatible** OS that fully fits a **1.44 MB** floppy (and scales to larger media).  
**Status:** Research complete · implementation not started  
**Living doc:** this file (supersedes narrative in `FloppyOS-ROADMAP.md` for task tracking)  
**Repo intent:** https://github.com/EdgeOfAssembly/FloppyOS  
**Date:** 2026-07-25

---

## Vision (one paragraph)

FloppyOS is a **game-oriented, size-obsessed MS-DOS 4.x-class system**: maximum conventional memory, smart game profiles, modern drivers where useful, and a **custom compressed primary filesystem (FlopFS)** so a single 1.44 MB disk carries far more logical payload than FAT12 alone. Classic **FAT12 / FAT16 / FAT32 / exFAT** remain first-class for foreign volumes and dual-boot media. We **do not copy** third-party or monorepo code literally — we use it as **architecture reference and behavioral oracle**, then implement our own clean sources.

---

## Non-negotiable constraints

| Constraint | Rule |
|------------|------|
| **Media size** | Boot floppy target = **1 474 560 bytes** = **2880 × 512** sectors, CHS **80/2/18** (IBM HD 3.5") |
| **Compatibility** | Apps that run on MS-DOS 4–6 / FreeDOS should run (INT 21h, PSP, MCB, EXE/COM/SYS, FCBs where required) |
| **Primary FS** | **FlopFS** (custom, compressed) on distribution/boot floppies |
| **Secondary FS** | **FAT12, FAT16, FAT32, exFAT** drivers for other volumes |
| **IP hygiene** | Inspiration only from monorepo / open sources; **no literal copy** of GPL FreeDOS into MIT tree without license review; MS-DOS 4.x is **MIT** (Microsoft) |
| **Hardware safety** | USB floppy **`/dev/sdc`** (MITSUMI) — **ALWAYS confirm human before format / mkfs / dd of whole device** |
| **Root** | If needed: `sudo -S < /tmp/password.txt …` — still never format without explicit OK |

### Live hardware (current session)

| Item | Value |
|------|--------|
| Device | `/dev/sdc` — MITSUMI USB FDD |
| Size | 1.41 MiB / 1 474 560 bytes / 2880 sectors |
| Mount | `/tmp/floppy` · `vfat` (FAT12) |
| Contents | Period MS-DOS-style system (`IO.SYS`, `MSDOS.SYS`, `COMMAND.COM`, HIMEM, EMM386, tools…) |
| Boot sector | OEM `MSDOS5.0`, media `0xF0`, classic FAT12 BPB, `55 AA` |

**Do not wipe this disk** until the human explicitly approves a format plan.

---

## Status legend

- `[ ]` not started  
- `[~]` partial / research only  
- `[x]` done  
- **P0** blocking · **P1** core · **P2** enhancement · **P3** later

---

# Track 0 — Project scaffolding (P0)

- [x] Seed README + high-level `FloppyOS-ROADMAP.md`
- [x] Monorepo inspiration survey (explore agents 2026-07-25)
- [x] This master `TODO.md`
- [x] **Milestone 1:** boot sector + `mkimg1440` + QEMU smoke (`FloppyOS OK`) — 2026-07-26
- [x] **Milestone 2:** stage1.5 + FlopFS superblock LBA1/2 — 2026-07-26
- [x] **Milestone 3:** stored kernel stub load + jump — 2026-07-26
- [x] **Milestone 4:** INT 21h AH=02/09/4C in kernel — 2026-07-26
- [x] **Milestone 5:** COM loader + hello.com — 2026-07-26
- [x] **Milestone 6:** INT 21h AH=25/30/35 + VER/VEC21 COM demo — 2026-07-26
- [x] **Milestone 7:** FlopFS root + AH=3D/3E/3F + COM by name — 2026-07-26
- [x] **Milestone 8:** MCB + AH=48/49/4A + MEM OK — 2026-07-26
- [x] **Milestone 9:** COMMAND shell DIR/TYPE + AH=1A/4E/4F — 2026-07-26
- [x] **Milestone 10:** AH=4B COM exec + AH=4C parent return — 2026-07-26
- [x] **Milestone 11:** MZ EXE load via AH=4B + HELLO.EXE — 2026-07-26
- [x] **Milestone 12:** interactive shell + AH=01/08/0A/0B + sendkey smoke — 2026-07-26
- [x] **Py86 boot:** full chain to **A>** on local Py86 360K (8086-safe) — 2026-07-26
- [x] Create tree layout under `FloppyOS/`:

```text
FloppyOS/
  TODO.md                 # this file
  README.md
  FloppyOS-ROADMAP.md     # narrative history
  docs/
    architecture.md
    flopfs-spec.md
    fat-compat.md
    boot-chain.md
    size-budget.md
    hardware-safety.md    # /dev/sdc rules
  boot/                   # boot sector + stage1.5
  kernel/                 # IO.SYS / MSDOS.SYS equivalents
  shell/                  # COMMAND.COM equivalent
  fs/
    flopfs/               # primary custom FS
    fat/                  # FAT12/16/32
    exfat/
  drivers/                # HIMEM, mouse, USB, disk, …
  tools/                  # host Linux: mkimg, mkflopfs, floppack, sys
  tests/
    images/
    golden/
  third_party/            # vendored *references* only (licenses!)
```

- [ ] Document license policy (MIT core; list allowed reference trees)
- [ ] Checkpoint git before first FEATURE commit (`efficient-git`)
- [ ] Symlink or document path to MS-DOS sources: **`/home/wizard/MS-DOS`** (v1.25, v2.0, v4.0, v4.0-ozzie) — MIT  
  - Note: `/tmp/RetroCodeMess/src/MS-DOS` is **empty**; real tree is under `$HOME`

---

# Track 1 — Toolchain & build (P0)

**Success:** reproducible build of 16-bit real-mode binaries + host tools on this Gentoo box.

- [x] **OpenWatcom** installed at **`/opt/ow`** (`binl/wcc`, `wcc386`, `wlink`, `wasm`)
- [ ] Document OW env in shell profile / `docs/toolchain-notes.md` (done) + verify `wcc` in PATH for builds
- [ ] Install / verify **GCC-IA16** (tkchia) as alternate pure-16-bit path
- [ ] NASM/YASM/UASM for boot sector (≤512 B)
- [ ] Host: `gcc` C23 for `tools/` (mkflopfs, floppack, image writer)
- [ ] Makefile / CMake skeleton: `make boot`, `make kernel`, `make image1440`
- [ ] CRLF / path fixes notes for MS-DOS 4.x `SETENV.BAT` builds (if building upstream first)
- [ ] Size report target: `make size` → per-component bytes + free on 1.44M image
- [x] **Custom QEMU 10.2.3** `i386-softmmu` → `/usr/local` (build on `/tmp`, archives on `/mnt/floppyos-build`)
- [x] DOSBox available (`/usr/local/bin/dosbox`) for app/game runs
- [x] **TCC note:** LINKS phoenixthrush/Tiny-C-Compiler is archived dump — **skip**; not P0 (see toolchain-notes)

### Toolchain preference (from roadmap)

1. **OpenWatcom** (`/opt/ow`)  
2. GCC-IA16  
3. DOSBox + **QEMU i386** + period tools / UASM/JWasm fallback  

### Host disk policy

| Path | Use |
|------|-----|
| `/tmp` | Compile / extract (tmpfs) |
| `/mnt` (e.g. `/mnt/floppyos-build`) | Archives, logs, large tarballs |
| `/` | Avoid large builds (often near full) |  

---

# Track 2 — Boot chain (P0)

**Success:** BIOS loads FloppyOS from 1.44 MB image → kernel in memory → shell prompt (emulator first).

### 2.1 Design (FlopFS-aware)

```text
BIOS INT 19h
  → LBA 0 boot sector (fake BPB + geometry; NOT FAT parser)
  → stage1.5 in reserved sectors (INT 13h multi-sector)
  → read FlopFS superblock LBA 1 (mirror LBA 2)
  → locate \IO.SYS (or \FLOPDOS.SYS), stream deflate via puff
  → kernel entry → mount FlopFS → CONFIG → COMMAND.COM
```

- [x] Spec `docs/boot-chain.md` (M1 banner stage; Option A hybrid later)
- [x] Boot sector ASM: jump, OEM, **geometry BPB**, print banner, `55 AA` (M1; INT 13h multi-sector = M2)
- [x] Stage1.5: load + SB verify (M2) + stored kernel load/jump (M3); deflate later
- [ ] Fallback path: classic FAT12 boot (for dual-personality disks) — Option B
- [x] `tools/mkimg1440` — assemble 2880-sector `.img` without touching `/dev/sdc`
- [x] QEMU boot test: `make smoke` → `FloppyOS OK` on serial
- [ ] DOSBox-X boot test (daily)
- [ ] Py86 smoke (8086 authenticity; 360K first if 1.44M BIOS limited)

### 2.2 Inspiration (do not copy)

| Source | Path | Steal idea |
|--------|------|------------|
| MS-DOS 4.0 boot/BIOS | `/home/wizard/MS-DOS/v4.0/src/BOOT`, `…/BIOS` (`MSLOAD`, `MSBIO*`, `SYSINIT*`) | Load stages, device init order |
| Live FAT12 boot | `/dev/sdc` sector 0 (read-only `xxd`) | BPB field layout, error strings |
| FreeDOS floppy imgs | `/home/wizard/freedos-img/` | Size packing, multi-disk sets |
| AXE skill | `AXE/skills/boot_flow_reconstruction.md` | Stage timeline checklist |
| PyFloppy | `PyFloppy/floppy_drive.py` | CHS 80/2/18 constants |

---

# Track 3 — Kernel (MS-DOS compatibility) (P0→P1)

**Success:** Working `IO.SYS` + `MSDOS.SYS` (or unified kernel) implementing enough INT 21h that period games and tools run.

### 3.1 Bootstrap from open MS-DOS 4.x (MIT)

Primary reference (behavioral + structure, then **our** reimplementation / controlled fork):

| Tree | Path | Role |
|------|------|------|
| MS-DOS 4.0 | `/home/wizard/MS-DOS/v4.0/src/DOS` | Kernel: `ALLOC`, `EXEC`, `HANDLE`, `FAT`, `DIR`, `DISK*`, … |
| MS-DOS 4.0 BIOS | `…/src/BIOS` | `IO.SYS` side: disk, CON, clock, sysinit |
| MS-DOS 4.0 CMD | `…/src/CMD/COMMAND` | Shell reference |
| MS-DOS 2.0 / 1.25 | `/home/wizard/MS-DOS/v{1.25,2.0}` | Minimal historical behavior |
| FASM port notes | https://github.com/AndresTraks/MS-DOS-FASM | Modern assembler adaptation ideas |
| Paterson 86-DOS | https://github.com/DOS-History/Paterson-Listings | Ancestry only |

- [ ] Build **unmodified** MS-DOS 4.0 once (proof of toolchain) → golden binaries
- [ ] Inventory INT 21h AH functions used by target games (trace-driven)
- [ ] Define FloppyOS kernel module map (memory, process, file, device, FS VFS)
- [~] INT 21h AH=02/09/4C installed (M4); MCB/PSP/arena still open
- [ ] Implement / port: MCB chain, PSP, arena (`dos_memory_arena_analyzer` skill as oracle)
- [ ] Implement: COM / MZ EXE loader (`retro/docs/RE/exe_format_reference.md` math)
- [ ] Implement: device driver chain (CONFIG.SYS `DEVICE=`)
- [x] Implement: handle + FCB file APIs (AH=0F/10/14/21/27; MS-DOS 1.25 RECSIZ/RR)
- [ ] Conventional memory maximizers: DOS=HIGH path later with XMS
- [ ] Country / codepage hooks (Finnish 850 present on live floppy — keep NLS optional)

### 3.2 Compatibility oracle tools (monorepo)

| Tool | Path | Use |
|------|------|-----|
| RBIL dump | `retro/dumpexe/interrupts/` | INT 21h/13h/10h checklist |
| dumpexe | `retro/dumpexe/` | Analyze apps, packers, SYS headers (**GPLv2** — tool only, not ship in MIT core) |
| Py86 BIOS model | `Py86/` (+ `PCBIOS.ASM` if present) | Real INT 13h / 7C00 boot path for CI |
| DOSBox DEBUG_TRACE | `retro/8086-cga.conf` | INT + file I/O traces |
| AXE skills | `AXE/skills/dosbox_int21_trace.md`, `dos_memory_arena_analyzer.md` | Test methodology |
| AXE DOSBox presets | `AXE/tools/dosbox-presets/` (cga-8086, ega-286, vga-386, headless-re) | Era-accurate test matrix |
| AXE unpackers | `AXE/tools/dos_unpackers/` (unlzexe, depklite, …) | Unpack packed games for profile tests |
| Extender detect | `dos/detect_extenders.*` | Game profile signals (not a DPMI host) |
| Real MS-DOS 6 utils | `retro/DOS/`, `Py86/DOS.BAK/`, live `/tmp/floppy` | Behavioral golden files |
| Borland C 3.1 | `retro/BORLANDC/` | Period CRT / probe builds only |
| 1.44MB golden imgs | `Py86/IMG/msdos6_22disk{1,2,3}.img` | FAT12 layout + boot oracle |
| Game floppy imgs | `src/system-shock-multilingual-floppy-ibm-pc/disk*.img` | PyFloppy / image-tool regression |

- [ ] Golden test: same app under MS-DOS 6.22 image vs FloppyOS → INT21 trace diff
- [ ] Golden test: `MEM` / MCB walk matches expected free blocks after boot

---

# Track 4 — FlopFS (custom primary filesystem) (P0→P1)

**Working name:** FlopFS · magic `FLOPFS01`  
**Goal:** control layout, compression, dedup; maximize logical capacity on 1.44 MB.

Full design sketch: `/tmp/grok-1000/floppyos-explore-fs-compress.md` (promote → `docs/flopfs-spec.md`).

### 4.1 On-disk layout (1.44 MB)

```text
LBA 0        Boot sector (BIOS + fake BPB; no FAT parse)
LBA 1        Superblock primary
LBA 2        Superblock mirror
LBA 3..R-1   Stage1.5 + early inflate
LBA …        Bitmap / free map
LBA …        Inode + directory (packed)
LBA …        Extent / block-map table
LBA …        Data region (compressed CAS blobs)
LBA 2879     Optional generation / trailer CRC
```

Metadata budget target: **~16 KiB** → **~1.42 MiB** data region.

- [ ] Write formal `docs/flopfs-spec.md` (superblock, inode, dirent, extent, blob header)
- [ ] Superblock CRC + generation counter
- [ ] Logical block size default **2048** (configurable 4096)
- [ ] Blob header: unc_size, comp_size, codec, flags (+ optional CRC32/XXH32)
- [ ] Pack-once **RO** mode (distribution) + light **RW** scratch area
- [ ] Optional pack-time **dedup** (ArmorFS CAS idea, tiny hash table)

### 4.2 Codecs

| Codec | On-disk | 8086 | Notes |
|-------|---------|------|-------|
| stored | 0 | yes | Incompressible / already packed |
| **deflate (puff)** | 1 | **yes** | **Primary** — `zlib-1.3.2/contrib/puff/` (~4 KiB, &lt;2 KiB stack) |
| LZSS 12/4 | 2 | yes | Tiny decompressor; weaker ratio |
| LZ4 | 3 | 386+ optional | Fast |
| zstd | host-only | no | Host experiments → re-encode to deflate |

- [ ] Port / reimplement **puff-class** inflate for IA16 (own code, puff as reference)
- [ ] Host `floppack`: zlib/miniz deflate level 9; store if no win
- [ ] Never require zstd/SHA-256/SQLite on the DOS side

### 4.3 Host tools (Linux)

- [ ] `mkflopfs` — create empty image with geometry + superblocks
- [ ] `floppack` — pack directory tree → compressed image
- [ ] `flopfsck` — verify CRC, extents, free map
- [ ] `floplist` / `flopget` — inspect without mount
- [ ] Optional FUSE `mount.flopfs` (ArmorFS CLI shape: create/mount/unmount)
- [ ] `write-floppy` script that **refuses** `/dev/sdc` unless `--i-confirm-format` **and** human already said yes in chat
  - **Gap confirmed:** monorepo has **no** existing USB-FDD write scripts; PyFloppy writes `.img` only; GPIO bridge is Pi Shugart, not block-device USB

### 4.4 Inspiration (ideas only)

| Source | Path | Idea |
|--------|------|------|
| ArmorFS | `ArmorFS/` (`df_block`, `df_compress`, `df_journal`, `ARMORFS.md`) | CAS blocks, compress API boundary, refcount dedup, host pack tools |
| zlib puff | `zlib-1.3.2/contrib/puff/` | Tiny inflate footprint |
| DriveSpace binaries | `retro/DOS/DRVSPACE.*` | Historical “compressed volume” UX (binary oracle only) |
| AR LZSS notes | `retro/AR/`, `cube/AR/` | Floppy-era dictionary codecs; ~200 B-class 8086 decompress ideas |
| star/ | `star/` | Host image packing: sparse + gzip/zstd/lzop (build pipeline only) |
| CBM disk docs | `docs/D64.TXT`, `c64/d64_extractor.c` | Central-dir / seek-aware layout patterns (not the FS itself) |
| PyFloppy | `PyFloppy/` | Geometry, sector image sizes |

### 4.5 Why not FAT12 as primary?

FAT12 wastes clusters/FAT tables, has no transparent compression, and limits packing tricks. FlopFS lets us:

1. Compress per 2 KiB logical block  
2. Dedup identical blocks across files  
3. Dense packed directories / long names without VFAT hacks  
4. Keep boot path under our control  

FAT remains for **interchange** and **foreign disks**.

---

# Track 4b — FlopFS integrity + dedup (P1, after current shell)

Primary FS stays **FlopFS** (not FAT). Spec: `docs/flopfs-spec.md` § Integrity + dedup.

- [ ] XXH3-64 per **file** payload (`size > 0`); dirs not hashed
- [ ] Size-0 files: **skip hash** (`xxh3=0`, no blob)
- [ ] Pack-time dedup in `mkflopfs` (same hash+size → shared LBA)
- [ ] Runtime refcount before freeing a shared blob
- [ ] Host tests: two identical HELLO copies → one data extent

---

# Track 5 — FAT12 / FAT16 / FAT32 / exFAT (P3 — bottom of queue)

**Deferred.** Guest MS-DOS on Py86 already does FAT12; we do not need a host `fat12.py`. FloppyOS foreign volumes come **after** FlopFS integrity/dedup and iron86 FDC write/format.

**Success:** mount and R/W (as appropriate) foreign volumes; format tools later.

| FS | Priority | Notes |
|----|----------|-------|
| FAT12 | P1 | Floppies, classic media; BPB parse; dual-boot Option B |
| FAT16 | P1 | HDD partitions, USB sticks (small) |
| FAT32 | P1–P2 | Large disks; pairs with **INT 13h LBA extensions** |
| exFAT | P2–P3 | Modern flash; patent/license diligence; read-first then write |

- [ ] Shared block I/O layer (CHS + LBA INT 13h)
- [ ] BPB / EBPB / FAT32 FSInfo parsers
- [ ] VFS switch: detect FlopFS magic @ LBA 1 **else** BPB → FAT family
- [ ] `fat.sys` device or kernel module for 12/16
- [ ] FAT32 + LBA (roadmap Phase 3)
- [ ] exFAT read-only prototype
- [ ] Host tests with known images (`Py86/IMG/msdos6_22disk*.img`, FreeDOS imgs)

### Inspiration

- Live BPB on `/dev/sdc` (read-only study)  
- MS-DOS 4.0 `DOS/FAT.ASM`, `DISK*.ASM` (structure, not paste)  
- FreeDOS kernel behavior via `/home/wizard/freedos-img` images (oracle)

---

# Track 6 — Shell (COMMAND.COM) (P1)

- [ ] Minimal COMMAND: internal `DIR`, `CD`, `COPY`, `DEL`, `TYPE`, `REN`, `PATH`, `SET`, batch
- [ ] Load via INT 21h AH=4Bh; permanent shell PSP
- [ ] Batch: `AUTOEXEC.BAT`, `IF`, `GOTO`, `CALL`
- [ ] Size target: competitive with FreeCOM / MS-DOS COMMAND (~30–55 KiB uncompressed; less on FlopFS)

### Inspiration (not ports)

| Source | Notes |
|--------|-------|
| MS-DOS 4 `CMD/COMMAND` | Behavioral reference (MIT) |
| FreeCOM | Ideas; **GPL** — license gate before any code reuse |
| `sshell/` | Modern shell architecture only — **do not port C++ bash-like shell to 16-bit** |
| Live `/tmp/floppy/COMMAND.COM` | Golden binary behavior |

---

# Track 7 — Size budget & packing (P0 continuous)

**Disk:** 1 474 560 bytes total.

| Component | Soft target (on-disk compressed) | Notes |
|-----------|----------------------------------|-------|
| Boot + stage1.5 + superblocks | ≤ 12–16 KiB | Reserved area |
| Kernel (IO+DOS) | 40–80 KiB | puff-packed |
| COMMAND.COM | 20–40 KiB | |
| Core drivers (optional set) | 0–40 KiB | Profile-selected |
| Free for apps/data | **≥ 1.2 MiB** physical ≈ **2–3.5 MiB** logical w/ compression | Goal |

- [ ] `docs/size-budget.md` with measured numbers after first image
- [ ] Strip messages; optional NLS packs on second disk
- [ ] UPX-like or custom EXE packer for utilities (own or open MIT)
- [ ] High-load everything possible (XMS/UMB) once HIMEM exists

Live reference system on `/tmp/floppy` is already ~**810 KiB** of files (many optional: EMM386, ZIP tools) — FloppyOS core must be **much** leaner.

---

# Track 8 — Gaming & modern hardware (P2) — from ROADMAP

### 8.1 Smart defaults & game profiles

- [ ] Auto CONFIG.SYS / AUTOEXEC maximizing conventional memory
- [ ] User files override
- [ ] Game EXE hash (XXH32/CRC32) + extender detect (`dos/detect_extenders`) → profile
- [ ] Example: Ultima VII-class pure real-mode profile

### 8.2 Classic drivers (high-loaded)

- [ ] HimemX (XMS)
- [ ] CuteMouse
- [ ] NANSI.SYS
- [ ] SHSUCDX + tiny CD
- [ ] VESA (FreeBE/AF / UniVBE ideas)
- [ ] Sound: SB Pro/16/AWE, GUS, MPU (SoftMPU ideas)

### 8.3 USB

- [ ] USB 1.1/2.0 mass-storage stack research (USBASPI / USBDDOS / FreeDOS usbdos — **ideas**)
- [ ] Optional HID
- [ ] No pure xHCI in 16-bit real mode — BIOS/EHCI fallback

### 8.4 Large disks / SATA

- [ ] INT 13h Extensions (LBA) end-to-end
- [ ] Prefer BIOS legacy for SATA; AHCI experimental later
- [ ] Ties to FAT32/exFAT

### 8.5 DPMI

- [ ] Optional CWSDPMI integration; conflict policy vs DOS/4GW games

---

# Track 8b — Local Py86 test harness (P0)

- [x] Local copy at `subprojects/py86/` (do **not** edit monorepo `Py86/`)
- [x] Config `tests/py86_floppyos.json` + `make run-py86` / `make smoke-py86`
- [x] Prefer `--display term` + control socket over QEMU on this host
- [ ] Stabilize FloppyOS boot under 5150 path (local FDC/BIOS tweaks OK)
- [ ] Optional later: **C++23 rewrite** of local Py86 for speed (`subprojects/py86-cpp/`)

# Track 9 — Testing strategy (P0 continuous)

| Layer | Tool | Role |
|-------|------|------|
| Daily | **DOSBox-X** | Fast boot + games |
| Image boot | **QEMU** | `-fda` full images |
| Accuracy | **86Box / PCem** | ISA, timing, sound |
| 8086 purity | **Py86** (`/tmp/RetroCodeMess/Py86`) | Real BIOS path; control socket CI |
| Geometry | **PyFloppy** | Sector images, HD35 constants |
| RE / traces | dumpexe + `8086-cga.conf` | Compatibility diffs |
| Real iron | MITSUMI `/dev/sdc` | **Last** step; human-confirmed write |

- [ ] CI-ish script: build image → QEMU serial/screenshot → prompt detect
- [ ] Contract tests for host tools (`mkflopfs`, `floppack`)
- [ ] Never claim green without command + exit code (`evidence-and-done`)

### Hardware write checklist (mandatory)

1. Unmount `/tmp/floppy` if mounted  
2. Show human: `lsblk`, `xxd` first sector, plan  
3. **Wait for explicit “yes, format /dev/sdc”**  
4. Only then `mkfs` / `dd` / `floppack --device`  
5. Verify read-back checksum  

---

# Track 10 — Host utilities & Linux byproduct (P3)

From original roadmap Phase 4:

- [ ] Enhanced COMMAND-like shell for Linux (drive letters, dual slash) — optional; `sshell` is separate project
- [ ] `EXE2BIN`, tiny `EDIT` ports
- [ ] Image convert: raw ↔ IMD ↔ (later) SCP via PyFloppy ideas

---

# Track 11 — Documentation (P1)

- [ ] `docs/architecture.md` — VFS, memory, boot
- [ ] `docs/flopfs-spec.md` — on-disk format v0.1
- [ ] `docs/fat-compat.md` — detection order, limitations
- [ ] `docs/boot-chain.md`
- [ ] `docs/size-budget.md`
- [ ] `docs/hardware-safety.md` — `/dev/sdc` policy
- [ ] `docs/inspiration.md` — monorepo map (below summary)
- [ ] Man pages for host tools (`man-pages` skill)

---

# Inspiration map (monorepo + home) — reference only

| Area | Location | Use as |
|------|----------|--------|
| MS-DOS 4.x sources (MIT) | `/home/wizard/MS-DOS` | Kernel/shell **behavioral base** |
| FreeDOS images | `/home/wizard/freedos-img` | Size/layout oracle |
| ArmorFS | `RetroCodeMess/ArmorFS` | Compressed CAS FS **design** |
| PyFloppy | `RetroCodeMess/PyFloppy` | Floppy geometry / images |
| zlib puff | `RetroCodeMess/zlib-1.3.2/contrib/puff` | Tiny inflate **reference** |
| dos extenders detect | `RetroCodeMess/dos` | Game profiles |
| RE / INT / EXE | `RetroCodeMess/retro` | Loader + INT checklist |
| AXE DOS skills | `RetroCodeMess/AXE/skills` | Test methodology |
| Py86 | `RetroCodeMess/Py86` | 8086 boot tests |
| sshell | `RetroCodeMess/sshell` | Shell UX ideas only |
| Live FAT12 system | `/dev/sdc` → `/tmp/floppy` | Real media study (**no format**) |
| MS-DOS 6.22 imgs | `/home/wizard/msdos6_22disk*.img`, Py86 `IMG/` | Golden OS |
| Period DOS utils | `retro/DOS/`, home `DOS6.22_*` | Utility behavior |

**Explicit non-bases for literal copy:** FreeDOS kernel/FreeCOM (GPL unless dual-license path), DriveSpace, proprietary extenders, CAPS/IPF library.

---

# Suggested implementation order (first 8 weeks)

| Week | Deliverable |
|------|-------------|
| 1 | Scaffold + host `mkimg1440` + boot sector that prints OK in QEMU |
| 2 | Stage1.5 + FlopFS superblock + pack empty image |
| 3 | ~~puff~~ **stored kernel stub load (M3 done)**; puff next |
| 4 | **INT 21h AH=02/09/4C (M4 done)**; expand services next |
| 5 | File open/read on FlopFS; load COM stub |
| 6 | MZ loader + trivial COMMAND prompt |
| 7 | FAT12 read path for foreign floppy images (emulator) |
| 8 | Size crunch + first “real” utility set on one 1.44M image |

Parallel: MS-DOS 4.0 upstream build for golden comparison (Track 3).

---

# Open decisions (need human input later)

1. **Kernel strategy:** clean-room reimplementation vs controlled MIT fork of MS-DOS 4.0 with heavy patches?  
2. **FlopFS name / magic** final (`FLOPFS01` vs `FOSFS001`)?  
3. **Dual-personality disks** (tiny FAT12 boot + FlopFS data) required for v1 or Option A only?  
4. **License of shipped image** if any GPL driver is bundled (keep core MIT-clean)?  
5. **When to write real USB floppy** — only after emulator green; human confirms each format of `/dev/sdc`.

---

# Track 12 — Nice-to-have later (from LINKS.txt audit, not crucial now)

Recorded 2026-07-26. **Do not block** boot/kernel/FlopFS work on these.

### 12.1 Sound / MIDI companions (host or external HW)

- [ ] **SoftMPU** path for games expecting Roland MPU ([bjt42/softmpu](https://github.com/bjt42/softmpu) — better than raw GUS-Timidity for FloppyOS)
- [ ] **munt/munt** — external MT-32 synth for SoftMPU (active, ★ high)
- [ ] **dwhinham/mt32-pi** — Pi-based MT-32 box companion
- [ ] Skip for OS tree: Nuked-SC55, emusc, BulkyMIDI-32, GUS-Timidity, audio-scripts (wrong layer)

### 12.2 CGA / period video QA

- [ ] **MobyGamer/CGACompatibilityTester** — DOS CGA register test suite (best of CGA batch)
- [ ] Skip PCB/sim: cga_sim, cga_artifact_color, CGA_Schematics, CGA_Redux, EGACard, EGAMemory, 5155-EGA, mce-adapter

### 12.3 BIOS study (reference only, not shipping ROMs)

- [ ] philspil66 / gawlas **IBM-PC-BIOS** (prefer philspil66 for 5150)
- [ ] ricardoquesada/bios-8088 (Tandy/PCjr disasm)
- [ ] kaneton/appendix-bios (AT 286)
- [ ] Better modern BIOS projects if ever needed: [GLaBIOS](https://github.com/640-KB/GLaBIOS), [skiselev/8088_bios](https://github.com/skiselev/8088_bios)
- [ ] Skip: virtualxt/pcxtbios (archived), retrobios dump catalog as code source

### 12.4 Optical / mass-storage lab (host HW)

- [ ] **libcdio** — host ISO/UDF tools (Phase 3 image mounting)
- [ ] **ZuluIDE-firmware** — real ATAPI CD emulator for SHSUCDX driver lab
- [ ] Skip: OmniDrive (disc dumping), DiscEmu-luckfox (ZuluIDE wins)

### 12.5 Shell / alternate OS curiosity

- [ ] FreeCOM (GPL) as behavioral oracle — already `subprojects/shell-freecom-notes`
- [ ] 4DOS-src — archived + license-restricted; ideas only
- [ ] Skip: CP/M (cpm22, cpmish), Tandy 2000 (non-PC-compatible)

### 12.6 Extenders / debug extras

- [ ] dos32a-ng for DOS/4GW-class games (P2 subproject exists)
- [ ] lDebug (ecm) for advanced host debugging (heavier than DOS-debug)
- [ ] UASM as optional JWasm alternative (not required for Japheth stack)
- [ ] VSBHDA (Japheth) if Sound Blaster HD audio stack wanted later
- [ ] Skip: Dos64-stub (long mode)

### 12.7 Video extras

- [ ] FreeBE/AF + Allegro 4.2 (giftware DOS path) for game demos
- [ ] Skip: PluMGMK vbesvga.drv (Win3.x/9x), SciTech Display Doctor dump, Dreamcast Allegro

---

# Track 13 — LINKS.txt integration (2026-07-26)

- [x] Group links → `docs/links-grouped.md`
- [x] Version/fork audits → `docs/links-audit-{A,B,C}.md`
- [x] P0/P1/P2 **subprojects** scaffolded under `subprojects/`
- [x] Nice-to-have recorded in Track 12
- [ ] Run `subprojects/*/fetch.sh` only when ready to build (optional)
- [ ] Wire first binaries: JemmEx + CTMOUSE into image budget once boot works
- [ ] Prefer **BootProg** patterns in Track 2 boot work
- [ ] Prefer **bochs-emu/VGABIOS** over qemu/vgabios for VBE reference
- [ ] Prefer **Jemm** over FDOS/emm386; **dos32a-ng** over amindlost/dos32a
- [ ] Student FAT* repos: **do not vendor** — use FatFs notes + FlopFS

### Better forks discovered (cheat sheet)

| LINKS.txt entry | Prefer instead |
|-----------------|----------------|
| FDOS/emm386 | Baron-von-Riedesel/**Jemm** |
| amindlost/dos32a | yetmorecode/**dos32a-ng** |
| qemu/vgabios | **bochs-emu/VGABIOS** |
| phoenixthrush/Tiny-C-Compiler | **gcc-ia16** / OpenWatcom (TCC dump archived) |
| Arquivotheca/4DOS-src | FDOS/**freecom** (still GPL) |
| Student fat12/fat16 repos | ChaN **FatFs/Petit** + BootProg **mkimg144** |
| Cacodemon345/VSBHDASF | Baron-von-Riedesel/**VSBHDA** (if sound later) |

---

# Immediate next actions

1. [x] Create full tree scaffold + `docs/hardware-safety.md`  
2. [x] `tools/mkimg1440` + boot sector → QEMU (`make smoke`)  
3. [x] **Milestone 2:** stage1.5 + FlopFS superblock LBA1 + empty pack image  
4. [x] Promote FlopFS design to `docs/flopfs-spec.md`  
5. [ ] Install **gcc-ia16** / use OpenWatcom `/opt/ow` + **JWasm**  
6. [ ] Link `/home/wizard/MS-DOS` in README; attempt v4.0 toolchain smoke  
7. [ ] **Do not** format `/dev/sdc` until human says so  

---

## Related reports (session artifacts)

- `/tmp/grok-1000/floppyos-explore-dos-boot.md`  
- `/tmp/grok-1000/floppyos-explore-fs-compress.md`  
- `/tmp/grok-1000/floppyos-explore-broad.md`  
- `/tmp/grok-1000/floppyos-links-grouped.md` (+ copies under `docs/`)  
- `/tmp/grok-1000/floppyos-links-audit-{A,B,C}.md`  
- `FloppyOS-ROADMAP.md` (original narrative phases 1–4)

**Note:** Explore agents reported `src/MS-DOS/` empty — correct for the monorepo placeholder. Authoritative MIT sources live at **`/home/wizard/MS-DOS`** (already cloned).

---

*Team: Grok (orchestrator) + explore subagents · Updated 2026-07-26*

---

# Bottom of queue (do not start until FlopFS integrity + iron86 gates 1/2/4/5)

These wait on purpose (2026-09-03 milestone `bios-flopfs-dir`):

- [ ] **FAT12 / FAT16 / FAT32 / exFAT** foreign volumes (Track 5) — Py86 never shipped `fat12.py`; MS-DOS DIR is guest FAT. FloppyOS primary FS is FlopFS.
- [ ] **iron86 FDC Format / Write Data / Scan / drive B:** — INT 19 boot and DIR only need Read. COPY/FCB write/format/second floppy later. See `subprojects/iron86/TODO.md`.
- [ ] CGA **light pen**, cycle-exact 6845 snow/wait-states, dual MDA+CGA “burn the tube” — no PC game used these.
