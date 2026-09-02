# iron86 TODO

Gates 1/4 (kernel stack is FloppyOS): DAA/WAIT/ESC.
Gates 2/5 done: 256K PPI nibble (`io_nibble=0x06`), speaker `--no-audio`.

## Bottom of queue (after those)

- [ ] FDC **Write Data** DMA-from-mem (persist image)
- [ ] FDC **Format** / **Scan** / **Read ID**
- [ ] Floppy **B:** (second 360K, INT 13 DL=1)
- [ ] Full 8237 HOLD/HLDA / DRAM refresh tick
- [ ] MDA B000 live display (Py86 default); CGA 3D9 + mode 4 (Py86 Gate 7, no `cga.py` yet)
- [ ] XT Xebec HDC
- [ ] Cassette BASIC ROM map
- [ ] 80186 helpers
