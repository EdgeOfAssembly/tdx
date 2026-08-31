; over.com — CALL helper then INT 20. After step-over of CALL, AX=2.
        org     0x100
        xor     ax, ax
        call    helper
        int     0x20
helper:
        inc     ax
        inc     ax
        ret
