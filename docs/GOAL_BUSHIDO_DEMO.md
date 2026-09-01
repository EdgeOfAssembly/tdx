# Multi-agent goal: TDX plays the Bushido demo loop

Copy everything below the line into an orchestrator turn. Depth 1. Spawn only.

---

## Goal (one sentence)

**Make `tdx` and `tdxview` work — they work when Bushido’s demo loop runs
through the same scenes as DOSBox without crashing, going black, or dying.**

That is the whole bar. Not cycle-exact. Not a pixel-diff of CGA composite.
The guest must *play*: title → courtyard → hit-flash → tavern → dojo →
high scores → title again, while the CPU window stays a live debugger.

## What “works” means (all true)

1. Start:

   ```bash
   ./tdx /mnt/bushido/bushido/BUSHIDO.EXE --cwd /mnt/bushido/bushido --sock /tmp/tdx.sock
   ./tdxview --sock /tmp/tdx.sock --listen /tmp/tdxview.sock --scale 3
   ```

2. Agents drive **only** the two UNIX sockets (no Xmux for tdx/tdxview):

   | Window | Socket | Client |
   |--------|--------|--------|
   | CPU (listing/regs) | `/tmp/tdx.sock` | `python3 scripts/tdxctl.py …` |
   | Game (CGA) | `/tmp/tdxview.sock` | `python3 scripts/tdxctl.py --view …` |

   ```bash
   python3 scripts/tdxctl.py ping
   python3 scripts/tdxctl.py --view ping
   python3 scripts/tdxctl.py run          # F9
   python3 scripts/tdxctl.py shot /tmp/tdx-cpu.bmp
   python3 scripts/tdxctl.py --view shot /tmp/tdx-game.bmp
   ```

3. After F9, the **game** window shows the golden scenes in order
   (`docs/golden-demo/scenes/01-title.png` … `07-title-again.png`).
   Layout and poses must match; palette/scale may differ.

4. The loop **returns to the title** (scene 07 ≈ scene 01) without:

   - `UC_ERR_INSN_INVALID` / footer **cpu fault**
   - footer **terminated** (INT 20 / AH=4C) mid-demo
   - empty CPU listing
   - tdxview stuck black through a whole scene
   - CS:IP wandering to `00FF:xxxx` zeros

5. `make -s test` exit 0. `make -s verify` after tests (or `formal: not run` + reason).

## Golden reference (already recorded)

DOSBox + Xmux, 400 cycles, CGA. **Do not re-record unless the golden is missing.**

| Scene | Path |
|-------|------|
| Title | `docs/golden-demo/scenes/01-title.png` |
| Courtyard | `docs/golden-demo/scenes/02-courtyard.png` |
| Hit-flash | `docs/golden-demo/scenes/03-hitflash.png` |
| Tavern | `docs/golden-demo/scenes/04-tavern.png` |
| Dojo | `docs/golden-demo/scenes/05-dojo.png` |
| Scores | `docs/golden-demo/scenes/06-scores.png` |
| Title again | `docs/golden-demo/scenes/07-title-again.png` |

How it was captured: `docs/golden-demo/README.md`.
Guest: `/mnt/bushido/bushido/BUSHIDO.EXE`.

## Hard rules

- **No Xmux** for tdx or tdxview. Xmux is only for the DOSBox golden (done).
- Two sockets, keep-alive (`tdxctl --ctl` or one connect / many JSON lines).
- Liberal screenshots of **both** windows on every scene change.
- C++23 / C23, Allman, `include/rex/rex.h` ABI, `make -s test`.
- Git: checkpoint, then `FEATURE` / `FIXUP` `vN` (skill efficient-git). Do not
  commit `BORLANDC/`.
- Do not special-case DOS in `rex.h`.

## Topology

```text
                 Orchestrator (this session)
        ┌──────────────┼──────────────┐
        ▼              ▼              ▼
   [implement]    [visual]       [review]
   tdx/tdxview    UNIX shots     diffs/tests
   Unicorn/BIOS   vs golden
```

Prefix spawn descriptions: `[implement]`, `[visual]`, `[review]`.

## Subagent prompts (copy, fill TASK)

### Implementer

```
You are the Implementer for /mnt/TurboDebugger (C++23, Allman, rex.h ABI).
TASK: <one concrete hole that blocks the Bushido demo loop>
Do not use Xmux. Prove with make -s test.
Smallest correct change. Doxygen on public APIs.
Write a short summary to /tmp/tdx-impl.md (paths + commands + exit codes).
```

### Visual tester

```
You are the Visual Tester. You never spawn children.
Drive tdx + tdxview on DISPLAY=:0 through UNIX sockets only:
  python3 scripts/tdxctl.py ping
  python3 scripts/tdxctl.py --view ping
  python3 scripts/tdxctl.py run
  python3 scripts/tdxctl.py shot /tmp/tdx-cpu.bmp
  python3 scripts/tdxctl.py --view shot /tmp/tdx-game.bmp
Pipeline: shot → READ the PNG → one action → shot.
Compare tdxview frames to docs/golden-demo/scenes/01-title.png … 07-title-again.png.
Fail if: black scene, cpu fault, terminated, empty listing, CS=00FF, loop never
returns to title.
Write VISUAL_LOG.md under /tmp/tdx-visual/ with timestamp, cmd, shot path, scene.
No Xmux.
```

### Reviewer

```
You are the Reviewer/QA. Do not edit product code.
Read the implementer summary and git diff. Check: demo-loop holes, INT 10/16/21,
PIT/IRQ0, F9 stop-on-fault, listing db-fallback, tests.
Require make -s test evidence. Write /tmp/tdx-review.md (severity, file:line).
```

## Orchestrator loop

1. Visual: F9, screenshot both sockets, name the scene vs golden.
2. If the game window diverges or the CPU dies → Implementer gets one hole.
3. Reviewer on the diff.
4. Repeat until scene 07 (title) appears after the attract, with a live listing.

## Non-goals

- Cycle-exact 8086 timing
- Pixel-identical CGA composite artifact
- Putting tdx under Xmux
- Replacing Unicorn with DOSBox’s CPU
- Porting Bushido itself (`/mnt/bushido` `bushido_port` is a different goal)
