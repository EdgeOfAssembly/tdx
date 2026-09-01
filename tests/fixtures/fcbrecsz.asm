; fcbrecsz.com — FCB open must keep a guest recsize set before AH=0Fh.
; Bushido writes 25 into FCB+0Eh then opens BUSHIDO.SCR; clobbering that
; to 128 makes AH=21 return 128-byte slabs instead of 25-byte names.
        org     0x100
        mov     word [fcb + 0x0E], 25
        mov     ah, 0x0F
        mov     dx, fcb
        int     0x21
        cmp     al, 0
        jne     fail
        cmp     word [fcb + 0x0E], 25
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
