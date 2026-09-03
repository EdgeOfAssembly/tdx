# iron86 TODO

Gates 1/4 (kernel stack is FloppyOS): DAA/WAIT/ESC.
Gates 2/5 done: 256K PPI nibble (`io_nibble=0x06`), speaker `--no-audio`.

## Bottom of queue (after those)

- [ ] FDC **Write Data** DMA-from-mem (persist image)
- [ ] FDC **Format** / **Scan** / **Read ID**
- [x] Floppy **B:** (second 360K, INT 13 DL=1, `attach_floppy` unit 1 / `--floppy-b`)
- [ ] Full 8237 HOLD/HLDA / DRAM refresh tick
- [x] **CGA BIOS CRT_CHAR_GEN 8×8** (`F000:FA6E`, runtime from guest, tdxview 8×16 double-row)
- [x] **MDA 8K 5788005** + tdxview B000 (`tdx --mda`). BIN gitignored, not pushed.
- [ ] CGA **3D9** palette; **40-col** modes 0/1
- [x] **mode 6** 640×200 1bpp + composite in tdxview (Dragon Wars)
- [x] **mode 4** 320×200 old-CGA NTSC (Reenigne/86Box); gold `games/SCREEN.CGA`
- [x] **720K** FlopFS pack when a host dir does not fit 360K (`--floppy-b`)
- [ ] **1.2M / 1.44M** packer + CHS from image size (not auto yet)
- [ ] **XT HDD C:** auto-size from a host dir larger than 720K
- [x] PPI I/O nibble **0x0F = 544K** (24-APR-81 BIOS max; not 640K / not EMS)
- [x] **`--exec-map FILE`** — 1 MiB executed-opcode map (same linear addrs as RAM)
- [ ] MDA **blink**; exact 720×350 timing
- [ ] **Hercules** 720×348 gfx (not MDA — HGC is MDA text + extra graphics)
- [ ] XT Xebec HDC (`--hdc`, 320h, DMA3, IRQ5) — test with a raw image
- [ ] Cassette BASIC ROM map
- [ ] 80186 helpers

## Bottom of queue

- [ ] ~~CGA light pen~~ **WILLNOTFIX** (no light pen hardware here)
- [ ] iron86 dual MDA+CGA in one guest — last; tdx/tdxview is the dual-head
- [ ] CGA snow / wait-states (cycle-exact 6845) — unless the composite-artifact game needs it

See also repo `docs/TODO.md`.
