# BootProg boot sector

**Priority:** P0  
**Role for FloppyOS:** 512-byte FAT12/16/32 loaders + flp144 + mkimg144. Primary boot reference for 1.44MB images.

## Upstream

| Field | Value |
|-------|--------|
| Preferred URL | https://github.com/alexfru/BootProg |
| Known good version | V2.0 (2023-04) |
| License (upstream) | BSD-2-Clause |

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

## FloppyOS wiring sketch

```text
BootProg flp144 (sector 0)
  → STARTUP.BIN (stage1.5 / loader)
  → FlopFS or FAT payload → kernel
```

Also study for dual-personality disks (tiny FAT12 boot + FlopFS data).
Host helper: `mkimg144.c` → adapt into `tools/mkimg1440`.
