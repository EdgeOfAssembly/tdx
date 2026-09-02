# FloppyOS M9 — Tiny Shell Integration Plan

**Date:** 2026-07-26  
**Status:** plan only (no code)  
**Source milestone:** M8 (MCB + AH=48/49/4A + HELLO.COM as init)  
**Target:** M9 — `COMMAND.COM` as init shell; `HELLO.COM` as packed extra; headless QEMU smoke

**Evidence base (read):**
- `/tmp/RetroCodeMess/FloppyOS/tools/mkflopfs.c` — `-c` / `-f` / `init_name`
- `/tmp/RetroCodeMess/FloppyOS/Makefile` — image recipe, kernel pad 8192
- `/tmp/RetroCodeMess/FloppyOS/programs/hello.asm` — current init COM
- `/tmp/RetroCodeMess/FloppyOS/docs/flopfs-spec.md` — SB + root + init
- `/tmp/RetroCodeMess/FloppyOS/kernel/kernel.asm` — INT 21h, `run_init_com`, pad
- `/tmp/RetroCodeMess/FloppyOS/tests/smoke_qemu.sh` — current needles

---

## 1. How to pack COMMAND.COM as init and HELLO.COM as extra `-f`

### What `mkflopfs` already does (no tool change required)

| Flag | Behavior |
|------|----------|
| **`-c COM`** | First file in root; basename → FCB; sets **`sb.com_lba` / `com_sectors`** and **`sb.init_name`** from that FCB (`mkflopfs.c` ~286–294, 415–420) |
| **`-f FILE`** | Additional root entries only (after `-c`); basename → 8.3 FCB; **does not** change init |
| Order | Files laid out contiguously after kernel; root sector last |

Kernel boot path (`run_init_com`):
1. `fs_init` copies `sb.init_name` → `init_fcb`
2. Open path built from FCB (e.g. `COMMAND.COM`) via AH=3D
3. Read into PSP `com_load_seg` (default **0x2000**):0100
4. Far jump; on open fail → LBA fallback using `com_lba` (still the `-c` file)

### Makefile image recipe change

**Today** (`Makefile` ~47–48):

```make
$(BUILD_DIR)/mkflopfs -i $@ -s $(BUILD_DIR)/stage1_5.bin \
	-k $(BUILD_DIR)/kernel.bin -c $(BUILD_DIR)/hello.com
```

**M9 target:**

```make
$(BUILD_DIR)/command.com: shell/command.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<

# image deps: add command.com; keep hello.com
$(BUILD_DIR)/mkflopfs -i $@ -s $(BUILD_DIR)/stage1_5.bin \
	-k $(BUILD_DIR)/kernel.bin \
	-c $(BUILD_DIR)/command.com \
	-f $(BUILD_DIR)/hello.com
```

**Resulting root (expected):**

| Index | FCB name | Role |
|------:|----------|------|
| 0 | `COMMAND COM` | init + COM cache |
| 1 | `HELLO   COM` | extra file only |

**Host check after pack:**

```text
mkflopfs: … init=COMMAND.COM
  COMMAND.COM  LBA …  N bytes
  HELLO.COM    LBA …  M bytes
```

**Constraints (already enforced):**
- 8.3 only; basename uppercased (`path_to_fcb`)
- Max **8** files total (`MAX_FILES`); root **16** entries / 1 sector
- Each file ≤ 64 sectors (32 KiB)
- Image still 1 474 560 bytes

**Do not** pass `-c hello.com -f command.com` — that would make HELLO the init again.

---

## 2. Shell non-interactive script for headless QEMU smoke

### Why non-interactive

`tests/smoke_qemu.sh` runs:

```bash
timeout 12s qemu-system-i386 … -display none -serial stdio …
```

No keyboard, no AH=0A yet, no batch interpreter. A prompt that waits for input will hang until timeout (smoke may still PASS on partial needles if they appear before hang — **do not rely on that**).

### M9 policy: built-in smoke sequence (no stdin)

`COMMAND.COM` on entry runs a **fixed script**, then exits via AH=4C (kernel still **halts** — acceptable for M9):

```text
1. Banner          AH=09  "FloppyOS COMMAND" CR LF
2. DIR             list root entries (see §2.1)
3. TYPE HELLO.COM  AH=3D open → AH=3F read chunks → AH=02/09 print → AH=3E close
4. SHELL OK        AH=09  "SHELL OK" CR LF
5. Exit            AX=4C00 INT 21h
```

Optional later: if PSP cmdline (`[80h]` length / `[81h]` text) non-empty, parse one line; M9 smoke uses **empty** cmdline (`clear_psp` zeros it) → always run built-in script.

### 2.1 DIR without AH=4E/4F (recommended M9.0)

Kernel has **no** FindFirst/FindNext. Two options:

| Option | Kernel change | DIR quality |
|--------|---------------|-------------|
| **A. Probe list** | none | Open fixed names `COMMAND.COM`, `HELLO.COM` (and any other known); print name + size from successful opens / reads |
| **B. Minimal AH=4E/4F** | yes (~100–250 B + DTA state) | Real root walk via existing `root_buf` |

**Recommend A for M9.0** (shell-only, zero kernel risk):  
DIR prints at least:

```text
COMMAND.COM
HELLO.COM
```

(with sizes if cheap: open + use handle metadata is not exposed; size can be omitted or obtained by reading to EOF and counting — overkill; **name-only DIR is enough for smoke**).

**M9.1 (optional):** AH=4E/4F walking `root_buf` FCB names → 8.3 print; still no wildcards required if DS:DX = `*.*` treated as “all”.

### 2.2 TYPE HELLO.COM

Uses existing file API only:

```asm
; open "HELLO.COM", read 256-byte chunks to local buf, emit via AH=02
; stop on AX=0 or CF; close
```

**Note:** HELLO.COM is a **binary** COM. TYPE will dump machine code **and** embedded `$` strings. Serial log will likely contain the substring `Hello COM` from the file image — useful optional needle, **not** the same as executing HELLO (no AH=4B EXEC).

Do **not** expect `VER 7.10` / `VEC21 OK` / `MEM OK` unless shell **runs** HELLO (out of M9 scope without EXEC or a shell-local load+jump).

### 2.3 No interactive loop in M9 smoke build

Skip `A>` prompt loop until a later milestone with AH=0A (buffered input) or QEMU sendkeys. Keep a single code path: script → exit.

---

## 3. Whether kernel needs >8192 bytes

### Short answer

| Work item | Needs kernel >8192? |
|-----------|---------------------|
| COMMAND.COM as separate COM + Makefile `-c`/`-f` | **No** |
| Non-interactive DIR (probe) + TYPE + SHELL OK | **No** |
| Minimal AH=4E/4F FindFirst/Next | **Maybe** — measure free pad first |
| AH=4B EXEC / return-to-parent 4C | **Likely yes** later (not M9.0) |
| AH=0A line input for interactive shell | **Maybe** later |

### Current pad contract

```asm
; kernel/kernel.asm last line
times 8192 - ($ - $$) db 0
```

```make
@test $$(wc -c < $@) -eq 8192
```

`mkflopfs` already sizes `kernel_sectors` from file length (max 64 sectors / 32 KiB). stage1.5 loads `k_secs` from SB — **no boot change** if image stays ≤ gap below COM at 0x2000.

### Memory map headroom

| Region | Seg | Notes |
|--------|-----|-------|
| Kernel image | 0x1000 | 8 KiB → ends phys 0x12000 |
| COM PSP | 0x2000 | fixed default |
| Gap kernel→COM | ~56 KiB | room to grow kernel **on disk/in RAM** before colliding with 0x2000 |
| MCB arena | 0x3000–0xA000 | M8 |

Growing kernel image past **0x2000** would collide with fixed COM load — **not** an issue for a modest bump to 12–16 KiB later.

### Pre-implement measurement (mandatory)

```bash
cd /tmp/RetroCodeMess/FloppyOS && make kernel
# free pad ≈ 8192 - (offset of first trailing zero run)
# or: nasm list file / wc of non-pad portion
python3 -c "
b=open('build/kernel.bin','rb').read()
i=len(b)
while i and b[i-1]==0: i-=1
print('used', i, 'free_pad', len(b)-i)
"
```

- If free pad **≥ ~200 B** and M9.0 is shell-only → **keep 8192**.
- If adding 4E/4F + DTA and pad overflows → either shrink messages or bump to **12288/16384** in one FIXUP (`times N`, Makefile test, docs). Prefer **16384** only if 12 KiB is tight.

**M9.0 default: kernel stays 8192; no new INT 21h.**

---

## 4. Smoke needles list

### Keep (boot + kernel + load path)

| Needle | Source |
|--------|--------|
| `FloppyOS OK` | boot sector |
| `FloppyOS stage1.5` | stage1.5 |
| `FlopFS superblock OK` | stage1.5 |
| `FloppyOS kernel` | kernel banner |
| `INT21 OK` | kernel |
| `loading COM...` | `run_init_com` |
| `COM by name OK` | init open by `init_name` |

### Add (M9 shell)

| Needle | Source |
|--------|--------|
| `FloppyOS COMMAND` | shell banner (exact string TBD; keep short) |
| `COMMAND.COM` | DIR line |
| `HELLO.COM` | DIR line |
| `SHELL OK` | end of built-in script |

### Optional (TYPE dump of binary HELLO)

| Needle | Notes |
|--------|-------|
| `Hello COM` | appears inside HELLO.COM image when TYPEd; fragile if hello.asm strings change |

### Remove or demote from default smoke (no longer auto-exec HELLO)

| Needle | Why |
|--------|-----|
| `VER 7.10` | only if HELLO runs |
| `VEC21 OK` | only if HELLO runs |
| `MEM OK` | only if HELLO runs |

**Suggested `tests/smoke_qemu.sh` needle block (M9):**

```bash
for needle in \
  "FloppyOS OK" "FloppyOS stage1.5" "FlopFS superblock OK" \
  "FloppyOS kernel" "INT21 OK" "loading COM..." \
  "COM by name OK" \
  "FloppyOS COMMAND" \
  "COMMAND.COM" "HELLO.COM" \
  "SHELL OK"
do
  …
done
echo "smoke_qemu: PASS — M9 shell chain OK (rc=$rc)"
```

**Secondary test (optional M9.1):** separate image or flag that still runs HELLO as `-c` for M8 regression — not required if CI is single-image.

---

## 5. Minimal COMMAND.COM structure (`org 100h`)

**Path:** `/tmp/RetroCodeMess/FloppyOS/shell/command.asm`  
**Build:** `nasm -f bin -o build/command.com shell/command.asm`  
**Load:** kernel PSP 0x2000, entry 0x0100 (same as HELLO)

```asm
; command.asm — M9 tiny non-interactive shell
        bits 16
        org  0x100

start:
        mov     dx, msg_banner
        mov     ah, 0x09
        int     0x21

        call    cmd_dir
        call    cmd_type_hello

        mov     dx, msg_shell_ok
        mov     ah, 0x09
        int     0x21

        mov     ax, 0x4C00
        int     0x21

; --- DIR: probe known root names (M9.0) ---
cmd_dir:
        mov     dx, name_command
        call    dir_one
        mov     dx, name_hello
        call    dir_one
        ret

; DS:DX -> ASCIIZ path; if AH=3D succeeds, print name + CRLF, close
dir_one:
        push    dx
        mov     ax, 0x3D00
        int     0x21
        pop     dx
        jc      .no
        push    ax                      ; handle
        ; print path as $-string variant or char loop
        call    print_asz_crlf
        pop     bx
        mov     ah, 0x3E
        int     0x21
.no:    ret

; --- TYPE HELLO.COM ---
cmd_type_hello:
        mov     dx, name_hello
        mov     ax, 0x3D00
        int     0x21
        jc      .fail
        mov     [handle], ax
.rd:    mov     bx, [handle]
        mov     dx, buf
        mov     cx, 256
        mov     ah, 0x3F
        int     0x21
        jc      .cl
        test    ax, ax
        jz      .cl
        mov     cx, ax
        mov     si, buf
.outc:  lodsb
        mov     dl, al
        mov     ah, 0x02
        int     0x21
        loop    .outc
        jmp     .rd
.cl:    mov     bx, [handle]
        mov     ah, 0x3E
        int     0x21
.fail:  ret

name_command: db "COMMAND.COM", 0
name_hello:   db "HELLO.COM", 0
msg_banner:   db "FloppyOS COMMAND", 13, 10, "$"
msg_shell_ok: db "SHELL OK", 13, 10, "$"
handle:       dw 0
buf:          times 256 db 0

; print_asz_crlf: implement with AH=02 loop until 0, then CR LF
```

### Design notes

- **No** FreeCOM / MS-DOS CMD code (GPL / size); greenfield NASM COM only.
- **No** AH=4B; shell does not spawn HELLO — only TYPE.
- Reuse patterns from `programs/hello.asm` (AH=09/02/3D/3F/3E/4C).
- Keep binary **tiny** (target ≪ 2 KiB for M9); room later for prompt + parser.
- `shell/` directory already exists (empty) — natural home.

### PSP / memory

- Runs at fixed 0x2000 like current HELLO.
- MCB arena @0x3000 remains free; shell need not call AH=48 for M9.0.
- AH=4C still **halts** kernel (`i4c`) — shell does not “return to DOS”; fine for smoke.

---

## Out of scope for M9.0 (explicit)

- Interactive `A>` prompt / AH=0A
- Batch / AUTOEXEC.BAT
- AH=4B EXEC, AH=4D return code, parent return on 4C
- AH=4E/4F wildcards (optional M9.1)
- FreeCOM port or MS-DOS 4 CMD sources
- Kernel growth unless pad measurement forces it
- Changing FlopFS on-disk version (still v0.4)

---

## Docs / smoke touch list

| File | Change |
|------|--------|
| `shell/command.asm` | **new** |
| `Makefile` | build `command.com`; `-c command.com -f hello.com` |
| `tests/smoke_qemu.sh` | M9 needles (§4) |
| `README.md` | milestone M9; serial chain blurb |
| `docs/int21.md` | only if 4E/4F added |
| `docs/flopfs-spec.md` | optional note: init often `COMMAND.COM` |
| `TODO.md` | check M9 shell box |

---

## Implementation checklist

1. **Measure** current kernel used/free pad under 8192 (confirm M9.0 needs no grow).
2. **Add** `shell/command.asm` — banner → DIR probe → TYPE HELLO.COM → `SHELL OK` → AH=4C.
3. **Makefile**
   - `$(BUILD_DIR)/command.com` from `shell/command.asm`
   - `programs` / `image` deps include `command.com`
   - `mkflopfs … -c $(BUILD_DIR)/command.com -f $(BUILD_DIR)/hello.com`
4. **Keep** `programs/hello.asm` as packed demo file (not init).
5. **Update** `tests/smoke_qemu.sh` needles (§4); drop VER/VEC/MEM unless EXEC added.
6. **`make clean && make smoke`** — require exit 0 + all needles; capture serial log path on fail.
7. **`make size`** — record command.com / hello.com / kernel.bin bytes.
8. **Sanity:** `mkflopfs` stdout shows `init=COMMAND.COM` and both files listed.
9. **Docs:** README milestone → M9; optional one-liner in flopfs-spec init section.
10. **Do not** pull FreeCOM sources; do not implement interactive prompt in M9.0.
11. **Defer** AH=4E/4F / AH=4B / kernel pad bump to M9.1+ if needed.
12. **Git** (when implementing): checkpoint → FEATURE v1 M9 tiny shell; evidence = smoke command + exit code.

---

*Plan artifact: `/tmp/grok-1000/floppyos-m9-shell-plan.md`*
