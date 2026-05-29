// native/bench.c — native equivalent of harness/bench.mjs.
//
// Built once per engine: the engine source is included as a single TU so we
// can call eval_source() / input_ptr() / output_ptr() directly without any
// JS/wasm host. Compile-time selection:
//
//   clang -O2 -DENGINE_NAME='"lisp"' -DENGINE_SRC='"../engines/lisp.c"' \
//         -o native_bench_lisp native/bench.c
//
// Each binary runs the same five benchmarks the wasm harness does, best-of-N,
// and prints one TSV line per benchmark: engine \t benchmark \t ms.
// A wrapper script (or just bash for-loop) aggregates the four engine outputs.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// The engine source defines static globals, eval_source(), input_ptr(), output_ptr().
// We include it directly so those statics are visible here in the same TU.
// Two prep steps:
//   - silence the wasm-only export_name attribute that native clang doesn't know;
//   - rename the engine's own memset/memcpy (defined for freestanding wasm) so
//     they don't collide with libc when linking native. The engine doesn't call
//     these by name in C — they exist only to satisfy compiler-synthesized calls
//     for static array zero-init, which native libc handles fine.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wunknown-attributes"
#define memset __engine_unused_memset
#define memcpy __engine_unused_memcpy
#include ENGINE_SRC
#undef memset
#undef memcpy
#pragma clang diagnostic pop

static const struct { const char *name, *src; } BENCHES[] = {
  {"fib(24)",
   "(begin (define fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))) (fib 24))"},
  {"tak(18,12,6)",
   "(begin (define tak (lambda (x y z) (if (< y x) (tak (tak (- x 1) y z) (tak (- y 1) z x) (tak (- z 1) x y)) z))) (tak 18 12 6))"},
  {"ack(3,4)",
   "(begin (define ack (lambda (m n) (if (= m 0) (+ n 1) (if (= n 0) (ack (- m 1) 1) (ack (- m 1) (ack m (- n 1))))))) (ack 3 4))"},
  {"nrev+sum(150)",
   "(begin (define ap (lambda (a b) (if (null? a) b (cons (car a) (ap (cdr a) b))))) (define rv (lambda (l) (if (null? l) nil (ap (rv (cdr l)) (cons (car l) nil))))) (define mk (lambda (n) (if (= n 0) nil (cons n (mk (- n 1)))))) (define sm (lambda (l) (if (null? l) 0 (+ (car l) (sm (cdr l)))))) (sm (rv (mk 150))))"},
  {"tailsum(30000)",
   "(begin (define ts (lambda (n a) (if (= n 0) a (ts (- n 1) (+ a n))))) (ts 30000 0))"},
};
#define NBENCH ((int)(sizeof(BENCHES) / sizeof(BENCHES[0])))

// Read result string from the engine's output buffer (for the sanity-check pass).
static void read_result(int olen, char *out, size_t outsz) {
  if (olen < 0 || (size_t)olen >= outsz) olen = (int)outsz - 1;
  memcpy(out, output_ptr(), olen);
  out[olen] = '\0';
}

static double bench_one(const char *src, int reps) {
  size_t n = strlen(src);
  double best_ms = 1e18;
  for (int i = 0; i < reps; i++) {
    memcpy(input_ptr(), src, n);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int olen = eval_source((unsigned)n);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    (void)olen;
    double ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
    if (ms < best_ms) best_ms = ms;
  }
  return best_ms;
}

int main(int argc, char **argv) {
  int reps = (argc > 1) ? atoi(argv[1]) : 25;
  if (reps < 1) reps = 1;

  // One pass first to capture the result string for sanity-check output.
  // Print one line per benchmark: ENGINE \t NAME \t MS \t RESULT.
  for (int i = 0; i < NBENCH; i++) {
    double ms = bench_one(BENCHES[i].src, reps);
    // Capture result from the last iteration's output (already in outbuf).
    char res[256];
    size_t n = strlen(BENCHES[i].src);
    memcpy(input_ptr(), BENCHES[i].src, n);
    int olen = eval_source((unsigned)n);
    read_result(olen, res, sizeof(res));
    printf("%s\t%s\t%.4f\t%s\n", ENGINE_NAME, BENCHES[i].name, ms, res);
    fflush(stdout);
  }
  return 0;
}
