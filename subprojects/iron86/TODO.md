# iron86 TODO

Gates 1/4 (kernel stack is FloppyOS): DAA/WAIT/ESC.
Gates 2/5 done: 256K PPI nibble (`io_nibble=0x06`), speaker `--no-audio`.

## Bottom of queue (after those)

- [ ] FDC **Write Data** DMA-from-mem (persist image)
- [ ] FDC **Format** / **Scan** / **Read ID**
- [x] Floppy **B:** (second 360K, INT 13 DL=1, `attach_floppy` unit 1 / `--floppy-b`)
- [ ] Full 8237 HOLD/HLDA / DRAM refresh tick
- [x] **CGA BIOS CRT_CHAR_GEN 8×8** (`F000:FA6E`, runtime from guest, tdxview 8×16 double-row)
- [ ] **MDA 8K adapter character ROM** (9×14, not in 8K BIOS)
- [ ] MDA B000 live display in tdxview (DIP 0x3D)
- [ ] **Hercules** 720×348 @ B000 after MDA text
- [ ] CGA 3D9 palette vs Unicorn `dos_cga.c`
- [ ] XT Xebec HDC (`--hdc`, 320h, DMA3, IRQ5) — test with a raw image
- [ ] Cassette BASIC ROM map
- [ ] 80186 helpers

## Bottom of queue (no PC game used these)

- [ ] CGA **light pen** (3DC/3DB, 6845 R16/R17 strobe)
- [ ] MDA/CGA dual-adapter “wrong init burns the tube” hardware damage model
- [ ] Cycle-exact 6845 (14.318 MHz, 553 ns glyph fetch, CGA snow / wait states)
- [ ] Full 8K MDA character-generator ROM (duplicate of P1 CGA/MDA font — keep one task)

See also repo `docs/TODO.md`.
