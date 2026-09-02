# Hardware safety — USB floppy `/dev/sdc`

## Rule

**Never format, `mkfs`, or `dd` to `/dev/sdc` (or any USB FDD) without explicit human confirmation in chat.**

## Current device (typical session)

| Item | Value |
|------|--------|
| Device | `/dev/sdc` — MITSUMI USB FDD |
| Size | 1 474 560 bytes |
| Mount | often `/tmp/floppy` (vfat) |

## Allowed without confirmation

- Read-only inspect: `lsblk`, `xxd -l 512 /dev/sdc`, mount RO study
- All work on **image files** under `/tmp` or the git tree
- QEMU / DOSBox with `.img` files

## Write path (only after human says yes)

```bash
# After human: "yes, format /dev/sdc" (or equivalent)
./tools/write-floppy.sh --i-confirm-format --device /dev/sdc --image build/floppyos.img
```

`mkimg1440` **refuses** paths under `/dev/`.

## Disk policy while developing

See `docs/workspace-policy.md`: compile on `/tmp`, archive on `/mnt`, commit/push milestones to GitHub.
