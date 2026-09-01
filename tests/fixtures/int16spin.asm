; waitkey.com — spin on INT16 AH=01h (check key), then AH=00h (wait key).
; Exercises handle_int16 both branches in a tight loop to reproduce the
; ASan SEGV at handle_int16 after a reset (uc engine recreated).
[BITS 16]
[ORG 0x100]
start:
    mov ah, 0x01        ; check keystroke (non-blocking)
    int 0x16
    jz .nope            ; ZF=1 => none
    mov ah, 0x00        ; consume
    int 0x16
.nope:
    ; also do a blocking wait occasionally
    mov ah, 0x00
    int 0x16
    jmp start
