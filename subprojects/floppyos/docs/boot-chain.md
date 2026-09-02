# Boot chain (current)

## Target media

**Default:** 360 KB 5.25" DD — 40 cyl × 2 heads × 9 spt × 512 = **368640** bytes.  
(Optional 1.44 MB via `make image1440`.)

## Stages

```text
BIOS INT 19h
  → LBA 0 boot sector @ 0000:7C00
  → print "FloppyOS OK"
  → INT 13h load superblock LBA 1 → 0900:0000 (verify FLOPFS01)
  → print "SB loaded"
  → INT 13h load stage1.5 LBA 3–4 → 0800:0000 (verify 0xFA CLI)
  → print "loading stage1.5..."
  → far jump 0800:0000

stage1.5
  → print "FloppyOS stage1.5"
  → use superblock @ 0900:0
  → print "FlopFS superblock OK"
  → load kernel (kernel_lba/secs → kernel_load_seg:0000)
  → load init COM (com_lba/secs → com_load_seg:0100)
  → print "jumping to kernel"
  → far jump kernel_load_seg:0000

kernel
  → install INT 21h, MCB
  → print "FloppyOS kernel" / "INT21 OK"
  → build PSP @ com_load_seg, jump com_load_seg:0100

COMMAND.COM
  → "FloppyOS COMMAND"
  → interactive "A>" prompt
```

## 8086 constraints

- All boot/stage1.5/kernel code must be **8086-legal** (no 386 `0F 85` near Jcc, no PUSHA on pure 8086 path for stage1.5).
- Prefer short conditional jumps + `jmp` for long distances under `cpu 8086`.

## Testing

See `docs/BOOT_SUCCESS.md` and `docs/py86-testing.md`.
