; fcbopen.com — INT 21 AH=0Fh FCB open of TINY.COM (same fixtures dir).
        org     0x100
        mov     ah, 0x0F
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        mov     ax, 0x4C00
        int     0x21
fail:
        mov     ax, 0x4C01
        int     0x21
fcb:
        db      0
        db      'TINY    '
        db      'COM'
