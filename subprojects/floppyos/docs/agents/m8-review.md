# FloppyOS Milestone 8 — MCB Review

**Date:** 2026-07-26  
**Scope:** `kernel/kernel.asm` (`mcb_init`, `dos_alloc`, `dos_free`, `dos_realloc`, `mcb_coalesce_es`, INT 21h 48/49/4A), `programs/hello.asm` MEM test, brief `/tmp/grok-1000/floppyos-m8-mcb-brief.md`  
**Verdict:** **fix-first** (do not ship as DOS-compatible 48/49/4A)

---

## Executive summary

Arena geometry (Policy A: first MCB `0x3000`, top `0xA000`) and first-fit split/coalesce structure are largely sound for the happy path. The **MEM OK** smoke can pass today because CF happens to stay clear across COM startup, not because error returns work.

Two correctness defects dominate:

1. **Critical:** `STC`/`CLC` before `IRET` never reaches the caller — stacked flags are restored, so AH=48/49/4A CF semantics are broken.
2. **Critical (4A):** shrink by exactly one paragraph updates `arena_size` without inserting a free MCB → chain skip / arena trash.

Kernel binary size (exactly 8192, load `0x1000`) still ends at paragraph `0x1200` &lt; COM `0x2000` — **OK**.

---

## Check results (requested)

| # | Check | Result |
|---|--------|--------|
| 1 | Stack/register corruption on INT 21h return (esp. BX on AH=48 fail) | **Critical CF bug**; BX itself is correct inside `dos_alloc` but useless if CF never reports failure |
| 2 | Coalesce infinite loops | **No loop on well-formed arena**; **possible** on corrupt `size` (wrap to self) — no cap |
| 3 | Arena bounds `0x3000–0xA000` vs kernel `0x1000` / COM `0x2000` | **OK** (Policy A) |
| 4 | `dos_free` CF/AX error handling | AX=9 on bad sig is right; **CF lost on IRET**; no AX=7 path |
| 5 | `dos_realloc` correctness | **Critical** shrink-by-1 hole; grow-after-coalesce OK; CF lost |
| 6 | 8192-byte kernel before COM `0x2000` | **OK** (`0x1000 + 0x200 = 0x1200 &lt; 0x2000`) |

---

## Findings

### F1 — Critical: INT 21h CF never returned (IRET restores pre-INT flags)

**Where:** `int21_handler` tails for `i48` / `i49` / `i4a` (and all other AH handlers):

```565:570:/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm
i48:    call    dos_alloc
        iret
i49:    call    dos_free
        iret
i4a:    call    dos_realloc
        iret
```

`dos_alloc` / `dos_free` / `dos_realloc` end with `stc`/`clc`, but **`IRET` pops FLAGS from the interrupt frame**. There is **no** write to the stacked flags word (no `[bp+6]` / `and`/`or` CF fixup anywhere in the kernel).

**Effect:**

| Path | Intended | Actual to caller |
|------|----------|------------------|
| AH=48 success | CF=0, AX=seg | CF = **whatever it was at `INT 21h` entry** |
| AH=48 fail | CF=1, AX=8, BX=largest | CF **not set**; AX=8, BX=largest still in regs |
| AH=49 fail | CF=1, AX=9 | CF **not set** |
| AH=4A fail | CF=1, AX=8/9 | CF **not set** |

**Why smoke still prints MEM OK:** `enter_com` does `xor ax,ax` (clears CF) before `retf`. `hello.asm` mostly uses `mov` between INTs (flags preserved). So CF stays 0 for the whole COM run — success looks correct by accident; **failure would also look like success**.

Failure scenario if alloc ever fails with CF sticky-clear:

```asm
; hello.asm — would treat AX=8 as a segment
mov es, ax          ; ES = 8
mov byte [es:0], 0xA5   ; writes into IVT
```

**Fix (canonical):** after `call dos_*`, set/clear CF in the flags image on the stack, then `iret`. Example pattern:

```asm
; SP: [IP][CS][FLAGS]  after INT
i48:    call    dos_alloc
        jc      .cf1
        ; clear CF in stacked flags
        push    bp
        mov     bp, sp
        and     byte [bp+6], 0xFE   ; BP+0=saved BP, +2=IP, +4=CS, +6=flags low
        pop     bp
        iret
.cf1:   push    bp
        mov     bp, sp
        or      byte [bp+6], 0x01
        pop     bp
        iret
```

(Or one shared `int21_return` that branches on CF.) Same fix is required for AH=3D/3E/3F and unknown-AH — pre-existing systemic bug, **blocking for M8 memory API claims**.

**Registers on AH=48:** `dos_alloc` correctly leaves **BX = `al_best`** on fail and does **not** push/pop BX. CX/DX/SI/DI/BP/ES preserved. AX = result. **No BX corruption** in the callee — only the CF channel is broken.

---

### F2 — Critical: AH=4A shrink by 1 paragraph breaks the MCB chain

**Where:** `dos_realloc` shrink path (same threshold as alloc split):

```204:222:/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm
        ; shrink
        mov     cx, ax
        sub     cx, bx
        cmp     cx, 2
        jb      .set
        mov     [es:3], bx
        ...
        dec     cx
        mov     [es:3], cx
        ...
        jmp     .set
...
.set:   mov     ax, [cs:al_need]
        mov     [es:3], ax
```

When `old_size - new_size == 1` (`cx == 1`):

1. `cmp cx, 2` / `jb .set` → **no free MCB created**
2. `.set` writes `arena_size = new_size` (one less than before)
3. Next walk: `next = mcb + new_size + 1` points **one paragraph before** the real next MCB
4. Chain reads payload bytes as an MCB → signature trash → alloc fails / wrong merges

**Contrast with AH=48:** same `cmp cx, 2` on alloc only **over-allocates** (gives need+1) — safe. On **4A shrink**, changing size without a remainder MCB is arena corruption.

**DOS-correct rule:** split whenever `old > new` (allow free MCB with `size == 0` when `old == new+1`):

```asm
sub cx, bx        ; remainder paras including the new MCB slot
jz  .set          ; exact — no split
; cx >= 1 → create free MCB with size cx-1 (may be 0)
```

**Grow path:** coalesce-then-compare is correct; dead `cmp ax,bx / jae .set` under `.grow` is unreachable (harmless). Grow fail sets `BX=[es:3]` (max possible after coalesce) — good **if CF worked**.

---

### F3 — Major: bad MCB signature reported as AX=8, not AX=7

**Where:** `dos_alloc` `.w` → `.fail`:

```69:73:/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm
.w:     mov     al, [es:0]
        cmp     al, 'M'
        je      .sig
        cmp     al, 'Z'
        je      .sig
        jmp     .fail
```

`.fail` always `mov ax, 8`. Brief / DOSSYM: **7 = arena_trashed**, **8 = not_enough_memory**.

Apps that distinguish “OOM, try smaller BX” vs “memory arena destroyed” will mis-handle trash.

**Also:** `dos_free` / `dos_realloc` use AX=9 for bad sig on the **target** block (correct for invalid block) but never surface AX=7 if the **chain** is trash during coalesce.

---

### F4 — Major: coalesce does not validate next signature; no walk bound / anti-wrap

**Where:** `mcb_coalesce_es`:

```131:148:/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm
.co:    cmp     byte [es:0], 'Z'
        je      .cdone
        mov     ax, es
        add     ax, [es:3]
        inc     ax
        push    es
        mov     es, ax
        cmp     word [es:1], 0
        jne     .cno
        ...
        add     [es:3], bx
```

Issues:

1. **No check** that next `[0]` is `'M'` or `'Z'` before treating `owner==0` as a free MCB.
2. **No `ARENA_TOP` check** — corrupt `size` walks into ROM/VGA.
3. **Wrap infinite loop (corrupt `size == 0xFFFF` on a free `'M'`):**  
   `next = es + 0xFFFF + 1 == es` → self, owner 0 → `bx = 0` → `add size, 0` → `jmp .co` forever (sig stays `'M'`).

Well-formed Policy A init (`'Z'`, size `0x6FFF`) does **not** hit this. Still required for “arena trashed” hardening (brief §7).

**Normal coalesce control flow is fine:** absorbing a following `'Z'` sets local sig to `'Z'` then `.co` exits; owned next stops; multi-free runs merge forward.

---

### F5 — Minor: AH=48 over-allocates when `free_size == need + 1`

`cmp cx, 2` / `jb .take` refuses to create a **0-paragraph free** remainder. DOS typically splits and leaves a free MCB with `size=0`.

**Impact:** block’s `arena_size` is `need+1`; caller still only asked for `need` usable paras — usually OK. Slightly wrong for tools that trust MCB size == request. Prefer split on `cx >= 1` for DOS parity (same fix as F2).

---

### F6 — Minor: `hello.asm` MEM test is thin

```51:66:/tmp/RetroCodeMess/FloppyOS/programs/hello.asm
        mov     bx, 16
        mov     ah, 0x48
        int     0x21
        jc      .memfail
        mov     es, ax
        mov     byte [es:0], 0xA5
        ...
        mov     ah, 0x49
        int     0x21
        jc      .memfail
        ... "MEM OK"
```

Missing vs brief smoke (`memtest`):

- OOM path: `BX=0xFFFF` → expect CF=1, AX=8, BX=largest (~`0x6FFF` after init)
- Free then re-alloc (coalesce / reuse)
- Optional AH=4A shrink/grow

With F1 unfixed, the OOM test would **false-pass** (`jc` not taken).

---

### F7 — OK: Arena geometry (Policy A)

```11:12:/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm
ARENA_FIRST     equ 0x3000
ARENA_TOP       equ 0xA000
```

```40:54:/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm
mcb_init:
        ...
        mov     byte [es:0], 'Z'
        mov     word [es:1], 0
        mov     ax, ARENA_TOP
        sub     ax, ARENA_FIRST
        dec     ax              ; size = 0x6FFF
        mov     [es:3], ax
```

| Region | Paras | Notes |
|--------|-------|-------|
| Kernel | `0x1000` | SS=CS, SP=`0xFFFE` → phys `0x10000–0x1FFFF` |
| COM PSP | `0x2000` | Fixed load; `PSP:[2]=0x9FFF` |
| First MCB | `0x3000` | Above full COM 64K window |
| Top | `0xA000` | Exclusive; last payload ends `0x9FFF` |

`0x3000 + 0x6FFF + 1 = 0xA000` — **exact**. No clash with kernel stack (different 64K linear window than COM/arena).

---

### F8 — OK: `current_psp` for owner stamp

- Boot: `current_psp = CS` (`0x1000`) — unused for alloc before COM.
- `enter_com` sets `current_psp = psp_seg` before `retf` — **correct** for COM-originated AH=48.
- Matches `docs/int21.md` (“set to COM PSP on enter”).

---

### F9 — OK: Kernel 8 KiB vs COM at `0x2000`

- `Makefile`: `test $(wc -c) -eq 8192`
- `kernel.asm`: `times 8192 - ($ - $$) db 0`
- Load segment `0x1000`, size `0x2000` bytes = `0x200` paragraphs → end paragraph **`0x1200`**
- COM at **`0x2000`** → **`0x1200 < 0x2000`** — gap free (historically stage/FS buffers, not MCB)

---

### F10 — OK / note: `dos_free` behavior

```156:179:/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm
dos_free:
        ...
        ; bad sig → AX=9, stc
.ok:    mov     word [es:1], 0
        call    mcb_coalesce_es
        xor     ax, ax
        clc
```

- Payload→MCB via `ES-1`: correct.
- Lazy-or-eager: immediate forward coalesce is allowed by brief.
- Does **not** coalesce **backward** (previous free); next AH=48 walk from `first_mcb` coalesces when it hits the earlier free — **OK**.
- Free already-free: succeeds (owner←0) — DOS-like.
- ES restored to caller payload segment — good.
- **CF still broken (F1).**

---

## Coalesce / alloc walk (no infinite loop on clean arena)

Happy-path alloc:

1. `mcb_init` → single free `'Z'` @ `0x3000`, size `0x6FFF`
2. AH=48 BX=16 → coalesce no-op (already Z) → split → alloc `'M'` size 16 @ `0x3000`, free `'Z'` @ `0x3011` size `0x6FFF-16-1`
3. AH=49 → owner 0 → coalesce merges into one `'Z'` again

`.nxt` stops on `'Z'`; coalesce stops on `'Z'` or owned next. **No infinite loop** unless F4 corruption.

---

## Severity index

| ID | Severity | Title |
|----|----------|-------|
| F1 | **critical** | IRET drops CF — 48/49/4A (and 3D/3E/3F) error returns broken |
| F2 | **critical** | AH=4A shrink-by-1 (and any shrink with remainder &lt; 2 paras handled as “no split”) trashes chain |
| F3 | **major** | Arena trash → AX=8 instead of AX=7 |
| F4 | **major** | Coalesce/walk: no sig check, no top bound, wrap can spin |
| F5 | **minor** | AH=48 keeps need+1 instead of size-0 free MCB |
| F6 | **minor** | MEM smoke too weak; cannot catch F1/F2 |
| F7–F10 | ok | Geometry, PSP owner, 8K fit, free forward-coalesce |

---

## Fix-first list (ship gate)

**Must fix before ship / before claiming M8 MCB done:**

1. **F1** — Stacked-flags CF fixup on all INT 21h exits that report errors (minimum: 48/49/4A; preferably shared exit for 3D/3E/3F/unknown too).
2. **F2** — AH=4A: split on any shrink (`old > new`), allow free `size=0`; never reduce `arena_size` without a remainder MCB (or refuse shrink and keep old size).

**Should fix in same pass:**

3. **F3** — Return AX=7 on bad signature during chain walk.
4. **F4** — Validate next M/Z; abort coalesce with trash; optional max-steps or `next >= ARENA_TOP` guard.
5. **F5** — Align alloc split threshold with F2 (`jcxz` / `cx>=1`).
6. **F6** — Extend COM test: OOM (`BX=0xFFFF` + check CF/AX/BX), free+realloc; assert in `smoke_qemu.sh` if serialized.

**OK as-is:**

- Policy A layout `0x3000`/`0xA000`
- First-fit split of last `'Z'` (alloc gets `'M'`, tail keeps `'Z'`)
- `current_psp` at COM entry
- Kernel 8192 @ `0x1000` vs COM `0x2000`
- BX largest-free on alloc fail **once CF works**

---

## Final recommendation

# **fix-first**

Do **not** ship M8 as DOS-compatible memory services until **F1** is fixed. Happy-path MEM OK is insufficient evidence. If AH=4A remains exported, **F2** is also a hard blocker (silent arena corruption on common “shrink by 1 para” / odd remainder). After F1+F2, re-run `tests/smoke_qemu.sh` and add an OOM/CF assertion.

**Ship only if:** product explicitly documents “CF unreliable / 4A unsafe” and M8 is demoted to experimental — not recommended.
