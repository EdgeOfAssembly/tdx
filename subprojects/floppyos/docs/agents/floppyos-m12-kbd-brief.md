# FloppyOS M12: INT 21h AH=01/08/0A/0B keyboard input
AH=01: wait key, echo (AH=02), AL=ASCII (ext: AL=0 then next=scan). AH=08: wait, no echo, AL=ASCII. AH=0B: AL=FFh if ready else 00 (nonblock). AH=0A: DS:DX buffer; CF clear on success.
AH=0A buf: [0]=max chars (excludes CR); [1]=count stored; [2..]=data then CR; count excludes CR; stop at max or CR; BS may edit if implemented.
INT 16h under CON: AH=00 block (AL=ASCII,AH=scan); AH=01 peek (ZF=1 empty); optional AH=10/11 enhanced. FloppyOS may wrap BIOS or poll COM1 instead of full kbd IRQ.
COM1 fallback: putc already mirrors VGA→0x3F8; headless `qemu -serial stdio -display none` has no PS/2 keys—poll LSR@3FDh bit0, read RBR@3F8 so host stdin drives AH=01/08/0A.
keypress.py --emulator-mode: X11 US-layout fake keys into GUI guest (needs display). QEMU sendkey: monitor scancodes (awkward with mon:stdio). Serial pipe: host stdin→COM1 — preferred M12 smoke path.
M12 goal: add i01/i08/i0b/i0a in kernel.asm; dual source INT16 then COM1; optional COMMAND A> loop; smoke: type line via serial, expect echo in log.
Status: not implemented (int21_handler lacks 01/08/0A/0B); serial_init/putc exist; update docs/int21.md after land.
