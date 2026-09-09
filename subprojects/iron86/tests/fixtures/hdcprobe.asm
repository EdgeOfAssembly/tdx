; hdcprobe.asm — Xebec S1410 Test Drive Ready (opcode 00) via PIO.
; IBM XT Fixed Disk Adapter: 322h select, 321h status, 320h data.
;
; DCB is six zero bytes: class 0, opcode 00, drive 0.
; Halt codes (AX after HLT):
;   AL=00  completion, error bit clear (ready)
;   AX=FF01  select timeout (no BSY|CD|REQ)
;   AL=FE  completion-phase timeout
;
; Assemble: nasm -f bin -o hdcprobe.com hdcprobe.asm

        org     100h

        mov     dx, 322h
        xor     al, al
        out     dx, al                  ; select pulse

        mov     cx, 1000h
.wait_sel:
        mov     dx, 321h
        in      al, dx
        and     al, 0Dh                 ; REQ|CD|BSY
        cmp     al, 0Dh
        jz      .dcb
        loop    .wait_sel
        mov     ax, 0FF01h
        hlt

.dcb:
        mov     dx, 320h
        xor     al, al
        mov     cx, 6
.send:
        out     dx, al                  ; six-byte DCB, all zeros
        loop    .send

        mov     cx, 1000h
.wait_cmp:
        mov     dx, 321h
        in      al, dx
        mov     ah, al                  ; keep full status in AH
        and     al, 07h                 ; REQ|IO|CD
        cmp     al, 07h
        jz      .read
        loop    .wait_cmp
        mov     al, 0FEh
        hlt

.read:
        mov     dx, 320h
        in      al, dx                  ; completion byte
        hlt
