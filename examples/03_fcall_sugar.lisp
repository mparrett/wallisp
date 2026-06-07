; F-call syntactic sugar (lives in engines/reader.h).
;
;   fn(a, b)    reads as  (fn a b)
;   fn(a b)     reads as  (fn a b)    -- comma is whitespace
;   fn (a b)    is NOT a call -- whitespace breaks tight-bind, two siblings
;   f(g(x), y)  reads as  (f (g x) y)
;
; Args are bare siblings, not single expressions. To pass (- n 1) as one
; arg, nest the sugar: -(n, 1) reads as (- n 1).

(define (fact n)
  (if (< n 2)
      1
      (* n fact(-(n, 1)))))

fact(6)
