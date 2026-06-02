---
status: done
assigned: codex
created: 2026-06-02
updated: 2026-06-02
---
# Codex review — 2026-06-02 follow-up (a1)

Quality assessment of the C source as research vs. product code. Outcome:
the "make a cleaned `standalone/bytecode_gc.c`" suggestion acted on at
commit a471a6d (`standalone/wallisp.c`, 635 lines from 852, EXP1 strings
and disasm mode dropped, 46/46 conformance suite passes).

---

The C source is unusually effective as research code, but I would not call it clean product code.

The strongest parts:

- **Single-file engines are the right choice for the study.** Duplicating reader/printer/primitives across engines would be bad in normal software, but here it preserves honest A/B comparisons. You can read one file and understand one implementation.
- **The hot-path code is clear enough to audit.** `bytecode_gc.c` especially has a straightforward shape: tags, arena, reader, primitives, compiler, VM, GC, exports. That order is good.
- **The comments capture the actual engineering lessons.** Many comments explain why a choice exists, not just what it does. The GC root comments and EXP notes are useful.
- **The wasm ABI is admirably small.** `input_ptr`, `eval_source`, `output_ptr`, and optionally `gc_count` are easy to embed from any host.

The weaker parts:

- **`bytecode_gc.c` has become a lab notebook.** EXP1 strings, H6 comments, disasm mode, GC experiment notes, primitive validation notes, and production VM logic all live together. That is fine for the repo’s current purpose, but it makes the file feel heavier than the actual VM needs to be.
- **There is a lot of implicit global state.** That is normal for tiny freestanding wasm, but it prevents multiple VM instances, persistent sessions with clean lifecycle control, concurrent use, and host-managed memory sizing.
- **Error handling is intentionally coarse.** Returning `ERR`/`<error>` everywhere keeps the code tiny, but debugging malformed programs or embedding this in a tool would be frustrating.
- **The parser/compiler tolerates too much silently.** This is acceptable for a tinylisp-style experiment, but it is one of the first things I would tighten for standalone use.
- **The object model is starting to strain.** Uniform cons cells are elegant; strings as wrapper conses plus side heap are the first sign that future vectors/records/byte arrays would need a real heap design.
- **Some docs/comments are now stale.** That is not C source exactly, but it matters because the source has evolved faster than the top-level contract.

My overall read: the code is good experimental systems code. It is compact, measurable, and locally understandable. The main productization step would not be “refactor everything”; it would be to make a cleaned `standalone/bytecode_gc.c` or `standalone/lisp_region.c` that removes experiment commentary, chooses one language contract, and keeps only the host ABI plus tests that define that contract.