; iflag.com — AH=4B child must start with IF=1 (kernel sti before retf)
        cpu     8086
        bits    16
        org     0x100
        pushf
        pop     ax
        test    ax, 0x0200
        jz      fail
        mov     dx, ok
        mov     ah, 0x09
        int     0x21
        mov     ax, 0x4C00
        int     0x21
fail:
        mov     dx, bad
        mov     ah, 0x09
        int     0x21
        mov     ax, 0x4C01
        int     0x21
ok:     db      "IF OK", 13, 10, "$"
bad:    db      "IF OFF", 13, 10, "$"
