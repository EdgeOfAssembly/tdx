; ud.com — NOP, F1, INT 20. iron86 treats F1 as NOP (not #UD); INT 20 halts.
        org     0x100
        nop
        db      0xF1
        int     0x20
