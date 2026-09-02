# CuteMouse (ctmouse)

**Priority:** P0  
**Role for FloppyOS:** Best modern CuteMouse source: JWasm/Linux build, 8086 fixes. Ship CTMOUSE.EXE.

## Upstream

| Field | Value |
|-------|--------|
| Preferred URL | https://github.com/davidebreso/ctmouse |
| Known good version | main (2024-12); SF CuteMouse 2.1b4 lineage |
| License (upstream) | GPL-2.0 |

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
