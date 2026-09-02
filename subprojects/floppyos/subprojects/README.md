# FloppyOS subprojects

Companion projects that help FloppyOS **right now**.  
Upstream code is **not copied** into the core OS tree; each subproject documents preferred upstreams, licenses, fetch steps, and how it plugs into FloppyOS.

**Source list:** `/tmp/LINKS.txt`  
**Grouping:** `docs/links-grouped.md`  
**Audits:** `docs/links-audit-A.md` … `C.md` (copied from session reports)

## Priority subprojects (P0 — start here)

| Dir | Role | Preferred upstream |
|-----|------|--------------------|
| [toolchain-ia16](toolchain-ia16/) | 16-bit C compiler (host) | [tkchia/gcc-ia16](https://github.com/tkchia/gcc-ia16) + [build-ia16](https://github.com/tkchia/build-ia16) |
| [qemu-i386](qemu-i386/) | Custom QEMU 10.2.3 i386-softmmu | built → `/usr/local` |
| [jwasm](jwasm/) | Assembler for Japheth stack + ctmouse | [Baron-von-Riedesel/JWasm](https://github.com/Baron-von-Riedesel/JWasm) **v2.20+** |
| [boot-bootprog](boot-bootprog/) | 512-byte FAT boot + `mkimg144` | [alexfru/BootProg](https://github.com/alexfru/BootProg) (BSD-2) |
| [mem-jemm](mem-jemm/) | EMS/UMB + optional built-in XMS (**JemmEx**) | [Baron-von-Riedesel/Jemm](https://github.com/Baron-von-Riedesel/Jemm) **v5.86+** |
| [mem-himemx](mem-himemx/) | Standalone XMS if not using JemmEx | [Baron-von-Riedesel/HimemX](https://github.com/Baron-von-Riedesel/HimemX) **v3.40** |
| [drv-ctmouse](drv-ctmouse/) | Mouse driver (8086-safe) | [davidebreso/ctmouse](https://github.com/davidebreso/ctmouse) (GPL-2) |

## Secondary (P1 — after bootable core)

| Dir | Role | Preferred upstream |
|-----|------|--------------------|
| [debug-dosdebug](debug-dosdebug/) | Small on-floppy DEBUG | [Baron-von-Riedesel/DOS-debug](https://github.com/Baron-von-Riedesel/DOS-debug) **v2.51**; advanced: [lDebug](https://pushbx.org/ecm/web/#projects-ldebug) |
| [video-vbe-ref](video-vbe-ref/) | VBE INT 10h semantics | [bochs-emu/VGABIOS](https://github.com/bochs-emu/VGABIOS) (not qemu/vgabios mirror) |
| [video-freebe](video-freebe/) | VBE/AF period drivers | [FreeBE/AF](https://shawnhargreaves.com/freebe/) |
| [drv-nansi](drv-nansi/) | ANSI console TSR | [FDOS/ansi](https://github.com/FDOS/ansi) (frozen NANSI 4.0d) |
| [ext-hx](ext-hx/) | HDPMI / HX for DPMI apps | [Baron-von-Riedesel/HX](https://github.com/Baron-von-Riedesel/HX) **v2.23+** |

## Optional later (P2 — not blocking)

| Dir | Role | Preferred upstream |
|-----|------|--------------------|
| [drv-shsucd](drv-shsucd/) | MSCDEX replacement | [adoxa/shsucd](https://github.com/adoxa/shsucd) |
| [ext-dos32a](ext-dos32a/) | DOS/4GW-class extender | [yetmorecode/dos32a-ng](https://github.com/yetmorecode/dos32a-ng) (not amindlost dump) |
| [fat-runtime-notes](fat-runtime-notes/) | FAT design notes | ChaN FatFs / Petit FatFs (student FAT* repos skipped) |
| [shell-freecom-notes](shell-freecom-notes/) | Shell ideas | [FDOS/freecom](https://github.com/FDOS/freecom) (GPL — license gate) |

## Explicitly not subprojects (skip)

- `FDOS/emm386` → use **Jemm** upstream  
- `phoenixthrush/Tiny-C-Compiler` → **archived** junk; use gcc-ia16  
- `Dos64-stub` → long mode, irrelevant  
- Student FAT12/16 homework repos  
- `PluMGMK/vbesvga.drv` → Windows 3.x/9x  
- SciTech Display Doctor dump, Dreamcast Allegro  
- Most CGA/EGA PCB projects, CP/M, Tandy 2000  

See master roadmap **Track 12** (nice-to-have) and **Track 13** (LINKS integration).

## Rules

1. **Never** paste GPL into MIT core without license review.  
2. Prefer **prebuilt tiny binaries** on the 1.44 MB image; rebuild from subproject when patching.  
3. **Never** format `/dev/sdc` without human confirmation.  
4. Fetch scripts are optional; they clone into `subprojects/<name>/upstream/` (gitignored if huge).

## py86 (local copy — preferred emulator)

| | |
|--|--|
| Path | `subprojects/py86/` |
| Upstream ref | monorepo `Py86/` (**read-only for agents**) |
| Display | `--display term` |
| Control | `--control-socket` + `py86_ctl.py` |
| Future | Optional C++23 port for speed |

See `subprojects/py86/FLOPPYOS.md` and `docs/py86-testing.md`.
