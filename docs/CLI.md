# TDX / tdxview / tdxctl / iron86 — command-line reference

Order-independent options (cli-design). No-args prints usage except **tdxview**
(attaches to `/tmp/tdx.sock`). `-h`/`--help` and `-v`/`--version` on every tool.
Version **0.19** (tdx / tdxview / tdxctl), iron86 **0.13**.

Man pages: `man/tdx.1`, `man/tdxview.1`, `man/tdxctl.1`, `man/iron86.1`
(`mandoc -T lint`; `man -l man/tdx.1`).

---

## tdx

```text
tdx [options] [file.exe|file.com]
tdx --floppy-a image.img [options]
tdx --bios BIOS.BIN [--floppy-a A] [--floppy-b B] [options]
```

| Switch | Meaning |
|--------|---------|
| `-h`, `--help` | Usage |
| `-v`, `--version` | Version |
| `--floppy-a PATH` | A: raw image **or** host directory (FlopFS pack) |
| `--floppy PATH` | Alias for `--floppy-a` |
| `--floppy-b PATH` | B: image or host dir (FlopFS **360K** if it fits, else **720K**) |
| `--uc-floppy IMAGE` | Same boot on Unicorn (not the iron86 `--bios` path) |
| `--bios FILE` | IBM 5150 8K BIOS on iron86 (`FFFF:0000`) |
| `--mda` | MDA 80×25 (B000). Default CGA |
| `--no-ui` | Headless |
| `--game` | Also open CGA in this process (default: use **tdxview**) |
| `--no-sock` | Do not listen on the agent socket |
| `--sock PATH` | Agent socket (default `/tmp/tdx.sock`) |
| `--log-file PATH` | Also write logs to PATH |
| `--symbols PATH` | TSV/MAP symbols |
| `--ghidra` | Headless Ghidra export then load symbols |
| `--cwd PATH` | DOS current directory for INT 21 |
| `--run` | After load, run until break/halt |
| `--verbose` | Log every stepped instruction |
| `--exec-map FILE` | 1 MiB executed-opcode map (iron86) |
| `--scale N` | CPU window integer scale (default 2) |

Always `scripts/tdx-kill.sh` before a new tdx/tdxview pair.

---

## tdxview

```text
tdxview [options]
```

| Switch | Meaning |
|--------|---------|
| `-h`, `--help` | Usage |
| `-v`, `--version` | Version |
| `--sock PATH` | tdx socket (default `/tmp/tdx.sock`) |
| `--listen PATH` | Agent socket (default `/tmp/tdxview.sock`) |
| `--no-listen` | Do not listen for agents |
| `--no-composite` | CGA gfx as RGBI (default: **old-CGA NTSC**, Reenigne/86Box) |
| `--scale N` | Graphics scale (default 2 → 640×400). Text is 640×400 |

---

## tdxctl

```text
tdxctl [options] <command> [args…]
```

| Switch | Meaning |
|--------|---------|
| `-h`, `--help` | Usage |
| `-v`, `--version` | Version |
| `--sock PATH` | Socket (default `/tmp/tdx.sock`, or `/tmp/tdxview.sock` with `--view`) |
| `--view` | Talk to tdxview (game window shot/keys) |
| `--ctl` | Keep-alive stdin pipeline |

Commands include `unpause`, `pause`, `delay [N]`, `key`, `shot`, `cga`, `status`,
`bp` / `bpint` / `bpm`, `dump cga [FILE]` (16 KiB B800 → `SCREEN.CGA`),
`dump mda [FILE]` (4 KiB B000 → `SCREEN.MDA`). Prefer **`unpause`** over `run`
(F9 toggle).

---

## iron86

```text
iron86 [options] [file.com]
iron86 --bios BIOS.BIN [--floppy-a PATH] [--floppy-b PATH] [--keys STRING]
```

| Switch | Meaning |
|--------|---------|
| `-h`, `--help` | Usage |
| `-v`, `--version` | Version |
| `--bios FILE` | 5150 8K BIOS |
| `--no-fast-post` | Cold POST (no `RESET_FLAG=1234h`) |
| `--no-audio` | Disable PC speaker BEL (default on) |
| `--mda` | MDA 80×25 |
| `--floppy-a PATH` | Drive 0: image or directory |
| `--floppy-b PATH` | Drive 1: image or directory |
| `--floppy PATH` | Alias for `--floppy-a` |
| `--keys STRING` | Type STRING as INT 16h keys |
| `--exec-map FILE` | 1 MiB executed-opcode map |

---

## Floppy geometry (honest)

**Directory pack (virtual floppy):** non-recursive, max **32** 8.3 names.
Fits **360K** (40/2/9) if possible, else **720K** (80/2/9). **Fails** if the
tree needs more than 720K or more than 32 files. **No 1.2M / 1.44M packer yet.**
**No subdirectories.**

**Raw image CHS:** size **&lt; 300 000** bytes → 8 spt, 1 head; otherwise **9 spt,
2 heads** (covers 360K and 720K). **1.2M (15 spt) and 1.44M (18 spt) are not
auto-detected.**

**Next:** XT HDD as **C:** for trees larger than a floppy — see `docs/TODO.md`.
