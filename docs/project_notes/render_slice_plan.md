# Render slice — raw frame output for the persistent session (plan)

**Pre-registered 2026-06-08. Implementation pending.** Next engine slice on the
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
