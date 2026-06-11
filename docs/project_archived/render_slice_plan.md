# Render slice — raw frame output for the persistent session (plan + result)

> **ARCHIVED 2026-06-10 — shipped.** The render capability (`display`) and the
> per-frame string region-drop (`strheap-mark`/`strheap-reset`) landed in
> `bytecode_gc`; ADR-004 in `docs/project_notes/decisions.md` records the
> decision. Kept as the pre-registration + measurement record.

**Pre-registered 2026-06-08. Implemented + measured same day — prediction
CONFIRMED (see "Result" at the bottom).** Next engine slice on the
terminal/game roadmap (`terminal_game_roadmap.md`, ADR-003), chosen over the
roadmap's original B2 (`read-key`) because the game demo showed **input needs
no engine primitive in the host-driven model** — the host owns the keyboard and
calls `(go <action>)` per turn. The genuine next gap is *rendering*.

## The gap

Today a turn prints its result as a Lisp value — `(5 2 1 8432)`. A game wants a
frame. `bytecode_gc` has strings, but `print_val` emits them **quoted and
escaped** (`"....\n...."`), so a string frame renders with literal `\n` and
surrounding quotes. There is no way to emit raw bytes as output.

## What's being changed

Add a **raw-output primitive** to `bytecode_gc.c` (the strings/experiment
lineage; like EXP1 strings, this intentionally breaks 8-engine parity and is
gated the same way). Candidate designs:

1. **`(display s)`** — write a string's bytes straight to `outbuf` (no quotes,
   no escaping), return nil. Multiple `display`s in one eval concatenate.
   *Recommended:* smallest general primitive, composes (a `render` fn calls
   `display` per row), and leaves value-printing untouched.
2. **Print a top-level string result verbatim** — if the last form's value is a
   string, print it raw. Minimal, but conflates "value" with "frame" and breaks
   the printer's round-trip for ordinary REPL string inspection. *Rejected.*
3. **A dedicated frame buffer / `term/*` family** — heavier; defer until a real
   ANSI render loop needs cursor control.

ANSI (clear-screen, cursor-home) can come from the host between turns, or be
`string-append`-ed into the frame by the Lisp program — no extra primitive
needed either way.

## Dependency and the pre-registered prediction

Rendering allocates strings per frame, which puts the slice straight on top of
**B4 — `bytecode_gc`'s incomplete string-heap reclamation** (`strheap_top` only
bumps; the side heap has no free list/compactor — see `standalone/README.md`).

**Prediction:** a per-turn string render will leak — `strheap` grows unbounded
across turns and a long game eventually exhausts it, returning `<error>` even
though the cons arena is fine. This would **confirm B4 is the real game blocker**
(the roadmap's claim) and quantify it as *turns-to-exhaustion* at a given frame
size. Falsified if reclamation already handles transient frame strings.

## Method

1. Add `display` (option 1), rebuild `bytecode_gc.wasm` (+ commit per the
   tracked-wasm rule).
2. Extend the coin game to a 2D grid `render` that builds each frame with
   `string-append` and emits it via `display`; confirm a clean grid prints
   (no quotes/`\n`).
3. Drive M turns through `repl.mjs`; watch `gc_count()` and instrument
   `strheap_top`. Record turns-to-`<error>` vs frame size.

## What we get either way

- The render path itself (capability) regardless of the leak outcome.
- A measured answer to "is string reclamation the blocker?" — either a number
  that scopes the B4 fix, or a falsification that reopens the question.

## Scope

~15–30 lines engine (`display` prim + raw `outbuf` write), no host-loop change
yet (turns still host-driven via `repl.mjs`). The interactive xterm.js loop
(B5) stays downstream.

## Result (2026-06-08) — prediction CONFIRMED

Implemented `(display s)` in `bytecode_gc.c` (option 1): writes a string's bytes
straight to `outbuf`, returns nil. `run_buffer` suppresses the value echo when a
program produced output via `display` (so a frame isn't followed by `()`), but
always surfaces `<error>`. Added a `strheap_used()` introspection export
(sibling of `gc_count()`). `eval_source` byte-behaviour unchanged; existing
`test_bc.mjs` 70/70 and `test_session.mjs` (now 24/24, +6 `display` checks)
green; zero imports preserved.

**Capability:** the coin game renders as a clean raw grid (`harness/render_probe.mjs`):

```
.@...o....
..@..o....     @ = player, o = coin; collected at turn 5, respawns
....@o....
..o..@....
```

**Measurement** (drive frame-building turns via a recursive driver in one eval —
flat via TCO — sampling `strheap_used`):

| turns | strheap_used | bytes/turn | gc_count |
|------:|-------------:|-----------:|---------:|
| 1000  | 129,188      | ~128       | 0 |
| 4000  | 513,188      | ~128       | 0 |
| 8000  | 1,025,188    | ~128       | 0 |
| ~8200 | **exhausted → `<error>`** | | 0 |

Exactly as predicted: **monotonic ~128 bytes/turn, zero reclamation,
`gc_count` never moves** (the collector resets strheap mark bits but frees
nothing), exhausting the 1 MB heap at **~8,200 turns** for a *10-cell* frame.
At a realistic frame size the per-frame cost is far higher (building a row with
nested `string-append` is quadratic in width), so a full-screen render would
exhaust in **hundreds of frames** — seconds of play.

**Conclusion:** B4 (string-heap reclamation) is confirmed and quantified as the
real blocker for a sustained TUI game. The render *capability* is done; the next
engine work is a strheap reclamation strategy (free-list or compactor, or — for
short-lived per-frame strings — a region/generation reset between frames). That
is the next slice. Reproduce with `node harness/render_probe.mjs`.

## Follow-up: per-frame region reset (2026-06-08) — leak fixed for transient frames

Built the region-drop, not the compactor (rationale + the three rejected
automatic designs are ADR-004). Two primitives:

- `(strheap-mark)` → current heap top as a fixnum.
- `(strheap-reset m)` → drop the heap back to `m` (O(1)). Caller contract: no
  string allocated after the mark may still be reachable after the reset.

The render loop captures `base` *after* its definitions (so the `"@"`/`"."`/`"\n"`
literals, allocated at define time, sit below `base`) and resets to `base` each
turn. `render_probe.mjs` measures it:

| | without reset | with region reset |
|---|---|---|
| strheap after 8k turns | exhausted (`<error>`) | — |
| strheap after **100,000** turns | (would need ~12 MB) | **36 bytes, flat** |
| growth | ~128 B/turn | **0** |

`strheap_used` stays exactly at `base` (36 bytes — just the literals) across
100k frames. Verified literals survive a reset+realloc (the correctness trap):
`test_session.mjs` 30/30.

**Residual:** strings *kept* across frames (persistent-string churn) still leak;
only the general mark-compactor fixes that. Trigger to revisit: a program that
keeps allocating long-lived strings. For transient per-frame rendering — the
game case — the region reset is sufficient and O(1).
