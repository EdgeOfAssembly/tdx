; fcbread.com — FCB open must clear leftover seq/random pointers (DOS AH=0Fh).
; Dirty current-block/recsize/current-record/random-rec, open TINY.COM, then
; sequential AH=14 with recsize 1 must yield the first byte (B8h). Without the
; open reset, AH=14 seeks to leftover record 99 and hits EOF.
        org     0x100
        mov     ah, 0x1A
        mov     dx, dta
        int     0x21
        mov     ah, 0x0F
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        cmp     word [fcb + 0x0C], 0    ; current block cleared
        jne     fail
        cmp     word [fcb + 0x0E], 128  ; DOS sets recsize 80h
        jne     fail
        cmp     byte [fcb + 0x20], 0    ; current record cleared
        jne     fail
        mov     word [fcb + 0x0E], 1    ; record size 1
        mov     ah, 0x14
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        cmp     byte [dta], 0xB8        ; tiny.com starts MOV AX,imm16
        jne     fail
        int     0x20
fail:
        mov     ax, 0x4CFF
        int     0x21
fcb:
        db      0
        db      'TINY    '
        db      'COM'
        dw      0x1234                  ; current block (must be cleared)
        dw      1                       ; leftover recsize
        dd      0xFFFFFFFF              ; leftover file size
        dw      0, 0                    ; date/time
        times   8 db 0xAA               ; reserved
        db      99                      ; current record (must be cleared)
        dd      0xDEADBEEF              ; random record (must be cleared)
dta:
        times   16 db 0
