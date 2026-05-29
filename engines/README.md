# Engines

From-scratch implementations of the same tiny Lisp, built in this order.
For the side-by-side comparison (grid, capabilities, what each one taught us)
see [`../ENGINES.md`](../ENGINES.md). For the architectural tour see
[`../DEV.md`](../DEV.md); for measurements [`../FINDINGS.md`](../FINDINGS.md).

| order | file              | architecture                          | verdict                  |
|-------|-------------------|---------------------------------------|--------------------------|
| 1     | `lisp.c`          | tree-walker (recursive eval over AST) | baseline 1.0×            |
| 2     | `cek.c`           | CEK machine (heap continuations)      | 2.2× slower              |
| 3     | `bytecode.c`      | compile-once + stack VM + TCO         | 2.3–3.9× faster — winner |
| 4     | `bytecode_gc.c`   | bytecode + mark-sweep GC              | finalist                 |
| 5     | `cek_gc.c`        | CEK + mark-sweep GC (H4 substrate)    | engine-shape study       |
| 6     | `lisp_gc.c`       | tree-walker + GC (shadow-stack roots) | TRE-preserving GC port   |
| 7     | `lisp_region.c`   | tree-walker + region-drop GC          | H2 zero floor (~0.94× wasm)|
| 8     | `lisp_trampoline.c` | tree-walker, explicit `while(TRUE)` (mal step 5) | H1 verification (~1.005× wasm) |

All four share the same reader, printer, arena, primitives, and `eval_source`
wasm ABI. Only the evaluator differs — that's what makes the A/B honest.

The [`../prototype/`](../prototype/) directory is a **separate study**: a
bytecode-line optimization ladder starting from an even simpler base than
`bytecode.c` (no TCO, no GC), with one optimization added per step. Not the
chronology of these four — read its README for the ladder structure.
