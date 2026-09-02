# FAT runtime notes

**Priority:** P2  
**Role for FloppyOS:** Student FAT12/16 GitHub repos from LINKS.txt are SKIP. Use FatFs/Petit for runtime ideas; FlopFS remains primary.

## Upstream

| Field | Value |
|-------|--------|
| Preferred URL | https://elm-chan.org/fsw/ff/ |
| Known good version | ChaN FatFs / Petit FatFs |
| License (upstream) | FatFs license (BSD-ish) |

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
