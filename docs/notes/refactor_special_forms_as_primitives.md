---
status: open
assigned: unassigned
created: 2026-05-29
updated: 2026-05-29
---
# Refactor: dispatch special forms via the primitive table

## Summary

Restructure `eval()` to dispatch every operator — including `if`, `quote`,
`define`, `lambda`, `let`, `begin` — through the same primitive table as
`+`/`car`/`cons`. Special forms become primitives that *choose not to call
`evlis`* on their arguments. The dispatcher collapses to one line:

```c
eval(x, e) = atom(x) ? assoc(x, e)
           : cons(x) ? apply(eval(car(x), e), cdr(x), e)
           : x;
```

Lifted from Robert van Engelen's tinylisp (`src/tinylisp.c`). Compare to
our current `eval()` in `engines/lisp.c`, which is ~70 lines of cascading
`if(op==s_quote) ... if(op==s_if) ...` checks.

## Why interesting

- **Compactness.** The current cascade is six special-form branches with
  ad hoc handling each. The unified dispatch is one line of eval + N
  one-line primitive functions.
- **Extensibility.** New special forms become entries in the primitive
  table, not new branches in eval. `cond`, `letrec`, `setq`, `set-car!`
  all become trivial additions.
- **Cleaner reading of the model.** "A special form is a function that
  doesn't evaluate its arguments" is the operational truth. The current
  code obscures it behind syntactic recognition.

## Why this is NOT a measurement change

The primitive-table dispatch is conceptually the same work as the cascade
of `if(op==symbol)` checks — both are O(N) lookups in N (number of
special forms). It might even be slightly slower (table iteration vs. a
small fixed sequence of pointer-equality checks), but probably within
noise. **This refactor does not motivate H4-style A/B benchmarking.**
The question is purely "does the code read better and is it easier to
extend?"

If the refactor IS measured and turns out to perturb performance
significantly (either way), that's a finding for FINDINGS.md — but the
refactor's own justification doesn't require it.

## Sketch (for `engines/lisp.c`, possibly extended to `lisp_gc.c`)

```c
struct { const char *name; int evlis; u32 (*fn)(u32 args, u32 env); } prim_table[] = {
  // Special forms (evlis=0)
  {"quote",  0, f_quote},   // doesn't eval args
  {"if",     0, f_if},      // selectively evals args
  {"define", 0, f_define},  // partially evals args
  {"lambda", 0, f_lambda},
  {"let",    0, f_let},
  {"begin",  0, f_begin},
  // Regular primitives (evlis=1)
  {"+",      1, f_add},
  {"car",    1, f_car},
  // ...
  {NULL}
};
```

Then `apply(fn, args, env)`:

```c
u32 apply(u32 fn, u32 args, u32 env){
  if (is_prim(fn)) {
    int id = primidx(fn);
    if (prim_table[id].evlis) args = evlis(args, env);
    return prim_table[id].fn(args, env);
  }
  if (is_closure(fn)) {
    u32 bound = bind_params(fn, evlis(args, env));
    return eval(closure_body(fn), bound);
  }
  return ERR;
}
```

And `eval` collapses to the one-liner above.

## Open questions

1. **Which engines to refactor?** `lisp.c` is the most-readable
   reference; doing it there has the most pedagogical payoff. `lisp_gc.c`
   inherits the shadow-stack discipline we just designed; rewriting eval
   means re-deriving the unrooted-local windows for each branch. Probably
   do `lisp.c` first, port to `lisp_gc.c` only if the result is genuinely
   cleaner.

2. **Do `cek.c` / `bytecode.c` get the same treatment?** No — CEK and
   the bytecode compiler don't have a dispatcher in the same sense.
   CEK's `eval_expr` IS the dispatcher (transitioning through K
   continuations); the special-form recognition is intrinsic to the
   transition logic. The bytecode compiler emits different opcodes for
   different forms at compile time, not runtime. The refactor is
   tree-walker-specific.

3. **Does the shadow stack in `lisp_gc.c` survive?** Each special-form
   primitive becomes its own C function. The shadow-stack discipline
   (push x, env at entry; pop before tail return) would need to live in
   each one. Either:
   - Each `f_*` function takes (args, env) and manages its own pushes,
     OR
   - `apply` does the entry push/pop on behalf of all primitives, and
     primitives that recurse explicitly manage interior pushes.
   The second is cleaner but requires careful design. Worth sketching
   before committing.

## Acceptance

- `lisp.c` (and optionally `lisp_gc.c`) refactored to the
  primitive-table dispatcher.
- 26/26 parity tests pass.
- For `lisp_gc.c`: `countdown(1,000,000)` still returns `done`
  (TRE preserved).
- Existing bench numbers unchanged within noise (call it ±5%).
- Either: bench shows no measurable shift → call it a wash and ship
  for clarity; OR a measurable shift in either direction is a finding
  documented in FINDINGS.md.

## Related ticket

[`feat_region_drop_gc.md`](feat_region_drop_gc.md) — the other tinylisp
idea worth lifting. Independent: that one is a measurement experiment,
this one is a structural refactor. Could be done in either order.
