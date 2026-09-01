; int10.com — BIOS text: mode 03, cursor 0,0, write 'A' attr 07, INT20.
        org     0x100
        mov     ax, 0x0003
        int     0x10
        mov     ah, 0x02
        xor     bx, bx
        xor     dx, dx
        int     0x10
        mov     ax, 0x0941      ; AH=09 AL='A'
        mov     bx, 0x0007
        mov     cx, 1
        int     0x10
        int     0x20
