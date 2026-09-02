# iron86 TODO

Gates 1/4 (kernel stack is FloppyOS): DAA/WAIT/ESC.
Gates 2/5 done: 256K PPI nibble (`io_nibble=0x06`), speaker `--no-audio`.

## Bottom of queue (after those)

- [ ] FDC **Write Data** DMA-from-mem (persist image)
- [ ] FDC **Format** / **Scan** / **Read ID**
- [ ] Floppy **B:** (second 360K, INT 13 DL=1)
- [ ] Full 8237 HOLD/HLDA / DRAM refresh tick
- [ ] MDA B000 live display in tdxview (Py86 default DIP 0x3D); CGA 3D9 + mode 4 (Py86 Gate 7)
- [ ] XT Xebec HDC
- [ ] Cassette BASIC ROM map
- [ ] 80186 helpers

## Bottom of queue (no PC game used these)

- [ ] CGA **light pen** (3DC/3DB, 6845 R16/R17 strobe)
- [ ] MDA/CGA dual-adapter “wrong init burns the tube” hardware damage model
- [ ] Cycle-exact 6845 (14.318 MHz, 553 ns glyph fetch, CGA snow / wait states)
- [ ] Full 8K MDA character-generator ROM (BIOS INT 10 font is enough)
