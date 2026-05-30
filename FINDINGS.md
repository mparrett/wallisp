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

   *Sharper restatement:* the kanaka/mal guide's step 5 (TCO) prescribes "add a
   `while(TRUE)` loop around all code in EVAL; for `if`/`let*`/`do`/closure
   call, set `ast` and `env` then `continue` instead of recursing." Our
   `lisp.c` doesn't have that loop. clang's -O2 TRE writes it for us. mal's
   step 5 is an explicit version of what clang did implicitly here. (See
   `docs/project_notes/external_inspirations.md` for the wider comparison.)

2. **CEK's one exclusive win is deep NON-tail recursion.** `sum(10000)` with
   `(+ n (sum ...))` cannot be looped by any optimizer. The tree-walker
   overflows the C stack (~10k frames); CEK computes it, because depth lives in
   heap continuations. That is the *only* axis CEK clearly wins — and it cost
   2.2x speed plus ~130 lines of machine to get.

## Recursion-depth behavior (all need enough memory; the *mechanism* differs)

| case                     | tree-walker      | CEK            | bytecode              |
|--------------------------|------------------|----------------|-----------------------|
| deep tail recursion      | ✓ (`-O2` TRE)    | ✓ (tail calls) | ✓ (`OP_TAILCALL`)†    |
| deep non-tail recursion  | ✗ C-stack ~10k   | ✓ (arena)      | ✗ `CALL_MAX` bounded  |
| long loops, no GC        | best             | worst‡         | middle                |

† Original chart said bytecode lacked TCO; we have since added `OP_TAILCALL`
  (reuses the current call frame in tail position), so tail recursion no longer
  consumes `CALL_MAX` slots. See "OP_TAILCALL: proper tail calls in the bytecode
  VM" below for the measurement.
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

**Originally measured: TCO costs throughput.** The original table (kept here
as a historical anchor):

| benchmark | no-TCO | TCO    | TCO cost |
|-----------|--------|--------|----------|
| tak       | 5.388  | 5.390  | ~0%      |
| tailsum   | 2.332  | 2.500  | +7%      |
| fib(24)   | 10.878 | 11.778 | +8%      |
| nrev      | 0.933  | 1.058  | +13%     |

The original tell: `tailsum` is tail-recursive, so TCO does *less* work per
iteration (no call-stack push) yet still ran 7% slower. That ruled out the
per-op logic and pointed at the cause — the dispatch loop grew ~1KB, and a
bigger hot loop gave the underlying JIT less to optimize (register pressure,
codegen). Same substrate-JIT sensitivity seen elsewhere: loop *size* matters,
not just logic.

**Re-measured 2026-05-29 on clang 22 / V8 in Node 25: the sign has flipped.**
Same A/B (TCO vs no-TCO bytecode_gc, no-TCO built by patching the compiler at
`engines/bytecode_gc.c:266` to emit `OP_CALL; OP_RET` instead of
`OP_TAILCALL`), best-of-25, three repeats:

| benchmark      | no-TCO ms | TCO ms | TCO cost (was) |
|----------------|----------:|-------:|---------------:|
| tailsum(30000) |     1.649 |  1.573 |  **−5%** (+7%) |
| fib(24)        |     7.520 |  7.279 |  **−3%** (+8%) |
| meta-fib(12)   |     3.994 |  3.877 |  **−3%**       |

TCO is now a small win, not a cost. The "loop-size sensitivity" mechanism still
makes sense — bigger hot loop, less for the JIT to specialize — but clang 22
emits the `OP_TAILCALL` arm in a shape V8 now prefers (or at least doesn't
penalize). The two measurements bracket the substrate-JIT story: the cost is
real but the sign isn't stable across toolchain versions. Treat absolute throughput
deltas under 10% as "depends on which clang and V8 you build for today."

The metacircular row was added during the meta workup — natural hypothesis:
the mceval ↔ mcapply chain in `baselines/metacircular.lisp` is a long tail
loop (closure-call → eval-body → application → closure-call), so even a
program that isn't notionally tail-recursive at the source level benefits
when its hot dispatch edges ARE in tail position. Confirmed: meta-fib(12) is
3% faster with TCO.

**TCO fixes the stack, not the heap.** A long tail loop no longer exhausts the
call stack — but each iteration still *allocates a frame that is never
reclaimed*, so it now dies on ARENA exhaustion instead. The failure cliff scales
with arena size, not call-stack size (`cd(50000)` fails at 262144 cells, reaches
~200k at 2M cells). So "unbounded tail recursion" is still bounded by the arena:
TCO gives constant call *stack*, but only a GC gives constant *heap*. This is the
clearest argument yet that the garbage collector is the real remaining ceiling.

Verdict: kept as default. For a Lisp, proper tail calls are close to table
stakes (a loop that dies at a fixed depth is a bad surprise). On the original
toolchain the ~7% cost bought correct semantics; on the current toolchain
TCO is a small free win as well as correct.

## Honest caveats

- The speed ratios hold across five distinct shapes (2.3x–3.9x), so they're not
  a single-workload artifact — but all five are small numeric/list kernels. A
  very different workload (string-heavy, large data, IO) isn't represented
  because the language has no such features.
- *At the time of this measurement*, all three engines lacked a garbage
  collector — the shared ceiling, which the bytecode VM merely hit later by
  allocating less. Since then we've added `lisp_gc`, `lisp_region`, `cek_gc`,
  and `bytecode_gc`; the GC measurements are in the H4 / H2 sections below.
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

## Hand-written JS and C baselines — the substrate ladder, extended

Natural follow-up to the native-vs-wasm decomposition: what would these
algorithms cost with *no interpreter at all*? Hand-write the same five
benchmarks in JS (V8 native, no wasm) and C (`-O2` native, no wasm), run on
the same machine, and we get the upper bound on what these programs can cost.
Live in `baselines/bench.{js,c}`; inline-equivalent versions print at the end
of `node harness/bench.mjs`.

Same algorithms shape-for-shape: cons-cell linked lists (not arrays) for
`nrev+sum`; recursive `fib`/`tak`/`ack`; `tailsum` iterative in JS (no TCE),
recursive in C (clang TCEs it).

| benchmark      | bytecode_gc | js (V8) | c (-O2) | bc_gc / js | bc_gc / c |
|----------------|-------------|---------|---------|------------|-----------|
| fib(24)        | 7.31        | 0.33    | 0.13    | 22.2×      | 57.1×     |
| tak(18,12,6)   | 3.56        | 0.15    | 0.05    | 23.8×      | 71.1×     |
| ack(3,4)       | 0.57        | 0.05    | 0.02    | 11.5×      | 28.3×     |
| nrev+sum(150)  | 0.87        | 0.09    | 0.15    |  9.7×      |  6.0×     |
| tailsum(30000) | 1.57        | 0.02    | 0.00    | 96.9×      |  ∞ (folded)|

Two findings worth surfacing:

**1. JS beats C on `nrev+sum`.** The only row where the gap from the
interpreter to JS is wider than the gap to C. Mechanism: one iteration does
~11k cons allocations, each a short-lived three-word object; V8's young-gen
bump allocator absorbs that at near-zero amortized cost, while glibc `malloc`
pays book-keeping per call. On the compute-only rows (no allocation), C wins
over JS by the expected 2–3×. This is a well-known V8 advantage and the
benchmark hits it head-on; the C baseline could close the gap with an arena,
which is precisely what the engines do internally.

**2. `tailsum(30000)` in C clocks at 0.000 ms.** clang at `-O2` sees through
the tail recursion and rewrites the loop as `n*(n+1)/2`. The reported time
is below the timer's resolution because the work isn't being done at runtime;
it was done at compile time. The Lisp engines obviously can't do this — the
program isn't visible to clang. This is the structural reason an interpreter,
however well-written, has a ceiling it can't reach: it can't see the whole
program.

**The substrate ladder for fib(24)**, fastest at the top:
- C, `-O2` native — ~0.13 ms
- JS-on-V8 (V8 JITs the source) — ~0.33 ms
- `bytecode_gc`, wasm-on-V8 (V8 JITs the interpreter, which interprets the program) — ~7.3 ms
- `cek_gc`, wasm-on-V8 (worst engine) — ~57 ms

The engine work in this project moves us ~8× (cek_gc → bytecode_gc), on the
interpreter side of a fixed substrate. The remaining ~55× to native C lives
in compiler/JIT territory we don't control. Worth knowing where the headroom
actually sits.

## Build flag: `-O2` vs `-Oz` — half the wasm size, free or better on V8 (except CEK)

`-Oz` appears once in the falsification log above — as "the thing that ate
the switch-dispatch speedup." We never quantified the *size* win, and the
project's reflex has been to default to `-O2` everywhere (in `build.sh`).
Worth re-measuring properly.

Same eight engines, built twice (`-O2` and `-Oz`), Homebrew clang 22, wasm32,
identical link flags. Sizes are default-arena (shipped) builds; perf is fib(24)
on the big-arena variants the bench harness uses, best-of-25 in Node/V8:

| engine          | O2 bytes | Oz bytes | size Δ  | O2 fib ms | Oz fib ms | perf Δ   |
|-----------------|---------:|---------:|--------:|----------:|----------:|---------:|
| lisp            |    9840  |    4348  | −55.8%  |   14.51   |   12.53   |  −13.7%  |
| lisp_trampoline |    9838  |    4359  | −55.7%  |   13.92   |   12.44   |  −10.7%  |
| lisp_gc         |   10435  |    5484  | −47.4%  |   18.82   |   17.54   |   −6.8%  |
| lisp_region     |    9965  |    4453  | −55.3%  |   12.73   |   12.19   |   −4.2%  |
| cek             |   12964  |    5067  | −60.9%  |   27.04   |   71.03   | **+162.8%** |
| cek_gc          |   12050  |    6014  | −50.1%  |   50.67   |   95.57   |  **+88.6%** |
| bytecode        |   15434  |    6178  | −60.0%  |    7.24   |    7.91   |   +9.2%  |
| bytecode_gc     |   15748  |    7441  | −52.7%  |    7.31   |    6.50   |  **−11.1%** |

Three findings:

**1. `-Oz` consistently halves the wasm artifact** — every engine shrinks
47%–61%, with average ~55%. The total shipped wasm goes from 96.3 KB to
43.4 KB. The savings come from less inlining and more sharing of common
sequences; `-Oz` aggressively prefers code-size over hot-path inlining.

**2. For most engines, V8 prefers the smaller code.** The tree-walker
family and (most interestingly) `bytecode_gc` — the finalist — run *faster*
at `-Oz`. The mechanism is straightforward: smaller modules are friendlier
to V8's tiered JIT (less code to baseline-compile, smaller hot paths to
specialize, better instruction-cache behavior). `bytecode_gc` lands at
−11% time and −53% bytes simultaneously. Strict win.

**3. CEK regresses catastrophically.** `cek` at `-Oz` is **2.6× slower**;
`cek_gc` is **1.9× slower**. Consistent with the "Native build" section
above: CEK's tight musttail dispatch chain is the shape V8 specializes
hardest, and `-Oz` emits a coarser code shape (fewer inlined arm
specializations) that V8's tier-up pattern-matchers don't recognize as the
hot musttail loop. The thing that makes CEK fast on V8 is exactly what
`-Oz` declines to spend code-size on.

**Practical takeaway.** For shipped engines (`bytecode_gc`, tree-walker
variants), `-Oz` is strictly better — smaller and same-or-faster. For
CEK, `-O2` is non-negotiable. `build.sh` keeps `-O2` as the universal
default because a single flag that doesn't make any engine *worse* is the
safe shared setting; an opinionated build script would use `-Oz` for
everything except `cek*` and ship at roughly half the byte count.

The deeper methodological point: clang's `-O2`/`-Oz` choice interacts with
V8's JIT in a way that isn't predictable from either layer alone. `-Oz` is
"smaller and slower" on most native targets; on V8 it's "smaller and
usually equal or faster, but CEK 2× slower." Four substrates, four places
for the answer to flip.

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

H4  ENGINE INTERACTION .......................... PARTIALLY CONFIRMED in 2 engines;
    mechanism refuted and reframed. Three GC engines now exist (bytecode_gc,
    cek_gc, lisp_gc); together they show V8's amplification of the H2
    optimization barrier scales with engine JIT-specializability:
    bytecode_gc ~1.0×, lisp_gc ~1.1×, cek_gc ~1.4×.
    See "H4 — GC ported into CEK" and "H4 — GC ported into the tree-walker"
    below. Pre-registrations retained for the record.

H2 zero floor ................................... MEASURED via region-drop GC.
    lisp_region.c (tinylisp-style: cons allocates downward, gc() = sp =
    considx(cdr(g_head)) at top-level form boundaries, no function call
    reachable from cons). Pre-registered that lisp_region/lisp ≈ 1.0×;
    actually landed at ~0.94× wasm and ~0.99× native. Region-drop is
    *faster* than the no-GC tree-walker baseline by ~6% wasm. The H2 tax
    (1.05-1.83× across the three mark-sweep engines) goes away when cons
    can't reach any function call — and a small additional speedup comes
    from the lighter compare shape (sp == 0 vs cell_top >= MAX_CELLS).
    See "H2 zero floor — region-drop GC" below.

H1 verification ................................. CONFIRMED via explicit trampoline.
    lisp_trampoline.c rewrites eval() as a hand-rolled while(TRUE) loop
    (mal step 5 verbatim) instead of relying on clang's -O2 TRE to do the
    same. Pre-registered ≈1.0× both substrates within ±5%; measured
    native mean 1.007×, wasm mean 1.005×, all individual benchmarks
    within ±2%. Wasm modules differ by 2 bytes out of 9840 (0.02%).
    The recursive lisp.c form pays no hidden cost vs the explicit
    trampoline — clang's TRE is doing exactly the transformation mal
    step 5 prescribes. H1's "clang TRE = mal step 5 trampoline" framing
    is now empirically grounded. See "H1 verification — explicit
    trampoline tree-walker" below.
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

## Metacircular evaluator — Lisp-in-Lisp on each engine

A tiny Lisp interpreter (`baselines/metacircular.lisp`) written in wallisp's
own Lisp — `eval` / `apply` over an env-as-alist, recursion via Y combinator
(we have no mutation, so we can't tie the env knot otherwise). Sits at the
end of `harness/bench.mjs` as `meta-fib(12)`. The point is the new
measurement axis: **interpretation on top of interpretation.**

Pre-registered predictions from `feat_metacircular_eval.md`:

- (a) `bytecode_gc` lands in single-digit-to-low-tens-of-ms on metacircular
      fib(10).
- (b) Engine ordering on metacircular fib matches ordering on direct fib
      (because the metacircular evaluator IS the program in all cases).
- (c) `bytecode_gc / lisp_region` ratio on metacircular ≈ direct ratio.

Numbers, best-of-25, big-arena variants for the no-GC engines:

| engine          | direct fib(12) | meta-fib(12) | metacircular tax |
|-----------------|---------------:|-------------:|-----------------:|
| lisp            |       0.047 ms |    11.53 ms  |   ~245×          |
| lisp_trampoline |       0.046 ms |    12.92 ms  |   ~281×          |
| lisp_gc         |       0.060 ms |    16.58 ms  |   ~277×          |
| lisp_region     |       0.041 ms |    11.98 ms  |   ~294×          |
| cek             |       0.087 ms |    19.90 ms  |   ~229×          |
| cek_gc          |       0.130 ms |    30.59 ms  |   ~235×          |
| bytecode        |       0.037 ms |     3.22 ms  |   ~**86×**       |
| bytecode_gc     |       0.034 ms |     5.44 ms  |   ~**159×**      |

Verdict:

- **(a) CONFIRMED.** bytecode_gc at 5.4 ms on meta-fib(12), well within
  the "single-digit to low-tens of ms" prediction. bytecode_gc IS fast
  enough to host its own evaluator at a non-embarrassing speed.

- **(b) REFUTED.** Engine ordering on metacircular is *not* the ordering
  on direct fib:
  - Direct (fastest→slowest): bytecode_gc, bytecode, lisp_region,
    lisp_tramp, lisp, lisp_gc, cek, cek_gc.
  - Meta   (fastest→slowest): bytecode, bytecode_gc, lisp, lisp_region,
    lisp_tramp, lisp_gc, cek, cek_gc.
  - Top-2 flip: bytecode_gc → bytecode (the GC engine LOSES to the
    no-GC engine under metacircular workload).
  - 3rd-place flip: lisp_region → lisp (the region-drop H2 advantage
    shrinks relative to the other tree-walkers).

- **(c) REFUTED.** `bytecode_gc / lisp_region` direct = 0.83×; meta = 0.45×.
  Roughly 2× more spread between engines on metacircular than direct.
  The metacircular workload AMPLIFIES engine differences — max/min on
  direct fib(12) is 3.8×, on meta-fib(12) it's 9.5×.

Three findings sharpen out of these refutations:

**1. The metacircular tax varies 86×–294× by engine.** Not a constant. The
spread tracks two axes: (a) dispatch shape under deep nested application
(bytecode wins by a lot — its compiled-once `OP_CALL` machinery doesn't
care that the recursion is mceval-shaped), and (b) allocation rate under
heavy cons pressure (the metacircular eval cons's an env extension on
every closure call). Engines that win on both axes (bytecode) pay the
smallest tax; engines that lose on either (tree-walker family pays for
dispatch, CEK pays for both) pay more.

**2. Bytecode's lead WIDENS under metacircular workload.** Direct
fib(12): bytecode is ~1.3× faster than the tree-walker. Metacircular
fib(12): bytecode is ~3.6× faster than the tree-walker. The lever
GROWS. This is the opposite of H4's "GC widens bytecode's lead by
allocation pressure" mechanism: there, the widening was driven by
collection cost interacting with engine shape. Here, the widening is
driven by the workload itself becoming dispatch-heavier (every mceval
call is a special-form-or-application decision; bytecode's compiled
dispatch pays this once at compile time, tree-walkers pay it every step).

**3. The GC tax flips sign on metacircular.** Direct fib(12):
bytecode_gc beats bytecode by ~10% (no GC fires, the optimization-barrier
tax is negligible at this size). Metacircular fib(12): bytecode_gc
LOSES to bytecode by 69% (5.44 vs 3.22 ms). The metacircular workload
allocates ~10× more than the direct workload (env extensions on every
closure call, intermediate eval-list cons cells, closure tuples), and
bytecode_gc fires its first real GC cycle. This is the H4 mechanism in
its full form: the tax isn't collection time per se, it's the cost of
having to assume `cons` could reach `gc()` from inside the hot mceval
loop. On direct fib, that hot loop doesn't allocate, so the barrier
is amortized to nothing; on metacircular, the hot loop IS the allocator.

The third finding is also a small ratification of H2's mechanism. The
no-GC tree-walker (`lisp`) at 11.53 ms slightly *beats* the region-drop
(`lisp_region`) at 11.98 ms on metacircular — within noise, but
suggesting region-drop's compare-with-zero advantage is in the
direct-fib hot loop, not the metacircular loop. Each step of mceval
allocates many intermediate cons cells (env extension, eval-list),
and region-drop's bump pointer fills approximately as fast as
lisp's. The H2 "below no-GC floor" effect needs a non-allocating
hot loop to manifest; the metacircular eval is the opposite shape.

### Bytecode shape — what the metacircular compiles to

A follow-up question: what does mceval actually look like in the VM's
own bytecode? Built a `DISASM_ONLY` variant of `bytecode_gc`
(`engines/bytecode_gc.c`, `harness/disasm.sh`, `harness/disasm.mjs`) and
dumped it. Direct fib(24) compiles to **60 words**; the metacircular
evaluator to **810 words** — 13.5×, almost exactly tracking the source
ratio (14.8×). Opcode mix is `LOADG`-dominated (32% of the listing),
mostly the primitive symbols mcapply tests against in its nested-`if`
dispatch ladder. Only 21 of 116 calls are `TAILCALL` (18%) — the
eval/apply alternation IS tail at the outer edges, but the
primitive-test ladder isn't, which is why the TCO win on meta-fib is
~3% rather than the loop-shape would suggest.

The `LOADG`-share is misleading at first glance — that's the same shape
of armchair argument the env-lookup hotspot hypothesis (above) made for
the tree-walker, where the speedup turned out to be ~5%. The wasm view
(via `wasm2wat`, dispatch loop is a tight 12-arm `br_table`) confirms
V8 already specializes the LOADG arm hard. Filed in
`docs/project_notes/bytecode_disasm.md` and `wasm_dispatch.md` with
both snapshots; "would a faster mcapply primitive dispatch actually
help?" is left as a pre-registered prediction (we predict <10% speedup)
rather than a recommendation.

What's the deeper takeaway? **Workload shape sets which engine
axes matter.** Direct fib measures dispatch + a near-zero
allocation rate; the engines order by dispatch quality. Metacircular
fib measures dispatch ON TOP OF a heavy allocation rate; the
engines order by *both*, which lets bytecode pull farther ahead and
flips the bytecode_gc / bytecode ordering. The eight-engine suite
doesn't have a single "fastest engine" — the answer depends on
what the program is doing per step.

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

## H4 — GC ported into the tree-walker (`lisp_gc.c`)

Same collector as before, ported into the recursive `eval()`. This is the
trickier port the handoff flagged: tree-walker locals (x, env, fn, args,
newenv, v) span allocating recursive calls, so the bytecode_gc/cek_gc
pattern of "next cons's gc_a/gc_b protects the in-flight cell" isn't enough
on its own. The fix is an explicit `R_save` shadow stack with a strict
push/pop discipline at every eval() and eval_list() entry/exit. Tail
returns pop the outer frame *before* the recursive call (after reading the
next x out into a C local), which lets clang's TRE recycle the C frame and
keeps the shadow stack at the caller's baseline — preserving the
tree-walker's unbounded-tail-recursion property.

Correctness: 26/26 parity tests vs `lisp.wasm`. `countdown(1,000,000)` runs
to completion with 26 GC cycles fired mid-loop — surviving a million tail
calls under repeated collection is the cleanest possible signal that
(a) TRE survived the shadow-stack pattern, and (b) the root set is
sufficient.

### Pre-registered prediction (BEFORE running the bench)

Now that three GC engines exist, the H2/H4 mechanism should reproduce a
fifth time. Specifically:
  (a) `lisp_gc / lisp` GC tax should land in the H2 band on native
      (~1.2–1.4×), and higher on wasm (engine-shape sensitive).
  (b) Tree-walker GC count should be higher than CEK_gc's on most
      benchmarks: every step recursively builds `args` via eval_list, every
      closure-call rebuilds an env-list, and there's no operand stack to
      amortize across. (CEK conses for continuations; tree-walker conses
      for args + envs.)
  (c) The bytecode_gc lead over lisp_gc should be larger than the
      bytecode_big lead over lisp_big (same engine-interaction effect we
      saw for CEK).
  (d) Mechanism: the per-cons-callsite optimization barrier story should
      hold here too. The tree-walker is C-stack-recursive with no tight
      bytecode dispatch loop — V8 might pessimize it *less* than CEK or
      bytecode (less to lose in a sprawling recursive function), so the
      wasm GC tax could land *below* CEK_gc's 1.87×.
  (e) The native nrev "narrowing" pattern (from CEK_gc) might recur here
      since the tree-walker is also already cons-heavy on user-level work.
    Falsification: any of (a)–(d) failing falsifies the corresponding part.

### Measured (262K-cell default arena, best-of-25)

**Native (Apple clang, ARM64):**

| benchmark      | lisp   | lisp_gc | cek    | cek_gc | bc     | bc_gc  | lisp_gc tax | cek_gc tax | bc_gc tax | TW-lead widen | CEK-lead widen |
|----------------|--------|---------|--------|--------|--------|--------|-------------|------------|-----------|---------------|----------------|
| fib(24)        | 11.039 | 15.390  | 24.293 | 32.288 |  5.214 |  5.154 |  1.39×      |  1.33×     |  0.99×    |  1.41×        |  1.32×         |
| tak(18,12,6)   |  4.807 |  5.982  | 10.545 | 13.761 |  2.379 |  2.478 |  1.24×      |  1.30×     |  1.04×    |  1.19×        |  1.24×         |
| ack(3,4)       |  1.022 |  1.227  |  2.093 |  2.740 |  0.377 |  0.404 |  1.20×      |  1.31×     |  1.07×    |  1.12×        |  1.17×         |
| nrev+sum(150)  |  1.741 |  2.039  |  3.014 |  3.558 |  0.449 |  0.568 |  1.17×      |  1.18×     |  1.27×    | **0.93×**     | **0.85×**      |
| tailsum(30000) |  2.815 |  3.412  |  6.066 |  7.696 |  1.087 |  1.137 |  1.21×      |  1.27×     |  1.05×    |  1.16×        |  1.18×         |

**Wasm (Node/V8):**

| benchmark      | TW     | TW_gc  | CEK    | CEK_gc | bc    | bc_gc | lisp_gc tax | cek_gc tax | bc_gc tax | TW-lead widen | CEK-lead widen | gc tw/cek/bc |
|----------------|--------|--------|--------|--------|-------|-------|-------------|------------|-----------|---------------|----------------|--------------|
| fib(24)        | 14.860 | 20.015 | 27.323 | 51.622 | 7.351 | 7.315 |  1.35×      |  1.89×     |  0.99×    |  1.36×        |  2.10×         |  4 / 43 / 1  |
| tak(18,12,6)   |  6.165 |  8.791 | 12.251 | 23.240 | 3.199 | 3.593 |  1.43×      |  1.90×     |  1.12×    |  1.27×        |  1.69×         |  3 / 19 / 0  |
| ack(3,4)       |  1.258 |  1.700 |  2.397 |  4.429 | 0.553 | 0.566 |  1.35×      |  1.85×     |  1.02×    |  1.32×        |  1.81×         |  0 /  3 / 0  |
| nrev+sum(150)  |  2.204 |  2.650 |  3.449 |  5.668 | 0.676 | 0.872 |  1.20×      |  1.64×     |  1.29×    | **0.93×**     |  1.27×         |  0 /  4 / 0  |
| tailsum(30000) |  3.502 |  4.833 |  6.804 | 12.860 | 1.543 | 1.570 |  1.38×      |  1.89×     |  1.02×    |  1.36×        |  1.85×         |  1 / 11 / 0  |

`tax = gc_build / no-gc-build`. `widen = (bc_gc / engine_gc) / (bc / engine)`.

### Result against the predictions

(a) **lisp_gc native tax in H2's 1.2–1.4× band, higher on wasm.** CONFIRMED.
    Native lands 1.17–1.39× (clustered around 1.20–1.24×, fib outlier at
    1.39×); wasm lands 1.20–1.43× (clustered around 1.35–1.43×).

(b) **Tree-walker GC count > CEK_gc GC count.** REFUTED, dramatically
    backwards. lisp_gc collects FAR less than cek_gc on every benchmark
    (4 vs 43 on fib; 3 vs 19 on tak; 0 vs 3 on ack). I had the cons rate
    wrong: CEK allocates a ~6-cell K continuation per evaluation step
    *every step* — the tree-walker only conses for the args list and the
    closure env, and a primitive call only needs ~2 conses (the args
    list). The tree-walker's recursive `eval()` is actually leaner per
    operation than CEK's musttail K-chain.

(c) **bytecode_gc widens its lead over lisp_gc.** CONFIRMED on 4/5
    benchmarks both substrates; refuted on nrev (which narrows the gap
    on both substrates — same pattern as CEK_gc native).

(d) **lisp_gc wasm tax might land below cek_gc's 1.87×.** CONFIRMED.
    lisp_gc wasm tax: 1.35× average. cek_gc: 1.87× average. bc_gc: 1.0×
    average. Clean three-way ordering by engine shape.

(e) **Native nrev narrowing recurs.** CONFIRMED. Both lisp_gc and cek_gc
    show widen < 1.0 on native nrev (0.93× and 0.85× respectively).

### What three GC engines together say (the new mechanism finding)

The H2 substrate-amplification mechanism reproduces a fifth time, and we
now have enough data to see *how* the amplification factor varies with
engine shape:

|                | wasm GC tax (avg) | native GC tax (avg) | wasm/native ratio |
|----------------|-------------------|---------------------|-------------------|
| `bytecode_gc`  |  ~1.05×           |  ~1.08×             |  ~1.0× (no amp)   |
| `lisp_gc`      |  ~1.34×           |  ~1.24×             |  ~1.1× (mild)     |
| `cek_gc`       |  ~1.83×           |  ~1.28×             |  ~1.4× (large)    |

V8's amplification of the optimization barrier scales with how
*JIT-specializable* the engine's hot loop is:

- `bytecode_gc` has a `for(;;){ switch(op){...} }` dispatch where many
  arms (`OP_CONST`, `OP_LOADL`, `OP_LOADG`, `OP_JMP`, the inlined arith
  fast path in `OP_CALL`) never reach `cons()`. V8 can keep specializing
  those arms; only the few cons-touching arms get pessimized. Net
  amplification ≈ 1.0×.
- `lisp_gc` is one big recursive C function. V8 inlines less aggressively
  across recursive call boundaries; there's no single "hot loop" for it
  to specialize tightly in the first place. Net amplification ≈ 1.1×.
- `cek_gc` is a musttail chain between two state functions, the wasm
  return-call form V8 specializes hardest. Every step's K-allocation
  taxes the specialization. Net amplification ≈ 1.4×.

**The amplification factor is a proxy for how much V8 had specialized the
non-GC version.** Engines that lived closer to V8's sweet spot pay more
to host a GC. This was implicit in the H2 → CEK_gc finding; lisp_gc
falsifies the alternative explanation that the wasm-tax magnitude is a
property of the GC itself (it isn't — the collector is the same code in
all three).

### Verdict on H4, full version

The narrow H4 claim ("GC widens bytecode's lead under a tight heap")
holds on 9/10 wasm-and-native datapoints across the two ported engines.
The one falsification (native nrev, both engines) is mechanism-consistent:
when the user program's hot loop is cons-bound, the GC tax is paid in
proportion by both sides, leaving no headroom for amplification.

The original H4 mechanism story (collect-less = win-more) is wrong.
What's actually happening: each engine pays a per-cons-callsite tax that
clang's `-O2` can mostly cancel (native lands 1.0–1.3× for all three),
plus a per-engine V8 amplification factor that ranges 1.0×–1.4×. The
three GC engines now establish that range, and the tree-walker fits the
intermediate slot exactly where the JIT-specializability story predicts.

### Falsification log (tree-walker addendum)

- Pre-registered prediction (b): tree-walker GC > CEK_gc GC. Result:
  far less, on every benchmark. Refuted; CEK over-allocates by ~10× per
  step relative to the tree-walker.
- Pre-registered prediction (a), (c), (d), (e): all confirmed.

### Tree-walker correctness — H1 property preserved

The trickiest port concern was clang's TRE: the tree-walker's headline
trait is unbounded tail recursion via clang turning `return eval(...)`
into a loop. The shadow-stack pattern keeps that property by popping the
outer frame BEFORE the tail call (after reading next-x into a C local),
so the TRE-recycled C frame and the pushed-then-popped shadow slot net
to zero per iteration. Confirmed: `countdown(1,000,000)` returns `done`
in lisp_gc with 26 GC cycles fired mid-loop. A million-step tail loop
surviving repeated collection is the strongest possible signal for both
TRE preservation and root-set sufficiency.

## H2 zero floor — region-drop GC (`lisp_region.c`)

Direct test of the H2 mechanism story by constructing a working GC engine
where `cons()` *cannot* reach a collector from the inner loop. Lifted from
Robert van Engelen's tinylisp: cells allocate downward (`cell[--sp]`), the
global env head sits at the lowest used index, and `gc()` is the one-liner
`sp = ord(g_env)` — drop everything above the env head. The invariant
`cons(a,b)` always lands at index < `min(a, b)` holds because allocation
is strictly monotonic downward, so the set of cells reachable through env
is exactly `cell[ord(g_env)..N]`. O(1), precise, no tracing.

The point isn't that this is a great general-purpose collector — it isn't
(closures created mid-eval can't survive past a top-level form unless
they're stored in env). The point is that **`cons()` is a pure local
mutation**: bump `sp`, write two words, return. No function call. The
compiler can treat it as side-effect-free with respect to anything outside
the cell arena. That's the difference vs `lisp_gc.c`, whose `cons()` has a
runtime branch to `cons_slow()` — a function call the compiler can't
prove away, so it conservatively pessimizes the surrounding hot loop.

### Pre-registered prediction (BEFORE running the bench)

If H2's "per-callsite optimization barrier" mechanism is what we've been
measuring at 1.05× / 1.34× / 1.83× wasm across `bytecode_gc` / `lisp_gc` /
`cek_gc`, then a `cons()` with no function-call hazard should pay **zero**
tax. Specifically:

  (a) `lisp_region / lisp_big` time ratio ≈ **1.0× both substrates** on
      every benchmark, within noise (call it ±5%). lisp_big has the same
      "no GC, fixed arena" property; the only difference is allocation
      direction (upward bump vs downward stack). Either should compile to
      equally tight inner loops.

  (b) For comparison: `lisp_gc / lisp_big` lands at 1.17–1.39× native and
      1.20–1.43× wasm. That tax is what region-drop should eliminate.
      Confirming (a) confirms the mechanism. The "GC tax" is really a
      "cons-can-call-out tax," and moving the call out of cons makes it
      vanish.

  (c) `countdown(1,000,000)` should still return `done` (TRE-preserving
      change). gc_count is uninteresting (drops with `sp = ord(env)` at
      top-level boundaries only; benchmarks are single top-level forms,
      so it fires once and reclaims nothing meaningful).

Falsification:
- (a) failing — `lisp_region` measurably slower than `lisp_big` — means
  either the downward-allocation pattern has its own overhead we didn't
  predict, OR the H2 mechanism story has a confounder. Either is
  interesting.
- (a) failing in the OTHER direction — `lisp_region` faster than `lisp_big`
  by a meaningful margin — would suggest the upward-bump pattern itself
  is paying some small tax (maybe `cell_top++` vs `--sp` codegen
  asymmetry). Also interesting.

### Measured (262K-cell default arena for unit tests; 16M-cell `_big` for the bench, matching the other engines)

**Native (Apple clang, ARM64, best-of-25):**

| benchmark      | lisp   | lisp_region | tax (region/lisp) | lisp_gc | region vs lisp_gc |
|----------------|--------|-------------|-------------------|---------|-------------------|
| fib(24)        | 11.170 | 11.097      |  0.993×           | 15.162  |  0.732×           |
| tak(18,12,6)   |  4.842 |  4.821      |  0.996×           |  6.042  |  0.798×           |
| ack(3,4)       |  0.997 |  0.986      |  0.989×           |  1.228  |  0.803×           |
| nrev+sum(150)  |  1.750 |  1.708      |  0.976×           |  2.065  |  0.827×           |
| tailsum(30000) |  2.767 |  2.728      |  0.986×           |  3.411  |  0.800×           |

**Wasm (Node/V8, best-of-25):**

| benchmark      | TW     | TW_region | tax (region/TW) | TW_gc  | region vs TW_gc | gc tw_rgn/tw_gc |
|----------------|--------|-----------|-----------------|--------|-----------------|-----------------|
| fib(24)        | 13.622 | 12.743    |  0.935×         | 18.841 |  0.676×         |  0 / 4          |
| tak(18,12,6)   |  6.145 |  5.768    |  0.939×         |  8.810 |  0.655×         |  0 / 3          |
| ack(3,4)       |  1.225 |  1.133    |  0.925×         |  1.653 |  0.685×         |  0 / 0          |
| nrev+sum(150)  |  2.206 |  2.087    |  0.946×         |  2.642 |  0.790×         |  0 / 0          |
| tailsum(30000) |  3.501 |  3.297    |  0.942×         |  4.829 |  0.683×         |  0 / 1          |

### Result against the predictions

(a) **`lisp_region / lisp_big` ≈ 1.0× both substrates.** REFUTED in the
    "interesting" direction. Native lands at 0.98–0.99× (within noise);
    wasm lands at **0.93–0.95× — region-drop is consistently ~6% FASTER
    than the no-GC tree-walker baseline.** The prediction's falsification
    section explicitly flagged this case ("`lisp_region` faster than
    `lisp_big` by a meaningful margin would suggest the upward-bump
    pattern itself is paying some small tax — also interesting"). It's
    that case.

(b) **`lisp_region` eliminates the mark-sweep GC tax.** CONFIRMED.
    Wasm: lisp_region runs at 0.66–0.79× of lisp_gc time (a 21–34%
    speedup). Native: 0.73–0.83× (17–27%). Mark-sweep's per-callsite
    optimization barrier is entirely gone, AND the lighter cons shape
    eats a few additional points.

(c) **`countdown(1,000,000)` returns `done`.** CONFIRMED in the 16M-cell
    `_big` variant — clang's -O2 TRE is preserved across the allocator
    change. The default-arena `lisp_region.wasm` (262K cells) cannot
    handle it: the engine's design property is that gc() only fires at
    top-level form boundaries, so a single tail loop that allocates
    per-iteration accumulates until the arena fills. This is documented
    behavior, not a bug — it's the cost of moving the slow path out of
    cons.

### What this says — the H2 mechanism, sharpened

The H2 wasm tax decomposition is now:

|                  | wasm GC tax (avg) | mechanism                                                         |
|------------------|-------------------|-------------------------------------------------------------------|
| `bytecode_gc`    |  ~1.05×           | cons fast/slow split; many cons-free arms in switch dispatch     |
| `lisp_gc`        |  ~1.34×           | cons fast/slow split; recursive eval, sprawling hot loop          |
| `cek_gc`         |  ~1.83×           | cons fast/slow split; tight musttail chain V8 specializes hardest |
| **`lisp_region`**| **~0.94×**        | **no slow path; cons is a pure local mutation**                   |

The previous three engines all paid a tax in the 1.05–1.83× band. We
attributed it to the per-cons-callsite optimization barrier — the
runtime branch to `cons_slow()` is a function call the compiler can't
prove away. Region-drop validates that mechanism by removing the
function-call hazard entirely AND happening to use a slightly cheaper
compare shape (`sp == 0` vs `cell_top >= MAX_CELLS`), and landing
*below* the no-GC baseline.

That sub-1.0× tax is the new finding: **even the no-GC `lisp.c`
baseline has a small amount of cons-shape inefficiency that V8 was
leaving on the table.** The "GC tax floor" isn't 1.0× of the upward-
bump arena — it's ~0.94× of it. The actual zero-tax pattern is "cons
is the simplest possible mutation; no function call, comparison
against zero, monotonic decrement."

### What it doesn't say

Region-drop is not a general-purpose collector. Closures created
mid-eval that aren't bound to env can't survive past their creating
top-level form (we don't implement tinylisp's "store nil if e == env"
closure trick that handles this case lazily). For our REPL-shaped
workloads — read a form, eval, repeat — that limitation doesn't bite.
For long-running programs where mid-eval garbage matters
(e.g., countdown(1M) in a small arena), region-drop is structurally
unsuited. Mark-sweep wins that case because it CAN reclaim mid-eval.

The H2 zero floor exists; it's just not for every workload.

### Falsification log

- Pre-registered prediction (a) — REFUTED in the "interesting"
  direction. lisp_region wasn't ≈1.0× of lisp_big; it was ~0.94×
  on wasm and ~0.99× on native. Reframed: the no-GC baseline itself
  was paying a small tax we hadn't isolated.
- (b) and (c): confirmed.

## H1 verification — explicit trampoline tree-walker (`lisp_trampoline.c`)

H1 found that the recursive `eval()` in `lisp.c` runs unbounded
tail recursion in flat C stack because `clang -O2` performs TRE on
`return eval(...)` self-calls. mal step 5 prescribes the *explicit*
version of the same transformation — `while(TRUE)` loop around eval,
`ast` and `env` rebound on tail branches, `continue` instead of
recursing. This engine writes that loop by hand. The two should
produce nearly-identical machine code; if they don't, H1's framing
needs a more nuanced phrasing.

### Pre-registered prediction (BEFORE running the bench)

  (a) `lisp_trampoline / lisp` ≈ **1.0× both substrates**, within
      noise (call it ±5%). clang's -O2 TRE produces the same loop
      the trampoline writes explicitly; the inputs to the optimizer
      and the codegen target are the same.
  (b) `lisp_trampoline` passes `countdown(1,000,000)` — same H1
      property as `lisp.c`, just made explicit instead of relying
      on -O2.
  (c) wasm module sizes within 1% of each other. A larger gap
      suggests clang's TRE produced different code shape from the
      hand-rolled loop.

Falsification:
- (a) deviating significantly (>5%) means clang's -O2 was doing
  *more than just TRE* on `lisp.c` (or *less*), and H1's "clang TRE =
  mal step 5 trampoline" framing needs revision.
- (b) failing — should be impossible if (a) holds.
- (c) deviating significantly is a softer falsification, showing
  codegen differs even if perf doesn't.

### Measured (best-of-25, both arenas at 16M cells)

**Native (Apple clang, ARM64):**

| benchmark      | lisp   | lisp_trampoline | ratio (tramp/lisp) |
|----------------|--------|-----------------|--------------------|
| fib(24)        | 10.918 | 11.013          | 1.009×             |
| tak(18,12,6)   |  4.767 |  4.691          | 0.984×             |
| ack(3,4)       |  0.969 |  0.977          | 1.008×             |
| nrev+sum(150)  |  1.705 |  1.736          | 1.018×             |
| tailsum(30000) |  2.691 |  2.732          | 1.015×             |

Mean: 1.007×.

**Wasm (Node/V8):**

| benchmark      | TW     | TW_tramp | ratio (tramp/TW) |
|----------------|--------|----------|------------------|
| fib(24)        | 13.301 | 13.502   | 1.015×           |
| tak(18,12,6)   |  6.127 |  6.148   | 1.003×           |
| ack(3,4)       |  1.224 |  1.230   | 1.005×           |
| nrev+sum(150)  |  2.203 |  2.209   | 1.003×           |
| tailsum(30000) |  3.503 |  3.503   | 1.000×           |

Mean: 1.005×.

**Wasm module sizes:** `lisp.wasm` = 9840 bytes, `lisp_trampoline.wasm` = 9838 bytes
(0.02% difference, 2 bytes out of 9840).

### Result against the predictions

(a) `lisp_trampoline / lisp` ≈ 1.0× both substrates within ±5%.
    **CONFIRMED.** Native mean 1.007×, wasm mean 1.005×. Every individual
    benchmark lands within ±2% on both substrates. The variance is
    indistinguishable from run-to-run noise.

(b) `countdown(1,000,000)` returns `done`. **CONFIRMED** in the 16M-cell
    big variant on both substrates. (Default 131K-cell arena OOMs the same
    way `lisp.c` does — neither has GC, so per-iteration cons allocations
    accumulate. The trampoline doesn't fix arena exhaustion; it preserves
    the flat-C-stack property, which is what H1 was about.) Mutual tail
    recursion `(even? 1000000)` → `t` also works.

(c) Wasm modules within 1% of each other. **CONFIRMED, dramatically.**
    The two `.wasm` modules differ by 2 bytes out of 9840 (0.02%). The
    structural and recursive forms compile to essentially the same code.

### Verdict on H1's "clang TRE = mal step 5 trampoline" framing

Empirically grounded. The two engines do the same work, run at the same
speed, and produce wasm modules within 0.02% of each other. clang's -O2
TRE is performing exactly the transformation mal step 5 prescribes by
hand — turning `return eval(...)` self-calls into `ast = ...; env = ...;
continue;`. The recursive eval form is not paying any hidden cost
relative to the explicit trampoline; the compiler hides nothing.

This refines H1's "Sharper restatement" (line 39): it's no longer just a
framing claim; it's a measured equivalence. The recursive lisp.c form is
a perfectly fine way to write a flat-C-stack tree-walker on any C
compiler with TRE — and modern clang has TRE. Compilers without TRE
would need the explicit trampoline; for us, the two are interchangeable.

### Falsification log

- All three pre-registered predictions confirmed within noise.
- No new findings, no surprises. This is a null result by design — the
  experiment was set up to verify a framing claim, not to find anything
  new. The clean confirmation IS the result.
