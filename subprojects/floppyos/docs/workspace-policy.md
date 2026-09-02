# FloppyOS workspace policy

**Updated:** 2026-07-26

## Disk layout while developing

| Path | Role |
|------|------|
| **`/tmp`** | **Primary work area** — RAM-backed tmpfs. Builds, extracts, images, scratch. Prefer this for all heavy work. |
| **`/mnt`** | Optional **archives / copies** when something should survive reboot or free `/tmp` (tarballs, logs, golden images). e.g. `/mnt/floppyos-build/` |
| **`/`** | Avoid large builds (often near full). Install prefixes like `/usr/local` / `/opt/ow` only when needed. |

## Git

- Develop under the git tree (this repo / monorepo path).
- **Commit and push to GitHub** at sensible milestones (FEATURE/FIXUP messages per project git rules) — agent may do so when a unit is verified, without waiting for an extra “please commit” each time.
- Still: no force-push of shared history; no secrets; checkpoint before large FEATURE series when useful.

## Hardware

- USB floppy `/dev/sdc`: **never format without explicit human OK**.

## Related

- Toolchain & QEMU: `docs/toolchain-notes.md`
- Subprojects: `subprojects/`
