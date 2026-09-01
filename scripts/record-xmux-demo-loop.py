#!/usr/bin/env python3
"""Record an Xmux session until the first game scene repeats (demo loop).

Takes liberal screenshots via ``xmux ctl`` SHOT. Stops when a later frame
matches the first non-boot scene after the view has clearly left that scene.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

from PIL import Image


def ahash(path: Path, size: int = 16) -> int:
    """Average-hash an image to a ``size*size``-bit integer."""
    with Image.open(path) as im:
        gray = im.convert("L").resize((size, size), Image.Resampling.BILINEAR)
        pixels = list(gray.getdata())
    avg = sum(pixels) / float(len(pixels))
    bits = 0
    for p in pixels:
        bits = (bits << 1) | (1 if p >= avg else 0)
    return bits


def hamming(a: int, b: int) -> int:
    """Bit count of XOR of two hashes."""
    return (a ^ b).bit_count()


def is_mostly_black(path: Path, thresh: int = 18) -> bool:
    """True if mean luma is below ``thresh`` (boot/mode-switch flash)."""
    with Image.open(path) as im:
        gray = im.convert("L").resize((64, 40), Image.Resampling.BILINEAR)
        pixels = list(gray.getdata())
    return (sum(pixels) / float(len(pixels))) < thresh


def parse_shot_ok(line: str) -> Path | None:
    """Parse ``OK /path`` or ``N OK /path`` from xmux ctl."""
    s = line.strip()
    if not s:
        return None
    parts = s.split()
    if "OK" not in parts:
        return None
    i = parts.index("OK")
    if i + 1 >= len(parts):
        return None
    return Path(parts[i + 1])


def main() -> int:
    """CLI entry: record until the opening scene returns."""
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--session", default="bushido", help="Xmux session name")
    ap.add_argument(
        "--out",
        type=Path,
        default=Path("/mnt/TurboDebugger/docs/golden-demo/dosbox"),
        help="Directory for versioned PNGs + manifest",
    )
    ap.add_argument("--interval-ms", type=int, default=350, help="Delay between shots")
    ap.add_argument("--max-sec", type=int, default=900, help="Hard stop")
    ap.add_argument("--match-dist", type=int, default=8, help="Hamming match to first scene")
    ap.add_argument("--leave-dist", type=int, default=28, help="Hamming to count as left scene")
    ap.add_argument("--min-away-sec", type=float, default=12.0, help="Must leave before return")
    ap.add_argument(
        "--ref",
        type=Path,
        default=None,
        help="Reference PNG for loop start/end (title). Default: first non-black shot.",
    )
    args = ap.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    manifest = args.out / "MANIFEST.jsonl"
    base = args.out / "loop.png"

    proc = subprocess.Popen(
        ["xmux", "ctl", args.session],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )
    assert proc.stdin is not None
    assert proc.stdout is not None

    first_hash: int | None = ahash(args.ref) if args.ref is not None else None
    left_at: float | None = None
    seen_ref = first_hash is None  # if no --ref, first content frame is the ref
    unique: set[int] = set()
    n = 0
    t0 = time.monotonic()
    stopped = "timeout"

    try:
        with manifest.open("w", encoding="utf-8") as mf:
            while True:
                elapsed = time.monotonic() - t0
                if elapsed >= args.max_sec:
                    break
                proc.stdin.write(f"SHOT {base}\n")
                proc.stdin.flush()
                line = proc.stdout.readline()
                path = parse_shot_ok(line)
                if path is None or not path.is_file():
                    time.sleep(args.interval_ms / 1000.0)
                    continue
                n += 1
                if is_mostly_black(path):
                    rec = {
                        "n": n,
                        "t": round(elapsed, 3),
                        "path": str(path),
                        "black": True,
                    }
                    mf.write(json.dumps(rec) + "\n")
                    mf.flush()
                    time.sleep(args.interval_ms / 1000.0)
                    continue
                h = ahash(path)
                unique.add(h)
                dist = None if first_hash is None else hamming(h, first_hash)
                if first_hash is None:
                    first_hash = h
                    dist = 0
                    seen_ref = True
                elif (not seen_ref) and dist is not None and dist <= args.match_dist:
                    seen_ref = True
                    dist = 0
                if (
                    seen_ref
                    and dist is not None
                    and dist >= args.leave_dist
                    and left_at is None
                ):
                    left_at = elapsed
                rec = {
                    "n": n,
                    "t": round(elapsed, 3),
                    "path": str(path),
                    "hash": f"{h:064x}",
                    "dist_first": dist,
                    "left": left_at is not None,
                    "unique": len(unique),
                }
                mf.write(json.dumps(rec) + "\n")
                mf.flush()
                print(
                    f"n={n:04d} t={elapsed:6.1f}s dist={dist} unique={len(unique)} {path.name}",
                    flush=True,
                )
                if (
                    left_at is not None
                    and dist is not None
                    and dist <= args.match_dist
                    and (elapsed - left_at) >= args.min_away_sec
                    and len(unique) >= 6
                ):
                    stopped = "loop"
                    rec_done = {
                        "n": n,
                        "t": round(elapsed, 3),
                        "event": "loop_complete",
                        "path": str(path),
                    }
                    mf.write(json.dumps(rec_done) + "\n")
                    break
                time.sleep(args.interval_ms / 1000.0)
    finally:
        try:
            proc.stdin.write("QUIT\n")
            proc.stdin.flush()
        except BrokenPipeError:
            pass
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()

    summary = {
        "stopped": stopped,
        "shots": n,
        "unique_hashes": len(unique),
        "elapsed_sec": round(time.monotonic() - t0, 3),
        "first_hash": None if first_hash is None else f"{first_hash:064x}",
        "left_at": left_at,
        "manifest": str(manifest),
    }
    (args.out / "SUMMARY.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary), flush=True)
    return 0 if stopped == "loop" else 2


if __name__ == "__main__":
    sys.exit(main())
