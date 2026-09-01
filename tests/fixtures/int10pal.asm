; int10pal.com — mode 04h then INT 10 AH=0Bh BH=0 BL=0Dh (magenta background).
        org     0x100
        mov     ax, 0x0004
        int     0x10
        mov     ax, 0x0B00
        mov     bx, 0x000D
        int     0x10
        int     0x20
