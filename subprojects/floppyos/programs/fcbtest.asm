; fcbtest.com — FCB open TINY.COM, recsize after open, AH=21 then AH=27
        cpu     8086
        bits    16
        org     0x100
        mov     ah, 0x1A
        mov     dx, dta
        int     0x21
        mov     ah, 0x0F
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        cmp     word [fcb + 0x0E], 128
        jne     fail
        mov     word [fcb + 0x0E], 1
        mov     ah, 0x21
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        cmp     byte [dta], 0xB8
        jne     fail
        cmp     word [fcb + 0x21], 0
        jne     fail
        mov     ah, 0x27
        mov     cx, 1
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        cmp     word [fcb + 0x21], 1
        jne     fail
        mov     dx, msg_ok
        mov     ah, 0x09
        int     0x21
        mov     ax, 0x4C00
        int     0x21
fail:
        mov     dx, msg_bad
        mov     ah, 0x09
        int     0x21
        mov     ax, 0x4C01
        int     0x21
fcb:
        db      0
        db      'TINY    '
        db      'COM'
        times   25 db 0
dta:
        times   16 db 0
msg_ok: db      "FCB OK", 13, 10, "$"
msg_bad: db     "FCB FAIL", 13, 10, "$"
