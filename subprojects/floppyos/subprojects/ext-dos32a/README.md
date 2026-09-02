# DOS/32A-ng

**Priority:** P2  
**Role for FloppyOS:** Prefer over amindlost/dos32a dump. Optional DOS/4GW-compatible extender for games.

## Upstream

| Field | Value |
|-------|--------|
| Preferred URL | https://github.com/yetmorecode/dos32a-ng |
| Known good version | 9.1.2 (2023) |
| License (upstream) | DOS/32A Liberty |

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
