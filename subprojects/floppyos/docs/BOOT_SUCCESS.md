# FloppyOS boots to `A>` on Py86 (verified)

**Date:** 2026-07-26  
**Commit:** `947dd8234` (and docs follow-ups)  
**Emulator:** **local** `FloppyOS/subprojects/py86` only — monorepo `Py86/` is not modified.

## Result

Full boot chain on IBM 5150-class Py86 with **360 KB** (5.25" DD, 40/2/9) image:

```text
FloppyOS OK
SB loaded
loading stage1.5...
FloppyOS stage1.5
FlopFS superblock OK
loading kernel...
loading COM...
jumping to kernel
FloppyOS kernel
INT21 OK
starting COMMAND...
FloppyOS COMMAND
Commands: DIR TYPE VER EXIT  (or run FILE.COM/.EXE)
A>
```

This is **our** shell prompt (COMMAND.COM), analogous to MS-DOS 2.11’s `A>`.

## Golden reference (MS-DOS 2.11)

| Item | Path |
|------|------|
| Image | `tests/images/msdos211-cdp-360k-disk01.img` |
| Config | `tests/py86_msdos211.json` |
| Behavior | Date prompt → **Enter** → time prompt → **Enter** → `A>` |

```bash
cd FloppyOS/subprojects/py86
python3 py86.py --config ../../tests/py86_msdos211.json --display term --fast-post --steps 0
```

## How to run FloppyOS

```bash
cd /tmp/RetroCodeMess/FloppyOS
make image                 # 360K default (368640 bytes)
make run-py86              # --display term (preferred)
python3 tests/smoke_py86.py  # automated until A> with wall timeouts
```

**Avoid QEMU SDL on this host** — it has hung the machine. Prefer Py86.

## Root causes fixed to reach `A>`

| Bug | Fix |
|-----|-----|
| NASM emitted **386 near-Jcc** (`0F 85`) in stage1.5 — illegal on 8086 | `cpu 8086` + inverted short jumps + `jmp` |
| INT 13h hang reading superblock after stage1.5 | Boot **preloads** superblock LBA1 → `0900:0000` |
| INT 13h hang loading COMMAND from kernel | Stage1.5 **preloads** COM → `com_seg:0100` |
| Kernel `fs_init` disk read hang | Deferred; init via preloaded COM |
| Infinite test hangs | `smoke_py86.py` per-stage **wall-clock timeouts** |

## Image layout (360K)

| LBA | Content |
|-----|---------|
| 0 | Boot sector (BPB 40/2/9, media `0xFD`) |
| 1–2 | FlopFS superblock + mirror |
| 3–4 | Stage1.5 (1024 B) |
| 5+ | Kernel (8 KiB / 16 sectors) |
| … | COMMAND.COM, HELLO.COM, HELLO.EXE, root dir |

## Policy

| Tree | Rule |
|------|------|
| `FloppyOS/subprojects/py86/` | Local copy — may edit (incl. future C++23 port) |
| Monorepo `Py86/` | **Do not edit** |
| `/dev/sdc` USB floppy | **Never format without human OK** |
