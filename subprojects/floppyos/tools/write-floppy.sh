#!/usr/bin/env bash
# Write a raw 1.44 MB image to a block device — REQUIRES explicit human OK.
# Default: refuse everything. Never run against /dev/sdc without confirmation.
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: write-floppy.sh --i-confirm-format --device /dev/sdX --image FILE.img

  DANGEROUS: overwrites the entire block device.
  FloppyOS policy: human must confirm format of USB floppy (/dev/sdc) in chat
  before you pass --i-confirm-format.

  Refuses if:
    - --i-confirm-format missing
    - device is not a block device
    - image is not exactly 1474560 bytes
EOF
}

DEVICE=""
IMAGE=""
CONFIRM=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --i-confirm-format) CONFIRM=1; shift ;;
    --device) DEVICE="${2:-}"; shift 2 ;;
    --image) IMAGE="${2:-}"; shift 2 ;;
    *) echo "unknown: $1" >&2; usage; exit 2 ;;
  esac
done

if [[ "$CONFIRM" -ne 1 ]]; then
  echo "write-floppy: refusing — pass --i-confirm-format only after human OK" >&2
  exit 1
fi
if [[ -z "$DEVICE" || -z "$IMAGE" ]]; then
  usage
  exit 2
fi
if [[ ! -b "$DEVICE" ]]; then
  echo "write-floppy: not a block device: $DEVICE" >&2
  exit 1
fi
if [[ ! -f "$IMAGE" ]]; then
  echo "write-floppy: image not found: $IMAGE" >&2
  exit 1
fi

SIZE=$(stat -c%s "$IMAGE")
if [[ "$SIZE" -ne 1474560 ]]; then
  echo "write-floppy: image size $SIZE != 1474560" >&2
  exit 1
fi

echo "write-floppy: about to: dd if=$IMAGE of=$DEVICE bs=512 count=2880 conv=fsync"
echo "write-floppy: ABORT now if this is wrong (Ctrl-C). Sleeping 3s..."
sleep 3
# Caller must already have root / sudo if needed
dd if="$IMAGE" of="$DEVICE" bs=512 count=2880 conv=fsync status=progress
echo "write-floppy: done. Verify with: xxd -l 512 $DEVICE | head"
