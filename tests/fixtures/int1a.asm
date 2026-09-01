; int1a.com — INT 1A AH=00 must return BDA 0040:006C, not C clock().
        org     0x100
        mov     ax, 0x0040
        mov     ds, ax
        mov     word [0x6C], 0x1234
        mov     word [0x6E], 0x0005
        xor     ax, ax
        int     0x1A
        cmp     dx, 0x1234
        jne     fail
        cmp     cx, 0x0005
        jne     fail
        int     0x20
fail:
        mov     ax, 0x4CFF
        int     0x21
