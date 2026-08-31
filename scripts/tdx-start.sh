#!/bin/sh
# Start tdx + tdxview on the current $DISPLAY. Agents use UNIX sockets, not Xmux.
set -eu

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  printf '%s\n' "Usage: tdx-start.sh <file.exe|file.com> [cwd]" \
    "  tdx listens on /tmp/tdx.sock" \
    "  tdxview listens on /tmp/tdxview.sock" \
    "  Agent: tdxctl shot | tdxctl --view shot | tdxctl --ctl" \
    "tdx-start.sh 0.5"
  exit 0
fi
if [ "${1:-}" = "-v" ] || [ "${1:-}" = "--version" ]; then
  echo "tdx-start.sh 0.5"
  exit 0
fi
if [ $# -lt 1 ]; then
  echo "Usage: tdx-start.sh <file.exe|file.com> [cwd]" >&2
  exit 2
fi

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EXE=$1
CWD=${2:-$(CDPATH= cd -- "$(dirname -- "$EXE")" && pwd)}

# Do not match wrappers; kill by binary path.
for p in $(ps -C tdx -o pid= 2>/dev/null); do
  cmd=$(tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null || true)
  case $cmd in
    *"$ROOT/tdx "*) kill "$p" 2>/dev/null || true ;;
  esac
done
for p in $(ps -C tdxview -o pid= 2>/dev/null); do
  cmd=$(tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null || true)
  case $cmd in
    *"$ROOT/tdxview"*) kill "$p" 2>/dev/null || true ;;
  esac
done
sleep 0.2

"$ROOT/tdx" "$EXE" --cwd "$CWD" --sock /tmp/tdx.sock >/tmp/tdx-cpu.log 2>&1 &
"$ROOT/tdxview" --sock /tmp/tdx.sock --listen /tmp/tdxview.sock --scale 3 >/tmp/tdxview.log 2>&1 &

printf '%s\n' \
  "agent cpu:  tdxctl shot" \
  "agent game: tdxctl --view shot" \
  "keep-alive: tdxctl --ctl" \
  "optional spectator: xmux attach still works if you launched under xmux"
