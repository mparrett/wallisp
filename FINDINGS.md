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

H4  ENGINE INTERACTION .......................... NOT yet measured.
    Does GC widen the bytecode VM's lead under a tight heap (it allocates less,
    so it should collect less often)? Needs GC ported into the other engines.

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
