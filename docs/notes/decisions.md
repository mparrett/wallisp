# Decisions

## ADR-001: Add native build as measurement substrate (2026-05-29)

**Context**: The project's empirical results all flowed through V8-on-wasm via
Node. FINDINGS.md repeatedly hit caveats it couldn't resolve from that vantage
point — most notably H2's "~1.6× GC overhead" claim, which we couldn't tell was
a compiler-level effect (the cons-can-reach-gc optimization barrier) or a V8
JIT artifact. The wasm runtime layer itself was an unmeasured variable.

**Decision**: Add a parallel native build path (`bash build.sh --native`) that
compiles each engine to a native binary by including the engine `.c` as a single
TU (`-DENGINE_SRC=...`) and macro-renaming the engine's own `memset`/`memcpy`
out of libc's way. Produces `native_bench_<engine>` and `native_cli_<engine>`
binaries, runs the same five canonical benchmarks in-process best-of-25.

**Alternatives**:
- Trust the wasm numbers as-is (rejected — leaves H2 unresolved).
- Test under a different wasm runtime (wasmtime, wasm3) — would isolate the
  JIT but not the substrate. Doesn't answer "what does C alone cost?".
- Single-engine native bench — cheaper but loses the cross-engine comparison.

**Consequences**:
- ~250 lines of new code (`native/bench.c`, `native/main.c`, build.sh
  extension); regenerable artifacts gitignored.
- We can now decompose every wasm measurement into engine cost + substrate
  cost. Three immediate refinements (see FINDINGS.md "Native build" section):
  1. wasm-on-V8 overhead is 1.1×–1.6× — smaller than engine-design gaps.
  2. The JIT *flattens* engine differences (CEK gets ~1.11× overhead because
     its tight musttail loop is V8's sweet spot; bytecode gets ~1.4× because
     its dispatch loop has more interpreter machinery V8 has to be conservative
     about).
  3. GC overhead has a native floor of ~1.3× (the genuine compiler-level
     optimization barrier), amplified by V8 to ~1.4×. H2's mechanism story is
     confirmed; the magnitude is now decomposed.
- Future engine work can A/B native first (faster iteration, no JIT noise)
  then verify wasm.
- Native CLI doubles as a no-Node way to run programs (useful if the Docker
  ticket ever lands without bundling Node).

## ADR-002: User-lambda arity check + `cond` + `define`-shorthand, all eight engines (2026-05-30)

**Context**: The surface language was minimal by design (six special forms,
eleven primitives, no `cond`, no `(define (f x) ...)` shorthand), and arity
on user lambdas was unchecked — under-supply silently bound missing params
to `()`, over-supply silently dropped. Silent failure crossed the line from
"minimal-validation aesthetic" into "footgun" for anything past a fib loop.

**Decision**: Add three changes to all eight engines:

1. Arity check at call time for user closures (return `<error>` on mismatch).
   Primitives stay unchecked — they have legitimate variable-arity use
   (`(+ 1 2 3)` collapses naturally through repeated `apply_prim`) and
   matching `tinylisp/mal` on the prim side keeps the engines small.
2. `(define (f a b) body)` shorthand desugaring to `(define f (lambda (a b) body))`.
3. `cond` with `else` clause head, desugaring to nested `if`.

Tree-walkers handle (2)/(3) inline in `eval`; the CEK engines desugar and
re-tail-call `eval_expr` (the GC variant roots the in-flight nested-if via
`R_save`); bytecode rewrites the AST in `compile()`.

**Alternatives considered**:
- Float support — rejected (32-bit tagged values have no room for a `double`
  without NaN-boxing all engines or heap-boxing each float; printing alone
  is ~500 lines of Grisu/Ryū; and floats wouldn't change any of the integer
  fib/loop/list benchmark ratios that drive FINDINGS).
- Variadic lambdas (`(lambda (x . rest) ...)`) — bigger lift (dotted-pair
  reader + binder tail-gather) and not currently load-bearing.
- Multi-body lambdas — small and reasonable, deferred until something needs it.
- Apply changes only to `bytecode_gc.c` — rejected because parity is the
  project's invariant; a feature that exists in one engine isn't "the
  language."

**Consequences**:
- Surface language is now `quote if define lambda let begin cond` with
  function-shorthand `define` and `else`-clause `cond`.
- Silent arity bugs in user code now surface as `<error>`.
- Parity gains 12 programs (`harness/parity.mjs` — 55 total, up from 43);
  bytecode tests gain 12 cases (`harness/test_bc.mjs` — 35 unique cases).
- No measurable bench regression — desugarings are AST/compile-time only,
  and the arity check is a single integer compare at call entry.
- Engine files grew 19–42 lines each (267 insertions across 8 engine .c
  files + the two harness files).

## ADR-003: Path to an interactive terminal / TUI-game experience (2026-06-08)

**Context**: A partner team (xsofy + let-go) dropped a pointer doc in
`docs/notes/tui-game-pointers-from-xsofy-letgo-2026-06-08.md` —
lessons from shipping a TUI roguelike on a homegrown Lisp. It prompted the
question: how far is wallisp from offering (a) a terminal REPL experience and
(b) an xterm.js TUI game?

The triage (full roadmap in `terminal_game_roadmap.md`) found the distance is
governed by one fact: wallisp's host contract is **one-shot, stateless,
zero-imports** — `eval_source` re-`init()`s every call, modules import nothing,
and there is no input/terminal/bitwise surface. Zero-imports is currently
**load-bearing to the project's identity** ("libraries, not commands", DEV.md).

**Decision**: Treat the terminal/game track as an **explicit opt-in**, not
default mission creep, and pin the approach so it doesn't drift:

1. **Preserve zero-imports.** Drive everything through the two existing memory
   buffers plus a **host-driven tick loop** (JS reads key → calls eval →
   flushes `outbuf` to the terminal/xterm.js). The wasm emits ANSI into
   `outbuf`; input bytes arrive via `inbuf`. No FFI.
2. **Sidestep SharedArrayBuffer/COI.** Because no read blocks *inside* the
   wasm, the partner doc's pointer #7 (SAB + cross-origin isolation +
   `coi-serviceworker.js`) is unnecessary — that machinery only exists to
   support blocking input in let-go's Go VM.
3. **Critical path, in order:** persistent session (`eval_persistent` seam) →
   `(read-key)` input prim → `(term/write)`/`(term/move-cursor)` streaming
   output prim → fix `bytecode_gc`'s incomplete string-heap reclamation.
4. **Sequence Milestone A (REPL) before B (game).** A is days and mostly host
   glue; B is weeks, dominated by the string-reclamation work.

**Alternatives considered**:
- **Blocking input via SAB/COI** (the partner doc's recipe) — rejected; our
  one-shot eval model makes it unnecessary, and it's the fiddliest part of
  their stack.
- **FFI / host imports for terminal I/O** — rejected; breaks the zero-imports
  identity that the whole study rests on.
- **Do nothing** — viable; this is a study repo, not a game repo. Left open in
  the roadmap as "does wallisp want this surface at all, or does the track live
  in a fork/downstream repo?" This ADR records *how* we'd do it if we choose
  to, not a commitment to ship.

**Consequences**:
- A concrete, measure-don't-guess-compatible roadmap exists
  (`terminal_game_roadmap.md`) with two milestones, gap lists, effort, and a
  per-pointer triage of the partner doc.
- The string-reclamation gap (B4) is now named as the single real blocker for
  a game, separable from the small enabling seams (A1/B1–B3).
- Current I/O-surface facts are pinned in `key_facts.md` so the "we have no
  input/terminal/bitwise prims" baseline doesn't have to be re-derived.
- Next concrete step when picked up: prototype Milestone A's `eval_persistent`
  seam + a minimal Node REPL driver, to validate the session change is as small
  as claimed.

## ADR-004: Explicit region-drop for transient strings, not automatic reclamation (2026-06-08)

**Context**: The render slice (`render_slice_plan.md`) quantified B4 — the
`bytecode_gc` string heap is a pure bump pointer (`gc()` resets mark bits but
frees nothing), so a per-frame string render leaks ~128 bytes/turn and exhausts
1 MB in ~8,200 frames. We needed reclamation of short-lived per-frame strings.

**Decision**: Add two primitives — `(strheap-mark)` returns the current heap
top, `(strheap-reset m)` drops the heap back to a mark — and have the render
loop bracket each frame with them. O(1) region-drop (tinylisp/`lisp_region.c`
lineage). The mark is captured *after* definitions, so string *literals* (which
`compile` allocates into the heap at define time and `OP_CONST` references from
persistent bytecode) sit below the mark and are preserved; only per-frame
scratch above the mark is dropped.

**Why not automatic per-eval reclamation** (the tempting "just reset each
frame"): worked through three failures before choosing explicit —
- **Reset-to-0 corrupts literals.** `"@"`/`"."`/`"\n"` are allocated at define
  time and live in persistent bytecode; resetting below them clobbers them on
  the next allocation. Any reset point must be a post-setup watermark, which
  needs a signal — exactly what `strheap-mark` provides.
- **Cheap escape-detection is unsafe OR useless.** Flagging only user
  `cons`/`set!`/`define` of a string misses strings *captured in a closure*
  that's then stored to a global (env frames are built by the VM, not user
  `cons`) → dangling pointer. Flagging *internal* `cons` instead is safe but
  trips on every transient primitive arg-list (`string-append`'s args get
  consed), so a pure render tenures every frame and reclaims nothing.
- **Sound automatic reclamation needs a compactor.** Live strings are
  interspersed with dead ones, so reclaiming them requires sliding survivors
  and rewriting wrapper offsets (a moving collector for the side heap). That's
  the heavier "medium" work the roadmap flagged; deferred.

**Alternatives considered**:
- **Mark-compact the strheap in `gc()`** — the general fix; correct and
  automatic but ~50–80 lines (forwarding/sort, root the `OP_CONST` constants).
  Deferred until persistent-string churn (below) actually bites.
- **A host opt-in "frame mode" that resets to a fixed base** — still needs the
  post-setup base, i.e. the same watermark; explicit primitives are more
  flexible (per-region within an eval) for the same cost.

**Consequences**:
- Per-frame render leak is fixed under the discipline: `render_probe.mjs` shows
  `strheap_used` flat at 36 bytes across **100,000 frames** (vs exhaustion at
  ~8,200 without it). Works in both execution models (host-driven one-eval-per-
  frame and a single-eval recursive loop) because the boundary is per-turn, not
  per-eval.
- **Caller contract** (documented at the primitive and in the plan): no string
  allocated after the mark may still be reachable after the reset — its wrapper
  offset would dangle. `strheap-reset` validates the mark is a fixnum in
  `[0, top]`, but cannot detect a live escapee; that's the program's
  responsibility, as with any region allocator.
- **Residual leak — persistent-string churn.** Strings *kept* across frames
  (e.g. redefining a global string each turn) still accumulate; only the
  general compactor fixes that. Tests + `render_probe.mjs` cover the transient
  case; the churn case is the trigger to revisit the compactor.
- Tests: `test_session.mjs` 30/30 (+6 strheap checks incl. the literal-survives-
  reset trap); `test_bc.mjs` 70/70; zero imports preserved.

## ADR-005: Run-without-recompile (input slots + rerun) for unbounded play (2026-06-08)

**Context**: The B5 host game loop (`harness/game.mjs`) drove each turn with a
fresh `eval_persistent("(turn dx dy)")`, which *appends* bytecode — Milestone A
never resets `cp` so earlier closures keep valid body pointers. Measured budget:
**~7,167 turns** before `code[]` (65536 words) fills and turns return `<error>`.
The strheap region reset (ADR-004) had already removed the string leak, so
`code[]` was the *only* remaining per-tick growth vector blocking unbounded play.

**Decision**: Compile the per-frame tick **once**, then re-run that same bytecode
every frame without compiling anything new. Two additive pieces in `bytecode_gc`:
- **Input slots + `(input i)`** — a host-writable `i32 g_input[8]`, exposed via
  the `input_slots_ptr()` export; `(input i)` reads slot `i`. The game's
  `(tick)` reads `(input 0)`/`(input 1)` instead of literal args, so the action
  arrives *without* an eval. This is the B2 input primitive the roadmap dropped
  — genuinely earned here, where the alternative (an eval to set a global) is
  exactly the recompile we're removing.
- **`rerun(entry)` export** — re-executes compiled bytecode from `entry`
  (obtained from `last_entry()` after the one-time `(tick)` compile) with the
  per-eval scratch reset but **`cp` untouched**. `run()` already re-zeroes the
  operand/call stacks + env on entry, and all game state lives in globals +
  input slots, so re-running the identical `LOADG tick; CALL 0; HALT` bytecode
  re-invokes the tick against fresh input each frame.

**Why this works / why it's bounded now**: every per-tick growth vector is
closed — `code[]` (rerun doesn't compile), strheap (per-frame region reset),
cons arena (GC reclaims dead state tuples), symbols (no runtime interning),
outbuf (reset each call). **Verified at 1,000,000 ticks**: no `<error>`,
`strheap_used` flat at 132, ~53k ticks/s.

**Alternatives considered**:
- **Reset `cp` per turn** — reintroduces the Milestone A closure-clobber bug
  (closures point into `code[]`). Rejected.
- **A bytecode compactor** (reclaim dead top-level forms) — general but heavy,
  and unnecessary once the tick is compiled once and reused.
- **Invoke the tick closure directly from the host** (skip even the `(tick)`
  trampoline) — would mean replicating the VM's call-frame setup in C; re-running
  the tiny compiled `(tick)` form is simpler and reuses `run()` unchanged.

**Consequences**:
- `harness/game.mjs` now drives via `rerun` + input slots; play is unbounded.
  `examples/coin2d.lisp` gained `(define (tick) (turn (input 0) (input 1)))`.
- The host writes input by poking `input_slots_ptr()`; it must **refetch the
  typed-array view** each use in case wasm memory moved (the driver does).
- `eval_source` / `eval_persistent` unchanged; `rerun` guards `entry < cp`.
- Tests: `test_session.mjs` 35/35 (+5 input/rerun checks); `test_bc.mjs` 70/70;
  zero imports preserved; root wasm rebuilt in the same commit.
- This closes the engine work for a real-time terminal game. Remaining is host
  only (the xterm.js flavour) and the deferred strheap mark-compactor (ADR-004)
  for persistent-string churn, which a transient-frame game doesn't hit.
