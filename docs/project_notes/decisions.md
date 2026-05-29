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
