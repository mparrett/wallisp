# External inspirations

Lisp implementations we've read and what they sharpen about wallisp's findings.
Each entry: what the project is, the framing it gave us, and where (if
anywhere) it's referenced in code or tickets.

## Robert-van-Engelen/tinylisp — "Lisp in 99 lines of C"

https://github.com/Robert-van-Engelen/tinylisp

Tour-de-force code golf: NaN-boxing, atoms-grow-up-cells-grow-down shared
buffer, special forms as primitives in the same dispatch table as `+`/`car`,
and a one-line eval. The single most interesting piece for *us* is the GC:

```c
void gc() { sp = ord(env); }
```

Region-drop. After each top-level REPL form, reset the cell stack pointer
to wherever the global env head sits. Works because every `cons(a,b)` is
allocated downward, so it always lands at an index lower than `min(a, b)` —
which makes "everything reachable from env" exactly equal to "everything
between env's index and the top of the arena." O(1), perfectly precise,
no tracing.

**Why this matters for wallisp's H2 story:** tinylisp's cons is the limit
case of our H2 "optimization-barrier-per-callsite" mechanism. cons never
reaches a collector from the inner loop, so the compiler can treat it as
side-effect-free. We measured the tax of "cons can reach gc()" at 1.05× /
1.34× / 1.83× wasm in our three mark-sweep GC engines. Robert's pattern
should land at 1.0× because the barrier is genuinely absent.

**Outcome (shipped 2026-05-29):** `engines/lisp_region.c` ports the
pattern. The H2 zero-floor experiment landed at **~0.94× wasm and ~0.99×
native** vs the no-GC tree-walker baseline — *faster* than the no-GC
baseline by ~6% on wasm. The mechanism story sharpens: even `lisp.c` was
paying a small cons-shape tax we hadn't isolated (the compare-with-
MAX_CELLS shape vs region-drop's compare-with-zero). Region-drop
eliminates the mark-sweep tax AND a bit more. See FINDINGS.md "H2 zero
floor — region-drop GC" and the ticket below for details.

- See `docs/project_incoming/feat_region_drop_gc.md` (status: done)

The other lift worth considering — special forms as primitives in the same
dispatch table — is its own structural refactor:

- See `docs/project_incoming/refactor_special_forms_as_primitives.md`

## kanaka/mal — "Make a Lisp"

https://github.com/kanaka/mal
- Process guide: https://github.com/kanaka/mal/blob/master/process/guide.md

89-language polyglot Lisp implementation with an 11-step build-up
(`step0_repl` → `stepA_mal`, where stepA is self-hosting). Each step is
a runnable, self-contained increment. Clojure-flavored Lisp (hash maps,
vectors, keywords). The C impl uses GLib + Boehm GC, which is far heavier
than our hand-rolled cons cells.

Two framings we picked up from reading the guide:

1. **mal step 5 (TCO) is the explicit version of what clang's -O2 TRE does
   for us.** The guide prescribes a `while(TRUE)` loop around eval with
   `ast`/`env` rebinding on tail branches. clang's TRE rewrites our
   recursive `lisp.c` into that exact shape silently. This is a sharper
   restatement of H1 — recorded in `FINDINGS.md` under "Two surprises that
   refuted the original hypotheses."

   Worth measuring directly: does an explicit-trampoline tree-walker
   (`lisp_trampoline.c`) match the recursive `lisp.c` under -O2?

   **Outcome (shipped 2026-05-29):** YES, within noise. Native mean
   1.007×, wasm mean 1.005×, wasm modules differ by 2 bytes out of
   9840. clang's -O2 TRE is doing exactly the transformation mal step
   5 prescribes. See FINDINGS.md "H1 verification — explicit trampoline
   tree-walker" and the ticket below.
   - `docs/project_incoming/feat_lisp_trampoline.md` (status: done)

2. **The "deferrable" pattern as hypothesis-progression.** mal's guide
   marks features as deferrable until later steps make them necessary.
   That's structurally what we've been doing with H3 ("mark vs sweep
   composition") and H4 ("engine interaction") — they were deferred until
   the prerequisites existed. mal makes the deferral explicit. Not action,
   just framing worth noticing.

## What we explicitly chose NOT to take

- **mal's 11-step pedagogical structure.** Good for "learn to write a Lisp,"
  conflicts with wallisp's "measure interpreter design choices" thesis.
  Restructuring into a step progression would dilute the measurement focus
  into a feature-rollout chronicle. FINDINGS.md already plays the analogous
  role for our project (hypotheses, measurements, falsifications).
- **mal's 89-language polyglot.** Not relevant unless we want to test the
  H2 substrate-amplification finding in non-wasm hosts (we don't, currently).
- **tinylisp's NaN-boxing.** Free doubles, but we're on wasm32 with no float
  story and 32-bit tagged words are the design point.
- **tinylisp's atoms-in-the-cell-buffer.** Shaves bytes; complicates the
  arena invariants the GC engines depend on.
