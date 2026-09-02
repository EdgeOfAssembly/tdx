; FloppyOS kernel M8 — FlopFS files + MCB (AH=48/49/4A)
        cpu 8086
        bits 16
        org  0

SPT             equ 9                  ; 360K 5.25"
HEADS           equ 2
SB_SEG          equ 0x0900
MAX_HANDLES     equ 4
HANDLE_BASE     equ 5
ROOT_ENTRIES    equ 16
ARENA_FIRST     equ 0x3000
ARENA_TOP       equ 0xA000

kernel_entry:
        mov     [cs:boot_drive], dl
        cli
        mov     ax, cs
        mov     ds, ax
        mov     es, ax
        mov     ss, ax
        mov     sp, 0xFFFE
        mov     [cs:current_psp], ax
        sti
        call    serial_init
        call    install_int21
        call    mcb_init
        ; Banner BEFORE any further disk I/O (fs_init can hang on picky FDC)
        mov     dx, msg_banner
        mov     ah, 0x09
        int     0x21
        mov     dx, msg_int21
        mov     ah, 0x09
        int     0x21
        ; fs_init deferred: root dir INT13 can hang on 5150 FDC after many reads.
        ; Init COM via superblock com_lba fallback (SB still at 0900:0).
        mov     byte [cs:fs_ok], 0
        call    enter_preloaded_com
        mov     ax, 0x4C00
        int     0x21
.hang:  hlt
        jmp     .hang

mcb_init:
        push    ax
        push    es
        mov     ax, ARENA_FIRST
        mov     [cs:first_mcb], ax
        mov     es, ax
        mov     byte [es:0], 'Z'
        mov     word [es:1], 0
        mov     ax, ARENA_TOP
        sub     ax, ARENA_FIRST
        dec     ax
        mov     [es:3], ax
        pop     es
        pop     ax
        ret

; BX=paragraphs -> AX=seg or CF AX=8 BX=max free
dos_alloc:
        push    cx
        push    dx
        push    si
        push    di
        push    bp
        push    es
        mov     [cs:al_need], bx
        mov     word [cs:al_best], 0
        mov     ax, [cs:first_mcb]
        mov     es, ax
.w:     mov     al, [es:0]
        cmp     al, 'M'
        je      .sig
        cmp     al, 'Z'
        je      .sig
        mov     ax, 7                   ; arena trashed
        mov     bx, [cs:al_best]
        stc
        jmp     .out
.sig:   cmp     word [es:1], 0
        jne     .nxt
        call    mcb_coalesce_es
        mov     ax, [es:3]
        cmp     ax, [cs:al_best]
        jbe     .cmp
        mov     [cs:al_best], ax
.cmp:   cmp     ax, [cs:al_need]
        jb      .nxt
        mov     bx, [cs:al_need]
        mov     cx, ax
        sub     cx, bx
        cmp     cx, 2
        jb      .take
        mov     [es:3], bx
        mov     dl, [es:0]
        mov     byte [es:0], 'M'
        mov     ax, es
        add     ax, bx
        inc     ax
        push    es
        mov     es, ax
        mov     [es:0], dl
        mov     word [es:1], 0
        dec     cx
        mov     [es:3], cx
        pop     es
.take:  mov     ax, [cs:current_psp]
        mov     [es:1], ax
        mov     ax, es
        inc     ax
        clc
        jmp     .out
.nxt:   cmp     byte [es:0], 'Z'
        je      .fail
        mov     ax, es
        add     ax, [es:3]
        inc     ax
        mov     es, ax
        jmp     .w
.fail:  mov     ax, 8
        mov     bx, [cs:al_best]
        stc
.out:   pop     es
        pop     bp
        pop     di
        pop     si
        pop     dx
        pop     cx
        ret

; ES = MCB, coalesce following free blocks into ES
mcb_coalesce_es:
        push    ax
        push    bx
        push    cx
        push    es
.co:    cmp     byte [es:0], 'Z'
        je      .cdone
        mov     ax, es
        add     ax, [es:3]
        inc     ax
        push    es
        mov     es, ax
        cmp     word [es:1], 0
        jne     .cno
        mov     bx, [es:3]
        inc     bx
        mov     cl, [es:0]
        pop     es
        add     [es:3], bx
        cmp     cl, 'Z'
        jne     .co
        mov     byte [es:0], 'Z'
        jmp     .co
.cno:   pop     es
.cdone: pop     es
        pop     cx
        pop     bx
        pop     ax
        ret

; ES=payload -> free MCB at ES-1
dos_free:
        push    bx
        push    cx
        push    es
        mov     ax, es
        dec     ax
        mov     es, ax
        mov     bl, [es:0]
        cmp     bl, 'M'
        je      .ok
        cmp     bl, 'Z'
        je      .ok
        mov     ax, 9
        stc
        jmp     .df
.ok:    mov     word [es:1], 0
        call    mcb_coalesce_es
        xor     ax, ax
        clc
.df:    pop     es
        pop     cx
        pop     bx
        ret

; AH=4A resize: ES=payload, BX=new paras
dos_realloc:
        push    cx
        push    dx
        push    si
        push    es
        mov     [cs:al_need], bx
        mov     ax, es
        dec     ax
        mov     es, ax
        mov     bl, [es:0]
        cmp     bl, 'M'
        je      .rok
        cmp     bl, 'Z'
        je      .rok
        mov     ax, 9
        stc
        jmp     .rout
.rok:   call    mcb_coalesce_es
        mov     ax, [es:3]
        mov     bx, [cs:al_need]
        cmp     bx, ax
        ja      .grow
        ; shrink
        mov     cx, ax
        sub     cx, bx
        cmp     cx, 1
        jb      .set                    ; equal size
        mov     [es:3], bx
        mov     dl, [es:0]
        mov     byte [es:0], 'M'
        mov     ax, es
        add     ax, bx
        inc     ax
        push    es
        mov     es, ax
        mov     [es:0], dl
        mov     word [es:1], 0
        dec     cx                      ; free payload paras (may be 0)
        mov     [es:3], cx
        pop     es
        jmp     .set
.grow:  ; need more - only if next free (already coalesced)
        cmp     ax, bx
        jae     .set
        mov     ax, 8
        mov     bx, [es:3]
        stc
        jmp     .rout
.set:   mov     ax, [cs:al_need]
        mov     [es:3], ax
        xor     ax, ax
        clc
.rout:  pop     es
        pop     si
        pop     dx
        pop     cx
        ret

fs_init:
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    ds
        push    es
        mov     byte [cs:fs_ok], 0
        mov     ax, SB_SEG
        mov     es, ax
        cmp     word [es:0], 0x4C46
        jne     .fd
        mov     ax, [es:66]
        mov     [cs:root_lba], ax
        test    ax, ax
        jz      .fd
        mov     ax, SB_SEG
        mov     ds, ax
        mov     si, 72
        push    cs
        pop     es
        mov     di, init_fcb
        mov     cx, 11
        rep     movsb
        push    cs
        pop     ds
        push    cs
        pop     es
        mov     bx, root_buf
        mov     ax, [cs:root_lba]
        call    disk_read
        jc      .fd
        mov     byte [cs:fs_ok], 1
.fd:    pop     es
        pop     ds
        pop     di
        pop     si
        pop     bp
        pop     bx
        pop     dx
        pop     cx
        pop     ax
        ret

run_init_com:
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    ds
        push    es
        push    cs
        pop     ds
        mov     dx, msg_com
        mov     ah, 0x09
        int     0x21
        call    fcb_to_path
        mov     dx, path_buf
        mov     ax, 0x3D00
        int     0x21
        jc      .fb
        mov     [cs:tmp_handle], ax
        mov     ax, SB_SEG
        mov     es, ax
        mov     ax, [es:64]
        test    ax, ax
        jnz     .ps
        mov     ax, 0x2000
.ps:    mov     [cs:psp_seg], ax
        call    clear_psp
        mov     ax, [cs:psp_seg]
        mov     ds, ax
        mov     dx, 0x0100
        mov     cx, 0x6000
        mov     bx, [cs:tmp_handle]
        mov     ah, 0x3F
        int     0x21
        jc      .cl
        test    ax, ax
        jz      .cl
        mov     bx, [cs:tmp_handle]
        mov     ah, 0x3E
        int     0x21
        push    cs
        pop     ds
        mov     dx, msg_byname
        mov     ah, 0x09
        int     0x21
        call    enter_com
        jmp     .out
.cl:    mov     bx, [cs:tmp_handle]
        mov     ah, 0x3E
        int     0x21
.fb:    push    cs
        pop     ds
        mov     dx, msg_fallback
        mov     ah, 0x09
        int     0x21
        call    load_com_fallback
.out:   mov     ax, cs
        mov     ds, ax
        mov     es, ax
        mov     ss, ax
        mov     sp, 0xFFFE
        pop     es
        pop     ds
        pop     di
        pop     si
        pop     bp
        pop     bx
        pop     dx
        pop     cx
        pop     ax
        ret

clear_psp:
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    es
        mov     ax, [cs:psp_seg]
        mov     es, ax
        xor     di, di
        mov     cx, 128
        xor     ax, ax
        rep     stosw
        mov     byte [es:0], 0xCD
        mov     byte [es:1], 0x20
        mov     word [es:2], 0x9FFF
        mov     byte [es:0x50], 0xCD
        mov     byte [es:0x51], 0x21
        mov     byte [es:0x52], 0xCB
        mov     byte [es:0x80], 0
        mov     byte [es:0x81], 0x0D
        pop     es
        pop     di
        pop     si
        pop     bp
        pop     bx
        pop     dx
        pop     cx
        pop     ax
        ret

enter_com:
        mov     ax, [cs:psp_seg]
        mov     [cs:current_psp], ax
        mov     [cs:dta_seg], ax
        mov     word [cs:dta_off], 0x80
        mov     ds, ax
        mov     es, ax
        mov     ss, ax
        mov     sp, 0xFFFE
        push    ax
        mov     ax, 0x0100
        push    ax
        xor     ax, ax
        xor     bx, bx
        xor     cx, cx
        xor     dx, dx
        xor     si, si
        xor     di, di
        mov     dl, [cs:boot_drive]
        retf

load_com_fallback:
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    es
        mov     ax, SB_SEG
        mov     es, ax
        mov     cx, [es:62]
        jcxz    .x
        mov     [cs:com_secs], cx
        mov     ax, [es:58]
        mov     [cs:com_lba], ax
        mov     ax, [es:64]
        test    ax, ax
        jnz     .p
        mov     ax, 0x2000
.p:     mov     [cs:psp_seg], ax
        call    clear_psp
        mov     es, ax
        mov     bx, 0x0100
        mov     cx, [cs:com_secs]
        mov     si, [cs:com_lba]
.l:     push    cx
        push    bx
        mov     ax, si
        call    disk_read
        pop     bx
        pop     cx
        jc      .x
        inc     si
        add     bx, 512
        loop    .l
        call    enter_com
.x:     pop     es
        pop     di
        pop     si
        pop     bp
        pop     bx
        pop     dx
        pop     cx
        pop     ax
        ret

fcb_to_path:
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    es
        push    cs
        pop     es
        mov     di, path_buf
        mov     si, init_fcb
        mov     cx, 8
.a:     lodsb
        cmp     al, ' '
        je      .b
        stosb
.b:     loop    .a
        mov     al, '.'
        stosb
        mov     si, init_fcb+8
        mov     cx, 3
.c:     lodsb
        cmp     al, ' '
        je      .d
        stosb
.d:     loop    .c
        xor     al, al
        stosb
        pop     es
        pop     di
        pop     si
        pop     bp
        pop     bx
        pop     dx
        pop     cx
        pop     ax
        ret

disk_read:
        push    ax
        push    bx
        push    cx
        push    dx
        mov     [cs:dr_bx], bx
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
        mov     dl, [cs:boot_drive]
        mov     bx, [cs:dr_bx]
        mov     ax, 0x0201
        int     0x13
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        ret

; COM image already at [SB com_load_seg]:0100 (stage1.5). Build PSP and far-jump.
enter_preloaded_com:
        push    ax
        push    es
        push    di
        push    cx
        mov     ax, 0x0900
        mov     es, ax
        mov     ax, [es:64]             ; com_load_seg
        test    ax, ax
        jnz     .ps
        mov     ax, 0x2000
.ps:    mov     [cs:psp_seg], ax
        ; clear PSP 256 bytes
        mov     es, ax
        xor     di, di
        mov     cx, 128
        xor     ax, ax
        rep     stosw
        mov     byte [es:0], 0xCD
        mov     byte [es:1], 0x20
        mov     word [es:2], 0x9FFF
        mov     byte [es:0x50], 0xCD
        mov     byte [es:0x51], 0x21
        mov     byte [es:0x52], 0xCB
        mov     byte [es:0x80], 0
        mov     byte [es:0x81], 0x0D
        mov     dx, msg_precom
        mov     ah, 0x09
        int     0x21
        ; jump to PSP:0100
        mov     ax, [cs:psp_seg]
        mov     [cs:current_psp], ax
        mov     [cs:dta_seg], ax
        mov     word [cs:dta_off], 0x80
        mov     ds, ax
        mov     es, ax
        mov     ss, ax
        mov     sp, 0xFFFE
        push    ax
        mov     ax, 0x0100
        push    ax
        xor     ax, ax
        xor     bx, bx
        xor     cx, cx
        xor     dx, dx
        mov     dl, [cs:boot_drive]
        retf


install_int21:
        push    ax
        push    bx
        push    ds
        xor     ax, ax
        mov     ds, ax
        mov     bx, 0x21*4
        mov     word [bx], int21_handler
        mov     [bx+2], cs
        pop     ds
        pop     bx
        pop     ax
        ret

int21_handler:
        sti
        cmp     ah, 0x01
        je      i01
        cmp     ah, 0x02
        je      i02
        cmp     ah, 0x07
        je      i07
        cmp     ah, 0x08
        je      i08
        cmp     ah, 0x09
        je      i09
        cmp     ah, 0x0A
        je      i0a
        cmp     ah, 0x0B
        je      i0b
        cmp     ah, 0x0F
        je      i0f
        cmp     ah, 0x10
        je      i10
        cmp     ah, 0x14
        je      i14
        cmp     ah, 0x21
        je      i21
        cmp     ah, 0x27
        je      i27
        cmp     ah, 0x25
        je      i25
        cmp     ah, 0x30
        je      i30
        cmp     ah, 0x35
        je      i35
        cmp     ah, 0x1A
        je      i1a
        cmp     ah, 0x3D
        je      i3d
        cmp     ah, 0x3E
        je      i3e
        cmp     ah, 0x3F
        je      i3f
        cmp     ah, 0x4E
        je      i4e
        cmp     ah, 0x4F
        je      i4f
        cmp     ah, 0x48
        je      i48
        cmp     ah, 0x49
        je      i49
        cmp     ah, 0x4A
        je      i4a
        cmp     ah, 0x4B
        je      i4b
        cmp     ah, 0x4C
        je      i4c
        mov     ax, 1
        stc
        iret
i01:    call    con_getkey
        call    putc                    ; echo
        clc
        iret
i02:    push    ax
        mov     al, dl
        call    putc
        pop     ax
        clc
        iret
i07:    call    con_getkey              ; raw, no echo
        clc
        iret
i08:    call    con_getkey              ; no echo
        clc
        iret
i09:    push    ax
        push    si
        mov     si, dx
.j:     lodsb
        cmp     al, '$'
        je      .k
        call    putc
        jmp     .j
.k:     pop     si
        pop     ax
        clc
        iret
i25:    push    ax
        push    bx
        push    es
        xor     bx, bx
        mov     es, bx
        mov     bl, al
        xor     bh, bh
        shl     bx, 1
        shl     bx, 1
        mov     [es:bx], dx
        mov     [es:bx+2], ds
        pop     es
        pop     bx
        pop     ax
        clc
        iret
i30:    mov     ax, 0x0A07
        xor     bx, bx
        xor     cx, cx
        clc
        iret
i35:    push    ax
        xor     bx, bx
        mov     es, bx
        mov     bl, al
        xor     bh, bh
        shl     bx, 1
        shl     bx, 1
        les     bx, [es:bx]
        pop     ax
        clc
        iret
i0a:    call    dos_buffered_input
        clc
        iret
i0b:    call    con_key_ready
        mov     al, 0
        jz      .i0b0
        mov     al, 0xFF
.i0b0:  clc
        iret
i0f:    call    fcb_open
        jmp     iret_cf
i10:    call    fcb_close
        jmp     iret_cf
i14:    mov     word [cs:fcb_nrec], 1
        mov     byte [cs:fcb_mode], 0        ; sequential
        call    fcb_read
        jmp     iret_cf
i21:    mov     word [cs:fcb_nrec], 1
        mov     byte [cs:fcb_mode], 1        ; random, no RR bump
        call    fcb_read
        jmp     iret_cf
i27:    mov     [cs:fcb_nrec], cx
        mov     byte [cs:fcb_mode], 2        ; block, RR = last+1
        call    fcb_read
        jmp     iret_cf
i1a:    mov     [cs:dta_seg], ds
        mov     [cs:dta_off], dx
        clc
        jmp     iret_cf
i3d:    call    dos_open
        jmp     iret_cf
i3e:    call    dos_close
        jmp     iret_cf
i3f:    call    dos_read
        jmp     iret_cf
i4e:    call    dos_findfirst
        jmp     iret_cf
i4f:    call    dos_findnext
        jmp     iret_cf
i48:    call    dos_alloc
        jmp     iret_cf
i49:    call    dos_free
        jmp     iret_cf
i4a:    call    dos_realloc
        jmp     iret_cf
i4b:    cmp     al, 0
        jne     .i4b_bad
        ; save parent INT return frame (SP -> IP,CS,FLAGS of INT 21h)
        mov     ax, [cs:current_psp]
        mov     [cs:parent_psp], ax
        mov     [cs:parent_ss], ss
        mov     [cs:parent_sp], sp
        mov     ax, [cs:dta_seg]
        mov     [cs:parent_dta_seg], ax
        mov     ax, [cs:dta_off]
        mov     [cs:parent_dta_off], ax
        call    dos_exec
        ; returns only on error (CF=1)
        jmp     iret_cf
.i4b_bad:
        mov     ax, 1
        stc
        jmp     iret_cf

i4c:    cmp     byte [cs:exec_active], 0
        je      .i4c_halt
        ; --- return to parent after AH=4B ---
        mov     byte [cs:exec_active], 0
        ; free child block (PSP segment from alloc)
        mov     ax, [cs:child_psp]
        test    ax, ax
        jz      .i4c_rest
        mov     es, ax
        call    dos_free
.i4c_rest:
        mov     ax, [cs:parent_psp]
        mov     [cs:current_psp], ax
        mov     ax, [cs:parent_dta_seg]
        mov     [cs:dta_seg], ax
        mov     ax, [cs:parent_dta_off]
        mov     [cs:dta_off], ax
        cli
        mov     ss, [cs:parent_ss]
        mov     sp, [cs:parent_sp]
        ; restore parent data segments (IRET does not)
        mov     ax, [cs:parent_psp]
        mov     ds, ax
        mov     es, ax
        sti
        xor     ax, ax
        clc
        jmp     iret_cf
.i4c_halt:
        cli
.z:     hlt
        jmp     .z

; Propagate CF into flags image that IRET will restore (INT stack: IP,CS,FLAGS)
iret_cf:
        push    bp
        mov     bp, sp
        jc      .setc
        and     byte [bp+6], 0xFE
        jmp     .go
.setc:  or      byte [bp+6], 0x01
.go:    pop     bp
        iret



; ---- AH=4B execute COM or MZ EXE (M10/M11) ----
dos_exec:
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    bp
        push    es
        push    ds

        push    ds
        push    dx
        call    dos_open
        pop     dx
        pop     ds
        jc      ex_nf
        mov     [cs:ex_handle], ax

        mov     bx, ax
        sub     bx, HANDLE_BASE
        mov     cl, 4
        shl     bx, cl
        add     bx, handles
        mov     ax, [cs:bx+6]
        mov     [cs:ex_fsize], ax
        mov     [cs:ex_hptr], bx

        mov     word [cs:bx+10], 0
        mov     word [cs:bx+12], 0
        push    cs
        pop     ds
        mov     dx, ex_hdr
        mov     cx, 2
        mov     bx, [cs:ex_handle]
        call    dos_read
        jc      ex_bad
        cmp     word [cs:ex_hdr], 0x5A4D
        je      ex_mz
        cmp     word [cs:ex_hdr], 0x4D5A
        je      ex_mz

        ; --- COM ---
        mov     bx, [cs:ex_hptr]
        mov     word [cs:bx+10], 0
        mov     word [cs:bx+12], 0
        mov     ax, [cs:ex_fsize]
        mov     bx, ax
        add     bx, 0x100+0x200+15
        mov     cl, 4
        shr     bx, cl
        cmp     bx, 0x40
        jae     ex_com_par
        mov     bx, 0x40
ex_com_par:
        call    dos_alloc
        jc      ex_nomem_h
        mov     [cs:child_psp], ax
        call    setup_psp
        mov     ax, [cs:child_psp]
        mov     ds, ax
        mov     dx, 0x0100
        mov     cx, [cs:ex_fsize]
        test    cx, cx
        jnz     ex_com_rd
        mov     cx, 1
ex_com_rd:
        mov     bx, [cs:ex_handle]
        call    dos_read
        jc      ex_fail_free
        mov     bx, [cs:ex_handle]
        call    dos_close
        mov     ax, [cs:child_psp]
        mov     [cs:ex_ss], ax
        mov     word [cs:ex_sp], 0xFFFE
        mov     [cs:ex_cs], ax
        mov     word [cs:ex_ip], 0x0100
        jmp     ex_run

ex_mz:
        mov     bx, [cs:ex_hptr]
        mov     word [cs:bx+10], 0
        mov     word [cs:bx+12], 0
        push    cs
        pop     ds
        mov     dx, ex_hdr
        mov     cx, 32
        mov     bx, [cs:ex_handle]
        call    dos_read
        jc      ex_bad
        mov     ax, [cs:ex_hdr+8]
        mov     [cs:ex_cparhdr], ax
        mov     ax, [cs:ex_hdr+6]
        mov     [cs:ex_crlc], ax
        mov     ax, [cs:ex_hdr+10]
        mov     [cs:ex_minalloc], ax
        mov     ax, [cs:ex_hdr+14]
        mov     [cs:ex_ss_rel], ax
        mov     ax, [cs:ex_hdr+16]
        mov     [cs:ex_sp], ax
        mov     ax, [cs:ex_hdr+20]
        mov     [cs:ex_ip], ax
        mov     ax, [cs:ex_hdr+22]
        mov     [cs:ex_cs_rel], ax
        mov     ax, [cs:ex_hdr+24]
        mov     [cs:ex_lfarlc], ax
        mov     ax, [cs:ex_hdr+4]
        mov     bx, 512
        mul     bx
        mov     cx, [cs:ex_hdr+2]
        test    cx, cx
        jz      ex_fsz
        sub     ax, 512
        add     ax, cx
ex_fsz:
        mov     [cs:ex_fsize], ax
        mov     ax, [cs:ex_cparhdr]
        mov     cl, 4
        shl     ax, cl
        mov     [cs:ex_hdrbytes], ax
        mov     bx, [cs:ex_fsize]
        sub     bx, ax
        mov     [cs:ex_loadsize], bx
        mov     ax, bx
        add     ax, 15
        mov     cl, 4
        shr     ax, cl
        add     ax, 16
        add     ax, [cs:ex_minalloc]
        mov     bx, ax
        call    dos_alloc
        jc      ex_nomem_h
        mov     [cs:child_psp], ax
        call    setup_psp
        mov     ax, [cs:child_psp]
        add     ax, 0x10
        mov     [cs:ex_loadseg], ax
        mov     bx, [cs:ex_hptr]
        mov     ax, [cs:ex_hdrbytes]
        mov     [cs:bx+10], ax
        mov     word [cs:bx+12], 0
        mov     ax, [cs:ex_loadseg]
        mov     ds, ax
        xor     dx, dx
        mov     cx, [cs:ex_loadsize]
        mov     bx, [cs:ex_handle]
        call    dos_read
        jc      ex_fail_free
        mov     cx, [cs:ex_crlc]
        jcxz    ex_mz_ok
        mov     bx, [cs:ex_hptr]
        mov     ax, [cs:ex_lfarlc]
        mov     [cs:bx+10], ax
        mov     word [cs:bx+12], 0
        cmp     cx, 64
        jbe     ex_rlc
        mov     cx, 64
ex_rlc:
        mov     ax, cx
        shl     ax, 1
        shl     ax, 1
        push    cx
        mov     cx, ax
        push    cs
        pop     ds
        mov     dx, ex_reloc
        mov     bx, [cs:ex_handle]
        call    dos_read
        pop     cx
        jc      ex_fail_free
        push    cs
        pop     ds
        mov     si, ex_reloc
ex_rel_loop:
        push    cx
        lodsw
        mov     dx, ax
        lodsw
        add     ax, [cs:ex_loadseg]
        mov     es, ax
        mov     bx, dx
        mov     ax, [cs:ex_loadseg]
        add     [es:bx], ax
        pop     cx
        loop    ex_rel_loop
ex_mz_ok:
        mov     bx, [cs:ex_handle]
        call    dos_close
        mov     ax, [cs:ex_loadseg]
        add     ax, [cs:ex_ss_rel]
        mov     [cs:ex_ss], ax
        mov     ax, [cs:ex_loadseg]
        add     ax, [cs:ex_cs_rel]
        mov     [cs:ex_cs], ax
        jmp     ex_run

ex_run:
        mov     byte [cs:exec_active], 1
        mov     ax, [cs:child_psp]
        mov     [cs:current_psp], ax
        mov     [cs:dta_seg], ax
        mov     word [cs:dta_off], 0x80
        mov     ds, ax
        mov     es, ax
        mov     ss, [cs:ex_ss]
        mov     sp, [cs:ex_sp]
        xor     bx, bx
        xor     cx, cx
        xor     dx, dx
        xor     si, si
        xor     di, di
        push    word [cs:ex_cs]
        push    word [cs:ex_ip]
        mov     dl, [cs:boot_drive]
        retf

ex_fail_free:
        mov     bx, [cs:ex_handle]
        call    dos_close
        mov     ax, [cs:child_psp]
        mov     es, ax
        call    dos_free
        mov     ax, 8
        stc
        jmp     ex_out
ex_nomem_h:
        mov     bx, [cs:ex_handle]
        call    dos_close
        mov     ax, 8
        stc
        jmp     ex_out
ex_bad:
        mov     bx, [cs:ex_handle]
        call    dos_close
        mov     ax, 11
        stc
        jmp     ex_out
ex_nf:
        mov     ax, 2
        stc
ex_out:
        pop     ds
        pop     es
        pop     bp
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret

setup_psp:
        push    ax
        push    cx
        push    di
        push    es
        mov     ax, [cs:child_psp]
        mov     es, ax
        xor     di, di
        mov     cx, 128
        xor     ax, ax
        rep     stosw
        mov     byte [es:0], 0xCD
        mov     byte [es:1], 0x20
        mov     word [es:2], 0x9FFF
        mov     byte [es:0x50], 0xCD
        mov     byte [es:0x51], 0x21
        mov     byte [es:0x52], 0xCB
        mov     byte [es:0x80], 0
        mov     byte [es:0x81], 0x0D
        mov     ax, [cs:parent_psp]
        mov     [es:0x16], ax
        pop     es
        pop     di
        pop     cx
        pop     ax
        ret

; ---- FindFirst/FindNext (M9) ----
dos_findfirst:
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    es
        push    ds
        cmp     byte [cs:fs_ok], 1
        je      .havefs
        call    fs_init
        cmp     byte [cs:fs_ok], 1
        jne     .ffe
.havefs:
        mov     si, dx
        call    path_parse_wild
        jc      .ffe
        push    cs
        pop     es
        push    cs
        pop     ds                  ; fcb_tmp / find_pat live in kernel CS
        mov     si, fcb_tmp
        mov     di, find_pat
        mov     cx, 11
        rep     movsb
        mov     word [cs:find_idx], 0
        call    find_scan
        jmp     .ffo
.ffe:   mov     ax, 18
        stc
.ffo:   pop     ds
        pop     es
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret

dos_findnext:
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    es
        push    ds
        cmp     byte [cs:fs_ok], 1
        jne     .fne
        call    find_scan
        jmp     .fno
.fne:   mov     ax, 18
        stc
.fno:   pop     ds
        pop     es
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret

find_scan:
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    es
.fsl:   mov     bx, [cs:find_idx]
        cmp     bx, ROOT_ENTRIES
        jae     .fsn
        mov     ax, bx
        mov     cl, 5
        shl     ax, cl
        mov     di, root_buf
        add     di, ax                  ; cs:di dirent
        inc     word [cs:find_idx]
        cmp     byte [cs:di], 0
        je      .fsn
        mov     si, find_pat
        mov     cx, 11
        push    di
.fsm:   mov     al, [cs:si]
        mov     ah, [cs:di]
        cmp     al, '?'
        je      .fso
        cmp     al, ah
        jne     .fsx
.fso:   inc     si
        inc     di
        loop    .fsm
        pop     di
        call    fill_dta                ; di = dirent
        clc
        jmp     .fsd
.fsx:   pop     di
        jmp     .fsl
.fsn:   mov     ax, 18
        stc
.fsd:   pop     es
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret

; fill DTA from cs:di dirent
fill_dta:
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    ax
        push    cx
        push    dx
        push    bx
        push    bp
        push    si
        push    di
        push    es
        mov     [cs:fd_di], di
        mov     ax, [cs:dta_seg]
        mov     es, ax
        mov     bx, [cs:dta_off]
        push    di
        mov     di, bx
        mov     cx, 21
        xor     al, al
        rep     stosb
        pop     di
        mov     byte [es:bx+0x15], 0x20
        mov     word [es:bx+0x16], 0
        mov     word [es:bx+0x18], 0
        mov     ax, [cs:di+16]
        mov     [es:bx+0x1A], ax
        mov     ax, [cs:di+18]
        mov     [es:bx+0x1C], ax
        ; build NAME.EXT at DTA+1Eh
        lea     si, [di]                ; name 8
        mov     di, bx
        add     di, 0x1E
        mov     cx, 8
.fdn:   mov     al, [cs:si]
        inc     si
        cmp     al, ' '
        je      .fds
        stosb
.fds:   loop    .fdn
        mov     al, '.'
        stosb
        mov     si, [cs:fd_di]
        add     si, 8
        mov     cx, 3
.fde:   mov     al, [cs:si]
        inc     si
        cmp     al, ' '
        je      .fdt
        stosb
.fdt:   loop    .fde
        ; if ext empty, remove dot
        ; simple: always leave NAME.EXT; if no ext chars, backspace dot
        ; check if we wrote only name+dot
        xor     al, al
        stosb
        pop     es
        pop     di
        pop     si
        pop     bp
        pop     bx
        pop     dx
        pop     cx
        pop     ax
        ret

; path_parse_wild: like path_parse but * -> ? fill
path_parse_wild:
        push    ax
        push    cx
        push    dx
        push    di
        push    es
        push    cs
        pop     es
        mov     di, fcb_tmp
        mov     cx, 11
        mov     al, ' '
        rep     stosb
        mov     di, fcb_tmp
.w0:    mov     al, [si]
        test    al, al
        jz      .wok
        cmp     al, 0x5C
        je      .wsk
        cmp     al, '/'
        je      .wsk
        jmp     .wn
.wsk:   inc     si
        jmp     .w0
.wn:    mov     cx, 8
.wn1:   mov     al, [si]
        test    al, al
        jz      .wok
        cmp     al, '.'
        je      .wex
        cmp     al, '*'
        je      .wstar_n
        call    toupper
        cmp     al, '?'
        je      .wq
        stosb
        inc     si
        loop    .wn1
        jmp     .wn2
.wq:    stosb
        inc     si
        loop    .wn1
        jmp     .wn2
.wstar_n:
        inc     si
        mov     al, '?'
        rep     stosb
        ; skip to dot or end
.wn2:   mov     al, [si]
        test    al, al
        jz      .wok
        cmp     al, '.'
        je      .wex
        inc     si
        jmp     .wn2
.wex:   inc     si
        mov     di, fcb_tmp+8
        mov     cx, 3
.we1:   mov     al, [si]
        test    al, al
        jz      .wok
        cmp     al, '*'
        je      .wstar_e
        call    toupper
        stosb
        inc     si
        loop    .we1
        jmp     .wok
.wstar_e:
        mov     al, '?'
        rep     stosb
.wok:   clc
        jmp     .wout
.wout:  pop     es
        pop     di
        pop     dx
        pop     cx
        pop     ax
        ret

path_parse:
        push    ax
        push    cx
        push    di
        push    es
        push    cs
        pop     es
        mov     di, fcb_tmp
        mov     cx, 11
        mov     al, ' '
        rep     stosb
        mov     di, fcb_tmp
.s0:    mov     al, [si]
        test    al, al
        jz      .bad
        cmp     al, 0x5C
        je      .sk
        cmp     al, '/'
        je      .sk
        jmp     .nm
.sk:    inc     si
        jmp     .s0
.nm:    mov     cx, 8
.n1:    mov     al, [si]
        test    al, al
        jz      .ok
        cmp     al, '.'
        je      .ex
        call    toupper
        stosb
        inc     si
        loop    .n1
.n2:    mov     al, [si]
        test    al, al
        jz      .ok
        cmp     al, '.'
        je      .ex
        inc     si
        jmp     .n2
.ex:    inc     si
        mov     di, fcb_tmp+8
        mov     cx, 3
.e1:    mov     al, [si]
        test    al, al
        jz      .ok
        call    toupper
        stosb
        inc     si
        loop    .e1
.ok:    clc
        jmp     .out
.bad:   stc
.out:   pop     es
        pop     di
        pop     cx
        pop     ax
        ret
toupper:
        cmp     al, 'a'
        jb      .r
        cmp     al, 'z'
        ja      .r
        sub     al, 32
.r:     ret

dos_open:
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    es
        push    ds
        cmp     byte [cs:fs_ok], 1
        je      .havefs
        call    fs_init
        cmp     byte [cs:fs_ok], 1
        jne     .err
.havefs:
        mov     si, dx
        call    path_parse
        jc      .err
        push    cs
        pop     ds                  ; fcb_tmp is in kernel CS
        xor     bx, bx
.srch:  cmp     bx, ROOT_ENTRIES
        jae     .err
        mov     ax, bx
        mov     cl, 5
        shl     ax, cl
        mov     di, root_buf
        add     di, ax
        cmp     byte [cs:di], 0
        je      .err
        push    bx
        push    cs
        pop     es
        mov     si, fcb_tmp
        mov     cx, 11
        push    di
        repe    cmpsb
        pop     di
        pop     bx
        je      .hit
        inc     bx
        jmp     .srch
.hit:   xor     si, si
.hf:    cmp     si, MAX_HANDLES
        jae     .err
        mov     ax, si
        mov     cl, 4
        shl     ax, cl
        mov     bx, handles
        add     bx, ax
        cmp     byte [cs:bx], 0
        je      .got
        inc     si
        jmp     .hf
.got:   mov     byte [cs:bx], 1
        mov     ax, [cs:di+12]
        mov     [cs:bx+2], ax
        mov     ax, [cs:di+14]
        mov     [cs:bx+4], ax
        mov     ax, [cs:di+16]
        mov     [cs:bx+6], ax
        mov     ax, [cs:di+18]
        mov     [cs:bx+8], ax
        mov     word [cs:bx+10], 0
        mov     word [cs:bx+12], 0
        mov     ax, [cs:di+20]
        mov     [cs:bx+14], ax
        mov     ax, si
        add     ax, HANDLE_BASE
        clc
        jmp     .ok
.err:   mov     ax, 2
        stc
.ok:    pop     ds
        pop     es
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret

dos_close:
        push    bx
        push    si
        mov     ax, bx
        sub     ax, HANDLE_BASE
        cmp     ax, MAX_HANDLES
        jae     .e
        mov     cl, 4
        shl     ax, cl
        mov     si, handles
        add     si, ax
        cmp     byte [cs:si], 0
        je      .e
        mov     byte [cs:si], 0
        xor     ax, ax
        clc
        jmp     .d
.e:     mov     ax, 6
        stc
.d:     pop     si
        pop     bx
        ret

dos_read:
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    es
        push    ds
        mov     [cs:rd_ds], ds
        mov     [cs:rd_dx], dx
        mov     [cs:rd_want], cx
        mov     word [cs:rd_got], 0
        mov     ax, bx
        sub     ax, HANDLE_BASE
        cmp     ax, MAX_HANDLES
        jae     .re
        mov     cl, 4
        shl     ax, cl
        add     ax, handles
        mov     [cs:rd_hp], ax
        mov     bx, ax
        cmp     byte [cs:bx], 0
        je      .re
.loop:  mov     ax, [cs:rd_want]
        sub     ax, [cs:rd_got]
        jz      .rok
        mov     si, [cs:rd_hp]
        mov     bx, [cs:si+6]
        sub     bx, [cs:si+10]
        jz      .rok
        cmp     ax, bx
        jbe     .n1
        mov     ax, bx
.n1:    mov     [cs:rd_left], ax
        mov     ax, [cs:si+10]
        mov     bx, ax
        and     bx, 511
        mov     [cs:rd_off], bx
        mov     cl, 9
        shr     ax, cl
        add     ax, [cs:si+2]
        mov     [cs:rd_lba], ax
        mov     ax, 512
        sub     ax, [cs:rd_off]
        cmp     ax, [cs:rd_left]
        jbe     .n2
        mov     ax, [cs:rd_left]
.n2:    mov     [cs:rd_chunk], ax
        push    cs
        pop     es
        mov     bx, sec_buf
        mov     ax, [cs:rd_lba]
        call    disk_read
        jc      .re
        mov     ax, [cs:rd_ds]
        mov     es, ax
        mov     di, [cs:rd_dx]
        push    cs
        pop     ds
        mov     si, sec_buf
        add     si, [cs:rd_off]
        mov     cx, [cs:rd_chunk]
        rep     movsb
        push    cs
        pop     ds
        mov     ax, [cs:rd_chunk]
        add     [cs:rd_dx], ax
        add     [cs:rd_got], ax
        mov     si, [cs:rd_hp]
        add     [cs:si+10], ax
        jmp     .loop
.rok:   mov     ax, [cs:rd_got]
        clc
        jmp     .rd
.re:    xor     ax, ax
        stc
.rd:    pop     ds
        pop     es
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret

; ---- FCB (MS-DOS 1.25): AH=0F/10/14/21/27 ----
; DS:DX = FCB. AL=00h ok / FFh fail (0F/10); AL=0/1/3 (14/21/27).
fcb_open:
        push    bx
        push    cx
        push    si
        push    di
        push    es
        push    ds
        mov     [cs:fcb_ds], ds
        mov     [cs:fcb_dx], dx
        push    cs
        pop     es
        mov     si, dx
        inc     si                      ; skip drive
        mov     di, fcb_tmp
        mov     cx, 11
        rep     movsb
        call    fcb11_to_path
        push    cs
        pop     ds
        mov     dx, path_buf
        call    dos_open
        jc      .fo_fail
        mov     [cs:fcb_hnd], ax
        ; size from handle table
        sub     ax, HANDLE_BASE
        mov     cl, 4
        shl     ax, cl
        add     ax, handles
        mov     si, ax
        mov     ax, [cs:si+6]
        mov     [cs:fcb_szlo], ax
        mov     ax, [cs:si+8]
        mov     [cs:fcb_szhi], ax
        mov     ds, [cs:fcb_ds]
        mov     bx, [cs:fcb_dx]
        xor     ax, ax
        mov     [bx+0x0C], ax           ; EXTENT
        mov     word [bx+0x0E], 128     ; RECSIZ (1.25 OPEN)
        mov     ax, [cs:fcb_szlo]
        mov     [bx+0x10], ax
        mov     ax, [cs:fcb_szhi]
        mov     [bx+0x12], ax
        mov     ax, [cs:fcb_hnd]
        mov     [bx+0x18], al           ; host fd
        mov     byte [bx+0x19], 0xFC
        mov     byte [bx+0x20], 0       ; NR
        mov     word [bx+0x21], 0
        mov     word [bx+0x23], 0       ; RR
        xor     al, al
        clc
        jmp     .fo_out
.fo_fail:
        mov     al, 0xFF
        clc
.fo_out:
        pop     ds
        pop     es
        pop     di
        pop     si
        pop     cx
        pop     bx
        ret

fcb_close:
        push    bx
        cmp     byte [bx+0x19], 0xFC
        jne     .fc_fail
        mov     al, [bx+0x18]
        xor     ah, ah
        push    bx
        mov     bx, ax
        call    dos_close
        pop     bx
        jc      .fc_fail
        mov     byte [bx+0x19], 0
        xor     al, al
        clc
        pop     bx
        ret
.fc_fail:
        mov     al, 0xFF
        clc
        pop     bx
        ret

fcb11_to_path:
        push    ax
        push    cx
        push    si
        push    di
        push    es
        push    cs
        pop     es
        mov     di, path_buf
        mov     si, fcb_tmp
        mov     cx, 8
.n:     lodsb
        cmp     al, ' '
        je      .nd
        stosb
        loop    .n
        jmp     .dot
.nd:    inc     si
        loop    .nd
.dot:   mov     al, '.'
        stosb
        mov     si, fcb_tmp+8
        mov     cx, 3
.e:     lodsb
        cmp     al, ' '
        je      .ed
        stosb
        loop    .e
        jmp     .z
.ed:    inc     si
        loop    .ed
.z:     xor     al, al
        stosb
        pop     es
        pop     di
        pop     si
        pop     cx
        pop     ax
        ret

fcb_read:
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    ds
        mov     [cs:fcb_ds], ds
        mov     [cs:fcb_dx], dx
        mov     bx, dx
        cmp     byte [bx+0x19], 0xFC
        jne     .fr_eof
        mov     ax, [bx+0x0E]
        test    ax, ax
        jnz     .fr_rs
        mov     ax, 128
.fr_rs: mov     [cs:fcb_rsiz], ax
        cmp     byte [cs:fcb_mode], 0
        jne     .fr_rnd
        ; sequential: rec = extent*128 + NR
        mov     ax, [bx+0x0C]
        mov     cx, 128
        mul     cx
        xor     cx, cx
        mov     cl, [bx+0x20]
        add     ax, cx
        adc     dx, 0
        jmp     .fr_have
.fr_rnd:
        mov     ax, [bx+0x21]           ; RR low 16
        xor     dx, dx
.fr_have:
        mov     [cs:fcb_rec], ax
        ; offset = rec * recsiz
        mov     cx, [cs:fcb_rsiz]
        mul     cx                      ; dx:ax = byte offset
        mov     [cs:fcb_offlo], ax
        mov     [cs:fcb_offhi], dx
        mov     al, [bx+0x18]
        xor     ah, ah
        mov     [cs:fcb_hnd], ax
        ; seek handle
        sub     ax, HANDLE_BASE
        cmp     ax, MAX_HANDLES
        jae     .fr_eof
        mov     cl, 4
        shl     ax, cl
        add     ax, handles
        mov     si, ax
        cmp     byte [cs:si], 0
        je      .fr_eof
        mov     ax, [cs:fcb_offlo]
        mov     [cs:si+10], ax
        mov     ax, [cs:fcb_offhi]
        mov     [cs:si+12], ax
        ; bytes = nrec * recsiz
        mov     ax, [cs:fcb_nrec]
        test    ax, ax
        jnz     .fr_n
        mov     ax, 1
.fr_n:  mov     cx, [cs:fcb_rsiz]
        mul     cx
        mov     cx, ax                  ; want (low 16)
        mov     bx, [cs:fcb_hnd]
        mov     ds, [cs:dta_seg]
        mov     dx, [cs:dta_off]
        call    dos_read
        jc      .fr_eof
        ; AX = bytes got
        mov     [cs:fcb_got], ax
        xor     dx, dx
        div     word [cs:fcb_rsiz]
        mov     [cs:fcb_gotrec], ax     ; full records
        mov     ds, [cs:fcb_ds]
        mov     bx, [cs:fcb_dx]
        cmp     word [cs:fcb_got], 0
        je      .fr_eof
        mov     ax, [cs:fcb_got]
        cmp     ax, [cs:fcb_rsiz]
        jb      .fr_part
        ; full at least one; if gotrec < nrec → partial block
        mov     ax, [cs:fcb_gotrec]
        cmp     ax, [cs:fcb_nrec]
        jb      .fr_part
        xor     al, al                  ; AL=0 all records
        jmp     .fr_upd
.fr_part:
        mov     al, 3
        jmp     .fr_upd
.fr_eof:
        mov     ds, [cs:fcb_ds]
        mov     bx, [cs:fcb_dx]
        mov     al, 1
        clc
        jmp     .fr_done
.fr_upd:
        ; sequential: NR += gotrec
        cmp     byte [cs:fcb_mode], 0
        jne     .fr_rr
        mov     cl, [bx+0x20]
        add     cl, [cs:fcb_gotrec]
        mov     [bx+0x20], cl
        jmp     .fr_ok
.fr_rr:
        ; mode 1: RR = last record (rec + gotrec - 1)
        ; mode 2: RR = last+1 (rec + gotrec)
        mov     ax, [cs:fcb_rec]
        add     ax, [cs:fcb_gotrec]
        cmp     byte [cs:fcb_mode], 2
        je      .fr_setrr
        dec     ax
.fr_setrr:
        mov     [bx+0x21], ax
        mov     word [bx+0x23], 0
.fr_ok: clc
.fr_done:
        pop     ds
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret

; ---- console input (M12): INT 16h + COM1 serial fallback ----
; ZF=1 if no key, ZF=0 if key available
con_key_ready:
        push    ax
        mov     ah, 0x01
        int     0x16
        jnz     .ckr_yes
        ; serial data ready? 0xFF = open bus (no 8250 on Py86/5150)
        push    dx
        mov     dx, 0x3FD
        in      al, dx
        pop     dx
        cmp     al, 0xFF
        je      .ckr_no
        test    al, 0x01
        jz      .ckr_no
.ckr_yes:
        pop     ax
        or      ax, ax                  ; clear ZF... need ZF=0
        ; force ZF=0
        test    sp, sp
        ret
.ckr_no:
        pop     ax
        cmp     ax, ax                  ; ZF=1
        ret

; wait for key -> AL=ASCII (AH=scan if from kbd, 0 if serial)
con_getkey:
        push    bx
        push    cx
        push    dx
.cgk_wait:
        mov     ah, 0x01
        int     0x16
        jnz     .cgk_bios
        mov     dx, 0x3FD
        in      al, dx
        cmp     al, 0xFF                ; no UART (open bus)
        je      .cgk_wait
        test    al, 0x01
        jz      .cgk_wait
        mov     dx, 0x3F8
        in      al, dx
        ; map CR
        cmp     al, 10                  ; LF -> CR
        jne     .cgk_s1
        mov     al, 13
.cgk_s1:
        xor     ah, ah
        jmp     .cgk_done
.cgk_bios:
        xor     ah, ah
        int     0x16                    ; AH=00 get key
        ; AL=ascii AH=scan
.cgk_done:
        pop     dx
        pop     cx
        pop     bx
        ret

; AH=0A DS:DX buffer: [0]=max, [1]=count, [2+]=chars + CR
dos_buffered_input:
        push    ax
        push    bx
        push    cx
        push    si
        push    di
        push    es
        mov     si, dx
        mov     bl, [si]                ; max
        test    bl, bl
        jz      .bi_done
        cmp     bl, 1
        jb      .bi_done
        xor     bh, bh
        mov     di, si
        add     di, 2                   ; data start
        xor     cx, cx                  ; count
.bi_loop:
        call    con_getkey
        cmp     al, 13                  ; CR
        je      .bi_cr
        cmp     al, 8                   ; BS
        je      .bi_bs
        cmp     al, 127
        je      .bi_bs
        ; printable?
        cmp     al, 32
        jb      .bi_loop
        ; room? count < max-1 (leave room? DOS allows max chars then ignores)
        mov     ah, bl
        dec     ah                      ; max usable = max (DOS stores up to max)
        cmp     cl, bl
        jae     .bi_loop                ; full
        mov     [di], al
        inc     di
        inc     cx
        call    putc                    ; echo
        jmp     .bi_loop
.bi_bs:
        test    cx, cx
        jz      .bi_loop
        dec     di
        dec     cx
        ; echo BS space BS
        mov     al, 8
        call    putc
        mov     al, ' '
        call    putc
        mov     al, 8
        call    putc
        jmp     .bi_loop
.bi_cr:
        mov     [di], al                ; store CR
        mov     al, 13
        call    putc
        mov     al, 10
        call    putc
        mov     [si+1], cl              ; count
.bi_done:
        pop     es
        pop     di
        pop     si
        pop     cx
        pop     bx
        pop     ax
        ret


putc:
        push    ax
        push    bx
        push    dx
        mov     ah, 0x0E
        mov     bh, 0
        mov     bl, 0x07
        push    ax
        int     0x10
        pop     ax
        call    serial_putc
        pop     dx
        pop     bx
        pop     ax
        ret

serial_init:
        push    ax
        push    dx
        mov     dx, 0x3F9
        xor     ax, ax
        out     dx, al
        mov     dx, 0x3FB
        mov     al, 0x80
        out     dx, al
        mov     dx, 0x3F8
        mov     al, 0x0C
        out     dx, al
        mov     dx, 0x3F9
        xor     al, al
        out     dx, al
        mov     dx, 0x3FB
        mov     al, 0x03
        out     dx, al
        mov     dx, 0x3FA
        mov     al, 0xC7
        out     dx, al
        mov     dx, 0x3FC
        mov     al, 0x0B
        out     dx, al
        pop     dx
        pop     ax
        ret

serial_putc:
        push    ax
        push    dx
        push    cx
        mov     ah, al
        mov     dx, 0x3FD
        mov     cx, 0x2000              ; timeout if no UART (5150/Py86)
.w:     in      al, dx
        test    al, 0x20
        jnz     .ready
        loop    .w
        jmp     .skip
.ready: mov     dx, 0x3F8
        mov     al, ah
        out     dx, al
.skip:  pop     cx
        pop     dx
        pop     ax
        ret

msg_fsinit:   db "fs_init...", 13, 10, "$"
msg_fsdone:   db "fs_done", 13, 10, "$"
msg_precom:   db "starting COMMAND...", 13, 10, "$"
msg_banner:   db "FloppyOS kernel", 13, 10, "$"
msg_int21:    db "INT21 OK", 13, 10, "$"
msg_com:      db "loading COM...", 13, 10, "$"
msg_byname:   db "COM by name OK", 13, 10, "$"
msg_fallback: db "COM fallback LBA", 13, 10, "$"

boot_drive:   db 0
fs_ok:        db 0
        align 2
dta_seg:      dw 0
dta_off:      dw 0
find_idx:     dw 0
exec_active:  db 0
        align 2
parent_psp:   dw 0
parent_ss:    dw 0
parent_sp:    dw 0
parent_dta_seg: dw 0
parent_dta_off: dw 0
child_psp:    dw 0
ex_handle:    dw 0
ex_fsize:     dw 0
ex_hptr:      dw 0
ex_cparhdr:   dw 0
ex_crlc:      dw 0
ex_minalloc:  dw 0
ex_ss_rel:    dw 0
ex_cs_rel:    dw 0
ex_lfarlc:    dw 0
ex_hdrbytes:  dw 0
ex_loadsize:  dw 0
ex_loadseg:   dw 0
ex_ss:        dw 0
ex_sp:        dw 0
ex_cs:        dw 0
ex_ip:        dw 0
ex_hdr:       times 32 db 0
ex_reloc:     times 256 db 0
fd_di:        dw 0
find_pat:     times 11 db 0
current_psp:  dw 0
first_mcb:    dw 0
al_need:      dw 0
al_best:      dw 0
root_lba:     dw 0
psp_seg:      dw 0
com_lba:      dw 0
com_secs:     dw 0
tmp_handle:   dw 0
dr_bx:        dw 0
rd_ds:        dw 0
rd_dx:        dw 0
rd_want:      dw 0
rd_got:       dw 0
rd_left:      dw 0
rd_off:       dw 0
rd_lba:       dw 0
rd_chunk:     dw 0
rd_hp:        dw 0
init_fcb:     times 11 db 0
fcb_tmp:      times 11 db 0
path_buf:     times 16 db 0
fcb_ds:       dw 0
fcb_dx:       dw 0
fcb_hnd:      dw 0
fcb_szlo:     dw 0
fcb_szhi:     dw 0
fcb_nrec:     dw 0
fcb_mode:     db 0
fcb_rsiz:     dw 0
fcb_rec:      dw 0
fcb_offlo:    dw 0
fcb_offhi:    dw 0
fcb_got:      dw 0
fcb_gotrec:   dw 0
handles:      times 64 db 0
root_buf:     times 512 db 0
sec_buf:      times 512 db 0

        times 16384 - ($ - $$) db 0
