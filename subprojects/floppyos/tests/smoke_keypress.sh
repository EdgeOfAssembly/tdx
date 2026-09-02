#!/usr/bin/env bash
# Optional M12 visual test: keypress.py + QEMU SDL (needs DISPLAY / X11).
# Prefer tests/smoke_interactive.py (monitor sendkey) for headless CI.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="${1:-$ROOT/build/floppyos.img}"
QEMU="${QEMU:-qemu-system-i386}"

if [[ -z "${DISPLAY:-}" ]]; then
  echo "smoke_keypress: no DISPLAY — use smoke_interactive.py instead" >&2
  exit 2
fi
if ! command -v keypress.py >/dev/null; then
  echo "smoke_keypress: keypress.py not found" >&2
  exit 2
fi

SCRIPT=$(mktemp)
trap 'rm -f "$SCRIPT"' EXIT
cat >"$SCRIPT" <<'EOF'
# Wait for boot
<wait:3>
dir
<wait:1>
ver
<wait:1>
exit
<wait:1>
EOF

# emulator-mode: QEMU uses US layout internally
keypress.py \
  --emulator-mode \
  -d 2 \
  -t 0.05 \
  -w "QEMU\|qemu" \
  -n \
  "$QEMU -machine pc,accel=tcg -m 16 -drive file=${IMG},format=raw,if=floppy,index=0 -boot a -display sdl" \
  "$SCRIPT"

echo "smoke_keypress: launched (check QEMU window / manual verify)"
