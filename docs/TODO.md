# TDX / iron86 / FloppyOS — what next

**Milestone (2026-09-04): CGA COMPOSITE WORKS** — tag `milestone/cga-composite-works`,
commit `a66b0aa` + docs. Dragon Wars on B: (720K FlopFS), FloppyOS `AH=42` LSEEK,
tdxview old-CGA NTSC (Reenigne/86Box). B800 dump **byte-identical** to DOSBox
`games/SCREEN.CGA`. How-to: `docs/HOWTO_DRAGON_WARS.md`. CLI: `docs/CLI.md` + `man/*.1`.

**Earlier:** 5150 BIOS POST; FloppyOS A>; B: dir-as-FlopFS; Bushido INT 21h 4Bh;
Unicorn MZ still default for `tdx file.exe`.

Do **not** commit IBM BIOS ROM or game binaries. Always `scripts/tdx-kill.sh` before a new tdx/tdxview pair.

---

## tdx / tdxview

**Working:** CPU window, tdxview 640×400 text / 320×200×scale gfx, UNIX sockets, `tdxctl`, BPINT 10 **AX=0004** only, BPM, titles `TDX <ver> [FILE] PID:n` (file empty until load or INT 21h 4Bh).

- [x] **Window title after AH=4Ch** — `exec_name` becomes `COMMAND.COM` on INT 21h 4Ch (Unicorn + iron86 CD 21).
- [x] **tdxview SDL_TEXTINPUT** — printable keys (including FI `:`) from text input; KEYDOWN keeps F-keys/arrows/Enter.
- [x] **`--preset ibm-pc`/`5150` vs `ibm-xt`/`5160`** (tdx 0.27); `--hdc` implies xt; prefetch default 6
- [x] **`--hdc` / `--hdc-rom` / `--no-fast-post`**
- [x] **`tdxctl run` vs unpause** — agent `run`/`F9` is start-only (same as `unpause`). CPU-window F9 still toggles. `getip` `stop=1` is last slice STEP, not paused.
- [x] **Unique sockets per pair** — if `/tmp/tdx.sock` (or tdxview listen) is live, fall back to `/tmp/tdx-<pid>.sock` and print the path.
- [x] **CGA 3D9 palette** — iron86 OUT 3D9 → BDA 40:66; tdxview `dos_cga_palette_argb` (cyan/magenta vs green/red). Modes **0/1** 40-col fat glyphs.
- [ ] **VCR / symbols / Ghidra** — keep working on Unicorn EXE; untested on `--bios` iron86.
- [ ] **Unicorn FloppyOS keys** — host INT 21 + INT 16 workaround; `--uc-floppy` is not the Bushido path. Don’t spend time unless someone wants Unicorn as DOS.

---

## iron86 (BIOS + chips)

**Working:** 8086 dispatch (enough for POST + Bushido), packed PPI/PIT/PIC/DMA/FDC, two 6845s (MDA `3Bx` + CGA `3Dx`), LPT stub, 360K A:+B:, dir→FlopFS pack, XT keys IRQ1, BDA `40:49` → tdxview mode, speaker `--no-audio`, DAA/WAIT/ESC NOPs.

### BIOS policy (2026-09-06)

**Default:** `tdx --floppy-a` → 5150 8K (`ibm-pc`) unless `--preset ibm-xt` / `5160` or `--hdc` (implies xt, 64K 10JAN86 + Xebec ROS). Fast-post is warm-boot (`40:72` + `BX=1234h`, jump `F000:E199`; 10JAN86 1234h compare is **E175**). `--no-fast-post` cold POST reaches 640 KB OK; 5160 SW1 mux is PB3 (iron86 0.22) so EQUIP_FLAG keeps IPL. Do **not** grow the C++ INT 10/13/16 clone as the main path.

- [x] **PIT/IRQ0 under tdx F9** — BIOS ch0 reload 0 strides in `pit_tick`; F9 slices keep stepping through INT 16 wait so ROM INT 8 advances `40:6C`. `pit_poll` pulses iron86 IRQ0 at 18.2 Hz wall. (TANK.COM INT 1A stall.)
- [ ] **Profile POST** — `make profile` (gprof `-pg`), then speed warm-boot without patching ROS (checksum ERR01). PIT/CGA status waits, tdx F9 slice.
- [ ] **`--no-bios` fallback** — keep working when the user has no ROM. Today: enough INT 10/13/16 for FloppyOS + SETUP. **Do not expand this until last.**
- [ ] **Full reimplemented BIOS (LAST)** — PC → XT → AT → 386/486/Pentium. For each machine, read the **official ROS** and reimplement hot paths in **Intel syntax** (`-masm=intel` / `.intel_syntax noprefix` inline asm), not a second C++ BIOS. Massive; after HDD/FDC/FlopFS.

Still intercept INT 13h on `--bios` (720K vs 40-track 5150 INT 13) and INT 16h for `tdxctl type` until PPI/IRQ1 + ROM INT 16 is proven.

### Fonts (complete for US IBM 5788005)

The card ROM `ROM/IBM_5788005_AM9264_1981_CGA_MDA_CARD.BIN` (MAME SHA1 `c2a8b108…`, **not pushed**) is the real IBM MDA+CGA character generator: 256×8×14 MDA + 256×8×8 CGA thick (thin bank unused). BIOS `CRT_CHAR_GEN` (`F000:FA6E`) is the 128-glyph 8×8 used for graphics TTY. tdxview uses 5788005 when present, else BIOS 8×8. **US CP437 fonts are 100%.** (European 4733197 Ø/ø ROM is a different chip; ignore unless a game needs it.)

### IBM video modes — implement when a game needs them

| Mode | Hardware | tdxview now |
|------|----------|-------------|
| 0 | CGA 40×25 B&W text | **ok** (fat 16×16 glyphs) |
| 1 | CGA 40×25 16-color text | **ok** (fat 16×16 glyphs) |
| 2 | CGA 80×25 B&W text | ok (80×25) |
| 3 | CGA 80×25 16-color text | ok (FloppyOS A>) |
| 4 | CGA 320×200 4-color | ok (Bushido) |
| 5 | CGA 320×200 4-color, burst off | treated as 4 |
| 6 | CGA 640×200 2-color | **ok** 1bpp + old-CGA composite in tdxview (Dragon Wars) |
| 7 | **MDA 80×25 text only** (no graphics) | ok (`--mda`, B000, green 5151) |

**MDA (IBM 5151)** has **only mode 7** (80×25 text, no bitmap). Green-on-black is the 5151 P39 phosphor.

**Hercules Graphics Card (HGC)** is **not** MDA. It is a later third-party card: MDA-compatible **text** (same `B000` / `3Bx`) **plus** a 720×348 2-color graphics page (two pages, extra bits on `3B8`). Games that “need Hercules” are not MDA titles.

Still missing — **case-by-case when a game needs it** (composite artifact already has a test title):

**CGA**
- [x] **3D9 palette** — OUT 3D9 + BDA 40:66; `dos_cga_palette_argb` (bg, cyan/magenta vs green/red, intensity).
- [x] **Mode 6** 640×200 1bpp (tdxview; not mode 4’s 2bpp).
- [x] **Modes 0 / 1** 40-column text (fat 16×16 glyphs in 640×400).
- [x] **Composite artifact color** — Reenigne/86Box old-CGA NTSC. Mode 4 320×200
  (Dragon Wars, 3D8=2Ah burst on) and mode 6 640×200. `--no-composite` = RGBI.
  Gold: `games/SCREEN.CGA` (not pushed).
- [ ] **Snow / wait-states** — CGA VRAM contention (cycle-exact 6845). Low priority unless that game shows it.

**MDA**
- [ ] **Blink** (attr bit 7, ~1.875 Hz).
- [ ] **Exact 720×350 timing** (9×14 cells; we draw 720×350 already, not crystal-accurate).

**Hercules (separate from MDA)**
- [ ] **720×348 graphics** at `B000`, page 0/1, `3B8` graphics enable. After a Herc game is in hand.

tdx (CPU) + tdxview (user screen) is the dual-head setup. A second **iron86** MDA+CGA pair in one guest is **last**, not needed for this debugger.

### WILLNOTFIX

- [ ] ~~CGA **light pen** (`3DC`/`3DB`, 6845 R16/R17)~~ **WILLNOTFIX** — we do not have a light pen.

### P1 — storage

- [x] **XT hard disk Xebec as C:** — `320h–323h`, DMA3, IRQ5, ROS `C800:0`
  (`IBM_XEBEC_5000059`). `tdx --preset ibm-xt --hdc FILE`. Guest FDISK +
  FORMAT C: as letter C: + **`DIR C:`** + copy+reboot persist + cold POST to A>
  (iron86 0.22 PB3 SW1 mux). Never `--hdc /dev/sd*`.
- [x] **FORMAT C: copy+reboot** — `ECHO HELLO>C:HI.TXT`, full tdx restart, `DIR C:`
  still lists `HI.TXT` (host image hash changed). Burst `tdxctl type` of a long
  line can stack-overflow; slow type worked.
- [x] Guest **FORMAT C:** from blank Type-1 as letter C: (`hdc-type1-fmt.img`,
  FAT12 `IBM  3.3`, `FORMAT.COM` 19683). Earlier FDISK-as-E: then DIP mux is obsolete.
- [ ] **HDD auto-size from a host dir** that does not fit any floppy (1.3MB tree → C:).
- [x] **FDC Write Data** DMA-from-mem (persist A:/B: when attached from a host file).
- [x] **FDC Format / Scan / Read ID**.
- [x] **720K geometry** — FlopFS packer: 360K if it fits, else 720K 80/2/9
  (Dragon Wars ~708K). Over 720K → fail today (use `--hdc`).

**Floppy auto-size (answers 1–2, 2026-09-04)** — raw CHS is done in iron86; dir pack still flat 32 names / 360–720 only.

- [x] **Raw image CHS at runtime** (iron86) — pick geometry from file size, all common
  IBM PC floppy formats, not only “&lt;300KB → 8/1 else 9/2”:
  | Size | CHS | Notes |
  |------|-----|--------|
  | 160K / 180K | 40/1/8 or 40/1/9 | SS DD |
  | 320K / 360K | 40/2/8 or 40/2/9 | DS DD (5150 INT 13) |
  | 720K | 80/2/9 | DD (Dragon Wars B:) |
  | 1.2M | 80/2/15 | 5.25" HD |
  | 1.44M | 80/2/18 | 3.5" HD |
  | 2.88M | 80/2/36 | optional later |
  FDC + FloppyOS `disk_read` LBA→CHS must use the **per-unit** SPT/heads, not
  drive 0’s 9/2 only.

- [ ] **Directory → virtual floppy: smallest standard size that fits** —
  measure the host tree (**recursive, subdirs included**), then pack FlopFS
  into 360K → 720K → 1.2M → 1.44M (and 2.88M if we add it). Today: **one
  directory, no subdirs, 32 8.3 names, 360K else 720K only.** A 1.3MB game
  dir must become 1.44M, not fail. If even 1.44M is too small → HDD C: (item
  above). Raise the 32-dirent cap when 1.2M/1.44M (or subdirs) need it.

### P2 — CPU / PC

- [ ] **80186 helpers** if a real code path needs them (`PUSHA`/`POPA`/`IMUL imm`/`PUSH imm`/`INS`/`OUTS`/`ENTER`/`LEAVE`). Bushido halt on `0x69` was **unrelocated garbage**, not a 186 requirement. Add when a proven CS:IP in the EXE uses them.
- [ ] Full 8237 HOLD/HLDA / DRAM refresh.
- [x] Conventional RAM **544K** (PPI nibble `0x0F`, 24-APR-81 BIOS max). 640K needs Oct-82 BIOS.
- [ ] COM1 8250 if FloppyOS UART print should be visible.

### Bottom of queue

- [ ] Cycle-exact 6845 (14.318 MHz, snow, wait states).
- [ ] iron86 dual MDA+CGA in one guest (tdx/tdxview already is the dual-head). Last.
- [ ] Cassette BASIC ROM map.
- [ ] 8087 (WAIT/ESC already NOP).

---

## FloppyOS

**Working:** Boot via 5150 INT 19; FlopFS v0.4; COMMAND `A:`/`B:`; DIR/TYPE/VER/EXIT; AH=4B COM/MZ with **all** MZ relocs + **32-bit** `dos_read` (Bushido 72K); FCB read; 32 root entries.

- [ ] **AH=4Ch** — return to COMMAND and tell tdx to retitle `COMMAND.COM`.
- [ ] **FCB write / create / find** (AH=16/17/11–13/15/22).
- [x] **AH=42 LSEEK** — Dragon Wars `DATA1` (SET then 3F). HELLO.COM SEEK OK.
- [ ] **Handle API** AH=3C/40/41 (create/write/unlink). 3D/3E/3F/42 exist. `AH=40` needed for `dragon -s` persist.
- [ ] **FlopFS v0.5 integrity** — XXH3-64 on `size>0` files; dirs not hashed; **size-0 skip hash**; pack-time dedup (hardlink-like); refcount before free. Spec: `subprojects/floppyos/docs/flopfs-spec.md`.
- [ ] Remaining **double-push** clones if any path besides `fill_dta` still unbalanced (`run_init_com` was fixed).
- [ ] **FAT12/16/32/exFAT** foreign volumes — **bottom of queue**. Primary FS stays FlopFS. Py86 never shipped `fat12.py`.
- [ ] COPY, batch, timestamps, subdirs, compression.
- [ ] INT 20/22/23/24/25/26 as needed for DOS apps.

---

## Suggested order

1. **Docs** match iron86 0.22 / tdx 0.27 (FORMAT C: as C:, cold POST to A>).
2. tdx/tdxview agent UX (`run` vs unpause, unique sockets, AH=4Ch title, FI colon).
3. CGA **3D9** palette + **40-col** modes 0/1 (iron86 + tdxview).
4. FDC Write (saves / COPY / `AH=40`).
5. FDC Format / Scan / Read ID.
6. Dir pack smallest-fit including subdirs (1.2M/1.44M) + HDD auto-size.
7. Skip for now: MDA blink, Hercules, Cassette BASIC, cycle-exact 8237 HOLD unless a title needs it.
8. **Full C++/Intel-asm BIOS clone (PC/XT/AT/Pentium) — last.** `--no-bios` until then.

**Docs:** `docs/CLI.md`, `docs/HOWTO_DRAGON_WARS.md`, `docs/MILESTONE_CGA_COMPOSITE.md`, `man/*.1`. Keep this file in sync.

Py86 remains the chip-level reference (`/tmp/RetroCodeMess/Py86`). Unicorn remains the default **MZ EXE** debugger path.
