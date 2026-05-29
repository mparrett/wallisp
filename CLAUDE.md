# wallisp — agent instructions

Repo tour: `DEV.md` (architecture, ISA, file map, open threads).
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
- `cek.c` needs **`-mtail-call`** (uses `__attribute__((musttail))`).
- `bytecode_gc.c` needs **`-fno-builtin`** (defines its own `memset` / `memcpy`).
  The tell when this is missing is an undefined-`memset` link error.

## Project Memory

Memory files live in `docs/project_notes/`.

**Before proposing changes**: Check `decisions.md` for existing ADRs
**When encountering errors**: Search `bugs.md` for known solutions
**When looking up config**: Check `key_facts.md` for ports, URLs, environments

When resolving bugs or making decisions, update the relevant file.
