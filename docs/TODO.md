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

- [ ] **Window title after AH=4Ch** — restore `COMMAND.COM` (or clear) when the child exits; today the last EXE name sticks.
- [ ] **tdxview SDL_TEXTINPUT** — host FI layout sends `SDL_Keycode` `;` for `:`; colon is Shift+`;` on US XT. Type `B:` from a Finnish keyboard without tdxctl.
- [ ] **`tdxctl run` vs unpause** — `run` toggles F9; agents should prefer `unpause`. Document or make `run` start-only.
- [ ] **Unique sockets per pair** — `/tmp/tdx.sock` is one pair; two TDX windows need `--sock` defaults that include PID, or a small launcher.
- [ ] **CGA 3D9 palette** in tdxview for all palettes Bushido uses (cyan/magenta vs red/green).
- [ ] **VCR / symbols / Ghidra** — keep working on Unicorn EXE; untested on `--bios` iron86.
- [ ] **Unicorn FloppyOS keys** — host INT 21 + INT 16 workaround; `--uc-floppy` is not the Bushido path. Don’t spend time unless someone wants Unicorn as DOS.

---

## iron86 (BIOS + chips)

**Working:** 8086 dispatch (enough for POST + Bushido), packed PPI/PIT/PIC/DMA/FDC, two 6845s (MDA `3Bx` + CGA `3Dx`), LPT stub, 360K A:+B:, dir→FlopFS pack, XT keys IRQ1, BDA `40:49` → tdxview mode, speaker `--no-audio`, DAA/WAIT/ESC NOPs.

### Fonts (complete for US IBM 5788005)

The card ROM `ROM/IBM_5788005_AM9264_1981_CGA_MDA_CARD.BIN` (MAME SHA1 `c2a8b108…`, **not pushed**) is the real IBM MDA+CGA character generator: 256×8×14 MDA + 256×8×8 CGA thick (thin bank unused). BIOS `CRT_CHAR_GEN` (`F000:FA6E`) is the 128-glyph 8×8 used for graphics TTY. tdxview uses 5788005 when present, else BIOS 8×8. **US CP437 fonts are 100%.** (European 4733197 Ø/ø ROM is a different chip; ignore unless a game needs it.)

### IBM video modes — implement when a game needs them

| Mode | Hardware | tdxview now |
|------|----------|-------------|
| 0 | CGA 40×25 B&W text | **missing** (drawn as 80×25) |
| 1 | CGA 40×25 16-color text | **missing** (drawn as 80×25) |
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
- [ ] **3D9 palette** — full color-select (background, cyan/magenta vs red/green vs intense, burst).
- [x] **Mode 6** 640×200 1bpp (tdxview; not mode 4’s 2bpp).
- [ ] **Modes 0 / 1** 40-column text (0 ≈ B&W, 1 = 16-color).
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

- [ ] **XT hard disk (Xebec / WD1002-ish) as C:** — ports `320h–323h`, DMA3, IRQ5,
  option ROM `C800:0` if we ship a ROM. Py86 `hdc_xebec.py` is the reference.
  **Next session (Fri/Sat):** `--hdc FILE` **and** auto-size a virtual HDD from a
  host directory that does not fit any floppy (e.g. 1.3MB tree → C: image).
- [ ] **FDC Write Data** DMA-from-mem (persist A:/B:). Needed for COPY, editors, save games.
- [ ] **FDC Format / Scan / Read ID** — after Write.
- [x] **720K geometry** — FlopFS packer: 360K if it fits, else 720K 80/2/9
  (Dragon Wars ~708K). **Not yet:** 1.2M (80/2/15) or 1.44M (80/2/18) packer or
  CHS auto-detect. Raw images: &lt;300KB → 8/1, else 9/2. Pack is **non-recursive**,
  **32** 8.3 names. Over 720K → fail (use HDD C: next).

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

1. **XT HDD C:** auto-sized from a host dir that does not fit 720K (user: next Fri/Sat).
2. FDC Write (saves / COPY / `AH=40`).
3. FlopFS xxhash3/dedup.
4. Optional 1.2M/1.44M floppy packer + CHS from image size.
5. CGA 40-col / 3D9 / Hercules **only with a title in hand**.
6. FAT12 and FDC Format/Scan last.

**Docs:** `docs/CLI.md`, `docs/HOWTO_DRAGON_WARS.md`, `docs/MILESTONE_CGA_COMPOSITE.md`, `man/*.1`. Keep this file in sync.

Py86 remains the chip-level reference (`/tmp/RetroCodeMess/Py86`). Unicorn remains the default **MZ EXE** debugger path.
