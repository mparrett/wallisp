# Bug Log

## 2026-07-21 — memory-safety hardening at the untrusted boundaries

Six issues surfaced by the repo audit ([audit_2026-07-21.md](audit_2026-07-21.md))
and fixed. All were reachable from pure Lisp input (a fuzzer would find them
quickly); none was the expected "arena exhaustion returns `<error>`" case. Full
test suite (parity 136/136, and every other suite) stays green after the fixes.

- **Forgeable `%string` wrapper → OOB read/write** (`bytecode_gc.c`). `is_string`
  trusted an unvalidated heap offset in the cons cdr, so `(cons '%string N)` gave
  wild `str_off`/`str_len`/`str_data` reads and a 3-byte OOB write in `mark()`.
  Fix: `is_string` now bounds-checks the offset + entry against `strheap_top`
  (overflow-safe ordering), and `mark()` guards `off <= STR_HEAP_BYTES-4`. A
  forged wrapper now reads as a non-string → string ops return `<error>`.
- **Unbounded shadow stack → GC-metadata corruption** (`lisp_gc.c`). A flat
  ~6000-arg call overran `R_save[]` into the adjacent GC mark arrays. Fix: a
  `R_ssp >= SAVE_MAX-8` guard at `eval`/`eval_list` entry sets `g_oom` and
  returns `ERR` (slop of 8 covers any single frame's ≤5 pushes). cek_gc's shadow
  stack is strictly balanced per step and needed no guard.
- **Unbounded operand stack → corruption** (`bytecode.c`, `bytecode_gc.c`,
  `standalone/wallisp.c`). `vsp`/`R_vsp` was never checked against `VSTACK_MAX`.
  Fix: a `vsp >= VSTACK_MAX-2` guard at the top of the dispatch loop (next to the
  existing `g_oom` check; branch is perfectly predicted so it's ~free).
- **Integer-literal overflow / silent fixnum wrap** (`reader.h`, all engines).
  `val*10+…` on `i32` was UB past `FIX_MAX` and silently wrapped. Fix: accumulate
  in `i64` with saturation, reject out-of-range literals as `ERR` (matches the
  arith prims' `fits_fix`). Note: `reader.h` is `#include`d before each engine's
  `FIX_MAX`/`fits_fix`, so the reader uses its own `READER_FIX_*` constants.
- **Unbounded reader recursion → stack-overflow trap** (`reader.h`, all engines).
  Deep `(((…` overflowed the C stack. Fix: a balanced depth counter
  (`READ_MAX_DEPTH 200`) on `read_expr`; deeper input returns `ERR`.
- **Cyclic-structure print hang / stack overflow** (`print_val`, all 9 engines).
  A `set-cdr!` self-cycle spun the cdr-spine loop forever; a `set-car!` cycle
  recursed to stack overflow. Fix: a bounded-printer wrapper — buffer-full check
  bounds the spine, `PRDEPTH_MAX 200` depth counter bounds the car chain (emits
  `...` past the cap). Output is now bounded on every engine.

Still open (documented, lower priority): 16-char symbol truncation collisions
(`intern`), `strheap-reset` dangling wrappers (documented caller contract), and
the standalone reader's `,` tokenization drift vs `reader.h`.
