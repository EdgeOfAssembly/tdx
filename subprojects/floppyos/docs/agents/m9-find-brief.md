# FloppyOS Milestone 9 — INT 21h Find (AH=1A / 4E / 4F) Brief

**Scope:** Minimal real-mode DOS-compatible directory search for **FlopFS root only**, **8.3 names only**. Enough for a tiny `DIR` and apps that call FindFirst/FindNext.

**Depends on:** M7 root dir (`root_buf`, 16 × 32-byte FCB entries), M7 path→FCB uppercasing, existing CF/AX error style.

**Out of scope for M9:** subdirectories, paths with drive letters beyond ignore, volume labels, LFN, FCB-style AH=11/12, multi-drive.

---

## 1. DTA layout (FindFirst result — what DIR reads)

Default DTA is **PSP:0080h** until AH=1A changes it. After a successful AH=4E/4F, the **current DTA** holds:

| Off | Size | Field | M9 value |
|----:|-----:|-------|----------|
| 00h | 21 | **reserved** (DOS find state) | See §7 — apps must not touch; kernel may leave zeros if state is internal |
| **15h** | 1 | **attribute** | Synthesize: `0x20` (archive) for normal FlopFS files; `0x10` only if you later add dirs |
| **16h** | 2 | **time** (DOS packed) | `0` if FlopFS has no timestamps |
| **18h** | 2 | **date** (DOS packed) | `0` if none |
| **1Ah** | 4 | **size** (bytes, LE dword) | From dirent `size` @+16 |
| **1Eh** | 13 | **filename** ASCIIZ | `NAME.EXT` or `NAME` (no trailing spaces); max 12 chars + NUL |

```
; Offsets relative to DTA base (DS:DX from AH=1A, or default PSP:80h)
DTA_ATTR    equ 15h   ; BYTE
DTA_TIME    equ 16h   ; WORD
DTA_DATE    equ 18h   ; WORD
DTA_SIZE    equ 1Ah   ; DWORD
DTA_FNAME   equ 1Eh   ; 13 bytes ASCIIZ
```

**DIR loop (app side):** `AH=1A` → set DTA buffer → `AH=4E` with `*.*` → print DTA+1Eh / size → `AH=4F` until CF and `AX=18`.

---

## 2. AH=1Ah — Set Disk Transfer Address

| | |
|--|--|
| **In** | `AH=1Ah`, **DS:DX** = far pointer to DTA buffer (≥ 43 bytes used by find) |
| **Out** | none (no CF convention required; DOS always succeeds) |
| **Action** | Store `dta_seg=DS`, `dta_off=DX` in kernel. All subsequent AH=4E/4F (and later AH=2F get-DTA) use this. |
| **Default** | On process start: DTA = **current PSP:0080h** (same as classic DOS). M9: set when loading COM, or hardcode PSP `0x2000:0080` until multi-PSP. |

No get-DTA (AH=2F) required for M9 unless shell needs it.

---

## 3. AH=4Eh — Find First Matching File

| | |
|--|--|
| **In** | `AH=4Eh`, **CX** = attribute mask, **DS:DX** = ASCIIZ filespec |
| **Out OK (CF=0)** | DTA filled (§1); AL may be undefined — ignore |
| **Out fail (CF=1)** | **AX** = error (§5) |

**Filespec (M9):**

- Accept: `*.*`, `HELLO.COM`, `HELLO`, `\HELLO.COM`, optional leading `\`
- Reject / ignore path components: no `A:`, no `SUB\FILE` → **AX=3** (path not found) or treat as not found
- Convert to **11-byte FCB pattern** (8+3, space-pad, upper case) via same rules as AH=3D, plus wildcards (§6)

**Attribute mask (minimal):**

| CX bit | Meaning | M9 |
|--------|---------|-----|
| 0x01 | R/O | ignore (no FlopFS attr yet) |
| 0x02 | Hidden | match only if set (we never set → skip if you invent hidden) |
| 0x04 | System | same |
| 0x08 | Volume | **never match** (no vol label entries) |
| 0x10 | Directory | **never match** (root-only files) |
| 0x20 | Archive | ignore |

**Classic DOS rule (simplified):** a directory entry matches if every “special” bit it has (H/S/V/D) is also set in CX; normal files always match. M9 with only plain files: **any CX accepts all root files** that pass the name wildcard.

**Algorithm:**

1. Parse DS:DX → FCB pattern (11 bytes, with `?`).
2. Save pattern + attr mask in **kernel find state** (§7); set `next_index = 0`.
3. Scan root entries from index 0; first match → fill DTA, set `next_index = match+1`, CF=0.
4. No match → CF=1, **AX=18** (no more files). Optional: AX=2 if pattern has no wildcards and exact name missing — **M9 may always use 18** for simplicity (DIR-compatible enough).

**Root scan:** `root_buf`, 16 entries × 32 bytes; skip `name[0]==0` (free/end). Compare FCB name[0..10] with pattern (§6).

---

## 4. AH=4Fh — Find Next Matching File

| | |
|--|--|
| **In** | `AH=4Fh` only (uses current DTA + kernel state from last 4E) |
| **Out OK (CF=0)** | Next match written to **same** DTA |
| **Out fail (CF=1)** | **AX=18** no more files |

**Action:** Continue scan from kernel `next_index` with saved pattern/attr. On match, update DTA + `next_index`. If exhausted → CF=1, AX=18.

**Do not** re-parse filespec from DTA reserved area unless you implement full DOS DTA state; prefer kernel state (§7).

---

## 5. Error codes

| AX | Meaning | When |
|----|---------|------|
| **18** | `ERROR_NO_MORE_FILES` | No match on 4E, or exhausted on 4F (**primary**) |
| 2 | file not found | Optional exact-name 4E miss |
| 3 | path not found | Spec contains subdir / bad path |
| 1 | invalid function | Not used if 1A/4E/4F implemented |

**Carry flag:** success CF=0; error CF=1 and AX=code. Match existing kernel INT 21h epilogue style (M7/M8).

---

## 6. Wildcard matching (minimal 8.3 FCB)

### 6.1 ASCIIZ → 11-byte pattern

1. Uppercase A–Z; reject or stop at first `\` after optional leading `\`.
2. Split on first `.`: name field / ext field. No dot → ext = three spaces.
3. Copy into 8-char name and 3-char ext, **space-pad** on the right.
4. **`*` handling (per field):** on seeing `*`, fill **remaining** bytes of **that field only** with `?`. Characters after `*` in the same field are ignored (DOS behavior).
5. **`?`** copies through as `?` (matches any single character in that position).

Examples:

| Spec | FCB pattern (· = space) |
|------|-------------------------|
| `*.*` | `???????????` |
| `HELLO.COM` | `HELLO···COM` |
| `H*.C*` | `H???????C??` |
| `HI?` | `HI?·······` (ext spaces) |
| `*.COM` | `????????COM` |

### 6.2 Match predicate

For `i = 0..10`:

```
match if pattern[i] == '?' OR pattern[i] == entry[i]
```

Entry names are already upper-case space-padded FCB (FlopFS dirent[0..10]). No LFN, no lowercase.

### 6.3 Dot files / empty

- `name[0]==0` → end/free, stop or skip.
- Do not match deleted `0xE5` if you ever use it.

---

## 7. Recommended: kernel find state (not DTA-only)

DOS stores search context in DTA[0..14]; the format is version-specific and painful. **M9 recommendation:**

```
; Kernel (CS-relative), one active search (enough for single-task DIR)
find_active   db 0          ; 0=none, 1=search open
find_attr     db 0          ; CX low byte from 4E
find_next     db 0          ; next root index to try (0..16)
find_pat      db 11 dup(0)  ; FCB pattern with ?
; dta_seg / dta_off from AH=1A
```

| Why kernel state | |
|------------------|---|
| FlopFS root is a flat array — **index** is the natural cursor | |
| Avoid inventing a fake DOS DTA reserved layout | |
| Apps that only read attr/time/date/size/name still work | |
| Single-threaded kernel: one search is enough for M9 | |

**Still write** attr/time/date/size/name into the user DTA so DIR and Turbo C `findfirst` work. Optionally zero DTA[0..14] or store a private cookie; do **not** require apps to preserve DTA for 4F if you key solely off kernel state (stricter than DOS, fine for M9 single-task).

**Multi-search later:** cookie in DTA[0] + table of states, or full DOS-compatible reserved block.

---

## 8. Implementation sketch (kernel)

```
AH=1A:  dta_seg=DS; dta_off=DX; iret success

AH=4E:  parse DS:DX → find_pat
        find_attr=CL; find_next=0; find_active=1
        jmp find_scan

AH=4F:  if !find_active → fail AX=18
        /* fall through find_scan */

find_scan:
        for i = find_next .. 15:
            de = root_buf + i*32
            if de[0]==0: break
            if !fcb_match(find_pat, de): continue
            write DTA: attr=0x20, time=0, date=0, size=de.size
            fcb_to_asciiz(de, DTA+1Eh)
            find_next = i+1
            CF=0; return
        find_active=0; CF=1; AX=18
```

**FCB→ASCIIZ for DTA+1Eh:** trim trailing spaces from name; if ext not all spaces, emit `.` + trim ext; NUL-terminate (e.g. `HELLO.COM`, `README`).

---

## 9. Acceptance checks

1. `AH=1A` with buffer; `AH=4E` CX=0 DS:DX=`*.*` → first root file in DTA+1Eh, size correct.
2. Repeated `AH=4F` walks all packed root files; then CF=1, AX=18.
3. `HELLO.COM` exact → one hit; second 4F → AX=18.
4. `*.COM` matches only `.COM` entries.
5. `NOPE.$$$` → CF=1, AX=18 (or 2).
6. Default DTA at PSP:80h works if 1A never called.
7. Does not break AH=3D/3E/3F open/read of same names.

---

## 10. Touch list

| File | Change |
|------|--------|
| `kernel/kernel.asm` | AH=1A/4E/4F dispatch; dta_*; find_*; fcb wildcard match; DTA fill |
| `docs/int21.md` | Document 1A/4E/4F |
| `docs/flopfs-spec.md` | Optional: note find API |
| `programs/` | Optional tiny DIR.COM smoke |
| `tests/smoke_qemu.sh` | Optional: run DIR after boot |

---

*M9 brief — FlopFS root find only. Align with RBIL INT 21h AH=1A/4E/4F semantics; implement the subset above.*
