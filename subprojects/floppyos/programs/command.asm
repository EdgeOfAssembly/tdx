        cpu 8086
; FloppyOS COMMAND.COM (M12) — interactive prompt + DIR/TYPE/VER/EXIT
; Also runs a short demo if first command is empty? No — pure interactive.
; Automation: QEMU monitor sendkey or serial (kernel COM1 fallback).
        bits 16
        org  0x100

start:
        mov     dx, msg_banner
        mov     ah, 0x09
        int     0x21

        mov     dx, 0x80
        mov     ah, 0x1A
        int     0x21

.main:
        mov     dx, msg_prompt
        mov     ah, 0x09
        int     0x21

        ; AH=0A buffered line
        mov     dx, linebuf
        mov     ah, 0x0A
        int     0x21

        ; NUL-terminate at count
        mov     si, linebuf
        mov     cl, [si+1]
        xor     ch, ch
        mov     di, si
        add     di, 2
        add     di, cx
        mov     byte [di], 0
        mov     si, linebuf+2

        call    skip_sp
        cmp     byte [si], 0
        je      .main
        cmp     byte [si], 13
        je      .main

        ; EXIT?
        mov     di, cmd_exit
        call    match_cmd
        jc      .do_exit

        ; DIR?
        mov     di, cmd_dir
        call    match_cmd
        jc      .do_dir

        ; VER?
        mov     di, cmd_ver
        call    match_cmd
        jc      .do_ver

        ; TYPE x?
        mov     di, cmd_type
        call    match_cmd
        jc      .do_type

        ; HELLO / run COM or EXE by name if ends with .COM/.EXE
        call    try_exec
        jnc     .main

        mov     dx, msg_bad
        mov     ah, 0x09
        int     0x21
        jmp     .main

.do_exit:
        mov     dx, msg_bye
        mov     ah, 0x09
        int     0x21
        mov     ax, 0x4C00
        int     0x21

.do_ver:
        mov     ah, 0x30
        int     0x21
        mov     [ver_maj], al
        mov     [ver_min], ah
        mov     dx, msg_ver
        mov     ah, 0x09
        int     0x21
        mov     dl, [ver_maj]
        add     dl, '0'
        mov     ah, 0x02
        int     0x21
        mov     dl, '.'
        mov     ah, 0x02
        int     0x21
        mov     al, [ver_min]
        cmp     al, 10
        jb      .v1
        mov     dl, '1'
        mov     ah, 0x02
        int     0x21
        mov     dl, '0'
        mov     ah, 0x02
        int     0x21
        jmp     .v2
.v1:    add     al, '0'
        mov     dl, al
        mov     ah, 0x02
        int     0x21
.v2:    mov     dx, msg_crlf
        mov     ah, 0x09
        int     0x21
        jmp     .main

.do_dir:
        mov     dx, msg_dir
        mov     ah, 0x09
        int     0x21
        mov     dx, pattern
        xor     cx, cx
        mov     ah, 0x4E
        int     0x21
        jc      .dir_done
.dir_lp:
        mov     dx, 0x80+0x1E
        call    print_asz
        mov     dx, msg_crlf
        mov     ah, 0x09
        int     0x21
        mov     ah, 0x4F
        int     0x21
        jnc     .dir_lp
.dir_done:
        jmp     .main

.do_type:
        call    skip_sp
        cmp     byte [si], 0
        je      .type_usage
        ; SI -> filename
        mov     dx, si
        mov     ax, 0x3D00
        int     0x21
        jc      .type_err
        mov     [handle], ax
.ty_lp:
        mov     bx, [handle]
        mov     cx, 1
        mov     dx, onebuf
        mov     ah, 0x3F
        int     0x21
        jc      .ty_cl
        cmp     ax, 1
        jne     .ty_cl
        mov     dl, [onebuf]
        mov     ah, 0x02
        int     0x21
        jmp     .ty_lp
.ty_cl:
        mov     bx, [handle]
        mov     ah, 0x3E
        int     0x21
        mov     dx, msg_crlf
        mov     ah, 0x09
        int     0x21
        jmp     .main
.type_usage:
        mov     dx, msg_type_u
        mov     ah, 0x09
        int     0x21
        jmp     .main
.type_err:
        mov     dx, msg_nofile
        mov     ah, 0x09
        int     0x21
        jmp     .main

; try exec SI as path; CF=0 success, CF=1 not attempted/fail
; If no '.', try NAME.COM then NAME.EXE (MS-DOS COMMAND).
try_exec:
        push    si
        mov     bx, si
.te1:   mov     al, [bx]
        test    al, al
        jz      .te_nodot
        cmp     al, '.'
        je      .te_dot
        inc     bx
        jmp     .te1
.te_dot:
        mov     dx, si
        call    .te_4b
        pop     si
        ret
.te_nodot:
        mov     di, exec_path
        mov     bx, si
.te_cp: mov     al, [bx]
        test    al, al
        jz      .te_com
        cmp     al, 13
        je      .te_com
        mov     [di], al
        inc     bx
        inc     di
        jmp     .te_cp
.te_com:
        mov     byte [di], '.'
        mov     byte [di+1], 'C'
        mov     byte [di+2], 'O'
        mov     byte [di+3], 'M'
        mov     byte [di+4], 0
        mov     dx, exec_path
        call    .te_4b
        jnc     .te_ok
        mov     byte [di+1], 'E'
        mov     byte [di+2], 'X'
        mov     byte [di+3], 'E'
        mov     dx, exec_path
        call    .te_4b
.te_ok: pop     si
        ret
.te_4b:
        push    cs
        pop     es
        mov     bx, epb
        mov     ax, 0x4B00
        int     0x21
        ret
        ret

; DI -> uppercase cmd keyword (ASCIIZ), SI -> line
; CF=1 if match (and SI advanced past cmd)
match_cmd:
        push    ax
        push    bx
        push    si
        mov     bx, si
.mc:
        mov     al, [di]
        test    al, al
        jz      .mc_end
        mov     ah, [bx]
        call    up_ah
        cmp     al, ah
        jne     .mc_no
        inc     di
        inc     bx
        jmp     .mc
.mc_end:
        ; next must be NUL, CR, space, or tab
        mov     al, [bx]
        test    al, al
        jz      .mc_yes
        cmp     al, 13
        je      .mc_yes
        cmp     al, ' '
        je      .mc_sp
        cmp     al, 9
        je      .mc_sp
        jmp     .mc_no
.mc_sp:
        inc     bx
.mc_yes:
        ; update SI on stack... use returned SI via [sp]
        pop     ax                      ; old si discard
        mov     si, bx
        pop     bx
        pop     ax
        stc
        ret
.mc_no:
        pop     si
        pop     bx
        pop     ax
        clc
        ret

up_ah:
        cmp     ah, 'a'
        jb      .u
        cmp     ah, 'z'
        ja      .u
        sub     ah, 32
.u:     ret

skip_sp:
.sp:    mov     al, [si]
        cmp     al, ' '
        je      .s1
        cmp     al, 9
        je      .s1
        ret
.s1:    inc     si
        jmp     .sp

print_asz:
        push    ax
        push    si
        mov     si, dx
.pa:    lodsb
        test    al, al
        jz      .pd
        mov     dl, al
        mov     ah, 0x02
        int     0x21
        jmp     .pa
.pd:    pop     si
        pop     ax
        ret

epb:    dw 0, 0, 0, 0, 0, 0, 0
exec_path: times 16 db 0
handle: dw 0
onebuf: db 0
ver_maj: db 0
ver_min: db 0
pattern: db "*.*", 0

linebuf:
        db 80                   ; max
        db 0                    ; count
        times 82 db 0

cmd_exit: db "EXIT", 0
cmd_dir:  db "DIR", 0
cmd_ver:  db "VER", 0
cmd_type: db "TYPE", 0

msg_banner: db "FloppyOS COMMAND", 13, 10
            db "Commands: DIR TYPE VER EXIT  (or run FILE.COM/.EXE)", 13, 10, "$"
msg_prompt: db "A> ", "$"
msg_dir:    db 13, 10, "$"
msg_ver:    db "FloppyOS version $"
msg_bad:    db "Bad command", 13, 10, "$"
msg_bye:    db "Goodbye", 13, 10, "$"
msg_type_u: db "TYPE file", 13, 10, "$"
msg_nofile: db "File not found", 13, 10, "$"
msg_crlf:   db 13, 10, "$"
