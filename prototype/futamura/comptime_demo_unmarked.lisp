; Same as comptime_demo.lisp but with the $ removed — the version
; you'd run if there were no preprocessor at all. Every call to (work x)
; recomputes (sum-to 100) at runtime.

(define (sum-to n)
  (if (= n 0)
      0
      (+ n (sum-to (- n 1)))))

(define (work x)
  (+ x (sum-to 100)))

(define (loop n acc)
  (if (= n 0)
      acc
      (loop (- n 1) (+ acc (work n)))))

(loop 1000 0)
