; int3pad.com — guest INT3 is padding, not a user breakpoint.
        org     0x100
        int3
        mov     ax, 1
        int     0x20
