#!/usr/bin/env python3
"""M12 interactive smoke: drive QEMU via monitor sendkey (INT 16h path).

Uses a UNIX monitor socket — no X11 required. Prefer this over keypress.py
for CI; use keypress.py --emulator-mode when you want SDL + real window keys.

  ./tests/smoke_interactive.py [floppyos.img]
"""
from __future__ import annotations

import os
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
QEMU = os.environ.get("QEMU", "qemu-system-i386")
MON = Path(os.environ.get("TMPDIR", "/tmp")) / f"floppyos-mon-{os.getpid()}.sock"
LOG = Path(os.environ.get("TMPDIR", "/tmp")) / f"floppyos-int-{os.getpid()}.log"


def mon_cmd(sock: socket.socket, cmd: str) -> None:
    sock.sendall((cmd + "\n").encode())
    time.sleep(0.05)


def send_text(sock: socket.socket, text: str) -> None:
    """Type text then Enter via QEMU sendkey."""
    special = {
        " ": "spc",
        ".": "dot",
        "/": "slash",
        "\\": "backslash",
        "-": "minus",
        "_": "shift-minus",
        ":": "shift-semicolon",
    }
    for ch in text:
        if ch == "\n" or ch == "\r":
            mon_cmd(sock, "sendkey ret")
            continue
        if ch in special:
            mon_cmd(sock, f"sendkey {special[ch]}")
            continue
        if "A" <= ch <= "Z":
            mon_cmd(sock, f"sendkey shift-{ch.lower()}")
            continue
        if "a" <= ch <= "z" or "0" <= ch <= "9":
            mon_cmd(sock, f"sendkey {ch}")
            continue
        # skip unknown
    mon_cmd(sock, "sendkey ret")
    time.sleep(0.3)


def main() -> int:
    img = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "floppyos.img"
    if not img.is_file():
        print(f"missing image: {img}", file=sys.stderr)
        return 1
    if MON.exists():
        MON.unlink()

    cmd = [
        QEMU,
        "-machine",
        "pc,accel=tcg",
        "-m",
        "16",
        "-drive",
        f"file={img},format=raw,if=floppy,index=0",
        "-boot",
        "a",
        "-display",
        "none",
        "-serial",
        f"file:{LOG}",
        "-monitor",
        f"unix:{MON},server,nowait",
        "-no-reboot",
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        # wait for monitor socket
        for _ in range(50):
            if MON.exists():
                break
            time.sleep(0.1)
        else:
            print("monitor socket not created", file=sys.stderr)
            return 1
        time.sleep(1.5)  # boot to prompt
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(str(MON))
        # drain banner
        sock.settimeout(0.3)
        try:
            while True:
                if not sock.recv(4096):
                    break
        except OSError:
            pass
        sock.settimeout(2.0)

        send_text(sock, "dir")
        time.sleep(0.8)
        send_text(sock, "ver")
        time.sleep(0.5)
        send_text(sock, "exit")
        time.sleep(0.8)
        mon_cmd(sock, "quit")
        sock.close()
        proc.wait(timeout=5)
    finally:
        if proc.poll() is None:
            proc.kill()
        if MON.exists():
            try:
                MON.unlink()
            except OSError:
                pass

    text = LOG.read_text(errors="replace") if LOG.exists() else ""
    # also accept serial file with nulls
    needles = [
        "FloppyOS COMMAND",
        "A>",
        "COMMAND.COM",
        "HELLO.COM",
        "FloppyOS version 7.10",
        "Goodbye",
    ]
    missing = [n for n in needles if n not in text]
    print(text.replace("\x00", ""))
    if missing:
        print("FAIL missing:", ", ".join(missing), file=sys.stderr)
        return 1
    print("smoke_interactive: PASS (monitor sendkey)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
