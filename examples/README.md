# examples/

Small programs that show what wallisp can do. Numbered roughly by
complexity. Each file ends with the expression whose value gets
printed — `eval_source` returns the result of the last top-level form.

## Run an example

```bash
# Via the wasm CLI (uses bytecode_gc.wasm by default — no build needed
# if you cloned with the tracked .wasm at the repo root)
node harness/lisp-cli.mjs examples/02_factorial.lisp

# Or pipe stdin
cat examples/04_lists.lisp | node harness/lisp-cli.mjs

# Or inline
node harness/lisp-cli.mjs -e '(+ 1 2)'
```

Run with a different engine by pointing the CLI at a different wasm:
the file is a hardcoded path in `harness/lisp-cli.mjs:14` — edit it to
`lisp.wasm` (tree-walker), `cek.wasm` (CEK machine), etc. All eight
engines accept the same source and produce the same output.

## The set

| file                       | what it shows                                          | result |
|----------------------------|--------------------------------------------------------|--------|
| `01_hello.lisp`            | smallest program that runs                             | `42`   |
| `02_factorial.lisp`        | classic recursion in standard s-expr form              | `3628800` |
| `03_fcall_sugar.lisp`      | the `fn(a, b)` reader sugar — same factorial, sweeter | `720`  |
| `04_lists.lisp`            | cons / car / cdr / null? / recursive list walk         | `55`   |
| `05_closures.lisp`         | lexical capture, multiple instances sharing nothing    | `21`   |

## Session examples (persistent REPL)

`06_game_session.lisp` is different: it's a **turn-loop game** that relies on
state surviving across evals, so it runs through the persistent-session REPL,
not the one-shot CLI:

```bash
node harness/repl.mjs < examples/06_game_session.lisp     # final score: 2
```

A coin-collector where each `(go ±1)` is one turn and the RNG is threaded
through the state tuple — see `docs/project_notes/terminal_game_roadmap.md`
(Milestone A validation). The REPL is line-oriented, so each form stays on one
line.

## Bigger reference programs

These live outside `examples/` because they have other roles too:

- `baselines/metacircular.lisp` — wallisp interpreting wallisp
  (a tiny eval/apply written in the language itself, used as the
  metacircular benchmark in `harness/bench.mjs`).
- `prototype/futamura/*.lisp` — programs written specifically to feed
  the Futamura specializer; each one demonstrates a specialization
  pattern (constant folding, closure inlining, etc.). Read them for the
  specialization story, not as general examples.

## What wallisp has

Integers (30-bit fixnum), cons cells, symbols, lambdas/closures, and
the special forms `quote`, `if`, `define`, `lambda`, `let`, `begin`,
`cond`, `set!`. Plus the f-call sugar in `engines/reader.h`.

## What wallisp doesn't have

No strings (except in `bytecode_gc` via EXP1, see
`engines/bytecode_gc.c`), no floats, no vectors, no hash tables, no
modules. The point is the engine ladder, not the language surface.
