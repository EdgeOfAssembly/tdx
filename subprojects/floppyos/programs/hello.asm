; hello.com — INT 21h demo (M5–M8): strings, version, vectors, MCB alloc
        bits 16
        org  0x100

start:
        mov     dx, msg_hello
        mov     ah, 0x09
        int     0x21

        mov     ah, 0x30
        int     0x21
        mov     [maj], al
        mov     [min], ah

        mov     dx, msg_ver
        mov     ah, 0x09
        int     0x21
        mov     al, [maj]
        add     al, '0'
        mov     dl, al
        mov     ah, 0x02
        int     0x21
        mov     dl, '.'
        int     0x21
        mov     al, [min]
        cmp     al, 10
        jne     .d1
        mov     dl, '1'
        mov     ah, 0x02
        int     0x21
        mov     dl, '0'
        int     0x21
        jmp     .nl
.d1:    add     al, '0'
        mov     dl, al
        mov     ah, 0x02
        int     0x21
.nl:    mov     dl, 13
        mov     ah, 0x02
        int     0x21
        mov     dl, 10
        int     0x21

        mov     ah, 0x35
        mov     al, 0x21
        int     0x21
        mov     dx, msg_vec
        mov     ah, 0x09
        int     0x21

        ; --- M8: allocate 16 paragraphs, touch, free ---
        mov     bx, 16
        mov     ah, 0x48
        int     0x21
        jc      .memfail
        mov     es, ax
        mov     byte [es:0], 0xA5
        cmp     byte [es:0], 0xA5
        jne     .memfail
        mov     ah, 0x49
        int     0x21
        jc      .memfail
        mov     dx, msg_mem
        mov     ah, 0x09
        int     0x21
        jmp     .exit

.memfail:
        mov     dx, msg_memfail
        mov     ah, 0x09
        int     0x21

.exit:  mov     ax, 0x4C00
        int     0x21

msg_hello:   db "Hello COM", 13, 10, "$"
msg_ver:     db "VER $"
msg_vec:     db "VEC21 OK", 13, 10, "$"
msg_mem:     db "MEM OK", 13, 10, "$"
msg_memfail: db "MEM FAIL", 13, 10, "$"
maj: db 0
min: db 0
