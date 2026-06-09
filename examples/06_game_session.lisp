; 06_game_session.lisp — a turn-loop game in the PERSISTENT session.
;
; Unlike the other examples (one-shot, run via lisp-cli.mjs / eval_source),
; this is a *session* demo: each line is one turn, and state survives across
; lines via eval_persistent. Run it through the REPL:
;
;   node harness/repl.mjs < examples/06_game_session.lisp
;
; NOTE: the REPL is line-oriented (one eval_persistent per line), so every
; top-level form here is kept on a SINGLE line. Multi-line input is a pending
; "real Milestone A" nicety (see terminal_game_roadmap.md). Comment-only lines
; echo as () when piped in — harmless.
;
; Coin-collector on a 0..9 line. State is (pos coin score seed):
;   pos   — player position
;   coin  — coin position (respawned from the RNG when collected)
;   score — coins collected
;   seed  — threaded LCG state (partner-doc pointer #2: RNG is a value)
; step/place/respawn are pure (pointer #1: (state,action)->state); the only
; mutation is `go`, the one-line turn operator. See terminal_game_roadmap.md.

(define (rng s) (mod (* (+ s 1) 75) 65537))
(define (mk a b c d) (cons a (cons b (cons c (cons d ())))))
(define (s-pos st) (car st))
(define (s-coin st) (car (cdr st)))
(define (s-score st) (car (cdr (cdr st))))
(define (s-seed st) (car (cdr (cdr (cdr st)))))
(define (clamp p) (cond ((< p 0) 0) ((< 9 p) 9) (else p)))
(define (respawn np score ns) (mk np (mod ns 10) score ns))
(define (place st np) (if (= np (s-coin st)) (respawn np (+ (s-score st) 1) (rng (s-seed st))) (mk np (s-coin st) (s-score st) (s-seed st))))
(define (step st mv) (place st (clamp (+ (s-pos st) mv))))

(define st (mk 0 (mod 12345 10) 0 12345))
(define (go mv) (begin (set! st (step st mv)) st))

; play: walk right to the coin (collect), then back left to the next one
st
(go 1)
(go 1)
(go 1)
(go 1)
(go 1)
(go -1)
(go -1)
(go -1)
(s-score st)
