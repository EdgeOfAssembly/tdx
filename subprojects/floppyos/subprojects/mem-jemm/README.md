# Jemm / JemmEx

**Priority:** P0  
**Role for FloppyOS:** Prefer JemmEx (built-in XMM) alone on floppy to save conventional memory and file count. Prefer over FDOS/emm386 mirror.

## Upstream

| Field | Value |
|-------|--------|
| Preferred URL | https://github.com/Baron-von-Riedesel/Jemm |
| Known good version | v5.86 stable / v5.87pre1 |
| License (upstream) | Partly Artistic |

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

## Floppy image tip

On a 1.44 MB game disk prefer **JemmEx alone** (embedded XMM) instead of HimemX + Jemm386 to reduce resident conventional memory and file count.
