# Decisions

## ADR-001: Add native build as measurement substrate (2026-05-29)

**Context**: The project's empirical results all flowed through V8-on-wasm via
Node. FINDINGS.md repeatedly hit caveats it couldn't resolve from that vantage
point — most notably H2's "~1.6× GC overhead" claim, which we couldn't tell was
a compiler-level effect (the cons-can-reach-gc optimization barrier) or a V8
JIT artifact. The wasm runtime layer itself was an unmeasured variable.

**Decision**: Add a parallel native build path (`bash build.sh --native`) that
compiles each engine to a native binary by including the engine `.c` as a single
TU (`-DENGINE_SRC=...`) and macro-renaming the engine's own `memset`/`memcpy`
out of libc's way. Produces `native_bench_<engine>` and `native_cli_<engine>`
binaries, runs the same five canonical benchmarks in-process best-of-25.

**Alternatives**:
- Trust the wasm numbers as-is (rejected — leaves H2 unresolved).
- Test under a different wasm runtime (wasmtime, wasm3) — would isolate the
  JIT but not the substrate. Doesn't answer "what does C alone cost?".
- Single-engine native bench — cheaper but loses the cross-engine comparison.

**Consequences**:
- ~250 lines of new code (`native/bench.c`, `native/main.c`, build.sh
  extension); regenerable artifacts gitignored.
- We can now decompose every wasm measurement into engine cost + substrate
  cost. Three immediate refinements (see FINDINGS.md "Native build" section):
  1. wasm-on-V8 overhead is 1.1×–1.6× — smaller than engine-design gaps.
  2. The JIT *flattens* engine differences (CEK gets ~1.11× overhead because
     its tight musttail loop is V8's sweet spot; bytecode gets ~1.4× because
     its dispatch loop has more interpreter machinery V8 has to be conservative
     about).
  3. GC overhead has a native floor of ~1.3× (the genuine compiler-level
     optimization barrier), amplified by V8 to ~1.4×. H2's mechanism story is
     confirmed; the magnitude is now decomposed.
- Future engine work can A/B native first (faster iteration, no JIT noise)
  then verify wasm.
- Native CLI doubles as a no-Node way to run programs (useful if the Docker
  ticket ever lands without bundling Node).

## ADR-002: User-lambda arity check + `cond` + `define`-shorthand, all eight engines (2026-05-30)

**Context**: The surface language was minimal by design (six special forms,
eleven primitives, no `cond`, no `(define (f x) ...)` shorthand), and arity
on user lambdas was unchecked — under-supply silently bound missing params
to `()`, over-supply silently dropped. Silent failure crossed the line from
"minimal-validation aesthetic" into "footgun" for anything past a fib loop.

**Decision**: Add three changes to all eight engines:

1. Arity check at call time for user closures (return `<error>` on mismatch).
   Primitives stay unchecked — they have legitimate variable-arity use
   (`(+ 1 2 3)` collapses naturally through repeated `apply_prim`) and
   matching `tinylisp/mal` on the prim side keeps the engines small.
2. `(define (f a b) body)` shorthand desugaring to `(define f (lambda (a b) body))`.
3. `cond` with `else` clause head, desugaring to nested `if`.

Tree-walkers handle (2)/(3) inline in `eval`; the CEK engines desugar and
re-tail-call `eval_expr` (the GC variant roots the in-flight nested-if via
`R_save`); bytecode rewrites the AST in `compile()`.

**Alternatives considered**:
- Float support — rejected (32-bit tagged values have no room for a `double`
  without NaN-boxing all engines or heap-boxing each float; printing alone
  is ~500 lines of Grisu/Ryū; and floats wouldn't change any of the integer
  fib/loop/list benchmark ratios that drive FINDINGS).
- Variadic lambdas (`(lambda (x . rest) ...)`) — bigger lift (dotted-pair
  reader + binder tail-gather) and not currently load-bearing.
- Multi-body lambdas — small and reasonable, deferred until something needs it.
- Apply changes only to `bytecode_gc.c` — rejected because parity is the
  project's invariant; a feature that exists in one engine isn't "the
  language."

**Consequences**:
- Surface language is now `quote if define lambda let begin cond` with
  function-shorthand `define` and `else`-clause `cond`.
- Silent arity bugs in user code now surface as `<error>`.
- Parity gains 12 programs (`harness/parity.mjs` — 55 total, up from 43);
  bytecode tests gain 12 cases (`harness/test_bc.mjs` — 35 unique cases).
- No measurable bench regression — desugarings are AST/compile-time only,
  and the arity check is a single integer compare at call entry.
- Engine files grew 19–42 lines each (267 insertions across 8 engine .c
  files + the two harness files).
