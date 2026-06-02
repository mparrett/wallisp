# Session Handoff

**Created:** 2026-06-01T17:30:00Z
**Session ID:** c7b6b04f-e223-4ed8-8b08-48c142ebaee9
**Working Directory:** /Users/matt/projects-new/wallisp

## What to read first
Tier B from `docs/project_notes/gap_closure_plan.md` is shipped: EXP2 (`call/cc`
on CEK) and EXP1 (strings on `bytecode_gc`). **Both pre-registered predictions
were falsified**, both falsifications are honest and decomposed — H7 and H6 in
FINDINGS document the mechanism updates. Branch is `main`, clean, 4 new commits
pushed to `origin/main`. The gap-closure plan is fully executed except PR3
(conditional, deferred).

## Summary
Resumed from `HANDOFF_2026-05-31_tier-a-complete`, archived it, then shipped both
Tier B experiments. EXP2 added `call/cc` to both CEK engines (~25 lines each)
plus a gated 16-program parity suite. EXP1 added strings to `bytecode_gc` only
(~140 lines: wrapper-cons encoding, 1 MB strheap, 5 primitives, GC mark+sweep
hooks) plus a gated 44-program parity suite. Both experiments yielded clean
falsifications with mechanism-model updates. ENGINES.md narrative was also
polished to reflect post-PR1c and post-EXP2 findings.

## Current State
Branch: `main` (up to date with `origin/main`, working tree clean).

Session commits (4 total, all pushed):

```
120ba11  feat(bytecode_gc): EXP1 — strings + falsification of H6
261efef  docs: refresh ENGINES.md narrative — bytecode_gc post-PR1, cek post-EXP2
14b0ab3  feat(cek): EXP2 — call/cc on cek.c and cek_gc.c + falsification
aea9e8b  docs: archive HANDOFF_2026-05-31_tier-a-complete
```

Test state (all green):
- `harness/parity.mjs`: **117/117** programs agree across all 8 engines (unchanged).
- `harness/parity_callcc.mjs` (new, EXP2): **16/16** agree between `cek.wasm` and `cek_gc.wasm`.
- `harness/parity_strings.mjs` (new, EXP1): **44/44** pass on `bytecode_gc.wasm`.
- `harness/test_bc.mjs`: **70/70** (unchanged).
- **17/17 wasm modules** freestanding (zero imports).

`harness/bench_callcc.mjs` (new, EXP2): generator-vs-explicit benchmark.
Reproducible decomposition of the 23× ratio.

## Uncommitted State / Untouched

*Uncommitted:* none — working tree clean.

*Untouched (deliberate):*
- **PR3 — 32-bit fixnums.** Still conditional. No evidence of the overflow trap
  firing in real programs. Defer until it does.
- **EXP1 variable-length strheap reclamation.** Strings leak until `eval_source`
  resets via `init()`. Programs that allocate strings in a hot loop will OOM
  the 1 MB strheap before they OOM cells. Free-list or compactor is ~50 more
  lines. Out of EXP1's measurement scope; revisit when a real workload needs it.
- **EXP1 extras: `string→list`, `list→string`, `substring`, `number→string`.**
  Trivial to add; not load-bearing for H6. Deferred.
- **EXP2 `call/cc` on bytecode_gc.** Plan called for an `<error>` stub. Decided
  to leave `call/cc` unbound on all non-CEK engines instead — functionally
  identical at the printer (UNBOUND → `<error>`), simpler, no new primitive id.
- **Gap-closure plan document itself.** Not updated to mark EXP1/EXP2 as
  shipped/falsified; FINDINGS H6/H7 are the authoritative record. The plan
  acts as the audit-driven work tracker for what was *proposed*; the
  measurement record lives in FINDINGS.

## Gotchas

- **EXP2 falsified at 23×, but `call/cc` itself is cheap (1.37× marginal).** The
  expensive part is the generator idiom — cycling first-class continuations
  through `set!`-stored references defeats CEK's internal control-flow
  amortization. If you write `call/cc` benchmarks, expect this decomposition.
- **CEK's tail-loop gap vs bytecode is 7.7× on EXP2's workload, not 2.2×.** The
  `cek.c` banner's 2.2× quote is fib-specific (low allocation per step). Heavy
  tail-loop workloads pay per-step K_ARGS allocation; bytecode amortizes via
  OP_TAILCALL. ENGINES.md cek paragraph was updated to call this out.
- **EXP1's H6 falsification updated the mechanism model: V8's specialisation of
  the `bytecode_gc` hot path is driven by what's reachable from the OP_CALL /
  cons fast path, not by static code size of `gc()` or `apply_prim`.** Cold-path
  growth is free; hot-path type-dispatch is what costs. Don't assume "adding
  GC work makes things slower" without checking which path it touches.
- **`apply_prim` has TWO switches.** First switch handles unary prims and
  returns directly. Second switch is gated by `if(!is_cons(d0)) return ERR;` —
  so unary prims placed there will silently error on `n=1` arity. Bit me on
  `string?` / `string-length`; fixed by moving them to the first switch.
  Documented in source. Watch for it on any new unary primitive.
- **String wrapper cons must self-evaluate in `compile()`.** A `(s_string . off)`
  wrapper looks like a function application to a naive compiler. Added an
  `is_string(x)` check near the top of `compile()` next to the fixnum/special
  case. Same pattern applies to any future tagged-cons value type.
- **`call/cc` on `cek_gc.c` requires R_save pinning across two allocations.**
  `make_cont(next)` is one cons; then `cons(cont, NIL)` is another. Between
  them, `cont` is only a C local — GC fired in either cons() needs the pin.
  Same discipline as the existing `done`/`args` pins in K_ARGS. Documented in
  source.
- **`bytecode_gc.c`'s `scan_code` walk in `gc()` doesn't need changes for
  strings.** Strings are stored as wrapper cons in `code[]` via OP_CONST; the
  existing `case OP_CONST: mark(code[p])` already reaches them. The string's
  strheap entry gets marked through `mark()`'s new `cells[i].car == s_string`
  check. No new opcode means no new operand-width entry in `scan_code` either.
- **Run-to-run bench variance still 10–25%.** Use min-of-min discipline (3+
  passes × 25 reps for wasm). The H6 measurement (Δ ratio 0.998×) is well
  inside that noise floor — falsification stands.

## Next Steps
Ordered by likely value. All optional — the gap-closure plan is effectively
exhausted; pick by what would teach the most.

1. **Push remote backups / decide what's next.** Branch is pushed; from here
   the project has no pending audit-driven work. Reasonable directions:
   - Write up a "what falsifications taught us" retrospective for the project
     log. EXP1/EXP2 both yielded mechanism updates; ENGINES.md/FINDINGS already
     capture them but a synthesis sitting next to the audit would be useful.
   - Investigate the 7.7× CEK tail-loop tax: is there a substrate fix (e.g.,
     K_ARGS frame reuse on tail dispatch)? Would change CEK's matrix position
     if it works. Pre-register a prediction first.
   - Add `string→list` / `list→string` / `number→string` to `bytecode_gc` so
     the string capability becomes useful for programs (currently programs
     that touch text are gated to a half-built suite). Cheap; ~30 lines each.
   - Variable-length strheap reclamation. ~50 lines. Lets programs use
     strings in hot loops without OOM.
   - PR3 — 32-bit fixnums, but only if a real program needs it. Still no
     evidence the overflow trap fires.

2. **A second pre-registered experiment in the "extend a benchmark to break a
   prediction" vein** would fit the project's measure-don't-guess discipline.
   Possible targets: does `bytecode_gc`'s post-PR1 advantage over `bytecode`
   generalise beyond fib? It's prim-density-bound; metacircular eval might
   be a different shape. The plan covered this informally in PR1c — a focused
   benchmark would tighten the finding.

3. **Clean-up tickets in `docs/project_incoming/`.** Most are `status: done`;
   no `in_progress`. If anything from this session needs a ticket (probably
   not — FINDINGS H6/H7 cover it), file before the trail goes cold.

## Open Tickets
None tracked in `docs/project_incoming/` for EXP1/EXP2 follow-up. No
`in_progress` tickets anywhere. `docs/project_notes/gap_closure_plan.md` was
the audit-driven work tracker; PR1, PR2, EXP1, EXP2 are all shipped. PR3 is
conditional and still unblocked-by-need.
