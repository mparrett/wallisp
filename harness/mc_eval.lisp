; mc_eval.lisp — a metacircular evaluator for a small wallisp subset.
;
; Subset (the GUEST language):
;   - literals: fixnum, nil, symbols (via quote)
;   - (quote x), (if c t e), (lambda (p...) b), (let ((x e) ...) b)
;   - function application (any number of args)
;   - primitives (dispatched by symbol): cons car cdr + - * = < null? pair?
;
; What's NOT in the guest: define, set!, letrec, begin, cond, /, mod.
; Self-recursion uses the pass-by-self idiom — the test program at the
; bottom demonstrates how to write recursive functions without letrec.
;
; The HOST is any wallisp engine. The evaluator is self-contained (no
; prelude required). It's the harness/bench target for H8.

; ---- environment: a-list of (symbol . value) pairs ----
(define (mc-extend params args env)
  (if (null? params) env
      (cons (cons (car params) (car args))
            (mc-extend (cdr params) (cdr args) env))))

; If the env doesn't bind sym, return the symbol itself — primitives live as
; bare symbols that mc-apply hands to mc-apply-prim. This is the classic
; "primitives are not in the env" idiom; it keeps env lookup purely positional.
(define (mc-lookup sym env)
  (if (null? env) sym
      (if (= (car (car env)) sym) (cdr (car env))
          (mc-lookup sym (cdr env)))))

; ---- closure rep: ('closure params body env) ----
(define (mc-make-closure ps b e) (cons 'closure (cons ps (cons b (cons e ())))))
(define (mc-closure? v) (if (pair? v) (= (car v) 'closure) ()))
(define (mc-clo-params c) (car (cdr c)))
(define (mc-clo-body c)   (car (cdr (cdr c))))
(define (mc-clo-env c)    (car (cdr (cdr (cdr c)))))

; ---- primitive dispatch: guest primitive symbol -> host call ----
(define (mc-apply-prim op args)
  (if (= op 'cons)  (cons  (car args) (car (cdr args)))
  (if (= op 'car)   (car   (car args))
  (if (= op 'cdr)   (cdr   (car args))
  (if (= op '+)     (+     (car args) (car (cdr args)))
  (if (= op '-)     (-     (car args) (car (cdr args)))
  (if (= op '*)     (*     (car args) (car (cdr args)))
  (if (= op '=)     (=     (car args) (car (cdr args)))
  (if (= op '<)     (<     (car args) (car (cdr args)))
  (if (= op 'null?) (null? (car args))
  (if (= op 'pair?) (pair? (car args))
      '<unknown-prim>))))))))))) ; nested if avoids cond's macro expansion cost

; ---- mutual recursion: eval and apply ----
(define (mc-eval expr env)
  (if (number? expr) expr
  (if (null? expr)   expr
  (if (symbol? expr) (mc-lookup expr env)
      ; expr is a cons — dispatch on its head
      (if (= (car expr) 'quote)  (car (cdr expr))
      (if (= (car expr) 'if)
            (if (mc-eval (car (cdr expr)) env)
                (mc-eval (car (cdr (cdr expr))) env)
                (mc-eval (car (cdr (cdr (cdr expr)))) env))
      (if (= (car expr) 'lambda)
            (mc-make-closure (car (cdr expr)) (car (cdr (cdr expr))) env)
      (if (= (car expr) 'let)
            ; (let ((x e) ...) body) ≡ ((lambda (x ...) body) e ...)
            (mc-apply (mc-make-closure (mc-let-params (car (cdr expr)))
                                       (car (cdr (cdr expr)))
                                       env)
                      (mc-eval-args (mc-let-args (car (cdr expr))) env))
            ; otherwise: application
            (mc-apply (mc-eval (car expr) env)
                      (mc-eval-args (cdr expr) env))))))))))

(define (mc-let-params bs) (if (null? bs) () (cons (car (car bs))      (mc-let-params (cdr bs)))))
(define (mc-let-args   bs) (if (null? bs) () (cons (car (cdr (car bs))) (mc-let-args   (cdr bs)))))

(define (mc-eval-args args env)
  (if (null? args) ()
      (cons (mc-eval (car args) env) (mc-eval-args (cdr args) env))))

(define (mc-apply f args)
  (if (mc-closure? f)
      (mc-eval (mc-clo-body f) (mc-extend (mc-clo-params f) args (mc-clo-env f)))
      (mc-apply-prim f args)))

; ---- driver: eval an expression in the empty env ----
(define (mc-run expr) (mc-eval expr ()))

; ---- the H8 test workload: metacircular fib via pass-by-self ----
; Pass-by-self avoids needing letrec in the guest. The lambda receives
; itself as its first arg; recursive calls pass `self` along.
;
;   guest-fib(self, n) = if n<2 then n else self(self, n-1) + self(self, n-2)
;
; The let just binds `fib` for readability; recursion goes through `self`.
(mc-run
  '(let ((fib (lambda (self n)
                (if (< n 2) n
                    (+ (self self (- n 1))
                       (self self (- n 2)))))))
     (fib fib 10)))
