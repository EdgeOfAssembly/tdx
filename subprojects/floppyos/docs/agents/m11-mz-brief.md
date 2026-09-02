# FloppyOS Milestone 11 — MZ .EXE Loader (INT 21h AH=4B00)

**Status:** design brief for implementers  
**Depends on:** M10 COM exec (`dos_exec` in `kernel/kernel.asm`), MCB arena (M8), FlopFS open/read (M7), PSP + AH=4C parent return (M10)  
**Oracle math:** `/tmp/RetroCodeMess/retro/docs/RE/exe_format_reference.md`  
**Style match:** `docs/agents/m8-mcb-brief.md`

---

## 0. Goal

Extend **INT 21h AH=4B AL=00** so it loads classic **real-mode MZ .EXE** files (not just .COM), then transfers control with correct registers and relocations applied.

**Out of scope for M11:** overlays (`e_ovno != 0`), NE/PE/LE, environment block copy, FCB fill from EPB, `e_maxalloc` full shrink dance (optional), checksum.

**Success smoke (serial):**

```text
… → COMMAND → EXEC HELLO.EXE → Hello EXE → EXEC OK → SHELL OK
```

(Keep HELLO.COM path working; branch on magic.)

---

## 1. MZ header fields needed

Read **at least 28 bytes** (`0x1C`) from file offset 0 after open. All multi-byte fields are **little-endian words**.

| Off | Size | Field | Role for M11 loader |
|-----|------|-------|---------------------|
| `00` | 2 | `e_magic` | Must be `0x5A4D` (`'MZ'`) or `0x4D5A` (`'ZM'` — accept both) |
| `02` | 2 | `e_cblp` | Bytes used in **last** 512-byte page (`0` = full page) |
| `04` | 2 | `e_cp` | File size in **512-byte pages** (rounded up) |
| `06` | 2 | `e_crlc` | Number of relocation entries |
| `08` | 2 | `e_cparhdr` | Header size in **paragraphs** (includes reloc table) |
| `0A` | 2 | `e_minalloc` | Min extra paragraphs **above** load image |
| `0C` | 2 | `e_maxalloc` | Max extra paragraphs (`0xFFFF` = take all free — optional M11) |
| `0E` | 2 | `e_ss` | Initial SS **relative to load_seg** |
| `10` | 2 | `e_sp` | Initial SP (absolute offset in SS) |
| `12` | 2 | `e_csum` | Ignore |
| `14` | 2 | `e_ip` | Entry IP |
| `16` | 2 | `e_cs` | Initial CS **relative to load_seg** |
| `18` | 2 | `e_lfarlc` | File offset of first relocation entry |
| `1A` | 2 | `e_ovno` | Overlay number; **must be 0** for M11 (else fail) |

**Ignore for M11:** `e_res`, OEM fields, `e_lfanew` (PE). Pure DOS EXEs have no PE header.

```c
/* packed layout for mental model */
struct mz_hdr {
    uint16_t e_magic;    /* 0x00 */
    uint16_t e_cblp;     /* 0x02 */
    uint16_t e_cp;       /* 0x04 */
    uint16_t e_crlc;     /* 0x06 */
    uint16_t e_cparhdr;  /* 0x08 */
    uint16_t e_minalloc; /* 0x0A */
    uint16_t e_maxalloc; /* 0x0C */
    uint16_t e_ss;       /* 0x0E */
    uint16_t e_sp;       /* 0x10 */
    uint16_t e_csum;     /* 0x12 */
    uint16_t e_ip;       /* 0x14 */
    uint16_t e_cs;       /* 0x16 */
    uint16_t e_lfarlc;   /* 0x18 */
    uint16_t e_ovno;     /* 0x1A */
};
```

---

## 2. Image size calculation (`e_cp` / `e_cblp`)

**Total file size claimed by header** (not necessarily `stat` size):

```text
if e_cblp == 0:
    file_bytes = e_cp * 512
else:
    file_bytes = (e_cp - 1) * 512 + e_cblp
```

**Header size on disk:**

```text
hdr_bytes = e_cparhdr * 16
```

**Load module size** (bytes copied into memory — **not** including MZ header):

```text
image_bytes = file_bytes - hdr_bytes
```

**Load module size in paragraphs** (round up):

```text
image_paras = (image_bytes + 15) / 16
```

**Sanity checks (fail CF=1 if broken):**

- `e_cp >= 1`
- `hdr_bytes >= 0x1C` and `hdr_bytes < file_bytes` (or `image_bytes > 0`)
- Prefer also: `file_bytes <= actual FlopFS size` (use handle size from M10)

**Note:** DOS historically trusts the header for load length, not the directory size. M11 may clamp to min(header, file size) for safety.

---

## 3. Memory: PSP (16 paras) + load module + minalloc

Classic DOS layout for EXE:

```text
[ MCB ]
[ PSP          ]  16 paragraphs (256 bytes)  ← child_psp = AX from AH=48
[ load image   ]  image_paras                ← load_seg = child_psp + 0x10
[ extra / BSS  ]  e_minalloc (min) … e_maxalloc (max)
```

**Allocation size in paragraphs (M11 minimum):**

```text
alloc_paras = 16 + image_paras + e_minalloc
```

Optional (closer to DOS when `e_maxalloc == 0xFFFF`):

```text
; allocate largest free, then AH=4A shrink to:
;   16 + image_paras + min(e_maxalloc, free_after_image)
; M11 may skip: just alloc 16 + image_paras + e_minalloc
```

**Reuse M10 PSP init** (`clear_psp` / inline in `dos_exec`):

| PSP off | Value |
|---------|--------|
| `00` | `CD 20` (INT 20h) |
| `02` | top of memory segment (keep `0x9FFF` or compute from MCB) |
| `16h` | parent PSP (`parent_psp`) |
| `50h` | `CD 21 CB` (INT 21h / RETF) |
| `80h` | command tail (0 + CR for now; EPB later) |

**Owner:** MCB owner = `child_psp` (same as COM path).  
**Free on AH=4C:** free the **PSP segment** block (entire alloc including image) — already done in M10 `i4c`.

---

## 4. Where the image loads

```text
load_seg = child_psp + 0x10     ; first paragraph after PSP
```

- **Do not** load at `PSP:0100` (that is COM-only).
- Read **`image_bytes`** from file offset **`hdr_bytes`** into linear address `load_seg:0000`.
- Implementation sketch (real mode):

```text
; after open + parse header + alloc:
; seek/read: skip first hdr_bytes (or AH=3F loop discard, or offset into FlopFS read)
; DS = load_seg, DX = 0, CX = image_bytes (may need multi-read if >64K — M11: require image_bytes < 64K)
call dos_read
```

**M11 size limit:** single-segment load image (`image_bytes ≤ 0xFFFF`). Multi-segment EXEs still work if image fits in one alloc and CS/SS are relative paragraphs within it — but one `dos_read` must handle CX≤65535; if image >64K, loop reads (optional stretch goal).

---

## 5. Relocation table

Each entry is **4 bytes** at file offset `e_lfarlc`:

| Bytes | Field |
|-------|--------|
| 0–1 | `off` — offset within segment |
| 2–3 | `seg` — segment **relative to load_seg** |

For `i = 0 .. e_crlc-1`:

```text
entry_file_off = e_lfarlc + i * 4
read word off, word seg

; patch location in memory:
;   linear = (load_seg + seg) * 16 + off
;   word_at(linear) += load_seg
```

Real-mode code pattern:

```asm
; BX = load_seg
; SI:DI or far pointer to reloc item
mov     ax, [reloc_seg]     ; relative segment from entry
add     ax, bx              ; + load_seg
mov     es, ax
mov     di, [reloc_off]
add     [es:di], bx         ; add load_seg to the fixup word
```

**`e_crlc == 0`:** skip loop (valid; many tiny hand-built EXEs).

**Bounds:** `e_lfarlc + e_crlc*4 ≤ hdr_bytes` (relocs live inside header region). Fail if not.

---

## 6. Register setup at entry

After load + relocations:

| Register | Value |
|----------|--------|
| **DS** | `child_psp` (PSP segment) |
| **ES** | `child_psp` |
| **SS** | `load_seg + e_ss` |
| **SP** | `e_sp` |
| **CS** | `load_seg + e_cs` |
| **IP** | `e_ip` |
| AX | often `0` or drive-related; M10 COM sets DL=boot drive — keep harmless zeros OK |
| BX,CX,DX,SI,DI | 0 (DOS does not guarantee; zero is fine) |

Transfer:

```asm
mov     ax, [child_psp]
mov     ds, ax
mov     es, ax
mov     ax, [load_seg]
add     ax, [e_ss]
cli
mov     ss, ax
mov     sp, [e_sp]
sti
; far jump: push CS' then IP then retf
mov     ax, [load_seg]
add     ax, [e_cs]
push    ax
mov     ax, [e_ip]
push    ax
retf
```

**Contrast with COM (M10 current):**

| | COM | EXE |
|--|-----|-----|
| Load | `PSP:0100` whole file | `load_seg:0000` image only |
| CS:IP | `PSP:0100` | `load_seg+e_cs : e_ip` |
| SS:SP | `PSP:FFFE` | `load_seg+e_ss : e_sp` |
| DS/ES | PSP | PSP |

Parent save/restore (`parent_ss/sp`, `exec_active`, free child on 4C) **unchanged** from M10.

---

## 7. Distinguishing COM vs EXE

After open, **read first 2 bytes** (or full 28-byte header):

```text
if word[0] == 0x5A4D or word[0] == 0x4D5A:
    → MZ EXE path
else:
    → COM path (M10 behavior)
```

**Do not** rely on filename extension alone (DOS does not; `FOO.COM` can be MZ and vice versa is rare but magic wins).

**COM path remains:**

1. `alloc_paras = (fsize + 0x100 + stack_slack + 15) / 16` (existing M10 formula)
2. Load entire file at `PSP:0100`
3. `CS=DS=ES=SS=PSP`, `IP=0x100`, `SP=0xFFFE`

**Edge:** MZ with `e_maxalloc==0` and tiny model sometimes used for “COM-like EXE”; still use EXE rules if magic matches.

---

## 8. Minimal test EXE strategy

### 8.1 Preferred: NASM tiny MZ (no relocs)

`programs/hello_exe.asm` — build with NASM to a flat MZ file (header + image).

Sketch:

```asm
; hello_exe.asm — minimal MZ for FloppyOS M11
        bits 16

; --- MZ header (32 bytes / 2 paragraphs for simplicity) ---
e_magic:    db 'M','Z'
e_cblp:     dw IMAGE_END % 512
e_cp:       dw (IMAGE_END + 511) / 512
e_crlc:     dw 0
e_cparhdr:  dw 2                ; 32-byte header
e_minalloc: dw 0x10             ; 256 bytes extra
e_maxalloc: dw 0xFFFF
e_ss:       dw 0                ; SS = load_seg
e_sp:       dw 0x100            ; small stack in extra
e_csum:     dw 0
e_ip:       dw start            ; offset within image
e_cs:       dw 0                ; CS = load_seg
e_lfarlc:   dw 0x1C
e_ovno:     dw 0
            times 32-($-$$) db 0

; --- load image (file offset 32) ---
start:
        push    cs
        pop     ds              ; if we need DS=CS for data; DOS sets DS=PSP
        ; Better: use PSP via DS as DOS left it — put string in a known place
        ; For AH=09 need DS:DX → use CS override or set DS=CS
        mov     ax, cs
        mov     ds, ax
        mov     dx, msg
        mov     ah, 0x09
        int     0x21
        mov     ax, 0x4C00
        int     0x21

msg:    db "Hello EXE", 13, 10, "$"

IMAGE_END equ $
```

Build:

```bash
nasm -f bin -o programs/hello.exe programs/hello_exe.asm
# pack into FlopFS image via mkflopfs -f HELLO.EXE=...
```

**Fix `e_cblp`/`e_cp`:** either compute with a tiny host script after assemble, or pad image so size is exact and set fields by hand in a second pass. Simplest: **fixed pad** to known size (e.g. 512-byte file: header 32 + image 480).

### 8.2 Hand-built binary (no assembler reloc)

Hex-construct 32-byte header + tiny body that only does `INT 21h AH=09` / `AH=4C`. Useful if NASM header math is fiddly. Keep under 256 bytes total.

### 8.3 One reloc test (optional second binary)

```asm
; data word that must become load_seg after fixup
seg_fix: dw 0                 ; reloc entry points here
; reloc table: off=offset(seg_fix), seg=0
; at runtime: mov ds, [cs:seg_fix] should equal load_seg
```

### 8.4 COMMAND.COM change

Mirror M10 HELLO.COM exec:

```asm
exe_name: db "HELLO.EXE", 0
; AH=4B00 same EPB
```

Print `EXEC EXE OK` on success. Keep COM test in smoke or alternate.

### 8.5 Makefile / image

- Add `programs/hello.exe` target
- `mkflopfs -f` include `HELLO.EXE`
- `make smoke` serial expect `Hello EXE` (or both COM+EXE)

---

## 9. Suggested `dos_exec` control flow (delta from M10)

```text
dos_exec (M11):
  open path (AH=3D path)
  read 2 bytes magic (or 28-byte header into CS buffer)
  if MZ/ZM:
      parse full header
      if e_ovno != 0 → fail AX=1 or 11
      compute file_bytes, hdr_bytes, image_bytes, image_paras
      alloc_paras = 16 + image_paras + e_minalloc
      dos_alloc → child_psp
      init PSP
      load_seg = child_psp + 0x10
      read image_bytes from file off hdr_bytes → load_seg:0
      for each reloc: add load_seg to word at (load_seg+seg):off
      close handle
      exec_active=1; current_psp=child_psp; DTA=PSP:80
      DS=ES=child_psp
      SS:SP = load_seg+e_ss : e_sp
      retf to load_seg+e_cs : e_ip
  else:
      existing COM path (unchanged)
```

**Error codes (match M10 style):**

| AX | Meaning |
|----|---------|
| 2 | file not found / open fail |
| 8 | not enough memory |
| 11 | invalid format (bad magic fields, overlay, trunc image) — optional; or reuse 1 |

On any fail after alloc: close handle, free child, CF=1 return to `i4b`.

---

## 10. Files to touch

| Path | Change |
|------|--------|
| `kernel/kernel.asm` | Branch in `dos_exec`; MZ parse/load/reloc/entry; maybe `mz_hdr` buffer in CS data |
| `programs/hello_exe.asm` (new) | Minimal test EXE |
| `programs/command.asm` | EXEC HELLO.EXE (or both) |
| `Makefile` | Build EXE + pack image |
| `docs/int21.md` | Note AH=4B loads COM **and** MZ EXE |
| `README.md` / `TODO.md` | Milestone M11 |

**Do not** change MCB algorithm, FlopFS, or AH=4C parent return unless a bug is found.

---

## 11. Acceptance checklist

- [ ] HELLO.COM still runs via AH=4B (regression)
- [ ] HELLO.EXE (0 relocs) prints and returns to COMMAND via AH=4C
- [ ] DS/ES = PSP at entry (verify with a test that writes PSP:80 or uses AH=09 with DS=PSP carefully)
- [ ] SS:SP from header (stack not clobbering code)
- [ ] Optional: 1-reloc EXE patches correctly
- [ ] Bad magic / truncated header → CF=1, parent continues
- [ ] `make smoke` green with serial evidence

---

## 12. References

- MS-DOS MZ layout: `retro/docs/RE/exe_format_reference.md` §1
- Current COM exec: `kernel/kernel.asm` `dos_exec` (~line 656)
- INT 21h table: `docs/int21.md`
- MCB: `docs/agents/m8-mcb-brief.md`
- Ralf Brown: INT 21h AH=4B AL=00 (load and execute)

---

*End of M11 brief.*
