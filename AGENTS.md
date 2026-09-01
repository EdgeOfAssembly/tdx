# TDX / librex — agent notes

- Language: C++23 (g++) + C23 for `mz_parse.c` / `dos_cga.c`. Allman braces.
- Public ABI: `include/rex/rex.h` (C). Do not break it casually.
- Tests: `make -s test` (Catch2). Formal: `make -s verify` (CBMC on MZ parser).
- Makefile now emits/includes `-MMD -MP` header deps: editing anything under
  `include/` correctly rebuilds dependents. (A stale `.o` after a header edit
  once silently produced an ABI-mismatched `rex_insn` and crashed tdx.)
- Extra tools: `scripts/img2txt.py` renders an image to ASCII on stdout
  (import of `/mnt/ascii` pipeline; no video stage) so text-only models can
  "see" screenshots: `scripts/img2txt.py /tmp/shot.bmp -w 110`.
- CLI: `cli-design` — no-args = usage, `-h`/`--help`, `-v`/`--version` from 0.2,
  `--no-ui` / `--no-sock` (defaults ON). In-process CGA is **off**; `--game` enables it.
  Separate viewer: `tdxview` (no-args attaches to `/tmp/tdx.sock`).
- Interactive proof: **tdx** + **tdxview** on the host display; agents talk only
  through UNIX sockets (`/tmp/tdx.sock`, `/tmp/tdxview.sock`) and `tdxctl shot`.
  Do **not** put tdx/tdxview under Xmux. Golden Bushido demo:
  `docs/GOAL_BUSHIDO_DEMO.md` (DOSBox+Xmux reference loop is already recorded).
- Do not commit `BORLANDC/` (copyrighted Borland C++ 3.1).
- Single file LOC: soft 5k, hard 10k (`cloc`).
- Future arches (Z80/6502/M68K) implement the same session/target ideas; do not
  special-case DOS in `rex.h`.
