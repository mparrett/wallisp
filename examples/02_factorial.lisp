; Classic recursion — factorial in standard s-expression form.
; (define (name args) body) is sugar for (define name (lambda (args) body)).

(define (fact n)
  (if (< n 2)
      1
      (* n (fact (- n 1)))))

(fact 10)
