# Session Handoff

**Created:** 2026-06-04T08:00:00Z
**Session ID:** 279e456f-dd9c-468c-832d-ab4a18da1868
**Working Directory:** /Users/matt/projects-new/wallisp

## What to read first
Two threads from this session both ended up duplicating existing project
state — the new auto-memory `grep-before-gap-claim` records the lesson.
Specifically: (1) added `number?`/`symbol?` predicates citing
metacircular-eval as the need, but `baselines/metacircular.lisp` already
existed and didn't need them; (2) pre-registered H8 to test whether
bytecode's win generalizes to metacircular, but `docs/index.html` §9
had already published the qualitative finding 6 days earlier. Both
artifacts shipped clean and the disclosures are committed; just note
the pattern before starting any "this gap hasn't been done" work.

## Summary
Three big arcs shipped: a clean `standalone/` extraction of the finalist
VM (635 lines from 852, 80-test conformance suite, prelude included),
`number?`/`symbol?` predicates across all 8 engines + standalone with
parity now at 136/136, and H8 — the formal pre-registered falsification
of "metacircular eval narrows bytecode's advantage" (it widened, from
2.54× to 3.94× faster vs tree-walker; mechanism story revised from
"PR1's inline arithmetic fast path" to "V8's br_table dispatch
specialization"). Plus a broad staleness audit that refreshed
`docs/index.html`, added snapshot headers to 2026-05-31 plan docs, and
archived four done tickets.

## Current State
Branch: `main` (pushed to `origin/main` at commit `0bdf96e`; working tree
clean).

Session commits (12 total, all pushed):

```
0bdf96e  docs: remove orphaned harness/mc_eval.lisp (bench uses baselines/metacircular.lisp)
df1ec91  docs: staleness audit — refresh public claims, archive done tickets
7ab3087  findings(h8): metacircular bench — P1 falsified, advantage widens
3ab1211  feat(h8): metacircular evaluator + pre-registered prediction
466140c  feat(predicates): ship number? + symbol? across all 8 engines + standalone
68fdbde  standalone: ship a small prelude (not, comparators, list ops, assoc)
c8434ad  docs: file acdw "A Month with Clojure" in external_inspirations.md
bdda7b9  docs: file whitespace-sugar design notes as feat ticket (parked)
c7d4577  docs: archive codex reviews 2026-06-02 + 2026-06-02-a1
a471a6d  standalone: extract clean wallisp-core from engines/bytecode_gc.c
3ad6cda  docs+build: refresh PR1 validation + bytecode_gc extensions; fix web/build-standalone
beb0274  docs: archive HANDOFF_2026-06-01_tier-b-exps-falsified
```

Test state (all green, verified after the last doc edits):
- `harness/parity.mjs`: **136/136** programs agree across all 8 engines
  (was 117/117; +19 new for `number?`/`symbol?`).
- `harness/test_bc.mjs`: **70/70** (unchanged).
- `standalone/test.mjs`: **80/80** (46 core + 22 prelude + 12 predicates).
- **17/17 wasm modules** still zero imports.

## Uncommitted State / Untouched

*Uncommitted:* none — working tree clean.

*Untouched (deliberate):*
- **`standalone/`'s expected drift** is part of the contract — codex
  review framed this explicitly. When the lab engine changes, don't
  reflexively port to standalone; decide deliberately.
- **`gap_closure_plan.md` and `legs_vs_toy_audit.md`** are now marked as
  snapshots with status-pointer headers (in this session); content
  itself was deliberately NOT rewritten. Future-Claude should not
  "freshen" these dated docs — the snapshot framing is the point.
- **PR3 (32-bit fixnums)** still parked. No real program has tripped
  the overflow trap.
- **`refactor_special_forms_as_primitives.md`** in incoming is
  `status: open` — left alone.
- **`docs/index.html` H8 numbers** — §9 quotes ~3.6× faster; H8 measured
  3.94×. Close enough that I didn't reconcile; both directions match.
  If a future session wants to align them precisely, the §9 numbers are
  the older ones.

## In Progress
None. All threads closed cleanly. The gap-closure plan's open question
("does bytecode_gc's advantage generalize?") was H8, now answered.

## Gotchas

- **Auto-memory `grep-before-gap-claim`.** Loaded into future sessions
  via MEMORY.md. Concretely: before claiming a feature is missing or
  pre-registering a "this hasn't been measured" experiment, run
  `rg <feature>` over the repo and `git log --since=<recent> -- <path>`
  first. This session lost work twice to skipping that check.
- **The bench reports min-of-min within a single invocation but
  variance between invocations is 10-25%.** For ratios, run the bench
  3-5 times and take min across runs (script in this session's bash
  log if needed). A single run can show ratios anywhere from 0.19× to
  0.48× on meta-fib — only the cross-run min is reliable.
- **`baselines/metacircular.lisp`** is the canonical metacircular eval,
  not `harness/mc_eval.lisp` (which was removed this session). It uses
  Y-combinator recursion and the "atom self-evals if env lookup
  returns unbound" trick, which is why it doesn't need `number?` or
  `symbol?` predicates.
- **`docs/index.html` §9 ("Workload shape sets which engine axes
  matter")** is a load-bearing finding section that pre-dates H8.
  Treat it as authoritative for the qualitative metacircular story;
  H8 in FINDINGS.md is the quantitative falsification record.
- **PR1's inline arithmetic fast path was NOT the dominant cause of
  bytecode's win** — H8 falsified that mechanism story. The dominant
  cause is V8's specialization of the VM's `br_table` dispatch. The
  inline fast path matters for correctness (rebinding-safe primitive
  inlining) and a small perf bump; don't credit it for the headline
  speedup. ENGINES.md, FINDINGS.md, and h8 pre-reg doc all reflect
  this.
- **Bench discipline carry-over from prior sessions still applies:**
  `cek.wasm` arena-exhausts under default arena on heavy workloads
  (use `cek_big.wasm`); bare `<error>` from a no-GC engine is almost
  always arena exhaustion, not a logic bug.

## Next Steps
Ordered by what would teach the most. All optional — no urgent threads.

1. **Reconcile `docs/index.html` §9 with H8.** §9 quotes ~3.6× speedup;
   H8 measures 3.94×. Close, but worth aligning — and §9 might benefit
   from a cross-reference to FINDINGS H8 for readers who want the
   structured record.
2. **Investigate the CEK 7.7× tail-loop tax** (still parked from the
   2026-06-01 handoff). The pre-registration framing for this would
   need to be: pre-register a K_ARGS frame-reuse hypothesis, build it
   on a branch, measure. ENGINES.md cek paragraph documents the tax
   but no follow-up was scheduled.
3. **String reclamation in `bytecode_gc`** (~50 lines, variable-length
   free-list or compactor). Currently strings leak until
   `eval_source()` resets via `init()`. Out of EXP1's measurement
   scope but blocks any real program that uses strings in a hot loop.
   If reintroducing strings to `standalone/` is ever a goal, this is
   the prerequisite.
4. **Polish work on `standalone/`:** the README is solid; the CLI is
   minimal. If there's an audience signal (codex review's "wallisp-core"
   direction), the next moves are a richer REPL example program,
   maybe `string→list` / `number→string` if strings come back.
5. **A second pre-registered experiment in the H8 vein.** Now that the
   workload-sensitivity question is settled for dispatch shape, the
   open analog is allocation shape: does the GC-tax direction (which
   §9 saw flip sign on metacircular pre-1-arg-fast-path) hold on
   currently-running engines? §9 was measured before the 1-arg fast
   path landed. Re-running with current bench would either confirm
   the §9 mechanism story or update it.

## Open Tickets
- `docs/project_incoming/feat_docker_dev_env.md` (status: open) —
  Docker dev environment. Stable since 2026-05-29.
- `docs/project_incoming/feat_whitespace_sugar.md` (status: open) —
  Filed this session as a "parked" idea. Crosses from measurement
  study into Lisp dialect design; reader would roughly double in size.
  Not on near-term roadmap.
- `docs/project_incoming/refactor_special_forms_as_primitives.md`
  (status: open) — Stable since 2026-05-29.
