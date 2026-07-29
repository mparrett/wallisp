// harness/engine.mjs — the shared wasm loader for every driver in harness/.
//
// Every engine exports the same tiny ABI (DEV.md, "wasm ABI"): write source
// bytes at input_ptr(), call eval_source(len) -> output length, read the result
// string at output_ptr(). That handshake used to be re-implemented in each of
// the ~15 files here, so changing the ABI meant editing all of them and hoping
// none were missed. It lives here now.
//
// NOT converted, deliberately:
//   * standalone/ — a declared fork with its own drift policy (standalone/README.md).
//     Sharing code with the study repo is exactly what that fork exists to avoid.
//   * web/*.html — must stay self-contained single files; that's their purpose.
//
// Views over memory.buffer are built per call rather than cached. These modules
// use a fixed --initial-memory and never grow, so a cached view would work
// today, but a detached buffer after a future memory.grow is a silent
// wrong-answer bug and the per-call cost is a couple of pointer reads.

import fs from 'fs';

const enc = new TextEncoder();
const dec = new TextDecoder();

// A bare name ("bytecode_gc.wasm") resolves against the repo root, which is
// where build.sh puts the modules. An absolute path is used as-is.
function resolve(wasm) {
  return wasm.startsWith('/') ? wasm : new URL('../' + wasm, import.meta.url);
}

/**
 * Instantiate an engine module.
 *
 * Returns:
 *   run(src)             one-shot eval_source — re-inits the VM every call
 *   evalPersistent(src)  eval_persistent — globals/symbols/arena/strheap survive
 *   resetSession()       reset_session — start or clear a persistent session
 *   rerun(entry)         re-execute already-compiled bytecode (no recompile)
 *   exports              raw wasm exports (gc_count, strheap_used, last_entry,
 *                        input_slots_ptr, icount — engine-dependent)
 *   name                 the file's basename, for table headers and messages
 *
 * The persistent-session methods throw a legible error on engines that don't
 * export them (only bytecode_gc does) rather than failing as "not a function".
 * eval_source and eval_persistent must not be mixed on one instance — see
 * DEV.md; load two instances if a driver needs both.
 */
export async function loadEngine(wasm) {
  const path = resolve(wasm);
  let bytes;
  try {
    bytes = fs.readFileSync(path);
  } catch (e) {
    if (e.code === 'ENOENT') {
      throw new Error(`engine not found: ${wasm}\n  Run \`bash build.sh\` first ` +
                      `(the *_big.wasm and disasm.wasm variants are not checked in).`);
    }
    throw e;
  }
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;

  // Every engine's input buffer is `#define INCAP 8192`, and eval_source clamps
  // with `len < INCAP ? len : INCAP` — so oversized source is SILENTLY truncated
  // and the module evaluates a prefix. The interactive drivers (lisp-cli, repl,
  // game, render_probe, disasm) each guarded this separately; the test and bench
  // drivers didn't guard at all. Centralised here, because a wrong answer from a
  // truncated program is far worse than a thrown error.
  const INCAP = 8192;
  const write = (src) => {
    const b = enc.encode(src);
    if (b.length > INCAP) {
      throw new Error(`source too large: ${b.length} bytes > INCAP ${INCAP}. ` +
                      `The module would silently truncate and evaluate a prefix.`);
    }
    new Uint8Array(ex.memory.buffer, ex.input_ptr(), b.length).set(b);
    return b.length;
  };
  const read = (n) => dec.decode(new Uint8Array(ex.memory.buffer, ex.output_ptr(), n));

  const need = (fn, what) => {
    if (typeof ex[fn] !== 'function') {
      throw new Error(`${String(wasm)} does not export ${fn}() — ${what} is bytecode_gc-only.`);
    }
    return ex[fn];
  };

  return {
    name: String(wasm).split('/').pop(),
    exports: ex,
    run: (src) => read(ex.eval_source(write(src))),
    evalPersistent: (src) => read(need('eval_persistent', 'the persistent session')(write(src))),
    resetSession: () => need('reset_session', 'the persistent session')(),
    rerun: (entry) => read(need('rerun', 'run-without-recompile')(entry)),
  };
}

/** Load several engines concurrently. Returns [{name, ...}] in the given order. */
export function loadEngines(files) {
  return Promise.all(files.map(loadEngine));
}
