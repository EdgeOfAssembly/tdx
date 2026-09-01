; ud.com — NOP then invalid FF /7 so Unicorn stops with a CPU fault.
        org     0x100
        nop
        db      0xFF, 0xFF
        int     0x20
