# FloppyOS Links Audit B — Boot / FAT / Video / Shell

**Date:** 2026-07-26  
**Scope:** Batch B candidates — boot sectors, FAT12/16 readers, command shell, VESA/VBE video, FreeBE/AF, Allegro.  
**Context:** FloppyOS is a **1.44 MB MS-DOS-compatible OS** (game-oriented drivers, SoftMPU/SB/GUS, VESA, CD via SHSUCDX). Boot path is FAT12 floppy → stage2 → kernel; FS is FlopFS + FAT tracks.  
**Method:** GitHub page/README skim + web search for push dates, licenses, successors. GitHub REST API partially rate-limited during audit; dates from commits pages / search corroboration.  
**Verdict key:**

| Tag | Meaning |
|-----|---------|
| **P0** | Ship-critical or first-class reference for FloppyOS core path |
| **P1** | Strong secondary reference / optional subproject |
| **P2** | Keep on radar; later / niche / companion |
| **SKIP** | Wrong layer, dead homework, wrong platform, or zero FloppyOS leverage |

**Recommendation key:** `reference` (read/adapt patterns) · `subproject` (vendor or track as dependency) · `ignore`

**Better alternatives** called out where the listed repo is superseded.

---

## Executive summary — best picks now

| Need | Pick | Why |
|------|------|-----|
| **Boot sector (FAT12 1.44M)** | **[alexfru/BootProg](https://github.com/alexfru/BootProg)** | Purpose-built 512-byte FAT12/16/32 loaders; loads `STARTUP.BIN` (.COM/.EXE); includes `flp144` + `mkimg144`; BSD-2; NASM. **P0 · subproject/reference** |
| **FAT runtime (kernel FS)** | **ChaN FatFs / Petit FatFs** (not in this batch) | Prefer over student FAT12/16 homework. Host tools: mtools / dosfstools. |
| **FAT learning / image tools** | **qihaiyan/fat12** (weak P2) | Only marginally useful; FreeBSD-derived headers. All other FAT* in batch → **SKIP** |
| **Shell** | **[FDOS/freecom](https://github.com/FDOS/freecom)** (successor) | GPL-2, active. 4DOS-src is archived + license-restricted. |
| **VBE *semantics* (INT 10h AX=4Fxx)** | **[bochs-emu/VGABIOS](https://github.com/bochs-emu/VGABIOS)** (`vbe.c`) | Active LGPL-2.1 successor of qemu/vgabios mirror. **P0 · reference** (not a physical-card driver) |
| **DOS game graphics lib** | **Allegro 4.2.x official / [msikma/allegro-4.2.2-xc](https://github.com/msikma/allegro-4.2.2-xc)** | Giftware; real DOS/DJGPP path. DC port is wrong platform. |
| **VBE/AF accelerated drivers** | **FreeBE/AF 1.2** (Shawn Hargreaves site) | Historical Allegro-era VBE/AF; free. SciTech Display Doctor is binary dump only. |
| **Win3.1 VBE driver** | PluMGMK/vbesvga.drv | Excellent project, **wrong OS layer** for FloppyOS. |
| **Sound (misfiled in video group)** | **[Baron-von-Riedesel/VSBHDA](https://github.com/Baron-von-Riedesel/VSBHDA)** | Upstream of VSBHDASF; SF already merged. |

---

## 1. Boot

### 1.1 [alexfru/BootProg](https://github.com/alexfru/BootProg) — **P0** · **subproject / reference**

| | |
|--|--|
| **What** | Collection of **512-byte** x86 boot sectors that parse FAT12/16/32, find `STARTUP.BIN`, load .COM/.EXE (with relocations), hand off with BIOS drive in `DL` and magic SI/DI/BP markers for hybrid DOS/bare-metal programs |
| **Default branch** | `master` |
| **Activity** | 9 commits; last major **2023-04-15** (“V 2.0: Major improvements”); ~114★ / 12 forks. Mature / stable, not abandoned junk |
| **Archived?** | No |
| **License** | **BSD-2-Clause** (`license.txt`, © 2000–2023 Alexey Frunze) |
| **Artifacts** | `boot12.asm` / `flp144.asm` + bins; `boot16` (CHS/LBA); `boot32` (LBA/CHS); `mkimg144.c`; `demo1.com` |
| **Better forks / successors** | Upstream is canonical. Related: [alexfru/MBiRa](https://github.com/alexfru/MBiRa) multi-boot manager. No better “enhanced fork” needed |
| **FloppyOS value** | **Direct hit** for 1.44 MB FAT12 boot path. Loads a second-stage or entire small OS as `STARTUP.BIN` without requiring contiguous clusters. Works with Watcom/Turbo C / NASM toolchains FloppyOS already cares about. Does **not** provide INT 21h — correct for pre-DOS stage |
| **Caveats** | No size check on `STARTUP.BIN` (may read extra cluster tail); CHS geometry via INT 13h/08 on HDDs; boot32 needs 386+ |
| **Action** | **Adopt as primary boot-sector reference.** Consider vendoring `flp144`/`boot12` + `mkimg144` under `boot/` with attribution. Integrate into image-build scripts |

---

## 2. FAT12 / FAT16 (batch candidates)

> **Batch-wide note:** Every FAT* repo below is a **student / hobby host-side image reader** (or one-shot formatter). None is a production in-kernel FS for a DOS-compatible OS. Prefer **ChaN [FatFs](https://elm-chan.org/fsw/ff/)** / **[Petit FatFs](https://elm-chan.org/fsw/ff/00index_p.html)** for runtime, **mtools/dosfstools** for host image tooling, and Microsoft **fatgen103** + OSDev FAT wiki for on-disk layout truth.

### 2.1 [qihaiyan/fat12](https://github.com/qihaiyan/fat12) — **P2** · **reference (weak)**

| | |
|--|--|
| **What** | Simple FAT12 tools: `dos_ls`, `dos_cp`, planned `dos_scandisk`; headers trimmed from **FreeBSD** FAT12 |
| **Default branch** | `master` |
| **Activity** | 7 commits; last push **2024-01-17** (PR merge fixing FAT entry offset); original work ~2016 |
| **Archived?** | No |
| **License** | **Not declared** on GitHub (FreeBSD-derived headers → treat as BSD-ish; verify before copy) |
| **Better forks** | None meaningful. Prefer FatFs / FreeBSD `msdosfs` sources if you want real code |
| **FloppyOS value** | Host-side floppy image ls/cp patterns; BPB/dirent header shapes. Not ship-on-floppy |
| **Action** | Optional read for structure names only. Do not vendor |

### 2.2 [RPAnimation/FAT12](https://github.com/RPAnimation/FAT12) — **SKIP**

| | |
|--|--|
| **What** | FAT12 image reader with POSIX-like open/read/seek API; sample image included |
| **Default branch** | `main` |
| **Activity** | 4 commits; last push **2021-04-16**; ★1; updated_at 2024 (metadata only) |
| **Archived?** | No (but dead) |
| **License** | **None** |
| **Better forks** | None |
| **FloppyOS value** | Coursework-quality reader; no write path; no boot integration |
| **Action** | Ignore |

### 2.3 [variousCodingTasks/FAT12](https://github.com/variousCodingTasks/FAT12) — **SKIP**

| | |
|--|--|
| **What** | Mini project: create a formatted **1.44 MB** FAT12 image (BPB, 2 FATs, root, data) |
| **Default branch** | `master` |
| **Activity** | **1 commit** only; NetBeans `nbproject` cruft |
| **Archived?** | No (but one-shot homework) |
| **License** | **None** |
| **Better forks** | `mkfs.fat`, BootProg’s `mkimg144`, mtools |
| **FloppyOS value** | Concept only (format a 1.44M image). Implementation not worth extracting |
| **Action** | Ignore — use `mkimg144` or dosfstools |

### 2.4 [MightyPork/fat16](https://github.com/MightyPork/fat16) — **P2** · **reference (optional)**

| | |
|--|--|
| **What** | Lightweight simplified **FAT16** C library (`fat16.c`/`fat16.h` + `blockdev.h` abstraction) |
| **Default branch** | `master` |
| **Activity** | 19 commits; last **2015-06-10**; frozen |
| **Archived?** | No (inactive) |
| **License** | **MIT** |
| **Better forks** | **ChaN FatFs** (maintained, FAT12+16+32, configurable size) |
| **FloppyOS value** | Clean block-device split is pedagogically nice; too old/incomplete vs FatFs |
| **Action** | Skim API style if designing FlopFS block layer; do not vendor |

### 2.5 [rweichler/FAT16](https://github.com/rweichler/FAT16) — **P2** · **reference (docs)**

| | |
|--|--|
| **What** | C reader for FAT16 images; ships **spec/** with FAT layout PDF + Microsoft fatgen103 |
| **Default branch** | `master` |
| **Activity** | 16 commits; last **2015-06-04**; frozen |
| **Archived?** | No (inactive) |
| **License** | **None** declared |
| **Better forks** | Code: FatFs. Specs: keep Microsoft fatgen103 from official source |
| **FloppyOS value** | The **bundled PDFs / layout notes** are the only durable asset |
| **Action** | Grab layout references if needed; ignore C code |

### 2.6 [Daste745/fat16](https://github.com/Daste745/fat16) — **SKIP**

| | |
|--|--|
| **What** | FAT16 reader library; author admits WIP/bugs; “will try FAT12 and fail miserably” |
| **Default branch** | `master` |
| **Activity** | 4 commits; last push **2022-02-23**; ★2 |
| **Archived?** | No |
| **License** | **MPL-2.0** |
| **Better forks** | FatFs |
| **FloppyOS value** | Explicitly incomplete |
| **Action** | Ignore |

### FAT summary — prefer instead

| Need | Prefer |
|------|--------|
| In-kernel / bootloader FAT12 R/W | **Petit FatFs** (tiny) or full **FatFs** configured read-only/minimal |
| Boot-time load only | **BootProg** (already finds file via FAT) + own stage2 |
| Host create/mount 1.44M images | `mkfs.fat -F 12`, mtools, BootProg `mkimg144` |
| On-disk format truth | Microsoft fatgen103 + OSDev FAT wiki |
| Student repos in this batch | **Ignore** (except weak header/docs skim) |

---

## 3. Shell

### 3.1 [Arquivotheca/4DOS-src](https://github.com/Arquivotheca/4DOS-src) — **P2** · **reference (archive only)**

| | |
|--|--|
| **What** | Archive pointer to **4DOS** sources (branches per release: `4dos7501`, `4dos8`); README-only on `master` |
| **Default branch** | `master` |
| **Activity** | 1 commit on master; **archived 2024-08-14** (read-only) |
| **Archived?** | **Yes** |
| **License** | **Not OSI open source** — modified MIT-style with **“FreeDOS only” compile restriction** (Rex Conn). Community practice allows other DOS variants; still awkward for a clean FloppyOS tree |
| **Better forks / successors** | Binaries/sources: [4dos.info](https://4dos.info/sources.htm), FreeDOS ibiblio mirror. **Active open shell:** [FDOS/freecom](https://github.com/FDOS/freecom) (GPL-2, last activity 2026). Windows lineage: JP Software TCC / Take Command |
| **FloppyOS value** | Feature inspiration (aliases, DESCRIPT.ION, history) for a rich shell — **not** a clean dependency. Size of full 4DOS may fight 1.44 MB budget |
| **Action** | Reference feature list only. Prefer **FreeCOM** as COMMAND.COM-compatible base; optional later “enhanced shell” design notes from 4DOS docs |

---

## 4. Video / VESA / graphics

### 4.1 [qemu/vgabios](https://github.com/qemu/vgabios) — **P0** · **reference** (use successor tree)

| | |
|--|--|
| **What** | LGPL VGA BIOS for **emulated** VGA (Bochs/QEMU/plex86). Includes **`vbe.c` / `vbe.h`** — Bochs VBE DISPI interface, mode tables, INT 10h AX=4Fxx paths. **Not** a driver for physical cards |
| **Default branch** | `master` |
| **Activity** | Mirror of old git.qemu.org tree; history ends ~**2010** era on this mirror (171 commits). **Active development moved** |
| **Archived?** | Not formally, but **stale mirror** |
| **License** | **LGPL-2.1** (`COPYING`) |
| **Better forks / successors** | **[bochs-emu/VGABIOS](https://github.com/bochs-emu/VGABIOS)** — current LGPL VGABIOS (commits into **2025**); QEMU ships prebuilt `pc-bios` binaries from this project line |
| **FloppyOS value** | Gold-standard reference for **VBE client expectations** (mode info blocks, LFB flags, protected-mode interface notes, Cirrus extension contrast). Use when implementing FloppyOS VESA services or validating against QEMU/Bochs |
| **Action** | **Reference `vbe.c` + `vbe_display_api.txt` from bochs-emu/VGABIOS.** Do not ship as option ROM on real hardware. Do not treat qemu/ mirror as upstream |

### 4.2 [PluMGMK/vbesvga.drv](https://github.com/PluMGMK/vbesvga.drv) — **SKIP** (P3 curiosity / wrong OS)

| | |
|--|--|
| **What** | Modern **Windows 3.1 / 9x** generic SVGA display driver + VDD + grabber using VBE; True Color Full HD on modern GPUs |
| **Default branch** | `master` |
| **Activity** | **Very active** — 593+ commits; latest release **v1.0-beta4** (2026-07-25); issue templates Nov 2025 |
| **Archived?** | No — thriving retro Win16 project |
| **License** | **None declared** (Microsoft/Headland DDK heritage + new code; treat as all-rights-reserved / ask author) |
| **Better forks** | Canonical for Win3.1 VBE display; N/A for DOS |
| **FloppyOS value** | Wrong target OS. Useful only if studying **VBE mode enumeration / bank vs LFB** edge cases on real modern GPUs (VIDMODES.COM ideas). Cannot ship as FloppyOS video stack |
| **Action** | Ignore for FloppyOS core. Optional lab note: “how modern VBE BIOSes misbehave” |

### 4.3 [TolgaBagci/scitech-display-doctor-7](https://github.com/TolgaBagci/scitech-display-doctor-7) — **SKIP**

| | |
|--|--|
| **What** | Single file: `scitech-display-doctor-7.rar` — dump of SciTech **Display Doctor / UniVBE** era package |
| **Default branch** | `main` |
| **Activity** | **1 commit**; no source |
| **Archived?** | No (but not a software project) |
| **License** | **Proprietary** SciTech binary (redistribution status murky) |
| **Better forks / successors** | Historical UniVBE was the commercial VBE TSR. Free path: **FreeBE/AF** + BIOS VBE. No open UniVBE source |
| **FloppyOS value** | Binary archaeology only; legal/redistrib risk; huge vs floppy budget |
| **Action** | Ignore. Do not vendor RAR dumps |

### 4.4 [Cacodemon345/VSBHDASF](https://github.com/Cacodemon345/VSBHDASF) — **SKIP** (misfiled; use upstream)

| | |
|--|--|
| **What** | Fork of Japheth’s **VSBHDA** — Sound Blaster emulation on HDA/AC97/SBLive with **TinySoundFont** MPU-401. **Not video** |
| **Default branch** | `main` |
| **Activity** | ~100 commits on fork; README: **“mostly halted since VSBHDA v1.7 added soundfont support from this fork”** |
| **Archived?** | No |
| **License** | Follow upstream VSBHDA (check Japheth’s terms; typically source-available DOS tooling) |
| **Better forks / successors** | **[Baron-von-Riedesel/VSBHDA](https://github.com/Baron-von-Riedesel/VSBHDA)** — active upstream (~★150, releases through v1.9 series); SF support already merged |
| **FloppyOS value** | Sound stack (Phase 2+), not video. Needs HDPMI + Jemm + lots of XMS for SF — heavy for 1.44 MB OS image; better as **optional host/real-PC companion** for game audio |
| **Action** | Track **upstream VSBHDA** under sound audit, not video. Ignore this fork |

### 4.5 [FreeBE/AF](https://shawnhargreaves.com/freebe/) — **P1** · **reference**

| | |
|--|--|
| **What** | Free **VBE/AF 2.0** accelerated graphics drivers (vbeaf.drv) for Allegro/MGL era: Matrox, S3, Cirrus 54x, mach64, NVidia Riva/TNT, TGUI 9440, plus dumb framebuffer ports of old Allegro chipset drivers |
| **Host** | Non-GitHub: shawnhargreaves.com (Shawn Hargreaves / Allegro author) |
| **Activity** | Last release **v1.2 (1999-06-27)** — historical freeze. Still the only free VBE/AF driver set |
| **Archived?** | Project complete / abandoned (SciTech moved to closed Nucleus) |
| **License** | **Free** — binaries and sources may be distributed and modified **without restriction** (site copyright section) |
| **Downloads** | [freebb12.zip](https://shawnhargreaves.com/freebe/freebb12.zip) (bin), [freebs12.zip](https://shawnhargreaves.com/freebe/freebs12.zip) (src) |
| **Better successors** | None for VBE/AF. Modern path is raw **VBE 2/3 LFB** or vendor-specific. Allegro 4 still documents FreeBE |
| **FloppyOS value** | If FloppyOS games use **Allegro 4** or a VBE/AF loader, FreeBE is the free accelerator pack for period hardware. Driver files are large relative to 1.44 MB — ship **per-card optional** or document “install FreeBE to C:\” |
| **Action** | **Reference** for VBE/AF API (`vbeaf.h`) and hardware notes. Optional companion download, not core OS |

### 4.6 [ianmicheal/DCAllegroVersion-4.2.2](https://github.com/ianmicheal/DCAllegroVersion-4.2.2) — **SKIP**

| | |
|--|--|
| **What** | **Dreamcast** port of Allegro 4.2.2 (Chui’s port, re-uploaded “from the archive”) |
| **Default branch** | `master` |
| **Activity** | 16 commits; last **2020-06-10** (“Add files via upload”) |
| **Archived?** | No (dead dump) |
| **License** | Allegro 4 **giftware** (permissive) |
| **Better forks / successors** | **DOS Allegro 4.2.x**: [liballeg.org/old.html](https://liballeg.org/old.html); cross-build: [msikma/allegro-4.2.2-xc](https://github.com/msikma/allegro-4.2.2-xc); modern: Allegro 5 (not DOS-native) |
| **FloppyOS value** | Wrong platform (SH4/Dreamcast). Zero x86 DOS leverage |
| **Action** | Ignore. If Allegro wanted: official 4.2.2 DOS/DJGPP tree |

### Video summary — prefer instead

| Need | Prefer |
|------|--------|
| VBE mode set / LFB from DOS | BIOS INT 10h AX=4Fxx + own thin wrapper; validate vs **bochs-emu/VGABIOS** `vbe.c` |
| Period accelerated API | **FreeBE/AF** + Allegro 4 (optional) |
| Win3.1 high-res | vbesvga.drv (not FloppyOS) |
| UniVBE binary dump | Skip (legal + size) |
| Game multimedia lib on DOS | **Allegro 4.2.x giftware** (not DC port) |

---

## 5. Cross-batch recommendation matrix

| # | Repo / site | Branch | Last activity (approx) | License | Priority | Recommendation |
|---|-------------|--------|------------------------|---------|----------|----------------|
| 1 | alexfru/BootProg | master | 2023-04 (v2.0) | BSD-2 | **P0** | **subproject / reference** |
| 2 | qihaiyan/fat12 | master | 2024-01 | undeclared | P2 | reference (weak) |
| 3 | RPAnimation/FAT12 | main | 2021-04 | none | SKIP | ignore |
| 4 | variousCodingTasks/FAT12 | master | 1 commit | none | SKIP | ignore |
| 5 | MightyPork/fat16 | master | 2015-06 | MIT | P2 | reference (optional) |
| 6 | rweichler/FAT16 | master | 2015-06 | none | P2 | reference (docs only) |
| 7 | Daste745/fat16 | master | 2022-02 | MPL-2.0 | SKIP | ignore |
| 8 | Arquivotheca/4DOS-src | master | archived 2024 | restricted | P2 | reference (archive); prefer FreeCOM |
| 9 | qemu/vgabios | master | stale mirror | LGPL-2.1 | **P0** | reference → **bochs-emu/VGABIOS** |
| 10 | PluMGMK/vbesvga.drv | master | 2026-07 (v1.0-β4) | undeclared | SKIP | ignore (Win3.1) |
| 11 | TolgaBagci/scitech-display-doctor-7 | main | 1 commit RAR | proprietary | SKIP | ignore |
| 12 | Cacodemon345/VSBHDASF | main | halted (merged up) | per upstream | SKIP* | use **VSBHDA** under sound |
| 13 | FreeBE/AF (shawnhargreaves) | n/a | 1999 v1.2 | free/unrestricted | **P1** | reference / optional companion |
| 14 | ianmicheal/DCAllegro… | master | 2020-06 | giftware | SKIP | ignore; use DOS Allegro 4.2 |

\*VSBHDASF is sound, not video — reclassify to sound batch.

---

## 6. Suggested FloppyOS wiring (Batch B)

```
[Boot]
  flp144 / boot12 (BootProg)  -->  STARTUP.BIN (stage2)
  mkimg144 or mkfs.fat -F 12  -->  CI floppy image

[FS]
  BootProg for load-from-FAT only
  Runtime: FatFs/Petit or custom FlopFS  (NOT student FAT repos)

[Shell]
  FreeCOM (GPL-2) as COMMAND.COM-class
  4DOS docs for feature wishlist only

[Video]
  INT 10h + VBE 2.0 LFB client code
  Validate modes against bochs-emu/VGABIOS vbe.c under QEMU
  Optional: FreeBE/AF for Allegro games on real SVGA
  Optional later: Allegro 4.2 DOS (giftware) as app framework — not DC port

[Sound — misfiled]
  Baron-von-Riedesel/VSBHDA (not VSBHDASF fork)
```

---

## 7. Out-of-batch upgrades (do these instead of weak Batch B FAT/shell/video)

| Gap in Batch B | Better source |
|----------------|---------------|
| Production FAT | ChaN FatFs / Petit FatFs |
| Active COMMAND.COM | https://github.com/FDOS/freecom |
| Live VGABIOS | https://github.com/bochs-emu/VGABIOS |
| DOS Allegro 4.2 | https://liballeg.org/old.html or msikma/allegro-4.2.2-xc |
| SB on modern HW | https://github.com/Baron-von-Riedesel/VSBHDA |
| Boot manager (multi) | https://github.com/alexfru/MBiRa |

---

*Audit B complete. Primary ship-now items from this batch: **BootProg** + **VGABIOS/vbe.c (bochs-emu)**; secondary **FreeBE/AF** + **FreeCOM**; discard student FAT repos, Win3.1 driver, SciTech RAR, Dreamcast Allegro, and the halted VSBHDASF fork.*
