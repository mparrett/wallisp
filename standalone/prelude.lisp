; wallisp prelude — small standard library, all in wallisp source.
;
; Usage: concat into a program to get these definitions, e.g.
;   cat prelude.lisp program.lisp | node cli.mjs
;
; Tail-recursive where natural (length, reverse, fold via accumulator
; helpers); list-builders (append, map, filter) stay non-tail for clarity
; — fine for small lists, use fold for big ones.

; ---- logic ----
(define (not x) (if x () t))

; ---- comparisons (wallisp core ships only < and =) ----
(define (>  a b) (< b a))
(define (>= a b) (not (< a b)))
(define (<= a b) (not (< b a)))

; ---- list length (tail-recursive) ----
(define (length-iter n l) (if (null? l) n (length-iter (+ n 1) (cdr l))))
(define (length l) (length-iter 0 l))

; ---- reverse (tail-recursive) ----
(define (reverse-iter acc l)
  (if (null? l) acc (reverse-iter (cons (car l) acc) (cdr l))))
(define (reverse l) (reverse-iter () l))

; ---- left fold (tail-recursive) ----
;   (fold f init '(a b c)) => (f (f (f init a) b) c)
(define (fold f acc l)
  (if (null? l) acc (fold f (f acc (car l)) (cdr l))))

; ---- list builders (not tail-recursive — cons stacks the result) ----
(define (append a b)
  (if (null? a) b (cons (car a) (append (cdr a) b))))

(define (map f l)
  (if (null? l) () (cons (f (car l)) (map f (cdr l)))))

(define (filter p l)
  (cond ((null? l) ())
        ((p (car l)) (cons (car l) (filter p (cdr l))))
        (else (filter p (cdr l)))))

; ---- assoc — a-list lookup by key, returns the (key . value) pair or () ----
(define (assoc k l)
  (cond ((null? l) ())
        ((= (car (car l)) k) (car l))
        (else (assoc k (cdr l)))))
