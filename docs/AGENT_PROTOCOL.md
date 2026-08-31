# Agent protocol

Default socket: `/tmp/tdx.sock` (disable with `--no-sock`).

Line-oriented. Either a JSON object or a bare command word.

```text
{"cmd":"step"}
{"cmd":"over"}
{"cmd":"run"}
{"cmd":"stop"}
{"cmd":"reset"}
{"cmd":"regs"}
{"cmd":"disasm"}
{"cmd":"mem","addr":"B800:0000","len":64}
{"cmd":"bp","addr":"1010:001A"}
{"cmd":"bpdel","id":1}
{"cmd":"shot"}
{"cmd":"key","key":"Left"}
{"cmd":"status"}
{"cmd":"cga"}
{"cmd":"quit"}
```

`run` / `F9` **toggles** the SDL F9 state (does not block the UI). `key` is DOS
INT 16 and **starts** F9 so the guest actually consumes it. `nav` is the CPU VCR
(`Up`/`Down`/`Home`/`End`/`PgUp`/`PgDn`) — Down steps **over** CALL/INT/REP/LOOP.

Agents should drive tdx through this socket (`tdxctl`), not Xmux key inject.

Bare words: `step`, `over`, `regs`, `cga`, `mem B800:0000 64`, `bp 1000:0100`.

Reply is one JSON line (`ok`, `cs`, `ip`, `stop`, plus command fields).
`cga` returns `mode`, `w`, `h`, `pixels_b64` (320×200 indices 0..3) for **tdxview**.
`shot` writes `/tmp/tdx-cpu-<pid>.bmp` and `/tmp/tdx-game-<pid>.bmp`.

The listen socket accepts **several clients** at once (`tdxview` + `tdxctl`).

Client: `scripts/tdxctl.py` (`tdxctl`). Viewer: `tdxview`.
