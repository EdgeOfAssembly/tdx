; cga3da.com — two IN DX,3DAh must differ (retrace toggle).
        org     0x100
        mov     dx, 0x3DA
        in      al, dx
        mov     bl, al
        in      al, dx
        cmp     al, bl
        je      stuck
        int     0x20
stuck:
        mov     ax, 0x4CFF
        int     0x21
