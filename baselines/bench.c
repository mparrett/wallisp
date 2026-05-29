// baselines/bench.c — hand-written native C equivalents of the five Lisp
// benchmarks. The reference point at the bottom end: what these algorithms
// cost in straight-line C at -O2, with no interpreter in the way.
//
//   clang -O2 -o native_bench_baseline baselines/bench.c
//   ./native_bench_baseline
//
// Compare against:
//   - harness/bench.mjs            (eight Lisp engines, wasm-on-V8)
//   - node baselines/bench.js      (hand-written JS, V8 native)
//
// Output format matches the native engine bench: ENGINE \t BENCHMARK \t MS \t RESULT.
//
// Args are read from argv to defeat constant-folding (-O2 would otherwise
// evaluate fib/tak/ack at compile time given fully-constant inputs).
// Defaults match the engine bench inputs.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static long fib(long n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

static long tak(long x, long y, long z) {
  return y < x ? tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y)) : z;
}

static long ack(long m, long n) {
  if (m == 0) return n + 1;
  if (n == 0) return ack(m - 1, 1);
  return ack(m - 1, ack(m, n - 1));
}

typedef struct cell { long h; struct cell *t; } cell;
static cell *cons(long h, cell *t) { cell *c = malloc(sizeof *c); c->h = h; c->t = t; return c; }
static cell *iota(long n) { return n == 0 ? NULL : cons(n, iota(n - 1)); }
static cell *app(cell *a, cell *b) { return a == NULL ? b : cons(a->h, app(a->t, b)); }
static cell *nrev(cell *l) { return l == NULL ? NULL : app(nrev(l->t), cons(l->h, NULL)); }
static long lsum(cell *l) { return l == NULL ? 0 : l->h + lsum(l->t); }

// Recursive form; clang -O2 turns the tail call into a loop, matching the
// Lisp engines' TCO behaviour for tail recursion.
static long tailsum(long n, long acc) { return n == 0 ? acc : tailsum(n - 1, acc + n); }

static double now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(int argc, char **argv) {
  int reps = (argc > 1) ? atoi(argv[1]) : 25;
  if (reps < 1) reps = 1;
  // argv-driven constants prevent the optimizer from folding fib/tak/ack
  // at compile time. Defaults match harness/bench.mjs.
  long fib_n = (argc > 2) ? atol(argv[2]) : 24;
  long takx  = (argc > 3) ? atol(argv[3]) : 18;
  long taky  = (argc > 4) ? atol(argv[4]) : 12;
  long takz  = (argc > 5) ? atol(argv[5]) : 6;
  long ackm  = (argc > 6) ? atol(argv[6]) : 3;
  long ackn  = (argc > 7) ? atol(argv[7]) : 4;
  long lstn  = (argc > 8) ? atol(argv[8]) : 150;
  long tsn   = (argc > 9) ? atol(argv[9]) : 30000;

  const char *names[5] = {
    "fib(24)", "tak(18,12,6)", "ack(3,4)", "nrev+sum(150)", "tailsum(30000)"
  };
  long results[5] = {0};
  double bests[5];
  for (int i = 0; i < 5; i++) bests[i] = 1e18;

  for (int i = 0; i < reps; i++) {
    double t0, t1;

    t0 = now_ms(); results[0] = fib(fib_n);                      t1 = now_ms();
    if (t1 - t0 < bests[0]) bests[0] = t1 - t0;

    t0 = now_ms(); results[1] = tak(takx, taky, takz);           t1 = now_ms();
    if (t1 - t0 < bests[1]) bests[1] = t1 - t0;

    t0 = now_ms(); results[2] = ack(ackm, ackn);                 t1 = now_ms();
    if (t1 - t0 < bests[2]) bests[2] = t1 - t0;

    // Leaks the per-rep lists; ~11k cells/rep × 25 reps is bounded and
    // doesn't change the timings vs an arena approach at these sizes.
    t0 = now_ms(); results[3] = lsum(nrev(iota(lstn)));          t1 = now_ms();
    if (t1 - t0 < bests[3]) bests[3] = t1 - t0;

    t0 = now_ms(); results[4] = tailsum(tsn, 0);                 t1 = now_ms();
    if (t1 - t0 < bests[4]) bests[4] = t1 - t0;
  }

  printf("engine\tbenchmark\tms\tresult\n");
  for (int i = 0; i < 5; i++)
    printf("c\t%s\t%.4f\t%ld\n", names[i], bests[i], results[i]);
  return 0;
}
