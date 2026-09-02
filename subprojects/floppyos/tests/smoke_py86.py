#!/usr/bin/env python3
"""Boot FloppyOS under local Py86 to A> with hard wall-clock timeouts."""
from __future__ import annotations

import os
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PY86_DIR = ROOT / "subprojects" / "py86"
CONFIG = ROOT / "tests" / "py86_floppyos.json"
SOCK = Path(os.environ.get("FLOPPYOS_PY86_SOCK", "/tmp/floppyos-py86.sock"))
IMG = ROOT / "build" / "floppyos.img"
PYTHON = "/mnt/python/bin/python" if Path("/mnt/python/bin/python").is_file() else "python3"

# Per-stage: (needle, max_steps, wall_seconds)
CHAIN = [
    ("FloppyOS OK", 5_000_000, 300),
    ("FlopFS superblock OK", 3_000_000, 200),
    ("loading COM", 15_000_000, 500),
    ("jumping to kernel", 5_000_000, 200),
    ("FloppyOS kernel", 3_000_000, 200),
    ("INT21 OK", 2_000_000, 150),
    ("starting COMMAND", 3_000_000, 200),
    ("FloppyOS COMMAND", 5_000_000, 300),
    ("A>", 3_000_000, 200),
]


def _read_dot(sock: socket.socket) -> str:
    buf = b""
    while b"\n.\n" not in buf and not buf.rstrip().endswith(b"."):
        chunk = sock.recv(8192)
        if not chunk:
            break
        buf += chunk
    return buf.decode("utf-8", errors="replace")


def ctl(sock: socket.socket, cmd: str, timeout_s: float) -> str:
    sock.settimeout(timeout_s)
    sock.sendall((cmd + "\n").encode())
    return _read_dot(sock)


def main() -> int:
    if not (PY86_DIR / "py86.py").is_file():
        print(f"missing {PY86_DIR}", file=sys.stderr)
        return 1
    if not IMG.is_file():
        print(f"missing {IMG}", file=sys.stderr)
        return 1
    if SOCK.exists():
        try:
            SOCK.unlink()
        except OSError:
            pass

    proc = subprocess.Popen(
        [
            PYTHON, "py86.py",
            "--config", str(CONFIG.resolve()),
            "--display", "none", "--fast-post",
            "--control-socket", str(SOCK),
            "--steps", "0", "--no-screen",
        ],
        cwd=str(PY86_DIR),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        for _ in range(100):
            if SOCK.exists():
                break
            time.sleep(0.1)
            if proc.poll() is not None:
                return 1
        else:
            return 1

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(str(SOCK))
        print(_read_dot(sock).strip()[:50], flush=True)

        for needle, steps, wall in CHAIN:
            print(f">>> {needle!r} (steps<={steps}, wall<={wall}s)", flush=True)
            t0 = time.time()
            try:
                r = ctl(sock, f"until {needle} {steps}", float(wall))
            except (socket.timeout, TimeoutError, OSError) as e:
                print(f"TIMEOUT/ERR {e}", flush=True)
                try:
                    print(ctl(sock, "mda", 15), flush=True)
                except OSError:
                    pass
                return 2
            print(f"    {r.strip()[:180]} ({time.time()-t0:.1f}s)", flush=True)
            if "FOUND" not in r:
                try:
                    print(ctl(sock, "mda", 15), flush=True)
                except OSError:
                    pass
                return 1

        print(ctl(sock, "mda", 15), flush=True)
        print(">>> type FCBTEST (FCB open/read)", flush=True)
        try:
            ctl(sock, "type FCBTEST\r", 30)
            r = ctl(sock, "until FCB OK 8000000", 180)
            print(r.strip()[:180], flush=True)
            if "FOUND" not in r:
                print(ctl(sock, "mda", 15), flush=True)
                return 1
        except (socket.timeout, TimeoutError, OSError) as e:
            print(f"FCBTEST TIMEOUT/ERR {e}", flush=True)
            try:
                print(ctl(sock, "mda", 15), flush=True)
            except OSError:
                pass
            return 1
        try:
            ctl(sock, "quit", 10)
        except OSError:
            pass
        print("smoke_py86: PASS — FloppyOS A> + FCB OK on local Py86", flush=True)
        return 0
    finally:
        if proc.poll() is None:
            proc.kill()
        if SOCK.exists():
            try:
                SOCK.unlink()
            except OSError:
                pass


if __name__ == "__main__":
    sys.exit(main())
