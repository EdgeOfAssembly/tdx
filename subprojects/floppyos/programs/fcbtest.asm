; fcbtest.com — FCB open TINY.COM, recsize after open, AH=21 then AH=27
; Matches MS-DOS 1.25 COMMAND TYPE (OPEN, RECLEN=1, RR=0, RDBLK) plus AH=21.
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
        jne     fail_open
        cmp     word [fcb + 0x0E], 128
        jne     fail_rsz
        mov     word [fcb + 0x0E], 1
        xor     ax, ax
        mov     word [fcb + 0x21], ax
        mov     word [fcb + 0x23], ax   ; COMMAND TYPE zeros RR after OPEN
        mov     ah, 0x21
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail_ah21
        cmp     byte [dta], 0xB8
        jne     fail_dta
        cmp     word [fcb + 0x21], 0
        jne     fail_rr21
        mov     ah, 0x27
        mov     cx, 1
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail_ah27
        cmp     word [fcb + 0x21], 1
        jne     fail_rr27
        mov     dx, msg_ok
        mov     ah, 0x09
        int     0x21
        mov     ax, 0x4C00
        int     0x21
fail_open:
        mov     dx, m_open
        jmp     fail_out
fail_rsz:
        mov     dx, m_rsz
        jmp     fail_out
fail_ah21:
        mov     dl, al
        add     dl, '0'
        mov     ah, 0x02
        int     0x21
        mov     dx, m_ah21
        jmp     fail_out
fail_dta:
        mov     dx, m_dta
        jmp     fail_out
fail_rr21:
        mov     dx, m_rr21
        jmp     fail_out
fail_ah27:
        mov     dl, al
        add     dl, '0'
        mov     ah, 0x02
        int     0x21
        mov     dx, m_ah27
        jmp     fail_out
fail_rr27:
        mov     dx, m_rr27
fail_out:
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
m_open: db      "FCB FAIL open", 13, 10, "$"
m_rsz:  db      "FCB FAIL recsize", 13, 10, "$"
m_ah21: db      "FCB FAIL AH21", 13, 10, "$"
m_dta:  db      "FCB FAIL DTA", 13, 10, "$"
m_rr21: db      "FCB FAIL RR21", 13, 10, "$"
m_ah27: db      "FCB FAIL AH27", 13, 10, "$"
m_rr27: db      "FCB FAIL RR27", 13, 10, "$"
