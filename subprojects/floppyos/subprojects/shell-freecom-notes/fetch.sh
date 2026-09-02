#!/usr/bin/env bash
# Optional shallow clone of upstream for study/build. Not required for FloppyOS core.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
DEST="$ROOT/upstream"
URL="https://github.com/FDOS/freecom"
if [[ -d "$DEST/.git" ]]; then
  echo "Already cloned: $DEST"
  git -C "$DEST" fetch --depth=1 origin
  exit 0
fi
echo "Cloning $URL -> $DEST"
git clone --depth=1 "$URL" "$DEST"
echo "Done. Remember license obligations before shipping."
