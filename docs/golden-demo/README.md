# Bushido demo loop (DOSBox golden)

Recorded 2026-09-01 under **Xmux session `bushido`** + DOSBox Staging
(`--noprimaryconf`, conf `/mnt/bushido/re/dosbox/bushido.conf`, 400 cycles/ms,
CGA, `BUSHIDO.EXE` from `/mnt/bushido/bushido`).

## Loop (title → attract → title)

| # | Scene | File |
|---|--------|------|
| 1 | Title (Bushido / 武士道 / Review Copy) | `scenes/01-title.png` |
| 2 | Courtyard attract (level-1 outdoor) | `scenes/02-courtyard.png` |
| 3 | Hit-flash (palette invert) | `scenes/03-hitflash.png` |
| 4 | Tavern / sake barrels | `scenes/04-tavern.png` |
| 5 | Dojo (indoor, tea table, ninja) | `scenes/05-dojo.png` |
| 6 | High scores “Great Warriors” | `scenes/06-scores.png` |
| 7 | Title again (same as 1) | `scenes/07-title-again.png` |

Full liberal capture: `dosbox/loop-*.png` (412 frames, ~132 s, gitignored).
Index: `dosbox/MANIFEST.jsonl` + `dosbox/SUMMARY.json`.

Recorder: `scripts/record-xmux-demo-loop.py`.

Re-record:

```text
SPECTATOR: xmux attach bushido --no-reconnect
xmux start bushido --geometry 960x600 --gl nvidia --no-attach -- \
  /usr/local/bin/dosbox --noprimaryconf --nolocalconf \
  --conf /mnt/bushido/re/dosbox/bushido.conf
```
