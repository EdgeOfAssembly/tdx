# Interactive input testing (M12)

## How guest input works

| Path | Mechanism | When |
|------|-----------|------|
| **BIOS keyboard** | INT 16h (AH=00/01) | QEMU SDL window, `sendkey`, real PC |
| **Serial fallback** | COM1 data ready (0x3FD/0x3F8) | Headless `-serial stdio` typing |

INT 21h **AH=01/07/08/0A/0B** use INT 16h first, then COM1 if no key pending.

## Recommended automation (CI / no X11)

**QEMU monitor `sendkey`** injects into the emulated keyboard → INT 16h.  
No X11, no keypress.py required:

```bash
make image
python3 tests/smoke_interactive.py build/floppyos.img
```

This starts QEMU with:

- `-display none`
- `-serial file:…` (capture console)
- `-monitor unix:…,server,nowait`
- scripted `sendkey` for `dir` / `ver` / `exit`

## Visual automation (X11)

When `DISPLAY` is set and you want a real window:

```bash
# keypress.py (preferred over raw xdotool for layout)
./tests/smoke_keypress.sh build/floppyos.img
```

Uses **`keypress.py --emulator-mode`** (US layout inside QEMU) and window match `QEMU|qemu`.

Equivalent idea with xdotool (more fragile on layout):

```bash
qemu-system-i386 -fda build/floppyos.img -boot a -display sdl &
sleep 3
xdotool search --name QEMU windowactivate type 'dir' key Return
```

## Manual

```bash
qemu-system-i386 -fda build/floppyos.img -boot a -display sdl
# or headless serial (type into terminal; serial fallback):
qemu-system-i386 -fda build/floppyos.img -boot a -nographic
```

## Why not only pipe stdin to `-serial stdio`?

Guest **INT 16h** does not see serial bytes unless the guest OS maps them.  
FloppyOS maps them in **AH=01/0A** as a fallback, so piping can work, but **sendkey** is closer to real keyboard hardware and is what `smoke_interactive.py` uses.
