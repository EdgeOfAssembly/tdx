; stage1.5 8086 — SB at 0900:0; load kernel + init COM; jump kernel
        cpu 8086
        bits 16
        org  0x8000

SPT     equ 9
HEADS   equ 2

stage15_start:
        cli
        xor     ax, ax
        mov     ds, ax
        mov     es, ax
        mov     ss, ax
        mov     sp, 0x7C00
        sti
        cld
        mov     [boot_drive], dl

        mov     si, msg_hello
        call    print_str

        mov     ax, 0x0900
        mov     es, ax
        cmp     word [es:0], 0x4C46
        je      .m1
        jmp     magic_fail
.m1:    cmp     word [es:2], 0x504F
        je      .m2
        jmp     magic_fail
.m2:    cmp     word [es:4], 0x5346
        je      .m3
        jmp     magic_fail
.m3:    cmp     word [es:6], 0x3130
        je      .m4
        jmp     magic_fail
.m4:
        mov     si, msg_sb_ok
        call    print_str

        ; kernel params
        mov     ax, [es:48]
        mov     [k_lba], ax
        mov     ax, [es:52]
        mov     [k_secs], ax
        mov     ax, [es:54]
        mov     [k_seg], ax
        ; com params
        mov     ax, [es:58]
        mov     [c_lba], ax
        mov     ax, [es:62]
        mov     [c_secs], ax
        mov     ax, [es:64]
        mov     [c_seg], ax

        mov     dl, [boot_drive]
        xor     ah, ah
        int     0x13

        mov     si, msg_loadk
        call    print_str
        mov     ax, [k_seg]
        mov     [rw_seg], ax
        mov     word [rw_off], 0
        mov     ax, [k_lba]
        mov     [rw_lba], ax
        mov     cx, [k_secs]
        call    load_cx_sectors
        jc      disk_fail

        ; Load COM at c_seg:0100 (PSP at c_seg:0000 cleared by kernel)
        mov     ax, [c_secs]
        test    ax, ax
        jz      .no_com
        mov     si, msg_loadc
        call    print_str
        mov     ax, [c_seg]
        ; load at c_seg:0100 => start para = c_seg + 0x10
        add     ax, 0x10
        mov     [rw_seg], ax
        mov     word [rw_off], 0
        mov     ax, [c_lba]
        mov     [rw_lba], ax
        mov     cx, [c_secs]
        call    load_cx_sectors
        jc      disk_fail
        ; flag for kernel: com preloaded
        mov     byte [cs:com_ready], 1
        jmp     .go
.no_com:
        mov     byte [cs:com_ready], 0
.go:
        mov     si, msg_jump
        call    print_str
        mov     dl, [boot_drive]
        mov     ax, [k_seg]
        push    ax
        xor     ax, ax
        push    ax
        retf

; CX = sector count; uses rw_lba, rw_seg, rw_off
load_cx_sectors:
.l:
        push    cx
        call    read_one
        pop     cx
        jc      .lf
        inc     word [rw_lba]
        add     word [rw_seg], 0x20
        loop    .l
        clc
        ret
.lf:    stc
        ret

disk_fail:
        mov     si, msg_disk
        call    print_str
        jmp     halt
magic_fail:
        mov     si, msg_magic
        call    print_str
halt:   hlt
        jmp     halt

read_one:
        push    ax
        push    bx
        push    cx
        push    dx
        push    es
        push    si
        mov     ax, [rw_lba]
        xor     dx, dx
        mov     cx, SPT
        div     cx
        mov     cl, dl
        inc     cl
        xor     dx, dx
        mov     bx, HEADS
        div     bx
        mov     ch, al
        mov     dh, dl
        mov     dl, [boot_drive]
        mov     ax, [rw_seg]
        mov     es, ax
        mov     bx, [rw_off]
        mov     si, 3
.try:
        mov     ax, 0x0201
        int     0x13
        jnc     .ok
        dec     si
        jz      .fail
        push    dx
        mov     dl, [boot_drive]
        xor     ah, ah
        int     0x13
        pop     dx
        jmp     .try
.ok:    clc
        jmp     .out
.fail:  stc
.out:   pop     si
        pop     es
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        ret

print_str:
        push    ax
        push    bx
.ps:    lodsb
        test    al, al
        jz      .pd
        mov     ah, 0x0E
        mov     bh, 0
        mov     bl, 0x07
        int     0x10
        jmp     .ps
.pd:    pop     bx
        pop     ax
        ret

msg_hello: db "FloppyOS stage1.5", 13, 10, 0
msg_sb_ok: db "FlopFS superblock OK", 13, 10, 0
msg_loadk: db "loading kernel...", 13, 10, 0
msg_loadc: db "loading COM...", 13, 10, 0
msg_jump:  db "jumping to kernel", 13, 10, 0
msg_disk:  db "stage1.5: disk error", 13, 10, 0
msg_magic: db "stage1.5: bad magic", 13, 10, 0
boot_drive: db 0
com_ready:  db 0
        align 2
rw_lba: dw 0
rw_seg: dw 0
rw_off: dw 0
k_lba:  dw 0
k_secs: dw 0
k_seg:  dw 0
c_lba:  dw 0
c_secs: dw 0
c_seg:  dw 0
        times 1024 - ($ - $$) db 0
