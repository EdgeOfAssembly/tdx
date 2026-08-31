; setblock.com — INT 21 AH=4A BX=FFFF must fail with available size in BX.
        org     0x100
        mov     ah, 0x4A
        mov     bx, 0xFFFF
        int     0x21
        int     0x20
