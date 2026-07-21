# Legs vs toy — audit (2026-05-31)

> **Snapshot as of 2026-05-31.** Several gaps named here have since shipped:
> primitive validation (PR1, all 8 engines), strings (EXP1, `bytecode_gc`
> only), `call/cc` (EXP2, CEK engines). For current language surface and
> findings, see `FINDINGS.md`, `ENGINES.md`, and `docs/index.html` §9.
> The audit is preserved as a historical reference point, not as a
> live status doc.

Honest read of where wallisp is research-grade and where it's a teaching toy,
with cost/benefit on each gap. Pairs with `gap_closure_plan.md` for action.

## Where it has legs

- **Measurement-driven design with a falsification ledger.** Pre-registered
  hypotheses, public failures. Env-lookup hotspot, switch dispatch, CEK's
  tail-call advantage, "GC overhead is collection cost" — all looked obvious,
  all got refuted by data. That discipline is rare in hobby compilers.
- **8-engine matrix with cross-engine parity as CI.** 55 programs × 8 engines
  forced to agree means engine drift gets caught immediately. The matrix is
  what made the three-way GC-tax decomposition possible (`bytecode_gc` 1.05× /
  `lisp_gc` 1.34× / `cek_gc` 1.83×) and the mapping to V8 JIT-specializability
  is a genuinely novel finding.
- **Region-drop beating no-GC by 6%** (`lisp_region.c`). Replicable,
  counterintuitive, well-explained (compare-shape difference). The kind of
  result that earns trust.
- **Zero-import freestanding wasm in ~400 lines each.** Eight engines bench
  together, all run in a single self-contained HTML page. The smallness is
  what makes the A/B honest.
- **Hand-WAT round-tripping demonstrated as workable for instrumentation**
  (the icount probe). The "additive edits in WAT, logic in C" split is a real
  workflow, not just a stunt.
- **Native build as a confound-stripper for V8** (ADR-001). Lets the project
  make claims about *mechanism*, not just numbers.

## Where it's obviously a toy

- **No strings.** *The* defining toy-tell. Can't read user input, format
  errors, or write any program that touches text. Breaks the "uniform cons
  cell" arena invariant the mark-sweep GC depends on.
- **30-bit fixnums that wrap silently.** `tailsum(50000)` already silently
  wraps from 1.25B to 176M; all eight engines agree, so parity passes —
  "consistent, not correct."
- **No floats, no division, no mod.** ADR-002 rejected floats with good
  reasoning; div/mod are just missing.
- **Primitive validation is absent.** `(+ 1)` → `1`, `(+ 'a 1)` → empty,
  `(car nil)` → undefined. Codex review flagged this; user-lambda arity check
  shipped, primitives didn't.
- **No mutation.** No `set!`, no `set-car!`. Memoization, accumulators-via-
  mutation, iterative loops in the Scheme sense — all unavailable.
- **No `apply`, `call/cc`, varargs, multi-body lambdas, `letrec`, macros.**
  Language is `quote if define lambda let begin cond` plus eleven primitives.
  Tinylisp territory.
- **Error model is one bit.** Bare `<error>` token; no position, no message,
  no traceback. Reader silently tolerates a missing `)`.
- **No host interaction beyond `eval_source`.** "Zero imports" is load-bearing
  for the measurement thesis but means the wasm modules are libraries you call
  from JS, not programs.
- **Fixed-size arenas, no growth.** Hit `MAX_CELLS`, you OOM.
- **`bc_super` silently disagrees on primitive rebinding.** Documented; lives
  in `prototype/`, not shipped.

## Gap economics

### Worth doing — small cost, fixes real problems, doesn't dilute thesis

- **Primitive arity + type checks.** ~30 lines × 8 engines. Removes the
  most-cited footgun. The user-arity check already shipped without measurable
  bench regression. Motivation: intellectual-honesty brand is undermined every
  time someone runs `(+ 1)` and gets `1`.
- **`set!` and `set-car!` / `set-cdr!`.** ~15 lines per tree-walker, one new
  `OP_STORE` for bytecode. Lifts language from "purely functional accident"
  to "actual mutable Lisp." Opens benchmark shapes (memoized fib, in-place
  rotation) currently unwriteable.
- **Division / mod / bit ops.** ~5 lines per op per engine. No design cost.
  Removes "wait, really?" reaction from readers.
- **32-bit fixnums or overflow detection.** Either lift to a different tag
  scheme or trap on overflow. Silent wrap is a *measurement hazard*, not just
  a usability gap — already silently invalidated one benchmark.

### Worth doing for one engine, not eight — explicitly break parity

- **Strings (bytecode_gc only).** Either separate string heap or type-tagged
  GC sweep. ~200 lines. Cost: parity story gains a `*` ("strings: bytecode_gc
  only"). Benefit: finalist becomes embeddable. Required if anyone is meant to
  *use* this beyond reading the writeup.
- **`call/cc` (CEK exclusive).** CEK gets it nearly for free (kontinuation is
  already first-class internally); bytecode would need stack-snapshotting.
  Motivation: gives CEK an actual exclusive capability beyond "deep non-tail
  recursion" — closes the "what is CEK *for*" question the project quietly
  leaves open.

### Skip — closing them dilutes thesis without paying for itself

- **Floats.** ADR-002 reasoning still holds.
- **Macros / hygiene / module system.** Multi-week lifts that push engines
  past the ~500-line ceiling that makes A/B honest. Different project.
- **Error positions / traceback.** Threading spans through reader → AST →
  bytecode is invasive; benefit is usability, not what this project optimizes.
- **Wasm host imports for I/O.** Kills the "zero imports" property that's
  load-bearing for the measurement claim. Build-flag fork = not a feature.
- **Moving / compacting GC.** Every index becomes rewritable; existing
  non-moving sweep already at 1.05× wasm in `bytecode_gc`. Diminishing returns.

## Framing tension underneath all of this

This project's actual value is the *negative results* — CEK didn't help,
switch dispatch didn't matter, env lookup wasn't the bottleneck, GC tax is
JIT-amplification not collection. Every gap-closure that adds language
features *without adding a measurement axis* dilutes that brand. Gap-closures
that *open* new measurement axes (strings → "how does a non-uniform heap
interact with mark-sweep"; `call/cc` → "does CEK's continuation-as-data
finally pay off") sharpen it.

Recommendation summary:
- Tier A (bugfixes): primitive validation, `set!`, div/mod, 32-bit fixnums.
- Tier B (new experiments with pre-registered hypotheses): strings in
  `bytecode_gc`, `call/cc` in CEK.
- Tier C (skip): everything else.
