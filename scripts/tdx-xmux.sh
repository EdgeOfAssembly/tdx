#!/bin/sh
# Start TDX CPU TUI and tdxview CGA window in two Xmux sessions.
#
# Never use `xmux run` for tdx/tdxview — it blocks until the GUI exits
# (looks like a hung "reload" task). `xmux start` forks and returns.
set -eu

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  printf '%s\n' "Usage: tdx-xmux.sh <file.exe|file.com> [cwd]" \
    "  Session tdx      — CPU TUI (1280x800)" \
    "  Session tdx-game — CGA user screen (960x600)" \
    "  Restarts tdx in-place; keeps tdx-game if it is already running." \
    "tdx-xmux.sh 0.3"
  exit 0
fi
if [ "${1:-}" = "-v" ] || [ "${1:-}" = "--version" ]; then
  echo "tdx-xmux.sh 0.3"
  exit 0
fi
if [ $# -lt 1 ]; then
  echo "Usage: tdx-xmux.sh <file.exe|file.com> [cwd]" >&2
  exit 2
fi

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EXE=$1
CWD=${2:-$(CDPATH= cd -- "$(dirname -- "$EXE")" && pwd)}
XMUX=/usr/local/bin/xmux

export PATH="/usr/local/bin:$PATH"

# CPU session only — do not drop a spectator attached to tdx-game.
"$XMUX" kill tdx 2>/dev/null || true
"$XMUX" start tdx --geometry 1280x800 --gl nvidia --no-attach -- \
  "$ROOT/tdx" "$EXE" --cwd "$CWD" --sock /tmp/tdx.sock

if "$XMUX" list 2>/dev/null | grep -q '^tdx-game '; then
  if ! pgrep -x tdxview >/dev/null 2>&1; then
    eval "$("$XMUX" env tdx-game)"
    "$ROOT/tdxview" --sock /tmp/tdx.sock --scale 3 >/tmp/tdxview.log 2>&1 &
  fi
else
  "$XMUX" start tdx-game --geometry 960x600 --gl nvidia --no-attach -- \
    "$ROOT/tdxview" --sock /tmp/tdx.sock --scale 3
fi

printf '%s\n' \
  "SPECTATOR: xmux attach tdx --no-reconnect" \
  "SPECTATOR: xmux attach tdx-game --no-reconnect"
