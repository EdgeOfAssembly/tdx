; fcbrecsz.com — MS-DOS 1.25 OPEN always sets recsize to 128.
; Guest recsize 25 before AH=0Fh must be overwritten (set recsize after open).
        org     0x100
        mov     word [fcb + 0x0E], 25
        mov     ah, 0x0F
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        cmp     word [fcb + 0x0E], 128
        jne     fail
        int     0x20
fail:
        mov     ax, 0x4CFF
        int     0x21
fcb:
        db      0
        db      'TINY    '
        db      'COM'
        times   24 db 0
