#!/bin/bash
# FORMAT C: proof harness for iron86 Xebec (IBM XT Fixed Disk Adapter).
#
# Hardware: ports 320h–323h, IRQ5, DMA3, IBM CHS DCB, option ROM at C8000.
# This script prepares images and prints (or runs) the iron86 command.
# Full FDISK + FORMAT C: + copy + reboot is interactive; the orchestrator
# drives it. Unit tests cover persist / opcode 05 / Type-1 LBA.
#
# ROM paths (do not fetch; local dumps only):
#   Xebec ROS 8K:  $ROS
#   5150 8K BIOS:  $BIOS_5150   (FE000)
#   5160 08NOV82:  $U19_8K + $U18_32K  → 40K concat, or 27256 U19 + U18 → 64K
#   5160 10JAN86:  $U19_86 + $U18_86   → 64K at F0000
#
# Usage:
#   tests/hdc-format-c.sh              # create images, print command
#   tests/hdc-format-c.sh --run        # also exec iron86 (until-prompt)
#   tests/hdc-format-c.sh --concat-only
set -euo pipefail

ROOT="${IRON86_ROOT:-/mnt/TurboDebugger}"
IRON86_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IRON86="${IRON86:-$IRON86_DIR/iron86}"
OUT="${IRON86_HDC_OUT:-/tmp/iron86-hdc-proof}"
mkdir -p "$OUT"

ROS="${IRON86_XEBEC_ROM:-$ROOT/Py86/ROM/IBM/XT/FixedDiskAdapter/IBM_XEBEC_5000059_1982.BIN}"
BIOS_5150="${BIOS_5150:-$ROOT/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN}"
U19_8K="${U19_8K:-$ROOT/Py86/ROM/IBM/XT/5160/BIOS_5160_08NOV82/BIOS_5160_08NOV82_U19_5000027.BIN}"
U18_32K="${U18_32K:-$ROOT/Py86/ROM/IBM/XT/5160/BIOS_5160_08NOV82/BIOS_5160_08NOV82_U18_1501512.BIN}"
U19_27256="${U19_27256:-$ROOT/Py86/ROM/IBM/XT/5160/BIOS_5160_08NOV82/BIOS_5160_08NOV82_U19_5000027_27256.BIN}"
U19_86="${U19_86:-$ROOT/Py86/ROM/IBM/XT/5160/BIOS_5160_10JAN86/BIOS_5160_10JAN86_U19_62X0854_27256_F000.BIN}"
U18_86="${U18_86:-$ROOT/Py86/ROM/IBM/XT/5160/BIOS_5160_10JAN86/BIOS_5160_10JAN86_U18_62X0851_27256_F800.BIN}"
DOS_IMG="${DOS_IMG:-/mnt/M1 Tank Platoon (v449.03) (1990)/re/tdx-tank/msdos331-a.img}"

HDC_IMG="$OUT/hdc.img"
BIOS_40K="$OUT/xt5160_08nov82_40k.bin"
BIOS_64K_82="$OUT/xt5160_08nov82_64k.bin"
BIOS_64K_86="$OUT/xt5160_10jan86_64k.bin"

CYL=306
HEADS=4
SPT=17
NSECTORS=$((CYL * HEADS * SPT))

echo "iron86 Xebec FORMAT C: prep"
echo "  ROS        $ROS"
echo "  DOS floppy $DOS_IMG"
echo "  hdc.img    $HDC_IMG  ($NSECTORS sectors, ${CYL}x${HEADS}x${SPT})"

if [[ ! -f "$ROS" ]]; then
  echo "error: missing Xebec ROS $ROS" >&2
  exit 1
fi

dd if=/dev/zero of="$HDC_IMG" bs=512 count="$NSECTORS" status=none

if [[ -f "$U19_8K" && -f "$U18_32K" ]]; then
  cat "$U19_8K" "$U18_32K" >"$BIOS_40K"
  echo "  5160 40K   $BIOS_40K  (U19 8K + U18 32K)"
fi
if [[ -f "$U19_27256" && -f "$U18_32K" ]]; then
  cat "$U19_27256" "$U18_32K" >"$BIOS_64K_82"
  echo "  5160 64K   $BIOS_64K_82  (U19 27256 + U18)"
fi
if [[ -f "$U19_86" && -f "$U18_86" ]]; then
  cat "$U19_86" "$U18_86" >"$BIOS_64K_86"
  echo "  5160 64K   $BIOS_64K_86  (10JAN86 U19+U18)"
fi

# Prefer 64K 5160 (INT 13 HDD POST + option-ROM scan). 5150 8K + ROS also works.
BIOS="$BIOS_5150"
if [[ -f "$BIOS_64K_86" ]]; then
  BIOS="$BIOS_64K_86"
elif [[ -f "$BIOS_64K_82" ]]; then
  BIOS="$BIOS_64K_82"
elif [[ -f "$BIOS_40K" ]]; then
  BIOS="$BIOS_40K"
fi
echo "  --bios     $BIOS"

CMD=(
  "$IRON86" --preset ibm-xt
  --bios "$BIOS"
  --hdc "$HDC_IMG"
  --hdc-rom "$ROS"
  --floppy-a "$DOS_IMG"
  --no-audio
)

echo
echo "Guest steps (MS-DOS 3.31):"
echo "  1. Boot A:  (date/time: Enter)"
echo "  2. FDISK    create primary DOS partition, reboot if asked"
echo "  3. FORMAT C:"
echo "  4. COPY a small file to C:, reboot, DIR C:"
echo
echo "Command:"
printf '  %q' "${CMD[@]}"
echo
echo
echo "Concat recipe (if you build a 64K --bios yourself):"
echo "  cat U19_32K U18_32K > xt5160_64k.bin     # 1986 chips, both 32K"
echo "  cat U19_8K  U18_32K > xt5160_40k.bin     # 08NOV82 (iron86 also accepts 40K)"
echo "  cat U19_27256 U18_32K > xt5160_64k.bin   # 08NOV82 in 27256 U19"

if [[ "${1:-}" == "--concat-only" ]]; then
  exit 0
fi

if [[ "${1:-}" == "--run" ]]; then
  if [[ ! -x "$IRON86" ]]; then
    echo "building iron86..." >&2
    make -C "$IRON86_DIR" -s iron86
  fi
  if [[ ! -f "$DOS_IMG" ]]; then
    echo "error: missing DOS image $DOS_IMG" >&2
    exit 1
  fi
  exec "${CMD[@]}" --until-prompt
fi
