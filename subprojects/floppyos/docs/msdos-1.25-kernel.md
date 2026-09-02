# MS-DOS 1.25 kernel — FCB / INT 21h reference

Oracle for FloppyOS and tdx FCB shims. Behavior below is from the 1.25
sources, **not** from later DOS 2+ handle APIs.

**Do not copy MS-DOS source into this tree.** Cite `file:line` and keep
snippets to a few lines.

| Role | Path |
|------|------|
| Kernel | `/home/wizard/MS-DOS/v1.25/source/MSDOS.ASM` |
| Build wrapper | `/home/wizard/MS-DOS/v1.25/source/STDDOS.ASM` |
| BIOS / IO.SYS-style | `/home/wizard/MS-DOS/v1.25/source/IO.ASM` |
| COMMAND.COM | `/home/wizard/MS-DOS/v1.25/source/COMMAND.ASM` |

Revision banner: `MSDOS.ASM:1` (86-DOS 1.25, Tim Paterson, 1982-03-03).

---

## 1. File map

| File | What it is | FCB? |
|------|------------|------|
| **`STDDOS.ASM`** | Thin build file: `MSVER`/`IBM`/`HIGHMEM`/`DSKTEST`, then `INCLUDE MSDOS.ASM`. This tree: `MSVER EQU TRUE`, `IBM EQU FALSE` (`STDDOS.ASM:9–21`). | No logic of its own |
| **`MSDOS.ASM`** | DOS kernel: INT 20h/21h, FCB open/read/write, FAT12, 32-byte directory, DTA. Dispatch at `COMMAND` (`MSDOS.ASM:270`). | **Yes — all FCB syscalls** |
| **`IO.ASM`** | Machine I/O (console, aux, printer, FDC). Far-jump table at offset 0: `INIT/STATUS/INP/OUTP/PRINT/AUXIN/AUXOUT/READ/WRITE/DSKCHG/…` (`IO.ASM:109–126`). DOS calls this as BIOS; it does **not** implement FCB. | No |
| **`COMMAND.ASM`** | COMMAND.COM (resident + transient). Internal `TYPE`/`COPY`/`DIR` are FCB **clients**. | Client only |

`IBM EQU TRUE` (PC DOS) vs `FALSE` (this STDDOS) changes console keys, device names (`COM1`), and **OPEN’s EXTENT zeroing** (`ZEROEXT`, `MSDOS.ASM:47–61`). Recsize-on-open and RR rules are the same.

INT vectors from `INTBASE EQU 80H` (`MSDOS.ASM:65`): INT 20h abort, **21h** `COMMAND`, 22h terminate addr, 23h Ctrl-C, 24h critical error, 25h/26h BIOS disk read/write.

---

## 2. INT 21h dispatch (FCB-related)

Entry `COMMAND` (`MSDOS.ASM:270`): **AH** = function. `AH > MAXCOM` (46) → `AL=0`, IRET. CP/M `CALL 5` uses **CL**, copied to AH, max `MAXCALL` (36).

AH ≥ 13 uses the disk stack; AH ≤ 12 uses the I/O stack (`MSDOS.ASM:319–323`). Table is 0-based (`DISPATCH`, `MSDOS.ASM:349`).

| Dec | AH | Label | FCB / DTA |
|-----|-----|-------|-----------|
| 15 | `0Fh` | `OPEN` | DS:DX FCB. `AL=00` ok, `FFh` fail |
| 16 | `10h` | `CLOSE` | DS:DX FCB |
| 17 | `11h` | `SRCHFRST` | DS:DX FCB; 32-byte dirent → DTA |
| 18 | `12h` | `SRCHNXT` | |
| 19 | `13h` | `DELETE` | |
| 20 | `14h` | `SEQRD` | sequential read, 1 record |
| 21 | `15h` | `SEQWRT` | sequential write |
| 22 | `16h` | `CREATE` | |
| 23 | `17h` | `RENAME` | |
| 26 | `1Ah` | `SETDMA` | DS:DX = DTA |
| 33 | `21h` | `RNDRD` | random read, 1 record |
| 34 | `22h` | `RNDWRT` | |
| 35 | `23h` | `FILESIZE` | size in RR, in records |
| 36 | `24h` | `SETRNDREC` | RR ← EXTENT+NR |
| 39 | `27h` | `BLKRD` | random **block** read; **CX** = record count |
| 40 | `28h` | `BLKWRT` | CX in/out = records |
| 41 | `29h` | `MAKEFCB` | parse pathname → FCB |

On return from FCB I/O, **AL = `[DSKERR]`** (`SETNREX`, `MSDOS.ASM:1450`). `BLKRD`/`BLKWRT` also write **CX** back to the caller’s saved CX (`FINBLK`, `MSDOS.ASM:1428–1430`).

---

## 3. `FCBLOCK` layout

`MSDOS.ASM:78–96`. Packed; `FIRCLUS` is unaligned. Published “reserved” 18h–1Fh is DOS-internal.

| Off | Size | Name | Meaning |
|-----|------|------|---------|
| 00 | 1 | drive | 0 = default; 1 = A. OPEN stores **1-based physical** |
| 01–08 | 8 | name | Space-padded 8.3 name |
| 09–0B | 3 | ext | Space-padded |
| 0C–0D | 2 | **EXTENT** | Current block = record `>> 7` |
| 0E–0F | 2 | **RECSIZ** | Bytes/record (user-settable **after** OPEN) |
| 10–13 | 4 | **FILSIZ** | Size in bytes (STRUC `FILSIZ` at 10h, `DRVBP` at 12h; search reuses 10h as `FILDIRENT`) |
| 14–15 | 2 | FDATE | Last write |
| 16–17 | 2 | FTIME | Last write |
| 18 | 1 | **DEVID** | Bits 0–5 unit; bit7=1 device; file bit6=0 dirty; device bit6=0 EOF |
| 19–1A | 2 | FIRCLUS | First cluster |
| 1B–1C | 2 | LSTCLUS | Last cluster accessed |
| 1D–1E | 2 | CLUSPOS | Position of that cluster |
| 1F | 1 | (pad) | Forces NR to offset 20h |
| 20 | 1 | **NR** | Next sequential record, 0–127 |
| 21–23 | 3 | **RR** | Random record (low 24 bits) |
| 24 | 1 | RR+3 | 4th RR byte **only if RECSIZ &lt; 64** |

Extended FCB: byte 0 = `FFh`, +7 = normal FCB (`MOVNAME` `MSDOS.ASM:826–828`; same skip in `GETREC`/`GETRRPOS`/`CLOSE`).

Record number:

```text
rec = (EXTENT << 7) | (NR & 0x7F)     ; GETREC  MSDOS.ASM:2335
NR  = rec & 0x7F
EXTENT = rec >> 7                     ; SETNREX MSDOS.ASM:1440
```

---

## 4. OPEN — AH=`0Fh` / function 15

`GETFILE` (`MSDOS.ASM:545`): **DS:DX = FCB** in the **caller** segment. Name is copied from that pointer (`MOVNAME` `MSDOS.ASM:804`, `SI=DX`). Success: `ES:DI` = that same FCB.

`DOOPEN` (`MSDOS.ASM:864`) on a directory hit:

1. `FCB[0] ← THISDRV+1` (never leave 0).
2. **EXTENT:**
   - IBM `ZEROEXT`: `ADD DI,11` / `STOSW` → **both** EXTENT bytes 0 (`MSDOS.ASM:880–882`).
   - This STDDOS (`NOT ZEROEXT`): `ADD DI,12` / `STOSB` → **high** byte 0, **low byte unchanged** (CP/M may have set it) (`MSDOS.ASM:884–887`).
3. **`MOV AL,128` / `STOSW` → RECSIZ is always 128**, even if the guest stored 25 first (`MSDOS.ASM:888–889`).
4. From the 32-byte dirent (`MSDOS.ASM:99–110`): first cluster, **FILSIZ** (4 bytes), date, time.
5. `DEVID ← unit | 40h` (file, not dirty), `FIRCLUS`/`LSTCLUS` = first cluster, `CLUSPOS = 0`.
6. **Does not write NR or RR.**

Device names (`CON`/`AUX`/…) take `OPENDEV` (`MSDOS.ASM:909`): recsize 128, size 0, today’s date/time, `DEVID` with bit7 set.

`AL=00` success, `FFh` fail (`ERRET` `MSDOS.ASM:799`).

**After OPEN**, guests may change RECSIZ. `SETUP` on every I/O: RECSIZ `0` → 128; **nonzero is kept** (`MSDOS.ASM:1504–1508`). COMMAND `TYPE` and HEX2BIN set recsize **after** open.

---

## 5. SETDMA — AH=`1Ah` / function 26

```asm
; MSDOS.ASM:2611
SETDMA: MOV CS:[DMAADD],DX
        MOV CS:[DMAADD+2],DS
```

DTA = **DS:DX**. Default displacement `80h` (PSP DTA), segment filled at init (`MSDOS.ASM:3654`, `3738`). All FCB reads/writes transfer to `[DMAADD]`.

---

## 6. SEQRD / RNDRD / BLKRD

### 6.1 Entry

| Call | AH | Path | Record index | Count |
|------|-----|------|--------------|-------|
| SEQRD | `14h` | `GETREC` → `LOAD` → `FINSEQ` | EXTENT+NR | CX := 1 |
| RNDRD | `21h` | `GETRRPOS1` → `LOAD` → `FINRND` | RR | CX := 1 |
| BLKRD | `27h` | `GETRRPOS` → `LOAD` → `FINBLK` | RR | **CX = caller CX** |

```asm
; MSDOS.ASM:1453
GETRRPOS1: MOV CX,1
GETRRPOS:  ; DI=DX FCB (skip 7 if FF-prefixed)
           MOV AX,WORD PTR [DI.RR]
           MOV DX,WORD PTR [DI.RR+2]   ; DH = RR byte 3
```

`GETRRPOS1` is RNDRD’s one-record wrapper. `GETRRPOS` does **not** force CX=1 — BLKRD keeps the user’s record count (1.24 note: “Restore fcn. 27 to 1.0 level”, `MSDOS.ASM:30`).

### 6.2 `SETUP` and 3-byte RR

If `RECSIZ >= 64`, **`DH ← 0`**: ignore RR MSB (`MSDOS.ASM:1521–1523`). Same rule on `FILESIZE` / `SETRNDREC` (`MSDOS.ASM:2605–2607`, `2692–2694`): only a 3-byte field when recsize ≥ 64.

### 6.3 `LOAD` outputs

`LOAD` (`MSDOS.ASM:1865`) → `SETUP` then disk/device read → `SETFCB`/`ADDREC`.

The comment at `LOAD:1873` says “CX = bytes”; **the code returns records** (`SETFCB` divides byte count by RECSIZ, `MSDOS.ASM:1959–1983`; `STORE`’s comment at `2075` already says records). `ADDREC` (`MSDOS.ASM:1990`):

| Register | Meaning |
|----------|---------|
| **DX:AX** | Last record **read** (`RECPOS + CX - 1` if CX≠0; else unchanged `RECPOS`) |
| **CX** | Records transferred (partial last record counts as 1) |
| **AL** (via `SETNREX`) | `[DSKERR]` |

### 6.4 `DSKERR` → AL

| AL | Name | When |
|----|------|------|
| 0 | OK | All requested records, full |
| 1 | EOF | No data, or fewer **full** records than asked (`MSDOS.ASM:1965`, `1581`) |
| 2 | Trim | Request clipped to 64K segment remainder (`SETUP` `MSDOS.ASM:1573`) — may be overwritten later |
| 3 | Partial | Last record short; remainder **zero-filled** in the DTA (`MSDOS.ASM:1968`) |
| 4 | No FCB/drive | `GETBP` failed (`NOFILERR` `MSDOS.ASM:1465`) |

### 6.5 `FINRND` / `FINBLK` / `FINSEQ`

```asm
; MSDOS.ASM:1428  BLKRD only
FINBLK:  MOV [user CXSAVE],CX
         JCXZ FINRND
         ADD AX,1 / ADC DX,0     ; next record
FINRND:  MOV ES:[DI.RR],AX       ; RR low 16
         MOV ES:[DI.RR+2],DL     ; RR byte 2
         ; RR+3 written only if DH != 0
FINSEQ:  ; SEQRD: +1 if CX!=0, then SETNREX — does not write RR
SETNREX: NR ← AX & 7Fh ; EXTENT ← rec>>7 ; AL ← DSKERR
```

| Call | RR after | NR / EXTENT |
|------|----------|-------------|
| **AH=`21h` RNDRD** | **Last record read** (no +1) | `SETNREX` from that same rec |
| **AH=`27h` BLKRD** | **Last+1 if CX≠0**; if CX=0 (immediate EOF), RR unchanged | `SETNREX` from the value stored in RR |
| **AH=`14h` SEQRD** | **Unchanged** | `SETNREX` from last+1 if CX≠0 |

RNDWRT/BLKWRT share `FINRND`/`FINBLK`. SEQWRT shares `FINSEQ`.

---

## 7. COMMAND.COM `TYPE` sequence

Internal command `TYPE` → `TYPEFIL` (`COMMAND.ASM:191`, `1284`). Equates: `OPEN=15`, `RDBLK=39`, `SETDMA=26`, `FCB=5Ch`, `RR=33`, `RECLEN=14` (`COMMAND.ASM:48–65`).

Order:

1. **SETDMA** to TPA (`DS=[TPA]`, `DX=0`).
2. `DS=CS`, **OPEN** the PSP FCB at `5Ch` (parser already filled 8.3). Fail if `AL≠0`.
3. **Zero RR** (4 bytes at `FCB+33`).
4. **`RECLEN=1`** (byte records) — **after** OPEN’s 128.
5. Loop **RDBLK** (`AH=27h`) with `CX=[BYTCNT]` (TPA size in bytes).
6. Stop if **`CX=0`** (EOF) or a `1Ah` byte. Print with AH=2.

`COPY` is the same idea: `DEST+RECLEN=1`, RR zeroed, `WRBLK` (`COMMAND.ASM:1707–1724`). EXE load uses recsize 512 then 1 (`COMMAND.ASM:2047`, `2104`).

---

## 8. Implications for FloppyOS (guest DOS shim)

1. **FCB is `DS:DX` in the caller’s segment** at INT 21h entry — same DS the guest used. `GETFILE`/`MOVNAME` read the 11-byte 8.3 from **that** pointer (`MSDOS.ASM:545–554`, `821`). Copy name into kernel CS **first**; never run `fcb11_to_path` against kernel DS while SI still points at the guest FCB.
2. Extended FCB (`FFh` at `[DS:DX]`) → real FCB at `DX+7`.
3. OPEN: force **RECSIZ=128**; do not honor a pre-open 25. Zero EXTENT as in §4. Fill size/date/time/first cluster. **Do not** clear NR/RR (TYPE/fcbtest zero RR themselves).
4. I/O: RECSIZ 0 means 128; otherwise keep guest recsize (TYPE sets 1 after open).
5. DTA is whatever SETDMA last stored (default PSP:`80h`), not a kernel buffer.
6. AH=`14h`: rec from EXTENT+NR; bump NR/EXTENT; leave RR.
7. AH=`21h`: rec from RR; **do not** add 1 to RR; still `SETNREX`.
8. AH=`27h`: **CX is record count in and out**; RR ← last+1 iff CX≠0; AL=`DSKERR`.
9. If RECSIZ ≥ 64, RR is **24-bit** (ignore/don’t require offset 24h).
10. OPEN/CLOSE return `AL=00/FFh`. Reads return `AL=0/1/3` (and 2/4 if you implement trim / bad FCB).

FloppyOS `kernel.asm` FCB path (`fcb_open` / `fcb_read`) should keep the DS:DX copy into `fcb_tmp` before `fcb11_to_path`. Known 1.25 gaps to close when touching that code: OPEN must not zero NR/RR; random RR should be 24-bit, not 16-bit.

---

## Quick recap (OPEN / 21 / 27)

| | OPEN `0Fh` | RNDRD `21h` | BLKRD `27h` |
|--|------------|-------------|-------------|
| Pointer | DS:DX FCB | DS:DX FCB | DS:DX FCB |
| Recsize | **Always 128** | SETUP: 0→128 else keep | same |
| Position | EXTENT high 0 (MS) / both 0 (IBM); NR/RR untouched | RR (CX=1) | RR (CX=n) |
| RR after | unchanged | last read | last+1 if n≠0 |
| CX out | — | — | records transferred |
| AL | 00 / FF | DSKERR 0/1/3 | DSKERR 0/1/3 |
