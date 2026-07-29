# H13 — `OP_TAILCALL`'s 1-arg inline range diverged from `OP_CALL`'s

**Pre-registered 2026-07-29. Measured — (a) and (c) FALSIFIED; fix shipped.**

> The guard widening is worth 1.060x on the affected path and exactly nothing
> elsewhere — below the predicted 1.10-1.30x. Separately, adding the switch
> cases alone (a sham build whose new arms are unreachable) made fib 2.4%
> faster with no semantic change. See `FINDINGS.md` "H13".

Found by an architecture survey, not by a failing test — which is the
interesting part. The two 1-arg primitive fast paths in `bytecode_gc.c` cover
different sets of primitives, and nothing in the repo could tell.

## What's wrong

`engines/bytecode_gc.c` inlines 1-arg primitives at two sites. The guards
disagree:

```c
// OP_CALL, line 658
} else if(n==1 && (id==PR_CAR||id==PR_CDR||(id>=PR_NULLP&&id<=PR_SYMBOLP))){

// OP_TAILCALL, line 715
} else if(n==1 && (id==PR_CAR||id==PR_CDR||(id>=PR_NULLP&&id<=PR_LISTQ))){
```

The enum order is `PR_NULLP, PR_PAIRP, PR_LISTQ, PR_NUMBERP, PR_SYMBOLP`, so
`number?` and `symbol?` are inlined in operand position but fall through to
`cons` + `apply_prim` in **tail** position. Same answers, more work.

`number?`/`symbol?` shipped 2026-06-03 to unblock the metacircular evaluator;
the `OP_CALL` guard was widened to `PR_SYMBOLP` and the `OP_TAILCALL` guard was
not. H11's pre-registration (2026-06-06) then recorded *"`OP_TAILCALL` has
parallel fast-path blocks; they get the same treatment"* — the asymmetry was
assumed away three days after it was introduced.

## Why no test caught it

The two paths are semantically identical, so parity cannot see the difference:
the slow path returns the same value, only slower. Nothing else could see it
either, because **nothing in the repo calls these primitives in tail
position**:

- The five canonical benchmarks (fib, tak, ack, nrev+sum, tailsum) never call
  `number?` or `symbol?` at all.
- `baselines/metacircular.lisp` doesn't either — it dispatches on
  `pair?`/`null?`/`=` and leans on unbound atoms self-evaluating.
- `harness/parity.mjs` covers `number?`/`symbol?` only in operand position
  (`(number? 42)`, `(if (number? 5) 'num 'sym)`), never as a lambda's last
  expression, which is what compiles to `OP_TAILCALL`.

So this is a latent performance divergence in dead territory. That framing
matters for what we expect to measure.

## The fix is not a one-character edit

Widening the guard alone would be a **correctness bug**. `OP_TAILCALL`'s switch
uses `default:` for `PR_LISTQ`:

```c
default:       r=(is_nil(a)||is_cons(a))?TRUE:NIL; break; // PR_LISTQ
```

Admitting `PR_NUMBERP`/`PR_SYMBOLP` past the guard without adding their cases
would silently evaluate both as `list?` — `(number? 5)` in tail position would
return `()`. The guard and the switch have to move together, mirroring
`OP_CALL`: explicit `case PR_LISTQ:` and `case PR_NUMBERP:`, `default:` meaning
`PR_SYMBOLP`.

## Predictions

**(a) The five canonical benchmarks: no change.** They never touch these
primitives; the only effect is a slightly different `br_table` arm body.
Falsification window: any delta beyond ±2% (the established noise floor at
sub-20ms) means the edit perturbed V8's compilation of the dispatch loop, which
would be a finding in its own right — the H2/H6 "optimization barrier" pattern
recurring.

**(b) meta-fib(12): no change**, same reason.

**(c) A purpose-built tail-position microbenchmark: 1.10–1.30× faster.** The
inline path skips one `cons` plus one `apply_prim` call per iteration. The
2-arg arithmetic inlining measured 1.27–1.41× (FINDINGS.md, "OP_CALL primitive
inlining"), and a 1-arg predicate saves strictly less work per call, so the
prediction sits at or below that band. Falsification: **<1.05×** means the
inline path isn't earning its complexity for these primitives, and the honest
response is to narrow `OP_CALL` to match `OP_TAILCALL` rather than widen
`OP_TAILCALL` — remove the divergence by deleting code. **>1.45×** means
`apply_prim` costs more than the 2-arg data implies and the slow path deserves
separate attention.

**(d) Wasm size: +0.5% or less.** Two extra `case` arms in one `br_table` arm.

## Method

1. Add parity coverage for tail-position `list?`/`number?`/`symbol?` **first**,
   and confirm it passes against the unpatched engine (they take the slow path
   and return correct values). Then confirm the naive one-character fix makes
   those new cases **fail** — the test has to demonstrate teeth before it's
   worth trusting.
2. Baseline the microbenchmark on the checked-in `bytecode_gc.wasm`.
3. Apply the guard + switch fix, rebuild, re-measure.
4. Run the five canonical benchmarks and meta-fib on both builds as controls.
5. Record the outcome either way, including "no measurable change."

## What this is worth

Little, in isolation — nothing in the repo runs the affected path. The value is
the maintenance lesson: two verbatim-duplicated ~35-line blocks drifted, and
the project's own falsification discipline propagated the wrong assumption in
H11 rather than catching it. The parity cases added in step 1 outlive the
performance question.
