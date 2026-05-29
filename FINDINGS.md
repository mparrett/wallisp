# Three evaluators for the same tiny Lisp — what the measurements actually showed

Same language, same reader/printer/arena/primitives. Only the evaluator differs.
Every number below was measured in Node (V8) on wasm built with `clang -O2`,
best-of-25 runs, matched 20M-cell arenas. Treat absolute ms as relative, not
portable — the *ratios* are the point.

## The engines

- **`lisp.c` — tree-walker.** Recursively evaluates the cons-tree. Simplest.
- **`cek.c` — CEK machine.** Explicit Control/Environment/Kontinuation; every
  transition a wasm tail call (`return_call`), so the C stack never grows.
- **`bytecode.c` — bytecode VM.** Compiles the AST once into a flat instruction
  array; a stack machine executes it. Lexical (depth,index) variable addressing,
  operand stack instead of consed argument lists, O(1) global array.

## Speed: fib, best-of-25 (ms)

| N  | tree-walker | CEK    | bytecode | bytecode vs TW |
|----|-------------|--------|----------|----------------|
| 15 | 0.373       | 0.780  | 0.141    | 2.64x faster   |
| 18 | 1.529       | 3.348  | 0.588    | 2.60x faster   |
| 20 | 4.034       | 8.747  | 1.552    | 2.60x faster   |
| 22 | 10.734      | 24.096 | 4.139    | 2.59x faster   |

Bytecode is ~2.6x faster than the tree-walker, **constant across sizes** — a
real constant-factor win, not noise. CEK is ~2.2x *slower* than the tree-walker
(continuations are 5–6 cons cells each; heavy allocation, no GC).

## Two surprises that refuted the original hypotheses

1. **The tree-walker already has proper tail calls.** It runs `countdown(1e6)`
   and mutual `even?(1e6)` in flat stack — because `clang -O2` turns its
   `return eval(...)` self-calls into a loop (tail-recursion elimination). The
   premise that motivated CEK ("the tree-walker will blow the C stack on deep
   tail recursion") was wrong: the compiler underneath was already solving it.
   CEK's headline feature is **redundant** with `-O2` here.

2. **CEK's one exclusive win is deep NON-tail recursion.** `sum(10000)` with
   `(+ n (sum ...))` cannot be looped by any optimizer. The tree-walker
   overflows the C stack (~10k frames); CEK computes it, because depth lives in
   heap continuations. That is the *only* axis CEK clearly wins — and it cost
   2.2x speed plus ~130 lines of machine to get.

## Recursion-depth behavior (all need enough memory; the *mechanism* differs)

| case                     | tree-walker      | CEK            | bytecode              |
|--------------------------|------------------|----------------|-----------------------|
| deep tail recursion      | ✓ (`-O2` TRE)    | ✓ (tail calls) | ✓ if `CALL_MAX` big†  |
| deep non-tail recursion  | ✗ C-stack ~10k   | ✓ (arena)      | ✓ if `CALL_MAX` big   |
| long loops, no GC        | best             | worst‡         | middle                |

† bytecode has **no tail-call optimization**: every `OP_CALL` pushes a return
  frame, so even tail recursion consumes `CALL_MAX` slots. Adding an
  `OP_TAILCALL` (reuse the current frame in tail position) would fix this and is
  the obvious next step if deep tail loops matter.
‡ CEK allocates the most garbage per step, so it exhausts the arena first.

## Takeaways

- **For speed, bytecode is the lever** (2.6x), and it attacks the axis the
  substrate JIT does *not* already cover. It also allocates least on the hot
  path (operand stack, not consed args), so it tolerates the no-GC arena best
  on shapes like fib.
- **CEK is a capability tool, not a speedup** — worth it only if deep *non-tail*
  recursion or first-class continuations (`call/cc`) are the goal.
- **Profiling/empirics beat armchair reasoning repeatedly here**: the env-lookup
  hotspot (wrong), the switch-dispatch win (~5%, eaten by `-Oz`), and CEK's
  tail-call advantage (already free from `-O2`) all looked compelling on paper
  and dissolved under measurement.

## Does the 2.6x generalize? (multi-benchmark — run `node bench.mjs`)

fib alone is a narrow shape, so five canonical codes spanning different stresses,
matched 20M-cell arenas, best-of-25, all three engines cross-checked to agree:

| benchmark        | shape                              | TW (ms) | CEK (ms) | bytecode (ms) | bc vs TW |
|------------------|------------------------------------|---------|----------|---------------|----------|
| tak(18,12,6)     | extreme call volume, no alloc      | 12.08   | 25.70    | 5.32          | 2.27x    |
| fib(24)          | deep recursion + arithmetic        | 28.89   | 61.76    | 10.75         | 2.69x    |
| tailsum(30000)   | tight tail loop w/ accumulator     | 7.01    | 15.27    | 2.34          | 2.99x    |
| ack(3,4)         | deeply nested non-tail recursion   | 2.44    | 5.18     | 0.81          | 3.03x    |
| nrev+sum(150)    | ALLOCATION-bound, O(n^2) conses    | 3.57    | 6.72     | 0.91          | 3.94x    |

**The 2.6x was not a fib artifact** — bytecode wins 2.3x–3.9x across every shape.
Two notable points:

- **Biggest win is the allocation-bound benchmark (3.94x).** Counterintuitive:
  one might expect bytecode's operand-stack edge to shrink when the program
  itself conses heavily. It *grows*, because the tree-walker conses an argument
  list on every call *on top of* the program's own allocation — double the
  traffic. Bytecode's advantage compounds with allocation pressure.
- **Smallest win is tak (2.27x), pure call volume.** Mirror image: tiny per-call
  work, so bytecode's per-call frame-cons is a larger fraction of a smaller cost.

CEK stays ~2.1–2.4x slower than the tree-walker on all five — consistent with
the fib-only run.

### Gotcha surfaced by the suite: fixnums are 30-bit, not 32-bit
Two tag bits live in the low end, so the value range is ±2^29 (~536M).
`tailsum(50000)` silently wrapped (1,250,025,000 -> 176,283,176) until N was
lowered. All three engines wrap identically, so cross-checks still passed — a
reminder that "all engines agree" means consistent, not correct.

## OP_TAILCALL: proper tail calls in the bytecode VM (measured tradeoff)

The bytecode VM originally pushed a return frame on every `OP_CALL`, so tail
recursion consumed `CALL_MAX` slots and capped out. Adding `OP_TAILCALL` — emit
it in tail position, and have it reuse the current frame instead of pushing —
gives proper tail calls. Verified: with a deliberately tiny 1024-frame call
stack, a 1,000,000-deep tail loop and 1,000,000-deep *mutual* recursion both
complete, while non-tail `sum(5000)` still correctly hits the ceiling.

**It costs throughput** (isolated A/B, same process, results identical):

| benchmark | no-TCO | TCO    | TCO cost |
|-----------|--------|--------|----------|
| tak       | 5.388  | 5.390  | ~0%      |
| tailsum   | 2.332  | 2.500  | +7%      |
| fib(24)   | 10.878 | 11.778 | +8%      |
| nrev      | 0.933  | 1.058  | +13%     |

The tell: `tailsum` is tail-recursive, so TCO does *less* work per iteration (no
call-stack push) yet still ran 7% slower. That rules out the per-op logic and
points at the cause — the dispatch loop grew ~1KB, and a bigger hot loop gives
the underlying JIT less to optimize (register pressure, codegen). Same
substrate-JIT sensitivity seen elsewhere: loop *size* matters, not just logic.

**TCO fixes the stack, not the heap.** A long tail loop no longer exhausts the
call stack — but each iteration still *allocates a frame that is never
reclaimed*, so it now dies on ARENA exhaustion instead. The failure cliff scales
with arena size, not call-stack size (`cd(50000)` fails at 262144 cells, reaches
~200k at 2M cells). So "unbounded tail recursion" is still bounded by the arena:
TCO gives constant call *stack*, but only a GC gives constant *heap*. This is the
clearest argument yet that the garbage collector is the real remaining ceiling.

Verdict: kept as default. For a Lisp, proper tail calls are close to table
stakes (a loop that dies at a fixed depth is a bad surprise), and the ~7% buys
correct semantics. Trivially reverted if raw throughput is the only goal.

## Honest caveats

- The speed ratios hold across five distinct shapes (2.3x–3.9x), so they're not
  a single-workload artifact — but all five are small numeric/list kernels. A
  very different workload (string-heavy, large data, IO) isn't represented
  because the language has no such features.
- All three still lack a garbage collector — the shared ceiling. The bytecode
  VM merely hits it later on `fib` by allocating less.
- Numbers are V8-on-wasm. A standalone runtime (wasmtime, wasm3) would differ.

## Native build — separating engine cost from substrate cost

Same engines, compiled native (Apple clang, ARM64, `-O2`), same five benchmarks,
in-process best-of-25 (see `native/bench.c`). Lets us separate "the engine
design" from "the wasm-on-V8 substrate."

| benchmark      | TW native | TW wasm | CEK native | CEK wasm | bc native | bc wasm | bc_gc native | bc_gc wasm |
|----------------|-----------|---------|------------|----------|-----------|---------|--------------|------------|
| fib(24)        | 10.98     | 13.61   | 24.47      | 27.20    | 5.32      | 7.25    | 7.05         | 10.03      |
| tak(18,12,6)   | 4.92      | 6.13    | 10.55      | 12.23    | 2.38      | 3.19    | 2.77         | 4.44       |
| ack(3,4)       | 1.00      | 1.23    | 2.11       | 2.34     | 0.39      | 0.55    | 0.52         | 0.73       |
| nrev+sum(150)  | 1.75      | 2.20    | 3.11       | 3.45     | 0.45      | 0.68    | 0.59         | 0.84       |
| tailsum(30000) | 2.77      | 3.50    | 6.06       | 6.79     | 1.10      | 1.54    | 1.41         | 2.20       |

Three findings, each refines a prior claim:

**1. wasm-on-V8 overhead is small — ~1.1x to 1.6x across all engines and
benchmarks.** That's a strong empirical case FOR wasm as a deployment target:
you give up at most 60% of speed for the portability/sandboxing/zero-imports
property. The wasm bytecode VM at 7.25 ms on fib(24) is closer to its native
self (5.32) than to either wasm-CEK or wasm-tree-walker.

**2. The JIT *flattens* engine differences a bit — CEK has the smallest wasm
overhead (~1.11x), bytecode has the largest (~1.4x).** Why: CEK's tight
musttail loop is exactly the shape V8 specializes hardest (explicit state
transitions become native jumps). Bytecode spends more time in interpreter
dispatch machinery V8 has to JIT more conservatively. Consequence: **bytecode's
lead over the tree-walker is *larger* native (2.0x–3.9x) than wasm (1.9x–3.3x).**
V8 helps the simpler engine more.

**3. GC overhead native (~1.32x on fib) is smaller than GC overhead wasm
(~1.38x), but both are real.** This refines H2: the "optimization barrier" is
genuinely a compiler-level phenomenon (clang itself can't treat `cons` as
side-effect-free when it can reach `gc()`), but V8's JIT then amplifies it.
The H2 wasm number (1.66x in the original measurement) isn't a pure measurement
of the barrier — it's the barrier *amplified* by the JIT. Native lets us see
the floor: ~1.3x is what you pay for GC-safe allocation points regardless of
substrate. The story H2 told is confirmed; the number is now decomposed.

Engine ordering is identical native vs wasm (`bytecode > tree-walker > CEK`,
~2x gap each way). No surprises there.

### Caveat on these measurements

Different host machine from the original FINDINGS run (Apple Silicon, current
LLVM, V8 in Node 25 / JSC in Bun 1.3) — absolute ms shouldn't be compared
across reruns. The native/wasm *ratios* within this run are the load-bearing
result, since both were measured back-to-back on the same hardware.

================================================================================
MARK-SWEEP GC  (bytecode_gc.c)  — removing the shared ceiling
================================================================================

We chose mark-sweep over semispace because every heap object is a uniform cons
cell (so mark-sweep's fragmentation downside vanishes) and it is non-moving:
indices never change, so the scattered roots (operand stack, env, saved frames,
globals, AND quoted-data constants embedded in the bytecode) only need to be
MARKED, never rewritten. A moving collector would have to find and rewrite every
one of those indices precisely — exactly the bug surface this project keeps
getting burned by.

Five hypotheses were pre-registered BEFORE measuring, then tested:

H1  UNBOUNDED (headline) ........................ CONFIRMED, decisively.
    countdown(10,000,000) completes in a 512-cell arena, with GC running
    118,577 times to recycle the same handful of cells. TCO keeps the call
    stack flat; GC keeps the heap flat; together = genuinely unbounded tail
    recursion. The session-long ceiling is gone.

H5  CORRECTNESS / ROOT SET (make-or-break) ...... CONFIRMED.
    19/19 suite passes. fib(18)=2584 correct while GC fired 120 times mid-
    evaluation in a 512-cell arena. The quoted-data-survives-GC case passes —
    validating the trickiest root (cons operands embedded in the code stream).
    A wrong root set could not survive 120 collections during one computation.

H2  OVERHEAD .................................... measured; my predictions REFUTED.
    The GC build runs ~1.3-1.7x slower than the no-GC big-arena build. The
    surprise: this is NOT collection cost. Isolation experiments:
      - GC machinery merely present (mark arrays, hoisted registers,
        gc() defined but never called) ............... +6%
      - collection work itself (gc=0 vs gc=4 runs) ..... negligible
      - allocation path being GC-capable .............. the rest (the ~1.6x)
    Final decomposition (fib(24), same process):
      no-GC build .................................... 1.00x
      split cons, cons_slow does NOT call gc() ....... 1.11x
      split cons, cons_slow CALLS gc() (shippable) ... 1.66x
    CONCLUSION: the dominant cost is an OPTIMIZATION BARRIER. Once cons() can
    reach gc(), the compiler can no longer treat allocation as side-effect-free,
    so it pessimizes the VM hot loop around every cons. This is somewhat
    fundamental to having GC-safe allocation points — it is the price of "a
    collection may happen here," not the price of the collection itself.
    (Three intermediate explanations were proposed and falsified along the way:
    register-hoisting, then non-inlining, then the free-list pop. Measure,
    don't guess.)

H3  MARK vs SWEEP COMPOSITION ................... NOT yet measured.
    Structurally: mark is O(live), sweep is O(arena). A small live set in a
    large arena should be sweep-dominated — the argument for lazy sweep or a
    generational nursery later.

H4  ENGINE INTERACTION .......................... PARTIALLY CONFIRMED; mechanism refuted.
    See the "H4 — GC ported into CEK" section below for the measurement,
    falsification log, and the H2-from-a-new-angle reframing.
    Pre-registration retained below for the record.
    Does GC widen the bytecode VM's lead under a tight heap (it allocates less,
    so it should collect less often)? With `cek_gc.c` ported, the testable
    predictions (recorded BEFORE running the bench) are:
      (a) `bytecode_gc` GC-count < `cek_gc` GC-count on every benchmark.
          Mechanism: CEK allocates 5–6 conses per continuation step and
          continuations are the unit of progress; bytecode's operand stack +
          flat call frames amortize across many steps before the next cons.
      (b) `bytecode_gc / cek_gc` time ratio > `bytecode_big / cek_big` ratio.
          That is: GC AMPLIFIES bytecode's existing lead. (This is the core
          H4 claim.)
      (c) The amplification is largest on allocation-heavy programs (nrev)
          and smallest on tight tail loops with no body-level allocation
          (tailsum) — where both engines barely collect, so the gap should
          stay near the no-GC ratio.
      (d) Both `cek_gc` and `bytecode_gc` should be slower than their `_big`
          counterparts on the same benchmark, by a margin in the
          ~1.3–1.7x band the H2 decomposition established. If `cek_gc`'s
          slowdown lands meaningfully outside that band, H2's mechanism
          story (optimization barrier + JIT amplification of loop size)
          generalizes less than we thought, and we should investigate.
    Falsification: any of (a)–(c) failing falsifies H4 as stated.

Allocator design notes
- Hybrid: bump-allocate until the arena fills, THEN fall to free-list + GC.
  O(1) init; programs that never fill the arena pay zero collection tax.
- cons() is split: an inlinable bump fast path + an out-of-line cons_slow that
  handles free-list reuse and collection.
- VM registers (vsp/csp/env) are hoisted to file scope so gc() sees them as
  roots; this costs ~5% and is not the bottleneck.
- Trace uses an explicit mark stack (not C recursion) to bound stack depth.
- Freestanding memset/memcpy are provided (-fno-builtin) since clang emits calls
  to them for the mark-bit clear.
- Module still has ZERO imports — runs in any wasm host via the embedding API.

## OP_CALL primitive inlining in `bytecode_gc.c`

bc_inline's runtime-checked fast path, ported into the finalist engine.
In `OP_CALL` and `OP_TAILCALL`, when the live `fn` value is a primitive
in `[PR_ADD, PR_LT]` with `n==2`, do the arithmetic directly on the
operand stack — bypassing the arg-list cons-and-apply_prim path.

The runtime check *is* the redefinition guard: `fn` comes from `OP_LOADG`
which reads the live `gval[]`, so after `(define +)` the inline path
automatically falls through to the general path with the user's new
binding. bc_super's compile-time emission can't do this — it bakes
`gval[+]` into the bytecode and diverges silently. Trade-off: ~0.4x of
bc_super's theoretical speed for unconditional correctness.

GC-safety: ~20 LOC added; **no new roots**. The fast path only touches
`vstack`/`R_vsp` (already roots) and reaches no `cons()` call site,
so the H2 "optimization barrier" stays bounded to the existing
`cons → cons_slow → gc` chain.

### Measured A/B (same process, same machine, best-of-25)

**Wasm (Node/V8):**

| benchmark      | baseline ms | patched ms | speedup | gc baseline | gc patched |
|----------------|-------------|------------|---------|-------------|------------|
| fib(24)        | 10.135      | 7.179      | 1.41×   | 4           | 1          |
| tak(18,12,6)   | 4.443       | 3.504      | 1.27×   | 1           | 0          |
| ack(3,4)       | 0.726       | 0.550      | 1.32×   | 0           | 0          |
| nrev+sum(150)  | 0.850       | 0.872      | **0.97×** | 0         | 0          |
| tailsum(30000) | 2.181       | 1.571      | 1.39×   | 1           | 0          |

**Native (Apple clang, ARM64):**

| benchmark      | baseline ms | patched ms | speedup |
|----------------|-------------|------------|---------|
| fib(24)        | 7.240       | 5.261      | 1.38×   |
| tak(18,12,6)   | 2.885       | 2.528      | 1.14×   |
| ack(3,4)       | 0.532       | 0.424      | 1.26×   |
| nrev+sum(150)  | 0.609       | 0.599      | 1.02×   |
| tailsum(30000) | 1.490       | 1.165      | 1.28×   |

Correctness: full suite (23 tests × both engines = 46/46) passes including
the load-bearing rebind test
  `(begin (define + (lambda (a b) 99)) (+ 1 2)) => 99`.
The compile-time bc_super pattern would answer `3` here.

### Three findings worth flagging

**1. 4.8's GC prediction confirmed: fib(24) goes 4→1 GC cycles.**
   Fewer cons calls (no per-call arg list) → fewer collections. The
   `tak` and `tailsum` cases also dropped from 1→0. The speedup on
   these benchmarks is therefore *partly* the inline arithmetic and
   *partly* the GC pressure relief.

**2. `nrev+sum(150)` regresses 3% on wasm and is flat on native — and
   that's H2 from a fresh angle.** nrev's hot loop is `cons`/`car`/`cdr`/
   `null?`, none of which the inline fast path touches. So we pay the
   dispatch-loop-growth cost (~20 LOC more) with zero benefit on this
   shape. Native handles the larger loop fine; V8 regresses. Same
   substrate-JIT sensitivity that bit us in the TCO measurement and the
   native-vs-wasm GC overhead decomposition: **JIT amplifies the cost of
   a bigger hot loop, regardless of whether the new code is reached.**

**3. The win on the *inline-helping* benchmarks is ~1.27-1.41× wasm,
   1.14-1.38× native.** Higher than bc_inline's original 1.2× headline
   from the no-GC prototype — because in the GC build, dodging the
   cons-an-arg-list dance pays back twice (less work *and* less GC).
   Wasm gains slightly more than native because V8 specializes the
   shorter call path harder.

Edge: pre-existing undefined-on-type-error behavior preserved. `(+ 'x 1)`
produced garbage before and produces (different) garbage now — same class
of issue the fixval/shift trick has always carried.

## H4 — GC ported into CEK (`cek_gc.c`)

Mark-sweep collector ported from `bytecode_gc.c` into the CEK machine. Same
non-moving free-list sweep, same hoisted register protocol (`R_C/R_E/R_K/R_V`
hold the CEK state; gc() marks them as roots). One CEK-specific addition:
a small `R_save` shadow stack covers two unrooted-local windows where the
in-flight cell is neither in `gc_a/gc_b` of the next cons nor reachable from
`R_C/E/K/V` — `K_ARGS` building the new `done` cell before wrapping it into a
new K_ARGS frame, and `s_let`'s parallel vars/vals lists during the lambda
desugaring. Everywhere else, the existing "next cons's gc_a/gc_b protects the
live cell" discipline holds (see header comment in `engines/cek_gc.c`).

### Measured (262K-cell default arena, best-of-25)

**Native (Apple clang, ARM64):**

| benchmark      | cek    | cek_gc | bc     | bc_gc  | bc/cek  | bc_gc/cek_gc | widening | cek_gc tax | bc_gc tax |
|----------------|--------|--------|--------|--------|---------|--------------|----------|------------|-----------|
| fib(24)        | 25.235 | 32.911 |  5.454 |  5.374 |  4.63×  |  6.12×       |  1.32×   |  1.30×     |  0.99×    |
| tak(18,12,6)   | 10.902 | 13.926 |  2.523 |  2.594 |  4.32×  |  5.37×       |  1.24×   |  1.28×     |  1.03×    |
| ack(3,4)       |  2.155 |  2.807 |  0.397 |  0.442 |  5.43×  |  6.35×       |  1.17×   |  1.30×     |  1.11×    |
| nrev+sum(150)  |  3.185 |  3.646 |  0.461 |  0.623 |  6.91×  |  5.85×       |**0.85×** |  1.14×     |  1.35×    |
| tailsum(30000) |  6.227 |  7.875 |  1.135 |  1.213 |  5.49×  |  6.49×       |  1.18×   |  1.27×     |  1.07×    |

**Wasm (Node/V8):**

| benchmark      | CEK    | CEK_gc | bc    | bc_gc | bc/cek | bc_gc/cek_gc | widening | cek_gc tax | bc_gc tax | cek_gc GC | bc_gc GC |
|----------------|--------|--------|-------|-------|--------|--------------|----------|------------|-----------|-----------|----------|
| fib(24)        | 27.789 | 52.305 | 8.308 | 7.494 |  3.34× |  6.98×       |  2.09×   |  1.88×     |  0.90×    |  43       |  1       |
| tak(18,12,6)   | 12.560 | 23.498 | 3.278 | 3.653 |  3.83× |  6.43×       |  1.68×   |  1.87×     |  1.11×    |  19       |  0       |
| ack(3,4)       |  2.392 |  4.478 | 0.559 | 0.579 |  4.28× |  7.73×       |  1.81×   |  1.87×     |  1.04×    |   3       |  0       |
| nrev+sum(150)  |  3.605 |  6.007 | 0.703 | 0.927 |  5.13× |  6.48×       |  1.26×   |  1.67×     |  1.32×    |   4       |  0       |
| tailsum(30000) |  6.960 | 13.119 | 1.582 | 1.612 |  4.40× |  8.14×       |  1.85×   |  1.89×     |  1.02×    |  11       |  0       |

`widening = (bc_gc/cek_gc) / (bc/cek)`. `tax = gc_build / no-gc-build`.

### Result against the predictions

H4 is partially confirmed. The narrow claim holds; the *mechanism* I predicted
is refuted; what's actually driving the effect is H2 from a new angle.

(a) **`bytecode_gc` collects far less than `cek_gc`.** CONFIRMED, dramatically.
    Fib(24): 1 vs 43 cycles. tak(18,12,6): 0 vs 19. Across the suite, CEK_gc
    collects between 3× and 43× as often. CEK allocates a 5-cell continuation
    per evaluation step and never reuses; bytecode operates on a flat
    operand-stack frame and only conses for closure environments and the
    rare allocating primitive (CONS/CAR/CDR).

(b) **The lead widens.** CONFIRMED on every wasm benchmark (1.26–2.09×) and
    on four of five native benchmarks (1.17–1.32×). **Refuted on native
    nrev+sum(150)** — the GC build's gap *narrowed* from 6.91× to 5.85×.

(c) **Widening grows with allocation pressure.** REFUTED — backwards.
    nrev+sum, the only allocation-bound benchmark in the suite, shows the
    **smallest** widening on both substrates (1.26× wasm, 0.85× native).
    The largest widening is on compute-bound programs (fib, tailsum, ack).

(d) **GC tax sits in the H2 1.3–1.7× band.** Confirmed on native (the
    `cek_gc / cek` column lands 1.14–1.30×, right at the bytecode_gc native
    floor). Wasm tells a different story: CEK's GC tax is **flat at 1.87–1.89×**
    across four of five benchmarks, well above the 1.4× bytecode_gc paid in
    the original H2 measurement. The mechanism isn't new — V8 amplifies the
    optimization barrier — but the magnitude depends on the engine's hot-loop
    shape, and CEK's tight musttail chain is closer to V8's sweet spot than
    bytecode's dispatch loop is.

### What the data actually says (mechanism)

The H4 effect is real but it works through H2 (optimization barrier per
cons-callsite), not through the collection-frequency story I pre-registered.

Evidence:

1. **The GC tax is uncorrelated with collection count.** CEK_gc on wasm:
   fib (43 collections) → 1.88× tax; nrev (4 collections) → 1.67× tax;
   tailsum (11 collections) → 1.89× tax. A 10× swing in collection count
   moves the tax by ~12%. The tax is paid *per cons-callsite that could
   reach gc()*, regardless of whether gc() actually fired.

2. **Allocation-heavy programs show the smallest widening because both
   engines pay the per-cons tax in proportion.** On nrev, both engines'
   hot loops are *already* cons-dominated (`cons`/`car`/`cdr` are the user
   program). The H2 barrier has less non-cons surrounding code to pessimize.
   CEK_gc nrev tax: 1.14× native, 1.67× wasm — the lowest in either column.
   bytecode_gc nrev tax: 1.35× native, 1.32× wasm — the *highest* in those
   columns (bytecode's leaner pre-GC dispatch loop has more to lose).

3. **The native nrev anomaly resolves the same way.** bytecode_gc pays more
   tax on nrev (1.35×) than CEK_gc does (1.14×) because bytecode's
   inline-prims fast path only covers `+`/`-`/`*`/`=`/`<` — not `cons`.
   So nrev runs the *general* cons-and-apply path on bytecode_gc, paying
   the H2 barrier at every step. CEK had no inline-prims fast path to begin
   with, so its pre-GC nrev was already general-path. Adding GC adds less
   incremental tax for CEK on this specific shape, enough to flip the
   widening sign on native.

4. **CEK_gc's flat 1.87× wasm tax is the fourth independent observation of
   "V8 amplifies the larger/hotter hot loop's optimization barrier."**
   First was the original H2 (bytecode 1.4× wasm vs 1.3× native). Second
   was TCO (7%). Third was the inline-prims `nrev` regression. Now CEK_gc:
   V8 loses 1.87× to make the musttail chain GC-safe; native loses 1.27×.
   The substrate-vs-engine decomposition keeps reproducing.

### Verdict on H4

The phrased claim ("does GC widen bytecode's lead under a tight heap?") is
yes-with-an-asterisk: it widens on all wasm benchmarks and most native ones,
but the widening is driven by H2 acting on each engine's hot-loop shape, not
by bytecode collecting less often. If you reverse the H4 question — "does
the engine's GC tax scale with its allocation rate?" — the answer is no.
Tax scales with the number of cons-callsites in the hot loop and how much
non-cons code surrounds them, not with how many bytes the program allocates.

This makes the GC port work productive in a way I didn't predict: it didn't
expose a new mechanism, it provided a fourth, larger-magnitude data point
for the H2 mechanism, and it falsified my intuition about which benchmark
shapes would amplify the GC tax most.

### Falsification log

- Pre-registered prediction (c): widening greatest on nrev. Result: smallest
  on nrev, both substrates. Refuted.
- Pre-registered prediction (b): widening positive everywhere. Result:
  negative on native nrev. Refuted in narrow form; reframed: widening
  positive everywhere V8 is involved; native exposes the proportional-tax
  case where it can flip.
- Pre-registered prediction (d): CEK_gc tax in 1.3–1.7× band. Result:
  native yes, wasm no (1.87× — above band). Reframed: H2 magnitude is
  engine-shape-dependent.
