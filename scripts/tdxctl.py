#!/usr/bin/env python3
"""tdxctl — keep-alive JSON-line client for tdx / tdxview UNIX sockets."""

from __future__ import annotations

import argparse
import json
import socket
import sys
from pathlib import Path

VERSION = "0.9"


def usage() -> None:
    """Print CLI help (also used for no-args)."""
    print(
        """Usage: tdxctl [options] <command> [args…]

  Talk to tdx or tdxview over a keep-alive UNIX socket (no Xmux).

  run / F9 toggle run/pause (same as the CPU F9 key). pause/stop always
  pause; unpause always resumes. delay/faster/slower change F9 slice park
  (same as CPU +/- keys); 0 ms is fastest.

Commands:
  step | over | run | stop | pause | unpause | delay | faster | slower | reset | regs | disasm | status | cga | ping | quit | help
  mem <seg:off> [len]
  bp <seg:off> [once|every|N]
                     exec BP at CS:IP; once = first hit only
  bpint <n> [once|every|N]
                     INT n (hex); default every, once = first only
  bpinsn <pat> [once|every|N]
                     any insn matching Capstone text, e.g. bpinsn int 10
                     bpinsn call   bpinsn out
  bpdel <id>
  bplist             exec + INT + insn breakpoints
  shot [path]        screenshot; stdout = versioned path (Xmux-style timestamp)
  key <key>          DOS INT 16 (starts F9)
  nav <Up|Down|Home|End|PgUp|PgDn>
  delay [N|+|-]      F9 slice park in ms (query / set / slower / faster)
  faster             Decrease delay one 5 ms step (not below 0)
  slower             Increase delay one 5 ms step (cap 200 ms)

Options:
  -h, --help         Show this help and exit
  -v, --version      Show version and exit
      --sock PATH    Socket (default: /tmp/tdx.sock, or /tmp/tdxview.sock with --view)
      --view         Talk to tdxview (game window shot/keys)
      --ctl          Keep-alive: commands on stdin (pipeline FIFO), one connect

tdxctl """
        + VERSION
    )


def _hits_token(tok: str) -> int:
    """once=1, every=0, else integer remaining hits."""
    low = tok.lower()
    if low == "once":
        return 1
    if low == "every":
        return 0
    return int(tok, 0)


def build_line(cmd: str, rest: list[str]) -> str:
    """Turn argv into a JSON command object."""
    if cmd in {"mem"} and rest:
        obj: dict[str, object] = {"cmd": "mem", "addr": rest[0]}
        if len(rest) > 1:
            obj["len"] = int(rest[1], 0)
        return json.dumps(obj)
    if cmd in {"bp", "bp_set"} and rest:
        obj: dict[str, object] = {"cmd": "bp", "addr": rest[0]}
        if len(rest) > 1:
            obj["hits"] = _hits_token(rest[1])
        return json.dumps(obj)
    if cmd in {"bpint", "bp_int"} and rest:
        obj = {"cmd": "bpint", "int": int(rest[0], 16)}
        if len(rest) > 1:
            obj["hits"] = _hits_token(rest[1])
        return json.dumps(obj)
    if cmd in {"bpinsn", "bp_insn"} and rest:
        toks = list(rest)
        hits = 0
        if toks and toks[-1].lower() in {"once", "every"}:
            hits = _hits_token(toks[-1])
            toks = toks[:-1]
        return json.dumps({"cmd": "bpinsn", "pat": " ".join(toks), "hits": hits})
    if cmd in {"bpdel", "bp_del"} and rest:
        return json.dumps({"cmd": "bpdel", "id": int(rest[0], 0)})
    if cmd == "key" and rest:
        return json.dumps({"cmd": "key", "key": rest[0]})
    if cmd == "nav" and rest:
        return json.dumps({"cmd": "nav", "key": rest[0]})
    if cmd == "shot" and rest:
        return json.dumps({"cmd": "shot", "path": rest[0]})
    if cmd == "delay" and rest:
        token = rest[0]
        if token in {"+", "-"}:
            return json.dumps({"cmd": "delay", "arg": token})
        try:
            return json.dumps({"cmd": "delay", "ms": int(token, 0)})
        except ValueError:
            return json.dumps({"cmd": "delay", "arg": token})
    return json.dumps({"cmd": cmd})


class KeepAlive:
    """One UNIX connection, many JSON lines (Xmux ctl-style)."""

    def __init__(self, sock_path: str) -> None:
        self._path = sock_path
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.connect(sock_path)
        self._buf = b""

    def send_line(self, line: str) -> str:
        """Send one request, return one reply line (including newline)."""
        self._sock.sendall((line + "\n").encode("utf-8"))
        while b"\n" not in self._buf:
            chunk = self._sock.recv(4096)
            if not chunk:
                raise OSError(f"eof from {self._path}")
            self._buf += chunk
        idx = self._buf.find(b"\n")
        rec = self._buf[: idx + 1]
        self._buf = self._buf[idx + 1 :]
        return rec.decode("utf-8", errors="replace")

    def close(self) -> None:
        """Close the socket."""
        self._sock.close()


def emit_reply(cmd: str, reply: str) -> None:
    """shot: stdout = versioned path; other cmds: JSON."""
    if cmd != "shot":
        sys.stdout.write(reply)
        return
    try:
        obj = json.loads(reply)
    except json.JSONDecodeError:
        sys.stdout.write(reply)
        return
    path = obj.get("cpu") or obj.get("game")
    if isinstance(path, str) and path:
        sys.stdout.write(path + "\n")
        extra = obj.get("game") if obj.get("cpu") else None
        if isinstance(extra, str) and extra and extra != path:
            sys.stdout.write(extra + "\n")
        return
    sys.stdout.write(reply)


def run_one(ctl: KeepAlive, cmd: str, rest: list[str]) -> None:
    """Send one built command and print the reply."""
    emit_reply(cmd, ctl.send_line(build_line(cmd, rest)))


def main(argv: list[str] | None = None) -> int:
    """CLI entry."""
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-h", "--help", action="store_true")
    parser.add_argument("-v", "--version", action="store_true")
    parser.add_argument("--sock", default=None)
    parser.add_argument("--view", action="store_true")
    parser.add_argument("--ctl", action="store_true")
    parser.add_argument("command", nargs="?")
    parser.add_argument("args", nargs="*")
    ns = parser.parse_args(argv)
    if ns.version:
        print(f"tdxctl {VERSION}")
        return 0
    if ns.help or (ns.command is None and not ns.ctl):
        usage()
        return 0
    sock = ns.sock
    if sock is None:
        sock = "/tmp/tdxview.sock" if ns.view else "/tmp/tdx.sock"
    try:
        ctl = KeepAlive(sock)
    except OSError as exc:
        print(f"tdxctl: {exc}", file=sys.stderr)
        return 1
    try:
        if ns.command is not None:
            run_one(ctl, ns.command, ns.args)
        if ns.ctl:
            for raw in sys.stdin:
                parts = raw.split()
                if not parts:
                    continue
                run_one(ctl, parts[0], parts[1:])
    except OSError as exc:
        print(f"tdxctl: {exc}", file=sys.stderr)
        return 1
    finally:
        ctl.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
