; Demo: $-marked comptime folding.
;
; `sum-to` is a comptime-evaluable recursive function (no runtime args).
; `$(sum-to 100)` should fold to `5050` at preprocess time.
;
; `work` is the runtime entry. It does one cheap add per call instead of
; recursing through sum-to 100 times.

(define (sum-to n)
  (if (= n 0)
      0
      (+ n (sum-to (- n 1)))))

(define (work x)
  (+ x $(sum-to 100)))

(define (loop n acc)
  (if (= n 0)
      acc
      (loop (- n 1) (+ acc (work n)))))

(loop 1000 0)
