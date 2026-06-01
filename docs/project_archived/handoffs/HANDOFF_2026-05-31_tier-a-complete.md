# Session Handoff

**Created:** 2026-06-01T06:40:49Z
**Session ID:** 7f48fa69-a9dc-4557-ab68-7f8700ba8250
**Working Directory:** /Users/matt/projects-new/wallisp

## What to read first
Tier A from `docs/project_notes/gap_closure_plan.md` is fully shipped — PR1 (prim validation) and PR2 (mutation) both landed across all 8 engines, all 8 commits pushed to `origin/main`, working tree clean. Two parity-breaking experiments (EXP1 strings, EXP2 call/cc) are next in the plan, both pre-registered with hypotheses ready to test.

## Summary
Started with a "legs vs toy" audit identifying where the project reads as research-grade vs toy, plus a phased gap-closure plan. Then mechanically shipped two rollouts: PR1 (primitive arity/type validation, `/`, `mod`, 30-bit overflow trap) and PR2 (`set!`, `set-car!`, `set-cdr!`). Two notable findings surfaced: post-PR1, `bytecode_gc` now *beats* `bytecode` on prim-density-bound benchmarks (inline-prim fast path absorbs validation, no-inline-prim engines pay the tax twice); and `cek.wasm` literally cannot compute unmemo fib(18) in its default 131K-cell arena, but PR2's memoization makes the same program fit — mutation as a capability, not an optimization.

## Current State
Branch: `main` (up to date with origin).

Session commits (8 total, all pushed):

```
27df022  docs: legs-vs-toy audit + PR1 gap closure plan
5495693  feat(lisp): PR1a — prim validation, div/mod, 30-bit overflow trap
d20c764  feat(bytecode_gc): PR1b — prim validation through the inline-prim arms
52d373a  feat(engines): PR1c — mechanical port of prim validation to remaining 6
2e107ef  docs: refresh ENGINES.md table for post-PR1 numbers
354f740  feat(lisp): PR2a — set! / set-car! / set-cdr! on the tree-walker
945c7d6  feat(bytecode_gc): PR2b — set! / set-car! / set-cdr! through the GC engine
2c9dc22  feat(engines): PR2c — mechanical port of set! / set-car! / set-cdr! to remaining 6
```

All 8 engines now support: `set!`, `set-car!`, `set-cdr!`, `/`, `mod`, validated prim arity/types, polymorphic `=` (for symbol-identity in metacircular eval), 30-bit overflow trap. Parity unified at **117/117 programs agree across all 8 engines** (98 PR1 programs + 19 PR2 programs, no special-casing). `harness/test_bc.mjs` still 70/70. Freestanding property preserved (17/17 modules zero imports).

## Uncommitted State / Untouched

*Uncommitted:* none — working tree clean.

*Untouched (deliberate):*
- **`ENGINES.md` "What each engine taught us" narrative paragraphs.** The `bytecode_gc` entry still says "Smallest mark-sweep tax of the three GC engines (~1.05× wasm)" — misleading post-PR1c (tax is now negative on fib). The headline table was refreshed (commit `2e107ef`), but the prose wasn't. Flagged in `2e107ef`'s commit body and called out again in PR2c's response.
- **PR3 — 32-bit fixnums.** Gap-closure plan keeps PR3 conditional on the PR1 overflow trap firing in real programs someone actually wanted to run. No evidence yet that it has.
- **`lisp_region.c` `set-car!` cross-form limitation.** Documented inline at `engines/lisp_region.c` `env_set` comment and in FINDINGS PR2c section. Not fixed because fixing it (write barrier or full sweep) defeats the "O(1) `gc()`" property the engine exists to test as a measurement.
- **`harness/test_bc.mjs`.** Not extended to cover PR1/PR2 — the cross-engine parity suite (117 programs in `parity.mjs`) covers everything end-to-end. `test_bc` stays as the bytecode-only correctness suite.
- **`prototype/bc_super.c` primitive-redefinition divergence.** Known (FINDINGS line ~80, prototype README). Out of PR1/PR2 scope; `prototype/` is the optimization ladder, not a shipped engine.

## Gotchas

- **Run-to-run bench variance is large (10–25%).** Use min-of-min discipline: at least 3 passes × 25 reps for wasm, 10 runs for native. ENGINES.md notes this. Two engines' apparent regression numbers can flip sign between runs.
- **Trust ratios, not magnitudes.** The PR2c bench section in FINDINGS shows several engines "regressing negative" against the ENGINES.md baseline — that's ambient V8/system load, not PR2 helping.
- **`lisp_region`'s `set-car!` limitation is real.** Within a single `(begin ...)` form: fine (no GC fires by design). Across top-level forms: `(set-car! older-cell freshly-allocated)` loses the fresh cell at the next region reset. All current parity programs are single-form `(begin ...)` wraps.
- **`=` is deliberately polymorphic.** The metacircular evaluator in `baselines/metacircular.lisp` does `(= (car form) 'quote)` — symbol identity. PR1 kept `=` as raw identity compare for exactly this reason. Don't tighten it to numeric-only without rewriting metacircular.lisp.
- **Building requires Homebrew `llvm` + `lld` on PATH.** Apple's `/usr/bin/clang` lacks the wasm32 backend. The build instructions in `DEV.md` and `build.sh` cover this.
- **Native bench requires `bash build.sh --native`** to produce `native_bench_*` and `native_bench_baseline`. Native-bench numbers under 20ms are noise-floor — min over 10 runs minimum.
- **CEK arena**: `cek.wasm` (no-GC) heavily allocates K-continuations per step. Programs that work on tree-walkers may OOM on CEK in the default 131K-cell arena. Use `cek_big.wasm` for benchmarks (16M cells) or `cek_gc.wasm` for unrestricted use.
- **Inserting new opcodes in `bytecode_gc.c` requires updating `scan_code`** in `gc()` (around line 380), or the GC walker misreads the bytecode stream as quoted-data pointers. Each new opcode needs its operand width documented there. (Documented in PR2b commit body — the one place that catches first-time porters.)

## Next Steps
Ordered by leverage and recommended sequence. All from `docs/project_notes/gap_closure_plan.md`:

1. **EXP2 — `call/cc` in CEK (recommended).** ~30 lines per CEK engine (`cek.c`, `cek_gc.c`). The K-continuation chain is already first-class internally; `call/cc` captures the current K into a value-tagged cell and on invocation jumps to it. `bytecode_gc` gets a stub returning `<error>` for parity. Pre-registered prediction: generator benchmark on CEK runs within 1.5× of `bytecode_gc`'s explicit-recursion version. **Falsification on the upside** — CEK beats `bytecode_gc` — would be the project's first benchmark where CEK exclusively wins, finally justifying its 2.2× speed penalty with a real capability. Worth a dedicated FINDINGS H-section (probably H6 or H7).

2. **EXP1 — strings in `bytecode_gc` only.** Bigger (~250 lines in `engines/bytecode_gc.c` + ~80 lines harness). Intentionally breaks all-engine parity (gate string programs to `bytecode_gc`). Pre-registered: non-uniform heap raises GC tax from current negative-on-fib back to 1.15–1.30× wasm (V8 loses specialization on the type-tagged sweep). Unlocks programs that touch text. Plan details in `gap_closure_plan.md` EXP1 section.

3. **Quick polish — `ENGINES.md` narrative.** Rewrite the `bytecode_gc` paragraph in "What each engine taught us" to reflect the PR1c flipped ordering. ~5 min, single commit.

4. **PR3 — 32-bit fixnums.** Conditional. Defer until evidence of the overflow trap firing on a program someone wanted to run. No data yet.

## Open Tickets
None tracked in `docs/project_incoming/` for PR1/PR2 follow-up. `docs/project_notes/gap_closure_plan.md` itself acts as the open-work tracker for the audit-driven work; consult it for EXP1/EXP2/PR3 details.
