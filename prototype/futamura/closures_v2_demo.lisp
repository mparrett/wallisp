; Demo for closures-v2 (specializer): make-adder returns a closure, captured
; over the comptime n; the closure is then applied at the call site with a
; runtime arg.
;
;   (make-adder 5) → closure with body (+ x n) and captured env {n=5}
;   ((make-adder 5) m) → beta-reduce: body becomes (+ m 5) in the residual.
;
; loop iterates work over 1..N; total = sum_{i=1..N}(i + 5) = N*(N+1)/2 + 5N.
; For N=1000: 500500 + 5000 = 505500.

(define (make-adder n)
  (lambda (x) (+ x n)))

(define (work m)
  ((make-adder 5) m))

(define (loop i acc)
  (if (= i 0)
      acc
      (loop (- i 1) (+ acc (work i)))))
