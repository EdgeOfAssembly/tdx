# Py86 reference: official MS-DOS 2.11 360K

**Golden boot path** for local Py86 (5150 + 5.25" DD):

```text
FloppyOS/subprojects/py86/disks/MS-DOS-2.11-Columbia-Data-Products-OEM-5.25-360K/disk01.img
```

(Same file in monorepo `Py86/disks/...` — do not edit monorepo Py86.)

## What you should see

1. BIOS POST (use `--fast-post` to skip long RAM test)
2. MS-DOS boot
3. **`Current date is ...` / `Enter new date:`** → press **Enter**
4. **`Enter new time:`** → press **Enter**
5. **`A>`** prompt

## Interactive (terminal MDA)

```bash
cd /tmp/RetroCodeMess/FloppyOS/subprojects/py86
python3 py86.py \
  --config /dev/stdin \
  --display term \
  --fast-post \
  --steps 0 <<'EOF'
{
  "bios": { "path": "ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN" },
  "logging": { "log_file": "/tmp/py86-msdos211.log", "level": "WARNING" },
  "video": { "default_adapter": "MDA", "render_on_halt": true },
  "floppy": {
    "path": "disks/MS-DOS-2.11-Columbia-Data-Products-OEM-5.25-360K/disk01.img"
  }
}
EOF
```

Or use a saved config under `tests/py86_msdos211.json`.

At the date/time prompts: **Enter**, **Enter**.

## Control socket automation

```bash
# After py86 is up with --control-socket /tmp/py86.sock --steps 0
python3 py86_ctl.py -s /tmp/py86.sock step 8000000
python3 py86_ctl.py -s /tmp/py86.sock cont    # if paused after step
python3 py86_ctl.py -s /tmp/py86.sock key enter   # date
python3 py86_ctl.py -s /tmp/py86.sock step 500000
python3 py86_ctl.py -s /tmp/py86.sock cont
python3 py86_ctl.py -s /tmp/py86.sock key enter   # time
python3 py86_ctl.py -s /tmp/py86.sock step 1000000
python3 py86_ctl.py -s /tmp/py86.sock cont
python3 py86_ctl.py -s /tmp/py86.sock mda          # expect A>
```

Note: `step N` may leave the CPU **paused** — send **`cont`** so steps run.

## FloppyOS target geometry (aligned with this disk)

| | MS-DOS 2.11 CDP | FloppyOS (Py86 default) |
|--|-----------------|-------------------------|
| Size | 368640 | 368640 |
| CHS | 40/2/9 | 40/2/9 |
| Media | 0xFD | 0xFD |
| Sectors | 720 | 720 |

Build: `make image` → 360K `build/floppyos.img`.
