# wallisp — tiny Lisp → WebAssembly

A small Lisp implemented eight ways across three architectures (tree-walker,
CEK machine, bytecode VM) and three GC strategies (none, mark-sweep,
region-drop), compiled to freestanding `wasm32` with **zero imports**, then
driven by measurement to a finalist: a bytecode VM with TCO and a
hand-rolled mark-sweep GC.

## Quick start

```bash
bash build.sh                            # builds engines -> *.wasm (needs clang+wasm-ld and node)
node harness/test_bc.mjs                 # bytecode correctness suite
node harness/lisp-cli.mjs -e "(begin (define fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))) (fib 20))"
# open web/tiny-lisp-vm.html in a browser — self-contained live REPL + writeup
```

Prebuilt `*.wasm` modules are checked in at the repo root, so the harness and CLI
work without running `build.sh` first.

`harness/bench.mjs` also prints a **baselines** table: the same five
benchmarks hand-written in JS (V8 native) and C (`-O2` native), no interpreter.
The C row needs `bash build.sh --native` to produce `native_bench_baseline`;
standalone equivalents live in `baselines/bench.{js,c}`.

## Learn more

- **[docs/index.html](docs/index.html)** — external write-up: the headline
  findings with charts, code snippets, and the methodology, in a single
  self-contained HTML page. The friendly entry point if you're just visiting.
- **[ENGINES.md](ENGINES.md)** — side-by-side comparison of the eight engines:
  what each one is, what it costs, and what it taught us. Start here if you
  want the map.
- **[DEV.md](DEV.md)** — architectural tour: the language, the engines, the
  bytecode ISA, the GC, the optimization ladder, the hand-editable WAT track,
  and open threads.
- **[FINDINGS.md](FINDINGS.md)** — the empirical record: benchmark tables,
  pre-registered hypotheses, and surprises that refuted armchair guesses.
- **[CLAUDE.md](CLAUDE.md)** — instructions for Claude Code agents working in
  this repo.
