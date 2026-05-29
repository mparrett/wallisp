# Prototype — bytecode optimization ladder

Not earlier versions of the shipping engines. This is a **separate study**:
where do you implement "make primitive calls cheap" — at the VM, at the
compiler? Four builds, each adding one optimization to the previous:

| order | file           | what it adds                                              | speed vs base | correctness          |
|-------|----------------|-----------------------------------------------------------|---------------|----------------------|
| 0     | `bc_orig.c`    | simplest bytecode of all — no TCO, no GC, no icount       | (root)        | full                 |
| 1     | `bc_base.c`    | + `icount()` export for instruction-level measurement     | 1.0× baseline | full                 |
| 2     | `bc_inline.c`  | + VM checks `is_prim` at runtime, inlines `+ - * = <`     | 1.2×          | full (rebind-safe)   |
| 3     | `bc_super.c`   | + compiler emits dedicated `OP_PADD/…` superinstructions  | 1.6×          | diverges on `(define +)` |

The lesson — each level up trades generality for speed. See the "optimization
ladder" section in [`../DEV.md`](../DEV.md) for the full reasoning, and
[`../FINDINGS.md`](../FINDINGS.md) for the measurements.

The hand-WAT experiments in [`../wat/`](../wat/) start from `bc_orig.c` too
(via `wat/bc_edit.c`, which is `bc_orig` with `run()` marked `noinline` so the
dispatch becomes its own `$run` function in the WAT disassembly).
