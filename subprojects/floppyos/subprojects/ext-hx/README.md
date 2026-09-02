# HX / HDPMI

**Priority:** P1  
**Role for FloppyOS:** DPMI host for protected-mode apps. Full HXRT too big for minimal 1.44MB OS image; use on dev profile.

## Upstream

| Field | Value |
|-------|--------|
| Preferred URL | https://github.com/Baron-von-Riedesel/HX |
| Known good version | v2.23 / v2.24pre1 |
| License (upstream) | Japheth HX license (see package) |

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
