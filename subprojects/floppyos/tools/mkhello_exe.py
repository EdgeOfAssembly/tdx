#!/usr/bin/env python3
"""Build programs/hello.exe — minimal MZ for FloppyOS M11 smoke."""
from __future__ import annotations
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ASM = r"""
        bits 16
        cpu 8086
start:
        mov     ax, cs
        mov     ds, ax
        mov     dx, msg
        mov     ah, 0x09
        int     0x21
        mov     ax, 0x4C00
        int     0x21
msg:    db "Hello EXE", 13, 10, "$"
"""

def main() -> None:
    with tempfile.TemporaryDirectory() as td:
        td_path = Path(td)
        asm_path = td_path / "p.asm"
        bin_path = td_path / "p.bin"
        asm_path.write_text(ASM)
        subprocess.check_call(["nasm", "-f", "bin", "-o", str(bin_path), str(asm_path)])
        payload = bin_path.read_bytes()
    if len(payload) % 16:
        payload += b"\x00" * (16 - len(payload) % 16)
    header_paras = 4
    header_size = header_paras * 16
    total_file = header_size + len(payload)
    e_cblp = total_file % 512
    e_cp = (total_file + 511) // 512
    hdr = bytearray(header_size)
    struct.pack_into(
        "<2sHHHHHHHHHHHHH",
        hdr,
        0,
        b"MZ",
        e_cblp,
        e_cp,
        0,
        header_paras,
        16,
        0xFFFF,
        0,
        0xFFFE,
        0,
        0,
        0,
        28,
        0,
    )
    out = ROOT / "programs" / "hello.exe"
    out.write_bytes(bytes(hdr) + payload)
    print(f"wrote {out} ({out.stat().st_size} bytes)")

if __name__ == "__main__":
    main()
