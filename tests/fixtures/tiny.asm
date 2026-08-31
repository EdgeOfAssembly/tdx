; tiny.com — MOV/INC/ADD then INT 20. AX ends at 4.
        org     0x100
        mov     ax, 1
        inc     ax
        add     ax, 2
        int     0x20
