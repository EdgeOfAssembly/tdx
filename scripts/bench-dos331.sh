#!/bin/bash
# Time tdx --no-ui --run until MS-DOS 3.31 A> (date/time Enter).
# Usage: bench-dos331.sh [tdx-binary]
set -euo pipefail
ROOT="/mnt/M1 Tank Platoon (v449.03) (1990)/re/tdx-tank"
BIOS="/mnt/TurboDebugger/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN"
TDX="${1:-/mnt/TurboDebugger/tdx.release}"
SOCK="/tmp/tdx-bench.sock"
PY="${PY:-/mnt/python/bin/python}"
CTL="/mnt/TurboDebugger/scripts/tdxctl.py"
rm -f "$SOCK"
"$TDX" --no-ui --run --bios "$BIOS" \
  --floppy-a "$ROOT/msdos331-a.img" \
  --floppy-b "$ROOT/DISK01-nokey.img" \
  --sock "$SOCK" >/tmp/tdx-bench.log 2>&1 &
PID=$!
cleanup() {
  "$PY" "$CTL" --sock "$SOCK" quit >/dev/null 2>&1 || true
  wait "$PID" 2>/dev/null || true
  rm -f "$SOCK"
}
trap cleanup EXIT
for i in $(seq 1 80); do
  test -S "$SOCK" && break
  sleep 0.05
done
test -S "$SOCK"
START=$(date +%s.%N)
typed_date=0
typed_time=0
for n in $(seq 1 200); do
  "$PY" "$CTL" --sock "$SOCK" dump cga /tmp/tdx-bench-cga.bin >/dev/null 2>/dev/null || true
  if test -f /tmp/tdx-bench-cga.bin; then
    txt=$(python3 -c "b=open('/tmp/tdx-bench-cga.bin','rb').read(); print(''.join(chr(b[i]) if 32<=b[i]<127 else ' ' for i in range(0,len(b),2)))" 2>/dev/null || true)
    if echo "$txt" | grep -q 'A>'; then
      END=$(date +%s.%N)
      python3 -c "print('A> in {:.3f}s after {} polls'.format(float('$END')-float('$START'), $n))"
      exit 0
    fi
    if echo "$txt" | grep -qi 'new time' && test "$typed_time" -eq 0; then
      "$PY" "$CTL" --sock "$SOCK" type $'\n' >/dev/null 2>/dev/null || true
      typed_time=1
    elif echo "$txt" | grep -qi 'new date' && test "$typed_date" -eq 0; then
      "$PY" "$CTL" --sock "$SOCK" type $'\n' >/dev/null 2>/dev/null || true
      typed_date=1
    fi
  fi
  sleep 0.05
done
echo 'TIMEOUT no A>'
exit 1
