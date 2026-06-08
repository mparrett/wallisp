# Terminal / TUI-game roadmap (2026-06-08)

> High-level roadmap for taking wallisp from a one-shot expression evaluator to
> an interactive terminal experience, and eventually an xterm.js TUI game.
> Triggered by a partner-team pointer doc dropped in
> `docs/project_incoming/tui-game-pointers-from-xsofy-letgo-2026-06-08.md`
> (lessons from **xsofy** + **let-go**). The decision to pursue this as an
> explicit opt-in track — and *how* — is ADR-003 in `decisions.md`. This
> document is the roadmap; ADR-003 is the rationale.

## The one defining constraint

The whole host contract is **one-shot, stateless, zero-imports**:

- Write source into `inbuf` → `eval_source(len)` → read a printed result
  string from `outbuf`.
- `eval_source` calls `init()` **every time** — there is no persistent session
  (standalone `README.md` says so explicitly).
- Modules have **zero imports** by design — "libraries, not commands" (DEV.md).

Confirmed by grep (2026-06-08): no input/read-key primitive, no terminal /
ANSI / cursor handling, no bitwise ops, and the web showcase calls
`eval_source` once per click with no state carried over. Everything below
flows from relaxing this constraint — see `key_facts.md` "Interactive / I/O
surface (current state)".

**Key insight:** xterm.js is a *natural* host for a zero-imports module. The
wasm emits ANSI into `outbuf`; JS flushes it to the terminal; xterm input
events get written into `inbuf`. No FFI needed — the two memory buffers plus a
host-driven loop are the whole interface. So "game" is closer than the
architecture's purity suggests; the question is whether wallisp *wants* an I/O
surface, since zero-imports is load-bearing to its identity (that's the ADR).

---

## Milestone A — interactive REPL ("just a terminal experience")

**Status: PROTOTYPED (2026-06-08).** Shipped in `bytecode_gc` + a Node REPL.
Was close, as predicted — the engine seam is ~25 lines.

- **A1. Persistent session — DONE.** Added three additive exports to
  `engines/bytecode_gc.c`: `reset_session()` (starts/clears a session),
  `eval_persistent(len)` (keeps globals, interned symbols, the cons arena, and
  the string heap across calls), and `eval_source` factored over a shared
  `run_buffer()` helper but left behavior-identical. Globals already live in
  `gval[]` (GC-rooted) and `run()` re-zeroes the operand/call stacks + env on
  entry, so persistence was mostly about *not* calling `init()`.
  - Driver: `harness/repl.mjs` (readline → `eval_persistent` → print; `:reset`).
  - Test: `harness/test_session.mjs` (18 checks); existing `test_bc.mjs` 70/70
    unchanged (eval_source untouched).

**One non-obvious thing the prototype surfaced:** closures store a *code
address* into the shared `code[]` buffer, so a naive "`cp=0` per call" reset
clobbers the bytecode of closures defined on earlier lines → `(f 10)` after
`(define (f ...) ...)` returns `<error>`. Fix: persistent mode *appends*
bytecode (records an `entry` offset, `run(entry)`), never resets `cp`.

**Known limits of the prototype (acceptable; revisit if a session gets long):**
- `code[]` is bounded (`CODE_MAX` = 65536 words). Appending per line never
  reclaims dead top-level code, so a very long session eventually fills it —
  `emit` then sets `g_cerr` and evals return `<error>` (bounded, no
  corruption). A bytecode compactor is out of scope.
- **Don't interleave `eval_source` and `eval_persistent` on one instance** —
  `eval_source` calls `init()` and wipes the session by contract. Pick one mode
  per instance (the REPL only ever calls `eval_persistent`).
- GC-during-compile risk (below) is unchanged.

Remaining for a "real" Milestone A (beyond prototype): tty niceties
(multi-line input, history, arena/`code[]` pressure surfaced to the user).

---

## Milestone B — xterm.js TUI game (real-time loop)

**Distance: weeks, dominated by one item (B4).** Gaps in dependency order:

- **B1. Persistent state across ticks** — same fix as A1. (engine, small)
- **B2. Input primitive** — a `(read-key)` that reads host-fed bytes from a
  memory slot. New ABI + primitive. (engine, small–medium)
- **B3. Streaming output** — `(term/write …)` / `(term/move-cursor …)` that
  *append* ANSI to `outbuf` mid-eval, rather than one printed return value at
  the end. New primitive; host flushes each tick. (engine, small–medium)
- **B4. Strings with real reclamation — THE BLOCKER.** Only `bytecode_gc` has
  strings, and its side-heap reclamation is **incomplete** (that's why
  `standalone/` lifted them out). A game allocates strings every frame → the
  known leak becomes fatal. Needs the free-list/compactor the standalone
  README already flags. (engine, medium — this is the real work.)
- **B5. Host game-loop driver** in JS wiring xterm.js (input → eval → flush).
  (host, medium)

### The SAB nuance (vs the partner doc's pointer #7)

The doc prescribes SharedArrayBuffer + cross-origin-isolation + a
`coi-serviceworker.js`. That is only needed for *blocking* input reads from
inside wasm — let-go's Go VM blocks on input. **Our one-shot eval model
sidesteps it:** with a host-driven tick loop (JS reads key → calls eval →
flushes `outbuf`), nothing blocks inside the wasm, so **no SAB, no COI
handshake**. This removes the fiddliest part of their recipe.

---

## Triage of the partner doc's 10 pointers

| # | Pointer | Status for wallisp |
|---|---------|--------------------|
| 1 | Pure `(state,action)→state` + action log | **Free.** We're a pure evaluator — ideal host. Lives in the game program, not the engine. |
| 2 | Thread RNG as a value | **Adoptable today** — an LCG needs only `* + mod`, which we have. (No bitwise → no xorshift/PCG.) |
| 3 | Action log as ABI / versioning | Program-level; needs robust strings (B4) if logs are serialized as text. |
| 4 | Headless-safe `term/size` | N/A yet — no terminal primitives to make safe. Relevant once B3 lands. |
| 5 | Pin runtime in CI + drift smoke | Cheap CI hygiene; orthogonal to engine. |
| 6 | Dirty-region render + full-redraw fallback | Game-program concern; only after B3 exists. |
| 7 | WASM + xterm.js + COI + SAB | **Partially sidestepped** — host-driven loop avoids SAB/COI (see nuance above). |
| 8 | Native bindings for hot fns before caches | **Already our idiom** — a "native binding" is a C primitive in the engine; adding prims is well-trodden here. |
| 9 | Host/template fork tax | **We just lived this** — echoes our shared-reader / embedded-blob coupling (see `key_facts.md` tracked-wasm rule). |
| 10 | Optional mobile shell wrapper | Far downstream; only once B exists. |

---

## Headline

The choices that make wallisp a clean *study* — zero imports, stateless
one-shot eval, no I/O, fixnum-only — are exactly the ones a terminal/game needs
to relax. None are individually hard. The critical path is:

**persistent session (A1/B1) → input prim (B2) → streaming-output prim (B3) →
fix string reclamation (B4).**

The first three are small; string reclamation is the one genuine chunk of work.

## Sequencing

```
Milestone A — interactive REPL
  A1 persistent session (eval_persistent seam)         [engine, small]
  + Node tty/readline driver                           [host, small]

Milestone B — xterm.js TUI game        (do A first)
  B1 persistent state across ticks (= A1)              [engine, small]
  B2 (read-key) input primitive + ABI slot             [engine, small–medium]
  B3 (term/write)/(term/move-cursor) streaming ANSI    [engine, small–medium]
  B4 strings with real reclamation  ← THE BLOCKER      [engine, medium]
  B5 xterm.js host game-loop driver                    [host, medium]
```

Open question for the ADR/owner: does wallisp *want* to grow this I/O surface
at all, or does the terminal/game track live in a fork or downstream repo so
the study repo keeps its zero-imports identity? See ADR-003.
