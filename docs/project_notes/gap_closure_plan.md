# Gap-closure plan (2026-05-31)

Acts on `legs_vs_toy_audit.md`. Ordered by leverage; each item shaped as a
landable PR with a pre-registered prediction so we don't drift from the
project's measure-don't-guess discipline.

## Sequencing

```
Tier A (bugfixes, preserve 8-engine parity) ──┐
  PR1 prim-validation + div/mod + overflow    │
  PR2 set! / set-car! / set-cdr!              ├─ ship in this order
  PR3 (conditional) 32-bit fixnums            │
                                              │
Tier B (new experiments, intentionally break parity) ┤
  EXP1 strings in bytecode_gc                 │
  EXP2 call/cc in CEK                         │
```

Tier A first — it removes footguns the codex review flagged and unblocks any
benchmark that needs mutation. Tier B is research with its own pre-registered
hypotheses; do those only after Tier A has stabilised.

---

## PR1 — primitive validation + div/mod + arithmetic overflow

**Scope.** Three sub-PRs: pilot on two engines to settle the shape, then port
the remaining six in one mechanical pass. Total surface is each engine's
`apply_prim` (or equivalent) plus the bytecode VM's prim fast paths.

**Changes (constant across sub-PRs).**
1. Arity check on every primitive: `(+ 1)`, `(car)`, `(cons 1)` → `<error>`.
   `+` `-` `*` stay variadic but require ≥2 args (matches Scheme; current
   `(+)` → `0` becomes `<error>` only if we choose strict — flag in PR).
2. Type check on primitive arguments: `(+ 'a 1)`, `(car 5)`, `(< 'a 'b)` →
   `<error>` instead of silent garbage.
3. Add `/` (truncating div), `mod` (remainder). ~5 lines per engine each.
4. Trap arithmetic overflow at the prim level. Detect when result exceeds
   30-bit signed range; return `<error>`. Cheaper than retagging to 32-bit
   (PR3) and gives the safety property today.

### Sub-sequence

**PR1a — `lisp.c` only.** Settles the *semantics* questions cheaply: where in
`eval` the check sits, how `<error>` propagates through `eval_list`, exact
parity contract for `(+ 1)` vs `(+ 1 2 3)`. The other tree-walker engines
(`lisp_trampoline`, `lisp_gc`, `lisp_region`) port by near-copy-paste once
this lands. `harness/parity.mjs` adds a `SUPPORTS_PR1` gate so the new
error-expected programs only check against piloted engines; existing 55
programs keep running against all 8.

**PR1b — `bytecode_gc.c`.** Settles the *implementation* questions on the
hardest substrate: coexistence with the inline-prim fast path, any new GC
roots, whether V8 specialisation holds. This is the finalist; if the design
breaks anywhere it breaks here. CEK and the no-GC bytecode are easier
substrates by comparison.

**PR1c — port to remaining 6.** `lisp_trampoline`, `lisp_gc`, `lisp_region`,
`cek`, `cek_gc`, `bytecode`. Mechanical translation of the shape settled in
PR1a/PR1b. Ungate `parity.mjs`; the new programs go back to all-engine
agreement.

**Files touched.** All 8 of `engines/*.c` (one per sub-PR);
`harness/parity.mjs` and `harness/test_bc.mjs` gain programs in PR1a, gate
relaxes in PR1c; `README.md` validation paragraph updated in PR1c.

**Estimated diff.** ~30–50 lines per engine × 8 = ~300–400 lines total,
split roughly 80 / 80 / 200 across PR1a / PR1b / PR1c.

**Pre-registered prediction.**
- (a) No measurable bench regression on the existing 5-benchmark suite.
  The new checks are integer compares on the cold prim path. Falsification:
  >3% slowdown on `bytecode_gc` fib(24) wasm — would force a redesign at
  PR1b before continuing.
- (b) Parity holds at 55-program count plus the new error/op programs
  (gated to piloted engines during PR1a/PR1b, all engines after PR1c).
- (c) `bc_super` continues to be the only divergent engine (the rebinding
  bug pre-dates this PR).
- (d) PR1b reveals zero new GC root issues. Falsification: any program in
  the existing GC test (`test_bc.mjs`) starts producing different output
  on `bytecode_gc` after PR1b — would mean a new allocation crept into a
  path the root set doesn't cover.

**FINDINGS update.** New short section: "Primitive validation tax (negligible)."

---

## PR2 — `set!`, `set-car!`, `set-cdr!`

**Scope.** Adds mutation. Three special forms / primitives across all 8
engines.

**Changes.**
1. Tree-walkers: new `eval_set` branch; walks the env chain and rebinds the
   cell that owns the symbol. ~15 lines.
2. CEK: new K-continuation kind for the `set!` evaluation step. ~25 lines.
3. Bytecode: `OP_STORE` (local) + `OP_STOREG` (global), emitted by the
   compiler in tail-of-`set!` position. ~25 lines.
4. `set-car!` / `set-cdr!` are pure primitives; just mutate the cell.
   ~10 lines per engine.

**Files touched.** All 8 `engines/*.c`; new programs in `harness/parity.mjs`
(memoised fib, in-place rotate, mutable counter); `DEV.md` language section.

**Estimated diff.** ~50–80 lines per engine × 8 = ~500 lines.

**Pre-registered prediction.**
- (a) Memoised fib benchmark — `(define cache ...)` + `set-car!` on a
  vector-shaped cons list — beats unmemoised fib by ≥10× at N=20 on
  `bytecode_gc`. If it doesn't, we have a `set!` correctness bug.
- (b) Engine ordering on the existing 5-benchmark suite unchanged.
- (c) GC root-set still complete: `set-car!` writing a fresh cons that points
  to a fresh cons survives a forced collection. Add as a `test_bc.mjs` case.

**FINDINGS update.** Short note on memoisation benchmark. **No new H-number**
— this is a language feature, not a measurement hypothesis.

---

## PR3 — 32-bit fixnums (conditional, only if PR1's overflow trap proves
insufficient)

**Scope.** Tag scheme change in all 8 engines.

**Open question.** Worth measuring first: of the programs we actually want to
run (current suite + metacircular fib + memoised fib), how many would hit
PR1's overflow trap? If the answer is "approximately zero," skip this.

**Possible designs.**
- (a) Lift to 31-bit fixnums by stealing one tag bit instead of two
  (cons-vs-not + fixnum-vs-other), tightening symbol/special encoding.
- (b) Box fixnums above 30 bits into a cons cell holding two halves.
  Cheap on the hot path (fast for small ints), pays on overflow.
- (c) Leave at 30-bit, trap on overflow (PR1 already does this). Document.

**Defer the decision.** Run PR1 first; collect a list of programs that trip
the trap; pick (a) / (b) / (c) based on whether the trap fires in normal use.

---

## EXP1 — strings in `bytecode_gc`

**Status.** Pre-registered experiment, not a PR yet.

**Hypothesis.** Adding a separate string heap (variable-length, freed by a
type-tagged sweep) to `bytecode_gc` will increase its GC tax above the
current 1.05× wasm baseline, because the sweep loop can no longer assume
uniform-sized objects and V8 will lose specialisation. Predicted new tax:
1.15×–1.30× wasm on fib(24).

**Falsifications.**
- New GC tax stays ≤1.10× → V8 specialised over the type-dispatch anyway;
  string heap was free. Interesting; would update the mechanism model.
- New GC tax >1.40× → the type-dispatch broke a different optimisation
  than the cons-reaches-gc one we already attributed H2 to. Worth isolating.

**Scope.** `engines/bytecode_gc.c` only. Reader/printer in the same file
gain string literal `"..."`. New primitives: `string?`, `string-length`,
`string-ref`, `string=?`, `string-append`. **Breaks parity** —
`harness/parity.mjs` either gates string programs to `bytecode_gc`, or moves
to a per-engine capability flag.

**Estimated diff.** ~250 lines in `bytecode_gc.c`; ~80 lines of harness
plumbing.

**FINDINGS section.** New H-number ("H6 — non-uniform heap GC tax").

---

## EXP2 — `call/cc` in CEK

**Status.** Pre-registered experiment, not a PR yet.

**Hypothesis.** `call/cc` exposes CEK's already-first-class K continuation
as a Lisp value. Implementation is genuinely small (~30 lines: capture K
into a value-tagged cell; on invocation, jump to the captured K).
Generator-style benchmark (`yield` in a loop, consumer pulls 1M values)
will run on CEK at competitive speed with `bytecode_gc` running the same
program *without* `call/cc` (via explicit recursion). Predicted ratio:
`cek_callcc / bytecode_gc_explicit` ≤ 1.5×.

**Falsifications.**
- Ratio >3× → `call/cc` capture cost dominates; the "K is already first-class
  internally" framing is misleading at the actual implementation level.
- CEK with `call/cc` is faster than `bytecode_gc` → finally a benchmark
  where CEK exclusively wins. Would justify CEK's continued existence in
  the matrix on speed grounds, not just capability grounds.

**Scope.** `engines/cek.c` and `engines/cek_gc.c` get `call/cc`. Bytecode
gets a stub that returns `<error>` (parity-preserving). Harness gains a
`call/cc` test gated to CEK engines.

**Estimated diff.** ~30 lines in each CEK engine; ~50 lines of harness.

**FINDINGS section.** New H-number ("H7 — does CEK finally win?").

---

## What this plan deliberately does *not* do

- No floats, NaN-boxing, or numeric tower work (ADR-002 stands).
- No macros, hygiene, or module system.
- No source-position error reporting.
- No wasm host imports (preserves "zero imports" measurement claim).
- No moving / compacting GC.

If any of these later become load-bearing for a benchmark we want to write,
revisit — but don't pre-build them.

## Sequencing rationale

PR1 first: it's a strict honesty improvement and removes the codex-review
finding without changing semantics for any *valid* program. PR2 second: it
unlocks programs (memoisation, mutable state) we currently can't write at
all, which both EXPs may want. PR3 is on-demand. Tier B experiments are
independent of each other; do whichever feels higher-value when Tier A
lands.
