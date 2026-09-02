; fcbah21.com — recsize after open; AH=21 does not bump RR; AH=27 does (MS-DOS 1.25).
        org     0x100
        mov     ah, 0x1A
        mov     dx, dta
        int     0x21
        mov     ah, 0x0F
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        mov     word [fcb + 0x0E], 1    ; recsize after open (tiny.com byte 0 = B8h)
        mov     ah, 0x21
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        cmp     byte [dta], 0xB8
        jne     fail
        cmp     word [fcb + 0x21], 0    ; AH=21 leaves RR at last record (0)
        jne     fail
        mov     ah, 0x27
        mov     cx, 1
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        cmp     word [fcb + 0x21], 1    ; AH=27 RR = last+1
        jne     fail
        int     0x20
fail:
        mov     ax, 0x4CFF
        int     0x21
fcb:
        db      0
        db      'TINY    '
        db      'COM'
        times   25 db 0                  ; through RR 4th byte (FCB+24h)
dta:
        times   16 db 0
