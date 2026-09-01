; out3d9.com — OUT 3D9h,35h must land in BDA 0040:0066.
        org     0x100
        mov     ax, 0x0004
        int     0x10
        mov     dx, 0x3D9
        mov     al, 0x35
        out     dx, al
        int     0x20
