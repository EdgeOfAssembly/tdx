# Testing FloppyOS with Py86 (preferred)

## Policy

| Tree | Policy |
|------|--------|
| **`FloppyOS/subprojects/py86/`** | **Local copy** — edit freely for FloppyOS |
| **`/tmp/RetroCodeMess/Py86`** (monorepo) | **Do not modify** — upstream reference only |

Never `rsync` FloppyOS hacks back into monorepo Py86 without an explicit, reviewed PR.

## Why Py86 here

- Authentic 8086 / 5150-class path (good for real-mode FloppyOS)
- **`--display term`** is fast and does not need SDL
- **UNIX control socket** (`--control-socket`) for automation (`status`, `mda`, `type`, `key`, `until`, `step`, `quit`)
- **QEMU SDL has hung this Gentoo host** — treat QEMU as optional/risky

## Quick try (you)

```bash
cd /tmp/RetroCodeMess/FloppyOS
make image
make run-py86
# → MDA 80×25 in the terminal; Ctrl+Q / Ctrl+C to quit
```

Automated (control socket, no live display):

```bash
make smoke-py86
```

## Control socket (manual)

```bash
cd subprojects/py86
python3 py86.py \
  --config ../../tests/py86_floppyos.json \
  --display none --fast-post --control-socket /tmp/floppyos-py86.sock --steps 0 &

python3 py86_ctl.py -s /tmp/floppyos-py86.sock status
python3 py86_ctl.py -s /tmp/floppyos-py86.sock step 1000000
python3 py86_ctl.py -s /tmp/floppyos-py86.sock mda
python3 py86_ctl.py -s /tmp/floppyos-py86.sock type 'dir'
python3 py86_ctl.py -s /tmp/floppyos-py86.sock key enter
python3 py86_ctl.py -s /tmp/floppyos-py86.sock quit
```

Config: `tests/py86_floppyos.json` → floppy `../../build/floppyos.img` (relative to local Py86 CWD).

## Future: C++23 rewrite of *local* Py86

Allowed and encouraged **only under `subprojects/py86`** (or `subprojects/py86-cpp/`) for speed:

- Same control-socket protocol if possible (keep `smoke_py86.py` working)
- gcc, C++23, Doxygen — project quality rules
- Monorepo `/tmp/RetroCodeMess/Py86` stays the Python reference

Track as roadmap item; not required for M1–M12 demos.

## 5150 vs 1.44MB note

Local Py86 targets IBM 5150 + FDC; 1.44MB images are supported in `fdc_pc.py` by size.  
If FloppyOS boot fails on 5150 BIOS geometry assumptions, fix **local** FDC/BIOS options only — not monorepo Py86.

## Success criteria

See **[BOOT_SUCCESS.md](BOOT_SUCCESS.md)** — FloppyOS reaches `A>` under local Py86.
