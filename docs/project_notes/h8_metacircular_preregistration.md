# H8 — does bytecode_gc's win generalize to metacircular eval?

**Pre-registered 2026-06-04. Measurement pending.**

The handoff `HANDOFF_2026-06-01_tier-b-exps-falsified` left this question
open: *does `bytecode_gc`'s post-PR1 advantage over `bytecode` (and over the
tree-walker) generalize beyond direct `fib`?* The natural way to test is a
**different-shape workload** that exercises different code paths — and the
metacircular evaluator (a wallisp interpreter written in wallisp) is the
canonical one.

The evaluator is committed at `harness/mc_eval.lisp` and is known to produce
the right answer (`fib(10) = 55`) on 7 of the 8 engines; `cek.wasm` arena-
exhausts under the default arena, so the bench will use `cek_big.wasm`
(same _big-variant pattern the existing direct-workload bench uses for the
no-GC engines).

## The mechanism model being tested

`bytecode_gc`'s direct-`fib(24)` advantage over the tree-walker (~2.3-3.3×
wasm-on-V8) is attributed in `FINDINGS.md` to two things:

1. **PR1 inline arithmetic fast path.** OP_CALL with `n=2` and op in
   `PR_ADD..PR_LT` skips arg-list consing and `apply_prim` entirely,
   doing direct typed arithmetic on the operand stack.
2. **Flat dispatch.** The VM's run loop is one `br_table` over 12-14
   opcodes. V8 specializes each arm tightly; the tree-walker re-walks
   the AST cons-graph per evaluation step.

Metacircular `fib(10)` shifts the workload shape:

- Most host steps are in `mc-eval`'s outer cond chain
  (`number? / null? / symbol? / quote / if / lambda / let / app`) and in
  `mc-lookup`'s cdr-walk through the env. These are `if` and `=`-on-symbol,
  not the `+/-/*` that the inline fast path attacks.
- The arithmetic `+` and `-` from the guest's `fib` body STILL go through
  the inline fast path (eventually, via `mc-apply-prim`'s host call). But
  they're a small share of total host time.
- Per host step, allocation is heavier: env extension on every function
  call, args list cons on every application, intermediate cons in
  `mc-eval-args`. So the GC tax — H2's "cons-can-reach-gc optimization
  barrier" — applies to more of the hot loop than on direct fib.

## Pre-registered prediction

**P1 (primary):** On metacircular `fib(10)` (= 55), the ratio
`bytecode_gc_big / lisp_big` lands in **[0.40×, 0.70×]** wall-clock —
i.e. `bytecode_gc` is 1.4× to 2.5× faster than the tree-walker. **This is
narrower than the direct `fib(24)` ratio of ~0.30-0.43× (2.3-3.3× faster).**

The mechanism story: the inline arithmetic fast path is *less* of the hot
loop on metacircular than on direct, so its contribution to the ratio
shrinks. The br_table dispatch advantage persists. Net: narrower win.

### Falsification windows

| Outcome on `bytecode_gc_big / lisp_big` ratio | Interpretation |
|---|---|
| **[0.40, 0.70]** | **Confirmed.** Mechanism transfers; narrowing is the dominant signal. |
| **< 0.40** | **Falsified — preserves.** Bytecode advantage holds at ~direct levels on a very different workload. The mechanism model is wrong: something other than inline arithmetic explains the win — possibly the dispatch shape itself, or a V8 specialization not in the current model. |
| **[0.70, 0.85]** | **Confirmed weak.** Narrowing exceeded the predicted floor; arithmetic fast path matters more than thought, but mechanism direction is right. |
| **[0.85, 1.0]** | **Falsified — vanishes.** Bytecode barely faster than the tree-walker. Mechanism wrong in the other direction: the inline fast path was carrying *more* of the win than predicted; without arithmetic-heavy hot loops, bytecode loses most of its edge. |
| **> 1.0** | **Falsified — inverts.** Bytecode *slower* than the tree-walker. Compile-then-execute pays for itself only on workloads with arithmetic-heavy hot loops; metacircular is below that threshold. Would be a meaningful finding about when bytecode actually helps. |

## Secondary observations (not pre-registered with windows; descriptive)

These I'll note when running the bench but won't predict tight bands for —
they're "look-at-it" rather than falsifiable.

- **S1: CEK relative cost.** `cek_big / lisp_big` on direct fib lands at
  ~2.2×. On metacircular I expect this to *narrow* (CEK's per-step
  overhead is fixed; relative cost shrinks when each guest step takes more
  host steps). A ratio < 1.5× would be notable.
- **S2: GC tax preservation.** `bytecode_gc_big / bytecode_big` on direct
  fib lands at ~1.05×. On metacircular, with more allocation per host
  step, this could widen or stay flat. Either way it's information.
- **S3: Engine ordering.** Does the relative ordering of the 8 engines
  change between direct fib(24) and metacircular fib(10)? Preserved
  ordering would suggest the underlying mechanism (V8 specialization +
  GC-barrier optimization) is workload-shape-independent; reordering
  would tell us where workload sensitivity lives.

## What this experiment does NOT test

- **Metacircular fib's absolute speed.** This is a ratio comparison;
  metacircular eval is necessarily 50-500× slower than direct on any
  engine. That's expected and not the point.
- **Whether the metacircular evaluator is the "right" implementation.**
  It's a small subset interpreter (no `define`, `set!`, `letrec`); fib
  uses the pass-by-self idiom for recursion. A different evaluator
  shape might shift the host hot path; this measures *this* evaluator.
- **Native vs wasm.** Direct fib has both rows. Metacircular initially
  only needs wasm — the host-vs-V8 question (H2's substrate amplification)
  is separately measured.

## What "confirms" or "falsifies" looks like in practice

The bench prints a table like:

```
engine                  ms (best-of-N)    ratio vs lisp_big
lisp_big                XXX               1.00×
lisp_trampoline_big     XXX               x.xx×
lisp_region_big         XXX               x.xx×
lisp_gc                 XXX               x.xx×
cek_big                 XXX               x.xx×
cek_gc                  XXX               x.xx×
bytecode_big            XXX               x.xx×
bytecode_gc             XXX               x.xx×
```

P1 is decided by the bottom-row ratio. The mechanism story holds if it
lands in the confirmation window; it gets rewritten otherwise — and the
rewrite goes into FINDINGS.md as H8's outcome, following the pattern
H6/H7 set.

## Bench harness plan

- New file: `harness/bench_metacircular.mjs`
- Reuses the bench-driver pattern from `harness/bench.mjs` (load each
  wasm, write source, time `eval_source`, take min-of-min over passes).
- One workload, one row per engine.
- 3 passes × 10 reps per engine (lower than direct bench's 25 because
  each rep is ~100× longer; variance dominates differently).
- Native variant is OPTIONAL — the wasm-on-V8 numbers carry P1; native
  is informative for S2/S3 but not load-bearing.
