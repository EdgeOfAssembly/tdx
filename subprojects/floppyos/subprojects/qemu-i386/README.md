# Custom QEMU i386 (FloppyOS host emulator)

**Priority:** P0 (boot CI)  
**Version:** 10.2.3  
**Install:** `/usr/local` (`qemu-system-i386`, `qemu-img`, …)

## Why not only DOSBox?

DOSBox is great for games/apps. FloppyOS needs a **full PC** path: BIOS POST, INT 13h floppy, optional HDD/LBA, serial, screenshots — **QEMU** is the right tool for OS boot images.

## Build layout (this machine)

| Path | Purpose |
|------|---------|
| `/tmp/qemu-10.2.3.tar.xz` | Working tarball (also archived) |
| `/mnt/floppyos-build/qemu-10.2.3.tar.xz` | **Archive** of source tarball |
| `/tmp/qemu-10.2.3` | Extracted source (compile on tmpfs) |
| `/tmp/qemu-i386-build` | Out-of-tree build |
| `/mnt/floppyos-build/*.log` | configure/build/install/analyzer logs |
| `/usr/local` | Install prefix |

**Rule:** compile on **`/tmp`**; large archives on **`/mnt`**. Root is nearly full.

## Configure flags used

```bash
/tmp/qemu-10.2.3/configure \
  --prefix=/usr/local \
  --target-list=i386-softmmu \
  --enable-kvm \
  --enable-sdl \
  --enable-tools \
  --enable-strip \
  --disable-docs \
  --disable-guest-agent \
  --disable-werror \
  --audio-drv-list=pa,alsa,sdl,oss
```

Result: **i386-softmmu only**, KVM+TCG, SDL/GTK/curses, slirp, tools (`qemu-img`).

## Rebuild

```bash
cd /tmp/qemu-i386-build
make -j$(nproc)
sudo -S make install < /tmp/password.txt
```

## FloppyOS smoke

```bash
qemu-system-i386 -fda build/floppyos.img -boot a -display sdl
# headless CI-ish:
qemu-system-i386 -fda build/floppyos.img -boot a -nographic -serial mon:stdio
```

## Related host tools

- OpenWatcom: `/opt/ow`
- DOSBox: `/usr/local/bin/dosbox`
- TCC (LINKS): archived phoenixthrush dump — **skip**; see `docs/toolchain-notes.md`
