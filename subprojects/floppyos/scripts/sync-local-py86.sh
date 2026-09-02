#!/usr/bin/env bash
# Create/update FloppyOS/subprojects/py86 from monorepo Py86 (read-only source).
# Never writes to monorepo Py86. Safe to re-run; preserves FLOPPYOS.md and local overlays.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${PY86_UPSTREAM:-$ROOT/../Py86}"
DST="$ROOT/subprojects/py86"

if [[ ! -d "$SRC" ]]; then
  echo "upstream Py86 not found: $SRC" >&2
  exit 1
fi
if [[ -L "$DST" ]]; then
  echo "removing symlink $DST (will replace with real copy)"
  rm -f "$DST"
fi
mkdir -p "$DST"

# Preserve FloppyOS-only files across sync
KEEP=$(mktemp -d)
trap 'rm -rf "$KEEP"' EXIT
[[ -f "$DST/FLOPPYOS.md" ]] && cp -a "$DST/FLOPPYOS.md" "$KEEP/"
[[ -d "$DST/floppyos_overlay" ]] && cp -a "$DST/floppyos_overlay" "$KEEP/"

rsync -a \
  --delete \
  --exclude 'FLOPPYOS.md' \
  --exclude 'floppyos_overlay/' \
  --exclude '__pycache__/' \
  --exclude '*.pyc' \
  --exclude 'py86.log' \
  --exclude 'py86_trace.log' \
  --exclude 'py86_profile.stats' \
  --exclude 'capture/' \
  --exclude 'vram_port*' \
  --exclude '.pytest_cache/' \
  --exclude '.git/' \
  --exclude 'session-ses_*.md' \
  "$SRC/" "$DST/"

[[ -f "$KEEP/FLOPPYOS.md" ]] && cp -a "$KEEP/FLOPPYOS.md" "$DST/"
[[ -d "$KEEP/floppyos_overlay" ]] && cp -a "$KEEP/floppyos_overlay" "$DST/"

# Ensure marker exists
if [[ ! -f "$DST/FLOPPYOS.md" ]]; then
  echo "WARNING: FLOPPYOS.md missing after sync — restore from git" >&2
fi

echo "OK: local Py86 at $DST (from $SRC)"
echo "    Edit only $DST — never $SRC"
du -sh "$DST"
