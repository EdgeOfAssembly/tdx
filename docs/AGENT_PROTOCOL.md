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
{"cmd":"key","key":"F8"}
{"cmd":"status"}
{"cmd":"cga"}
{"cmd":"quit"}
```

Bare words: `step`, `over`, `regs`, `cga`, `mem B800:0000 64`, `bp 1000:0100`.

Reply is one JSON line (`ok`, `cs`, `ip`, `stop`, plus command fields).
`cga` returns `mode`, `w`, `h`, `pixels_b64` (320×200 indices 0..3) for **tdxview**.
`shot` writes `/tmp/tdx-cpu-<pid>.bmp` and `/tmp/tdx-game-<pid>.bmp`.

The listen socket accepts **several clients** at once (`tdxview` + `tdxctl`).

Client: `scripts/tdxctl.py` (`tdxctl`). Viewer: `tdxview`.
