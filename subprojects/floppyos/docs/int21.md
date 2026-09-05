# FloppyOS INT 21h (current)

| AH | Name | Notes |
|----|------|-------|
| **01** | Read char echo | INT 16h, else COM1 serial |
| 02 | Write char | DL=char → VGA + COM1 |
| **07** | Read char raw | no echo |
| **08** | Read char | no echo |
| **0A** | Buffered line | DS:DX buffer max/count/data |
| **0B** | Input status | AL=00 none, AL=FF ready |
| **0E** | Select drive | AL=0/1 (A:/B:); ignore AL>1. Returns AL=2 (logical drives). |
| **19** | Get drive | AL=`cur_drive` (0=A, 1=B) |
| 09 | Write string | DS:DX, ends with `$` |
| 25 | Set vector | AL=int, DS:DX=handler |
| 30 | Get version | AL=7, AH=10 (7.10) |
| 35 | Get vector | AL=int → ES:BX |
| **0F** | FCB open | DS:DX FCB; AL=00h ok / FFh fail. RECSIZ=128 (MS-DOS 1.25). |
| **10** | FCB close | DS:DX FCB; AL=00h/FFh |
| **11** | FCB find first | DS:DX unopened FCB (`?` wildcards; extended `FFh`+attr). AL=00h/FFh. DTA = drive + 32-byte FAT dirent (extended: 7-byte prefix first). Volume attr `08h` → none (no labels). |
| **12** | FCB find next | Continue AH=11. AL=00h/FFh |
| **14** | FCB sequential read | DTA; AL=0 full / 1 EOF / 3 partial |
| **21** | FCB random read | RR; does not increment RR |
| **27** | FCB random block | CX=records; RR = last+1 |
| 3D | Open | AL=0 read-only, DS:DX ASCIIZ 8.3 (`B:FILE.COM`) → AX=handle (5–8) |
| 3E | Close | BX=handle |
| 3F | Read | BX=handle, CX=bytes, DS:DX buf → AX=count |
| **42** | LSEEK | AL=0/1/2 SET/CUR/END, BX=handle, CX:DX offset → DX:AX pos |
| **1A** | Set DTA | DS:DX = disk transfer area |
| **1B** | Get FAT (cur) | AL=spc (2), CX=512, DX=clusters (354), DS:BX → media `F8h` (HDD stub; no FAT). TANK.COM uses `[bx]==F8` to skip key-disk INT 13 on the current floppy. |
| **1C** | Get FAT (DL) | Same stub as 1B (DL ignored) |
| **4E** | Find first | CX=attr, DS:DX filespec (`*.*`, `B:*.*`) |
| **4F** | Find next | continue search |
| **4B** | Exec | AL=0..3 load+run **COM or MZ EXE** (AL>3 → AX=1 CF). Full payload read required. Relocs; ES:BX EPB (minimal). |
| **48** | Allocate | BX=paragraphs → AX=segment; fail CF AX=8 BX=largest free |
| **49** | Free | ES=block (payload segment) |
| **4A** | Resize | ES=block, BX=new paragraphs (shrink/grow if coalesced). Coalesce keeps `'M'` on owned blocks (do not mark COM as `'Z'`). |
| 4C | Terminate | return to parent if AH=4B active; else halt |

## MCB arena (M8)

- First MCB at segment **0x3000** (above kernel @0x1000 and fixed COM PSP @0x2000)
- Top exclusive **0xA000**
- Classic 16-byte MCB: type `M`/`Z`, owner, size in paragraphs
- `current_psp` set to COM PSP on enter

See agent brief: `/tmp/grok-1000/floppyos-m8-mcb-brief.md`
