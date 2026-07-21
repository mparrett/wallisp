# Session Handoff

**Created:** 2026-05-29T11:57:47-0700
**Session ID:** 0fbb668c-d588-459b-9f1e-c146f16cfe38
**Working Directory:** wallisp

## What to read first

Repo was imported from a claude.ai web chat at the start of this session — see `docs/project_incoming/wallisp-opus48-chat-transcript.md` for the chronological build-up of the four engines. CLAUDE.md was restructured early in this session: it's now lean and agent-focused; the architectural deep-dive moved to `DEV.md`. Don't load `DEV.md`/`FINDINGS.md` by default — they're large; read on demand.

## Summary

Initialized project memory, restructured docs (CLAUDE.md split into README + DEV.md + lean CLAUDE.md per Anthropic best practices), fixed the macOS build (LLVM 20 `strlen` regression → `-fno-builtin` everywhere), added a native build path that decomposes wasm/V8 overhead from engine cost, and ported the bc_inline primitive-inlining pattern into bytecode_gc.c with measured speedups + a third independent observation of the H2 "JIT amplifies hot-loop size cost" mechanism.

## Current State

Branch `main`, working tree clean, all commits pushed locally (no remote configured).

```
fe994b6  feat(bytecode_gc): inline 2-arg arith/cmp prims in OP_CALL/TAILCALL
511e14f  docs(dev): reference per-dir READMEs + add native/ to file map
9a0fc17  docs: per-directory READMEs for engines/ and prototype/
26381ad  docs(notes): ADR-001 native build as measurement substrate
a616b5b  chore: gitignore regenerable build artifacts
9d961c5  feat(native): native build path + decompose wasm/JIT overhead
055f7fb  docs(dev): document verified JS runtimes (Node, Bun) + Deno gap
88832ac  docs: open feat_docker_dev_env
1fc3429  docs(dev): add macOS build setup section
c78650e  fix(build): -fno-builtin on all engines, restore zero-imports
fdcc6cf  docs: split CLAUDE.md into README + DEV.md + lean CLAUDE.md
b203160  docs: receive opus 4.8 chat transcript
3941ee8  docs: init project memory
```

Engines verified end-to-end this session: `node harness/test_bc.mjs` → 46/46 (23 tests × `bytecode.wasm` + `bytecode_gc.wasm`); `node harness/bench.mjs` runs all 4 engines, no DISAGREE; `./native_bench_*` all four engines produce matching results. The macOS build needs `PATH="$(brew --prefix llvm)/bin:$(brew --prefix lld)/bin:$PATH" bash build.sh` (both formulas are keg-only).

## Uncommitted State / Untouched

- **Uncommitted:** none. Tree clean.
- **Untouched (deliberate):**
  - `web/tiny-lisp-vm.html` — the self-contained browser REPL. Not exercised this session; should still work but unverified post-rebuild.
  - `wat/` (probe.wat, bc_edit.c, bc_edit.wat, bc_instr.wat) — hand-edit experiments, untouched.
  - `prototype/bc_*.c` builds — present and rebuilt, but never benchmarked this session.
  - `/tmp/bytecode_gc_baseline.wasm` and `/tmp/native_bench_bytecode_gc_baseline` — A/B baselines from the inline-prims measurement. Safe to leave; will get cleaned by tmpwatch eventually.

## In Progress

None. The session ended at a clean stopping point after the inline-prims feature shipped.

## Gotchas

- **macOS build needs `lld` + PATH gymnastics.** Apple's `/usr/bin/clang` lacks the wasm32 backend; Homebrew's `llvm` ships it but is keg-only. `wasm-ld` lives in a separate `lld` formula (also keg-only). Recipe in `DEV.md` "macOS setup" section.
- **`-fno-builtin` is required on ALL engines now** (not just `bytecode_gc.c`). LLVM 20+ synthesizes `env.strlen`/`env.memset` calls that break the zero-imports property otherwise. Documented in `build.sh:7-10`.
- **`gc_count()` returns the count for the most recent `eval_source` only** — `init()` resets `g_numgc=0`. Don't compute deltas; read after each run.
- **bench.mjs uses `bytecode_gc.wasm` at the default 262K-cell arena** deliberately (not `_big`), so `gc_count` is meaningful. The other 3 engines use `*_big.wasm` (16M cells) because they have no GC.
- **The `nrev+sum(150)` regression on wasm (0.97×) is real and instructive, not a bug.** It's the third independent observation of H2's "JIT amplifies a larger hot loop's cost" — same mechanism as the GC build's 1.66× overhead and TCO's 7%. Documented in `FINDINGS.md` "OP_CALL primitive inlining" section.
- **The diagnostic heuristic in CLAUDE.md:** a bare `<error>` from any no-GC engine (`lisp`, `cek`, `bytecode`, `prototype/*`) is almost always arena exhaustion. Reach for `bytecode_gc` or bump `MAX_CELLS` before debugging the engine.
- **Bun works as a Node replacement zero-changes.** Deno needs `node:` prefixes + explicit `process`/`Buffer` imports AND its `process.hrtime.bigint()` shim is too coarse for sub-ms bench (would need `performance.now()`). Documented in `DEV.md` "JS runtimes" section.

## Next Steps

User stated intent: **side quest to port the GC into the tree-walker (`engines/lisp.c`) and CEK (`engines/cek.c`)** — this is DEV.md "Open threads" #3. The goal is to test FINDINGS H4: "Does GC widen the bytecode VM's lead under a tight heap?" (Bytecode allocates less → collects less often.) Specific guidance:

1. **Port the mark-sweep collector from `engines/bytecode_gc.c`** to `lisp.c` and `cek.c`. Likely structure: lift the collector + free-list code wholesale, then enumerate each engine's roots — they differ:
   - Tree-walker: env (global head + per-frame), reader rp/rend pointers? probably not, those are transient. Active recursion call frames have local `u32` values that DO need to be roots (the C stack holds intermediate cons cells during evaluation). This is the trickiest part — those locals can't be seen by gc(). One approach: introduce a per-call "register frame" pushed onto a side stack the GC can scan.
   - CEK: simpler — the explicit C, E, K registers ARE the live state. Hoist them to file scope like bytecode_gc did with R_vsp/R_csp/R_env.

2. **Pre-register H4 as a hypothesis BEFORE measuring.** Predict: bytecode_gc should still win, and the gap should widen as arena shrinks (because bytecode allocates least per operation). Then test.

3. **Use the native bench for the cleanest read** — per this session's H2-decomposition finding, V8 amplifies loop-size costs. If the GC port grows the dispatch loop or eval(), wasm numbers will be confounded by JIT effects. `native_bench_lisp_gc` / `native_bench_cek_gc` would isolate the engine signal.

4. **Mind the C stack on tree-walker GC roots.** The tree-walker recurses through `eval()`, and intermediate cons cells live in C locals during that recursion. A naive GC port will collect them. Two options: (a) accept that GC only runs at top level (not mid-eval — limits the H1-style demonstration); (b) explicit shadow stack maintained by `eval()` callers. (b) is more work but matches what bytecode_gc actually does.

5. **Watch for the JIT-amplification trap.** If wasm bench shows the new GC build *regressing* on some benchmark, check native first — it may be the same H2 pattern this session uncovered three times.

6. **After the GC ports, the open thread #4 (hand-written WAT VM) and #5 (TCE cross-validation via WAT diff)** remain as the next two natural follow-ups. #5 is the smallest scope and the most "measure don't guess" appropriate — confirms or refutes that clang's `-O2` is doing TRE on the tree-walker (every CEK conclusion rests on this).

## Open Tickets

- `docs/project_incoming/feat_docker_dev_env.md` (status: open) — bundle a pinned-toolchain Docker image to avoid the macOS keg-only dance and clang-version regressions. Not in progress; can be picked up any time.
- `docs/project_incoming/wallisp-opus48-chat-transcript.md` — the imported chat transcript from the claude.ai web session that built this project. Not really a ticket (no frontmatter); kept for historical reference.

## Cross-references

- **ADR-001** (`docs/project_notes/decisions.md`) — native build as measurement substrate. The GC-port work above should follow the same pattern: build `native_bench_<engine>` first for clean engine signal, then read wasm for the JIT story.
- **DEV.md "Open threads"** is the menu of next-up work; #3 is the side quest the user picked.
- **FINDINGS.md** has all the empirical data this session produced: native A/B (lines ~150-200), inline-prims A/B (lines ~270-330).
