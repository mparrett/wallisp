# wallisp — agent instructions

Repo tour: `DEV.md` (architecture, ISA, file map, open threads).
Engine comparison: `ENGINES.md` (8-engine matrix, capabilities, tradeoffs).
Empirical record: `FINDINGS.md` (benchmark tables, hypotheses).
Read these on demand — they're large; don't load by default.

## Working norm — read this first

The whole project runs on **measure, don't guess**. Pre-register a prediction,
then test it. A long list of "obvious" performance hypotheses have been *falsified*
by benchmarks: the env-lookup hotspot, switch dispatch, CEK's tail calls, TCO
making recursion "unbounded", and the source of GC overhead. When in doubt, write
a microbenchmark or instrument the VM — don't reason from the armchair.

## Measurement traps (we have been bitten by each)

- **Both sides must complete.** A bare `<error>` looks like a blazing-fast result.
  Always check the actual output, not just the timing. We lost a `fib(24)`
  comparison to this once.
- **Trust ratios, not magnitudes.** Wall-clock numbers are V8-on-wasm via Node;
  they don't port. Use best-of-N and report relative speedup.
- **V8's JIT is the real arbiter, not wasm source / icount.** The slower GC build
  is actually *leaner* wasm — V8 just optimizes a call-containing hot loop less.
  Don't infer performance from `.wasm` size or instruction count.

## Diagnostic heuristic

A bare `<error>` from any **no-GC** engine (`lisp`, `cek`, `bytecode`, the whole
`prototype/` line) is almost always **arena exhaustion**, not a logic bug — they
have a fixed arena and no collector. Reach for `bytecode_gc` or bump `MAX_CELLS`
before debugging the engine.

## Build gotchas

If building by hand instead of via `build.sh`:
- **Every** engine needs **`-fno-builtin`** on LLVM 20+: without it clang
  synthesizes `strlen`/`memset` calls that break the zero-imports property
  (and, for `bytecode_gc.c`, lower its own `memset`/`memcpy` into calls to
  themselves). The tell when this is missing is an undefined-`memset` link error.
- `cek.c` / `cek_gc.c` need **`-mtail-call`** (uses `__attribute__((musttail))`).

## Project notes

Durable, reader-facing project docs live in `docs/notes/` — read and update these:

**Before proposing changes**: Check `decisions.md` for existing ADRs
**When looking up config**: Check `key_facts.md` for ports, URLs, environments

When making a decision, update the relevant file.

**Keep internal working notes out of this repo.** Audits, bug/incident logs,
session handoffs, and review transcripts are process artifacts, not public
reference — they belong in the out-of-repo notes archive, never committed here.
`docs/notes/` is only for durable docs a public visitor should see. (Earlier
such material has already been archived outside this repo and is available on
request.)
