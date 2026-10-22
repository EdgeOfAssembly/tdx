# Former vendored trees (split out)

iron86 and FloppyOS are no longer vendored here.

| Project  | Dev tree       | How tdx uses it |
|----------|----------------|-----------------|
| iron86   | `/mnt/iron86`  | **System libiron86** (`sudo make install`); `pkg-config iron86` |
| FloppyOS | `/mnt/FloppyOS`| Guest images / FlopFS only (not linked into tdx) |

```text
cd /mnt/iron86 && sudo make install   # once
cd /mnt/tdx && make -s check-iron86 && make -s test
```

Gentoo’s default `pkg-config` path omits `/usr/local/lib/pkgconfig`; the tdx
Makefile prepends it. See `AGENTS.md` and `README.md`.
