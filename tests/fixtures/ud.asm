; ud.com — NOP then ICEBP (F1); Unicorn #UD, not 8086 FF /7 padding.
        org     0x100
        nop
        db      0xF1
        int     0x20
