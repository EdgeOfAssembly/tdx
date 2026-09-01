; parsefcb.com — INT 21 AH=29 fills FCB from 'FOO.BAR'.
        org     0x100
        mov     ax, cs
        mov     ds, ax
        mov     es, ax
        mov     si, fname
        mov     di, fcb
        mov     ax, 0x2900
        int     0x21
        int     0x20
fname:  db      'FOO.BAR', 0
fcb:    times   32 db 0
