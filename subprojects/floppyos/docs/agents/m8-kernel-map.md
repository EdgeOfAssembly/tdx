# FloppyOS M8 kernel map — INT 21h memory services

**Source milestone:** M7 (FlopFS + AH=3D/3E/3F + COM by name)  
**Target:** M8 — MCB arena + INT 21h AH=48/49/4A  
**Kernel:** `/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm`  
**Date:** 2026-07-26

---

## 1. Current INT 21h AH list

From `int21_handler` (`kernel.asm:275–297`) and `docs/int21.md`:

| AH   | Handler label | Implementation | Notes |
|------|---------------|----------------|-------|
| 02h  | `i02`         | inline         | Write char DL → VGA INT 10h + COM1 |
| 09h  | `i09`         | inline         | Write `$`-terminated string DS:DX |
| 25h  | `i25`         | inline         | Set vector AL, DS:DX |
| 30h  | `i30`         | inline         | Version AX=0x0A07 (7.10) |
| 35h  | `i35`         | inline         | Get vector AL → ES:BX |
| 3Dh  | `i3d`         | `dos_open`     | Open 8.3 path → handle 5–8 |
| 3Eh  | `i3e`         | `dos_close`    | Close BX=handle |
| 3Fh  | `i3f`         | `dos_read`     | Read BX/CX/DS:DX |
| 4Ch  | `i4c`         | halt loop      | Terminate (no parent return) |
| *else* | —           | AX=1, CF=1     | Unknown function |

**Not present (M8 targets):** 48h allocate, 49h free, 4Ah resize.  
**Also absent (later):** 4Bh EXEC, 4Dh get return code, 48h-adjacent arena walk helpers.

Dispatcher is a linear `cmp ah` / `je` chain — **not** a jump table.

---

## 2. Where to insert AH=48/49/4A in dispatcher

Insert **after 3Fh, before 4Ch** (numeric order, matches existing style):

```asm
; kernel.asm int21_handler — after i3f branch (~line 291)
        cmp     ah, 0x3F
        je      i3f
        cmp     ah, 0x48          ; M8: allocate
        je      i48
        cmp     ah, 0x49          ; M8: free
        je      i49
        cmp     ah, 0x4A          ; M8: resize
        je      i4a
        cmp     ah, 0x4C
        je      i4c
        ; fallthrough: unknown → AX=1, STC, IRET
```

Handler stubs (same pattern as file API):

```asm
i48:    call    dos_alloc
        iret
i49:    call    dos_free
        iret
i4a:    call    dos_realloc
        iret
```

Place new functions **after `dos_read` / before `putc`** (~line 606) to keep INT 21h service bodies together.

---

## 3. Free bytes in 4096 kernel image

Pad directive (`kernel.asm:698`):

```asm
        times 4096 - ($ - $$) db 0
```

Makefile enforces exact size:

```make
@test $$(wc -c < $@) -eq 4096
```

### Fixed tail (exact)

| Region | Bytes |
|--------|------:|
| Messages (`msg_*`) | 82 |
| `boot_drive`+`fs_ok`+align | 2 |
| Words `root_lba`…`rd_hp` (16×2) | 32 |
| `init_fcb`+`fcb_tmp`+`path_buf` | 38 |
| `handles` (4×16) | 64 |
| `root_buf` | 512 |
| `sec_buf` | 512 |
| **Data+msgs subtotal** | **≈1242** |

### Code size / free pad (estimate)

No `build/kernel.bin` present at map time (only host tools + `hello.com`).  
Code body is ~lines 12–664 (entry → `serial_putc`).

| Assumption | Code ≈ | Free pad ≈ |
|------------|-------:|-----------:|
| ~3.0 B/instr | ~1650 | **~1200** |
| ~3.5 B/instr | ~1925 | **~930** |
| ~4.0 B/instr | ~2200 | **~650** |

**Working estimate: ~700–1100 free bytes** in the 4096 image.

**Measure before coding M8:**

```bash
cd /tmp/RetroCodeMess/FloppyOS
nasm -f bin -o /tmp/k.bin -l /tmp/k.lst kernel/kernel.asm
# free pad = 4096 - first non-zero-from-end, or:
# in listing: address of `times 4096` line = used size
python3 -c "d=open('/tmp/k.bin','rb').read(); print(4096-len(d.rstrip(b'\\x00')))"
```

Minimal MCB+48/49/4A is typically **250–450 bytes** code + **~16–32 bytes** vars — **may fit in 4K**, but tight once coalesce/error paths/strings land.

---

## 4. Risk: expanding kernel past 4096

### Current load geometry

| Item | Value | Source |
|------|-------|--------|
| `kernel_load_seg` | **0x1000** (phys 0x10000) | `mkflopfs.c` `KERNEL_LOAD_SEG_DEFAULT` |
| Kernel image | **8 sectors / 4096 B** | `times 4096`, Makefile |
| Image end | phys **0x11000** (seg 0x1100) | |
| `com_load_seg` | **0x2000** (phys 0x20000) | SB + `run_init_com` default |
| `KERNEL_MAX_SECS` | 64 (32 KiB) | `mkflopfs.c` — headroom OK |
| Superblock | 0x0900 | stage1.5 + `SB_SEG` |
| Stage1.5 | 0x0800 | boot chain |

### If kernel → 8192 (16 sectors)

| | |
|--|--|
| Image end | phys **0x12000** (seg 0x1200) |
| Still below COM | 0x2000 − 0x1200 = **0xE00 paragraphs (~56 KiB)** gap |
| Disk | 16×512 = 8 KiB at `kernel_lba` (default LBA 5) — fine on 1.44M |
| stage1.5 | already loops `k_secs` from SB — **no boot change** if SB updated |

### Recommend **8192 (16 sectors)** if M8 needs room

**Touch list when growing:**

1. `kernel/kernel.asm` — `times 8192 - ($ - $$) db 0`
2. `Makefile` — `@test … -eq 8192`
3. `docs/int21.md` / flopfs notes — document new size
4. `mkflopfs` — already derives `kernel_sectors` from file length (no hardcode of 8)
5. Optional: keep 4096 until first `times` overflow, then jump to 8192 in one FIXUP

**Do not** grow into 0x2000 without moving `com_load_seg`.

---

## 5. `run_init_com` / 0x2000 PSP vs MCB arena

### What M7 does today

```text
run_init_com:
  AH=3D open init_name
  psp_seg = SB.com_load_seg (else 0x2000)
  clear_psp:
    zero 256 bytes at psp_seg:0
    [0]=CD 20, [2]=0x9FFF (mem top), [50]=CD 21 CB, cmdline empty
  AH=3F read up to 0x6000 bytes → psp_seg:0100
  AH=3E close
  enter_com:
    DS=ES=SS=psp_seg, SP=0xFFFE
    retf → psp_seg:0100
```

Fallback path (`load_com_fallback`) uses same `psp_seg` / `clear_psp` / `enter_com`.

### Kernel stack vs arena (important)

At entry: `SS=CS` (0x1000), `SP=0xFFFE`  
→ stack lives at linear **0x1FFFE** downward — **same linear range** as paragraphs 0x1100–0x1FFF.

| Phase | SS | Who owns 0x11000–0x1FFFF? |
|-------|----|---------------------------|
| Kernel init / INT 21h from kernel | 0x1000 | Kernel stack (high offsets) |
| After `enter_com` | 0x2000 | Free for arena / COM heap |
| AH=4C today | — | **halt** — never returns to kernel |

So post-COM the conflict is moot; **during COM**, INT 21h AH=48 must allocate from an MCB chain that does **not** assume kernel still owns 0x1000:high.

### Conflict with naive MCB arena

Classic DOS arena:

```text
[MCB][block][MCB][block]…[MCB'Z'][last free]
 first MCB just after DOS kernel
```

**Naive plan:** first MCB at 0x1100 (after 4K kernel) or 0x1200 (after 8K), free through 0x9FFF.

**Conflict A — fixed COM hole:**  
Init COM is planted at **0x2000** without an MCB. A free block 0x1100→0x9FFF would **overlap** the COM image (0x2000:0100+). Allocations could corrupt HELLO.COM.

**Conflict B — PSP:2 = 0x9FFF:**  
`clear_psp` claims the process owns memory up to 0x9FFF. That only matches DOS if the COM’s allocated MCB is sized to that top (or a single large block). Today it is a **lie** (no arena).

**Conflict C — kernel SS during early AH=48:**  
If anything called AH=48 before `enter_com` with SS=0x1000, stack and arena share linear space. Prefer: **init arena after stack is quiet**, or put kernel stack in a dedicated low region / keep SP below first MCB.

### Recommended M8 layout

```text
phys        seg     content
00000       0000    IVT + BDA
08000       0800    stage1.5 (1–2 KiB)
09000       0900    superblock mirror (SB_SEG)
10000       1000    kernel image (4 or 8 KiB)
11000/12000 first_mcb → free MCB chain
20000       2000    init COM: either
                      (a) pre-carved allocated MCB owner=psp, or
                      (b) AH=48-allocated segment (psp_seg dynamic)
9FFF0       9FFF    conventional top (PSP word / last MCB)
A0000               video
```

**Preferred policy for init COM:**

1. `mcb_init`: first MCB at `kernel_load_seg + (kernel_bytes/16)`, single free block to `0x9FFF − first`.
2. **Carve** init process: either
   - **Fixed:** split free block so an allocated MCB covers `0x2000` … `0x2000+paras−1` (owner = 0x2000), remainder free above/below; or
   - **Dynamic (better long-term):** `dos_alloc` ~0x1000 paragraphs, set `psp_seg` to returned AX, load COM there (ignore SB `com_load_seg` except as hint).
3. `clear_psp`: set `[2]` = **end of that allocated block**, not hard-coded 0x9FFF (unless the block really goes there).

Until AH=4B EXEC exists, (a) fixed carve at 0x2000 is enough for M8 smoke.

---

## 6. Concrete patch plan

### 6.1 New variables (`kernel.asm` data section, near `psp_seg`)

```asm
first_mcb:    dw 0        ; segment of first MCB
arena_end:    dw 0x9FFF   ; last usable paragraph (conventional top)
; optional:
; kernel_paras: dw 0x100  ; 4K → 0x100; 8K → 0x200
```

MCB is **one paragraph** at `mcb_seg:0`:

| Off | Size | Field |
|----:|-----:|-------|
| 0 | 1 | `'M'` or `'Z'` (last) |
| 1 | 2 | owner (0=free, 8=DOS, else PSP seg) |
| 3 | 2 | size in paragraphs (**excluding** this MCB) |
| 5 | 11 | reserved / name (zero OK) |

### 6.2 Functions to add

| Function | Role |
|----------|------|
| `mcb_init` | Called from `kernel_entry` after `fs_init` (or before `run_init_com`). Build one free MCB after kernel. |
| `mcb_next` | AX=mcb → AX=next (mcb+1+size), CF if past arena |
| `mcb_coalesce` | Merge adjacent free blocks |
| `dos_alloc` | AH=48: BX=paras → AX=block seg (mcb+1), update BX=largest free on fail; CF/AX=8 nomem |
| `dos_free` | AH=49: ES=block seg → mark owner 0, coalesce |
| `dos_realloc` | AH=4A: ES=block, BX=new paras; shrink/grow or CF |
| `mcb_from_block` | ES=block → ES=MCB (dec seg) + validate |

### 6.3 Call-site wiring

1. **`kernel_entry`** — after `install_int21` / `fs_init`:

   ```asm
   call    mcb_init
   ```

2. **`int21_handler`** — three `cmp`/`je` as in §2.

3. **`run_init_com` / `clear_psp`** — after arena exists:
   - Carve or alloc block for COM.
   - Set PSP `[2]` from allocated end.
   - Optional: set MCB owner = `psp_seg`.

4. **`i4c` (optional M8 polish)** — free process block / return to kernel instead of pure halt (can stay halt for M8.0).

### 6.4 Docs / build

| File | Change |
|------|--------|
| `docs/int21.md` | Add AH=48/49/4A rows |
| `docs/flopfs-spec.md` | Note arena vs `com_load_seg` |
| `Makefile` | Size test 4096→8192 if grown |
| `programs/hello.asm` | Optional: small AH=48 alloc/free smoke |
| `tests/smoke_qemu.sh` | Expect new serial markers if added |

### 6.5 Suggested implementation order

1. Measure free pad (`nasm -l`).
2. If free < ~400 B → expand to **8192** first (FIXUP size only).
3. `mcb_init` + debug dump (serial print first MCB fields).
4. AH=48 first-fit + AH=49 + coalesce.
5. AH=4A shrink; grow if next free.
6. Reconcile `run_init_com` with arena (carve 0x2000).
7. COM smoke: alloc 16 paras, free, print OK.
8. Update `docs/int21.md`.

### 6.6 DOS semantics cheat-sheet (oracle)

| AH | In | Out success | Out fail |
|----|----|-------------|----------|
| 48 | BX=paragraphs | AX=seg, CF=0 | CF=1, AX=8, BX=largest free |
| 49 | ES=seg | CF=0 | CF=1, AX=9 (invalid mem) |
| 4A | ES=seg, BX=new size | CF=0, BX=max if fail grow | CF=1, AX=8/9 |

Use `dos_memory_arena_analyzer` / MS-DOS 4 `ALLOC` as behavioral oracle (TODO Track 3).

---

## Memory map summary (M7 → M8)

```text
seg 0x0000  IVT/BDA
seg 0x0800  stage1.5
seg 0x0900  FlopFS superblock (SB_SEG)
seg 0x1000  kernel CS=DS=ES=SS at entry; image 4KiB (→8KiB M8)
seg 0x1100+ *** M8 first_mcb / free arena ***
seg 0x2000  init COM PSP + .COM @ 0100  (com_load_seg)
seg 0x9FFF  conventional top (PSP field / arena end)
```

---

## File references

| Path | Why |
|------|-----|
| `/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm` | Dispatcher, COM load, pad |
| `/tmp/RetroCodeMess/FloppyOS/docs/int21.md` | AH table |
| `/tmp/RetroCodeMess/FloppyOS/docs/flopfs-spec.md` | `kernel_load_seg`, `com_load_seg` |
| `/tmp/RetroCodeMess/FloppyOS/programs/hello.asm` | COM consumer (02/09/30/35/4C) |
| `/tmp/RetroCodeMess/FloppyOS/tools/mkflopfs.c` | Defaults 0x1000 / 0x2000, max 64 secs |
| `/tmp/RetroCodeMess/FloppyOS/boot/stage1_5.asm` | Loads `k_secs` to `k_seg` |
| `/tmp/RetroCodeMess/FloppyOS/Makefile` | `kernel.bin` size gate |
