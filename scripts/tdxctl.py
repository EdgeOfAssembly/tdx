#!/usr/bin/env python3
"""tdxctl — send one JSON-line command to a running TDX agent socket."""

from __future__ import annotations

import argparse
import json
import socket
import sys
from pathlib import Path

VERSION = "0.2"


def usage() -> None:
    """Print CLI help (also used for no-args)."""
    print(
        """Usage: tdxctl [options] <command> [args…]

  Talk to a running tdx process over its UNIX socket.

Commands:
  step | over | run | stop | regs | disasm | status | cga | quit | help
  mem <seg:off> [len]
  bp <seg:off>
  bpdel <id>
  bplist
  shot
  key <key>

Options:
  -h, --help         Show this help and exit
  -v, --version      Show version and exit
      --sock PATH    Socket path (default: /tmp/tdx.sock)

tdxctl """
        + VERSION
    )


def send(*, sock_path: str, line: str) -> str:
    """Send one line and return the reply."""
    path = Path(sock_path)
    if not path.exists():
        raise FileNotFoundError(f"socket not found: {sock_path}")
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(str(path))
        sock.sendall((line + "\n").encode("utf-8"))
        chunks: list[bytes] = []
        while True:
            buf = sock.recv(4096)
            if not buf:
                break
            chunks.append(buf)
            if b"\n" in buf:
                break
        return b"".join(chunks).decode("utf-8", errors="replace")


def build_line(cmd: str, rest: list[str]) -> str:
    """Turn argv into a JSON command object."""
    if cmd in {"mem"} and rest:
        obj: dict[str, object] = {"cmd": "mem", "addr": rest[0]}
        if len(rest) > 1:
            obj["len"] = int(rest[1], 0)
        return json.dumps(obj)
    if cmd in {"bp", "bp_set"} and rest:
        return json.dumps({"cmd": "bp", "addr": rest[0]})
    if cmd in {"bpdel", "bp_del"} and rest:
        return json.dumps({"cmd": "bpdel", "id": int(rest[0], 0)})
    if cmd == "key" and rest:
        return json.dumps({"cmd": "key", "key": rest[0]})
    return json.dumps({"cmd": cmd})


def main(argv: list[str] | None = None) -> int:
    """CLI entry."""
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-h", "--help", action="store_true")
    parser.add_argument("-v", "--version", action="store_true")
    parser.add_argument("--sock", default="/tmp/tdx.sock")
    parser.add_argument("command", nargs="?")
    parser.add_argument("args", nargs="*")
    ns = parser.parse_args(argv)
    if ns.version:
        print(f"tdxctl {VERSION}")
        return 0
    if ns.help or ns.command is None:
        usage()
        return 0
    try:
        reply = send(sock_path=ns.sock, line=build_line(ns.command, ns.args))
    except OSError as exc:
        print(f"tdxctl: {exc}", file=sys.stderr)
        return 1
    sys.stdout.write(reply)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
