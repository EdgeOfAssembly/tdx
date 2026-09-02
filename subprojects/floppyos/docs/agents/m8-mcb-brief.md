# FloppyOS Milestone 8 — MCB / INT 21h AH=48/49/4A Implementation Brief

**Sources:** MS-DOS 2.0 `ALLOC.ASM` + `DOSSYM_v211.ASM`; MS-DOS 4.0 `INC/ARENA.INC` + `DOS/MSINIT.ASM`; FloppyOS `kernel/kernel.asm` (CS=0x1000, PSP=0x2000, PSP:[2]=0x9FFF); AXE skill `dos_memory_arena_analyzer` (oracle for dumps, not a code source).

---

## 1. MCB layout (exactly one paragraph = 16 bytes)

Each Memory Control Block occupies **one paragraph** immediately before the block it describes.

| Off | Size | Field | Notes |
|-----|------|-------|-------|
| 0 | 1 | `signature` | **`'M'` (0x4D)** = middle/normal; **`'Z'` (0x5A)** = **last** block in chain |
| 1 | 2 | `owner` | **0** = free; else **PSP segment** of owner (DOS system often uses special values e.g. 8 — M8 can treat non-zero as owned) |
| 3 | 2 | `size` | Size of **payload** in **paragraphs** — **does not include** this MCB paragraph |
| 5 | 3 | `reserved` | DOS 2–3 unused; leave 0 |
| 8 | 8 | `name` | DOS 4+ ASCII program name (space-padded); optional for M8, zero-fill |

```
; MS-DOS 4 ARENA.INC (canonical)
arena_signature     DB  ?     ; 'M' or 'Z'
arena_owner         DW  ?     ; 0 = free
arena_size          DW  ?     ; paragraphs of data after MCB
arena_reserved      DB  3 DUP(?)
arena_name          DB  8 DUP(?)
```

**Chain walk:**
```
next_mcb = current_mcb + arena_size + 1   ; +1 for the MCB paragraph itself
```
Stop when `signature == 'Z'`. Any other signature → **arena trashed** (error 7).

**Payload segment** returned to apps = `mcb_seg + 1` (this is what ES holds for free/resize).

---

## 2. INT 21h AH=48h — Allocate Memory

| | |
|--|--|
| **In** | `AH=48h`, `BX` = requested size in **paragraphs** |
| **Out OK (CF=0)** | `AX` = segment of allocated block (**MCB+1**); block owned by `CurrentPDB`/PSP |
| **Out fail (CF=1)** | `AX=8` not enough memory (`error_not_enough_memory`); `AX=7` arena trashed; **`BX` = largest free block size** (paragraphs) seen during scan |
| **Side effect** | May **split** a free block: allocated front (first-fit) or back (last-fit); remainder stays free with correct `'M'`/`'Z'` |

**MS-DOS algorithm (ALLOC.ASM `$ALLOC`):**
1. Walk from `arena_head`; on each free block (`owner==0`) **coalesce** forward free neighbors first.
2. Track first / best / last free block ≥ BX (AllocMethod: 0=first, 1=best, 2=last — **M8: first-fit only**).
3. If none: fail with BX=max free size found.
4. Split if free_size > BX:  
   - allocated MCB: `size=BX`, `owner=CurrentPSP`, sig=`'M'` (unless it was the last and no remainder)  
   - remainder MCB at `alloc_mcb+BX+1`: inherits old `'M'`/`'Z'`, `owner=0`, `size=old_size-BX-1`
5. If exact fit: just set `owner=CurrentPSP`; return `AX=mcb+1`.

---

## 3. INT 21h AH=49h — Free Memory

| | |
|--|--|
| **In** | `AH=49h`, `ES` = segment of block to free (**payload**, not MCB) |
| **Out OK** | CF=0 |
| **Out fail** | CF=1, `AX=9` invalid block (`error_invalid_block`); `AX=7` if signature bad |
| **Action** | `mcb = ES-1`; verify `'M'`/`'Z'`; set `owner = 0`. **Do not** require coalesce here (DOS free is lazy; coalesce runs on next alloc/setblock). M8 **may** coalesce immediately for simplicity. |

---

## 4. INT 21h AH=4Ah — Resize (SetBlock) — optional for M8

| | |
|--|--|
| **In** | `AH=4Ah`, `ES` = block, `BX` = new size in paragraphs |
| **Out OK** | CF=0 |
| **Out fail** | CF=1: `AX=7` trashed, `AX=9` invalid, `AX=8` cannot grow; on grow fail **`BX`=max possible** after coalesce |
| **Action** | `mcb=ES-1`; coalesce forward free; if `BX ≤ size` shrink/split; if `BX > size` fail unless free space absorbed by coalesce covers it |

Skip in M8 if time-boxed; many COM tests only need 48/49. EXEC later needs 4A (COM often allocates max then shrinks).

---

## 5. Recommended FloppyOS arena geometry

### Current layout (from tree)

| Region | Segment | Notes |
|--------|---------|-------|
| IVT/BDA/boot | 0x0000–… | Leave alone |
| FlopFS SB | **0x0900** | Superblock buffer |
| **Kernel** | **0x1000** | `kernel.asm`: `SS=CS`, `SP=0xFFFE` → uses **full 64 KiB** window 0x1000:0000–FFFF |
| **Init COM PSP** | **0x2000** | `clear_psp` sets `PSP:[2]=0x9FFF` (mem top marker) |
| VGA / UMA | **0xA000** | First non-conventional |

**Do not** place first MCB at `0x1000+0x100=0x1100` while SP=0xFFFE — that collides with the kernel stack. Treat kernel reserved as **0x1000 … 0x1FFF** (0x1000 paragraphs) until stack is moved.

### M8 recommended init (classic-compatible)

```
arena_head     = 0x2000          ; first MCB paragraph
top_of_mem     = 0xA000          ; or INT 12h_KB * 64; match PSP:[2]+1 style
                                 ; PSP:[2] is already 0x9FFF → top segment usable = 0x9FFF
                                 ; last free payload ends at 0xA000

Initial chain (before COM is “owned” by arena — pick ONE policy):
```

**Policy A — free arena above fixed COM (simplest M8):**
```
; COM stays hard-loaded at 0x2000 outside allocator (current behavior)
arena_head = 0x3000              ; leave 0x2000–0x2FFF for ~64K COM image
MCB@0x3000: sig='Z', owner=0, size = 0xA000 - 0x3000 - 1 = 0x6FFF
```

**Policy B — COM is first owned block (closer to real DOS):**
```
arena_head = 0x1FFF              ; MCB immediately before PSP 0x2000
MCB@0x1FFF: 'M', owner=0x2000, size = com_paras   ; e.g. 0x1000 (64K) or measured
MCB@next:   'Z', owner=0,      size = 0xA000 - next - 1
```
Note: MCB at 0x1FFF lives in the top of the kernel segment window; only safe if kernel does not use offsets ≥ 0xFFF0. Prefer **Policy A** or shrink kernel stack to e.g. 0x2000 and set `kernel_end_para` explicitly.

**Globals to keep in kernel:**
- `arena_head` (word)
- `current_psp` (word) — owner stamped on AH=48; init = 0x2000
- Optional: `alloc_strategy` (byte) = 0 first-fit

**Error codes (DOSSYM):** 7=arena_trashed, 8=not_enough_memory, 9=invalid_block.

---

## 6. Minimal first-fit algorithms

### Coalesce (forward only, from free block DS)
```
while sig != 'Z':
  next = DS + size + 1
  if next.sig not in ('M','Z'): error trashed
  if next.owner != 0: break
  DS.size += next.size + 1
  DS.sig = next.sig          ; absorb 'Z' if next was last
```

### Allocate (AH=48, first-fit)
```
scan from arena_head:
  check signature
  if owner==0:
    coalesce(this)
    if size >= BX:
      if size > BX: split (alloc low, free high)
      owner = current_psp
      return AX = this+1
  if sig=='Z': break
  else this = this+size+1
fail: BX = max_free_seen; AX=8; CF=1
```

### Free (AH=49)
```
mcb = ES-1
if mcb.sig not in ('M','Z'): AX=9; CF=1
mcb.owner = 0
; optional: walk from arena_head and coalesce all free runs
CF=0
```

---

## 7. Edge cases (must handle)

| Case | Behavior |
|------|----------|
| **Last block `'Z'`** | Only one `'Z'` in chain; on split of last block, allocated gets `'M'`, remainder gets `'Z'` |
| **Exact-fit free** | No split; just set owner |
| **Coalesce free** | Always before size test on alloc/4A; free may leave adjacent free until next alloc |
| **BX=0 allocate** | DOS allows; returns a 0-paragraph block (MCB only). Accept or reject — document choice |
| **Free already free** | DOS still “succeeds” (owner←0). OK |
| **Free invalid ES** | Bad signature → error 9 |
| **Grow into following free (4A)** | Coalesce then split |
| **Arena hole / bad link** | Any non-M/Z → error 7; never infinite-loop (cap walk or detect wrap) |
| **Top bound** | `mcb + size + 1 == 0xA000` for last block (no MCB past top) |
| **Kernel / SB clash** | Never put free MCBs below `arena_head`; never free kernel segment |
| **PSP:[2]** | Keep as highest usable segment (0x9FFF); apps use it as mem top hint |

---

## 8. Suggested smoke test COM (`programs/memtest.asm`)

```asm
; memtest.com — AH=48/49 smoke (org 100h)
; 1) AH=48 BX=0x10  → expect CF=0, AX=seg
; 2) write pattern 0xA5 to ES:0 .. 256 bytes (ES=AX)
; 3) read back verify
; 4) AH=48 BX=0xFFFF → expect CF=1, AX=8, BX=largest
; 5) AH=49 ES=first_block → CF=0
; 6) AH=48 BX=0x10 again → should succeed (reuse after free; better if coalesce)
; 7) print "MEM OK$" / "MEM FAIL$" via AH=09; AH=4C exit
```

**QEMU check:** serial banner + `MEM OK`; optional dump of MCB chain from debugger:
```
seg:0  'M'/'Z', owner, size  → walk until 'Z'
```
Golden: after boot with Policy A, single free MCB at `arena_head` with `owner=0`, `sig='Z'`, `size=0xA000-arena_head-1`.

**Oracle:** dump conventional memory post-boot → feed `AXE/skills/dos_memory_arena_analyzer.md` workflow (via dosbox_memory_dump) when comparing to MS-DOS 6 / FreeDOS.

---

## 9. Implementation checklist (kernel)

1. Data: `arena_head`, `current_psp`  
2. `arena_init` after `install_int21`: build initial free `'Z'` block  
3. Dispatch INT 21: `48`→alloc, `49`→free, (`4A`→setblock)  
4. Stamp `owner=current_psp` on alloc; set `current_psp=0x2000` before `run_init_com`  
5. On AH=4C later: free all MCBs with `owner==exiting_psp` (see `arena_free_process` in ALLOC.ASM) — not required for M8 smoke  
6. Document in `docs/int21.md`  
7. Smoke COM + `tests/smoke_qemu.sh` assertion on serial `MEM OK`

---

## 10. Key file references

| Path | Why |
|------|-----|
| `/home/wizard/MS-DOS/v2.0/source/ALLOC.ASM` | Full alloc/free/setblock/coalesce |
| `/home/wizard/MS-DOS/v2.0/source/DOSSYM_v211.ASM` L634–693, 735–737, 911–913 | Struct, errors, AH numbers |
| `/home/wizard/MS-DOS/v4.0/src/INC/ARENA.INC` | 16-byte MCB + name field |
| `/home/wizard/MS-DOS/v4.0/src/DOS/MSINIT.ASM` L128–147 | First arena setup (`arena_head = PDB-1`, single free/`system` block) |
| `/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm` | Load seg 0x1000, PSP 0x2000, mem top 0x9FFF |
| `/tmp/RetroCodeMess/AXE/skills/dos_memory_arena_analyzer.md` | Post-hoc MCB walk oracle |
