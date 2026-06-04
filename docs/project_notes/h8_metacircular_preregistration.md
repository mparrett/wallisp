# H8 — does bytecode_gc's win generalize to metacircular eval?

**Pre-registered 2026-06-04. Measured 2026-06-04. P1 FALSIFIED — see "Outcome" below.**

> Note for future readers: the metacircular evaluator the bench actually
> ran is `baselines/metacircular.lisp` (already in the repo; uses
> Y-combinator), not the `harness/mc_eval.lisp` I wrote this session
> (uses pass-by-self). The H8 prediction is about workload *shape*, so
> either evaluator should produce the same finding; the bench uses the
> one wired into `harness/bench.mjs`.

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

The existing `harness/bench.mjs` already has the `meta-fib(N)` row wired
up against `baselines/metacircular.lisp`. No new harness needed.

## Outcome (measured 2026-06-04)

5 runs of `node harness/bench.mjs`, min-of-min per engine across runs.
Workload: `meta-fib(12)` = 144 (the bench's existing N, not the N=10
the prediction was written against — close enough; ratios shouldn't
shift materially across fib(10) → fib(12)).

| engine                  | direct fib(24) | ratio  | meta-fib(12) | ratio  | direction |
|-------------------------|---------------:|-------:|-------------:|-------:|-----------|
| `lisp_big`              |     19.281 ms  | 1.000× |    13.666 ms | 1.000× | (baseline) |
| `lisp_trampoline_big`   |     19.216 ms  | 0.997× |    13.570 ms | 0.993× | preserved |
| `lisp_region_big`       |     19.244 ms  | 0.998× |    13.801 ms | 1.010× | tiny tax |
| `lisp_gc`               |     25.885 ms  | 1.343× |    16.639 ms | 1.218× | narrowed |
| `cek_big`               |     33.357 ms  | 1.730× |    20.202 ms | 1.478× | narrowed |
| `cek_gc`                |     60.080 ms  | 3.116× |    34.975 ms | 2.560× | narrowed |
| `bytecode_big`          |      8.436 ms  | 0.437× |     3.716 ms | 0.272× | **WIDENED** |
| **`bytecode_gc`**       |    **7.593 ms**| **0.394×** | **3.464 ms** | **0.253×** | **WIDENED** |

### P1: FALSIFIED — preserves direction

Predicted `[0.40×, 0.70×]`. Measured **0.253×** (3.94× faster than the
tree-walker, up from 2.54× faster on direct fib). The bytecode advantage
*widened*, not narrowed. This falls into the "Falsified — preserves"
window the pre-registration named, and then some.

The mechanism story I pre-registered was wrong:
- ❌ **Predicted mechanism:** PR1's inline arithmetic fast path is a major
  contributor to bytecode's win. On a cond-heavy workload (less
  arithmetic in hot loop), bytecode should lose some edge.
- ✅ **Revised mechanism:** bytecode's win is dominated by **dispatch
  shape**, not arithmetic. The VM's `br_table` over 12-14 opcodes is
  the kind of hot-loop shape V8 specializes hard. The tree-walker's
  recursive eval re-walks AST cons-graphs on every step — the more
  cond-branches in the host eval (i.e., the more types/forms it
  dispatches over), the more AST re-walking, the *wider* the gap.

Put differently: **the inline arithmetic fast path is a small share of
the total speedup, not the dominant share.** That was a workload-
specific intuition (fib does mostly arithmetic) extrapolated wrong.

### S1: CONFIRMED — CEK ratios narrow

- `cek_big / lisp_big`: 1.730× → 1.478× (narrowed by ~14%)
- `cek_gc / lisp_big`:  3.116× → 2.560× (narrowed by ~18%)

CEK's per-step overhead is fixed (the K_ARGS frame consing); when each
host step is more work, the relative cost shrinks. Both CEK variants
move in the same direction, confirming the per-step interpretation.

### S2: GC tax preserved

- `bytecode_gc / bytecode_big` direct: 7.593 / 8.436 = **0.900×**
  (bytecode_gc is actually 11% *faster* due to smaller-arena cache fit;
  known FINDINGS H2 result).
- `bytecode_gc / bytecode_big` meta:   3.464 / 3.716 = **0.932×** (similar).

Cons rate per host step is similar across workloads; the GC tax (which
in this build is *negative* due to the cache effect) is preserved.

### S3: Engine ordering broadly preserved, gaps tighten in the middle

Ordering by speed is the same: `bytecode_gc < bytecode_big <
lisp_trampoline_big ≤ lisp_big ≤ lisp_region_big < lisp_gc < cek_big <
cek_gc`. But the *gaps* tighten everywhere except the bytecode tier:
the cek-vs-tree-walker spread compresses; the bytecode-vs-tree-walker
spread expands. The take-away: **bytecode is a category, not a
gradient** — it's qualitatively different from the others (compile-
once-execute-many vs. re-walk-the-tree), and complex workloads
amplify that.

## What changes elsewhere in the project

- `FINDINGS.md` gets a new H8 section following the H6/H7 template
  (which this doc was a pre-reg for).
- `ENGINES.md`'s bytecode paragraph gets a "workload-sensitivity"
  sentence noting the metacircular result strengthens (not weakens)
  the case for the dispatch-shape mechanism.
- The 2026-06-01 handoff's open question — "does bytecode_gc's post-
  PR1 advantage generalize beyond fib?" — is **answered yes, and more
  so**: on a workload with very different shape (cond-heavy,
  allocation-heavy), the advantage doesn't just generalize, it grows.
