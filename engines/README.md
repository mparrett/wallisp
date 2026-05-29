# Engines

From-scratch implementations of the same tiny Lisp, built in this order.
See [`../DEV.md`](../DEV.md) for the architectural tour and
[`../FINDINGS.md`](../FINDINGS.md) for measurements.

| order | file              | architecture                          | verdict                  |
|-------|-------------------|---------------------------------------|--------------------------|
| 1     | `lisp.c`          | tree-walker (recursive eval over AST) | baseline 1.0×            |
| 2     | `cek.c`           | CEK machine (heap continuations)      | 2.2× slower              |
| 3     | `bytecode.c`      | compile-once + stack VM + TCO         | 2.3–3.9× faster — winner |
| 4     | `bytecode_gc.c`   | bytecode + mark-sweep GC              | finalist                 |
| 5     | `cek_gc.c`        | CEK + mark-sweep GC (H4 substrate)    | engine-shape study       |
| 6     | `lisp_gc.c`       | tree-walker + GC (shadow-stack roots) | TRE-preserving GC port   |

All four share the same reader, printer, arena, primitives, and `eval_source`
wasm ABI. Only the evaluator differs — that's what makes the A/B honest.

The [`../prototype/`](../prototype/) directory is a **separate study**: a
bytecode-line optimization ladder starting from an even simpler base than
`bytecode.c` (no TCO, no GC), with one optimization added per step. Not the
chronology of these four — read its README for the ladder structure.
