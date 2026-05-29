---
status: done
assigned: claude-code
created: 2026-05-29
updated: 2026-05-29
shipped: 2026-05-29
shipped_in: engines/lisp_trampoline.c, FINDINGS.md "H1 verification — explicit trampoline tree-walker"
---
# Feature: explicit-trampoline tree-walker (`lisp_trampoline.c`)

## Outcome (2026-05-29)

Shipped. All three pre-registered predictions CONFIRMED within noise:
  (a) lisp_trampoline / lisp ≈ 1.0× both substrates → measured 1.005×
      wasm, 1.007× native; all individual benchmarks within ±2%.
  (b) countdown(1M) returns done → confirmed in 16M-cell big variant.
      Mutual (even? 1M) also confirmed.
  (c) Wasm modules within 1% of each other → 9840 vs 9838 bytes
      (0.02% difference; 2 bytes).

H1's "clang TRE = mal step 5 trampoline" framing is now empirically
grounded. See FINDINGS.md "H1 verification — explicit trampoline
tree-walker" for the writeup.

---

## Summary

Add a tree-walker variant whose `eval()` is structured as `while(TRUE){ ... }`
with `ast` and `env` rebound on tail branches, instead of a recursive
function that relies on clang's -O2 TRE to produce a flat C stack. This is
the canonical mal step 5 TCO implementation, applied to our tree-walker.
A/B vs `lisp.c` answers a sharp question: how much of `lisp.c`'s flat-stack
behavior comes from clang vs. from the structural transformation?

Reference: https://github.com/kanaka/mal/blob/master/process/guide.md#step-5-tail-call-optimization

## Why interesting

Our H1 finding ("the tree-walker already has proper tail calls under -O2")
is currently phrased as a property of clang's optimizer. The mal guide
prescribes the explicit version — a hand-rolled trampoline — as the
canonical Lisp TCO pattern. The two are the SAME transformation; clang just
hides it. If we measure them side-by-side and they're indistinguishable,
H1 gets a sharper restatement: "the structural transformation is what
makes the tree-walker flat; clang either does it or we do it, and either
way the cost is the same." If they differ, we learn something specific
about what -O2 was contributing beyond TRE.

See `docs/project_notes/external_inspirations.md` for the wider mal/wallisp
comparison this ticket lives within.

## Sketch

```c
static u32 eval(u32 ast, u32 env){
  for(;;){
    if(is_fix(ast) || tagof(ast)==TAG_SPEC) return ast;
    if(is_sym(ast)){
      u32 v = env_lookup(env, ast);
      return (v==UNBOUND) ? ERR : v;
    }
    u32 op = car(ast);
    if(op==s_quote)  return car(cdr(ast));
    if(op==s_lambda) return make_closure(car(cdr(ast)), car(cdr(cdr(ast))), env);
    if(op==s_define){
      u32 name = car(cdr(ast));
      u32 val  = eval(car(cdr(cdr(ast))), env);    // STILL recursive — non-tail
      if(val==ERR) return ERR;
      global_define(name, val);
      return name;
    }
    if(op==s_if){
      u32 t = eval(car(cdr(ast)), env);            // non-tail recurse
      if(t==ERR) return ERR;
      ast = is_nil(t) ? car(cdr(cdr(cdr(ast)))) : car(cdr(cdr(ast)));
      continue;                                    // TAIL: rebind ast, loop
    }
    if(op==s_begin){
      u32 b = cdr(ast);
      if(!is_cons(b)) return NIL;
      while(is_cons(cdr(b))){
        u32 r = eval(car(b), env);                 // non-tail
        if(r==ERR) return ERR;
        b = cdr(b);
      }
      ast = car(b); continue;                      // TAIL
    }
    if(op==s_let){
      u32 binds = car(cdr(ast));
      u32 body  = car(cdr(cdr(ast)));
      u32 newenv = env;
      for(u32 bnd=binds; is_cons(bnd); bnd=cdr(bnd)){
        u32 pair = car(bnd);
        u32 v = eval(car(cdr(pair)), env);         // non-tail
        if(v==ERR) return ERR;
        newenv = env_define(newenv, car(pair), v);
      }
      ast = body; env = newenv; continue;          // TAIL
    }
    // application
    u32 fn = eval(op, env);                        // non-tail
    if(fn==ERR) return ERR;
    u32 args = eval_list(cdr(ast), env);
    if(args==ERR) return ERR;
    if(is_prim(fn)) return apply_prim(fn, args);
    if(is_closure(fn)){
      u32 newenv = car(cdr(cdr(cdr(fn))));         // captured env
      u32 p = car(cdr(fn)), a = args;
      while(is_cons(p) && is_cons(a)){
        newenv = env_define(newenv, car(p), car(a));
        p = cdr(p); a = cdr(a);
      }
      ast = car(cdr(cdr(fn))); env = newenv;
      continue;                                    // TAIL
    }
    return ERR;
  }
}
```

The non-tail eval calls (`s_define`'s value-eval, `s_if`'s condition-eval,
`s_begin`'s mid-sequence evals, `s_let`'s binding-value evals, application's
operator-and-arg evals) stay as recursive C calls — same as `lisp.c`. Only
the tail-position evals get folded into the loop.

## Pre-registered prediction (record BEFORE measuring)

- (a) `lisp_trampoline / lisp` should land at **~1.0× both substrates** on
      every benchmark. The two compile to nearly identical machine code
      because clang's -O2 TRE produces exactly the loop the trampoline
      writes by hand.
- (b) `lisp_trampoline` should pass `countdown(1,000,000)` — same H1
      property as `lisp.c`, made explicit instead of relying on clang.
- (c) The wasm modules should be very close in size — ideally within 1%.
      A larger gap suggests clang's TRE wasn't doing what we thought.

Falsification:
- (a) deviating significantly (call it >5%) means clang's -O2 was doing
  *more than just TRE* on `lisp.c` (or *less*), and H1 needs a more
  nuanced phrasing.
- (b) failing — should be impossible if (a) holds.

## What this ISN'T

A new "engine" in the H4 sense. lisp_trampoline doesn't add GC, doesn't
change semantics, doesn't introduce a new design point. It's an H1
verification experiment. If the prediction holds, the trampoline variant
exists as a portability statement (works on any C compiler without
relying on -O2 TRE) but offers no measurement story beyond confirming H1.
If the prediction fails, that itself becomes the finding.

## Acceptance

- `lisp_trampoline.wasm` builds clean (zero imports).
- 26/26 parity tests pass.
- `countdown(1,000,000)` returns `done`.
- Native + wasm benchmark numbers vs `lisp` produced and recorded in
  FINDINGS.md under a small "H1 verification" subsection.
- Verdict on predictions (a)–(c) documented.

## Related tickets

- `feat_region_drop_gc.md` — the tinylisp idea worth lifting.
- `refactor_special_forms_as_primitives.md` — the tinylisp structural idea.
  (Could be combined with this one if both are pursued: the
  trampoline-loop and the primitive-table dispatcher complement each other,
  matching what tinylisp and mal both do.)
