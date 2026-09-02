; memw.com — one guest store to DS:0200 then INT20 (for BPM write-watch tests).
        org     0x100
        mov     byte [0x0200], 0xAA
        int     0x20
