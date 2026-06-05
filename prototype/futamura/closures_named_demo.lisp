; Demo for closures-v2-deep (top-level closure-valued defines).
;
;   (define add5 (make-adder 5))   ; evaluated at spec time → SV_LAMBDA
;   (define (work m) (add5 m))     ; (add5 m) beta-reduces via CompDef lookup
;
; Compared to closures_v2_demo.lisp, this version names the closure
; at top level instead of inlining `(make-adder 5)` at each use.
; Underlying mechanism is identical; the residual wasm should match.

(define (make-adder n)
  (lambda (x) (+ x n)))

(define add5 (make-adder 5))

(define (work m)
  (add5 m))

(define (loop i acc)
  (if (= i 0)
      acc
      (loop (- i 1) (+ acc (work i)))))
