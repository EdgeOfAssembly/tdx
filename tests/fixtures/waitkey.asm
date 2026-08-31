; waitkey.com — INT 16 AH=00 then DOS exit with AL = ASCII.
        org     0x100
        mov     ah, 0
        int     0x16
        mov     ah, 0x4C
        int     0x21
