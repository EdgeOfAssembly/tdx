; far.com — lcall 1000:helper (PSP CS). After tracing into it, AX becomes 0xAABB.
        org     0x100
        xor     ax, ax
        db      0x9A
        dw      helper
        dw      0x1000
        int     0x20
helper:
        mov     ax, 0xAABB
        retf
