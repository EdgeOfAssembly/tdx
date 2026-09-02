# gcc-ia16 toolchain

**Priority:** P0  
**Role for FloppyOS:** Host-side 16-bit C compiler. Do NOT vendor full GCC tree; install via build-ia16 / packages.

## Upstream

| Field | Value |
|-------|--------|
| Preferred URL | https://github.com/tkchia/gcc-ia16 |
| Known good version | active 2026-06 (use build-ia16 packages) |
| License (upstream) | GPL-2.0 + GCC exceptions |

See [UPSTREAM.md](UPSTREAM.md) for forks, rejects, and fetch notes.

## Integration

1. Build or download binary on the **host** (Linux).
2. Place tiny artifacts under `FloppyOS/dist/floppy/` when packaging a 1.44 MB image.
3. Do **not** merge upstream source into the FloppyOS kernel tree without license review.
4. Document CONFIG.SYS / AUTOEXEC lines in this README as they stabilize.

## Status

- [ ] Upstream fetched (optional `./fetch.sh`)
- [ ] Build recipe verified on this machine
- [ ] Binary size measured for 1.44 MB budget
- [ ] Wired into FloppyOS image build

## Install (host) — do not submodule GCC

```bash
# Prefer tkchia build-ia16 packages / scripts over cloning gcc-ia16 alone
# https://github.com/tkchia/build-ia16
which ia16-elf-gcc || echo "install gcc-ia16 via build-ia16"
```

OpenWatcom remains the other P0 C path (see master TODO Track 1).
