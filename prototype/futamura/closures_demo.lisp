; Demo for the closures-v1 specializer extension.
;
;   (work n) computes (+ n 15) via:
;     - `let` with two comptime fixnum bindings (k=5, m=10)
;     - inline lambda application: ((lambda (x) (+ x (+ k m))) n)
;
;   After specialization, the let bindings substitute at spec time, the inner
;   `(+ k m)` folds to 15, and the lambda inlines to `(+ x 15)` applied to n.
;   The residual `work` is just `(+ n 15)` — no let, no lambda, no closure.
;
;   `loop` then iterates: sum_{i=1..N}((work i)) = N*(N+1)/2 + 15*N.
;   For N=1000: 500500 + 15000 = 515500.

(define (work n)
  (let ((k 5)
        (m 10))
    ((lambda (x) (+ x (+ k m))) n)))

(define (loop i acc)
  (if (= i 0)
      acc
      (loop (- i 1) (+ acc (work i)))))
