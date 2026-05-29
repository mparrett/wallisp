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

**The two empty cells are honest.** Region-drop requires the live state to be
a downward-only chain anchored at a stable bottom — the global env. Neither
CEK (heap continuations are the live state; they churn mid-eval and aren't
anchored to env) nor bytecode (operand stack + call frames churn mid-eval)
naturally satisfies that invariant. Region-drop is a tree-walker-shaped
collector, not a universal one. Mark-sweep is the universal collector here;
each engine pays a different tax for hosting it (see "What each engine
taught us" below).

## Engines at a glance

| engine            | arch         | GC         | lines | arena (default) | fib(24) native ms | fib(24) wasm ms | vs `bytecode_gc` wasm |
|-------------------|--------------|------------|-------|-----------------|-------------------|-----------------|------------------------|
| `lisp.c`          | tree-walker  | none       |   370 |    131K cells   |  10.918           | 13.301          | 1.81×                  |
| `lisp_trampoline.c`| tree-walker | none       |   390 |    131K cells   |  11.013           | 13.502          | 1.84×                  |
| `lisp_region.c`   | tree-walker  | region-drop|   409 |    262K cells   |  10.811           | 12.577          | 1.71×                  |
| `lisp_gc.c`       | tree-walker  | mark-sweep |   506 |    262K cells   |  14.894           | 18.835          | 2.57×                  |
| `cek.c`           | CEK          | none       |   455 |    131K cells   |  24.303           | 27.163          | 3.70×                  |
| `cek_gc.c`        | CEK          | mark-sweep |   530 |    262K cells   |  32.159           | 51.098          | 6.97×                  |
| `bytecode.c`      | bytecode     | none       |   370 |    262K cells   |   5.188           |  7.211          | 0.98×                  |
| `bytecode_gc.c`   | bytecode     | mark-sweep |   475 |    262K cells   |   5.107           |  7.333          | 1.00× (anchor)         |

fib(24), best-of-25, both substrates. The full 5-benchmark matrix
(fib, tak, ack, nrev+sum, tailsum) is in [`FINDINGS.md`](FINDINGS.md);
fib preserves the ordering across the suite.

Bench builds use `_big.wasm` variants at 16M cells for the no-GC engines so
they don't OOM on heavy benchmarks; the GC engines use their default arenas
(262K) deliberately so collection is meaningful. Source lines are the engine
file alone; the reader/printer/primitives are duplicated across files
(intentional — keeps each engine a single TU and the A/B honest).

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

- **`cek.c`** — CEK's headline feature was redundant. 2.2× slower than
  the tree-walker on fib; the only axis it exclusively wins is deep
  *non-tail* recursion. The expensive capability is real (`sum(10000)`
  works on CEK, blows the C stack on the tree-walker), but it cost a
  2.2× speed penalty and ~130 lines of machine to acquire.

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

- **`bytecode_gc.c`** — the finalist. Smallest mark-sweep tax of the
  three GC engines (~1.05× wasm) because the switch dispatch has many
  cons-free arms V8 can keep specializing. Inline-prims fast path
  bypasses cons-and-apply_prim for 2-arg arithmetic at runtime,
  preserving correctness under `(define +)` redefinition while
  eliminating most of the remaining tax. The shippable engine.

## Tradeoffs in two lines

If your priority is speed, the engine architecture is the lever (bytecode
wins 2.6×); GC strategy is a smaller secondary effect (0.94×-1.83× swing).

If your priority is small + simple, `lisp.c` is 370 lines and runs everything
the suite throws at it within the C stack and a 16M-cell arena;
`lisp_region.c` adds 39 lines and gives you actual memory reclamation.

## Where the ceiling is — hand-written JS and C baselines

Two reference points sitting above the engine table: the same five benchmarks
hand-written in JavaScript (V8 native, no interpreter) and in C (`-O2` native,
no interpreter). Live in `baselines/bench.{js,c}` and run inline at the end of
`node harness/bench.mjs` (the C row needs `bash build.sh --native` to produce
`native_bench_baseline`).

| benchmark      | bytecode_gc | js (V8) | c (-O2) | bc_gc / js | bc_gc / c |
|----------------|-------------|---------|---------|------------|-----------|
| fib(24)        | 7.31 ms     | 0.33 ms | 0.13 ms | 22.2×      | 57.1×     |
| tak(18,12,6)   | 3.56 ms     | 0.15 ms | 0.05 ms | 23.8×      | 71.1×     |
| ack(3,4)       | 0.57 ms     | 0.05 ms | 0.02 ms | 11.5×      | 28.3×     |
| nrev+sum(150)  | 0.87 ms     | 0.09 ms | 0.15 ms |  9.7×      |  6.0×     |
| tailsum(30000) | 1.57 ms     | 0.02 ms | 0.00 ms | 96.9×      |  ∞ (folded) |

Two findings:

- **JS beats C on `nrev+sum`.** V8's young-gen bump allocator handles 11k
  short-lived cons cells per iteration faster than glibc `malloc`. The only
  benchmark where the interpreter-to-JS gap is wider than the interpreter-to-C
  gap. On the compute-only rows, C wins over JS by the expected 2–3×.
- **`tailsum(30000)` in C folds to closed-form.** clang at `-O2` rewrites the
  tail recursion as `n*(n+1)/2`. The interpreters can't see the whole program;
  this is the structural reason there's a ceiling no interpreter can reach.

The engine work in this project closes the gap between `cek_gc` (worst, ~57ms
fib) and `bytecode_gc` (finalist, ~7.3ms) — about 8×, on the interpreter side
of a fixed substrate. The remaining ~55× to native C lives in
compiler/JIT territory.
