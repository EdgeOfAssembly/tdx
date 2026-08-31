#!/bin/sh
# Headless Ghidra 12 export of function symbols for tdx --symbols.
# Usage: scripts/ghidra_export.sh FILE.EXE [OUT.sym]
set -eu

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ] || [ $# -lt 1 ]; then
  printf '%s\n' "Usage: ghidra_export.sh <file.exe|file.com> [out.sym]" \
    "  Runs analyzeHeadless (Ghidra 12) and writes TSV symbols." \
    "ghidra_export.sh 0.1"
  exit 0
fi
if [ "${1:-}" = "-v" ] || [ "${1:-}" = "--version" ]; then
  echo "ghidra_export.sh 0.1"
  exit 0
fi

IN=$1
OUT=${2:-${IN%.*}.sym}
GHIDRA_HOME_12=${GHIDRA_HOME_12:-/mnt/re-tools/opt/ghidra_12.1.2}
HEADLESS=$GHIDRA_HOME_12/support/analyzeHeadless
PROJDIR=${TMPDIR:-/tmp}/tdx-ghidra-$$
SCRIPTDIR=$(CDPATH= cd -- "$(dirname -- "$0")/../ghidra_scripts" && pwd)

if [ ! -x "$HEADLESS" ]; then
  echo "ghidra_export.sh: missing $HEADLESS" >&2
  exit 1
fi

mkdir -p "$PROJDIR"
# x86:LE:16:Real Mode — DOS real-mode binaries.
"$HEADLESS" "$PROJDIR" TDX -import "$IN" \
  -processor "x86:LE:16:Real Mode" \
  -analysisTimeoutPerFile 120 \
  -scriptPath "$SCRIPTDIR" \
  -postScript ExportRexSymbols.java "$OUT" \
  -deleteProject
echo "wrote $OUT"
