; int10m4.com — set CGA 04h, AH=0Fh must report 40 columns.
        org     0x100
        mov     ax, 0x0004
        int     0x10
        mov     ah, 0x0F
        int     0x10
        int     0x20
