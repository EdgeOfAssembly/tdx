# TDX / tdxview / tdxctl / iron86 — command-line reference

Order-independent options (cli-design). No-args prints usage except **tdxview**
(attaches to `/tmp/tdx.sock`). `-h`/`--help` and `-v`/`--version` on every tool.
Version **0.27** (tdx / tdxview / tdxctl), iron86 **0.23**.
`tdxctl getip` / `setip 0FDB:A360` / `setip 0xFE199` (canonical CS:IP).

Runtime floppy swap (no CPU reset): `tdxctl floppy b /path/DISK02.IMG`
(A: stays the DOS boot disk; B: is M1TP “DISK B”).

Man pages: `man/tdx.1`, `man/tdxview.1`, `man/tdxctl.1`, `man/iron86.1`
(`mandoc -T lint`; `man -l man/tdx.1`).

---

## tdx

```text
tdx [options] [file.exe|file.com]
tdx --floppy-a image.img [options]
tdx --bios BIOS.BIN [--floppy-a A] [--floppy-b B] [options]
tdx --floppy-a image.img --no-bios [options]
```

| Switch | Meaning |
|--------|---------|
| `-h`, `--help` | Usage |
| `-v`, `--version` | Version |
| `--floppy-a PATH` | A: raw image **or** host directory (FlopFS pack) |
| `--floppy PATH` | Alias for `--floppy-a` |
| `--floppy-b PATH` | B: image or host dir (FlopFS **360K** if it fits, else **720K**) |
| `--uc-floppy IMAGE` | Same boot on Unicorn (not the iron86 `--bios` path) |
| `--preset NAME` | `ibm-pc`/`5150` floppy; `ibm-xt`/`5160` Xebec. `--hdc` implies xt |
| `--bios FILE` | IBM 5150 8K or 5160 64K. Default from `--preset` |
| `--no-bios` | Skip ROM; iron86 C++ INT 10/13/16 (FloppyOS smoke) |
| `--mda` | MDA 80×25 (B000). Default CGA |
| `--no-8087` | Disable 8087 NPX (default on) |
| `--no-ui` | Headless |
| `--game` | Also open CGA in this process (default: use **tdxview**) |
| `--no-sock` | Do not listen on the agent socket |
| `--sock PATH` | Agent socket (default `/tmp/tdx.sock`) |
| `--log-file PATH` | Also write logs to PATH |
| `--symbols PATH` | TSV/MAP symbols |
| `--ghidra` | Headless Ghidra export (EXE, floppy image, or BIOS) then load symbols |
| `--cwd PATH` | DOS current directory for INT 21 |
| `--run` | After load, run until break/halt |
| `--verbose` | Log every stepped instruction |
| `--exec-map FILE` | 1 MiB executed-opcode map (iron86) |
| `--prefetch-bytes N` | BIU prefetch queue (default **6=8086**; 4=8088; 0=off) |
| `--hdc PATH` | Xebec Type-1 image (regular file only; never `/dev/sd*`) |
| `--hdc-rom FILE` | Xebec ROS at C8000 (default IBM 5000059 if present) |
| `--no-fast-post` | Cold POST from FFFF:0000 (default skips STGTST via 1234h) |
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

Commands include `unpause`, `pause`, `delay [N]`, `key`, `type` (`\\n` = Enter),
`shot`, `cga`, `status` / `getip` (`running` + `stop`; `stop=1` is STEP, not
paused), `bp` / `bpint` / `bpm`, `dump cga [FILE]` (16 KiB B800 → `SCREEN.CGA`),
`dump mda [FILE]` (4 KiB B000 → `SCREEN.MDA`). Agent **`run`** is start-only
(same as `unpause`); CPU-window F9 still toggles.

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
| `--preset NAME` | `ibm-pc`/`5150` floppy; `ibm-xt`/`5160` Xebec |
| `--bios FILE` | 5150 8K or 5160 64K (default from `--preset`) |
| `--no-fast-post` | Cold POST (no `RESET_FLAG=1234h`) |
| `--no-audio` | Disable PC speaker BEL (default on) |
| `--no-8087` | Disable 8087 NPX (default on) |
| `--mda` | MDA 80×25 |
| `--floppy-a PATH` | Drive 0: image or directory |
| `--floppy-b PATH` | Drive 1: image or directory |
| `--floppy PATH` | Alias for `--floppy-a` |
| `--keys STRING` | Type STRING as INT 16h keys |
| `--exec-map FILE` | 1 MiB executed-opcode map |
| `--hdc PATH` | Xebec Type-1 image (regular file only) |
| `--hdc-rom FILE` | Xebec ROS at C8000 |
| `--log FILE` | Append event log (default off) |
| `--lock-mem` | mlock guest RAM + HDC (default off) |
| `--config PATH` | INI (default `iron86.ini` in cwd if present) |
| `--until-prompt` | Stop at B800 `A>` (boot bench) |
| `--prefetch-bytes N` | BIU prefetch queue (default **6=8086**; 4=8088; 0=off) |

---

## Floppy geometry (honest)

**Directory pack (virtual floppy):** recursive 8.3 files flattened to the root.
Smallest of **360K / 720K / 1.2M / 1.44M**. Duplicate 8.3 names fail.
Larger trees: `--hdc DIR` packs a Type-1 FAT12 HDD (`DIR.img`).
Raw floppy **writes persist** when the image was attached from a host file.

**Raw image CHS:** exact size first (160K/180K/320K/360K/720K/1.2M/1.44M), else
nearest IBM geometry. No 2.88M.

**HDD:** `tdx --preset ibm-xt --hdc FILE` — guest FDISK + FORMAT C: as letter C:,
`DIR C:`, copy+reboot persist, and `--no-fast-post` cold POST to A> (iron86 0.23
PB3 SW1 mux; STGTST ~3 min). Fast-post remains the play path. Never `--hdc /dev/sd*`.
