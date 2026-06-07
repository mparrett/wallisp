# Shared reader extraction — plan

**Drafted 2026-06-07.** Status: planned, not yet started.

The reader (`skipws` / `is_delim` / `read_atom` / `read_list` / `read_expr`)
currently lives inline in every engine source file. An audit (recorded
below) shows 11 of those copies are semantically identical to
`engines/lisp.c`'s reader, with only cosmetic divergence. Extracting them
into a shared `engines/reader.h` lets future reader-shape changes (the
parked f-call sugar branch on `reader-sugar-fn-calls`, a potential
offside-rule sugar, anything else) be one-file edits instead of
fan-outs of 11.

## Audit summary (verified 2026-06-07)

Three real reader variants exist across 15 source files:

**Family A — canonical (11 engines, all freestanding wasm32):**
- `engines/lisp.c`, `lisp_trampoline.c`, `lisp_region.c`, `lisp_gc.c`,
  `cek.c`, `cek_gc.c`, `bytecode.c`
- `prototype/bc_base.c`, `bc_inline.c`, `bc_orig.c`, `bc_super.c`

All semantically identical to `engines/lisp.c`'s reader. Diffs are 100%
cosmetic: whitespace density, comment density, a dead `(void)tailp;`
vestige in `lisp.c`, slightly different list-build patterns. Zero
behavioral divergence.

**Family B — bytecode_gc (1 engine):**
- `engines/bytecode_gc.c`

Family A + a `read_string()` function for `"..."` literals + the `"`
case in `read_expr`'s dispatch + reader-side OOM signaling via `g_oom`
and GC rooting for in-flight allocations via `g_pending_str_off`. The
EXP1 string-heap experiment.

**Family C — Futamura dev tools (2 binaries, native libc, OUT OF SCOPE):**
- `prototype/futamura/specialize.c`, `preproc.c`

Family A + `$` in `is_delim` + a `$` case in `read_expr` that wraps as
`(__comptime__ ...)` for the H10 source-to-source PE. ALSO: uses
`fprintf(stderr)`/`exit(1)` instead of returning `ERR`, and `intern_cstr`
instead of `intern`. Fundamentally different ecosystem; included here
only so future readers know they were deliberately excluded.

## Conventions (agreed up front)

1. **File location & name**: `engines/reader.h`. Conventional `.h`
   even though it contains static-function bodies — that's a long-standing
   C-header use. `#include`'d from same directory; no `-I` flag changes
   needed.

2. **Static functions, single TU per include**: each engine `.c` is its
   own translation unit. The header's `static` functions duplicate per
   inclusion, which is exactly what's wanted (no symbol collision at
   link time, no shared object code — every wasm module stays
   self-contained).

3. **Hook style for Family B**: a single `#ifdef READER_HAS_STRINGS`
   block in `read_expr`'s dispatch. The opt-in name pattern, not a
   generic `READER_EXTRA_DISPATCH` macro — there's exactly one
   extension in the wild and over-generalizing would invite drift.

4. **Each phase = one commit**: Phase 1 (Family A consolidation) lands
   independently from Phase 2 (Family B hook). Bisection stays clean if
   either phase introduces a regression.

5. **No code changes outside the reader**: no opportunistic cleanup,
   no renaming, no comment edits to non-reader code. Keep the blast
   radius identical to the audit findings.

## Phase 1 — Family A extraction

**Goal**: 11 engines stop carrying inline reader copies; all `#include
"reader.h"` instead. Zero behavioral change.

### Steps

1. Create `engines/reader.h` containing the canonical reader. Use
   `engines/lisp.c`'s version (lines 99-156) as the base because it's
   the most-commented variant. Drop the dead `(void)tailp;` vestige
   (line 133) and the `*tailp=&head;` variable (line 120) — they're
   already dead code.
2. For each of the 11 Family A files: delete the inline reader region
   (skipws through end of read_expr), replace with `#include "reader.h"`.
   The include must come AFTER the engine's declarations of `cons`,
   `intern`, `mkfix`, `NIL`, `ERR`, `s_quote`, the `cells[]`/`symname[]`
   arrays — anything the reader references. In practice this means
   placing the include where the inline reader currently sits.
3. Build: `PATH="/opt/homebrew/opt/llvm/bin:$PATH" ./build.sh`. All 11
   wasm modules must build, and the zero-imports assertion must still
   pass (build.sh runs it automatically).
4. Verify parity: `node harness/parity.mjs`. **Must report 136/136 agree
   across all 8 engines** — same baseline as today (2026-06-07).
5. Verify bench: `node harness/bench.mjs`. Numbers should be unchanged.
   Any column shifting >2% is unexpected — the reader is in `init()` /
   read-once territory, so even noise shouldn't be visible.
6. Smoke-check the prototype engines (which have no harness coverage):
   `./native_cli_lisp -e "(+ 1 2)"` and the same for the bc_base,
   bc_inline, bc_super modules via `node harness/lisp-cli.mjs <wasm> -e
   "(+ 1 2)"` (or equivalent — they may not have a CLI; in that case
   `WebAssembly.instantiate` + a 5-line script suffices).

### Verification artifacts to commit

The parity result (`136/136 programs agree across all 8 engines`) and
bench delta table should appear in the Phase 1 commit message. Same
shape as H11's "before/after numbers in the FINDINGS write-up" — keeps
the audit trail self-contained.

### What can go wrong

- **Subtle latent reader divergence the audit missed**: covered by the
  parity baseline. If 136/136 → <136/136 after extraction, bisect by
  re-introducing engines' original readers one at a time.
- **Build-order include problems**: `cons`, `intern`, etc. must be
  declared before the include. If an engine has a forward-declaration
  ordering that doesn't match, that's the symptom. Fix by inserting
  forward declarations into the engine file, not into `reader.h` —
  `reader.h` should make minimal assumptions.
- **Prototype/bc_orig.c is an orphan**: imported in the first commit,
  never built, never touched. Leave it alone. No migration, no
  deletion — out of scope.

## Phase 2 — Family B hook (bytecode_gc strings)

**Goal**: `engines/bytecode_gc.c` joins the shared reader without losing
string support. One opt-in hook in `reader.h`.

### Steps

1. Add to `reader.h`:
   ```c
   #ifdef READER_HAS_STRINGS
   static u32 read_string();  // engine provides
   #endif
   ```
   And in `read_expr`:
   ```c
   #ifdef READER_HAS_STRINGS
   if(c=='"'){ return read_string(); }
   #endif
   ```
2. In `engines/bytecode_gc.c`:
   ```c
   #define READER_HAS_STRINGS
   static u32 read_string();  // forward — body below or above
   #include "reader.h"

   static u32 read_string(){
     // existing body (unchanged from current bytecode_gc.c:198-226)
   }
   ```
   Remove the now-redundant inline `skipws` / `is_delim` /
   `read_atom` / `read_list` / `read_expr` from `bytecode_gc.c`.
3. Build + parity (must stay 136/136) + `node harness/parity_strings.mjs`
   (must stay green — this is the bytecode_gc-only string test).
4. Bench: bytecode_gc column should stay flat. The reader runs once at
   eval_source start; no per-iteration cost.

### What can go wrong

- **`read_string` body has dependencies the shared reader doesn't know
  about**: `strheap`, `STR_HEAP_BYTES`, `str_entry_size`, `g_oom`,
  `g_pending_str_off`, `s_string`. These all live in `bytecode_gc.c`
  and will be in scope at include time because the include sits inside
  `bytecode_gc.c`. Should be fine.
- **The `#define` order matters**: `READER_HAS_STRINGS` must be defined
  before `#include "reader.h"`. Mark this with a comment on the define
  line.
- **Other engines stay clean**: `READER_HAS_STRINGS` is not defined in
  any non-bytecode_gc engine; the `#ifdef` block compiles out and they
  reject `"..."` exactly as they do today (whole `"foo"` token becomes
  the 5-char symbol literally named `"foo"` because `"` isn't a
  delimiter — pre-existing quirk, preserved).

## Phase 3 (follow-on, separate work) — land the parked f-call branch

Once Phase 1+2 are merged, the `reader-sugar-fn-calls` branch (current
sha `80fe5ce`) can rebase onto the shared reader. Its 6-line diff that
currently lives in `engines/lisp.c` becomes 6 lines in `engines/reader.h`,
instantly active in all 11 Family A engines and (via the existing hook
order) also in `bytecode_gc.c`.

Optional opt-in via `#define READER_FCALL_SUGAR` if the user wants
per-engine control — probably not needed, but the pattern from Phase 2
makes this almost free.

The 21 test cases in `tests/reader_sugar.sh` (already written on the
branch) will validate that the rebased sugar still produces the
expected ASTs across all engines.

## Out of scope (explicit)

- **Futamura tools** (Family C). Their reader is similar in shape but
  fundamentally different in error-handling strategy and intern API.
  Unifying them would require a much larger hook surface (error
  callback, intern function pointer) for marginal gain — they're dev
  tools, not engines. Revisit only if `$`-marked PE syntax grows
  features that the shared reader could plausibly host.

- **Removing the existing reader code from the `prototype/bc_*` files**
  vs. migrating them. The audit treats them as Family A; the
  prototype-engine smoke check (Phase 1 step 6) is the only verification
  we have for them. If a smoke check fails, the conservative move is to
  leave that one file with its inline reader and proceed.

- **Reader error-message quality**. The current reader returns `ERR` on
  any failure with no position info. Not addressed here — same surface
  area pre- and post-extraction.

## Verification commands (one-stop)

```bash
# Build everything (homebrew clang required for wasm32)
PATH="/opt/homebrew/opt/llvm/bin:$PATH" ./build.sh

# All-engine parity (8 engines, 136 programs)
node harness/parity.mjs

# Bytecode_gc-only string parity
node harness/parity_strings.mjs

# Bench (verify reader change has zero perf impact)
node harness/bench.mjs

# Prototype engine smoke (no harness coverage)
# adapt as needed; each prototype wasm exports the same input_ptr/eval_source ABI
```

Expected outcomes per phase:

| phase | parity.mjs | parity_strings.mjs | bench | builds |
|-------|------------|---------------------|-------|--------|
| Phase 1 commit | 136/136 | 8/8 | flat | 25/25 zero-imports |
| Phase 2 commit | 136/136 | 8/8 | flat | 25/25 zero-imports |
