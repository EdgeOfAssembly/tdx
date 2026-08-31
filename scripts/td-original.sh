#!/bin/sh
# Run original Borland TD.EXE under DOSBox Staging (reference, not TDX).
set -eu

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ] || [ $# -lt 1 ]; then
  printf '%s\n' "Usage: td-original.sh <DOS.EXE>" \
    "  Mounts BORLANDC + the guest dir and runs TD.EXE -l -vg." \
    "td-original.sh 0.1"
  exit 0
fi
if [ "${1:-}" = "-v" ] || [ "${1:-}" = "--version" ]; then
  echo "td-original.sh 0.1"
  exit 0
fi

GUEST=$1
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BC=$ROOT/BORLANDC
GDIR=$(CDPATH= cd -- "$(dirname -- "$GUEST")" && pwd)
GBASE=$(basename -- "$GUEST")

if [ ! -f "$BC/BIN/TD.EXE" ]; then
  echo "td-original.sh: missing $BC/BIN/TD.EXE" >&2
  exit 1
fi

exec dosbox --noprimaryconf --nolocalconf \
  -c "mount c $BC" \
  -c "mount d $GDIR" \
  -c "c:" \
  -c "cd bin" \
  -c "td -l -vg d:\\$GBASE"
