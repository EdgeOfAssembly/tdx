#!/usr/bin/env python3
"""Host tests: mkflopfs 32-entry root, 72KB file, root_sectors 1 vs 2."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
MKIMG = BUILD / "mkimg"
MKFLOPFS = BUILD / "mkflopfs"

SB_OFF = 512
ROOT_LBA_OFF = 66
ROOT_SECS_OFF = 70
DIRENT_SIZE = 32
SIZE_OFF = 16


def _run(cmd: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, check=check, capture_output=True, text=True)


def _pack(tmp: Path, files: list[Path]) -> Path:
    img = tmp / "disk.img"
    stage = tmp / "stage.bin"
    kernel = tmp / "kernel.bin"
    stage.write_bytes(b"\x90" * 1024)
    kernel.write_bytes(b"\x90" * 16384)
    _run([str(MKIMG), "-s", "360", "-o", str(img)])
    cmd = [str(MKFLOPFS), "-i", str(img), "-s", str(stage), "-k", str(kernel)]
    for f in files:
        cmd.extend(["-f", str(f)])
    _run(cmd)
    return img


def _sb(img: bytes) -> tuple[int, int]:
    sb = img[SB_OFF : SB_OFF + 512]
    assert sb[0:8] == b"FLOPFS01", sb[0:8]
    root_lba = struct.unpack_from("<I", sb, ROOT_LBA_OFF)[0]
    root_secs = struct.unpack_from("<H", sb, ROOT_SECS_OFF)[0]
    return root_lba, root_secs


def _dirent_count(img: bytes, root_lba: int) -> int:
    off = root_lba * 512
    n = 0
    for i in range(32):
        de = img[off + i * DIRENT_SIZE : off + (i + 1) * DIRENT_SIZE]
        if de[0] == 0:
            break
        n += 1
    return n


def _file_size(img: bytes, root_lba: int, fcb11: bytes) -> int:
    off = root_lba * 512
    for i in range(32):
        de = img[off + i * DIRENT_SIZE : off + (i + 1) * DIRENT_SIZE]
        if de[0:11] == fcb11:
            return struct.unpack_from("<I", de, SIZE_OFF)[0]
    raise AssertionError(f"missing dirent {fcb11!r}")


def test_five_files_one_root_sector(tmp: Path) -> None:
    files = []
    for i in range(5):
        p = tmp / f"F{i}.COM"
        p.write_bytes(b"X")
        files.append(p)
    img = _pack(tmp, files)
    data = img.read_bytes()
    root_lba, root_secs = _sb(data)
    assert root_secs == 1, root_secs
    assert _dirent_count(data, root_lba) == 5


def test_22_files_two_root_sectors_72k(tmp: Path) -> None:
    files = []
    big = tmp / "BIG.EXE"
    big.write_bytes(b"E" * (72 * 1024))
    files.append(big)
    for i in range(21):
        p = tmp / f"N{i:02d}.COM"
        p.write_bytes(b"Y")
        files.append(p)
    img = _pack(tmp, files)
    data = img.read_bytes()
    root_lba, root_secs = _sb(data)
    assert root_secs == 2, root_secs
    assert _dirent_count(data, root_lba) == 22
    assert _file_size(data, root_lba, b"BIG     EXE") == 72 * 1024


def test_too_many_dash_f(tmp: Path) -> None:
    img = tmp / "disk.img"
    stage = tmp / "stage.bin"
    kernel = tmp / "kernel.bin"
    stage.write_bytes(b"\x90" * 1024)
    kernel.write_bytes(b"\x90" * 16384)
    _run([str(MKIMG), "-s", "360", "-o", str(img)])
    extras: list[str] = []
    for i in range(33):
        p = tmp / f"Z{i:02d}.COM"
        p.write_bytes(b"Z")
        extras.extend(["-f", str(p)])
    r = _run(
        [str(MKFLOPFS), "-i", str(img), "-s", str(stage), "-k", str(kernel), *extras],
        check=False,
    )
    assert r.returncode != 0, r.stdout + r.stderr
    assert "too many" in (r.stderr + r.stdout).lower()


def main() -> int:
    if not MKIMG.is_file() or not MKFLOPFS.is_file():
        print("missing mkimg/mkflopfs (build tools first)", file=sys.stderr)
        return 2
    import tempfile

    with tempfile.TemporaryDirectory(prefix="flopfs-root-") as td:
        tmp = Path(td)
        (tmp / "t5").mkdir()
        test_five_files_one_root_sector(tmp / "t5")
        (tmp / "t22").mkdir()
        test_22_files_two_root_sectors_72k(tmp / "t22")
        (tmp / "t33").mkdir()
        test_too_many_dash_f(tmp / "t33")
    print("test_mkflopfs_root: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
