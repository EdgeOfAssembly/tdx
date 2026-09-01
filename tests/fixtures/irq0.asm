; irq0.com — hook IRQ0, program PIT ch0 mode 3 reload 64, wait for 2 ticks.
; Proves the 8253 falling-OUT pulse still latches IRR after the first EOI.
        org     0x100
        cli
        xor     ax, ax
        mov     es, ax
        mov     dx, isr
        mov     [es:0x20], dx
        mov     ax, cs
        mov     [es:0x22], ax
        mov     word [counter], 0
        mov     al, 0x36        ; ch0, lobyte/hibyte, mode 3
        out     0x43, al
        mov     al, 64
        out     0x40, al
        xor     al, al
        out     0x40, al
        sti
wait_ticks:
        cmp     word [counter], 2
        jb      wait_ticks
        int     0x20
isr:
        push    ax
        push    ds
        push    cs
        pop     ds
        inc     word [counter]
        mov     al, 0x20
        out     0x20, al
        pop     ds
        pop     ax
        iret
counter:
        dw      0
