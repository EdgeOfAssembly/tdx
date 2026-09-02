; FloppyOS boot 360K — load stage1.5 (LBA3-4 -> 0800:0) + superblock (LBA1 -> 0900:0)
        cpu 8086
        bits 16
        org  0x7C00

start:
        jmp     short main
        nop

oem_name:       db "FloppyOS"
bytes_per_sec:  dw 512
sec_per_clust:  db 1
reserved_secs:  dw 1
num_fats:       db 0
root_entries:   dw 0
total_secs16:   dw 720
media:          db 0xFD
fat_secs16:     dw 0
secs_per_track: dw 9
num_heads:      dw 2
hidden_secs:    dd 0
total_secs32:   dd 0
drive_num:      db 0
reserved1:      db 0
boot_sig:       db 0x29
vol_id:         dd 0x464C4F50
vol_label:      db "FloppyOS   "
fs_type:        db "FLOPFS  "

main:
        cli
        xor     ax, ax
        mov     ds, ax
        mov     es, ax
        mov     ss, ax
        mov     sp, 0x7C00
        sti
        mov     [boot_drive], dl

        mov     si, msg_boot
        call    print_str

        mov     dl, [boot_drive]
        xor     ah, ah
        int     0x13

        ; --- superblock LBA1 = C0 H0 S2 -> 0900:0000 ---
        mov     ax, 0x0900
        mov     es, ax
        xor     bx, bx
        mov     cx, 0x0002              ; cyl 0, sec 2
        mov     dh, 0
        mov     dl, [boot_drive]
        mov     ax, 0x0201
        int     0x13
        jc      .retry_sb
        jmp     .sb_ok
.retry_sb:
        xor     ah, ah
        mov     dl, [boot_drive]
        int     0x13
        mov     ax, 0x0201
        mov     cx, 0x0002
        mov     dh, 0
        mov     dl, [boot_drive]
        mov     bx, 0
        mov     es, bx
        mov     ax, 0x0900
        mov     es, ax
        xor     bx, bx
        mov     ax, 0x0201
        int     0x13
        jc      load_fail
.sb_ok:
        ; verify magic FLOPFS01
        cmp     word [es:0], 0x4C46
        jne     load_fail

        mov     si, msg_sb
        call    print_str

        ; --- stage1.5 LBA3-4 = S4,S5 -> 0800:0000 ---
        mov     ax, 0x0800
        mov     es, ax
        xor     bx, bx
        mov     cl, 4
        mov     ch, 0
        mov     dh, 0
        mov     di, 2
.ld:
        push    di
        mov     dl, [boot_drive]
        mov     ax, 0x0201
        int     0x13
        pop     di
        jnc     .ok
        push    di
        xor     ah, ah
        mov     dl, [boot_drive]
        int     0x13
        mov     ax, 0x0201
        mov     dl, [boot_drive]
        int     0x13
        pop     di
        jc      load_fail
.ok:
        add     bx, 512
        inc     cl
        dec     di
        jnz     .ld

        cmp     byte [es:0], 0xFA
        jne     load_fail

        mov     si, msg_go
        call    print_str
        mov     dl, [boot_drive]
        jmp     0x0800:0x0000

load_fail:
        mov     si, msg_fail
        call    print_str
.hang:  hlt
        jmp     .hang

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

msg_boot: db "FloppyOS OK", 13, 10, 0
msg_sb:   db "SB loaded", 13, 10, 0
msg_go:   db "loading stage1.5...", 13, 10, 0
msg_fail: db "boot: disk error", 13, 10, 0
boot_drive: db 0

        times 510 - ($ - $$) db 0
        dw 0xAA55
