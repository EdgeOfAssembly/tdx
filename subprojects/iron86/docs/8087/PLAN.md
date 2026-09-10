# 8087 NPX (iron86 0.23)

Default **on**. `--no-8087` only. Not 287/387.

- Chip: `include/iron86/fpu.h` + `src/fpu.cpp` (do not grow `cpu.cpp` with FSQRT).
- CPU: `op_esc` → `npx().esc(*this, last_op_, mr_)` after `decode_modrm()`; `op_wait` → `npx().wait()`.
- DIP: SW1-2 set → default `0x2F` (was `0x2D`). `--no-8087` keeps `0x2D`.
- v1: ops complete immediately (`busy()` false). No host thread.
- OCR: `/mnt/8087/ocr` (Intel 8087). Do not git the raw OCR dump.

Checkpoint: `checkpoint/20260910T211845Z-8087-npx`
