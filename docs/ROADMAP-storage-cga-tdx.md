# TDX / iron86 / tdxview — implementation roadmap

**Date:** 2026-09-10  
**Tree:** `/mnt/TurboDebugger`  
**Constraint:** docs first, then easiest → hardest. No 286, no BIOS clone, no `--no-bios` expansion, no FloppyOS AH=3C/40 except FDC-write proof, no Game-MD5, no MDA blink / Hercules / cassette BASIC.

Live facts (verified in source, not the 2026-09-10 reviews which lag in places):

| Claim | Live |
|-------|------|
| iron86 version | `0.22` (`subprojects/iron86/src/main.cpp`, `man/iron86.1`, `make test` greps it) |
| tdx / tdxview / tdxctl | `0.27` (`include/tdx/tdx_version.h`, `scripts/tdxctl.py`); **`man/tdxview.1` still `.TH … 0.25`** |
| FORMAT C: as letter C: + copy+reboot | iron86 `TODO.md` `[x]`; **`docs/TODO.md` still `[ ]` for “as letter C:”** |
| Cold POST to A> | iron86 `TODO.md` `[x]` (PB3 SW1 mux, ROM WP); BIOS policy in `docs/TODO.md` already describes it |
| FDC Write 05h/09h | **ST0=`0x80` invalid** (`pc.cpp` ~2085), not silent-OK (review Issue 6 is stale) |
| tdxview F9 | already sends `unpause` (`tdx_view.cpp:360`); agent `run` still **toggles** |
| Headless `unpause` | already drives CPU (`tdx_main.cpp:202–219`) |
| HDC `dma_from_memory` | already clamps to `curr_count+1` (`hdc.cpp:693`) |
| `docs/CLI.md` | tdx `--hdc` present; iron86 version still **0.21**; packer still 32/360–720 |
| 8237 | `dma_tick` ch0 refresh-addr for POST ECA9; FDC/HDC transfers are **instant in the command handler** |

Explore dump `/tmp/grok-tdx-explore-storage-cga-tdx.md` was **missing**; this plan is from live `rg`/`read`.

---

## 1. 8237 HOLD/HLDA / DRAM refresh — **SKIP**

**Decision: do not implement cycle-exact 8237 HOLD/HLDA or DRAM refresh wait-states in this slice.**

IBM PC DMA on this backend is already a **functional subset**: POST wrap-test registers, ch0 `dma_tick` so 5160 ECA9 sees `IN 00h` move (BIOS WAITF / refresh programming `mode 0x58`, mask `0x0E`), and FDC ch2 / Xebec ch3 copies that complete inside the port-write handler before the next guest insn. That is *faster* than real hardware (no sector-time, no HOLDA freeze), which is what M1TP, Bushido, Dragon Wars, and DOS 3.31 FORMAT/COPY already run on.

Cycle-exact HOLD would only matter if a title (1) started a transfer and then mutated the buffer expecting the old bytes to hit media, (2) depended on the CPU being frozen for N microseconds, or (3) needed refresh wait-states for cycle-accurate CGA snow. None of the play titles do that: they wait on IRQ6/IRQ5 or INT 13 completion, and FDC Write in this slice should copy the same instant way as Read (`fdc_dma_to_mem`). Keep `dma_tick` ch0. Revisit HOLD only if a *named* game desyncs after Write/Format land.

---

## 2. Ordered work list

### W1 — Docs first: TODOs / CLI / man match iron86 0.22

- **Size:** S  
- **Depends:** none  
- **Git:** `FEATURE v1 Docs match iron86 0.22`  
- **Checkpoint:** `checkpoint/pre-docs-022`

**Files:**

- `docs/TODO.md` — check **FORMAT C: as letter C:**; keep FDC write / dir pack / 3D9 / 40-col / tdxview items open; BIOS policy already states cold POST + PB3 + ROM WP — do not list those as next work; drop “Suggested order” items 1–2 that are done.
- `docs/CLI.md` — iron86 **0.22** (not 0.21); HDD paragraph already honest; packer paragraph stays 32/360–720 until W10; tdxview man version note.
- `man/tdxview.1` — `.TH` **tdx 0.27** (source is `TDX_VERSION_STRING`).
- `docs/tdx.1` — leftover mdoc with `--no-game` (flag does not exist). Either delete or point to `man/tdx.1`.
- `docs/AGENT_PROTOCOL.md` — after W2, `run` is start-only (can wait until W2 commit, or say “prefer unpause” now).
- `subprojects/iron86/TODO.md` vs `docs/TODO.md` — one source of truth for storage/video checkboxes (repo `docs/TODO.md` is the product list).
- `man/tdx.1` / `man/iron86.1` — skim for leftover prefetch-4 (tdx OPTIONS already says 6; iron86 man already 0.22).

**Acceptance:**

- `make test` (repo) — existing `mandoc -T lint` + `tdxctl -v | grep 0.27`.
- `make -C subprojects/iron86 test` — already greps `iron86 0.22`.
- Human: `docs/CLI.md` version line and HDD/cold-POST text match `iron86 -v` / iron86 TODO `[x]` items.
- No behavior change.

**Risks:** over-checking items that are play-proven but not in `make test` (FORMAT C: as C:). Keep the play citation (`hdc-type1-fmt.img`, shot path) in TODO text; do not invent a new harness in W1.

---

### W2 — `tdxctl run` is start-only (not F9 toggle)

- **Size:** S  
- **Depends:** W1 (docs can mention the new contract in the same series or W2 FIXUP)  
- **Git:** `FEATURE v1 tdxctl run starts F9`  
- **Checkpoint:** `checkpoint/pre-run-startonly`

**Files:**

- `src/rex/rex_sock.cpp` — `"run"` / `"F9"` from **agent** → `REX_UI_START_RUN` (same as `unpause`). CPU-window F9 key in `tdx_ui.cpp` may keep **toggle** (Turbo Debugger F9).
- `scripts/tdxctl.py` — help: `run` starts; `pause`/`stop` pause; `unpause`/`cont` start. Do not say toggle.
- `docs/AGENT_PROTOCOL.md`, `docs/CLI.md`, `man/tdxctl.1`
- `tests/test_bp.cpp` or new `tests/test_agent_run.cpp` — headless session: `running=0` → cmd `run` → `rex_session_running==1`; second `run` stays 1; `pause` → 0.

**Acceptance:**

- `make test` — new Catch2 case on `rex_session` UI cmds (no SDL).
- `tdxctl -h` does not say “toggle” for `run`.
- `getip` already has `running` + `stop` (`stop=1` is `REX_STOP_STEP`, not pause). Add one line to tdxctl help if missing; optional test that JSON has both keys.

**Risks:** anyone scripting `run` as pause-if-live will break. That is the requested contract. CPU F9 toggle stays for humans.

---

### W3 — Window title after AH=4Ch

- **Size:** S  
- **Depends:** W2 optional  
- **Git:** `FEATURE v1 Title COMMAND.COM after AH=4C`  
- **Checkpoint:** `checkpoint/pre-ah4c-title`

**Files:**

- `src/dos/dos_machine.cpp` — on iron86 `CD 21` with AH=4Ch (and INT 20), set `exec_name` to `"COMMAND.COM"` if a shell parent is plausible, else `clear()`. Today only AH=4Bh calls `note_exec_from_dsdx()` (~1393).
- `src/dos/dos_int.cpp` — Unicorn INT 21 AH=4Ch still **halts** the EXE session (correct for `tdx file.exe`). Do **not** halt iron86 `--bios` (guest FloppyOS `i4c` already returns to COMMAND — `kernel.asm:917`).
- `src/tdx/tdx_ui.cpp` — retitle when `exec_name` changes (already does).
- `src/rex/rex_sock.cpp` — `guest` JSON already from `rex_session_guest`.
- `tests/test_step.cpp` or new fixture: Unicorn tiny COM INT 21/4C → title empty or not stuck; iron86 path: mock 4Bh then 4Ch → `COMMAND.COM`.

**Acceptance:**

- `make test` — Catch2: after 4Ch, `rex_session_guest` is `COMMAND.COM` or empty, not the child basename.
- Manual: FloppyOS `HELLO.COM` then exit → CPU + tdxview titles show `COMMAND.COM`.

**Risks:** DOS 3.31 on `--bios` also uses AH=4C; restoring `COMMAND.COM` is correct. Do not treat BIOS-path 4Ch as `REX_STOP_HALTED`.

---

### W4 — tdxview `SDL_TEXTINPUT` (FI `;` vs US XT `:`)

- **Size:** S  
- **Depends:** none (can parallel W2/W3)  
- **Git:** `FEATURE v1 tdxview SDL_TEXTINPUT`  
- **Checkpoint:** `checkpoint/pre-textinput`

**Files:**

- `src/tdx/tdx_view.cpp` — `SDL_StartTextInput()`; printable path from `SDL_TEXTINPUT` (UTF-8 Latin-1 / CP437-safe bytes) → existing `{"cmd":"key","key":"…"}`. `SDL_KEYDOWN` only for F-keys, arrows, Enter, Esc, Backspace, Tab. Avoid double-inject (do not also send KEYDOWN for 32–126).
- `src/tdx/tdx_ui.cpp` — same for CPU window type-into-guest if it injects keycodes today (~785).
- `tests/` — unit-test the mapper if extracted (`tdx_sdl_text_to_xt`); otherwise a small helper + Catch2: text `:` → key `:`, text `;` → `;`. No SDL in Catch2.

**Acceptance:**

- `make test` — mapper cases including `:`, `;`, `B`, Shift letters (TEXTINPUT already gives `B` vs `b`).
- Interactive: Finnish layout type `B:` in tdxview → guest sees `B:` (not `B;`). Proof via `tdxctl --view` is optional; comment the FI case in TODO checkbox.

**Risks:** dead keys / compose; ignore. Alt-X must stay KEYDOWN. Do not send TEXTINPUT for Ctrl/Alt combos.

---

### W5 — Unique sockets per tdx/tdxview pair

- **Size:** M  
- **Depends:** W2 (agent scripts)  
- **Git:** `FEATURE v1 Unique tdx sockets per PID`  
- **Checkpoint:** `checkpoint/pre-unique-socks`

**Files:**

- `src/tdx/tdx_cli.cpp` / `include/tdx/tdx_cli.h` — default `--sock` = `/tmp/tdx.<pid>.sock` when unset.
- `src/tdx/tdx_view.cpp` — default `--listen` = `/tmp/tdxview.<pid>.sock`; default `--sock` still `/tmp/tdx.sock` **or** `TDX_SOCK` env (tdx prints/export).
- `src/rex/rex_sock.cpp` — do **not** `unlink` a live socket (refuse bind if connect() succeeds); unlink own path if `listen` fails.
- `scripts/tdx-start.sh` — start pair with matching socks; print `TDX_SOCK=… TDXVIEW_SOCK=…`; optional compatibility **symlink** `/tmp/tdx.sock` → this tdx sock **only if** the name is free (one-pair agents keep working).
- `scripts/tdx-kill.sh` — glob `/tmp/tdx.*.sock` `/tmp/tdxview.*.sock` plus the two classic names.
- `scripts/tdxctl.py` — `--sock` default: `$TDX_SOCK` else `/tmp/tdx.sock`.
- `docs/CLI.md`, `docs/AGENT_PROTOCOL.md`, `man/tdx.1`, `man/tdxview.1`, `AGENTS.md` (“one pair” paragraph).

**Acceptance:**

- `make test` — CLI parse default contains PID digits; existing `/tmp/tdx-test.sock` tests unchanged (explicit `--sock`).
- Two `tdx --no-ui` without `--sock` do not steal each other (second bind fails or uses another PID path).
- `tdx-start.sh -h` documents env.

**Risks:** breaking muscle-memory `/tmp/tdx.sock`. Mitigate with symlink-if-free + `TDX_SOCK`. Never unlink another process’s inode.

---

### W6 — Full CGA 3D9 palette (iron86 latch + tdxview)

- **Size:** M  
- **Depends:** W1  
- **Git:** `FEATURE v1 CGA 3D9 palette`  
- **Checkpoint:** `checkpoint/pre-cga-3d9`

**Must:** background nibble, pal0 (green/red/brown) vs pal1 (cyan/magenta/white), intensity bit4, mode-6 foreground nibble. Burst stays **3D8 bit2** (already composite).

**Files:**

- `subprojects/iron86/src/pc.cpp` — `OUT 3D9` currently falls through (~2370). Latch on `mc6845` or `pc` (`cga_3d9_`), mirror to BDA `0040:0066` (Unicorn already does this in `dos_machine.cpp:281`).
- `subprojects/iron86/include/iron86/crtc.h` or `pc.h` — field + Doxygen.
- `include/dos/dos_cga.h` — `dos_cga_palette_argb` already maps pal0/pal1; **0x30 special-case** (bright cyan/magenta) is Bushido title — keep, document. Add pal0 `0x10` goldens in `tests/test_cga.cpp`.
- `src/tdx/tdx_view.cpp` — RGBI gfx already uses `palreg`; **mode 6 RGBI** is hard-coded white/black (~880) — use 3D9 fg nibble. Composite paths already pass `palreg`.
- `src/tdx/tdx_ui.cpp` — in-process `--game` palette (already `dos_cga_palette_argb`).
- `tests/fixtures/out3d9.asm` + `tests/test_bios.cpp` — Unicorn OUT 3D9 → BDA. Add iron86 `test_pc.cpp` OUT 3D9 → `mem_read8(0x466)`.
- `docs/TODO.md` — check 3D9 boxes (iron86 + tdxview).

**Acceptance:**

- `make test` + `make -C subprojects/iron86 test`.
- Catch2: 3D9 `0x10` pal0 vs `0x30` pal1 vs `0x00` black bg; iron86 BDA `40:66`.
- Bushido courtyard RGBI: cyan/magenta (pal1) vs red/green (pal0) when the game switches (INT 10 AH=0Bh already on Unicorn; `--bios` ROM writes BDA + now hardware latch).

**Risks:** BIOS `--bios` already writes BDA via ROS; latching 3D9 is for games that `OUT DX,AL` without BIOS. Do not expand C++ INT 10 (`--no-bios`).

---

### W7 — CGA 40-column modes 0 / 1

- **Size:** M  
- **Depends:** W6 (mode-set / 3D8 burst)  
- **Git:** `FEATURE v1 CGA 40-column modes 0/1`  
- **Checkpoint:** `checkpoint/pre-cga-40col`

**Files:**

- `src/tdx/tdx_view.cpp` — text path always 80×25 at 640×400 (~916). If BDA mode is 0/1 **or** `video_cols==40`, draw **40** cells, glyph **16×16** (8×8 doubled) so the window stays 640×400. Mode 0 = burst off (3D8 bit2); RGBI may still show 16 attrs; composite uses existing 3D8.
- `src/rex/rex_sock.cpp` — optional `cols` in `cga` JSON from `40:4A`.
- `src/dos/dos_int.cpp` — Unicorn INT 10 AH=00 already sets `video_cols` 40 for modes 0/1/4/5. `blank_regen` still fills **80×25** — fill 40×25 for modes 0/1 (not `--no-bios` expansion; Unicorn EXE path).
- `subprojects/iron86/src/pc.cpp` — `--no-bios` tty is 80-col; **do not grow** that clone. `--bios` ROS programs CRTC R1=40; tdxview must honor BDA.
- New fixture `tests/fixtures/int10m0.asm` (AH=00 AL=01, write `A` at 0,0) + Catch2: B800 layout is 40-wide; `cga` JSON mode 1.
- `docs/TODO.md` video table: modes 0/1 **ok**.

**Acceptance:**

- `make test` — mode 1 fixture; tdxview logic unit-testable via cols in JSON if extracted.
- Manual: `INT 10 AX=0001` COM under Unicorn `--no-ui` dump B800 stride 40.

**Risks:** games that set mode 1 then write 80-wide anyway; follow BDA `40:4A`. Do not implement snow/wait-states.

---

### W8 — FDC Write Data DMA-from-mem + persist A:/B:

- **Size:** M  
- **Depends:** none (storage; after docs). Instant DMA (see §1).  
- **Git:** `FEATURE v1 FDC Write Data persist`  
- **Checkpoint:** `checkpoint/pre-fdc-write`

**Files:**

- `subprojects/iron86/include/iron86/pc.h` / `src/pc.cpp`
  - `fdc_dma_from_mem(uint8_t *dst, size_t n)` — mirror `fdc_dma_to_mem` (ch2 page `81h`, `curr_addr[2]`, clamp `curr_count+1`).
  - `fdc_exec` cmd **05h** (Write Data) and **09h** (Write Deleted): CHS loop like Read 06h; write into `floppy_[drv]`; ST0/ST1/ST2 + 7-byte result; IRQ6.
  - Keep host path per unit (`floppy_path_[2]`); `persist_floppy(unit)` `fwrite`/`fflush` after successful write **if** the mount was a regular file. Packed dirs: RAM-only (or document); do not overwrite a host directory.
  - INT 13 **AH=03** already copies to `floppy_[]` (~952) — **also persist** so reboot keeps COPY/saves.
- `subprojects/iron86/tests/test_pc.cpp` — program 8237 ch2 + FDC 05h; guest buffer → image byte; tmp file survives `attach_floppy_path` again.
- Optional COM fixture under tdx `--no-bios` or iron86 without ROM: INT 13 AH=03.
- **FDC write proof without FloppyOS AH=40:** DOS 3.31 `COPY` on A: (INT 13) **or** the unit test. Do not implement AH=3C/40.
- `docs/TODO.md` / iron86 `TODO.md` — check Write Data.

**Acceptance:**

- `make -C subprojects/iron86 test` — new FDC write + persist cases.
- `make test` (repo) if tdx links iron86 (it does).
- Play optional: DOS `COPY A:FOO A:BAR` then restart same image.

**Risks:** Write Deleted (09h) vs data (05h) — treat 09h as write + ST2 DD bit if cheap, else same bytes. DMA count underrun → ST1 EN (0x80) not OK. Multi-sector EOT same as read.

---

### W9 — FDC Format / Scan / Read ID

- **Size:** M  
- **Depends:** W8  
- **Git:** `FEATURE v1 FDC Format Scan Read ID`  
- **Checkpoint:** `checkpoint/pre-fdc-format`

**NEC 765 (cmd & 0x1F):**

| Cmd | Name | Need | Behavior |
|-----|------|------|----------|
| 0Ah | Read ID | 2 | Result: ST0–ST2 + C/H/R/N of next sector at `cyl[drv]` (R=1 or last) |
| 0Dh | Format Track | 6 | N, SC, GPL, D; DMA ID fields (C,H,R,N)×SC; fill `D` (usually F6) for SC sectors; persist |
| 11h / 19h / 1D | Scan EQ / LO / HI | 9 | DMA compare vs media; ST2 SH/SN |

**Files:** `pc.cpp` `fdc_exec` / `fdc_write_cmd` (need lengths already listed). Tests in `test_pc.cpp`. Persist via W8 helper.

**Acceptance:**

- `make -C subprojects/iron86 test` — Format one track → bytes `D`; Read ID after Seek; Scan EQ match/mismatch ST2.
- Guest FORMAT A: is **optional** play (DOS 3.31); unit test is the gate.

**Risks:** IDAM layout in raw `.img` is sector-only (no gap bytes). Format = fill sector payloads; do not grow a track-with-gaps image format. Scan can be “good enough” (byte compare, no FM/MFM).

---

### W10 — Dir pack: smallest floppy that fits (recursive, 360K→1.44M)

- **Size:** L  
- **Depends:** none for packer; FloppyOS SPT for **guest** 1.2M/1.44M  
- **Git:** `FEATURE v1 FlopFS pack smallest-fit` (split 1/2 packer, 2/2 FloppyOS SPT/root if needed)  
- **Checkpoint:** `checkpoint/pre-flopfs-pack`

**Today:** `flopfs_pack.cpp` — non-recursive, `k_root_ents=32`, 360K else 720K, 800K fails (`test_pack.cpp:161`).

**Packer (`flopfs_pack.cpp` / `.h`):**

1. Recursive walk; skip hidden/non-8.3; **preserve subdirectories** as FlopFS dirents (`codec=0x10`, `lba` → table of 32-byte dirents, `size`/`sectors` for the table). Do not flatten (games with `DATA\FOO`).
2. Measure data + dir tables; pick smallest of 360K (720), 720K (1440), 1.2M (2400), 1.44M (2880). Fail if larger → caller uses HDD (W11).
3. Raise root cap: `root_sectors = ceil(nents/16)`; superblock already has `root_sectors`. Drop hardcoded 32.
4. Constants `FLOPFS_SECS_1200K` / `FLOPFS_SECS_1440K`.
5. `test_pack.cpp`: nested `SUB/NOPE.COM` **included**; 800K → 1.2M or 1.44M not fail; 33 files → root_sectors≥3.

**FloppyOS (needed for 1.2M/1.44M and >32 names — not AH=3C/40):**

- `kernel.asm` `SPT equ 9` / `disk_read` (~569) — **wrong** for 15/18 spt. After mount, INT 13 AH=08 or derive from superblock `sector_count` (same table as `floppy_geom`). Per-drive SPT/heads.
- `ROOT_ENTRIES equ 32` — honor `root_sectors` from SB.
- AH=3D path `SUB\FILE` if dir codec present (minimal; no CHDIR).

**CLI/docs:** `docs/CLI.md` packer paragraph; `tdx -h` / `iron86 -h` “360K→1.44M”; man pages.

**Acceptance:**

- `make -C subprojects/iron86 test` — pack tests above.
- FloppyOS: `make test` in `subprojects/floppyos` if that target exists; otherwise a packed 1.44M image `DIR` under tdx `--floppy-a` (play) plus kernel unit if any.
- Existing 360K HELLO pack still 368640 bytes.

**Risks:** FlopFS spec v0.4 has no dir flag — document `codec=0x10` in `flopfs-spec.md` without bumping magic until kernel writes it. Name collisions in one directory still fail. 2.88M out of scope.

---

### W11 — XT HDD C: auto-size from a host directory

- **Size:** L  
- **Depends:** W10 only for “if packer fails, use `--hdc DIR`” UX; can start in parallel  
- **Git:** `FEATURE v1 HDC pack host dir`  
- **Checkpoint:** `checkpoint/pre-hdc-pack`

**Intent:** a tree that does not fit 1.44M (or any floppy) becomes a Xebec image DOS 3.31 can `DIR C:`. That is **FAT12/16**, not FlopFS (C: play path is DOS 3.31).

**Files (new):**

- `subprojects/iron86/src/hdc_pack.cpp` + `include/iron86/hdc_pack.h`
  - Recursive 8.3 + subdirs (FAT directories).
  - MBR + one primary (type 01 FAT12 or 04/06 FAT16).
  - Geometry: Type-1 **306×4×17** (~10 MB) if it fits; else grow **cylinders** (max 1024) then **heads** (max 16), SPT 17 (Xebec opcode 0Ch caps).
  - Cluster size so FAT12 if clusters ≤ 4084, else FAT16.
  - Write image file (caller supplies path or tempfile next to dir).
- `pc::attach_hdc_path` — if `S_ISDIR`, pack then `hdc_.attach`. **Still refuse** `S_ISBLK`/`S_ISCHR`.
- `tdx` / `iron86` `--hdc DIR` — same as file.
- `tests/test_hdc.cpp` or `test_hdc_pack.cpp` — tiny tree → DIR-able BPB; `/dev/sda` still refused.
- INI `[hdc] image=` may stay a file; CLI dir is enough.
- `docs/CLI.md`, man, TODO checkbox.

**Acceptance:**

- `make -C subprojects/iron86 test`.
- Host dir ~1.5 MB+ → image ≥ tree; BPB `OEM` / FAT ID sane; first file bytes at computed cluster.
- Play optional: `tdx --preset ibm-xt --hdc /path/to/game --floppy-a DOS3.31.img` → `DIR C:`.

**Risks:** no in-tree FAT packer today (FloppyOS deferred FAT). Keep the writer small and tested; do not import FatFs. Volume boot vs partition offset (63 or 17) must match Xebec LBA 0 = first sector of image (IBM Type-1 often whole-disk FAT without MBR — **match what FORMAT C: produced** on `hdc-type1-fmt.img`). Inspect that image’s first 512 bytes before choosing MBR-vs-superfloppy. Prefer **whatever DIR C: already mounts**.

---

### W12 — VCR / symbols / Ghidra on `--bios` iron86

- **Size:** L  
- **Depends:** none; after agent UX is nicer  
- **Git:** `FEATURE v1 VCR symbols Ghidra iron86`  
- **Checkpoint:** `checkpoint/pre-bios-vcr`

**Today:** VCR records Unicorn `on_write` (`dos_machine.cpp:198`). iron86 `mem_write8` does not. `--ghidra` uses `cli.input` (empty on `--bios`). Symbols TSV load is linear — should work if loaded.

**Files:**

- `src/dos/dos_machine.cpp` — on iron86 `step_one`, if `vcr_rec`, snapshot writes (hook `cpu::mem_write8` callback or diff dirty pages). `vcr_back`/`forward` must `iron->c.mem_write8` / set iron regs, not only `ram[]`.
- `src/tdx/tdx_main.cpp` `maybe_ghidra` — `--bios FILE` → export that ROM (or FloppyOS kernel if `--floppy-a` and no MZ). Do not require an MZ.
- `src/rex/rex_session.cpp` — `rex_symbols_load` on iron86 linear (already). Test: load TSV, lookup F000:E199.
- `tests/test_step.cpp` — iron86 `--no-bios` tiny COM VCR back restores a poked byte (no ROM needed).

**Acceptance:**

- `make test` — VCR back on iron86 COM; symbols lookup; `--ghidra` without input does not crash (skip if Ghidra absent).
- Manual: `tdx --bios … --symbols map.tsv` shows names in CPU window.

**Risks:** VCR tape size on POST (millions of writes). Record only when VCR is armed (UI Down/Up), not the whole POST. Ghidra on 64K BIOS is slow — timeout OK.

---

### W13 — Unicorn FloppyOS keys (`--uc-floppy`)

- **Size:** M  
- **Depends:** W4 (typing) optional  
- **Git:** `FEATURE v1 uc-floppy guest INT 21`  
- **Checkpoint:** `checkpoint/pre-uc-floppy`

**Problem:** `--uc-floppy` loads the boot sector into Unicorn (`load_floppy_uc`) and **host `dos_int.cpp` steals INT 21**, so FloppyOS kernel never sees AH=01/07/08/0A. INT 13 image read exists (~1308). Keys via INT 16 may work; console INT 21 does not.

**Fix (bounded):** if `floppy_uc` (or `iron` is null and `floppy_img` is a FloppyOS boot), **do not intercept INT 21** — let the guest kernel run. Keep INT 13 (image) + INT 16 (`tdxctl type` / wait_key). INT 10 can stay host for CGA if FloppyOS uses BIOS TTY; FloppyOS often uses its own TTY — prefer passing INT 10 through when `--uc-floppy`.

**Files:** `src/dos/dos_int.cpp` (hook gate), `src/dos/dos_machine.cpp` `load_floppy_uc`, `tests/test_floppy.cpp`.

**Acceptance:**

- `make test` — `--uc-floppy` fixture: INT 21 AH=09 from guest COM on the image not handled as host `$` print if we skip intercept… hard. Minimum: INT 16 inject fills buffer; guest `AH=00` returns the key.
- Do not make Unicorn the Bushido path.

**Risks:** rabbit hole. Cap at INT 21 pass-through + INT 16. If FloppyOS boot on Unicorn still dies on FDC ports, document “use `--floppy-a` iron86” and keep the key path only.

---

## 3. Series grouping (commits)

Liberal checkpoints **before each FEATURE**. `v1` first delivery; FIXUP v2 if review nits.

| Series | Commits | W# |
|--------|---------|----|
| **A — docs** | `FEATURE 1/1 v1 Docs match iron86 0.22` | W1 |
| **B — tdx agent** | `FEATURE 1/4 v1 tdxctl run starts F9` | W2 |
| | `FEATURE 2/4 v1 Title COMMAND.COM after AH=4C` | W3 |
| | `FEATURE 3/4 v1 tdxview SDL_TEXTINPUT` | W4 |
| | `FEATURE 4/4 v1 Unique tdx sockets per PID` | W5 |
| **C — CGA** | `FEATURE 1/2 v1 CGA 3D9 palette` | W6 |
| | `FEATURE 2/2 v1 CGA 40-column modes 0/1` | W7 |
| **D — FDC** | `FEATURE 1/2 v1 FDC Write Data persist` | W8 |
| | `FEATURE 2/2 v1 FDC Format Scan Read ID` | W9 |
| **E — packers** | `FEATURE 1/2 v1 FlopFS pack smallest-fit` | W10 |
| | `FEATURE 2/2 v1 HDC pack host dir` | W11 |
| **F — debugger extras** | `FEATURE 1/2 v1 VCR symbols Ghidra iron86` | W12 |
| | `FEATURE 2/2 v1 uc-floppy guest INT 21` | W13 |

If W10 needs a FloppyOS kernel SPT commit, make it `FEATURE 1/3` packer / `2/3` kernel / `3/3` docs, still series E.

**Quality every FEATURE:** gcc C++23 Allman, Doxygen on new public APIs, `make test` (repo) and `make -C subprojects/iron86 test` when iron86 changes, cli-design (`-h`/`-v`, `--no-*` only for defaults-on). Formal: `make verify` (CBMC MZ) unchanged; extract CHS/persist helpers if you add pure functions — else `formal: not run` (I/O + DMA not CBMC-shaped). Checkpoint ref before each FEATURE. No drive-by refactors.

---

## 4. What NOT to do this slice

- 80286 / `ibm-at` / 5170 / Game-MD5 presets  
- Full C++ or Intel-asm BIOS clone; **`--no-bios` INT 10/13/16 growth**  
- Cycle-exact 8237 HOLD/HLDA / refresh wait-states / CGA snow  
- MDA blink, exact 720×350 crystal, Hercules 720×348, cassette BASIC ROM  
- FloppyOS AH=3C/40/41, FCB write, FlopFS v0.5 xxhash3/dedup (unless a one-line persist proof needs a write — prefer INT 13)  
- 2.88M floppy, FAT12 as FloppyOS primary FS  
- Dual MDA+CGA in one iron86 guest  
- Expanding Unicorn as the Bushido/DOS play path  
- Committing IBM BIOS / Xebec ROS / game binaries  
- Silent FDC write success (already fixed to ST0=80; keep it until W8)

---

## 5. First implementable W# after docs

**Start W2 immediately after W1 merges:** `tdxctl run` → `REX_UI_START_RUN`, tests, help/man. Isolated, no hardware, unblocks agents, matches the user “prefer unpause; run should not toggle” decision.

Parallel after W1 (no file overlap with W2): **W4** TEXTINPUT (`tdx_view.cpp` / `tdx_ui.cpp` only) and **W8** FDC write (`iron86/pc.cpp` only) if two worktrees.

Do **not** start W10/W11 until W8 persist exists if the same person is packing write tests onto images; packers themselves do not need Write.

---

## Appendix — current stubs (line anchors)

| Item | Where |
|------|--------|
| FDC write stub ST0=80 | `subprojects/iron86/src/pc.cpp` ~2085 |
| FDC read DMA-to-mem | same ~1804, ~2017 |
| INT 13 AH=03 RAM-only | same ~952 |
| OUT 3D9 ignored | same ~2353–2370 |
| `dma_tick` ch0 | same ~1677 |
| Pack 32 / 360–720 | `subprojects/iron86/src/flopfs_pack.cpp` |
| Agent `run` toggle | `src/rex/rex_sock.cpp` ~245 |
| Title 4Bh only | `src/dos/dos_machine.cpp` ~1393 |
| tdxview text 80-col | `src/tdx/tdx_view.cpp` ~916 |
| tdxview F9 unpause | `src/tdx/tdx_view.cpp` ~360 |
| FloppyOS SPT=9 | `subprojects/floppyos/kernel/kernel.asm:8` |
| FloppyOS i4c returns | same ~917 |
| Default socks | `tdx_cli.cpp` / `tdx_view.cpp` `/tmp/tdx.sock` |
