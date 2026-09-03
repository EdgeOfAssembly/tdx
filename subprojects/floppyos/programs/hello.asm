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

        ; --- M8: allocate 16 paragraphs, SETBLOCK shrink, then ALLOC again ---
        mov     bx, 16
        mov     ah, 0x48
        int     0x21
        jc      .memfail
        mov     es, ax
        mov     byte [es:0], 0xA5
        cmp     byte [es:0], 0xA5
        jne     .memfail
        mov     bx, 8
        mov     ah, 0x4A
        int     0x21
        jc      .memfail
        push    es
        mov     bx, 256
        mov     ah, 0x48
        int     0x21
        jc      .memfail
        mov     es, ax
        mov     ah, 0x49
        int     0x21
        pop     es
        jc      .memfail
        mov     ah, 0x49
        int     0x21
        jc      .memfail
        mov     dx, msg_mem
        mov     ah, 0x09
        int     0x21

        ; --- AH=42 LSEEK on HELLO.COM (Dragon Wars DATA1 needs this) ---
        mov     dx, name_hello
        mov     ax, 0x3D00
        int     0x21
        jc      .seekfail
        mov     bx, ax
        xor     cx, cx
        xor     dx, dx
        mov     ax, 0x4200
        int     0x21
        jc      .seekfail
        or      dx, ax
        jnz     .seekfail
        xor     cx, cx
        mov     dx, 2
        mov     ax, 0x4200
        int     0x21
        jc      .seekfail
        cmp     ax, 2
        jne     .seekfail
        or      dx, dx
        jnz     .seekfail
        mov     ah, 0x3E
        int     0x21
        mov     dx, msg_seek
        mov     ah, 0x09
        int     0x21
        jmp     .exit

.seekfail:
        mov     dx, msg_seekfail
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
msg_seek:    db "SEEK OK", 13, 10, "$"
msg_seekfail:db "SEEK FAIL", 13, 10, "$"
name_hello:  db "HELLO.COM", 0
maj: db 0
min: db 0
