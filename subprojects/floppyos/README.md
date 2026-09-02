# FloppyOS

**Optimized, enhanced, 100% MS-DOS-compatible OS that fits a 1.44 MB floppy.**

- Custom primary filesystem (**FlopFS**) with compression (planned)
- FAT12 / FAT16 / FAT32 / exFAT for foreign volumes (planned)
- Game-oriented defaults, high conventional memory

## Try it (preferred: Py86, not QEMU)

```bash
cd FloppyOS
make image
make run-py86          # --display term (safe/fast on this host)
# make smoke-py86      # control-socket automation
```

**Do not edit** monorepo `Py86/` — only `subprojects/py86/` (local copy).  
QEMU has hung this Gentoo laptop; use Py86 unless you accept that risk.

## Current milestone: **M12+** interactive shell; **Py86 A> verified**

Boot chain under QEMU (serial):

```text
… → EXEC HELLO.COM → EXEC HELLO.EXE → Hello EXE → EXEC OK → SHELL OK
```

Kernel **INT 21h**: `02/09/25/30/35/3D/3E/3F/48/49/4A/4C`. MCB arena @0x3000.

```bash
cd FloppyOS
make -j$(nproc) smoke              # QEMU monitor sendkey (no X11)
# optional visual: ./tests/smoke_keypress.sh   # keypress.py + SDL
```

Requires: `nasm`, `gcc`, `qemu-system-i386` (e.g. custom 10.2.3 at `/usr/local`).

## Docs

| File | Purpose |
|------|---------|
| **[TODO.md](TODO.md)** | Master roadmap |
| [docs/workspace-policy.md](docs/workspace-policy.md) | `/tmp` work, `/mnt` archive, git push |
| [docs/hardware-safety.md](docs/hardware-safety.md) | **Never format `/dev/sdc` without human OK** |
| [docs/boot-chain.md](docs/boot-chain.md) | Boot stages |
| [docs/toolchain-notes.md](docs/toolchain-notes.md) | OpenWatcom `/opt/ow`, QEMU, TCC note |
| [subprojects/](subprojects/) | HimemX, Jemm, BootProg, … |

## Hardware

USB 3.5" floppy may be `/dev/sdc`. **Never format without explicit human confirmation.**

## Status

**Boots to `A>` on local Py86** (360K image). See `docs/BOOT_SUCCESS.md`.

## Status

Milestone 1 in progress / delivered via `make smoke`.
