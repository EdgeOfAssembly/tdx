# Agent protocol

Default socket: `/tmp/tdx.sock` (disable with `--no-sock`).

Line-oriented. Either a JSON object or a bare command word.

```text
{"cmd":"step"}
{"cmd":"over"}
{"cmd":"run"}
{"cmd":"stop"}
{"cmd":"pause"}
{"cmd":"unpause"}
{"cmd":"delay"}
{"cmd":"delay","ms":10}
{"cmd":"faster"}
{"cmd":"slower"}
{"cmd":"reset"}
{"cmd":"regs"}
{"cmd":"disasm"}
{"cmd":"mem","addr":"B800:0000","len":64}
{"cmd":"bp","addr":"1010:001A"}
{"cmd":"bp","addr":"1970:4969","hits":1}
{"cmd":"bp","addr":"1970:0100","end":"1970:0200"}
{"cmd":"bpm","addr":"B800:0000","end":"B800:3FFF"}
{"cmd":"bpm","addr":"B800:0000","hits":1}
{"cmd":"bpint","int":16}
{"cmd":"bpint","int":16,"hits":1}
{"cmd":"bpinsn","pat":"int 10","hits":0}
{"cmd":"bpinsn","pat":"call","hits":1}
{"cmd":"bpdel","id":1}
{"cmd":"bplist"}
{"cmd":"shot"}
{"cmd":"key","key":"Left"}
{"cmd":"status"}
{"cmd":"cga"}
{"cmd":"quit"}
```

`run` / `F9` **toggles** the SDL F9 state (does not block the UI). `pause` /
`stop` always pause; `unpause` always resumes. `delay` queries F9 slice park
(ms); `delay` + `ms` sets it (0 = fastest, cap 200). `faster` / `slower` step
by 5 ms (same as CPU `+` / `-`). `key` is DOS INT 16 and **starts** F9 so the
guest actually consumes it. `nav` is the CPU VCR (`Up`/`Down`/`Home`/`End`/
`PgUp`/`PgDn`) — Down steps **over** CALL/INT/REP/LOOP.

Breakpoints pause F9 (`stop` = breakpoint). Agents may set:

- `bp 1970:4969` — this CS:IP (optional `"hits":1` = first only)
- `bp 1970:0100 1970:0200` — any insn whose linear address is in that window
- `bpm B800:0000 B800:3FFF` — guest **write** to that RAM (CGA regen). `once` = first store.
- `bpint 10` — every `INT 10h`; `bpint 10 once` / `"hits":1` = first only
- `bpinsn int 10` / `bpinsn call` / `bpinsn out` — any instruction whose
  Capstone text matches (hex operands; `"int 10"` is INT 10h)
- `bplist` — exec + INT + insn lists; `bpdel <id>` removes exec/insn ids

Agents drive **tdx** (`/tmp/tdx.sock`) and **tdxview** (`/tmp/tdxview.sock`)
directly. Xmux is optional (human spectator only).

Keep-alive (one connect, many ops, like `xmux ctl`):

```text
tdxctl --ctl
shot
key Space
nav Down
```

Bare words: `step`, `over`, `regs`, `cga`, `mem B800:0000 64`, `bp 1000:0100`.

Reply is one JSON line (`ok`, `cs`, `ip`, `stop`, plus command fields).
`cga` returns `mode`, `w`, `h`, `pixels_b64` (320×200 indices 0..3) for **tdxview**.
`shot` writes `stem-YYYYMMDDTHHMMSS.mmm.bmp` (Xmux naming). `tdxctl shot` prints
that path on stdout. Game window: `tdxctl --view shot`.

Both sockets keep clients open (pipeline-safe).

Client: `scripts/tdxctl.py` (`tdxctl`). Viewer: `tdxview`.
