# FloppyOS Link Audit — Batch A (drivers · memory · toolchain)

**Date:** 2026-07-26  
**Scope:** GitHub repos for MS-DOS-compatible FloppyOS (1.44 MB floppy target)  
**Method:** GitHub HTML + REST API (`pushed_at`, releases, license, archived), plus web search for forks/successors  

### Priority key
| Pri | Meaning |
|-----|---------|
| **P0** | Need now for FloppyOS core / build / boot stack |
| **P1** | Important soon (compat, debug, optional memory) |
| **P2** | Later / optional (extenders, CD, 32-bit apps) |
| **skip** | Wrong target, dead mirror, or superseded by another row |

### Recommendation key
- **vendor as reference** — keep out-of-tree; study / copy patterns; ship prebuilt binaries if tiny  
- **wrap as subproject** — git submodule / subtree or vendored tree under FloppyOS  
- **ignore for now** — do not pull into tree yet  

---

## Summary table

| Repo | Default branch | Last push | Latest release | Archived? | Activity | Better / related forks | License | Pri | Recommendation |
|------|----------------|-----------|----------------|-----------|----------|------------------------|---------|-----|----------------|
| [Baron-von-Riedesel/HimemX](https://github.com/Baron-von-Riedesel/HimemX) | `master` | 2026-03-01 | **v3.40** (2026-03-01) | No | **Active** (Japheth) | None better; pairs with Jemm. HimemX2 variant for low EMB alloc | FD Himem **GPL and/or Artistic**; HimemX changes **PD** | **P0** | **Wrap as subproject** (or ship prebuilt `HIMEMX.EXE`/`HIMEMX2.EXE`); primary XMS host |
| [Baron-von-Riedesel/Jemm](https://github.com/Baron-von-Riedesel/Jemm) | `master` | 2026-05-26 | **v5.87pre1** (2026-05-26); stable **v5.86** (2026-01-25) | No | **Active** | **This is upstream**; FDOS/emm386 is a lagging mirror | **Partly Artistic** (see `Artistic.txt`); tools PD | **P0** | **Wrap as subproject** — prefer **JemmEx** (built-in XMM) to save conventional RAM on floppy |
| [FDOS/emm386](https://github.com/FDOS/emm386) | `master` | 2026-02-23 | (none own) | No | Mirror only | **Use [Baron-von-Riedesel/Jemm](https://github.com/Baron-von-Riedesel/Jemm)** | Same as Jemm (not SPDX-tagged) | **skip** | **Ignore** — FreeDOS packaging clone; PRs go upstream |
| [davidebreso/ctmouse](https://github.com/davidebreso/ctmouse) | `main` | 2024-12-13 | none | No | Quiet but useful | FreeDOS pkg [FDOS/mouse](https://github.com/FDOS/mouse); upstream SF [cutemouse](https://cutemouse.sourceforge.net/) v2.1b4 | **GPL-2.0** | **P0** | **Wrap as subproject** — modern JWasm/Linux build + 8086 fixes; best source tree for CuteMouse |
| [adoxa/shsucd](https://github.com/adoxa/shsucd) | `master` | 2026-04-23 | none (v3.x suite; readme 2022) | No | **Active** (Jason Hood) | Canonical; no better fork | **Freeware / permissive** (LICENSE.txt: use/alter/redistribute; no misrepresentation) | **P2** | **Vendor as reference** — MSCDEX replacement; not needed for pure floppy boot image |
| [FDOS/ansi](https://github.com/FDOS/ansi) | `master` | 2015-09-27 | NANSI **4.0d** (2007) | No | **Inactive** (1 commit dump) | Still the FreeDOS NANSI tree; no active successor found | **GPL** (`nansi.lsm`) | **P1** | **Vendor as reference** — ship prebuilt NANSI if ANSI console wanted; tiny TSR |
| [tkchia/gcc-ia16](https://github.com/tkchia/gcc-ia16) | `gcc-6_3_0-ia16-tkchia` | 2026-06-12 | via [build-ia16](https://github.com/tkchia/build-ia16) / Launchpad / GitLab releases | No | **Active** (de facto IA-16 GCC) | **This is the maintained fork** of crtc-demos/gcc-ia16; use **build-ia16** not raw gcc tree alone | **GPL-2.0** (+ GCC runtime exceptions) | **P0** | **Vendor as reference / external toolchain** — do **not** vendor full GCC tree; install via `build-ia16` or PPA; host-side only |
| [Baron-von-Riedesel/JWasm](https://github.com/Baron-von-Riedesel/JWasm) | `master` | 2026-06-30 | **v2.21pre1** (2026-02-06); stable **v2.20** (2025-11-30) | No | **Active** | [Terraspace/UASM](https://github.com/Terraspace/UASM) = feature fork; **Japheth JWasm** still best for HX/Jemm/HimemX | Sybase **Open Watcom Public License** (historically; not SPDX on GH) | **P0** | **Wrap as subproject** or host binary — required to rebuild Japheth stack + ctmouse |
| [Baron-von-Riedesel/DOS-debug](https://github.com/Baron-von-Riedesel/DOS-debug) | `master` | 2025-06-17 | **v2.51** (2025-06-17) | No | **Active** | **[lDebug](https://pushbx.org/ecm/web/#projects-ldebug)** (ecm) = enhanced successor (bootload, ELDs, scripts); keep Debug/X for small footprint | Freely redistributable (Paul Vojta terms in `debug.asm`; no SPDX) | **P1** | **Vendor as reference** — ship `DEBUG.COM`/`DEBUGX.COM` on floppy; consider lDebug for host/dev only |
| [Baron-von-Riedesel/HX](https://github.com/Baron-von-Riedesel/HX) | `master` | 2026-05-31 | **v2.24pre1** (2026-05-31); stable **v2.23** (2025-10-13) | No | **Active** | Canonical HX/HDPMI; no better fork | Not SPDX-tagged (Japheth HX license in package docs) | **P1** | **Vendor as reference** — HDPMI for DPMI apps; full HXRT too big for 1.44 MB OS image; host/dev useful |
| [Baron-von-Riedesel/Dos64-stub](https://github.com/Baron-von-Riedesel/Dos64-stub) | `master` | 2022-12-18 | **v1.0** (2020-11-08) | No | Stale | None | **GPL-2.0** | **skip** | **Ignore for now** — long-mode PE stub; needs 64-bit CPU + XMS; irrelevant to 1.44 MB real-mode floppy OS |
| [amindlost/dos32a](https://github.com/amindlost/dos32a) | `main` | 2022-01-21 | archival **v9.12** (2006 product) | No | Archive dump | Prefer **[yetmorecode/dos32a-ng](https://github.com/yetmorecode/dos32a-ng)** for buildable tree | Apache-like **DOS/32A Liberty** (“Other”) | **P2** | **Vendor as reference** only if preserving pristine 9.12; else use dos32a-ng |
| [yetmorecode/dos32a-ng](https://github.com/yetmorecode/dos32a-ng) | `main` | 2023-07-20 | **9.1.2** (2023-07-20) | No | Quiet (buildable) | **Best modern DOS/32A tree** (Docker/TASM build, legacy branches) | Same Liberty-style (**Other**) | **P2** | **Vendor as reference** — optional DOS/4GW-compatible extender; not for core floppy kernel |
| [phoenixthrush/Tiny-C-Compiler](https://github.com/phoenixthrush/Tiny-C-Compiler) | `main` | 2022-10-05 | none | **Yes** (2024-06-18) | Dead dump | **Official TCC:** [repo.or.cz/tinycc.git](https://repo.or.cz/tinycc.git); **DOS:** [tcc4dos](https://chiselapp.com/user/bencollver/repository/tcc4dos/) / Detlef Reimers HX port | **LGPL-2.1** | **skip** | **Ignore** — archived Win32 binary dump (2 commits). For on-floppy C: evaluate **tcc4dos + HX** or stick to **gcc-ia16** / Open Watcom |

---

## Per-repo notes (FloppyOS angle)

### Memory managers
1. **HimemX + Jemm** are the modern FreeDOS-class XMS/EMS stack (Japheth).  
2. For a **1.44 MB image**, prefer **JemmEx alone** (XMM embedded) over HimemX + Jemm386 to cut resident DOS memory and file count.  
3. HimemX2 only if Jemm warns about EMB above 16 MB (floppy DMA buffer placement).  
4. **FDOS/emm386** is explicitly a clone — never develop against it.

### Drivers
5. **ctmouse** (davidebreso) is the right source: 8086-clean, JWasm-buildable on Linux. Ship one small `CTMOUSE.EXE` on the floppy.  
6. **shsucd** (SHSUCDX) is excellent MSCDEX replacement but only matters if FloppyOS mounts ISO/CD images.  
7. **NANSI** is frozen GPL; still fine as a tiny optional `DEVICE=` for ANSI apps.

### Toolchain
8. **gcc-ia16 (tkchia)** is **P0 host toolchain** for 16-bit C. Use prebuilt packages from Launchpad/`build-ia16`; do not submodule the multi-GB GCC tree.  
9. **JWasm** is **P0** for assembling HimemX, Jemm, HX, DOS-debug, ctmouse. UASM is a parallel evolution — optional later, not required.  
10. **DOS-debug** for on-target debug; **lDebug** (ecm) if you need boot-time / scriptable debugging (heavier).  
11. **HX/HDPMI** for running/debugging DPMI and Win32-PE-under-DOS tools on the host or a “dev floppy,” not the minimal OS image.  
12. **Dos64-stub** and **phoenixthrush TCC** are out of scope for FloppyOS.

### DOS extenders
13. **dos32a-ng** > **amindlost/dos32a** for anything build-related.  
14. Both are **P2**: only if FloppyOS wants to run DOS/4GW-class games/apps.

---

## P0 shortlist (act first)

| # | Component | Why |
|---|-----------|-----|
| 1 | **tkchia/gcc-ia16** (+ build-ia16) | Primary 16-bit C compiler for FloppyOS userland/kernel helpers |
| 2 | **Baron-von-Riedesel/JWasm** | Assembler for memory managers, mouse, debug, HX |
| 3 | **Baron-von-Riedesel/Jemm** (JemmEx) | EMS/UMB/VCPI + optional built-in XMS |
| 4 | **Baron-von-Riedesel/HimemX** | Standalone XMS if not using JemmEx |
| 5 | **davidebreso/ctmouse** | Small open mouse driver, 8086-safe |

## Better forks / successors found

| Original | Prefer instead |
|----------|----------------|
| FDOS/emm386 | **Baron-von-Riedesel/Jemm** (upstream) |
| amindlost/dos32a | **yetmorecode/dos32a-ng** (buildable 2021+ tree) |
| phoenixthrush/Tiny-C-Compiler | Official **TinyCC** + **tcc4dos** (HX) if DOS TCC wanted |
| DOS-debug (feature depth) | **lDebug** (ecm) for advanced debug; keep Debug/X for size |
| JWasm (optional alt) | **UASM** (Terraspace) — not required for Japheth stack |
| CuteMouse SF sources | **davidebreso/ctmouse** for modern host builds |

---

## Suggested FloppyOS tree layout (if wrapping)

```text
third_party/
  jwasm/          # submodule or release tarball
  himemx/         # or binary-only dist/
  jemm/           # prefer building JemmEx
  ctmouse/
tools/
  ia16/           # document install of gcc-ia16; no full source tree
dist/floppy/
  HIMEMX.EXE | JEMMEX.EXE
  CTMOUSE.EXE
  DEBUG.COM       # optional
  NANSI.SYS       # optional
```

---

*Audit Batch A complete. Evidence: GitHub API `pushed_at` / releases as of 2026-07-26.*
