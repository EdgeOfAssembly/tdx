# FloppyOS Links Audit C — Sound / CGA-EGA / BIOS / Storage / Other

**Date:** 2026-07-26  
**Scope:** P2 (future nice-to-have) candidates — sound/MIDI, CGA/EGA, BIOS, storage HW, CP/M & misc.  
**Context:** FloppyOS is a **1.44 MB MS-DOS-compatible OS** (game-oriented drivers, SoftMPU/SB/GUS, VESA, CD via SHSUCDX). It is **not** a motherboard BIOS, FPGA core, or hardware PCB project.  
**Method:** GitHub API metadata (stars, push dates, archived flag) + README/description skim. Stats as of audit date.  
**Verdict key:**

| Tag | Meaning |
|-----|---------|
| **P2** | Keep on radar; useful later for FloppyOS (driver design, test suite, companion lab, docs) |
| **P2-companion** | Useful as *external* lab gear / host tool, not code to ship on the floppy |
| **P2-research** | Read-only reference (INT tables, schematics, dumps) — do not vendor |
| **SKIP** | Wrong layer, dead, niche HW, wrong platform, or zero FloppyOS leverage |

**Better alternatives called out** where the listed repo is superseded.

---

## 1. Sound / MIDI

### 1.1 [munt/munt](https://github.com/munt/munt) — **P2-companion**

| | |
|--|--|
| **What** | Canonical Roland MT-32 / CM-32L / LAPC-I software synth (`mt32emu` lib + Qt app + ALSA/Win drivers) |
| **Activity** | ★797 · 90 forks · last push **2026-06** · very alive |
| **Better forks** | Canonical upstream; no better fork. Integrated into DOSBox-X / many emulators. |
| **FloppyOS value** | Host-side only (C++ synth + ROMs). Does **not** fit a 1.44 MB DOS image. Roadmap already wants **SoftMPU + real/external Roland path** — munt is the gold-standard *external* synth for that path (alongside real MT-32 or mt32-pi). |
| **Action** | Document in game-profile MIDI notes: SoftMPU → serial/LPT MIDI → munt or hardware. Do not vendor. |

### 1.2 [dwhinham/mt32-pi](https://github.com/dwhinham/mt32-pi) — **P2-companion**

| | |
|--|--|
| **What** | Baremetal RPi 3+ kernel: Munt + FluidSynth → dedicated MT-32/GM box |
| **Activity** | ★1690 · 156 forks · last push **2025-02** · author states **no further releases** (burnout note in README); still popular |
| **Better forks** | Community forks exist for hardware packs; software core is frozen. Prefer stock releases + munt upstream for engine fixes. |
| **FloppyOS value** | Excellent **lab companion** for real-PC SoftMPU testing. Zero OS code. |
| **Action** | Optional lab setup only. |

### 1.3 [nukeykt/Nuked-SC55](https://github.com/nukeykt/Nuked-SC55) — **SKIP** (P3 curiosity)

| | |
|--|--|
| **What** | Cycle-level Roland SC-55 series emulator (MCU + PCM chip RE) |
| **Activity** | ★720 · 75 forks · last push **2025-10** · 77 open issues |
| **License** | MAME-style **non-commercial** (blocks commercial music boxes) |
| **Better forks** | Upstream is the accuracy king; emusc is freer license but less accurate (author admits this). |
| **FloppyOS value** | Host-only, heavy, needs ROMs, SC-55 era is late-DOS/Windows. FloppyOS sound plan is SB/GUS/MPU — not SC-55 softsynth on-floppy. |
| **Action** | Skip for FloppyOS. Optional host accuracy bench only. |

### 1.4 [skjelten/emusc](https://github.com/skjelten/emusc) — **SKIP**

| | |
|--|--|
| **What** | SC-55 reimplementation (libEmuSC LGPL + app GPL); extracts behavior from ROMs |
| **Activity** | ★273 · last push **2026-07** · active |
| **Better forks** | Nuked-SC55 for accuracy; this for redistributable-ish library experiments. |
| **FloppyOS value** | Same as Nuked — host synth, not DOS driver. |
| **Action** | Skip. |

### 1.5 [tebl/BulkyMIDI-32](https://github.com/tebl/BulkyMIDI-32) — **SKIP**

| | |
|--|--|
| **What** | Open hardware enclosure/modules for mt32-pi (“MIDI mountain”) |
| **Activity** | ★78 · last push **2025-03** |
| **FloppyOS value** | Pure PCB/case project. |
| **Action** | Skip. |

### 1.6 [kingbonj/GUS-Timidity](https://github.com/kingbonj/GUS-Timidity) — **SKIP**

| | |
|--|--|
| **What** | Gravis Ultrasound soundfont + Timidity cfg snippets |
| **Activity** | ★4 · last push **2022-04** · tiny (13 KB) |
| **FloppyOS value** | Host Timidity convenience only. Real GUS path for FloppyOS is **hardware GUS / UltraSound drivers**, not Timidity. |
| **Action** | Skip. |

### 1.7 [m13253/audio-scripts](https://github.com/m13253/audio-scripts) — **SKIP**

| | |
|--|--|
| **What** | Misc bash scripts for processing audio files |
| **Activity** | ★5 · last push **2017-05** · dead |
| **FloppyOS value** | Unrelated to DOS sound. |
| **Action** | Skip. |

### Sound summary — prefer instead

| Need | Prefer |
|------|--------|
| MPU-401 for games without hardware | **[bjt42/softmpu](https://github.com/bjt42/softmpu)** (★122, DOS TSR, roadmap already cites SoftMPU) |
| External MT-32 accuracy | munt / mt32-pi / real hardware |
| On-floppy SB/GUS | Period drivers + FreeDOS/Vogons references (not these repos) |

---

## 2. CGA / EGA

### 2.1 [dbalsom/cga_sim](https://github.com/dbalsom/cga_sim) — **SKIP** (P3 research)

| | |
|--|--|
| **What** | Verilog digital-logic sim of IBM CGA (for Digital simulator) |
| **Activity** | ★25 · last push **2026-03** · by MartyPC author |
| **FloppyOS value** | Deep HW timing research; FloppyOS does not implement a CGA card. |
| **Action** | Skip unless writing a cycle-accurate video doc. Prefer **MartyPC** for behavioral oracle. |

### 2.2 [dbalsom/cga_artifact_color](https://github.com/dbalsom/cga_artifact_color) — **SKIP**

| | |
|--|--|
| **What** | Rust tool: decode NTSC composite artifact color from CGA screenshots |
| **Activity** | ★10 · last push **2026-06** |
| **FloppyOS value** | Host image tool for demos/docs screenshots. Not OS code. |
| **Action** | Skip (bookmark if writing composite-video docs). |

### 2.3 [MobyGamer/CGACompatibilityTester](https://github.com/MobyGamer/CGACompatibilityTester) — **P2**

| | |
|--|--|
| **What** | DOS (Pascal) suite: register-level CGA compatibility tests for ISA video cards |
| **Activity** | ★60 · 9 forks · last push **2025-08** |
| **Better forks** | Canonical; MobyGamer is the authority. |
| **FloppyOS value** | **Best item in this section.** Use as golden test suite on 86Box/PCem/real CGA when validating video BIOS assumptions, mode sets, or any future CGA-aware utilities. Does not ship as a driver. |
| **Action** | Keep as **test oracle** under `tests/` inspiration list. |

### 2.4 [hkzlab/CGA_Schematics](https://github.com/hkzlab/CGA_Schematics) — **SKIP**

| | |
|--|--|
| **What** | IBM CGA schematics redrawn in KiCad |
| **Activity** | ★27 · last push **2026-06** |
| **FloppyOS value** | Hardware documentation only. |
| **Action** | Skip. |

### 2.5 [hkzlab/CGA_Redux](https://github.com/hkzlab/CGA_Redux) — **SKIP**

| | |
|--|--|
| **What** | Open-hardware CGA clone PCB from original schematics |
| **Activity** | ★54 · last push **2026-07** |
| **FloppyOS value** | Build-your-own ISA card — outside OS scope. |
| **Action** | Skip. |

### 2.6 [schlae/EGACard](https://github.com/schlae/EGACard) — **SKIP**

| | |
|--|--|
| **What** | EGA graphics card reference layout |
| **Activity** | ★18 · last push **2024-01** |
| **FloppyOS value** | HW only. |
| **Action** | Skip. |

### 2.7 [schlae/EGAMemory](https://github.com/schlae/EGAMemory) — **SKIP**

| | |
|--|--|
| **What** | 192K EGA “Moar RAM” expansion |
| **Activity** | ★7 · last push **2024-01** |
| **FloppyOS value** | HW only. |
| **Action** | Skip. |

### 2.8 [fjvva/IBM-5155-EGA-Adapter](https://github.com/fjvva/IBM-5155-EGA-Adapter) — **SKIP**

| | |
|--|--|
| **What** | Adapter: EGA → IBM 5155 internal mono monitor |
| **Activity** | ★0 · last push **2024-01** · one-off |
| **FloppyOS value** | Machine-specific hardware. |
| **Action** | Skip. |

### 2.9 [necroware/mce-adapter](https://github.com/necroware/mce-adapter) — **SKIP** (lab-only)

| | |
|--|--|
| **What** | RGBS converter for MDA / Hercules / CGA / EGA (GBS-friendly) |
| **Activity** | ★138 · 24 forks · last push **2025-02** |
| **FloppyOS value** | Useful if you own period cards + modern displays; zero software for FloppyOS. |
| **Action** | Skip for OS roadmap; fine as personal lab gear. |

### 2.10 [eliotw/IBM_PC](https://github.com/eliotw/IBM_PC) — **SKIP**

| | |
|--|--|
| **What** | Old Verilog IBM PC-related tree (no description) |
| **Activity** | ★6 · last push **2014-12** · dead · huge dump |
| **FloppyOS value** | Stale FPGA experiment. Prefer modern cores (ao486, etc.) if ever needed. |
| **Action** | Skip. |

---

## 3. BIOS

> FloppyOS is **OS software** loaded by the machine BIOS. These repos are useful as **behavioral references** for INT 10h/13h/16h expectations, not as something to ship.

### 3.1 [gawlas/IBM-PC-BIOS](https://github.com/gawlas/IBM-PC-BIOS) — **P2-research**

| | |
|--|--|
| **What** | Reconstruction of IBM PC / XT / AT / XT-286 BIOS sources |
| **Activity** | ★48 · last push **2020-08** · frozen |
| **Better** | Prefer **philspil66** for original 5150 listings; prefer **GLaBIOS / skiselev** for *modern open* BIOS. |
| **FloppyOS value** | Historical INT handler structure reference. |
| **Action** | Optional read-only reference. |

### 3.2 [philspil66/IBM-PC-BIOS](https://github.com/philspil66/IBM-PC-BIOS) — **P2-research** (prefer over gawlas for 5150)

| | |
|--|--|
| **What** | 1981–82 IBM PC BIOS reconstructed from Tech Ref listings |
| **Activity** | ★87 · last push **2021-09** · frozen |
| **FloppyOS value** | Cleaner “original PC” source study for boot/INT baseline. |
| **Action** | Prefer this over gawlas for 5150-era study. |

### 3.3 [ricardoquesada/bios-8088](https://github.com/ricardoquesada/bios-8088) — **P2-research**

| | |
|--|--|
| **What** | Disassembled BIOS ROMs: 5150, PCjr, Tandy 1000 family (+ IDA listings) |
| **Activity** | ★33 · last push **2025-02** |
| **FloppyOS value** | Clone/Tandy quirks matter for game compatibility notes. |
| **Action** | Keep as RE reference for Tandy/PCjr edge cases. |

### 3.4 [virtualxt/pcxtbios](https://github.com/virtualxt/pcxtbios) — **SKIP** (use better)

| | |
|--|--|
| **What** | Super PC/Turbo XT BIOS 3.1 (Plasma / phatcode lineage) |
| **Activity** | ★74 · **ARCHIVED** · last push 2024-11 |
| **Better forks / replacements** | **[640-KB/GLaBIOS](https://github.com/640-KB/GLaBIOS)** ★365, push 2026-07, modern open XT BIOS · **[skiselev/8088_bios](https://github.com/skiselev/8088_bios)** ★582, push 2026-07, Micro8088/NuXT/Xi8088 |
| **FloppyOS value** | Still a fine ROM for emulators, but archived; don’t build on it. |
| **Action** | Skip; cite GLaBIOS / 8088_bios if BIOS-level work ever appears. |

### 3.5 [kaneton/appendix-bios](https://github.com/kaneton/appendix-bios) — **P2-research**

| | |
|--|--|
| **What** | IBM AT 80286 BIOS (from PC-DOS retro materials) |
| **Activity** | ★135 · last push **2015-01** · frozen |
| **FloppyOS value** | AT-class INT 13h / CMOS / A20 reference. |
| **Action** | Read-only if implementing LBA/INT13h extensions docs. |

### 3.6 [abdess.github.io/retrobios/systems/ibm/](https://abdess.github.io/retrobios/systems/ibm/) — **SKIP**

| | |
|--|--|
| **What** | Catalog of IBM/PC-compatible BIOS **binary dumps** (hashes) for emulators |
| **Activity** | Static site (generated 2026-04) |
| **FloppyOS value** | Emulator ROM shopping list, not source. Legal/gray-area dumps. |
| **Action** | Skip for FloppyOS tree. Use only if configuring 86Box/PCem personally. |

### BIOS summary — prefer instead

| Need | Prefer |
|------|--------|
| Modern open XT BIOS | GLaBIOS, skiselev/8088_bios |
| Original IBM listings | philspil66 → gawlas |
| Clone quirks | ricardoquesada/bios-8088 |
| FloppyOS itself | MS-DOS 4.x IO.SYS/BIOS layer — **not** motherboard BIOS |

---

## 4. Storage HW

### 4.1 [libcdio/libcdio](https://github.com/libcdio/libcdio) — **P2** (host tools)

| | |
|--|--|
| **What** | Portable CD I/O library: ISO9660, UDF, SCSI MMC |
| **Activity** | ★58 · last push **2026-07** · active (GNU project mirror on GitHub) |
| **Better forks** | Canonical. Also `libcdio-paranoia`, `libisofs` ecosystem. |
| **FloppyOS value** | **Host-side** only (Linux tools for building/validating ISO images, Phase 3 ISO/CUE mount tooling). Not a DOS CD driver — that remains SHSUCDX + ATAPI/USBASPI class drivers. |
| **Action** | P2 for `tools/` when Phase 3 ISO work starts. |

### 4.2 [ZuluIDE/ZuluIDE-firmware](https://github.com/ZuluIDE/ZuluIDE-firmware) — **P2-companion**

| | |
|--|--|
| **What** | Firmware: emulates parallel ATA ATAPI CD-ROM or Zip/removable |
| **Activity** | ★128 · last push **2026-07** · active product |
| **Better forks** | Official ZuluIDE org is canonical (related: ZuluSCSI for SCSI). |
| **FloppyOS value** | Best **real-hardware CD test rig** for SHSUCDX / ATAPI driver work without spinning discs. |
| **Action** | Lab companion when CD stack is implemented. |

### 4.3 [RibShark/OmniDrive](https://github.com/RibShark/OmniDrive) — **SKIP**

| | |
|--|--|
| **What** | Patched firmware for MediaTek MT1959 LG/ASUS optical drives — raw/lead-in reads, Xbox/GC/Wii dumping |
| **Activity** | ★979 · last push **2026-07** · very hot |
| **FloppyOS value** | Archival disc-dumping firmware. Not a DOS storage stack. Wrong problem domain. |
| **Action** | Skip (cool project, zero FloppyOS leverage). |

### 4.4 [slzKud/DiscEmu-luckfox-pico-mini](https://github.com/slzKud/DiscEmu-luckfox-pico-mini) — **SKIP**

| | |
|--|--|
| **What** | CD-ROM emulator on Luckfox Pico Mini |
| **Activity** | ★2 · last push **2025-12** · niche |
| **Better** | **ZuluIDE** (mature, documented, PC-oriented). |
| **FloppyOS value** | Redundant weaker alternative to ZuluIDE. |
| **Action** | Skip. |

---

## 5. Other

### 5.1 [brouhaha/cpm22](https://github.com/brouhaha/cpm22) — **SKIP**

| | |
|--|--|
| **What** | CP/M 2.2 source |
| **Activity** | ★166 · last push **2024-07** |
| **FloppyOS value** | Different OS family (8080/Z80 BDOS). Historical curiosity only. |
| **Action** | Skip. |

### 5.2 [davidgiven/cpmish](https://github.com/davidgiven/cpmish) — **SKIP**

| | |
|--|--|
| **What** | Open “sort-of CP/M 2.2” distribution |
| **Activity** | ★403 · last push **2026-06** · active |
| **FloppyOS value** | Same — not MS-DOS. |
| **Action** | Skip. |

### 5.3 [Tandy2K/Tandy2000](https://github.com/Tandy2K/Tandy2000) — **SKIP**

| | |
|--|--|
| **What** | Tandy 2000 archive (80186, **not** IBM PC compatible) |
| **Activity** | ★30 · last push **2025-09** |
| **FloppyOS value** | Wrong machine architecture for MS-DOS PC games target. |
| **Action** | Skip. (Tandy **1000** is PC-compatible — use bios-8088 / Tandy docs instead.) |

### 5.4 [gist rubenerd/f92e1c…](https://gist.github.com/rubenerd/f92e1c10258ff083cd1e9b71f674e1c9) — **P2-research** (micro)

| | |
|--|--|
| **What** | One-page EMM386.EXE version table (DOS 6.x / WfW / Win95) |
| **Activity** | Created 2017 · static |
| **FloppyOS value** | Tiny but useful when documenting HimemX/EMM386 game profiles. |
| **Action** | Copy the version table into FloppyOS memory-manager docs; no dependency. |

### 5.5 [wiki.osdev.org/Accelerated_Graphic_Cards](https://wiki.osdev.org/Accelerated_Graphic_Cards) — **P2-research**

| | |
|--|--|
| **What** | OSDev wiki: VESA, linear framebuffer, vendor SVGA notes |
| **Activity** | Living wiki |
| **FloppyOS value** | Directly supports roadmap **VESA 1.x/2.x** driver work (FreeBE/AF, UniVBE references). |
| **Action** | Read when implementing VESA; pair with FreeBE/AF sources (not this audit list). |

---

## 6. Master verdict tables

### Nice later (P2 / P2-companion / P2-research)

| Item | Tag | Why keep |
|------|-----|----------|
| **munt/munt** | P2-companion | External MT-32 path for SoftMPU gaming |
| **dwhinham/mt32-pi** | P2-companion | Same, dedicated Pi box (frozen but usable) |
| **MobyGamer/CGACompatibilityTester** | **P2** | DOS CGA register test suite |
| **philspil66/IBM-PC-BIOS** | P2-research | Original 5150 BIOS reconstruction |
| **gawlas/IBM-PC-BIOS** | P2-research | Broader IBM PC/XT/AT reconstruction |
| **ricardoquesada/bios-8088** | P2-research | Clone/Tandy/PCjr disassemblies |
| **kaneton/appendix-bios** | P2-research | AT 286 BIOS reference |
| **libcdio/libcdio** | **P2** | Host ISO/UDF tooling for Phase 3 |
| **ZuluIDE/ZuluIDE-firmware** | P2-companion | Real ATAPI CD emulator for driver tests |
| **rubenerd EMM386 gist** | P2-research | Version table for memory docs |
| **OSDev Accelerated Graphic Cards** | P2-research | VESA/SVGA background |

**Not on the original list but better than several BIOS entries:**

| Item | Why |
|------|-----|
| [640-KB/GLaBIOS](https://github.com/640-KB/GLaBIOS) | Modern open XT BIOS (prefer over archived pcxtbios) |
| [skiselev/8088_bios](https://github.com/skiselev/8088_bios) | Active 8088 BIOS for homebrew XT boards |
| [bjt42/softmpu](https://github.com/bjt42/softmpu) | Actual DOS MPU-401 TSR (roadmap-aligned) |

### Pure skip

| Item | Why |
|------|-----|
| nukeykt/Nuked-SC55 | Host SC-55 emu; license + weight; not floppy DOS |
| skjelten/emusc | Same class as Nuked |
| tebl/BulkyMIDI-32 | mt32-pi hardware enclosure |
| kingbonj/GUS-Timidity | Stale Timidity soundfont cfg |
| m13253/audio-scripts | Dead unrelated bash scripts |
| dbalsom/cga_sim | Verilog CGA sim — not OS |
| dbalsom/cga_artifact_color | Host screenshot tool |
| hkzlab/CGA_Schematics | KiCad schematics |
| hkzlab/CGA_Redux | CGA clone PCB |
| schlae/EGACard | EGA PCB layout |
| schlae/EGAMemory | EGA RAM expansion HW |
| fjvva/IBM-5155-EGA-Adapter | 5155-specific adapter |
| necroware/mce-adapter | Video converter HW |
| eliotw/IBM_PC | Dead 2014 Verilog dump |
| virtualxt/pcxtbios | **Archived**; use GLaBIOS/8088_bios |
| abdess retrobios IBM page | Binary ROM catalog for emulators |
| RibShark/OmniDrive | Optical-drive dumping firmware |
| slzKud/DiscEmu-luckfox-pico-mini | Niche CD emu; ZuluIDE wins |
| brouhaha/cpm22 | CP/M, not MS-DOS |
| davidgiven/cpmish | CP/M-ish, not MS-DOS |
| Tandy2K/Tandy2000 | Non-PC-compatible machine archive |

---

## 7. FloppyOS priority guidance (this batch)

1. **Do not** pull any of these into the 1.44 MB image except possibly **shipping SoftMPU** (already on roadmap; not in this list) and period SB/GUS drivers.  
2. **Highest leverage from this list:** CGACompatibilityTester (tests), libcdio (host ISO tools), SoftMPU+munt/mt32-pi (MIDI story), OSDev VESA page + FreeBE/AF (graphics).  
3. **BIOS reconstructions** are study material only — FloppyOS boots *under* BIOS, it does not replace it.  
4. **All CGA/EGA PCB / adapter repos** are pure skip for an OS project.  
5. **CP/M / Tandy 2000 / OmniDrive** are interesting retro projects with **no** FloppyOS path.

---

*Audit C complete. Stats from GitHub API / pages, 2026-07-26.*
