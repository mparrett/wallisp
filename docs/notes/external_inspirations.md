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

- See `docs/notes/feat_region_drop_gc.md` (status: done)

The other lift worth considering — special forms as primitives in the same
dispatch table — is its own structural refactor:

- See `docs/notes/refactor_special_forms_as_primitives.md`

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
   - `docs/notes/feat_lisp_trampoline.md` (status: done)

2. **The "deferrable" pattern as hypothesis-progression.** mal's guide
   marks features as deferrable until later steps make them necessary.
   That's structurally what we've been doing with H3 ("mark vs sweep
   composition") and H4 ("engine interaction") — they were deferred until
   the prerequisites existed. mal makes the deferral explicit. Not action,
   just framing worth noticing.

## William E. Byrd — "The Most Beautiful Program Ever Written"

Talk digest filed 2026-05-29 (originally `lisp-in-lisp.md` in the inbox).
Byrd's framing: a tiny metacircular Lisp interpreter (eval/apply in Lisp,
environments-as-procedures) is "Maxwell's equations of software" — small
enough to fit on a 4×6 card, yet exposing the deep structure of computation
(scope, closures, recursion, evaluation strategy, continuations,
synthesis).

The talk lives at the *semantics* level (what the interpreter denotes,
how transformations like CPS / store / lazy eval / dynamic scope let you
explore language design from the same minimal core). wallisp lives at the
*implementation* level (what the interpreter costs once it hits clang,
wasm, V8, and a fixed cell arena). Different axes.

**One framing worth lifting — the metacircular evaluator as a benchmark.**
Writing a tiny Lisp-in-Lisp and running it on each of our eight engines
gives a new measurement: the substrate cost of *interpretation on top of
interpretation*. It tests whether `bytecode_gc` is fast enough to host
its own evaluator at a non-embarrassing speed, and whether the engine
ordering on direct fib mirrors the ordering on metacircular fib. The
constant-vs-varying-tax question is the empirical hook.

- See `docs/notes/feat_metacircular_eval.md`

What we are *not* lifting from the talk:

- **Environment-as-procedure representation.** Beautiful at the semantics
  level; not portable to our wasm implementation level (env in wallisp is
  a cons-chain reached by lexical address, not a first-class function).
- **miniKanren / relational interpretation.** Genuinely interesting but
  a different project — relational evaluation needs unification and a
  search engine on top of the language, which is bigger than wallisp.
- **Barliman / program synthesis.** Same — downstream of miniKanren.
- **CPS-transform the engines.** CEK already lives in that neighborhood
  semantically (continuations as data); duplicating the framing wouldn't
  add a measurement axis we don't have.

## acdw — "A Month with Clojure"

https://www.acdw.net/clojure/

A one-month-with-Clojure impression piece, framed as a Common Lisp /
Scheme / Clojure three-way:

- **Common Lisp**: incoherent (`mapcar` not `map`; three equalities
  `eq` / `eql` / `equal`); committee-designed legacy.
- **Scheme**: beautifully minimal ("fits within 50 pages"), but
  deliberately excludes pragmatic stuff like hash-maps and error
  handling from core.
- **Clojure**: pragmatic middle — `seq` abstraction so `map` works on
  any collection, single `=`, batteries-included stdlib, JVM ecosystem.
  Bracket-overloading (`[]`, `{}`, `#{}`) and `~` are listed as the
  syntactic cost.

**Relevance to wallisp.** The Scheme-pure-vs-Clojure-pragmatic axis is
exactly the tension between our study repo (small, consistent, eight
engines doing the *same* small thing — Scheme-pure on purpose) and any
future `wallisp-core` product story. The codex 2026-06-02 review's
"Moderately Closable" bucket — first-class vectors/arrays, a real
standard library, module/load system — is the Clojure-pragmatic move
in disguise. If `standalone/wallisp.c` ever picks up a real audience,
this is the question that gets reopened.

Not lifting any specific implementation idea — the article is about the
*experience* of using a language, not how to build one. Filed as a
pointer for the "research artifact or product?" decision, not as a
roadmap input.

## IOCCC 2025/cable — "a virtual machine in 366 bytes of C"

https://www.ioccc.org/2025/cable/index.html

Filed 2026-06-09 from `docs/notes/virtual-machine-366-bytes-c.md`
(exploratory "any overlap with us?" task). The entry is a **SUBLEQ** machine
— a one-instruction-set computer (OISC) whose single instruction is
`m[b] -= m[a]; if (m[b] <= 0) goto c;` with low-bit-tagged operands selecting
indirect addressing. 32-bit, 1.5 GB RAM, SDL3-backed framebuffer, and it boots
Linux and runs DOOM. The 366 bytes is *only the interpreter*; the OS + DOOM
arrive as a precompiled `vmlinux.bootimage` blob of SUBLEQ words produced by a
custom LLVM backend + softfloat + a Linux port. The napkin-sized C is real; the
ecosystem behind the blob is enormous.

**The framing it sharpens — interpreter size is not where the cost lives.**
This is the same lesson our project keeps re-learning from the other vantage
(speed), now stated for size. wallisp's tell is the inverse of cable's: our
*source* is small-ish (438–929 LOC per engine) but the **work is in the
engine**, whereas cable's source is microscopic and the work is **pushed into
the producer** (the LLVM→SUBLEQ toolchain + the bootimage). Both are the
"complexity is conserved, you only choose *where* it sits" observation that
already runs through DEV.md's compiler↔VM decoupling seam — cable just takes it
to the asymptote: a VM so small the entire semantics of the target programs
live in the compiled data, not the interpreter.

**The one concrete tie — SUBLEQ is the limit case of our bytecode ISA story,
from the opposite end that tinylisp's `gc()` anchors.** Our bytecode line moves
*up* the ISA (superinstructions: `OP_PADD…` fold prim dispatch into dedicated
opcodes, measured 1.6× faster / 18% fewer instructions — DEV.md). SUBLEQ is the
floor: one opcode, zero dispatch, semantics shoved entirely into the operand
stream. The interesting wallisp-shaped question it *poses* (not one we should
chase) is the dispatch-cost extreme — a single-instruction VM has no `br_table`
to specialize, so V8's whole "each arm optimizes independently" finding
(`wasm_dispatch.md`) has nothing to bite on; the loop is pure data-driven
arithmetic. That's a different measurement axis from anything in FINDINGS, and
filed only as a note, not a hypothesis.

**Why it is NOT an engine inspiration for us (and we should not port it):**
- **Wrong altitude.** wallisp measures *interpreter design choices for a Lisp*
  (env representation, GC barrier, tail-call shape). A SUBLEQ core erases all of
  those — there's no env, no cons, no GC, nothing the project's eight-engine
  matrix exists to compare. Porting it would be a different project.
- **The size win is relocation, not reduction.** 366 bytes only looks small
  because the toolchain + bootimage are off-screen. Our "small" is honest LOC
  you can read end to end; adopting cable's accounting would be the opposite of
  the project's measure-don't-hide ethos.
- **No wasm/V8 story.** The IOCCC point is byte-golf of C source under a size
  cap; ours is *runtime* behavior under clang→wasm→V8. Different arbiter
  entirely (the CLAUDE.md "V8's JIT is the real arbiter, not source size"
  trap names exactly this confusion).

Net: a sharp **framing** entry (complexity-relocation; the OISC floor of the ISA
spectrum our bytecode engines sit on), not a roadmap or engine input. No ticket.

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
- **IOCCC/cable's SUBLEQ core.** Wrong altitude — a one-instruction machine
  erases the env/cons/GC/tail-call axes the eight-engine matrix exists to
  measure; its 366-byte source relocates complexity into an off-screen
  toolchain rather than reducing it. Kept as a framing note, not an engine.
