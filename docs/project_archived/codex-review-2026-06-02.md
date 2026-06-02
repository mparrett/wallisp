---
status: done
assigned: codex
created: 2026-06-02
updated: 2026-06-02
---
# Codex review — 2026-06-02

Outcome: stale doc claims fixed in README.md / DEV.md (commit 3ad6cda),
orphaned `web/build-standalone.sh` repointed at `engines/bytecode_gc.c`
(same commit), and the "two artifacts" recommendation acted on as a
directional extraction at `standalone/` (commit a471a6d). PR3 (32-bit
fixnums) still deferred. Duplicated limitations section removed during
archival.

---

**Findings**

1. Documentation is stale in a few high-visibility places. [README.md](/Users/matt/projects-new/wallisp/README.md:29) still says primitive arity/type validation is absent, but the current suites include PR1 validation and pass. [DEV.md](/Users/matt/projects-new/wallisp/DEV.md:20) says no strings/division/modulo, while `bytecode_gc` now has strings plus `/` and `mod`. This matters if the repo is used as the basis for a standalone release: the public contract is currently less accurate than the implementation.

2. The web standalone build path looks abandoned. [web/build-standalone.sh](/Users/matt/projects-new/wallisp/web/build-standalone.sh:13) expects `web/lisp.c` and `web/template.html`, but `web/` only contains `build-standalone.sh` and `tiny-lisp-vm.html`. The checked-in HTML may be fine as an artifact, but the reproducible standalone builder is not aligned with the current repo.

3. The strongest extraction seam is real: the bytecode compiler/VM boundary is a flat `u32` bytecode array, and the wasm ABI is only `input_ptr`, `eval_source`, `output_ptr`, plus `gc_count` for the GC build. That is documented in [DEV.md](/Users/matt/projects-new/wallisp/DEV.md:46) and implemented in [engines/bytecode_gc.c](/Users/matt/projects-new/wallisp/engines/bytecode_gc.c:789). This is the subset to productize.

4. `bytecode_gc.c` is both the finalist and an experiment host. The EXP1 string heap is validated, but intentionally leaks string heap entries until `eval_source()` resets state; the source calls this out at [engines/bytecode_gc.c](/Users/matt/projects-new/wallisp/engines/bytecode_gc.c:565). For a standalone version, either omit strings from the core release or finish string reclamation and make strings part of the contract.

**Most Impactful Findings**

The major project result is not “a tiny Lisp in wasm”; it is the measured ranking of implementation strategies. Bytecode is the load-bearing lever: [ENGINES.md](/Users/matt/projects-new/wallisp/ENGINES.md:121) records it as 2.3-3.9x faster than the tree-walker, because it removes repeated AST walking, special-form dispatch, and consed arg lists from the hot path.

The CEK result is the sharpest negative finding. It earns capability, not speed: deep non-tail recursion and `call/cc`, but it is slower and allocates heavily. [ENGINES.md](/Users/matt/projects-new/wallisp/ENGINES.md:103) captures the important version: even `call/cc` did not rescue CEK as a performance story.

The GC finding is the most reusable systems lesson. Collection work itself was not the main cost; the cost was the optimizer barrier once `cons()` can reach `gc()`. The three GC engines plus region-drop make that robust, not anecdotal. The region-drop result at [ENGINES.md](/Users/matt/projects-new/wallisp/ENGINES.md:96) is especially valuable because it isolates the zero-tax floor.

The final practical winner is `bytecode_gc`: [ENGINES.md](/Users/matt/projects-new/wallisp/ENGINES.md:127) calls it the shippable engine, and I agree. It has TCO, real reclamation for cons cells, validated primitives, inline primitive fast paths that preserve redefinition semantics, and the cleanest host boundary.

**Standalone Strategy**

I would extract two artifacts, not the whole research repo:

1. `wallisp-core`: `engines/bytecode_gc.c`, a minimal build script, `harness/lisp-cli.mjs`, and a focused conformance suite. Keep the ABI exactly as-is. Build only `bytecode_gc.wasm`, assert zero imports, and default to `-Oz` for the shipped VM unless a fresh benchmark says otherwise.

2. `wallisp-study`: the current multi-engine repo, docs, benchmarks, prototypes, WAT experiments, and falsification log. This remains the evidence base and development lab.

For the standalone core, I would initially ship the Lisp subset without CEK, tree-walker, prototype engines, native baselines, WAT experiments, and archived handoffs. I would decide strings explicitly: either “core has no strings yet” and remove EXP1 from the product VM, or “strings are supported” and finish strheap reclamation first.

Verification I ran: `node harness/test_bc.mjs` passed 70/70, `node harness/parity.mjs` passed 117/117 across all 8 engines, `node harness/parity_strings.mjs` passed 44/44, and `node harness/parity_callcc.mjs` passed 16/16.

---
Yes. If code surface area is the main constraint, I’d change the recommendation.

For a smallest useful standalone, I’d pick `engines/lisp_region.c` or `engines/lisp.c`, not `bytecode_gc.c`.

Current line counts:

- `lisp.c`: 491 lines
- `lisp_region.c`: 515 lines
- `bytecode.c`: 486 lines
- `bytecode_gc.c`: 852 lines
- `lisp_gc.c`: 624 lines
- `cek.c`: 598 lines
- `cek_gc.c`: 693 lines

The nuance is that `bytecode.c` is slightly smaller than `lisp_region.c`, but it lacks GC, so long sessions hit arena limits. For a tiny standalone that still has a plausible “runs repeatedly in a REPL” story, `lisp_region.c` is the best surface-area pick: about 515 lines, simple tree-walker, region-drop reclamation, no bytecode compiler/VM split, no root scanner, no call stack arrays, no string heap.

My revised ranking under “smallest useful subset” would be:

1. **Smallest honest artifact:** `lisp.c` + CLI harness. Best for teaching and readability. No GC.
2. **Smallest practical artifact:** `lisp_region.c` + CLI harness. Still compact, has top-level reclamation, keeps the implementation model simple.
3. **Smallest performant artifact:** `bytecode.c`. Similar line count, faster, but no GC.
4. **Best overall artifact:** `bytecode_gc.c`. Still my performance/features recommendation, but not the surface-area recommendation anymore.

So the extraction changes from “ship the finalist VM” to “ship the smallest tree-walker, and keep `bytecode_gc` as the research-backed high-performance sibling.”

---

The glaring limitations split into three buckets: language surface, runtime limits, and “standalone/product” rough edges.

**Easily Closable**
- **Missing basic primitives:** `and`, `or`, `not`, `>=`, `<=`, `>`, `abs`, maybe `display`/`print`. These are straightforward primitive additions or desugarings.
- **More list helpers:** `append`, `map`, `filter`, `fold`, `length`, `reverse`, `assoc`. Most can be shipped as a small standard prelude in Lisp rather than C.
- **Better syntax sugar:** multi-expression lambda bodies, `let*`, `letrec`, shorthand `else` improvements, maybe `quasiquote` later. `let*` is easy desugaring; `letrec` is trickier but doable now that mutation exists.
- **Error consistency:** the VM already returns `<error>`, but errors are undifferentiated. Adding a few internal error codes or clearer printed errors is not hard, though it increases surface.
- **Input/output buffer limits:** `INCAP=8192`, `OUTCAP=4096`, `CODE_MAX=65536` are fixed compile-time caps. Raising or making them host-configurable is easy; making them dynamically grow inside freestanding wasm is less so.
- **String utility gaps in `bytecode_gc`:** `substring`, `string->list`, `list->string`, `number->string`, `symbol->string`. These are not conceptually hard, though they add C code.

**Moderately Closable**
- **Strings are only in `bytecode_gc`:** other engines don’t support them. If the standalone target is `bytecode_gc`, that’s fine. If cross-engine parity matters, porting strings everywhere is busywork.
- **String heap reclamation:** current strings leak until `eval_source()` resets. Adding a variable-size free list or compacting string heap is manageable but no longer “tiny.”
- **A real standard library:** useful, but it needs decisions about load model, prelude injection, namespacing, and tests.
- **Module/load system:** needed for non-toy use. Not hard in the host, but it changes the language experience.
- **Better numeric tower:** 30-bit fixnums are limiting. Moving to 32-bit immediates is impossible with the current 2-bit tag scheme; going to boxed bignums or i64-backed wasm values is more invasive.
- **First-class vectors/arrays:** useful and straightforward conceptually, but the current heap is cons-cell-shaped. Adding non-cons heap object types means expanding GC/object layout.

**Less Than Trivial**
- **A robust object model / heterogeneous heap:** cons cells are the whole runtime model. Strings already showed the extra complexity. Vectors, byte arrays, records, closures with better layouts, symbols with long names, etc. push toward a real object heap.
- **Proper diagnostics and source locations:** the reader/compiler currently throws away location info. Real errors require carrying spans through parsing and compilation.
- **Hygienic macros:** powerful, but a large semantic addition.
- **Continuations in bytecode:** CEK has `call/cc`; bytecode does not. Adding real continuations to the bytecode VM means capturing/restoring operand stack, call stack, env, and ip safely under GC.
- **Deep non-tail recursion in bytecode:** currently bounded by `CALL_MAX`. Fixing this properly means heap-allocating frames or growing call-stack storage, both changing the VM model.
- **Incremental/session state:** `eval_source()` calls `init()`, so each evaluation resets the world. A real REPL with persistent definitions needs an explicit VM instance lifecycle and host API changes.
- **Production-grade memory management:** mark-sweep is fine for the experiment, but real workloads want dynamic memory sizing, string reclamation, maybe generational behavior, and better OOM behavior.

If the goal is a tiny standalone, I’d close only: clearer docs, persistent REPL state if needed, a small prelude, basic missing predicates/comparators, and either remove strings or finish string reclamation. Everything else risks turning the “tiny” part into historical branding.

