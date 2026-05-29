I treated this as a project review, not a change request. No files changed.

**Findings**
- [FINDINGS.md](/Users/matt/projects-new/wallisp/FINDINGS.md:60) is stale in places: it still says bytecode has no TCO and later says all three engines lack GC at [FINDINGS.md](/Users/matt/projects-new/wallisp/FINDINGS.md:154), while the current repo clearly has `OP_TAILCALL` and multiple GC engines. That undermines the otherwise excellent empirical narrative.
- The VM has very loose runtime validation. `apply_prim()` assumes argument shape and types at [engines/bytecode_gc.c](/Users/matt/projects-new/wallisp/engines/bytecode_gc.c:151), and calls do not check closure arity at [engines/bytecode_gc.c](/Users/matt/projects-new/wallisp/engines/bytecode_gc.c:363). Examples I probed: `(+ 1)` returns `1`, `((lambda (x) x) 1 2)` returns `1`, `((lambda (x y) y) 1)` returns `()`, and `(+ 'a 1)` prints an empty result. Fine for a research toy, risky if this is meant to be a user-facing Lisp.
- The correctness suite is too narrow for the claims. [harness/test_bc.mjs](/Users/matt/projects-new/wallisp/harness/test_bc.mjs:48) only tests `bytecode.wasm` and `bytecode_gc.wasm`; it does not assert all-engine semantic parity, malformed input behavior, arity errors, stack/code limits, or GC pressure invariants. The benchmark harness cross-checks engines, but it is not a failing test suite.
- The build advertises “zero imports” but uses `--allow-undefined` at [build.sh](/Users/matt/projects-new/wallisp/build.sh:18) and does not appear to verify the output modules’ import sections. Runtime instantiation with `{}` catches this later, but a build-time assertion would fit the project’s rigor.

**Good**
The core idea is strong: one tiny language, multiple evaluator architectures, same ABI, same benchmark shapes, measured repeatedly. The docs are unusually good for a small experimental repo, especially [ENGINES.md](/Users/matt/projects-new/wallisp/ENGINES.md:1) and [DEV.md](/Users/matt/projects-new/wallisp/DEV.md:1). The project’s best quality is intellectual honesty: CEK was expected to help, measurement showed it mostly did not, and the repo preserved that result.

**Bad / Ugly**
The intentional source duplication keeps A/B comparisons honest, but maintenance cost is real. Reader, printer, tags, primitives, globals, and error behavior are repeated across engines, so semantic fixes need discipline. Error handling is also “tiny Lisp minimal” rather than robust: many invalid programs degrade into `nil`, empty output, or surprising arithmetic over tagged values.

**Interesting**
The most interesting parts are the negative results: clang already solving tree-walker tail recursion, CEK buying mainly deep non-tail recursion at a large cost, region-drop beating the no-GC tree-walker, and V8/JIT shape mattering more than raw wasm instruction intuition. That is the project’s durable value.

Verified:
- `node harness/test_bc.mjs` passed `46/46`.
- `node harness/bench.mjs` ran successfully; current run had `bytecode_gc` around `7.515ms` on `fib(24)` and `bytecode` beating tree-walker by `1.80x` to `3.27x` across the five benchmark shapes.