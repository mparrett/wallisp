; baselines/metacircular.lisp — a tiny Lisp interpreter, written in wallisp's
; own Lisp. The "Maxwell's equations" form: eval and apply over an environment
; represented as an association list, dispatched on the head symbol of each
; form. Recursion in the program-being-interpreted is supplied by an explicit
; Y combinator — we have no mutation, so we can't tie the env knot otherwise.
;
; Run on each engine to measure the metacircular tax (interpretation on top
; of interpretation). Compared against direct fib(N) in harness/bench.mjs.
; See docs/notes/feat_metacircular_eval.md for the predictions
; this measurement is testing.
;
; The benchmark expression at the bottom evaluates to fib(N) = the wallisp
; engine running mceval running the standard Y-combinator fib. Change N to
; scale the workload.

(begin

(define lookup (lambda (sym env)
  (if (null? env) 'unbound
    (if (= (car (car env)) sym) (cdr (car env))
      (lookup sym (cdr env))))))

(define extend (lambda (params args env)
  (if (null? params) env
    (cons (cons (car params) (car args))
          (extend (cdr params) (cdr args) env)))))

(define eval-list (lambda (l env)
  (if (null? l) nil
    (cons (mceval (car l) env)
          (eval-list (cdr l) env)))))

; Closure representation: (closure params body env) — a 4-tuple built with cons.
; Primitives represented as themselves (the symbol '+ is its own value); since
; unbound atoms self-eval, we don't need to bind primitives in the initial env.

(define mcapply (lambda (f args)
  (if (pair? f)
      ; closure: walk the 4-tuple to extract params/body/env
      (mceval (car (cdr (cdr f)))
              (extend (car (cdr f)) args (car (cdr (cdr (cdr f))))))
      ; primitive: nested-if dispatch on the symbol (no cond)
      (if (= f '+) (+ (car args) (car (cdr args)))
        (if (= f '-) (- (car args) (car (cdr args)))
          (if (= f '*) (* (car args) (car (cdr args)))
            (if (= f '=) (= (car args) (car (cdr args)))
              (if (= f '<) (< (car args) (car (cdr args)))
                (if (= f 'cons) (cons (car args) (car (cdr args)))
                  (if (= f 'car) (car (car args))
                    (if (= f 'cdr) (cdr (car args))
                      (if (= f 'null?) (null? (car args))
                        (if (= f 'pair?) (pair? (car args))
                          'unknown-prim)))))))))))))

(define mceval (lambda (form env)
  (if (pair? form)
      (if (= (car form) 'quote) (car (cdr form))
        (if (= (car form) 'if)
            (if (null? (mceval (car (cdr form)) env))
                (mceval (car (cdr (cdr (cdr form)))) env)
                (mceval (car (cdr (cdr form))) env))
          (if (= (car form) 'lambda)
              (cons 'closure
                    (cons (car (cdr form))
                          (cons (car (cdr (cdr form)))
                                (cons env nil))))
            ; application: eval the operator and the arguments, then apply
            (mcapply (mceval (car form) env)
                     (eval-list (cdr form) env)))))
      ; atom: lookup in env, fall through to self-eval if unbound
      ; (numbers and primitive symbols both reach this path)
      (if (= (lookup form env) 'unbound) form (lookup form env)))))

; Benchmark: fib(N) via Y combinator (no top-level `define` inside mceval,
; so recursion has to be expressed via Y).
(mceval
  '((lambda (Y)
      ((Y (lambda (fib)
            (lambda (n)
              (if (< n 2)
                  n
                  (+ (fib (- n 1)) (fib (- n 2)))))))
       8))
    (lambda (f)
      ((lambda (x) (f (lambda (v) ((x x) v))))
       (lambda (x) (f (lambda (v) ((x x) v)))))))
  nil)

)
