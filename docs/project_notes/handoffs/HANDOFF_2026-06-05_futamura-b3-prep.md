# Session Handoff

**Created:** 2026-06-05
**Session ID:** 5fddad8c-25b4-4da7-99e3-d70a52662cd0
**Working Directory:** /Users/matt/projects-new/wallisp

## What to read first

The H9/H10 partial-evaluation thread shipped this session — see `FINDINGS.md` (H9 + H10), `docs/futamura.html`, and `prototype/futamura/`. The specializer was then incrementally extended through three closure-support milestones (v1 / v2-extended / v2-deep). **Next planned step is B-v3 (cons-cell support).** The ceiling on the thread isn't engineering effort — it's the **supercompilation problem in the metacircular eval's Y combinator** (see Gotchas). Don't aim for the metacircular eval as a B-v3 milestone; aim for `nrev+sum` shape programs.

## Summary

Built two ways to remove the eval loop from wallisp (Futamura projection #1 to C/wasm, and `$`-marked source-to-source PE), both pre-registered at 3–5× and both measured at 50–65×. Wrote them up as H9 + H10 in `FINDINGS.md` and as a standalone editorial in `docs/futamura.html`. Then extended the H9 specializer through three closure milestones — `let` + inline lambda (v1), closures as first-class spec-time values with `make-adder` working (v2-extended), and `(define add5 (make-adder 5))` top-level closure binding (v2-deep). All measurements track H9's ~50× structural ratio. Documented the open thread as B-v3 (cons cells).

## Current State

**Branch:** `main`, **3 commits ahead of origin**, working tree clean.

```
4a6a52e  feat(h9): closures-v2-deep — top-level (define NAME EXPR) for closures
c70947a  feat(h9): closures-v2 — first-class spec-time closures, make-adder works
fa075a7  feat(h9): closures-v1 — let + inline-lambda specialization
bcd99d6  docs(index): link to futamura.html + bump arc count to ten
938d963  docs(futamura): standalone page covering H9 + H10
1fe4cb9  feat(h10): preproc --trace flag — log each $-fold to stderr
c153ce5  findings(h10): $-PE folds (sum-to 100) at preprocess time — 60-69× on the demo
37b5055  feat(h10): $-marked source-to-source PE with hard-fail comptime folding
138d2a5  findings(h9): PE residual is 50-65× over bytecode_gc on no-allocation programs
022943a  feat(h9): Futamura specializer (Lisp → C residualizer) + bench harness
```

The first 6 (bcd99d6 and below) are already pushed; the top 3 (`fa075a7`, `c70947a`, `4a6a52e`) plus this handoff need a push.

Key artifacts shipped this session:
- `prototype/futamura/specialize.c` (~550 LOC) — Lisp→C residualizer with full closure support
- `prototype/futamura/preproc.c` (~340 LOC) — `$`-marked source-to-source PE with hard-fail
- `prototype/futamura/{fib,tak,closures_demo,closures_v2_demo,closures_named_demo}.lisp` — bench programs
- `harness/{bench_futamura,bench_preproc}.mjs` — self-bootstrapping benches
- `docs/futamura.html` — standalone editorial covering H9 + H10
- `FINDINGS.md` — H9 + H10 entries (now 10 hypothesis arcs total)

## Uncommitted State / Untouched

**Uncommitted:** none. Working tree was clean before this handoff.

**Untouched (deliberate):**
- **Metacircular eval target.** Not approached; deliberate. See Gotchas.
- **H10 (`preproc.c`) closure support.** B-v2 only landed in the H9 specializer (`specialize.c`), not in the source-to-source preproc. The H10 path would benefit from the same eval_ct generalization but wasn't extended in this session. Lower priority than B-v3.
- **Cross-runtime measurement (Bun/JSC).** Existing harness infrastructure supports it cheaply. Not done this session; listed as an open follow-up in `FINDINGS.md` H9.
- **`docs/style.css` extraction.** `docs/index.html` and `docs/futamura.html` duplicate ~280 lines of CSS. Pays off only if a third page lands.

## In Progress

Nothing literally in flight. B-v3 is the planned next milestone but no code exists for it.

## Gotchas

- **The metacircular eval is supercompilation territory, not B-v3 territory.** `baselines/metacircular.lisp` uses a Y combinator: `(lambda (x) (f (lambda (v) ((x x) v))))`. PE'ing this without recognizing the recursive structure would infinitely unfold the self-application. Real PE systems handle this via *polyvariant specialization with memoization* or *supercompilation* — PhD-thesis-level work, not a weekend project. **Don't try to PE the metacircular eval as part of B-v3.** Aim for `nrev+sum`-shape programs (cons-cell-heavy, finite-loop, no self-applying closures).
- **`eval_ct_fuel` is a global state.** It MUST be reset to `EVAL_CT_FUEL` before each entry from `emit_expr` (search for `eval_ct_fuel = EVAL_CT_FUEL;` — there are two sites in `specialize.c`). If you add a new call site that invokes `eval_ct` without resetting, prior fuel exhaustion silently propagates.
- **Captured-env runtime bindings depend on C scope.** When a closure captures a `BV_RUNTIME` binding (a C local), the beta-reduce emit happens *inside the same C function* where the binding is declared. If you ever try to lift a closure to a top-level C function (e.g., to handle a closure that escapes its creating scope), you'd need to pass captured state explicitly. Not handled today; closures that escape errors out with "closure escaped to a runtime value position."
- **`({ stmt; expr; })` statement expressions are clang/GCC-specific.** Used by `bind_and_emit_body` for runtime let RHSs. If we ever change the wasm toolchain away from clang, those break. Not a concern today (clang is the toolchain), but worth knowing.
- **Comptime-only define detection works at `record_define` time.** A `(define (foo args) (lambda ...))` whose body IS itself a lambda is flagged `comptime_only` and skipped at emission. This relies on the body being syntactically a `(lambda ...)` form at the top level of the body. If you add macros or syntactic sugar that produces lambdas indirectly, the detection won't fire.
- **Regression discipline.** Every closure milestone (v1, v2, v2-deep) preserved byte-identical wasm for the previous demos. `cmp` is the regression test. If you change `emit_expr` or `eval_ct`, **always re-run** `cmp /tmp/regen.c prototype/futamura/build/residual_*_gen.c` for fib, tak, closures-v1, closures-v2, and closures-named before committing. Multiple iterations in this session caught bugs this way.
- **The H9 number (50–65×) is structural; the H10 number (60–69×) is reuse-dependent.** Don't conflate them when writing future findings. The H9 ratio reflects "no eval loop" and benefits every program; H10 reflects "this comptime value is reused N times" and shrinks linearly if N=1. The futamura.html essay's §4 comparison table is the canonical framing.
- **Push needed.** Three commits ahead of origin. The handoff commit (this file) will make four.

## Next Steps

The user wants **B-v3 (cons-cell support in the specializer)** as the next milestone. Plan:

1. **Add cons-cell representation to the residual C.** Today the residual works on tagged fixnums only (`u32` everywhere). For cons, we need a cell arena in the residual: a `Cell[N]` array with `head` / `tail` fields, plus the tag-as-cons encoding (`mkcons(i)` / `considx(v)`). Match `engines/lisp.c`'s 30-bit tagged value rep so cross-validation against the engines stays clean. Look at `engines/lisp.c` lines 22–80 for the canonical encoding.
2. **Extend `eval_ct` to fold `cons` / `car` / `cdr` at spec time.** SVal needs a new variant: `SV_CONS` carrying a list-of-SVal (or an internal cons-tree of SVals). When all args to `cons` are comptime, return SV_CONS. `car` of an SV_CONS returns its head; `cdr` returns its tail. `null?` and `pair?` similarly.
3. **Emit comptime cons-tree as literal initializer.** If a comptime cons-tree (SV_CONS) needs to be materialized into runtime — e.g., the residual stores it in a runtime variable that's then walked at runtime — emit a static const array of cells initializing the structure. This is where it gets fiddly: cons-trees can be deep, and emitting them as nested initializers requires care.
4. **Hard-fail on runtime cons.** If a `cons` call has any runtime arg, that's a runtime allocation — needs a `gc_alloc` or arena bump in the residual. For a first cut, hard-fail: "specialize: runtime cons not yet supported." Future work would add a runtime arena and conservative GC (cf. `engines/bytecode_gc.c`).
5. **Demo target: `nrev+sum`.** The benchmark `nrev` (naive reverse) + `lsum` shape from `harness/bench.mjs`. Iota builds a list, nrev reverses it, lsum sums it. With cons-comptime support: if the input N is comptime, the entire computation folds. If N is runtime, the residual would need runtime cons — that's the harder case.
6. **Honest predicted result for B-v3:** *Smaller* than H9's 50×, probably 3–10× over `bytecode_gc`. The H9 number came from "no eval loop"; for `nrev+sum`, the dominant cost shifts to heap traffic which the residual still has to do. Pre-register this prediction before measuring (per the project's measure-don't-guess norm).
7. **NOT B-v3 scope:** the metacircular eval, escaping closures, lambda-as-runtime-value, mutable cells (`set-car!`/`set-cdr!`), strings.

Concrete file targets for B-v3:
- `prototype/futamura/specialize.c` — add SV_CONS, cons-tree storage, comptime-cons emission
- `prototype/futamura/nrev_demo.lisp` — bench source
- `prototype/futamura/build.sh` — wire the new demo
- `harness/bench_futamura.mjs` — bench row
- `FINDINGS.md` — H11 entry once measured (with pre-registered prediction)

Cheaper alternative if B-v3 feels too big: **cross-runtime measurement** (`bench_futamura.mjs` under Bun/JSC). Existing harness pattern, ~30 min, falsifies (or doesn't) the "V8-amplifies-the-PE-win" story.

## Open Tickets

None new from this session. The `FINDINGS.md` H9 + H10 "Open questions" sections list the follow-up axes (allocation, closures, cross-runtime, composition) — those are documentation, not tickets.
