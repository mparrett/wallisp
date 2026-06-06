# H11 — branchless predicate primitives in `bytecode_gc.c`

**Pre-registered 2026-06-06. Measurement pending.**

Motivated by a brief look at branchless quicksort (the "branchless partition"
trick — replace data-dependent branches with arithmetic on the destination
index). The technique is broadly useful wherever a tight loop has an
unpredictable branch. wallisp has no sort workload, but it does have several
tight loops containing `cond ? TRUE : NIL` ternary returns inside JIT-hot
br_table arms. This experiment tests whether converting those ternaries to
branchless arithmetic produces measurable speedup, or whether V8's wasm
backend has already eliminated the branches via CMOV/select.

## What's being changed

In `engines/bytecode_gc.c`, all predicate primitives currently use the
ternary pattern, e.g.:

```c
case PR_NULLP: ... return is_nil(a) ? TRUE : NIL;
```

The branchless rewrite exploits `SP_T = SP_NIL + 1` (enum at line 45):

```c
case PR_NULLP: ... return mkspec(SP_NIL + (a == NIL));
```

The change is applied at THREE sites where these predicates live:

1. `apply_prim` (the slow path, used by mcapply and non-fast-path calls):
   `PR_NULLP`, `PR_PAIRP`, `PR_LISTQ`, `PR_NUMBERP`, `PR_SYMBOLP`, `PR_EQ`,
   `PR_LT`.
2. `OP_CALL` 1-arg fast-path block — predicate arms (`PR_NULLP`, `PR_PAIRP`,
   `PR_LISTQ`, `PR_NUMBERP`, `PR_SYMBOLP`).
3. `OP_CALL` 2-arg fast-path block — `PR_EQ`, `PR_LT`.

`OP_TAILCALL` has parallel fast-path blocks; they get the same treatment.

No other engine is touched. Bench compares the patched `bytecode_gc.wasm`
column against its own baseline; the other engines act as control rows.

## The mechanism model being tested

Prior data points (FINDINGS.md and `docs/project_notes/bytecode_disasm.md`):

- The bytecode dispatch loop already compiles to a single 12-arm `br_table`
  (`docs/project_notes/wasm_dispatch.md`). V8 has clean per-arm code to
  specialize.
- Env-as-array (a related "remove a tight branch chain" change) shipped at
  ~5% on direct fib — cited as evidence that V8 specializes the
  LOADG-OF-CONSTANT path hard (`bytecode_disasm.md:75-78`).
- The 1-arg primitive fast path landed at −18.5% on meta-fib(12) and −13%
  on nrev+sum(150), but the win was attributed to *skipping arg-list cons +
  apply_prim*, not to predicate-ternary inlining (the predicate ternaries
  were already in `apply_prim` before the fast path).
- The ternaries we're rewriting are simple, side-effect-free, and live
  inside JIT-hot arms. They are exactly the pattern V8's wasm backend
  typically lowers to `select`/`cmov`.

Mechanism prediction: the C-source ternaries are already being lowered to
branchless instructions by clang's wasm32 codegen (or by V8's downstream
specialization). The hand-written arithmetic form will produce identical
machine code at JIT time, or differ only at the margin.

## Pre-registered prediction

| benchmark             | predicted ratio (patched / baseline) | reason                                    |
|-----------------------|--------------------------------------|-------------------------------------------|
| `fib(24)`             | 0.99–1.01 (noise)                    | almost no predicates in the hot path      |
| `tak(18,12,6)`        | 0.99–1.01 (noise)                    | arithmetic, no predicates                 |
| `ack(3,4)`            | 0.99–1.01 (noise)                    | one `=` per call, dominated by recursion  |
| `nrev+sum(150)`       | **0.98–1.02** (noise band)           | hot loop is `null?/car/cdr`; predicate-heavy but V8 likely already CMOV |
| `tailsum(30000)`      | 0.99–1.01 (noise)                    | tight `=`-on-fixnum, may be the tightest predicate loop in the suite |
| `meta-fib(12)`        | **0.97–1.02**                        | per-host-step predicate ladder is the densest predicate workload we have |

**Upper bound for any benchmark: 3% speedup. Below that is indistinguishable
from run-to-run noise on this bench's best-of-25 methodology.**

Falsification criteria:
- If `nrev+sum(150)` or `meta-fib(12)` shows ≥3% improvement, the mechanism
  model is wrong: V8 IS leaving branch-elimination crumbs inside br_table
  arms, and the hand-written arithmetic form is recovering them.
- If ANY benchmark slows down by ≥2%, also a refutation — would mean the
  arithmetic form pessimizes some V8 path the ternary doesn't trigger.
- A null result (all ratios within ±2%) confirms the mechanism: V8/clang
  already do this lowering.

## What we get either way

- **Confirmed (predicted null):** another data point on the FINDINGS pile
  for "wasm-on-V8 already lowers simple-shape C source as well as you
  could by hand." Useful counterweight to the temptation to micro-optimize
  the C source for the JIT.
- **Refuted (real speedup):** a small but real lever (~3-10% expected
  ceiling) and evidence that V8's per-br_table-arm optimization isn't as
  thorough as the LOADG specialization suggests. Would warrant fanning out
  to the other engines and looking for similar ternary sites.

## Method

1. Build current `bytecode_gc.wasm` (baseline). Run `node harness/bench.mjs`
   with reps=25 (default) **5 times** and record the best-of-25 number per
   benchmark from each run. Best-of-best-of-25 is the baseline.
2. Edit `engines/bytecode_gc.c` per the spec above. Rebuild.
3. Run `node harness/bench.mjs` 5 more times. Best-of-best-of-25 is the
   measured number.
4. Report ratio (measured / baseline) per benchmark.

Result correctness is cross-checked automatically by the bench harness
(`results.every(r => r === results[0])`) — any "DISAGREE" line is a bug,
not a measurement.
