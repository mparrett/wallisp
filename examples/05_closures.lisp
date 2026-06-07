; Closures capture their lexical environment.
;
; `adder` returns a function that remembers its `x` even after `adder`
; itself has returned. The captured binding is shared across calls.

(define (adder x)
  (lambda (y) (+ x y)))

(define add5 (adder 5))
(define add10 (adder 10))

; Each closure carries its own x:
;   (add5 3)  = 8
;   (add10 3) = 13
; Last form is what gets printed.
(+ (add5 3) (add10 3))
