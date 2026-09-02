# FloppyOS local test images

| File | Size | Role |
|------|------|------|
| `msdos211-cdp-360k-disk01.img` | 360 KB (368640) | **Official MS-DOS 2.11** Columbia Data Products OEM 5.25" — golden Py86/5150 boot reference |
| `../../build/floppyos.img` | 360 KB (default) | FloppyOS image from `make image` |

## MS-DOS 2.11 reference boot (local Py86 only)

```bash
cd /tmp/RetroCodeMess/FloppyOS/subprojects/py86

python3 py86.py \
  --config ../../tests/py86_msdos211.json \
  --display term \
  --fast-post \
  --steps 0
```

During boot:

1. **Enter** at `Enter new date:`
2. **Enter** at `Enter new time:`
3. Expect **`A>`**

Same image also lives at:

```text
subprojects/py86/disks/MS-DOS-2.11-Columbia-Data-Products-OEM-5.25-360K/disk01.img
```

(Copied from monorepo `Py86/disks/...` for local testing. **Do not edit monorepo Py86.**)

## FloppyOS 360K

```bash
cd /tmp/RetroCodeMess/FloppyOS
make image          # 40/2/9, 368640 bytes
make run-py86       # --display term
```
