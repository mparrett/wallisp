# Bytecode disassembly — inspecting what the compiler emits

**Two layers of "bytecode" exist in this project; this doc is about the
inner one.** The bytecode VM (`engines/bytecode_gc.c`) compiles each
input Lisp form into a flat `u32` opcode stream (`OP_CONST`, `OP_LOADG`,
`OP_TAILCALL`, ...) before running. That's the VM's own IR. The whole VM
*itself* is then compiled by clang to **wasm**, which is the bytecode V8
sees and JITs. This doc is about the inner layer — the VM's IR for a
given input program. For the wasm-side view, see `wasm_dispatch.md`.

A build flag turns the post-compile path from "run" into "dump the
stream and return," which lets us see the actual emitted code for any
source program.

## How to use

```bash
bash harness/disasm.sh                                 # builds disasm.wasm
node harness/disasm.mjs baselines/metacircular.lisp    # disasm a file
node harness/disasm.mjs -e "(+ 1 2)"                   # disasm an expression
```

Implementation is `#ifdef DISASM_ONLY` in `engines/bytecode_gc.c` — the
normal build is byte-identical (15748 bytes either way). The disasm
variant is 22 KB because the dump routine adds code and `OUTCAP` jumps
from 4 KB to 256 KB (the metacircular alone dumps ~13 KB of listing).

## What dispatched the spelunk

Question on the table: "did we ever look at what the meta program
compiles to?" We had benchmark numbers (FINDINGS.md "Metacircular
evaluator") but never seen the bytecode. Three things worth knowing
showed up immediately.

### Sizes

```
direct fib(24)            60 words
baselines/metacircular    810 words
```

13.5× more bytecode for the metacircular evaluator vs direct fib — almost
exactly tracking the source ratio (~14.8×, 3697 bytes vs ~250). The
compiler isn't adding hidden bulk; the metacircular code really is
~14× more work per outer fib step than a hand-written equivalent.

### Opcode distribution (metacircular, 810 words total)

| op | count | % |
|---|---:|---:|
| LOADG | 119 | 32% |
| CALL | 95 | 26% |
| LOADL | 72 | 19% |
| TAILCALL | 21 | 6% |
| JFALSE | 21 | 6% |
| CONST | 18 | 5% |
| RET / POP / JMP / DEFG / CLOSURE | 5 each | — |
| HALT | 1 | — |

(LOADG count includes operand words; the `=`/`car`/etc. symbol slot is
one word past the opcode itself. Either way the share is huge.)

### Two surprises

**LOADG-heavy.** A third of the metacircular bytecode is global-symbol
loads, mostly the primitive symbols mcapply tests against in its nested
`(if (= f '+) ... (if (= f '-) ...))` ladder. Every iteration of mcapply
walks that ladder, and *bytecode-count share* makes LOADG look like the
obvious optimization target.

**But:** the project has been here before. The env-lookup hotspot
hypothesis (FINDINGS.md "Two surprises that refuted the original
hypotheses", ENGINES.md `lisp.c` entry) was exactly this shape of
argument applied to the tree-walker — "lookup is walked a lot, so it
must be the bottleneck." Replacing the cons-cell assoc-list with an O(1)
array gave **~5%**. Not the bottleneck. V8 specializes the tight
LOADG-OF-CONSTANT path hard, and the dispatch loop spends its real cost
elsewhere (turn-counting, allocation, branch density).

So the honest framing of the LOADG observation here is **a falsifiable
prediction, not a recommendation**: *if* we replace mcapply's nested-`if`
primitive dispatch with a hash or a branch table, we predict <10%
speedup, not the 32% the bytecode-count share suggests. A measurement
worth doing later only if cheap; otherwise filed as another instance of
"don't trust opcode counts for runtime cost."

**Only 18% of calls are tail.** 21 TAILCALL vs 95 CALL. The eval/apply
alternation at the OUTER level IS tail (mceval ends in `(mcapply ...)`,
mcapply for closures ends in `(mceval body env)`), and those edges are
load-bearing for stack safety on deep recursion. But the *primitive
dispatch ladder* in mcapply is mostly non-tail: every `(= f 'X)` test
is an argument to the surrounding `if`, so it compiles to a `CALL`,
not a `TAILCALL`. That explains why the TCO win on meta-fib is only
~3% — TCO buys nothing for 80% of the call sites.

## Why these matter for the broader story

The metacircular benchmark's headline finding (FINDINGS.md "Metacircular
evaluator") was that workload shape changes the engine ordering. This
disasm makes the *mechanism* concrete: meta isn't slow because the
compiler does extra work, it's slow because the *runtime* repeatedly
re-walks the primitive ladder + repeatedly allocates env extensions on
every closure call (every CLOSURE call → eval-list → cons-per-arg ×
extend → cons-per-binding). Both the dispatch shape and the allocation
rate are visible in the bytecode shape; the engine-ordering shift is
just those two factors hitting each engine's specialization profile
differently.

## Snapshot

A captured copy of the full metacircular bytecode disasm lives at
`bytecode_disasm.meta.txt` next to this file. Re-generate with the
commands above if the metacircular source changes.
