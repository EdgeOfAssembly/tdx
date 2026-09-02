# FlopFS on-disk format v0.4

**Status:** Milestone 7  
**Magic:** `FLOPFS01`  
**Media:** 2880 × 512-byte sectors

## Layout

| LBA | Content |
|-----|---------|
| 0 | Boot sector |
| 1–2 | Superblock + mirror |
| 3–4 | Stage1.5 |
| 5+ | Kernel (`kernel_lba` / `kernel_sectors`) |
| … | Root directory (`root_lba`, usually 1 sector) |
| … | File data blobs (stored codec) |

## Superblock (512 bytes, LE)

| Off | Size | Field |
|----:|-----:|-------|
| 0 | 8 | `magic` `FLOPFS01` |
| 8 | 2 | `version_major` = 0 |
| 10 | 2 | `version_minor` = **4** |
| 12 | 2 | `sector_size` = 512 |
| 14 | 2 | `sector_count` = 2880 |
| 16 | 4 | `stage15_lba` |
| 20 | 2 | `stage15_sectors` |
| 22 | 2 | `flags` |
| 24 | 4 | `generation` |
| 28 | 4 | `crc32` (0) |
| 32 | 16 | `label` |
| 48 | 4 | `kernel_lba` |
| 52 | 2 | `kernel_sectors` |
| 54 | 2 | `kernel_load_seg` |
| 56 | 1 | `kernel_codec` (0=stored) |
| 57 | 1 | `reserved0` |
| 58 | 4 | `com_lba` (cache / fallback for init COM) |
| 62 | 2 | `com_sectors` |
| 64 | 2 | `com_load_seg` (PSP, default `0x2000`) |
| 66 | 4 | `root_lba` |
| 70 | 2 | `root_sectors` (1, or 2 when >16 files) |
| 72 | 11 | `init_name` FCB 8.3 (`HELLO   COM`) |
| 83 | 429 | `reserved` |

## Directory entry (32 bytes)

| Off | Size | Field |
|----:|-----:|-------|
| 0 | 11 | FCB name (8+3, space-padded, upper case) |
| 11 | 1 | `codec` (0=stored) |
| 12 | 4 | `lba` |
| 16 | 4 | `size` (bytes) |
| 20 | 2 | `sectors` |
| 22 | 10 | zero |

Empty entry: `name[0] == 0`. End of dir: `name[0] == 0xE5` optional; M7 uses zero name as free/end.

Max entries per root: **32** (16 per sector; `root_sectors` is 1 or 2).

## Integrity + dedup (planned v0.5 — not on-disk yet)

**Hash:** **XXH3-64** (8 bytes). Fastest widely used non-crypto hash; 8 bytes fit in the existing dirent `pad[10]` with 2 bytes left for flags. Collision chance is negligible on a 360K–1.44M floppy (and still tiny on HDD-sized FlopFS). XXH3-128 (16 bytes) is reserved if we ever grow the dirent; do not use SHA/BLAKE for this.

**Not hashed:** directory entries (dirs have no payload hash). **Size-0 files:** skip hashing. They occupy no data sectors anyway. All empty files would share one XXH3 of the empty buffer, which does not save space and only complicates unique names. Store `xxh3=0`, `lba=0`, `sectors=0`, flag `HASHED=0`. Each empty name stays its own dirent.

**Hashed files (`size > 0`):** `xxh3` over the stored codec bytes. Flag `HASHED=1`.

**Dedup (transparent, pack-time then runtime writes):** dirents with the same XXH3 and size share one data blob (`lba`/`sectors` identical), like a hard link to content. Names stay unique. `mkflopfs` must not write a second copy. Future delete/truncate needs a **refcount** on the blob (superblock table or first-sector header) before freeing LBA.

Planned dirent `pad[10]` layout (still 32-byte entry):

| Off | Size | Field |
|----:|-----:|-------|
| 22 | 8 | `xxh3` (0 if not hashed) |
| 30 | 1 | `flags` bit0=`HASHED`, bit1=`SHARED` |
| 31 | 1 | reserved |

Do not bump `FLOPFS01` / version_minor until mkflopfs + kernel actually write these fields.

## INT 21h file API (M7)

| AH | Meaning |
|----|---------|
| 3D | Open: AL=0 read-only, DS:DX ASCIIZ path → AX=handle (5..8) |
| 3F | Read: BX=handle, CX=bytes, DS:DX buf → AX=bytes read |
| 3E | Close: BX=handle |

Paths: `HELLO.COM`, `\HELLO.COM`, `B:HELLO.COM`, `B:\HELLO.COM`. 8.3 only. Drive `A:`/`B:` (DL=0/1).

## Init program (M7)

Kernel opens `init_name` (default `HELLO.COM`) via AH=3D, reads into PSP:0100 via AH=3F, closes, far-jumps. Falls back to `com_lba` if open fails.
