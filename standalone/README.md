# wallisp — standalone

A tiny Lisp compiled to freestanding `wasm32` with **zero imports**:
bytecode VM with tail-call optimization, mark-sweep GC over a uniform
cons-cell arena.

This directory is a **clean extraction** from `engines/bytecode_gc.c` in
the wallisp study repo — experiment hooks (string heap, disasm mode,
hypothesis commentary) stripped, language contract pinned by `test.mjs`.
The study repo is the evidence base that picked this engine and these
design choices; see `ENGINES.md` / `FINDINGS.md` there.

The standalone is **expected to drift** from the study engine over time.
Bug fixes that matter for both should be ported deliberately, not assumed.

## Build

```bash
bash build.sh           # → wallisp.wasm
bash build.sh --check   # build + run test.mjs (80 tests: 46 core + 12 predicates + 22 prelude)
```

Requires clang with the `wasm32` target and `wasm-ld` (LLVM 12+). On
macOS, install `brew install llvm lld` and prepend them to `PATH`:

```bash
export PATH="$(brew --prefix llvm)/bin:$(brew --prefix lld)/bin:$PATH"
```

## Run

```bash
node cli.mjs program.lisp
node cli.mjs -e "(+ 1 2)"
echo "(* 6 7)" | node cli.mjs
```

To use the prelude (see `prelude.lisp`), concat it with your program:

```bash
cat prelude.lisp program.lisp | node cli.mjs
```

The prelude adds `not`, `>` / `>=` / `<=`, `length`, `reverse`, `fold`,
`append`, `map`, `filter`, and `assoc` on top of the core primitives.

## Language

- **Tagged 32-bit values**, low 2 bits = tag: `00` fixnum (30-bit signed,
  wraps at ±536M), `01` cons, `10` symbol (interned), `11` special
  (`nil`, `t`, `<error>`, primitives).
- **Special forms:** `quote if define set! lambda let begin cond`. `define`
  accepts the function shorthand `(define (f a b) body)`; `cond` recognises
  `else`.
- **Primitives:** `cons car cdr + - * / mod = < null? pair? list? number?
  symbol? set-car! set-cdr!`. Fixnum-only (no floats, no strings).
  Division-by-zero, arithmetic overflow, wrong arity, and wrong types all
  return `<error>`.
- **Reader:** recursive descent, `'` quote shorthand, `;` line comments.
- **TCO:** tail-position calls reuse the call frame, so a tail loop runs in
  constant call-stack space.

## Host ABI

The module exports four functions and has **zero imports**:

| export        | signature           | purpose                                       |
|---------------|---------------------|-----------------------------------------------|
| `input_ptr`   | `() -> i32`         | pointer to the 8 KB input buffer              |
| `output_ptr`  | `() -> i32`         | pointer to the 4 KB output buffer             |
| `eval_source` | `(i32 len) -> i32`  | compile + run; returns printed-result length  |
| `gc_count`    | `() -> i32`         | total GC cycles since the last `eval_source`  |

`eval_source(len)` resets all VM state — there is no persistent session.

## Limits

- 30-bit fixnums (no bignum)
- 262 144 cons cells (~2 MB), tunable via `MAX_CELLS` in `wallisp.c`
- 8 KB input source, 4 KB printed output (`INCAP`/`OUTCAP`)
- 65 536 bytecode words, 65 536-deep operand and call stacks

## What's not here

Lifted out vs. the study engine in `engines/bytecode_gc.c`:

- **Strings** — present in the study engine as a wrapper-cons + side heap;
  removed here because the side heap's reclamation is incomplete. A clean
  string implementation would need a free list or compactor.
- **`DISASM_ONLY` mode** — the study repo's bytecode listing harness.
- **Experiment-tagged commentary** (`EXP1`, `H1`-`H7`, `PR1`, etc.) — kept
  the engineering-rationale comments, dropped the references to the
  measurement record.

Adding any of these back is straightforward; do it explicitly when there's
a concrete reason, not by accident through a "sync from upstream" pass.
