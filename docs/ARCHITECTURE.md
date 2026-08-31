# TDX architecture

```text
  tdx (SDL2 CPU TUI, one Xmux session)          tdxview (SDL2 CGA, other Xmux)
       │                                              │
       ├── rex_session  (step / over / run / bp)      │
       │        └── dos_machine (Unicorn 8086)        │
       └── rex_sock  UNIX JSON  ──────────────── cga / key / step
```

`tdx` is the only emulator. `tdxview` is a framebuffer client (polls `{"cmd":"cga"}`).

**Why Unicorn, not DOSBox CPU:** DOSBox Staging is GPL and not a library.
Unicorn 2 is LGPL, embeddable, proven 16-bit step (`uc_emu_start(..., count=1)`).
Capstone 6 disassembles. SDL2 draws a TD-like text UI (not BGI — TD is text
mode; the *guest* may be CGA).

**Step over:** if the current insn is CALL, INT, REP, or LOOP, run until the
fall-through linear address (LOOP = remaining iterations). Inner breakpoints
still fire.

**CGA window:** BIOS mode 04h/05h VRAM at `B800:0000` (even/odd banks) decoded
to 320×200×4. Writes into `0xB8000–0xBFFFF` mark the surface dirty.

**Other platforms:** `rex_arch` already has Z80 / 6502 / M68K slots. A future
backend replaces `dos_machine` and keeps `rex_session_*`.
