# TDX / librex — agent notes

## Three-repo layout

| Tree | Path | Role |
|------|------|------|
| **tdx** | `/mnt/tdx` (symlink `/mnt/TurboDebugger` → same) | Debugger + librex; links system **libiron86** |
| **iron86** | `/mnt/iron86` | C++23 8086+PC machine; install with `sudo make install` |
| **FloppyOS** | `/mnt/FloppyOS` | Guest OS / FlopFS; independent of the tdx build |
| **share data** | `/usr/local/share/iron86` | BIOS + `disks/msdos331.img` (default guest); `disks/floppyos.img` is optional |

Build requires **system libiron86** (`pkg-config --exists iron86`). Install from the
iron86 tree: `cd /mnt/iron86 && sudo make install`. Headers land in
`/usr/local/include/iron86/`, libs in `/usr/local/lib/`, and
`/usr/local/lib/pkgconfig/iron86.pc`. The tdx Makefile prepends
`/usr/local/lib/pkgconfig` to `PKG_CONFIG_PATH` (Gentoo’s default path omits it).
`make check-iron86` fails fast if pkg-config cannot find iron86.

- Language: C++23 (g++) + C23 for `mz_parse.c` / `dos_cga.c`. Allman braces.
- Public ABI: `include/rex/rex.h` (C). Do not break it casually.
- Tests: `make -s test` (Catch2). Formal: `make -s verify` (CBMC on MZ parser).
- Emulator: **iron86** is the supported backend (`--floppy-a` / `--bios`) with
  guest `/usr/local/share/iron86/disks/msdos331.img`. Unicorn stays linked for
  positional MZ EXE/COM (`USE_UNICORN=1` default) but is **deprecated and not
  the supported path**. Do not flip `USE_UNICORN` to 0. `make USE_UNICORN=0`
  only builds a tree that omits the Unicorn dependency.
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
- **One pair only.** Before starting tdx or tdxview, always close leftovers:
  `scripts/tdx-kill.sh` (pkill -x tdx/tdxview, wait, rm `/tmp/tdx.sock`
  `/tmp/tdxview.sock`). Never attach tdxview to a stale socket or run a second
  tdx. Extra PIDs mix Unicorn EXE sessions with `--bios` FloppyOS and look like
  “terminated” / black tdxview.
- Do not commit `BORLANDC/` (copyrighted Borland C++ 3.1).
- Single file LOC: soft 5k, hard 10k (`cloc`).
- Future arches (Z80/6502/M68K) implement the same session/target ideas; do not
  special-case DOS in `rex.h`.
