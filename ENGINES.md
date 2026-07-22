# The engines — comparison

Side-by-side view of the eight engines in this project, organized along the
axes that matter for choosing one or reading the source. For the architectural
tour see [`DEV.md`](DEV.md); for the empirical record (hypotheses, benchmarks,
falsifications) see [`FINDINGS.md`](FINDINGS.md).

## The grid: what was built vs what wasn't

|             | no GC          | mark-sweep        | region-drop          |
|-------------|----------------|-------------------|----------------------|
| tree-walker | `lisp.c` *     | `lisp_gc.c`       | `lisp_region.c`      |
| CEK         | `cek.c`        | `cek_gc.c`        | **— not built**      |
| bytecode    | `bytecode.c`   | `bytecode_gc.c`   | **— not built**      |

\* tree-walker also has `lisp_trampoline.c`: structural variant of `lisp.c`
with an explicit `while(TRUE)` trampoline. Same semantics, same arena, same
no-GC. Built for H1 verification (confirmed: 1.005× wasm vs `lisp.c`,
0.02% wasm size delta — clang TRE produces equivalent code).

Beyond the canonical eight there is one **appendix experiment**: `lisp_rc.c`,
a tree-walker with a fourth GC strategy — **reference counting** (H12). It's
not a peer engine, just the axis probe; kept for its negative result (refcount
is the slowest GC strategy — see the glance table and "what each engine taught
us"). The headline framing stays "eight."

**The two empty cells are honest.** Region-drop requires the live state to be
a downward-only chain anchored at a stable bottom — the global env. Neither
CEK (heap continuations are the live state; they churn mid-eval and aren't
anchored to env) nor bytecode (operand stack + call frames churn mid-eval)
naturally satisfies that invariant. Region-drop is a tree-walker-shaped
collector, not a universal one. Mark-sweep is the universal collector here;
each engine pays a different tax for hosting it (see "What each engine
taught us" below). The refcount experiment (`lisp_rc.c`, H12) was likewise
built only on the tree-walker — the interesting comparison is
same-architecture, and its result was decisive enough there that the
CEK/bytecode ports weren't worth it.

## Engines at a glance

| engine            | arch         | GC         | lines | arena (default) | fib(24) native ms | fib(24) wasm ms | vs `bytecode_gc` wasm |
|-------------------|--------------|------------|-------|-----------------|-------------------|-----------------|------------------------|
| `lisp.c`          | tree-walker  | none       |   450 |    131K cells   |  12.86            | 16.83           | 1.90×                  |
| `lisp_trampoline.c`| tree-walker | none       |   460 |    131K cells   |  13.63            | 16.96           | 1.91×                  |
| `lisp_region.c`   | tree-walker  | region-drop|   481 |    262K cells   |  13.44            | 15.95           | 1.80×                  |
| `lisp_gc.c`       | tree-walker  | mark-sweep |   596 |    262K cells   |  16.61            | 22.59           | 2.55×                  |
| `lisp_rc.c` †     | tree-walker  | refcount   |   560 |    262K cells   |  ~27.1            | ~28.07          | 3.58×                  |
| `cek.c`           | CEK          | none       |   557 |    131K cells   |  27.81            | 34.58           | 3.90×                  |
| `cek_gc.c`        | CEK          | mark-sweep |   659 |    262K cells   |  33.49            | 62.93           | 7.10×                  |
| `bytecode.c`      | bytecode     | none       |   469 |    262K cells   |   5.92            |  9.33           | 1.05×                  |
| `bytecode_gc.c`   | bytecode     | mark-sweep |   956 |    262K cells   |   5.91            |  8.87           | 1.00× (anchor)         |

† `lisp_rc.c` was measured in a later pass (2026-07-22) on a warmer machine, so
its absolute ms sit on a different thermal calibration than the rows above —
they read high. Trust the portable `vs bytecode_gc` ratio (computed same-run)
and the controlled RC-vs-mark-sweep comparison in FINDINGS.md "H12". Verdict:
refcount is the *slowest* GC strategy here (~1.1–1.25× over mark-sweep),
because mark-sweep is lazy and barely fires when the arena fits, while
refcounting pays inc/dec eagerly on every reference.

fib(24), best-of-25, both substrates (min-of-min over 3 passes for wasm,
min over 10 runs for native — bench is noisy at sub-20ms). The full
5-benchmark matrix (fib, tak, ack, nrev+sum, tailsum) is in
[`FINDINGS.md`](FINDINGS.md); fib preserves the *qualitative* ordering
across the suite, but post-PR1 (primitive validation) the
`bytecode` / `bytecode_gc` ordering is now shape-dependent — see
"PR1c — mechanical port..." in FINDINGS.md for the flipped ordering
on prim-density-bound benchmarks.

Bench builds use `_big.wasm` variants at 16M cells for the no-GC engines so
they don't OOM on heavy benchmarks; the GC engines use their default arenas
(262K) deliberately so collection is meaningful. Source lines are the engine
file alone; the printer/primitives are still duplicated across files (each
engine remains a single TU — keeps the A/B honest). The reader was shared
into `engines/reader.h` via `#include` (see `docs/notes/shared_reader_plan.md`).

## Capabilities by architecture

Capabilities are architectural, not per-GC-variant. `bytecode_gc.c` and
`bytecode.c` can run the same programs; one just has reclaimable memory.

|                              | tree-walker                              | CEK                                      | bytecode                                 |
|------------------------------|------------------------------------------|------------------------------------------|------------------------------------------|
| Deep tail recursion          | ✓ clang `-O2` TRE flattens C stack       | ✓ wasm `return_call` (musttail)          | ✓ if `CALL_MAX` big; `OP_TAILCALL` reuses frame |
| Deep non-tail recursion      | ✗ C stack ~10k frames                    | ✓ depth lives in heap continuations      | ✗ `CALL_MAX` (configurable, but bounded) |
| TCE mechanism                | implicit (compiler TRE) or explicit (`lisp_trampoline.c`) | explicit `__attribute__((musttail))` | in-VM (`OP_TAILCALL` reuses call frame) |
| Closure encoding             | `(s_closure params body env)` cons chain | `(s_closure params body env)` cons chain | `(s_closure body-addr nparams env)` w/ bytecode address |
| Allocates per evaluation step| args list (eval_list); env extensions    | ~6-cell K continuation EVERY step        | operand stack push (no cons); env extensions on call only |
| Special-form dispatch        | cascade of `if(op==s_X)` in eval         | dispatch via K-continuation kind         | compiled once into bytecode ops          |
| Notable trap                 | deep non-tail recursion overflows C stack| highest allocation rate; arena OOMs first| no TCO without `OP_TAILCALL`             |

## What each engine taught us

The same eight engines from the angle of "what would we have missed without
this one." Each line is the load-bearing finding, with a pointer into
FINDINGS.md for the full story.

- **`lisp.c`** — the baseline. Refuted the env-lookup hotspot hypothesis,
  refuted the switch-dispatch win, and established H1 (clang `-O2` TRE
  makes the recursive tree-walker unbounded). Without this engine, we'd
  have built CEK to solve a problem that didn't exist.

- **`lisp_trampoline.c`** — H1 verification. Hand-rolled mal-step-5
  `while(TRUE)` trampoline matches the recursive form at 1.005× wasm
  and within 2 bytes of module size. The "clang TRE is doing the mal
  step 5 transformation for us" framing is now measured, not just stated.

- **`lisp_gc.c`** — H4 substrate for the tree-walker. Showed that the
  recursive eval pattern can carry a precise mark-sweep collector via an
  explicit shadow stack at every `eval()`/`eval_list()` entry; clang's
  TRE survives the discipline because tail returns pop *before* the
  recursive call, netting zero per iteration. `countdown(1,000,000)`
  runs with 26 GC cycles fired mid-loop.

- **`lisp_region.c`** — H2 zero floor. Refuted the prediction in the
  interesting direction: 0.94× wasm of the no-GC tree-walker (region-drop
  is *faster* than the no-GC baseline). The H2 tax we measured at
  1.05-1.83× across mark-sweep engines vanishes when `cons` can't reach
  any function call; an additional small win comes from the lighter
  compare shape (`sp == 0` vs `cell_top >= MAX_CELLS`).

- **`lisp_rc.c`** — H12, the fourth GC strategy. Refuted the prediction on
  *mechanism*: the refcount penalty tracks call volume (env-frame
  retain/release), not allocation, so `tak` costs more than the alloc-bound
  `nrev`. And it's the slowest GC strategy on the axis — mark-sweep is lazy
  (fires 0–4× when the arena fits) while refcount pays inc/dec eagerly on
  every reference. Needed an explicit `eval()` trampoline rather than
  C-recursion + TRE, because refcounting must release the tail-call frame
  after its body runs — the same reason SectorLambda's machine is an explicit
  loop (see `docs/notes/external_inspirations.md`).

- **`cek.c`** — CEK's headline features were both redundant. 2.2× slower
  than the tree-walker on fib; the only axis it exclusively wins is deep
  *non-tail* recursion (`sum(10000)` works on CEK, blows the C stack on
  the tree-walker). The fib tax understates the dispatch gap on heavily
  tail-looping shapes (EXP2 measured 7.7× vs `bytecode_gc` on a 2K-iter
  loop). EXP2 also tested whether exposing K as `call/cc` is a hidden
  speed win — falsified, 23× slower than `bytecode_gc` on a generator
  benchmark. `call/cc` itself is cheap (1.37× marginal); the generator
  idiom defeats CEK's internal control-flow amortization. CEK still has
  no benchmark where it exclusively wins on speed.

- **`cek_gc.c`** — H4 with the highest GC tax (~1.83× wasm). Established
  that V8's amplification of the H2 optimization barrier scales with
  engine JIT-specializability: CEK's tight musttail chain is V8's sweet
  spot, so V8 loses the most when forced to be conservative around `cons`.
  The three-way ordering (`bytecode_gc` ~1.05×, `lisp_gc` ~1.34×,
  `cek_gc` ~1.83×) maps onto engine shape.

- **`bytecode.c`** — the speed lever. 2.3-3.9× faster than the tree-walker
  across the full suite, constant across program shapes, by attacking
  the axis the substrate JIT doesn't already cover (special-form
  dispatch, env lookup via lexical address, no consed arg lists on the
  hot path). The single highest-leverage change in the project.

- **`bytecode_gc.c`** — the finalist, and post-PR1 the engine to beat on
  prim-density-bound shapes. Headline tax of ~1.05× wasm vs the no-GC
  `bytecode` *flipped sign on fib post-PR1c*: `bytecode_gc` now runs
  fib(24) slightly *faster* than `bytecode` because the inline-prim
  fast path absorbs PR1's per-op validation while the no-inline engines
  pay it twice. Inline-prims bypass cons-and-apply_prim for 2-arg
  arithmetic at runtime, preserving correctness under `(define +)`
  redefinition while eliminating most of the remaining GC tax. The
  shippable engine.

  **H8 update (2026-06-04, FINDINGS.md):** the bytecode advantage
  *widens*, not narrows, on a metacircular workload (3.94× vs the
  tree-walker on `meta-fib(12)`, up from 2.54× on direct `fib(24)`).
  The pre-registered mechanism story — that PR1's inline arithmetic
  fast path carried the win — was falsified. Revised model: the
  dominant mechanism is V8's specialization of the VM's `br_table`
  dispatch; the tree-walker's recursive eval re-walks AST cons-graphs
  on every step and pays more the more dispatch the host has to do.
  PR1's fast path still matters for correctness and a small bump on
  the arithmetic case, but it's not the headline cause.

## Tradeoffs in two lines

If your priority is speed, the engine architecture is the lever (bytecode
wins 2.6×); GC strategy is a smaller secondary effect (0.94×-1.83× swing).

If your priority is small + simple, `lisp.c` is 450 lines and runs everything
the suite throws at it within the C stack and a 16M-cell arena;
`lisp_region.c` adds 31 lines and gives you memory reclamation.

## Where the ceiling is — hand-written JS and C baselines

Two reference points sitting above the engine table: the same five benchmarks
hand-written in JavaScript (V8 native, no interpreter) and in C (`-O2` native,
no interpreter). Live in `baselines/bench.{js,c}` and run inline at the end of
`node harness/bench.mjs` (the C row needs `bash build.sh --native` to produce
`native_bench_baseline`).

| benchmark      | bytecode_gc | js (V8) | c (-O2) | bc_gc / js | bc_gc / c |
|----------------|-------------|---------|---------|------------|-----------|
| fib(24)        | 8.92 ms     | 0.43 ms | 0.13 ms | 20.7×      | 68.1×     |
| tak(18,12,6)   | 4.86 ms     | 0.20 ms | 0.05 ms | 24.9×      | 93.5×     |
| ack(3,4)       | 0.67 ms     | 0.05 ms | 0.02 ms | 12.7×      | 31.8×     |
| nrev+sum(150)  | 0.88 ms     | 0.11 ms | 0.15 ms |  7.7×      |  5.9×     |
| tailsum(30000) | 2.16 ms     | 0.02 ms | 0.00 ms | 99.9×      |  ∞ (folded) |

Two findings:

- **JS beats C on `nrev+sum`.** V8's young-gen bump allocator handles 11k
  short-lived cons cells per iteration faster than glibc `malloc`. The only
  benchmark where the interpreter-to-JS gap is wider than the interpreter-to-C
  gap. On the compute-only rows, C wins over JS by the expected 2–3×.
- **`tailsum(30000)` in C folds to closed-form.** clang at `-O2` rewrites the
  tail recursion as `n*(n+1)/2`. The interpreters can't see the whole program;
  this is the structural reason there's a ceiling no interpreter can reach.

The engine work in this project closes the gap between `cek_gc` (worst, ~63ms
fib) and `bytecode_gc` (finalist, ~8.9ms) — about 7×, on the interpreter side
of a fixed substrate. The remaining ~68× to native C lives in
compiler/JIT territory.
