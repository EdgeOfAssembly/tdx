; loop.com — CX=5 loop. Step-over on LOOP leaves AX=5.
        org     0x100
        xor     ax, ax
        mov     cx, 5
again:
        inc     ax
        loop    again
        int     0x20
