(module
  (memory (export "memory") 1)
  (global $cell_top (export "cell_top") (mut i32) (i32.const 0))
  ;; layout: vstack @ byte 0 ; cons arena @ 1024 (8 bytes/cell) ; program @ 8192
  ;; tagged values: fixnum = v<<2|0 ; cons = idx<<2|1
  (global $vsp (mut i32) (i32.const 0))
  (func (export "run") (param $ip i32) (result i32)
    (local $op i32) (local $a i32) (local $b i32) (local $i i32)
    (loop $loop
      (local.set $op (i32.load (local.get $ip)))
      (local.set $ip (i32.add (local.get $ip) (i32.const 4)))
      (block $halt (block $cons (block $add (block $const
        (br_table $const $add $cons $halt (local.get $op)))
        ;; --- OP_CONST: push next word, advance ip ---
        (i32.store (i32.shl (global.get $vsp) (i32.const 2)) (i32.load (local.get $ip)))
        (global.set $vsp (i32.add (global.get $vsp) (i32.const 1)))
        (local.set $ip (i32.add (local.get $ip) (i32.const 4)))
        (br $loop))
        ;; --- OP_ADD: pop b, pop a, push a+b (tag-00 fixnums add directly) ---
        (global.set $vsp (i32.sub (global.get $vsp) (i32.const 1)))
        (local.set $b (i32.load (i32.shl (global.get $vsp) (i32.const 2))))
        (global.set $vsp (i32.sub (global.get $vsp) (i32.const 1)))
        (local.set $a (i32.load (i32.shl (global.get $vsp) (i32.const 2))))
        (i32.store (i32.shl (global.get $vsp) (i32.const 2)) (i32.add (local.get $a) (local.get $b)))
        (global.set $vsp (i32.add (global.get $vsp) (i32.const 1)))
        (br $loop))
        ;; --- OP_CONS: pop cdr,car ; INLINE bump-allocate (no call!) ; push tagged cons ---
        (global.set $vsp (i32.sub (global.get $vsp) (i32.const 1)))
        (local.set $b (i32.load (i32.shl (global.get $vsp) (i32.const 2))))      ;; cdr
        (global.set $vsp (i32.sub (global.get $vsp) (i32.const 1)))
        (local.set $a (i32.load (i32.shl (global.get $vsp) (i32.const 2))))      ;; car
        (local.set $i (global.get $cell_top))
        (i32.store (i32.add (i32.const 1024) (i32.mul (local.get $i) (i32.const 8))) (local.get $a))
        (i32.store (i32.add (i32.const 1028) (i32.mul (local.get $i) (i32.const 8))) (local.get $b))
        (global.set $cell_top (i32.add (local.get $i) (i32.const 1)))
        (i32.store (i32.shl (global.get $vsp) (i32.const 2))
                   (i32.or (i32.shl (local.get $i) (i32.const 2)) (i32.const 1)))
        (global.set $vsp (i32.add (global.get $vsp) (i32.const 1)))
        (br $loop))
      ;; --- OP_HALT: fall out of loop ---
    )
    (i32.load (i32.shl (i32.sub (global.get $vsp) (i32.const 1)) (i32.const 2))))
)
