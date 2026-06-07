; Cons cells, the heart of any Lisp.
;
; cons builds pairs; car/cdr access them.
; A "list" is just a chain of cons cells ending in nil.

(define (iota n)
  (if (= n 0)
      nil
      (cons n (iota (- n 1)))))

(define (sum l)
  (if (null? l)
      0
      (+ (car l) (sum (cdr l)))))

; Sum 1..10 = 55
(sum (iota 10))
