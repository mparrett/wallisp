---
status: done
assigned: claude-code
created: 2026-05-29
updated: 2026-05-29
shipped: 2026-05-29
shipped_in: engines/lisp_region.c, FINDINGS.md "H2 zero floor — region-drop GC"
---
# Feature: region-drop GC engine (`lisp_region.c`) + structural cleanup ideas

## Outcome (2026-05-29)

Shipped. Pre-registered prediction (a) — `lisp_region/lisp` ≈ 1.0× — was
REFUTED in the "interesting" direction: wasm tax landed at ~0.94×,
native at ~0.99×. Region-drop is faster than the no-GC tree-walker
baseline on wasm. The H2 mechanism story is sharpened: the no-GC
baseline itself was paying a small cons-shape tax we hadn't isolated.
Predictions (b) and (c) confirmed: lisp_region eliminates the mark-sweep
tax entirely (0.66–0.83× of lisp_gc time), and clang's -O2 TRE is
preserved (countdown(1M) returns `done` in the 16M-cell big variant).

See FINDINGS.md "H2 zero floor — region-drop GC" for the full writeup
and falsification log.

---

## Summary

Add a sixth engine that replaces mark-sweep with the region-drop "GC" pattern
from Robert van Engelen's tinylisp (`gc() { sp = ord(env); }`). The intent is
*measurement*, not a new finalist — it's the zero-floor counterpoint to the
H2 substrate-amplification finding our three current GC engines established
(bytecode_gc ~1.05×, lisp_gc ~1.34×, cek_gc ~1.83× wasm tax). Region-drop
should land at ~1.0× both substrates because cons never reaches a collector
from the inner loop, so the per-callsite optimization barrier is genuinely
zero.

Reference: https://github.com/Robert-van-Engelen/tinylisp
- `src/tinylisp.c` — the 99-liner
- Article: `tinylisp.pdf` in the repo

## Why now / why not now

**Why interesting:** directly tests the H2 mechanism story by finding its
zero floor. If lisp_region's tax lands at ~1.0× wasm, that confirms the
per-cons-callsite tax is what we've been measuring all along, not some
intrinsic property of mark-sweep. If it lands higher, the story needs
revision.

**Why deferred:** H4 sequence is closed (three engines, mechanism reframed,
findings committed). The interesting next things from DEV.md "Open threads"
are #4 (hand-written WAT VM) and #5 (TCE cross-validation via WAT diff). The
region-drop port is its own self-contained study that can be picked up later.

## Key idea (how the region drop works)

In tinylisp:

- Cells are allocated on a stack growing DOWN from `N`: `cell[--sp]`.
- Atoms grow UP from 0: `hp++`. Collision aborts.
- Global env is a chain of cons cells; each `define` prepends, so the env
  HEAD has the lowest cell index.
- **The invariant**: `cons(a,b)` always returns a cell with index lower than
  min(a.index, b.index), because allocation is monotonic downward. So the
  set of cells reachable through env is *exactly* `cell[ord(env)..N]`.
- `gc()` is `sp = ord(env)`. Drops everything below the env head. O(1),
  perfectly precise, no tracing, no mark bits.

The catch: it only works at points where the only roots are the global env.
For tinylisp, that point is the REPL prompt. For our `eval_source` model
(read+eval all top-level forms before returning), the equivalent point is
*between* top-level forms.

## Sketch

1. Copy `lisp.c` → `lisp_region.c`. (Tree-walker, since that's the natural
   shape — closest to tinylisp's structure.)
2. Replace the cell arena with a downward-growing stack. Either keep a
   separate symbol table or fold atoms in like tinylisp does (collision
   check is one extra branch per cons).
3. `gc()` runs between top-level forms in `eval_source`: `sp = ord(g_env)`.
   No flag, no enable/disable dance — there's no fast/slow path in `cons`
   to begin with.
4. Wire into `build.sh` + `harness/bench.mjs` as a 7th engine slot.

## Pre-registered prediction (record BEFORE measuring)

- (a) `lisp_region / lisp_big` GC tax ≈ 1.0× on BOTH substrates. The current
      three GC engines all pay some tax even on benchmarks that fire zero
      collections (bc_gc 1.04×, lisp_gc 1.35×, cek_gc 1.87× on `ack(3,4)`
      wasm despite gc_count ∈ {0, 0, 3}) — that's the per-callsite
      optimization barrier. Region-drop's `cons` never reaches `gc()`, so
      the barrier should disappear.
- (b) Region-drop should be *faster* than lisp_gc on every benchmark (no
      per-callsite tax, no mark/sweep work).
- (c) `gc_count` semantics change: it's now "top-level form count," which
      is uninteresting. Drop the column for this engine or reuse it to
      report cells-reclaimed.
- (d) Region-drop should match lisp_big (no-GC big arena) within noise on
      the bench-suite programs, since none of them allocate enough to fill
      a 16M-cell arena. The real test of region-drop is a program that
      DOES need to reclaim — e.g., a long `(begin form1 form2 ... formN)`
      where intermediate values can't accumulate. But our suite doesn't
      exercise that shape. Consider adding a benchmark.

Falsification: (a) failing — region-drop pays a measurable wasm tax —
would refute the per-callsite-barrier mechanism and force a rethink of
the H2 reframing.

## Related ticket

The "special forms as primitives" dispatcher idea — also lifted from
tinylisp — is tracked separately as
[`refactor_special_forms_as_primitives.md`](refactor_special_forms_as_primitives.md).
The two are independent: this one is a measurement experiment, that one
is a structural refactor with no measurement claim attached.

## Acceptance for the engine port

- `lisp_region.wasm` builds clean (zero imports).
- Passes the 26-test parity suite vs `lisp.wasm`.
- `countdown(1,000,000)` returns `done` (TRE preserved).
- `native_bench_lisp_region` + `harness/bench.mjs` produce results matching
  the rest of the engines on all 5 canonical benchmarks.
- `FINDINGS.md` gets an "H4 — region-drop GC" section with measurement +
  verdict on prediction (a).

## Notes / risks

- **Closures crossing top-level form boundaries** are the subtlest case.
  In tinylisp this works because `closure(v,x,e)` stores `nil` instead of
  `e` when `e == env`, lazily re-resolving via the global env at call time.
  Without that optimization, a closure created in form N+1 might reference
  cells allocated during form N's eval that got dropped — except they don't
  get dropped if they're transitively reachable from env. Need to think
  through this carefully; the tinylisp source is the model.
- **`eval_source` does NOT do an interactive REPL.** It reads all top-level
  forms and evaluates them sequentially. Between-form `gc()` calls work but
  the user-visible benefit is small unless one form allocates a lot of
  garbage that the next form doesn't need.
- **The pattern interacts with the closure encoding.** Our current closure
  is `(s_closure params body env)` (4 conses). Region-drop's invariant
  needs every allocation site to maintain the downward-monotonic property.
  Verify `make_closure`'s cons chain still satisfies it.
