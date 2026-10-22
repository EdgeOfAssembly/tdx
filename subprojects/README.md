# Former vendored trees (split out)

iron86 and FloppyOS are no longer vendored here. Use sibling checkouts:

| Project  | Default path   |
|----------|----------------|
| iron86   | `/mnt/iron86`  |
| FloppyOS | `/mnt/FloppyOS`|

tdx builds against iron86 via `IRON86_ROOT` (default `/mnt/iron86`):

```text
make IRON86_ROOT=/mnt/iron86
```

See `AGENTS.md` and `README.md` for the three-repo layout.
