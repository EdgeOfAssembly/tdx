#!/usr/bin/env bash
# M12+: default smoke uses monitor sendkey interactive test
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="${1:-$ROOT/build/floppyos.img}"
exec python3 "$ROOT/tests/smoke_interactive.py" "$IMG"
