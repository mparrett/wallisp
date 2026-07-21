# Tiny Lisp → WebAssembly — developer guide

One small Lisp, implemented three ways (tree-walker, CEK machine, bytecode VM),
compiled to **freestanding wasm32** (no libc, **zero imports**), then driven by
measurement to a finalist: a bytecode VM with tail-call optimization and a
hand-rolled **mark-sweep garbage collector**.

For build/run, see [README.md](README.md). For the empirical record, see
[FINDINGS.md](FINDINGS.md). For agent-specific instructions, see
[CLAUDE.md](CLAUDE.md).

## The language

- **32-bit tagged values**, low 2 bits = tag: `00` fixnum (30-bit signed,
  **wraps at ~536M**), `01` cons (index into the cell arena), `10` symbol
  (interned, stable), `11` special (nil / true / error / primitives).
- **Special forms:** `quote if define lambda let begin cond`. `define` also
  accepts the function shorthand `(define (f a b) body)`; `cond` recognises
  `else` as the catch-all clause head.
- **Primitives (shared core):** `cons car cdr + - * / mod = < null? pair?
  list? number? symbol?`. Fixnum-only — no floats. Division-by-zero and
  arithmetic overflow are errors. PR1 brought this to all eight engines
  so the shared semantic floor is real, not "tinylisp/mal minimal-
  validation". (`number?` / `symbol?` shipped 2026-06-03 to unblock the
  metacircular eval.)
- **`bytecode_gc` extensions:** strings (`string?` `string-length` `string-ref`
  `string=?` `string-append`) and mutation (`set-car!` `set-cdr!`). Other
  engines leave these unbound; `harness/parity.mjs` gates the relevant
  programs to engines that support them.
- **Arity and types** are validated on both primitives and user lambdas.
  `(+ 1)`, `(+ 'a 1)`, and `((lambda (x) x) 1 2)` all return `<error>`.
  Some malformed programs still surface as bare `<error>` rather than a
  differentiated message.
- **Reader:** recursive descent, `'` quote shorthand, tolerant of a missing `)`.

## The engines — `engines/`

The side-by-side comparison lives in [`ENGINES.md`](ENGINES.md): grid of
what was built vs not, engines-at-a-glance table (lines/arena/fib(24) on
both substrates), capabilities by architecture, and one paragraph per
engine on what it taught us. Read that first.

The short version: three architectures (`lisp` tree-walker, `cek` machine,
`bytecode` VM), three GC strategies (none, mark-sweep, region-drop), eight
engines total (two grid cells empty by design — see ENGINES.md). Bytecode
is 2.3-3.9× faster than tree-walker by attacking the axis the substrate JIT
doesn't already cover; CEK pays a 2.2× speed penalty for a capability
(deep non-tail recursion) it exclusively owns.

Bytecode's biggest win (3.9×) is on allocation-heavy list reversal, smallest
(2.3×) on call-bound `tak`. Full numbers in `FINDINGS.md`.

## The bytecode ISA — the decoupling seam

Opcodes: `CONST LOADL LOADG DEFG POP JMP JFALSE CLOSURE CALL RET HALT`
(+ `PADD PSUB PMUL PEQ PLT` in the superinstruction build). The compiler→VM split
is a real producer/consumer seam: a **flat u32 array is the entire interface**
(plus the arena for any quoted constants). This is *why* the bytecode VM — alone
among the three — decouples cleanly. The tree-walker and CEK consume the AST
cons-graph + symbol table, i.e. "share the whole heap," so they have no thin seam.

## Garbage collector — `engines/bytecode_gc.c`

Non-moving **mark-sweep**, chosen over a copying collector because every heap
object is a uniform cons cell (so no fragmentation) and indices never move (roots
are only *marked*, never rewritten — a moving collector would have to find and
rewrite every scattered index). Hybrid allocator: bump until the arena fills, then
free-list + collect. Roots: operand stack, env, saved frames, globals, and the
cons-tagged `OP_CONST` constants embedded in the bytecode. Pre-registered, tested:

- **H1 (unbounded): confirmed.** `countdown(10,000,000)` completes in a **512-cell**
  arena (GC ran 118,577×). TCO flattens the stack, GC flattens the heap.
- **H5 (root set complete): confirmed.** `fib(18)` correct while GC fired 120×
  mid-evaluation in 512 cells; suite 19/19; quoted data survives collection.
- **H2 (overhead): ~1.3–1.7×, and it is NOT collection.** Isolation showed
  collection itself is nearly free; the cost is an **optimization barrier** — once
  `cons` can reach `gc()`, the compiler can't treat allocation as side-effect-free.

## The optimization ladder — `prototype/` (most recent thread)

From `bc_orig.c` (simplest: **no TCO, no GC**), we explored *where* to implement
"make primitive calls cheap," at three levels:

| build | what | speed vs base | instrs | allocation | correctness |
|---|---|---|---|---|---|
| `bc_base.c` | `OP_CALL`→`apply_prim`, args consed (+ instruction counter) | 1.0× | — | base | full |
| `bc_inline.c` | VM checks `is_prim` at runtime, inlines `+ - * = <` (no arg consing) | 1.2× | **identical** | 3× arena headroom; `tak` error→`7` | full (rebind-safe) |
| `bc_super.c` | **compiler** emits dedicated `OP_PADD…` opcodes (superinstructions) | **1.6×** | **18% fewer** | same as inline | diverges on global prim *redefinition* |

**Lesson:** each level up trades generality for speed. Runtime-inline checks the
live value (correct even if `+` is redefined) but pays a per-call check.
Compile-time superinstructions are fastest and finally cut the instruction count,
but bake in "primitives aren't rebound" (`(define +)` then `(+ 1 2)` gives 99 on
base/inline, 3 on super). Real Schemes gate exactly this behind
`usual-integrations`-style declarations. The instruction counter that made this
measurable was added by **hand-editing the WAT** (see `wat/`).

## Hand-editable WAT — `wat/`

Workflow: `clang → wasm2wat → hand-edit → wat2wasm → run` (round-trip faithful).

- `probe.wat` — a hand-written 4-opcode stack VM with **inline allocation** (proof
  the hot core is tractable in raw WAT; the inline `cons` has zero function calls).
- `bc_edit.c` — `bc_orig` with `run()` marked `noinline` so the dispatch is its own
  `$run` function.
- `bc_edit.wat` — clang's disassembly of `bc_edit` (the editable substrate).
- `bc_instr.wat` — `bc_edit.wat` hand-augmented with an instruction counter.

**Finding:** hand-WAT is great for **additive/observational** edits (instrumentation
dropped cleanly into `$run`). For **logic** edits that touch the data flow you must
reverse-engineer clang's frame layout (at `-O0`: frame pointer = `local 3`, `vsp`
at frame offset 76, `n`@28, `fn`@24) — fragile, one wrong offset is silent
corruption — so do those in C and recompile. Separately: the GC's measured ~1.6×
overhead lives at the **V8 JIT** layer, not in clang's wasm (the slower build's
wasm is actually *leaner*; V8 optimizes a call-containing hot loop less). Tools:
`npm i -g wabt` provides `wasm2wat`/`wat2wasm`.

## Build & run details

### macOS setup

`build.sh` needs Homebrew's `clang` (with wasm32 target) and `wasm-ld`. Apple's
`/usr/bin/clang` does **not** ship the wasm backend, and `wasm-ld` lives in the
separate `lld` formula — not bundled with `llvm`.

```bash
brew install llvm lld
```

Both are **keg-only** on macOS (Apple's clang takes precedence), so prepend them
to your PATH for the build:

```bash
export PATH="$(brew --prefix llvm)/bin:$(brew --prefix lld)/bin:$PATH"
bash build.sh
```

If you only want to *run* (not rebuild), the prebuilt `*.wasm` files are checked
in at the repo root — `node harness/test_bc.mjs` and `node harness/lisp-cli.mjs`
work as-is with just `node`. `harness/bench.mjs` is the exception: it loads the
`*_big.wasm` variants, which aren't prebuilt and require `build.sh`.

### Compiler flags

All engines: `--target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export-dynamic
-Wl,--allow-undefined -Wl,--initial-memory=33554432`. Extra per engine:
- `cek.c` needs **`-mtail-call`** (uses `__attribute__((musttail))`).
- `bytecode_gc.c` needs **`-fno-builtin`** (defines its own `memset`/`memcpy`).
- Arenas are tunable via `#define MAX_CELLS`. `build.sh` also emits big-arena
  `*_big.wasm` for `harness/bench.mjs`, since the no-GC engines exhaust a small
  arena on heavy benchmarks.
- Default is `-O2`. Swapping to `-Oz` roughly halves every wasm artifact (47%–61%)
  and is *equal or faster* on V8 for every engine except CEK, where it tanks
  perf 1.9×–2.6× (V8's musttail specialization needs the inlined dispatch arms
  `-O2` produces). Default kept at `-O2` so no engine regresses; see
  FINDINGS.md "Build flag: -O2 vs -Oz" for the full table.

**wasm ABI** (modules have zero imports; they're *libraries*, not commands — no
`_start`/WASI, so a standalone runtime's `run` subcommand won't work; embed via the
host API): write source bytes at `input_ptr()`, call `eval_source(len)` → returns
output length, read the result string at `output_ptr()`. `bytecode_gc.wasm` also
exports `gc_count()`; the `bc_base/inline/super` builds export `icount()`
(instructions dispatched). `harness/lisp-cli.mjs` is a minimal driver.

`bytecode_gc.wasm` additionally exports a **persistent-session** pair
(Milestone A — see `docs/notes/terminal_game_roadmap.md`):
`reset_session()` starts/clears a session and `eval_persistent(len)` evaluates
while keeping globals, symbols, the cons arena, and the string heap across
calls, so `(define x 5)` then `(+ x 1)` works across two calls. `eval_source`
is unchanged (one-shot, re-inits every call); the two modes must not be mixed
on one instance. Driver: `harness/repl.mjs`; contract test: `test_session.mjs`.

For terminal rendering (Milestone B render slice) `bytecode_gc.wasm` also
exports `(display s)` — a primitive that writes a string's bytes straight to
`outbuf` (no quotes/escaping, unlike the value printer), so a program can draw a
raw text frame; the value echo is suppressed when a program rendered this way.
`strheap_used()` reports string-heap bytes. The heap is bump-only, so transient
per-frame strings are reclaimed via an explicit O(1) region-drop —
`(strheap-mark)` captures the top, `(strheap-reset m)` drops back to it (the
render loop brackets each frame; ADR-004). Demo + before/after leak measurement:
`harness/render_probe.mjs`.

For unbounded real-time play (ADR-005) `bytecode_gc.wasm` exports
`input_slots_ptr()` (host pokes ints, read by `(input i)`), `last_entry()` (the
bytecode entry of the most recent eval), and `rerun(entry)` (re-execute compiled
bytecode without touching `cp`). The driver compiles `(tick)` once and `rerun`s
it per frame, so `code[]` stops growing — verified at 1e6 ticks. Driver:
`harness/game.mjs`; game: `examples/coin2d.lisp`.

### Bytecode disassembly + wasm inspection

Two views into "what is the VM actually executing," answering different
questions:

- **VM bytecode (the inner IR).** `bash harness/disasm.sh` builds a
  `disasm.wasm` variant of `bytecode_gc` that dumps the compiled `u32`
  opcode stream instead of running it. `node harness/disasm.mjs <file>`
  prints the listing (addresses + opcode names + decoded operands).
  Build flag: `-DDISASM_ONLY -DOUTCAP=262144` in
  `engines/bytecode_gc.c` — normal builds are byte-identical.
- **Engine wasm (what V8 sees).** `brew install wabt` then `wasm2wat
  bytecode_gc.wasm`. The big run-loop `switch` compiles to a single
  `br_table` over 12 opcodes; each arm specializes independently in V8.

See `docs/notes/bytecode_disasm.md` and `wasm_dispatch.md` for
the writeup of what we found inspecting the metacircular eval. Notable:
the bytecode-count share of `LOADG` looks alarmingly dominant (32%),
but the wasm view shows V8 already specializes those arms tight — the
env-lookup falsification (FINDINGS.md "Two surprises") generalizes.

### Native build

`bash build.sh --native` produces `native_bench_<engine>` and
`native_cli_<engine>` binaries — same engine source, included as a single TU
via `-DENGINE_SRC=...`, with the engine's own `memset`/`memcpy` macro-renamed
so they don't collide with libc. Uses Apple/system clang, no Homebrew toolchain
needed. The bench binaries answer "how much of the cost is the wasm/V8 layer
vs the engine itself" — see FINDINGS.md "Native build" section. Headline:
wasm-on-V8 costs only ~1.1x–1.6x over native, smaller than the engine-vs-engine
gaps; GC overhead has a native floor (~1.3x) that V8 amplifies.

### JS runtimes

Verified: **Node** (V8) and **Bun** (JavaScriptCore) — both run the harnesses
unchanged, and their `bench.mjs` ratios agree within ~5% across two independent
JIT engines (real cross-validation of "trust ratios, not magnitudes").

**Deno** needs porting and is currently a poor fit for `bench.mjs` regardless:
the bare-global ergonomics differ (Node-style `'fs'`/`'path'`/`'url'` need
`node:` prefixes; `process`/`Buffer` need explicit imports), and — the
blocker — Deno's `process.hrtime.bigint()` compat shim is too coarse to time
sub-ms benchmarks, producing `0ms` entries and `NaNx`/`Infinityx` ratios. A
real Deno port would need `performance.now()` in the bench path.

## Open threads / next steps

1. **Superinstructions into the GC build** — same edit on `bytecode_gc.c`. The
   new `OP_PADD…` arms are pure stack arithmetic and **don't allocate**, so they
   need *no* GC-root changes. The benefit appears as *fewer GC cycles* (less
   consing) rather than arena headroom; measure with the `gc_count()` export.
2. **Redefinition guard** so superinstructions are fast *and* correct under
   `(define +)`. Worth folding into the same pass as #1 — lands fast + correct
   in one shot instead of shipping the silent divergence.
3. **All-engine GC** — DONE. Both `cek_gc.c` and `lisp_gc.c` shipped. Three GC
   engines now exist; together they establish a clean ordering of V8's
   amplification of the H2 optimization barrier (bytecode_gc ~1.0×, lisp_gc
   ~1.1×, cek_gc ~1.4×) that maps to each engine's JIT-specializability. See
   FINDINGS.md sections "H4 — GC ported into CEK" and "H4 — GC ported into
   the tree-walker" for the measurement and falsification log.
4. **Hand-written-VM north star** — a clean hand-written WAT interpreter over our
   existing ISA, reusing the clang front-end as the bytecode *producer* (~400 lines;
   the tractable, instructive slice — skip hand-writing the reader/printer/compiler).
5. **TCE cross-validation via WAT diff** — the one place wasm is authoritative
   (a structural question, not a timing one): confirm clang turned the tree-walker's
   self-tail-call into a loop, then perturb it out of tail position and watch it
   become a real recursive `call`.

## File map

```
build.sh                 build all engines -> *.wasm (pass --native for native bins)
FINDINGS.md              full empirical record (engine benchmarks + GC hypotheses)
*.wasm                   prebuilt engines (run immediately without clang)
engines/                 the engines (see engines/README.md)
  lisp.c lisp_trampoline.c lisp_gc.c lisp_region.c cek.c cek_gc.c bytecode.c bytecode_gc.c
prototype/               bytecode optimization ladder (see prototype/README.md)
  bc_orig.c bc_base.c bc_inline.c bc_super.c
wat/                     hand-editable WAT experiments
  probe.wat bc_edit.c bc_edit.wat bc_instr.wat
harness/                 node drivers (also runs under Bun)
  lisp-cli.mjs           one-shot eval over any engine (re-inits each call)
  test_bc.mjs bench.mjs  bytecode tests; cross-engine benchmark (loads *_big.wasm)
  repl.mjs               persistent-session REPL over eval_persistent
  test_session.mjs       persistent-session contract test (state survives, cp-append)
  render_probe.mjs       render slice + per-frame strheap-leak measurement
  game.mjs               TUI game-loop driver (terminal; xterm.js flavour is web/game.html)
native/                  native build (no wasm, no JIT) — see "Native build" above
  bench.c main.c
web/                     self-contained browser showcase
  tiny-lisp-vm.html build-standalone.sh
  game.html              TUI game in xterm.js — generated by build-game.sh from
                         game-template.html (inlines bytecode_gc.wasm + coin2d.lisp)
docs/                    project memory (notes, incoming tickets, archived)
```
