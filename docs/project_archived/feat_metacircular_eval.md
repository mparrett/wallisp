---
status: done
assigned: claude-code
created: 2026-05-29
updated: 2026-05-29
shipped: 2026-05-29
shipped_in: baselines/metacircular.lisp, harness/bench.mjs, FINDINGS.md "Metacircular evaluator — Lisp-in-Lisp on each engine"
---
# Feature: metacircular evaluator (`baselines/metacircular.lisp`)

## Outcome (2026-05-29)

Shipped. Two of three pre-registered predictions REFUTED — the more
interesting outcome.

  (a) bytecode_gc lands single-digit-to-low-tens-of-ms on meta-fib(10) →
      **CONFIRMED**: 5.44 ms on meta-fib(12) on bytecode_gc.
  (b) Engine ordering on meta matches direct → **REFUTED**: bytecode_gc
      drops from 1st to 2nd (GC tax appears under heavy allocation);
      lisp jumps from 5th to 3rd; lisp_region's H2 advantage shrinks.
  (c) bytecode_gc / lisp_region ratio stays close → **REFUTED**: direct
      0.83×, meta 0.45× — engine differences amplify ~2× on metacircular.

Three findings sharpen: the metacircular tax varies 86×–294× by engine
(not constant); bytecode's lead WIDENS under metacircular workload
(1.3× → 3.6× over tree-walker, opposite of H4's mechanism); the GC tax
flips sign on metacircular (bytecode_gc loses 69% to bytecode because
the eval loop IS the allocator). Full writeup in FINDINGS.md
"Metacircular evaluator — Lisp-in-Lisp on each engine."

The Y-combinator approach worked cleanly — no engine surface changes
needed. Number-vs-symbol detection via "unbound lookup → self-eval"
worked as predicted in the design notes; no `number?` primitive added.

---

## Summary

Write a tiny Lisp interpreter in wallisp's own Lisp — `eval` and `apply`
defined over an environment-as-association-list, dispatched on the head
symbol of the form. Run a small benchmark (`fib(10)` or similar) on top of
this interpreter, which is itself running on each of the eight engines.

The result is a 9th measurement axis: **interpretation on top of
interpretation.** What does it cost to host a Lisp evaluator inside a
Lisp evaluator? Does the engine ordering on direct fib mirror the
ordering on metacircular fib?

Reference: `docs/project_notes/external_inspirations.md` — "William E. Byrd
— The Most Beautiful Program Ever Written" entry, where this idea came in.

## Why interesting

Three measurement questions, none of which we can currently answer:

1. **Is `bytecode_gc` fast enough to host its own evaluator?** If
   metacircular fib(10) takes seconds rather than milliseconds, the
   finalist engine has a "you can't realistically write programs that
   interpret programs on this" ceiling. If it lands in tens of ms, we have
   a competent platform for meta-programming.

2. **Is the metacircular tax constant across engines, or shape-dependent?**
   The naive expectation is "constant": the metacircular evaluator IS the
   program in all eight cases, so the engine's per-step cost is paid
   uniformly, and engine ordering on direct fib should match ordering on
   metacircular fib. The interesting alternative is "shape-dependent":
   the metacircular eval loop is itself a tree-walker over s-expressions,
   so engines that already specialize tree-walker shapes (lisp_region's
   region-drop, bytecode_gc's inline-prim path) might gain or lose more
   under the new workload. V8's JIT could specialize differently because
   the hot loop is now a Lisp-level eval-list rather than the engine's C
   loop.

3. **Does our Lisp have the expressive surface to write a self-interpreter
   cleanly?** We have: `define`, `lambda`, `if`, `begin`, `let`, `quote`,
   `cons`/`car`/`cdr`, `null?`/`pair?`, `+`/`-`/`*`/`=`/`<`. No `cond`,
   no `let*`, no `letrec` (top-level `define` covers it), no string ops,
   probably no `eq?` for symbols (need to verify `=` works for interned
   symbols — current bytecode_gc.c:156 shows PR_EQ is pointer-equality
   on tagged values, which should work for symbols). If we can't write
   the interpreter cleanly, that's itself a finding about which features
   are load-bearing for self-hosting.

## Sketch

A minimal evaluator in our subset, environment as alist:

```lisp
(begin
  ; env helpers
  (define lookup
    (lambda (sym env)
      (if (null? env) 'unbound
          (if (= (car (car env)) sym)
              (cdr (car env))
              (lookup sym (cdr env))))))

  (define extend
    (lambda (params args env)
      (if (null? params) env
          (cons (cons (car params) (car args))
                (extend (cdr params) (cdr args) env)))))

  ; closure repr: (closure params body env)
  (define mk-closure
    (lambda (params body env) (cons 'closure (cons params (cons body (cons env nil))))))

  ; eval-list maps eval over args
  (define eval-list
    (lambda (l env)
      (if (null? l) nil
          (cons (mceval (car l) env)
                (eval-list (cdr l) env)))))

  ; the evaluator itself
  (define mceval
    (lambda (form env)
      (if (pair? form)
          (begin
            ; dispatch on head; primitive ops are inlined for simplicity
            (let ((op (car form)))
              ...))
          (if (= form 0) 0          ; numbers self-eval — need a number?
              (lookup form env)))))

  ; benchmark: fib(10) interpreted by mceval
  (define prog
    '(begin
       (define fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2))))))
       (fib 10)))

  (mceval prog '()))
```

Open design points (resolve while implementing):

- **Number detection.** Without a `number?` primitive we can't distinguish
  symbols from numbers cleanly. Options: (a) reserve a special form for
  literal numbers, e.g., `(num 5)`; (b) add a `number?` primitive (small
  engine change across all 8); (c) treat anything that isn't a known
  symbol as a number and rely on `lookup` returning `'unbound`. Option (c)
  is cleanest if it works.
- **Special-form dispatch.** Each special form in the host language needs
  a matching arm in the metacircular evaluator: `quote`, `if`, `lambda`,
  `define`, `begin`, `let`. Each is straightforward but adds lines.
- **Closure call.** Apply walks the closure's captured env, extends with
  params/args, evals body. Standard.
- **Primitive call.** When the head resolves to a primitive op (`+`,
  `cons`, etc.), we need to apply it to evaluated args. Either dispatch
  on the symbol in mceval (`(= op '+) (+ (car args) (car (cdr args)))`),
  or rely on the host's apply mechanism. The first is more self-contained.

## Pre-registered prediction (record BEFORE measuring)

- (a) Metacircular fib(10) lands in **single-digit to low-tens-of-ms** on
      `bytecode_gc`. Justification: fib(10) is 177 function calls; the
      metacircular tax per call is roughly the cost of one mceval iteration
      (~10-50 host-level operations), so ~1.8-9k host-level ops. Direct
      fib(10) on bytecode_gc is ~0.5ms; metacircular should be 10-50× that.
- (b) Engine ordering on metacircular fib MATCHES ordering on direct fib
      within noise. The metacircular tax is roughly constant per engine
      because the same Lisp-level eval loop runs on all of them.
- (c) `bytecode_gc / lisp_region` ratio on metacircular fib should be
      close to the direct ratio (~0.07× from current bench). If it
      DIVERGES significantly, the metacircular workload is exposing an
      engine specialization that the direct benchmark missed.

Falsification:
- (a) failing to >50ms on bytecode_gc — host is too slow for
      meta-programming workloads, or the implementation has unintended
      O(n²) shape.
- (b) ordering inversion — interesting finding; suggests engine
      JIT-specializability shifts with workload shape.
- (c) ratio >2× off from direct — flags an engine-specific
      metacircular cost we'd want to understand.

## What this ISN'T

- **Not a new engine.** It's a benchmark program written in the host
  language, dispatched through each existing engine.
- **Not aiming for self-hosting wallisp.** The metacircular evaluator
  doesn't compile to bytecode, doesn't manage memory, doesn't reach the
  full subset. It's "evaluator-shaped Lisp code that runs."
- **Not a step toward miniKanren / Barliman / relational eval.** That's a
  different project. This is the implementation-level measurement axis
  Byrd's framing happens to suggest.

## Acceptance

- `baselines/metacircular.lisp` runs cleanly on all 8 engines (parity
  test extends to cover it: same output across engines).
- `harness/bench.mjs` (or a separate harness file) prints metacircular
  fib(N) times alongside the existing engine table.
- Verdict on predictions (a)–(c) recorded in FINDINGS.md under a new
  section, with the engine ordering compared to direct fib.

## Related tickets

- None directly. Adjacent to the baselines work (`baselines/bench.{js,c}`)
  — same idea of "extend the substrate ladder," different direction.
