# DOS-era test binaries

## `/mnt/floppy` (host directory on `/mnt`, **not** the USB drive)

Large MS-DOS 6.x-class utility set (~6.5 MB) for **future** FloppyOS compatibility tests.
Do **not** confuse with USB FDD `/dev/sdc` (often mounted at `/tmp/floppy`).

### Good early smoke candidates (small, real-mode)

| Binary | Notes |
|--------|--------|
| `MORE.COM` | Tiny filter; needs stdin/stdout |
| `CHOICE.COM` | Interactive — skip headless |
| `DOSKEY.COM` | TSR — needs stay-resident |
| `TREE.COM` | Needs directories |
| `MEM.EXE` | MZ EXE — needs AH=4B MZ loader (later) |
| `XCOPY.EXE` | MZ; heavy |
| `DEBUG.EXE` / `DEBUGX.COM` | Debugger; useful later |
| `EXE2BIN.EXE` | Small MZ tool |
| `FIND.EXE` / `SORT.EXE` | Filters |
| `COMMAND.COM` | Real MS-DOS shell (~54 KB) — oracle, not our init |

### Not for early FloppyOS

- Windows NE/LE (MW*, SMARTMON, VxDs)
- DriveSpace / MemMaker / large suites
- Self-extracting PKZIP EXEs until we have better EXE support

### Policy

- Copy **selected** tiny tools into FlopFS images under `tests/` when testing
- Never auto-format `/dev/sdc`; never wipe `/mnt/floppy` without human OK
