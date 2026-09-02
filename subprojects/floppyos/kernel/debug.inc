; FloppyOS kernel debug traces (NASM).
;   nasm -DDEBUG   or   make DEBUG=1
; Off by default (no -DDEBUG), same idea as C NDEBUG.
;
; UASM/MASM:  IFDEF DEBUG  ...  ENDIF
; NASM:       %ifdef DEBUG ... %endif
;
; Log line (INT 10h, so iron86 tty / tdx CGA both see it):
;   FloppyOS:<INT1A CX:DX ticks>:<verb> (<AH hex>) <name> AL=<hex>
; Example:
;   FloppyOS:0000002A:OPEN FCB (0F) BUSHIDO.SCR AL=00

%ifndef FLOPPYOS_DEBUG_INC
%define FLOPPYOS_DEBUG_INC

%macro dputc 1
%ifdef DEBUG
        push    ax
        mov     al, %1
        call    putc
        pop     ax
%endif
%endmacro

%macro dputn 1
%ifdef DEBUG
        push    ax
        mov     al, %1
        add     al, '0'
        call    putc
        pop     ax
%endif
%endmacro

%macro dputs 1
%ifdef DEBUG
        push    si
        mov     si, %1
        call    dbg_puts_cs
        pop     si
%endif
%endmacro

; AH=fn, BX=CS verb, SI=CS name, AL=result
%macro dlog_cs 0
%ifdef DEBUG
        call    dbg_line_cs
%endif
%endmacro

; AH=fn, BX=CS verb, SI=DS name (guest ASCIIZ), AL=result
%macro dlog_ds 0
%ifdef DEBUG
        call    dbg_line_ds
%endif
%endmacro

%endif ; FLOPPYOS_DEBUG_INC
