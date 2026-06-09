; coin2d.lisp — 2D coin-collector for harness/game.mjs (B5 host game loop).
;
; Contract with the driver:
;   - (turn dx dy): advance the world by (dx,dy) and render one frame via
;     display; the driver maps keys -> dx/dy and flushes outbuf to the terminal.
;   - renders its own frame and resets the string heap to `base` each turn, so
;     per-frame render strings don't leak (render_slice_plan.md / ADR-004).
; Pure core wallisp + strings; no list/let/and (built from cons + nested if).

(define (rng s) (mod (* (+ s 1) 75) 65537))
(define W 12)
(define H 6)

; state = (px py cx cy score seed)
(define (mk px py cx cy sc sd) (cons px (cons py (cons cx (cons cy (cons sc (cons sd ())))))))
(define (g-px s) (car s))
(define (g-py s) (car (cdr s)))
(define (g-cx s) (car (cdr (cdr s))))
(define (g-cy s) (car (cdr (cdr (cdr s)))))
(define (g-sc s) (car (cdr (cdr (cdr (cdr s))))))
(define (g-sd s) (car (cdr (cdr (cdr (cdr (cdr s)))))))

(define (cl v hi) (cond ((< v 0) 0) ((< hi v) hi) (else v)))

; move, then if landed on the coin: score+1 and respawn it from two rng draws
(define (respawn s nx ny n1 n2) (mk nx ny (mod n1 W) (mod n2 H) (+ (g-sc s) 1) n2))
(define (settle s nx ny)
  (if (if (= nx (g-cx s)) (= ny (g-cy s)) ())
      (respawn s nx ny (rng (g-sd s)) (rng (rng (g-sd s))))
      (mk nx ny (g-cx s) (g-cy s) (g-sc s) (g-sd s))))
(define (step s dx dy) (settle s (cl (+ (g-px s) dx) (- W 1)) (cl (+ (g-py s) dy) (- H 1))))

; render: @ = player, o = coin, . = empty
(define (glyph x y s)
  (cond ((if (= x (g-px s)) (= y (g-py s)) ()) "@")
        ((if (= x (g-cx s)) (= y (g-cy s)) ()) "o")
        (else ".")))
(define (rowstr x y s) (if (= x W) "" (string-append (glyph x y s) (rowstr (+ x 1) y s))))
(define (rows y s) (if (= y H) "" (string-append (string-append (rowstr 0 y s) "\n") (rows (+ y 1) s))))

; number -> string, so the score renders without a host round-trip
(define (digit d)
  (cond ((= d 0) "0") ((= d 1) "1") ((= d 2) "2") ((= d 3) "3") ((= d 4) "4")
        ((= d 5) "5") ((= d 6) "6") ((= d 7) "7") ((= d 8) "8") (else "9")))
(define (num n) (if (< n 10) (digit n) (string-append (num (/ n 10)) (digit (mod n 10)))))

(define (frame s) (string-append (rows 0 s) (string-append "score: " (num (g-sc s)))))

(define st (mk 1 1 8 4 0 12345))
(define base (strheap-mark))
(define (render) (begin (display (frame st)) (strheap-reset base)))
(define (turn dx dy) (begin (set! st (step st dx dy)) (render)))
