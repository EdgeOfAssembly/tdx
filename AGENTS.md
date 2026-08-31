# TDX / librex — agent notes

- Language: C++23 (g++) + C23 for `mz_parse.c` / `dos_cga.c`. Allman braces.
- Public ABI: `include/rex/rex.h` (C). Do not break it casually.
- Tests: `make -s test` (Catch2). Formal: `make -s verify` (CBMC on MZ parser).
- CLI: `cli-design` — no-args = usage, `-h`/`--help`, `-v`/`--version` from 0.1,
  `--no-ui` / `--no-sock` / `--no-game` (those features default ON).
- Interactive proof: Xmux session named `tdx`; two SDL windows; liberal screenshots.
- Do not commit `BORLANDC/` (copyrighted Borland C++ 3.1).
- Single file LOC: soft 5k, hard 10k (`cloc`).
- Future arches (Z80/6502/M68K) implement the same session/target ideas; do not
  special-case DOS in `rex.h`.
