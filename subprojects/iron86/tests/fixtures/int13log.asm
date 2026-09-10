; int13log.asm — INT 13h AH=00 DL=00 (floppy reset) then HLT.
; Used with iron86 --hdc (pc + bios_int) and --log to prove INT13 lines.
;
; Assemble: nasm -f bin -o int13log.com int13log.asm

        org     100h

        xor     ax, ax                  ; AH=00 reset
        xor     dx, dx                  ; DL=00 floppy
        int     13h
        hlt
