# FloppyOS toolchain & emulation notes

**Updated:** 2026-07-26

## Compilers (host)

| Tool | Location / upstream | Role |
|------|---------------------|------|
| **OpenWatcom** | **`/opt/ow`** — `binl/wcc`, `wcc386`, `wlink`, `wasm` | Preferred 16-bit real-mode C for DOS binaries |
| **gcc-ia16** | tkchia/gcc-ia16 + build-ia16 | Alternate pure IA-16 GCC |
| **JWasm** | Japheth JWasm | HimemX / Jemm / ctmouse rebuilds |
| **Tiny C Compiler** | LINKS: phoenixthrush/Tiny-C-Compiler | **Archived dump — skip.** Prefer official TCC + tcc4dos later if needed. Not P0. |

```bash
export WATCOM=/opt/ow
export PATH="$WATCOM/binl:$PATH"
```

## Emulation (i386)

| Tool | Role |
|------|------|
| DOSBox Staging | Fast daily app/game runs (`/usr/local/bin/dosbox`) |
| **Custom QEMU i386** | Full PC boot (BIOS, floppy INT 13h, LBA) — OS boot CI |
| Py86 | Authentic 8086 path |

### Build layout (this machine)

| Path | Purpose |
|------|---------|
| `/tmp/qemu-10.2.3` | Source extract (compile workspace) |
| `/tmp/qemu-i386-build` | Out-of-tree build dir |
| `/mnt/floppyos-build/` | Archives: tarball, analyzer logs, config notes |
| `/usr/local` | Install prefix |

**Rule:** compile on **`/tmp`**; archive large artifacts on **`/mnt`**. Root (`/`) is nearly full — do not build there.
