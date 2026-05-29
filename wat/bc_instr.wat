(module $bc_edit.wasm
  (type (;0;) (func (result i32)))
  (type (;1;) (func (param i32) (result i32)))
  (type (;2;) (func (param i32 i32)))
  (type (;3;) (func (param i32)))
  (func $input_ptr (type 0) (result i32)
    i32.const 1152)
  (func $output_ptr (type 0) (result i32)
    i32.const 9344)
  (func $eval_source (type 1) (param i32) (result i32)
    (local i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0
    i32.const 0
    i32.store offset=13464
    i32.const 0
    i32.const 0
    i32.store offset=13460
    i32.const 0
    i32.const 0
    i32.store8 offset=13452
    i32.const 0
    i32.const 0
    i32.store offset=13468
    i32.const 0
    i32.const 0
    i32.store8 offset=13456
    i32.const -4096
    local.set 1
    loop  ;; label = @1
      local.get 1
      local.tee 1
      i32.const 17568
      i32.add
      i32.const 15
      i32.store
      local.get 1
      i32.const 4
      i32.add
      local.tee 2
      local.set 1
      local.get 2
      br_if 0 (;@1;)
    end
    i32.const 0
    i32.const 1
    i32.store8 offset=17568
    i32.const 0
    i32.const 2
    i32.store offset=13464
    i32.const 0
    i32.const 26217
    i32.store16 offset=17616
    i32.const 0
    i64.const 8589934597
    i64.store offset=33984
    i32.const 0
    i32.const 1
    i32.store8 offset=17572
    i32.const 0
    i32.const 0
    i32.load offset=1051 align=1
    i32.store offset=17600
    i32.const 0
    i32.const 0
    i32.load8_u offset=1055
    i32.store8 offset=17604
    i32.const 17600
    local.set 1
    i32.const 1
    local.set 2
    i32.const 0
    local.set 3
    loop  ;; label = @1
      local.get 2
      local.set 4
      local.get 1
      local.set 5
      i32.const 0
      local.set 1
      block  ;; label = @2
        local.get 3
        local.tee 6
        i32.const 2
        i32.shl
        i32.const 33984
        i32.add
        i32.load
        local.tee 3
        i32.const 6
        i32.ne
        br_if 0 (;@2;)
        i32.const 0
        local.set 1
        block  ;; label = @3
          local.get 6
          i32.const 4
          i32.shl
          i32.const 17600
          i32.add
          i32.load8_u
          i32.const 100
          i32.ne
          br_if 0 (;@3;)
          i32.const 1
          local.set 2
          block  ;; label = @4
            loop  ;; label = @5
              block  ;; label = @6
                local.get 3
                local.get 2
                local.tee 1
                i32.ne
                br_if 0 (;@6;)
                local.get 3
                local.set 1
                br 2 (;@4;)
              end
              local.get 1
              i32.const 1
              i32.add
              local.tee 7
              local.set 2
              local.get 5
              local.get 1
              i32.add
              i32.load8_u
              local.get 1
              i32.const 1066
              i32.add
              i32.load8_u
              i32.eq
              br_if 0 (;@5;)
            end
            local.get 7
            i32.const -1
            i32.add
            local.set 1
          end
          local.get 1
          local.get 3
          i32.ge_u
          local.set 1
        end
        local.get 1
        local.set 1
      end
      block  ;; label = @2
        block  ;; label = @3
          local.get 1
          i32.eqz
          br_if 0 (;@3;)
          local.get 4
          local.set 5
          local.get 6
          i32.const 2
          i32.shl
          i32.const 2
          i32.or
          local.set 6
          br 1 (;@2;)
        end
        local.get 5
        i32.const 16
        i32.add
        local.set 1
        local.get 6
        i32.eqz
        local.tee 5
        local.set 2
        local.get 6
        i32.const 1
        i32.add
        local.tee 7
        local.set 3
        local.get 5
        local.set 5
        local.get 7
        i32.const 2
        i32.ne
        br_if 1 (;@1;)
      end
    end
    local.get 6
    local.set 1
    block  ;; label = @1
      local.get 5
      i32.const 1
      i32.and
      br_if 0 (;@1;)
      i32.const 0
      i32.const 3
      i32.store offset=13464
      i32.const 0
      i32.const 6
      i32.store offset=33992
      i32.const 0
      i32.const 0
      i32.load offset=1066 align=1
      i32.store offset=17632
      i32.const 0
      i32.const 0
      i32.load16_u offset=1070 align=1
      i32.store16 offset=17636
      i32.const 10
      local.set 1
    end
    i32.const 0
    local.get 1
    i32.store offset=17576
    i32.const 0
    i32.load offset=13464
    local.tee 4
    i32.const 0
    i32.ne
    local.set 1
    block  ;; label = @1
      block  ;; label = @2
        local.get 4
        br_if 0 (;@2;)
        local.get 1
        local.set 7
        br 1 (;@1;)
      end
      i32.const 17600
      local.set 2
      local.get 1
      local.set 5
      i32.const 0
      local.set 3
      loop  ;; label = @2
        local.get 5
        local.set 8
        local.get 2
        local.set 5
        i32.const 0
        local.set 1
        block  ;; label = @3
          local.get 3
          local.tee 6
          i32.const 2
          i32.shl
          i32.const 33984
          i32.add
          i32.load
          local.tee 3
          i32.const 6
          i32.ne
          br_if 0 (;@3;)
          i32.const 0
          local.set 1
          block  ;; label = @4
            local.get 6
            i32.const 4
            i32.shl
            i32.const 17600
            i32.add
            i32.load8_u
            i32.const 108
            i32.ne
            br_if 0 (;@4;)
            i32.const 1
            local.set 2
            block  ;; label = @5
              loop  ;; label = @6
                block  ;; label = @7
                  local.get 3
                  local.get 2
                  local.tee 1
                  i32.ne
                  br_if 0 (;@7;)
                  local.get 3
                  local.set 1
                  br 2 (;@5;)
                end
                local.get 1
                i32.const 1
                i32.add
                local.tee 7
                local.set 2
                local.get 5
                local.get 1
                i32.add
                i32.load8_u
                local.get 1
                i32.const 1073
                i32.add
                i32.load8_u
                i32.eq
                br_if 0 (;@6;)
              end
              local.get 7
              i32.const -1
              i32.add
              local.set 1
            end
            local.get 1
            local.get 3
            i32.ge_u
            local.set 1
          end
          local.get 1
          local.set 1
        end
        block  ;; label = @3
          local.get 1
          i32.eqz
          br_if 0 (;@3;)
          local.get 8
          local.set 7
          local.get 6
          i32.const 2
          i32.shl
          i32.const 2
          i32.or
          local.set 6
          br 2 (;@1;)
        end
        local.get 5
        i32.const 16
        i32.add
        local.set 2
        local.get 6
        i32.const 1
        i32.add
        local.tee 1
        local.get 4
        i32.lt_u
        local.tee 7
        local.set 5
        local.get 1
        local.set 3
        local.get 7
        local.set 7
        local.get 1
        local.get 4
        i32.ne
        br_if 0 (;@2;)
      end
    end
    local.get 6
    local.set 1
    block  ;; label = @1
      local.get 7
      i32.const 1
      i32.and
      br_if 0 (;@1;)
      i32.const 11
      local.set 1
      local.get 4
      i32.const 1023
      i32.gt_u
      br_if 0 (;@1;)
      i32.const 0
      local.get 4
      i32.const 1
      i32.add
      i32.store offset=13464
      local.get 4
      i32.const 4
      i32.shl
      local.tee 1
      i32.const 17600
      i32.add
      i32.const 0
      i32.load offset=1073 align=1
      i32.store align=1
      local.get 1
      i32.const 17604
      i32.add
      i32.const 0
      i32.load16_u offset=1077 align=1
      i32.store16 align=1
      local.get 4
      i32.const 2
      i32.shl
      local.tee 1
      i32.const 33984
      i32.add
      i32.const 6
      i32.store
      local.get 1
      i32.const 2
      i32.or
      local.set 1
    end
    i32.const 0
    local.get 1
    i32.store offset=17580
    i32.const 0
    i32.load offset=13464
    local.tee 4
    i32.const 0
    i32.ne
    local.set 1
    block  ;; label = @1
      block  ;; label = @2
        local.get 4
        br_if 0 (;@2;)
        local.get 1
        local.set 7
        br 1 (;@1;)
      end
      i32.const 17600
      local.set 2
      local.get 1
      local.set 5
      i32.const 0
      local.set 3
      loop  ;; label = @2
        local.get 5
        local.set 8
        local.get 2
        local.set 5
        i32.const 0
        local.set 1
        block  ;; label = @3
          local.get 3
          local.tee 6
          i32.const 2
          i32.shl
          i32.const 33984
          i32.add
          i32.load
          local.tee 3
          i32.const 3
          i32.ne
          br_if 0 (;@3;)
          i32.const 0
          local.set 1
          block  ;; label = @4
            local.get 6
            i32.const 4
            i32.shl
            i32.const 17600
            i32.add
            i32.load8_u
            i32.const 108
            i32.ne
            br_if 0 (;@4;)
            i32.const 1
            local.set 2
            block  ;; label = @5
              loop  ;; label = @6
                block  ;; label = @7
                  local.get 3
                  local.get 2
                  local.tee 1
                  i32.ne
                  br_if 0 (;@7;)
                  local.get 3
                  local.set 1
                  br 2 (;@5;)
                end
                local.get 1
                i32.const 1
                i32.add
                local.tee 7
                local.set 2
                local.get 5
                local.get 1
                i32.add
                i32.load8_u
                local.get 1
                i32.const 1024
                i32.add
                i32.load8_u
                i32.eq
                br_if 0 (;@6;)
              end
              local.get 7
              i32.const -1
              i32.add
              local.set 1
            end
            local.get 1
            local.get 3
            i32.ge_u
            local.set 1
          end
          local.get 1
          local.set 1
        end
        block  ;; label = @3
          local.get 1
          i32.eqz
          br_if 0 (;@3;)
          local.get 8
          local.set 7
          local.get 6
          i32.const 2
          i32.shl
          i32.const 2
          i32.or
          local.set 6
          br 2 (;@1;)
        end
        local.get 5
        i32.const 16
        i32.add
        local.set 2
        local.get 6
        i32.const 1
        i32.add
        local.tee 1
        local.get 4
        i32.lt_u
        local.tee 7
        local.set 5
        local.get 1
        local.set 3
        local.get 7
        local.set 7
        local.get 1
        local.get 4
        i32.ne
        br_if 0 (;@2;)
      end
    end
    local.get 6
    local.set 1
    block  ;; label = @1
      local.get 7
      i32.const 1
      i32.and
      br_if 0 (;@1;)
      i32.const 11
      local.set 1
      local.get 4
      i32.const 1023
      i32.gt_u
      br_if 0 (;@1;)
      i32.const 0
      local.get 4
      i32.const 1
      i32.add
      i32.store offset=13464
      local.get 4
      i32.const 4
      i32.shl
      local.tee 1
      i32.const 17600
      i32.add
      i32.const 0
      i32.load16_u offset=1024 align=1
      i32.store16 align=1
      local.get 1
      i32.const 17602
      i32.add
      i32.const 0
      i32.load8_u offset=1026
      i32.store8
      local.get 4
      i32.const 2
      i32.shl
      local.tee 1
      i32.const 33984
      i32.add
      i32.const 3
      i32.store
      local.get 1
      i32.const 2
      i32.or
      local.set 1
    end
    i32.const 0
    local.get 1
    i32.store offset=17584
    i32.const 0
    i32.load offset=13464
    local.tee 4
    i32.const 0
    i32.ne
    local.set 1
    block  ;; label = @1
      block  ;; label = @2
        local.get 4
        br_if 0 (;@2;)
        local.get 1
        local.set 7
        br 1 (;@1;)
      end
      i32.const 17600
      local.set 2
      local.get 1
      local.set 5
      i32.const 0
      local.set 3
      loop  ;; label = @2
        local.get 5
        local.set 8
        local.get 2
        local.set 5
        i32.const 0
        local.set 1
        block  ;; label = @3
          local.get 3
          local.tee 6
          i32.const 2
          i32.shl
          i32.const 33984
          i32.add
          i32.load
          local.tee 3
          i32.const 5
          i32.ne
          br_if 0 (;@3;)
          i32.const 0
          local.set 1
          block  ;; label = @4
            local.get 6
            i32.const 4
            i32.shl
            i32.const 17600
            i32.add
            i32.load8_u
            i32.const 98
            i32.ne
            br_if 0 (;@4;)
            i32.const 1
            local.set 2
            block  ;; label = @5
              loop  ;; label = @6
                block  ;; label = @7
                  local.get 3
                  local.get 2
                  local.tee 1
                  i32.ne
                  br_if 0 (;@7;)
                  local.get 3
                  local.set 1
                  br 2 (;@5;)
                end
                local.get 1
                i32.const 1
                i32.add
                local.tee 7
                local.set 2
                local.get 5
                local.get 1
                i32.add
                i32.load8_u
                local.get 1
                i32.const 1041
                i32.add
                i32.load8_u
                i32.eq
                br_if 0 (;@6;)
              end
              local.get 7
              i32.const -1
              i32.add
              local.set 1
            end
            local.get 1
            local.get 3
            i32.ge_u
            local.set 1
          end
          local.get 1
          local.set 1
        end
        block  ;; label = @3
          local.get 1
          i32.eqz
          br_if 0 (;@3;)
          local.get 8
          local.set 7
          local.get 6
          i32.const 2
          i32.shl
          i32.const 2
          i32.or
          local.set 6
          br 2 (;@1;)
        end
        local.get 5
        i32.const 16
        i32.add
        local.set 2
        local.get 6
        i32.const 1
        i32.add
        local.tee 1
        local.get 4
        i32.lt_u
        local.tee 7
        local.set 5
        local.get 1
        local.set 3
        local.get 7
        local.set 7
        local.get 1
        local.get 4
        i32.ne
        br_if 0 (;@2;)
      end
    end
    local.get 6
    local.set 1
    block  ;; label = @1
      local.get 7
      i32.const 1
      i32.and
      br_if 0 (;@1;)
      i32.const 11
      local.set 1
      local.get 4
      i32.const 1023
      i32.gt_u
      br_if 0 (;@1;)
      i32.const 0
      local.get 4
      i32.const 1
      i32.add
      i32.store offset=13464
      local.get 4
      i32.const 4
      i32.shl
      local.tee 1
      i32.const 17600
      i32.add
      i32.const 0
      i32.load offset=1041 align=1
      i32.store align=1
      local.get 1
      i32.const 17604
      i32.add
      i32.const 0
      i32.load8_u offset=1045
      i32.store8
      local.get 4
      i32.const 2
      i32.shl
      local.tee 1
      i32.const 33984
      i32.add
      i32.const 5
      i32.store
      local.get 1
      i32.const 2
      i32.or
      local.set 1
    end
    i32.const 0
    local.get 1
    i32.store offset=17588
    i32.const 0
    i32.load offset=13464
    local.tee 4
    i32.const 0
    i32.ne
    local.set 1
    block  ;; label = @1
      block  ;; label = @2
        local.get 4
        br_if 0 (;@2;)
        local.get 1
        local.set 7
        br 1 (;@1;)
      end
      i32.const 17600
      local.set 2
      local.get 1
      local.set 5
      i32.const 0
      local.set 3
      loop  ;; label = @2
        local.get 5
        local.set 8
        local.get 2
        local.set 5
        i32.const 0
        local.set 1
        block  ;; label = @3
          local.get 3
          local.tee 6
          i32.const 2
          i32.shl
          i32.const 33984
          i32.add
          i32.load
          local.tee 3
          i32.const 8
          i32.ne
          br_if 0 (;@3;)
          i32.const 0
          local.set 1
          block  ;; label = @4
            local.get 6
            i32.const 4
            i32.shl
            i32.const 17600
            i32.add
            i32.load8_u
            i32.const 37
            i32.ne
            br_if 0 (;@4;)
            i32.const 1
            local.set 2
            block  ;; label = @5
              loop  ;; label = @6
                block  ;; label = @7
                  local.get 3
                  local.get 2
                  local.tee 1
                  i32.ne
                  br_if 0 (;@7;)
                  local.get 3
                  local.set 1
                  br 2 (;@5;)
                end
                local.get 1
                i32.const 1
                i32.add
                local.tee 7
                local.set 2
                local.get 5
                local.get 1
                i32.add
                i32.load8_u
                local.get 1
                i32.const 1057
                i32.add
                i32.load8_u
                i32.eq
                br_if 0 (;@6;)
              end
              local.get 7
              i32.const -1
              i32.add
              local.set 1
            end
            local.get 1
            local.get 3
            i32.ge_u
            local.set 1
          end
          local.get 1
          local.set 1
        end
        block  ;; label = @3
          local.get 1
          i32.eqz
          br_if 0 (;@3;)
          local.get 8
          local.set 7
          local.get 6
          i32.const 2
          i32.shl
          i32.const 2
          i32.or
          local.set 6
          br 2 (;@1;)
        end
        local.get 5
        i32.const 16
        i32.add
        local.set 2
        local.get 6
        i32.const 1
        i32.add
        local.tee 1
        local.get 4
        i32.lt_u
        local.tee 7
        local.set 5
        local.get 1
        local.set 3
        local.get 7
        local.set 7
        local.get 1
        local.get 4
        i32.ne
        br_if 0 (;@2;)
      end
    end
    local.get 6
    local.set 1
    block  ;; label = @1
      local.get 7
      i32.const 1
      i32.and
      br_if 0 (;@1;)
      i32.const 11
      local.set 1
      local.get 4
      i32.const 1023
      i32.gt_u
      br_if 0 (;@1;)
      i32.const 0
      local.get 4
      i32.const 1
      i32.add
      i32.store offset=13464
      local.get 4
      i32.const 4
      i32.shl
      i32.const 17600
      i32.add
      i64.const 7310034283826799397
      i64.store
      local.get 4
      i32.const 2
      i32.shl
      local.tee 1
      i32.const 33984
      i32.add
      i32.const 8
      i32.store
      local.get 1
      i32.const 2
      i32.or
      local.set 1
    end
    i32.const 0
    local.get 1
    i32.store offset=17592
    i32.const 0
    i32.load offset=13464
    local.tee 4
    i32.const 0
    i32.ne
    local.set 1
    block  ;; label = @1
      block  ;; label = @2
        local.get 4
        br_if 0 (;@2;)
        local.get 1
        local.set 2
        i32.const 0
        local.set 1
        br 1 (;@1;)
      end
      i32.const 17600
      local.set 2
      local.get 1
      local.set 5
      i32.const 0
      local.set 3
      loop  ;; label = @2
        local.get 5
        local.set 8
        local.get 2
        local.set 5
        i32.const 0
        local.set 1
        block  ;; label = @3
          local.get 3
          local.tee 6
          i32.const 2
          i32.shl
          i32.const 33984
          i32.add
          i32.load
          local.tee 3
          i32.const 3
          i32.ne
          br_if 0 (;@3;)
          i32.const 0
          local.set 1
          block  ;; label = @4
            local.get 6
            i32.const 4
            i32.shl
            i32.const 17600
            i32.add
            i32.load8_u
            i32.const 110
            i32.ne
            br_if 0 (;@4;)
            i32.const 1
            local.set 2
            block  ;; label = @5
              loop  ;; label = @6
                block  ;; label = @7
                  local.get 3
                  local.get 2
                  local.tee 1
                  i32.ne
                  br_if 0 (;@7;)
                  local.get 3
                  local.set 1
                  br 2 (;@5;)
                end
                local.get 1
                i32.const 1
                i32.add
                local.tee 7
                local.set 2
                local.get 5
                local.get 1
                i32.add
                i32.load8_u
                local.get 1
                i32.const 1047
                i32.add
                i32.load8_u
                i32.eq
                br_if 0 (;@6;)
              end
              local.get 7
              i32.const -1
              i32.add
              local.set 1
            end
            local.get 1
            local.get 3
            i32.ge_u
            local.set 1
          end
          local.get 1
          local.set 1
        end
        block  ;; label = @3
          local.get 1
          i32.eqz
          br_if 0 (;@3;)
          local.get 8
          local.set 2
          local.get 6
          i32.const 1073741823
          i32.and
          local.set 1
          br 2 (;@1;)
        end
        local.get 5
        i32.const 16
        i32.add
        local.set 2
        local.get 6
        i32.const 1
        i32.add
        local.tee 1
        local.get 4
        i32.lt_u
        local.tee 7
        local.set 5
        local.get 1
        local.set 3
        local.get 1
        local.get 4
        i32.ne
        br_if 0 (;@2;)
      end
      local.get 7
      local.set 2
      i32.const 0
      local.set 1
    end
    local.get 1
    local.set 1
    block  ;; label = @1
      local.get 2
      i32.const 1
      i32.and
      br_if 0 (;@1;)
      i32.const 2
      local.set 1
      local.get 4
      i32.const 1023
      i32.gt_u
      br_if 0 (;@1;)
      i32.const 0
      local.get 4
      i32.const 1
      i32.add
      i32.store offset=13464
      local.get 4
      i32.const 4
      i32.shl
      local.tee 1
      i32.const 17600
      i32.add
      i32.const 0
      i32.load16_u offset=1047 align=1
      i32.store16 align=1
      local.get 1
      i32.const 17602
      i32.add
      i32.const 0
      i32.load8_u offset=1049
      i32.store8
      local.get 4
      i32.const 2
      i32.shl
      i32.const 33984
      i32.add
      i32.const 3
      i32.store
      local.get 4
      local.set 1
    end
    local.get 1
    i32.const 2
    i32.shl
    i32.const 13472
    i32.add
    i32.const 3
    i32.store
    i32.const 0
    i32.load offset=13464
    local.tee 5
    i32.const 0
    i32.ne
    local.set 1
    block  ;; label = @1
      block  ;; label = @2
        local.get 5
        br_if 0 (;@2;)
        local.get 1
        local.set 2
        i32.const 0
        local.set 1
        br 1 (;@1;)
      end
      i32.const 33984
      local.set 3
      i32.const 17600
      local.set 2
      local.get 1
      local.set 7
      i32.const 0
      local.set 6
      loop  ;; label = @2
        local.get 6
        local.set 6
        local.get 7
        local.set 7
        local.get 2
        local.set 2
        i32.const 0
        local.set 1
        block  ;; label = @3
          local.get 3
          local.tee 3
          i32.load
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 2
          i32.load8_u
          i32.const 116
          i32.eq
          local.set 1
        end
        block  ;; label = @3
          local.get 1
          i32.eqz
          br_if 0 (;@3;)
          local.get 7
          local.set 2
          local.get 6
          i32.const 1073741823
          i32.and
          local.set 1
          br 2 (;@1;)
        end
        local.get 3
        i32.const 4
        i32.add
        local.set 3
        local.get 2
        i32.const 16
        i32.add
        local.set 2
        local.get 6
        i32.const 1
        i32.add
        local.tee 1
        local.get 5
        i32.lt_u
        local.tee 4
        local.set 7
        local.get 1
        local.set 6
        local.get 5
        local.get 1
        i32.ne
        br_if 0 (;@2;)
      end
      local.get 4
      local.set 2
      i32.const 0
      local.set 1
    end
    local.get 1
    local.set 1
    block  ;; label = @1
      local.get 2
      i32.const 1
      i32.and
      br_if 0 (;@1;)
      i32.const 2
      local.set 1
      local.get 5
      i32.const 1023
      i32.gt_u
      br_if 0 (;@1;)
      i32.const 0
      local.get 5
      i32.const 1
      i32.add
      i32.store offset=13464
      local.get 5
      i32.const 4
      i32.shl
      i32.const 17600
      i32.add
      i32.const 116
      i32.store8
      local.get 5
      i32.const 2
      i32.shl
      i32.const 33984
      i32.add
      i32.const 1
      i32.store
      local.get 5
      local.set 1
    end
    local.get 1
    i32.const 2
    i32.shl
    i32.const 13472
    i32.add
    i32.const 7
    i32.store
    i32.const 1028
    i32.const 19
    call $bindp
    i32.const 1037
    i32.const 23
    call $bindp
    i32.const 1033
    i32.const 27
    call $bindp
    i32.const 1133
    i32.const 31
    call $bindp
    i32.const 1131
    i32.const 35
    call $bindp
    i32.const 1135
    i32.const 39
    call $bindp
    i32.const 1127
    i32.const 43
    call $bindp
    i32.const 1129
    i32.const 47
    call $bindp
    i32.const 1092
    i32.const 51
    call $bindp
    i32.const 1086
    i32.const 55
    call $bindp
    i32.const 1080
    i32.const 59
    call $bindp
    i32.const 0
    local.get 0
    i32.const 8192
    local.get 0
    i32.const 8192
    i32.lt_u
    select
    i32.const 1152
    i32.add
    i32.store offset=13444
    i32.const 0
    i32.const 1152
    i32.store offset=13440
    i32.const 0
    i32.const 0
    i32.store offset=13448
    i32.const 0
    local.set 1
    i32.const 1
    local.set 2
    block  ;; label = @1
      block  ;; label = @2
        loop  ;; label = @3
          local.get 5
          local.set 4
          local.get 2
          local.set 6
          local.get 1
          local.set 8
          block  ;; label = @4
            i32.const 0
            i32.load offset=13440
            local.tee 1
            i32.const 0
            i32.load offset=13444
            local.tee 7
            i32.ge_u
            br_if 0 (;@4;)
            local.get 1
            local.set 1
            loop  ;; label = @5
              block  ;; label = @6
                block  ;; label = @7
                  local.get 1
                  local.tee 1
                  i32.load8_u
                  local.tee 5
                  i32.const -9
                  i32.add
                  local.tee 2
                  i32.const 23
                  i32.gt_u
                  br_if 0 (;@7;)
                  i32.const 1
                  local.get 2
                  i32.shl
                  i32.const 8388627
                  i32.and
                  i32.eqz
                  br_if 0 (;@7;)
                  i32.const 0
                  local.get 1
                  i32.const 1
                  i32.add
                  local.tee 1
                  i32.store offset=13440
                  local.get 1
                  local.set 1
                  br 1 (;@6;)
                end
                local.get 5
                i32.const 59
                i32.ne
                br_if 2 (;@4;)
                block  ;; label = @7
                  local.get 1
                  local.get 7
                  i32.lt_u
                  br_if 0 (;@7;)
                  local.get 1
                  local.set 1
                  br 1 (;@6;)
                end
                local.get 7
                local.get 1
                i32.sub
                local.set 2
                local.get 1
                local.set 5
                loop  ;; label = @7
                  local.get 2
                  local.set 2
                  block  ;; label = @8
                    local.get 5
                    local.tee 1
                    i32.load8_u
                    i32.const 10
                    i32.ne
                    br_if 0 (;@8;)
                    local.get 1
                    local.set 1
                    br 2 (;@6;)
                  end
                  i32.const 0
                  local.get 1
                  i32.const 1
                  i32.add
                  local.tee 1
                  i32.store offset=13440
                  local.get 2
                  i32.const -1
                  i32.add
                  local.tee 3
                  local.set 2
                  local.get 1
                  local.set 5
                  local.get 1
                  local.set 1
                  local.get 3
                  br_if 0 (;@7;)
                end
              end
              local.get 1
              local.tee 2
              local.set 1
              local.get 2
              local.get 7
              i32.lt_u
              br_if 0 (;@5;)
            end
          end
          block  ;; label = @4
            i32.const 0
            i32.load offset=13440
            local.get 7
            i32.lt_u
            br_if 0 (;@4;)
            local.get 8
            local.set 1
            br 2 (;@2;)
          end
          block  ;; label = @4
            block  ;; label = @5
              call $read_expr
              local.tee 1
              i32.const 11
              i32.ne
              br_if 0 (;@5;)
              block  ;; label = @6
                local.get 8
                i32.eqz
                br_if 0 (;@6;)
                i32.const 1
                local.set 1
                local.get 6
                local.set 2
                i32.const 2
                local.set 3
                local.get 4
                local.set 5
                br 2 (;@4;)
              end
              i32.const 1
              local.set 2
              i32.const 60
              local.set 1
              i32.const 0
              i32.load offset=13448
              local.set 5
              loop  ;; label = @6
                local.get 1
                local.set 3
                local.get 2
                local.set 1
                block  ;; label = @7
                  block  ;; label = @8
                    local.get 5
                    local.tee 2
                    i32.const 4095
                    i32.le_u
                    br_if 0 (;@8;)
                    local.get 2
                    local.set 5
                    br 1 (;@7;)
                  end
                  local.get 2
                  i32.const 9344
                  i32.add
                  local.get 3
                  i32.store8
                  i32.const 0
                  local.get 2
                  i32.const 1
                  i32.add
                  local.tee 2
                  i32.store offset=13448
                  local.get 2
                  local.set 5
                end
                local.get 1
                i32.const 1
                i32.add
                local.tee 3
                local.set 2
                local.get 1
                i32.const 1098
                i32.add
                i32.load8_u
                local.set 1
                local.get 5
                local.set 5
                local.get 3
                i32.const 8
                i32.ne
                br_if 0 (;@6;)
              end
              i32.const 0
              local.set 1
              local.get 6
              local.set 2
              i32.const 1
              local.set 3
              i32.const 0
              i32.load offset=13448
              local.set 5
              br 1 (;@4;)
            end
            block  ;; label = @5
              local.get 6
              br_if 0 (;@5;)
              block  ;; label = @6
                i32.const 0
                i32.load offset=13468
                local.tee 2
                i32.const 65535
                i32.gt_u
                br_if 0 (;@6;)
                i32.const 0
                local.get 2
                i32.const 1
                i32.add
                i32.store offset=13468
                local.get 2
                i32.const 2
                i32.shl
                i32.const 2135232
                i32.add
                i32.const 4
                i32.store
                br 1 (;@5;)
              end
              i32.const 0
              i32.const 1
              i32.store8 offset=13456
            end
            local.get 1
            i32.const 3
            call $compile
            block  ;; label = @5
              i32.const 0
              i32.load8_u offset=13452
              br_if 0 (;@5;)
              i32.const 1
              local.set 1
              i32.const 0
              local.set 2
              i32.const 0
              local.set 3
              local.get 4
              local.set 5
              i32.const 0
              i32.load8_u offset=13456
              i32.const 1
              i32.and
              i32.eqz
              br_if 1 (;@4;)
            end
            i32.const 1
            local.set 2
            i32.const 60
            local.set 1
            i32.const 0
            i32.load offset=13448
            local.set 5
            loop  ;; label = @5
              local.get 1
              local.set 3
              local.get 2
              local.set 1
              block  ;; label = @6
                block  ;; label = @7
                  local.get 5
                  local.tee 2
                  i32.const 4095
                  i32.le_u
                  br_if 0 (;@7;)
                  local.get 2
                  local.set 5
                  br 1 (;@6;)
                end
                local.get 2
                i32.const 9344
                i32.add
                local.get 3
                i32.store8
                i32.const 0
                local.get 2
                i32.const 1
                i32.add
                local.tee 2
                i32.store offset=13448
                local.get 2
                local.set 5
              end
              local.get 1
              i32.const 1
              i32.add
              local.tee 3
              local.set 2
              local.get 1
              i32.const 1098
              i32.add
              i32.load8_u
              local.set 1
              local.get 5
              local.set 5
              local.get 3
              i32.const 8
              i32.ne
              br_if 0 (;@5;)
            end
            i32.const 1
            local.set 1
            i32.const 0
            local.set 2
            i32.const 1
            local.set 3
            i32.const 0
            i32.load offset=13448
            local.set 5
          end
          local.get 1
          local.tee 7
          local.set 1
          local.get 2
          local.set 2
          local.get 5
          local.tee 6
          local.set 5
          local.get 3
          local.tee 3
          i32.eqz
          br_if 0 (;@3;)
        end
        local.get 7
        local.set 1
        local.get 3
        i32.const 2
        i32.ne
        br_if 1 (;@1;)
      end
      block  ;; label = @2
        local.get 1
        br_if 0 (;@2;)
        i32.const 1
        local.set 2
        i32.const 40
        local.set 1
        i32.const 0
        i32.load offset=13448
        local.set 5
        loop  ;; label = @3
          local.get 1
          local.set 3
          local.get 2
          local.set 1
          block  ;; label = @4
            block  ;; label = @5
              local.get 5
              local.tee 2
              i32.const 4095
              i32.le_u
              br_if 0 (;@5;)
              local.get 2
              local.set 5
              br 1 (;@4;)
            end
            local.get 2
            i32.const 9344
            i32.add
            local.get 3
            i32.store8
            i32.const 0
            local.get 2
            i32.const 1
            i32.add
            local.tee 2
            i32.store offset=13448
            local.get 2
            local.set 5
          end
          local.get 1
          i32.const 1
          i32.add
          local.tee 3
          local.set 2
          local.get 1
          i32.const 1137
          i32.add
          i32.load8_u
          local.set 1
          local.get 5
          local.set 5
          local.get 3
          i32.const 3
          i32.ne
          br_if 0 (;@3;)
        end
        i32.const 0
        i32.load offset=13448
        return
      end
      block  ;; label = @2
        block  ;; label = @3
          i32.const 0
          i32.load offset=13468
          local.tee 1
          i32.const 65535
          i32.gt_u
          br_if 0 (;@3;)
          i32.const 0
          local.get 1
          i32.const 1
          i32.add
          i32.store offset=13468
          local.get 1
          i32.const 2
          i32.shl
          i32.const 2135232
          i32.add
          i32.const 10
          i32.store
          br 1 (;@2;)
        end
        i32.const 0
        i32.const 1
        i32.store8 offset=13456
      end
      call $run
      call $print_val
      i32.const 0
      i32.load offset=13448
      return
    end
    local.get 6)
  (func $bindp (type 2) (param i32 i32)
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0
    local.set 2
    loop  ;; label = @1
      local.get 2
      local.tee 3
      i32.const 1
      i32.add
      local.tee 4
      local.set 2
      local.get 0
      local.get 3
      i32.add
      i32.load8_u
      br_if 0 (;@1;)
    end
    local.get 4
    i32.const -1
    i32.add
    local.tee 5
    i32.const 16
    local.get 5
    i32.const 16
    i32.lt_u
    select
    local.set 6
    i32.const 0
    i32.load offset=13464
    local.tee 7
    i32.const 0
    i32.ne
    local.set 2
    block  ;; label = @1
      block  ;; label = @2
        local.get 7
        br_if 0 (;@2;)
        local.get 2
        local.set 3
        i32.const 0
        local.set 2
        br 1 (;@1;)
      end
      i32.const 17600
      local.set 3
      local.get 2
      local.set 8
      i32.const 0
      local.set 9
      loop  ;; label = @2
        local.get 8
        local.set 10
        local.get 3
        local.set 11
        i32.const 0
        local.set 2
        block  ;; label = @3
          local.get 9
          local.tee 12
          i32.const 2
          i32.shl
          i32.const 33984
          i32.add
          i32.load
          local.tee 8
          local.get 6
          i32.ne
          br_if 0 (;@3;)
          block  ;; label = @4
            block  ;; label = @5
              local.get 8
              br_if 0 (;@5;)
              local.get 8
              i32.eqz
              local.set 2
              br 1 (;@4;)
            end
            i32.const 0
            local.set 2
            local.get 12
            i32.const 4
            i32.shl
            i32.const 17600
            i32.add
            i32.load8_u
            local.get 0
            i32.load8_u
            i32.ne
            br_if 0 (;@4;)
            i32.const 1
            local.set 3
            block  ;; label = @5
              loop  ;; label = @6
                block  ;; label = @7
                  local.get 8
                  local.get 3
                  local.tee 2
                  i32.ne
                  br_if 0 (;@7;)
                  local.get 8
                  local.set 2
                  br 2 (;@5;)
                end
                local.get 2
                i32.const 1
                i32.add
                local.tee 9
                local.set 3
                local.get 11
                local.get 2
                i32.add
                i32.load8_u
                local.get 0
                local.get 2
                i32.add
                i32.load8_u
                i32.eq
                br_if 0 (;@6;)
              end
              local.get 9
              i32.const -1
              i32.add
              local.set 2
            end
            local.get 2
            local.get 8
            i32.ge_u
            local.set 2
          end
          local.get 2
          local.set 2
        end
        block  ;; label = @3
          local.get 2
          i32.eqz
          br_if 0 (;@3;)
          local.get 10
          local.set 3
          local.get 12
          i32.const 1073741823
          i32.and
          local.set 2
          br 2 (;@1;)
        end
        local.get 11
        i32.const 16
        i32.add
        local.set 3
        local.get 12
        i32.const 1
        i32.add
        local.tee 2
        local.get 7
        i32.lt_u
        local.tee 11
        local.set 8
        local.get 2
        local.set 9
        local.get 2
        local.get 7
        i32.ne
        br_if 0 (;@2;)
      end
      local.get 11
      local.set 3
      i32.const 0
      local.set 2
    end
    local.get 2
    local.set 2
    block  ;; label = @1
      local.get 3
      i32.const 1
      i32.and
      br_if 0 (;@1;)
      i32.const 2
      local.set 2
      local.get 7
      i32.const 1023
      i32.gt_u
      br_if 0 (;@1;)
      i32.const 0
      local.get 7
      i32.const 1
      i32.add
      i32.store offset=13464
      block  ;; label = @2
        local.get 4
        i32.const 1
        i32.eq
        br_if 0 (;@2;)
        local.get 0
        local.set 2
        local.get 7
        i32.const 4
        i32.shl
        i32.const 17600
        i32.add
        local.set 3
        local.get 5
        i32.const 16
        local.get 5
        i32.const 16
        i32.lt_u
        select
        local.set 0
        loop  ;; label = @3
          local.get 3
          local.tee 3
          local.get 2
          local.tee 2
          i32.load8_u
          i32.store8
          local.get 2
          i32.const 1
          i32.add
          local.set 2
          local.get 3
          i32.const 1
          i32.add
          local.set 3
          local.get 0
          i32.const -1
          i32.add
          local.tee 8
          local.set 0
          local.get 8
          br_if 0 (;@3;)
        end
      end
      local.get 7
      i32.const 2
      i32.shl
      i32.const 33984
      i32.add
      local.get 6
      i32.store
      local.get 7
      local.set 2
    end
    local.get 2
    i32.const 2
    i32.shl
    i32.const 13472
    i32.add
    local.get 1
    i32.store)
  (func $read_expr (type 0) (result i32)
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    block  ;; label = @1
      i32.const 0
      i32.load offset=13440
      local.tee 0
      i32.const 0
      i32.load offset=13444
      local.tee 1
      i32.ge_u
      br_if 0 (;@1;)
      local.get 0
      local.set 0
      loop  ;; label = @2
        block  ;; label = @3
          block  ;; label = @4
            local.get 0
            local.tee 0
            i32.load8_u
            local.tee 2
            i32.const -9
            i32.add
            local.tee 3
            i32.const 23
            i32.gt_u
            br_if 0 (;@4;)
            i32.const 1
            local.get 3
            i32.shl
            i32.const 8388627
            i32.and
            i32.eqz
            br_if 0 (;@4;)
            i32.const 0
            local.get 0
            i32.const 1
            i32.add
            local.tee 0
            i32.store offset=13440
            local.get 0
            local.set 0
            br 1 (;@3;)
          end
          local.get 2
          i32.const 59
          i32.ne
          br_if 2 (;@1;)
          block  ;; label = @4
            local.get 0
            local.get 1
            i32.lt_u
            br_if 0 (;@4;)
            local.get 0
            local.set 0
            br 1 (;@3;)
          end
          local.get 1
          local.get 0
          i32.sub
          local.set 3
          local.get 0
          local.set 2
          loop  ;; label = @4
            local.get 3
            local.set 3
            block  ;; label = @5
              local.get 2
              local.tee 0
              i32.load8_u
              i32.const 10
              i32.ne
              br_if 0 (;@5;)
              local.get 0
              local.set 0
              br 2 (;@3;)
            end
            i32.const 0
            local.get 0
            i32.const 1
            i32.add
            local.tee 0
            i32.store offset=13440
            local.get 3
            i32.const -1
            i32.add
            local.tee 4
            local.set 3
            local.get 0
            local.set 2
            local.get 0
            local.set 0
            local.get 4
            br_if 0 (;@4;)
          end
        end
        local.get 0
        local.tee 3
        local.set 0
        local.get 3
        local.get 1
        i32.lt_u
        br_if 0 (;@2;)
      end
    end
    block  ;; label = @1
      i32.const 0
      i32.load offset=13440
      local.tee 5
      local.get 1
      i32.lt_u
      br_if 0 (;@1;)
      i32.const 11
      return
    end
    block  ;; label = @1
      block  ;; label = @2
        block  ;; label = @3
          block  ;; label = @4
            local.get 5
            i32.load8_u
            local.tee 6
            i32.const -39
            i32.add
            br_table 2 (;@2;) 0 (;@4;) 1 (;@3;)
          end
          i32.const 0
          local.get 5
          i32.const 1
          i32.add
          i32.store offset=13440
          i32.const 3
          local.set 3
          i32.const 3
          local.set 2
          loop  ;; label = @4
            local.get 2
            local.set 7
            local.get 3
            local.set 8
            block  ;; label = @5
              i32.const 0
              i32.load offset=13440
              local.tee 0
              local.get 1
              i32.ge_u
              br_if 0 (;@5;)
              local.get 0
              local.set 0
              loop  ;; label = @6
                block  ;; label = @7
                  block  ;; label = @8
                    local.get 0
                    local.tee 0
                    i32.load8_u
                    local.tee 2
                    i32.const -9
                    i32.add
                    local.tee 3
                    i32.const 23
                    i32.gt_u
                    br_if 0 (;@8;)
                    i32.const 1
                    local.get 3
                    i32.shl
                    i32.const 8388627
                    i32.and
                    i32.eqz
                    br_if 0 (;@8;)
                    i32.const 0
                    local.get 0
                    i32.const 1
                    i32.add
                    local.tee 0
                    i32.store offset=13440
                    local.get 0
                    local.set 0
                    br 1 (;@7;)
                  end
                  local.get 2
                  i32.const 59
                  i32.ne
                  br_if 2 (;@5;)
                  block  ;; label = @8
                    local.get 0
                    local.get 1
                    i32.lt_u
                    br_if 0 (;@8;)
                    local.get 0
                    local.set 0
                    br 1 (;@7;)
                  end
                  local.get 1
                  local.get 0
                  i32.sub
                  local.set 3
                  local.get 0
                  local.set 2
                  loop  ;; label = @8
                    local.get 3
                    local.set 3
                    block  ;; label = @9
                      local.get 2
                      local.tee 0
                      i32.load8_u
                      i32.const 10
                      i32.ne
                      br_if 0 (;@9;)
                      local.get 0
                      local.set 0
                      br 2 (;@7;)
                    end
                    i32.const 0
                    local.get 0
                    i32.const 1
                    i32.add
                    local.tee 0
                    i32.store offset=13440
                    local.get 3
                    i32.const -1
                    i32.add
                    local.tee 4
                    local.set 3
                    local.get 0
                    local.set 2
                    local.get 0
                    local.set 0
                    local.get 4
                    br_if 0 (;@8;)
                  end
                end
                local.get 0
                local.tee 3
                local.set 0
                local.get 3
                local.get 1
                i32.lt_u
                br_if 0 (;@6;)
              end
            end
            block  ;; label = @5
              i32.const 0
              i32.load offset=13440
              local.tee 0
              local.get 1
              i32.lt_u
              br_if 0 (;@5;)
              local.get 7
              return
            end
            block  ;; label = @5
              local.get 0
              i32.load8_u
              i32.const 41
              i32.ne
              br_if 0 (;@5;)
              i32.const 0
              local.get 0
              i32.const 1
              i32.add
              i32.store offset=13440
              local.get 7
              return
            end
            call $read_expr
            local.set 3
            block  ;; label = @5
              block  ;; label = @6
                i32.const 0
                i32.load offset=13460
                local.tee 0
                i32.const 262144
                i32.lt_u
                br_if 0 (;@6;)
                i32.const 0
                i32.const 1
                i32.store8 offset=13452
                i32.const 11
                local.set 0
                br 1 (;@5;)
              end
              i32.const 0
              local.get 0
              i32.const 1
              i32.add
              i32.store offset=13460
              local.get 0
              i32.const 3
              i32.shl
              local.tee 2
              i32.const 38084
              i32.add
              i32.const 3
              i32.store
              local.get 2
              i32.const 38080
              i32.add
              local.get 3
              i32.store
              local.get 0
              i32.const 2
              i32.shl
              i32.const 1
              i32.or
              local.set 0
            end
            block  ;; label = @5
              block  ;; label = @6
                block  ;; label = @7
                  local.get 0
                  local.tee 0
                  i32.const 11
                  i32.ne
                  br_if 0 (;@7;)
                  local.get 8
                  local.set 3
                  br 1 (;@6;)
                end
                local.get 0
                local.set 3
                local.get 0
                local.set 2
                local.get 7
                i32.const 3
                i32.eq
                br_if 1 (;@5;)
                local.get 8
                i32.const 1
                i32.shl
                i32.const -8
                i32.and
                i32.const 38084
                i32.add
                local.get 0
                i32.store
                local.get 0
                local.set 3
              end
              local.get 7
              local.set 2
            end
            local.get 3
            local.set 3
            local.get 2
            local.set 2
            i32.const 11
            local.set 4
            local.get 0
            i32.const 11
            i32.ne
            br_if 0 (;@4;)
            br 3 (;@1;)
          end
        end
        local.get 5
        local.set 3
        loop  ;; label = @3
          i32.const 1
          local.set 0
          block  ;; label = @4
            block  ;; label = @5
              local.get 3
              local.tee 3
              i32.load8_u
              local.tee 2
              i32.const -9
              i32.add
              br_table 1 (;@4;) 1 (;@4;) 0 (;@5;) 0 (;@5;) 1 (;@4;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 1 (;@4;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 1 (;@4;) 1 (;@4;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 0 (;@5;) 1 (;@4;) 0 (;@5;)
            end
            local.get 2
            i32.eqz
            local.set 0
          end
          block  ;; label = @4
            block  ;; label = @5
              local.get 0
              i32.eqz
              br_if 0 (;@5;)
              local.get 3
              local.set 2
              br 1 (;@4;)
            end
            i32.const 0
            local.get 3
            i32.const 1
            i32.add
            local.tee 0
            i32.store offset=13440
            local.get 0
            local.set 3
            local.get 0
            local.set 2
            local.get 0
            local.get 1
            i32.ne
            br_if 1 (;@3;)
          end
        end
        block  ;; label = @3
          block  ;; label = @4
            local.get 2
            local.get 5
            i32.sub
            local.tee 9
            br_if 0 (;@4;)
            i32.const 1
            local.set 0
            local.get 5
            local.set 3
            i32.const 0
            local.set 2
            br 1 (;@3;)
          end
          block  ;; label = @4
            block  ;; label = @5
              local.get 6
              i32.const -43
              i32.add
              br_table 1 (;@4;) 0 (;@5;) 1 (;@4;) 0 (;@5;)
            end
            i32.const 1
            local.set 0
            local.get 5
            local.set 3
            local.get 9
            local.set 2
            br 1 (;@3;)
          end
          local.get 6
          i32.const 45
          i32.ne
          local.set 0
          local.get 5
          i32.const 1
          i32.add
          local.set 3
          local.get 9
          i32.const -1
          i32.add
          local.set 2
        end
        local.get 3
        local.set 3
        local.get 0
        local.set 10
        block  ;; label = @3
          local.get 2
          local.tee 2
          i32.eqz
          br_if 0 (;@3;)
          local.get 3
          local.set 0
          local.get 2
          i32.const -1
          i32.add
          local.set 4
          i32.const 0
          local.set 1
          i32.const 1
          local.set 7
          block  ;; label = @4
            loop  ;; label = @5
              local.get 4
              local.set 3
              local.get 1
              local.tee 2
              local.get 2
              i32.const 10
              i32.mul
              local.get 0
              local.tee 4
              i32.load8_u
              local.tee 0
              i32.add
              i32.const -48
              i32.add
              local.get 0
              i32.const -58
              i32.add
              i32.const 255
              i32.and
              i32.const 246
              i32.lt_u
              local.tee 0
              select
              local.set 2
              i32.const 0
              local.get 7
              local.get 0
              select
              local.set 8
              local.get 0
              br_if 1 (;@4;)
              local.get 4
              i32.const 1
              i32.add
              local.set 0
              local.get 3
              i32.const -1
              i32.add
              local.set 4
              local.get 2
              local.set 1
              local.get 8
              local.set 7
              local.get 3
              br_if 0 (;@5;)
            end
          end
          local.get 8
          i32.eqz
          br_if 0 (;@3;)
          local.get 2
          i32.const 0
          local.get 2
          i32.sub
          local.get 10
          select
          i32.const 2
          i32.shl
          return
        end
        local.get 9
        i32.const 16
        local.get 9
        i32.const 16
        i32.lt_u
        select
        local.set 10
        i32.const 0
        i32.load offset=13464
        local.tee 8
        i32.const 0
        i32.ne
        local.set 0
        block  ;; label = @3
          block  ;; label = @4
            local.get 8
            br_if 0 (;@4;)
            local.get 0
            local.set 11
            br 1 (;@3;)
          end
          i32.const 17600
          local.set 3
          local.get 0
          local.set 2
          i32.const 0
          local.set 1
          loop  ;; label = @4
            local.get 2
            local.set 11
            local.get 3
            local.set 4
            i32.const 0
            local.set 0
            block  ;; label = @5
              local.get 1
              local.tee 7
              i32.const 2
              i32.shl
              i32.const 33984
              i32.add
              i32.load
              local.tee 2
              local.get 10
              i32.ne
              br_if 0 (;@5;)
              block  ;; label = @6
                block  ;; label = @7
                  local.get 2
                  br_if 0 (;@7;)
                  local.get 2
                  i32.eqz
                  local.set 0
                  br 1 (;@6;)
                end
                i32.const 0
                local.set 0
                local.get 7
                i32.const 4
                i32.shl
                i32.const 17600
                i32.add
                i32.load8_u
                local.get 6
                i32.ne
                br_if 0 (;@6;)
                i32.const 1
                local.set 3
                block  ;; label = @7
                  loop  ;; label = @8
                    block  ;; label = @9
                      local.get 2
                      local.get 3
                      local.tee 0
                      i32.ne
                      br_if 0 (;@9;)
                      local.get 2
                      local.set 0
                      br 2 (;@7;)
                    end
                    local.get 0
                    i32.const 1
                    i32.add
                    local.tee 1
                    local.set 3
                    local.get 4
                    local.get 0
                    i32.add
                    i32.load8_u
                    local.get 5
                    local.get 0
                    i32.add
                    i32.load8_u
                    i32.eq
                    br_if 0 (;@8;)
                  end
                  local.get 1
                  i32.const -1
                  i32.add
                  local.set 0
                end
                local.get 0
                local.get 2
                i32.ge_u
                local.set 0
              end
              local.get 0
              local.set 0
            end
            block  ;; label = @5
              local.get 0
              i32.eqz
              br_if 0 (;@5;)
              local.get 11
              local.set 11
              local.get 7
              i32.const 2
              i32.shl
              i32.const 2
              i32.or
              local.set 4
              br 2 (;@3;)
            end
            local.get 4
            i32.const 16
            i32.add
            local.set 3
            local.get 7
            i32.const 1
            i32.add
            local.tee 0
            local.get 8
            i32.lt_u
            local.tee 4
            local.set 2
            local.get 0
            local.set 1
            local.get 4
            local.set 11
            local.get 0
            local.get 8
            i32.ne
            br_if 0 (;@4;)
          end
        end
        local.get 4
        local.set 4
        local.get 11
        i32.const 1
        i32.and
        br_if 1 (;@1;)
        i32.const 11
        local.set 4
        local.get 8
        i32.const 1023
        i32.gt_u
        br_if 1 (;@1;)
        i32.const 0
        local.get 8
        i32.const 1
        i32.add
        i32.store offset=13464
        block  ;; label = @3
          local.get 9
          i32.eqz
          br_if 0 (;@3;)
          local.get 5
          local.set 0
          local.get 8
          i32.const 4
          i32.shl
          i32.const 17600
          i32.add
          local.set 3
          local.get 10
          local.set 2
          loop  ;; label = @4
            local.get 3
            local.tee 3
            local.get 0
            local.tee 0
            i32.load8_u
            i32.store8
            local.get 0
            i32.const 1
            i32.add
            local.set 0
            local.get 3
            i32.const 1
            i32.add
            local.set 3
            local.get 2
            i32.const -1
            i32.add
            local.tee 4
            local.set 2
            local.get 4
            br_if 0 (;@4;)
          end
        end
        local.get 8
        i32.const 2
        i32.shl
        local.tee 0
        i32.const 33984
        i32.add
        local.get 10
        i32.store
        local.get 0
        i32.const 2
        i32.or
        return
      end
      i32.const 0
      local.get 5
      i32.const 1
      i32.add
      i32.store offset=13440
      call $read_expr
      local.set 3
      block  ;; label = @2
        block  ;; label = @3
          i32.const 0
          i32.load offset=13460
          local.tee 0
          i32.const 262144
          i32.lt_u
          br_if 0 (;@3;)
          i32.const 0
          i32.const 1
          i32.store8 offset=13452
          i32.const 11
          local.set 0
          br 1 (;@2;)
        end
        i32.const 0
        local.get 0
        i32.const 1
        i32.add
        i32.store offset=13460
        local.get 0
        i32.const 3
        i32.shl
        local.tee 2
        i32.const 38084
        i32.add
        i32.const 3
        i32.store
        local.get 2
        i32.const 38080
        i32.add
        local.get 3
        i32.store
        local.get 0
        i32.const 2
        i32.shl
        i32.const 1
        i32.or
        local.set 0
      end
      local.get 0
      local.set 3
      block  ;; label = @2
        i32.const 0
        i32.load offset=13460
        local.tee 0
        i32.const 262144
        i32.lt_u
        br_if 0 (;@2;)
        i32.const 0
        i32.const 1
        i32.store8 offset=13452
        i32.const 11
        return
      end
      i32.const 0
      i32.load8_u offset=17568
      local.set 2
      i32.const 0
      local.get 0
      i32.const 1
      i32.add
      i32.store offset=13460
      local.get 0
      i32.const 3
      i32.shl
      local.tee 4
      i32.const 38084
      i32.add
      local.get 3
      i32.store
      local.get 4
      i32.const 38080
      i32.add
      i32.const 2
      i32.const 0
      local.get 2
      select
      i32.store
      local.get 0
      i32.const 2
      i32.shl
      i32.const 1
      i32.or
      local.set 4
    end
    local.get 4)
  (func $compile (type 2) (param i32 i32)
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32)
    local.get 0
    local.set 2
    loop  ;; label = @1
      block  ;; label = @2
        block  ;; label = @3
          block  ;; label = @4
            block  ;; label = @5
              block  ;; label = @6
                local.get 2
                local.tee 2
                i32.const 3
                i32.and
                local.tee 3
                br_table 0 (;@6;) 2 (;@4;) 1 (;@5;) 0 (;@6;) 2 (;@4;)
              end
              block  ;; label = @6
                block  ;; label = @7
                  i32.const 0
                  i32.load offset=13468
                  local.tee 0
                  i32.const 65535
                  i32.gt_u
                  br_if 0 (;@7;)
                  i32.const 0
                  local.get 0
                  i32.const 1
                  i32.add
                  i32.store offset=13468
                  local.get 0
                  i32.const 2
                  i32.shl
                  i32.const 2135232
                  i32.add
                  i32.const 0
                  i32.store
                  br 1 (;@6;)
                end
                i32.const 0
                i32.const 1
                i32.store8 offset=13456
              end
              i32.const 0
              i32.load offset=13468
              local.tee 0
              i32.const 65535
              i32.gt_u
              br_if 2 (;@3;)
              i32.const 0
              local.get 0
              i32.const 1
              i32.add
              i32.store offset=13468
              local.get 0
              i32.const 2
              i32.shl
              i32.const 2135232
              i32.add
              local.get 2
              i32.store
              return
            end
            local.get 1
            i32.const 3
            i32.and
            local.tee 0
            i32.const 1
            i32.eq
            local.set 3
            block  ;; label = @5
              block  ;; label = @6
                local.get 0
                i32.const 1
                i32.eq
                br_if 0 (;@6;)
                local.get 3
                local.set 4
                br 1 (;@5;)
              end
              local.get 3
              local.set 3
              local.get 1
              local.set 5
              i32.const 0
              local.set 1
              loop  ;; label = @6
                local.get 1
                local.set 1
                local.get 3
                local.set 4
                block  ;; label = @7
                  local.get 5
                  i32.const 2
                  i32.shr_u
                  i32.const 3
                  i32.shl
                  local.tee 6
                  i32.const 38080
                  i32.add
                  i32.load
                  local.tee 0
                  i32.const 3
                  i32.and
                  i32.const 1
                  i32.ne
                  br_if 0 (;@7;)
                  local.get 0
                  local.set 0
                  i32.const 0
                  local.set 3
                  loop  ;; label = @8
                    local.get 3
                    local.set 3
                    block  ;; label = @9
                      local.get 0
                      i32.const 2
                      i32.shr_u
                      i32.const 3
                      i32.shl
                      local.tee 0
                      i32.const 38080
                      i32.add
                      i32.load
                      local.get 2
                      i32.ne
                      br_if 0 (;@9;)
                      local.get 1
                      local.set 6
                      local.get 3
                      local.set 7
                      local.get 4
                      local.set 4
                      br 4 (;@5;)
                    end
                    local.get 0
                    i32.const 38084
                    i32.add
                    i32.load
                    local.tee 5
                    local.set 0
                    local.get 3
                    i32.const 1
                    i32.add
                    local.set 3
                    local.get 5
                    i32.const 3
                    i32.and
                    i32.const 1
                    i32.eq
                    br_if 0 (;@8;)
                  end
                end
                local.get 6
                i32.const 38084
                i32.add
                i32.load
                local.tee 5
                i32.const 3
                i32.and
                i32.const 1
                i32.eq
                local.tee 0
                local.set 3
                local.get 5
                local.set 5
                local.get 1
                i32.const 1
                i32.add
                local.set 1
                local.get 0
                local.set 4
                local.get 0
                br_if 0 (;@6;)
              end
            end
            local.get 7
            local.set 3
            local.get 6
            local.set 5
            i32.const 0
            i32.load offset=13468
            local.set 0
            block  ;; label = @5
              local.get 4
              i32.const 1
              i32.and
              i32.eqz
              br_if 0 (;@5;)
              block  ;; label = @6
                block  ;; label = @7
                  local.get 0
                  i32.const 65535
                  i32.gt_u
                  br_if 0 (;@7;)
                  i32.const 0
                  local.get 0
                  i32.const 1
                  i32.add
                  i32.store offset=13468
                  local.get 0
                  i32.const 2
                  i32.shl
                  i32.const 2135232
                  i32.add
                  i32.const 1
                  i32.store
                  br 1 (;@6;)
                end
                i32.const 0
                i32.const 1
                i32.store8 offset=13456
              end
              block  ;; label = @6
                block  ;; label = @7
                  i32.const 0
                  i32.load offset=13468
                  local.tee 0
                  i32.const 65535
                  i32.gt_u
                  br_if 0 (;@7;)
                  i32.const 0
                  local.get 0
                  i32.const 1
                  i32.add
                  i32.store offset=13468
                  local.get 0
                  i32.const 2
                  i32.shl
                  i32.const 2135232
                  i32.add
                  local.get 5
                  i32.store
                  br 1 (;@6;)
                end
                i32.const 0
                i32.const 1
                i32.store8 offset=13456
              end
              block  ;; label = @6
                i32.const 0
                i32.load offset=13468
                local.tee 0
                i32.const 65535
                i32.gt_u
                br_if 0 (;@6;)
                i32.const 0
                local.get 0
                i32.const 1
                i32.add
                i32.store offset=13468
                local.get 0
                i32.const 2
                i32.shl
                i32.const 2135232
                i32.add
                local.get 3
                i32.store
                return
              end
              i32.const 0
              i32.const 1
              i32.store8 offset=13456
              return
            end
            block  ;; label = @5
              block  ;; label = @6
                local.get 0
                i32.const 65535
                i32.gt_u
                br_if 0 (;@6;)
                i32.const 0
                local.get 0
                i32.const 1
                i32.add
                i32.store offset=13468
                local.get 0
                i32.const 2
                i32.shl
                i32.const 2135232
                i32.add
                i32.const 2
                i32.store
                br 1 (;@5;)
              end
              i32.const 0
              i32.const 1
              i32.store8 offset=13456
            end
            block  ;; label = @5
              i32.const 0
              i32.load offset=13468
              local.tee 0
              i32.const 65535
              i32.gt_u
              br_if 0 (;@5;)
              i32.const 0
              local.get 0
              i32.const 1
              i32.add
              i32.store offset=13468
              local.get 0
              i32.const 2
              i32.shl
              i32.const 2135232
              i32.add
              local.get 2
              i32.const 2
              i32.shr_u
              i32.store
              return
            end
            i32.const 0
            i32.const 1
            i32.store8 offset=13456
            return
          end
          i32.const 3
          local.set 0
          block  ;; label = @4
            local.get 3
            i32.const 1
            i32.ne
            local.tee 5
            br_if 0 (;@4;)
            local.get 2
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38080
            i32.add
            i32.load
            local.set 0
          end
          block  ;; label = @4
            local.get 0
            local.tee 0
            i32.const 2
            i32.const 0
            i32.const 0
            i32.load8_u offset=17568
            select
            i32.ne
            br_if 0 (;@4;)
            block  ;; label = @5
              block  ;; label = @6
                i32.const 0
                i32.load offset=13468
                local.tee 0
                i32.const 65535
                i32.gt_u
                br_if 0 (;@6;)
                i32.const 0
                local.get 0
                i32.const 1
                i32.add
                i32.store offset=13468
                local.get 0
                i32.const 2
                i32.shl
                i32.const 2135232
                i32.add
                i32.const 0
                i32.store
                br 1 (;@5;)
              end
              i32.const 0
              i32.const 1
              i32.store8 offset=13456
            end
            i32.const 3
            local.set 0
            block  ;; label = @5
              local.get 3
              i32.const 1
              i32.ne
              br_if 0 (;@5;)
              local.get 2
              i32.const 1
              i32.shl
              i32.const -8
              i32.and
              i32.const 38084
              i32.add
              i32.load
              local.set 0
            end
            i32.const 3
            local.set 2
            block  ;; label = @5
              local.get 0
              local.tee 0
              i32.const 3
              i32.and
              i32.const 1
              i32.ne
              br_if 0 (;@5;)
              local.get 0
              i32.const 1
              i32.shl
              i32.const -8
              i32.and
              i32.const 38080
              i32.add
              i32.load
              local.set 2
            end
            local.get 2
            local.set 2
            block  ;; label = @5
              i32.const 0
              i32.load offset=13468
              local.tee 0
              i32.const 65535
              i32.gt_u
              br_if 0 (;@5;)
              i32.const 0
              local.get 0
              i32.const 1
              i32.add
              i32.store offset=13468
              local.get 0
              i32.const 2
              i32.shl
              i32.const 2135232
              i32.add
              local.get 2
              i32.store
              return
            end
            i32.const 0
            i32.const 1
            i32.store8 offset=13456
            return
          end
          local.get 0
          i32.const 6
          i32.const 0
          i32.const 0
          i32.load8_u offset=17572
          select
          i32.ne
          br_if 1 (;@2;)
          i32.const 3
          local.set 0
          block  ;; label = @4
            local.get 3
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 2
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 0
          end
          i32.const 3
          local.set 5
          block  ;; label = @4
            local.get 0
            local.tee 0
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 0
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38080
            i32.add
            i32.load
            local.set 5
          end
          local.get 5
          local.get 1
          call $compile
          block  ;; label = @4
            block  ;; label = @5
              i32.const 0
              i32.load offset=13468
              local.tee 0
              i32.const 65535
              i32.gt_u
              br_if 0 (;@5;)
              i32.const 0
              local.get 0
              i32.const 1
              i32.add
              i32.store offset=13468
              local.get 0
              i32.const 2
              i32.shl
              i32.const 2135232
              i32.add
              i32.const 6
              i32.store
              br 1 (;@4;)
            end
            i32.const 0
            i32.const 1
            i32.store8 offset=13456
          end
          block  ;; label = @4
            block  ;; label = @5
              i32.const 0
              i32.load offset=13468
              local.tee 0
              i32.const 65535
              i32.gt_u
              br_if 0 (;@5;)
              i32.const 0
              local.get 0
              i32.const 1
              i32.add
              i32.store offset=13468
              local.get 0
              i32.const 2
              i32.shl
              i32.const 2135232
              i32.add
              i32.const 0
              i32.store
              br 1 (;@4;)
            end
            i32.const 0
            i32.const 1
            i32.store8 offset=13456
          end
          i32.const 3
          local.set 5
          block  ;; label = @4
            local.get 3
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 2
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 5
          end
          i32.const 3
          local.set 6
          block  ;; label = @4
            local.get 5
            local.tee 5
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 5
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 6
          end
          i32.const 3
          local.set 5
          block  ;; label = @4
            local.get 6
            local.tee 6
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 6
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38080
            i32.add
            i32.load
            local.set 5
          end
          local.get 5
          local.get 1
          call $compile
          block  ;; label = @4
            block  ;; label = @5
              i32.const 0
              i32.load offset=13468
              local.tee 5
              i32.const 65535
              i32.gt_u
              br_if 0 (;@5;)
              i32.const 0
              local.get 5
              i32.const 1
              i32.add
              i32.store offset=13468
              local.get 5
              i32.const 2
              i32.shl
              i32.const 2135232
              i32.add
              i32.const 5
              i32.store
              br 1 (;@4;)
            end
            i32.const 0
            i32.const 1
            i32.store8 offset=13456
          end
          block  ;; label = @4
            block  ;; label = @5
              i32.const 0
              i32.load offset=13468
              local.tee 5
              i32.const 65535
              i32.gt_u
              br_if 0 (;@5;)
              i32.const 0
              local.get 5
              i32.const 1
              i32.add
              i32.store offset=13468
              local.get 5
              i32.const 2
              i32.shl
              i32.const 2135232
              i32.add
              i32.const 0
              i32.store
              br 1 (;@4;)
            end
            i32.const 0
            i32.const 1
            i32.store8 offset=13456
          end
          local.get 0
          i32.const 2
          i32.shl
          i32.const 2135232
          i32.add
          i32.const 0
          i32.load offset=13468
          i32.store
          i32.const 3
          local.set 0
          block  ;; label = @4
            local.get 3
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 2
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 0
          end
          i32.const 3
          local.set 2
          block  ;; label = @4
            local.get 0
            local.tee 0
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 0
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 2
          end
          i32.const 3
          local.set 0
          block  ;; label = @4
            local.get 2
            local.tee 2
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 2
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 0
          end
          i32.const 3
          local.set 2
          block  ;; label = @4
            local.get 0
            local.tee 0
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 0
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38080
            i32.add
            i32.load
            local.set 2
          end
          local.get 2
          local.get 1
          call $compile
          local.get 5
          i32.const 2
          i32.shl
          i32.const 2135232
          i32.add
          i32.const 0
          i32.load offset=13468
          i32.store
          return
        end
        i32.const 0
        i32.const 1
        i32.store8 offset=13456
        return
      end
      block  ;; label = @2
        local.get 0
        i32.const 0
        i32.load offset=17576
        i32.ne
        br_if 0 (;@2;)
        i32.const 3
        local.set 0
        block  ;; label = @3
          local.get 3
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 2
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38084
          i32.add
          i32.load
          local.set 0
        end
        i32.const 3
        local.set 5
        block  ;; label = @3
          local.get 0
          local.tee 0
          i32.const 3
          i32.and
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 0
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38084
          i32.add
          i32.load
          local.set 5
        end
        i32.const 3
        local.set 0
        block  ;; label = @3
          local.get 5
          local.tee 5
          i32.const 3
          i32.and
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 5
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38080
          i32.add
          i32.load
          local.set 0
        end
        local.get 0
        local.get 1
        call $compile
        block  ;; label = @3
          block  ;; label = @4
            i32.const 0
            i32.load offset=13468
            local.tee 0
            i32.const 65535
            i32.gt_u
            br_if 0 (;@4;)
            i32.const 0
            local.get 0
            i32.const 1
            i32.add
            i32.store offset=13468
            local.get 0
            i32.const 2
            i32.shl
            i32.const 2135232
            i32.add
            i32.const 3
            i32.store
            br 1 (;@3;)
          end
          i32.const 0
          i32.const 1
          i32.store8 offset=13456
        end
        i32.const 3
        local.set 0
        block  ;; label = @3
          local.get 3
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 2
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38084
          i32.add
          i32.load
          local.set 0
        end
        i32.const 0
        local.set 2
        block  ;; label = @3
          local.get 0
          local.tee 0
          i32.const 3
          i32.and
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 0
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38080
          i32.add
          i32.load
          i32.const 2
          i32.shr_u
          local.set 2
        end
        local.get 2
        local.set 2
        block  ;; label = @3
          i32.const 0
          i32.load offset=13468
          local.tee 0
          i32.const 65535
          i32.gt_u
          br_if 0 (;@3;)
          i32.const 0
          local.get 0
          i32.const 1
          i32.add
          i32.store offset=13468
          local.get 0
          i32.const 2
          i32.shl
          i32.const 2135232
          i32.add
          local.get 2
          i32.store
          return
        end
        i32.const 0
        i32.const 1
        i32.store8 offset=13456
        return
      end
      block  ;; label = @2
        local.get 0
        i32.const 0
        i32.load offset=17580
        local.tee 8
        i32.ne
        br_if 0 (;@2;)
        i32.const 3
        local.set 0
        block  ;; label = @3
          local.get 3
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 2
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38084
          i32.add
          i32.load
          local.set 0
        end
        i32.const 3
        local.set 5
        block  ;; label = @3
          local.get 0
          local.tee 0
          i32.const 3
          i32.and
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 0
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38080
          i32.add
          i32.load
          local.set 5
        end
        local.get 5
        local.set 6
        i32.const 3
        local.set 0
        block  ;; label = @3
          local.get 3
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 2
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38084
          i32.add
          i32.load
          local.set 0
        end
        i32.const 3
        local.set 2
        block  ;; label = @3
          local.get 0
          local.tee 0
          i32.const 3
          i32.and
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 0
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38084
          i32.add
          i32.load
          local.set 2
        end
        i32.const 3
        local.set 0
        block  ;; label = @3
          local.get 2
          local.tee 2
          i32.const 3
          i32.and
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 2
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38080
          i32.add
          i32.load
          local.set 0
        end
        local.get 0
        local.set 7
        i32.const 0
        local.set 5
        block  ;; label = @3
          local.get 6
          i32.const 3
          i32.and
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          i32.const 0
          local.set 0
          local.get 6
          local.set 2
          loop  ;; label = @4
            local.get 0
            i32.const 1
            i32.add
            local.tee 0
            local.set 5
            local.get 0
            local.set 0
            local.get 2
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.tee 3
            local.set 2
            local.get 3
            i32.const 3
            i32.and
            i32.const 1
            i32.eq
            br_if 0 (;@4;)
          end
        end
        local.get 5
        local.set 3
        block  ;; label = @3
          block  ;; label = @4
            i32.const 0
            i32.load offset=13468
            local.tee 0
            i32.const 65535
            i32.gt_u
            br_if 0 (;@4;)
            i32.const 0
            local.get 0
            i32.const 1
            i32.add
            i32.store offset=13468
            local.get 0
            i32.const 2
            i32.shl
            i32.const 2135232
            i32.add
            i32.const 5
            i32.store
            br 1 (;@3;)
          end
          i32.const 0
          i32.const 1
          i32.store8 offset=13456
        end
        block  ;; label = @3
          block  ;; label = @4
            i32.const 0
            i32.load offset=13468
            local.tee 0
            i32.const 65535
            i32.gt_u
            br_if 0 (;@4;)
            i32.const 0
            local.get 0
            i32.const 1
            i32.add
            i32.store offset=13468
            local.get 0
            i32.const 2
            i32.shl
            i32.const 2135232
            i32.add
            i32.const 0
            i32.store
            br 1 (;@3;)
          end
          i32.const 0
          i32.const 1
          i32.store8 offset=13456
        end
        block  ;; label = @3
          block  ;; label = @4
            i32.const 0
            i32.load offset=13460
            local.tee 2
            i32.const 262144
            i32.lt_u
            br_if 0 (;@4;)
            i32.const 0
            i32.const 1
            i32.store8 offset=13452
            i32.const 11
            local.set 2
            br 1 (;@3;)
          end
          i32.const 0
          local.get 2
          i32.const 1
          i32.add
          i32.store offset=13460
          local.get 2
          i32.const 3
          i32.shl
          local.tee 5
          i32.const 38084
          i32.add
          local.get 1
          i32.store
          local.get 5
          i32.const 38080
          i32.add
          local.get 6
          i32.store
          local.get 2
          i32.const 2
          i32.shl
          i32.const 1
          i32.or
          local.set 2
        end
        i32.const 0
        i32.load offset=13468
        local.set 5
        local.get 7
        local.get 2
        call $compile
        block  ;; label = @3
          block  ;; label = @4
            i32.const 0
            i32.load offset=13468
            local.tee 2
            i32.const 65535
            i32.gt_u
            br_if 0 (;@4;)
            i32.const 0
            local.get 2
            i32.const 1
            i32.add
            i32.store offset=13468
            local.get 2
            i32.const 2
            i32.shl
            i32.const 2135232
            i32.add
            i32.const 9
            i32.store
            br 1 (;@3;)
          end
          i32.const 0
          i32.const 1
          i32.store8 offset=13456
        end
        local.get 0
        i32.const 2
        i32.shl
        i32.const 2135232
        i32.add
        i32.const 0
        i32.load offset=13468
        local.tee 0
        i32.store
        block  ;; label = @3
          block  ;; label = @4
            local.get 0
            i32.const 65535
            i32.gt_u
            br_if 0 (;@4;)
            i32.const 0
            local.get 0
            i32.const 1
            i32.add
            i32.store offset=13468
            local.get 0
            i32.const 2
            i32.shl
            i32.const 2135232
            i32.add
            i32.const 7
            i32.store
            br 1 (;@3;)
          end
          i32.const 0
          i32.const 1
          i32.store8 offset=13456
        end
        block  ;; label = @3
          block  ;; label = @4
            i32.const 0
            i32.load offset=13468
            local.tee 0
            i32.const 65535
            i32.gt_u
            br_if 0 (;@4;)
            i32.const 0
            local.get 0
            i32.const 1
            i32.add
            i32.store offset=13468
            local.get 0
            i32.const 2
            i32.shl
            i32.const 2135232
            i32.add
            local.get 5
            i32.store
            br 1 (;@3;)
          end
          i32.const 0
          i32.const 1
          i32.store8 offset=13456
        end
        block  ;; label = @3
          i32.const 0
          i32.load offset=13468
          local.tee 0
          i32.const 65535
          i32.gt_u
          br_if 0 (;@3;)
          i32.const 0
          local.get 0
          i32.const 1
          i32.add
          i32.store offset=13468
          local.get 0
          i32.const 2
          i32.shl
          i32.const 2135232
          i32.add
          local.get 3
          i32.store
          return
        end
        i32.const 0
        i32.const 1
        i32.store8 offset=13456
        return
      end
      block  ;; label = @2
        local.get 0
        i32.const 0
        i32.load offset=17588
        i32.ne
        br_if 0 (;@2;)
        i32.const 3
        local.set 0
        block  ;; label = @3
          local.get 5
          br_if 0 (;@3;)
          local.get 2
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38084
          i32.add
          i32.load
          local.set 0
        end
        local.get 0
        local.tee 0
        local.set 3
        block  ;; label = @3
          local.get 0
          i32.const 3
          i32.and
          i32.const 1
          i32.eq
          br_if 0 (;@3;)
          block  ;; label = @4
            block  ;; label = @5
              i32.const 0
              i32.load offset=13468
              local.tee 0
              i32.const 65535
              i32.gt_u
              br_if 0 (;@5;)
              i32.const 0
              local.get 0
              i32.const 1
              i32.add
              i32.store offset=13468
              local.get 0
              i32.const 2
              i32.shl
              i32.const 2135232
              i32.add
              i32.const 0
              i32.store
              br 1 (;@4;)
            end
            i32.const 0
            i32.const 1
            i32.store8 offset=13456
          end
          block  ;; label = @4
            i32.const 0
            i32.load offset=13468
            local.tee 0
            i32.const 65535
            i32.gt_u
            br_if 0 (;@4;)
            i32.const 0
            local.get 0
            i32.const 1
            i32.add
            i32.store offset=13468
            local.get 0
            i32.const 2
            i32.shl
            i32.const 2135232
            i32.add
            i32.const 3
            i32.store
            return
          end
          i32.const 0
          i32.const 1
          i32.store8 offset=13456
          return
        end
        loop  ;; label = @3
          i32.const 3
          local.set 2
          block  ;; label = @4
            local.get 3
            local.tee 5
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            local.tee 0
            br_if 0 (;@4;)
            local.get 5
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 2
          end
          block  ;; label = @4
            local.get 2
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            i32.const 3
            local.set 2
            block  ;; label = @5
              local.get 0
              br_if 0 (;@5;)
              local.get 5
              i32.const 1
              i32.shl
              i32.const -8
              i32.and
              i32.const 38080
              i32.add
              i32.load
              local.set 2
            end
            local.get 2
            local.get 1
            call $compile
            block  ;; label = @5
              block  ;; label = @6
                i32.const 0
                i32.load offset=13468
                local.tee 2
                i32.const 65535
                i32.gt_u
                br_if 0 (;@6;)
                i32.const 0
                local.get 2
                i32.const 1
                i32.add
                i32.store offset=13468
                local.get 2
                i32.const 2
                i32.shl
                i32.const 2135232
                i32.add
                i32.const 4
                i32.store
                br 1 (;@5;)
              end
              i32.const 0
              i32.const 1
              i32.store8 offset=13456
            end
            i32.const 3
            local.set 3
            local.get 0
            br_if 1 (;@3;)
            local.get 5
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 3
            br 1 (;@3;)
          end
        end
        i32.const 3
        local.set 2
        local.get 0
        br_if 1 (;@1;)
        local.get 5
        i32.const 1
        i32.shl
        i32.const -8
        i32.and
        i32.const 38080
        i32.add
        i32.load
        local.set 2
        br 1 (;@1;)
      end
      block  ;; label = @2
        block  ;; label = @3
          local.get 0
          i32.const 0
          i32.load offset=17584
          i32.ne
          br_if 0 (;@3;)
          i32.const 3
          local.set 0
          block  ;; label = @4
            local.get 5
            br_if 0 (;@4;)
            local.get 2
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 0
          end
          i32.const 3
          local.set 3
          block  ;; label = @4
            local.get 0
            local.tee 0
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 0
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38080
            i32.add
            i32.load
            local.set 3
          end
          local.get 3
          local.set 0
          i32.const 3
          local.set 3
          block  ;; label = @4
            local.get 5
            br_if 0 (;@4;)
            local.get 2
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 3
          end
          i32.const 3
          local.set 2
          block  ;; label = @4
            local.get 3
            local.tee 3
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 3
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38084
            i32.add
            i32.load
            local.set 2
          end
          i32.const 3
          local.set 3
          block  ;; label = @4
            local.get 2
            local.tee 2
            i32.const 3
            i32.and
            i32.const 1
            i32.ne
            br_if 0 (;@4;)
            local.get 2
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            i32.const 38080
            i32.add
            i32.load
            local.set 3
          end
          local.get 3
          local.set 9
          i32.const 3
          local.set 4
          i32.const 3
          local.set 10
          local.get 0
          i32.const 3
          i32.and
          i32.const 1
          i32.ne
          br_if 1 (;@2;)
          local.get 0
          local.set 3
          i32.const 3
          local.set 5
          i32.const 3
          local.set 2
          i32.const 0
          i32.load offset=13460
          local.set 0
          loop  ;; label = @4
            local.get 0
            local.set 0
            local.get 2
            local.set 7
            local.get 5
            local.set 4
            i32.const 3
            local.set 2
            block  ;; label = @5
              local.get 3
              i32.const 1
              i32.shl
              i32.const -8
              i32.and
              local.tee 6
              i32.const 38080
              i32.add
              local.tee 5
              i32.load
              local.tee 3
              i32.const 3
              i32.and
              i32.const 1
              i32.ne
              br_if 0 (;@5;)
              local.get 3
              i32.const 1
              i32.shl
              i32.const -8
              i32.and
              i32.const 38080
              i32.add
              i32.load
              local.set 2
            end
            local.get 2
            local.set 2
            block  ;; label = @5
              block  ;; label = @6
                local.get 0
                i32.const 262144
                i32.lt_u
                br_if 0 (;@6;)
                i32.const 0
                i32.const 1
                i32.store8 offset=13452
                local.get 0
                local.set 3
                i32.const 11
                local.set 0
                br 1 (;@5;)
              end
              i32.const 0
              local.get 0
              i32.const 1
              i32.add
              local.tee 3
              i32.store offset=13460
              local.get 0
              i32.const 3
              i32.shl
              local.tee 10
              i32.const 38084
              i32.add
              local.get 7
              i32.store
              local.get 10
              i32.const 38080
              i32.add
              local.get 2
              i32.store
              local.get 3
              local.set 3
              local.get 0
              i32.const 2
              i32.shl
              i32.const 1
              i32.or
              local.set 0
            end
            local.get 0
            local.set 2
            local.get 3
            local.set 0
            i32.const 3
            local.set 3
            block  ;; label = @5
              local.get 5
              i32.load
              local.tee 5
              i32.const 3
              i32.and
              i32.const 1
              i32.ne
              br_if 0 (;@5;)
              local.get 5
              i32.const 1
              i32.shl
              i32.const -8
              i32.and
              i32.const 38084
              i32.add
              i32.load
              local.set 3
            end
            i32.const 3
            local.set 5
            block  ;; label = @5
              local.get 3
              local.tee 3
              i32.const 3
              i32.and
              i32.const 1
              i32.ne
              br_if 0 (;@5;)
              local.get 3
              i32.const 1
              i32.shl
              i32.const -8
              i32.and
              i32.const 38080
              i32.add
              i32.load
              local.set 5
            end
            local.get 5
            local.set 3
            block  ;; label = @5
              block  ;; label = @6
                local.get 0
                i32.const 262144
                i32.lt_u
                br_if 0 (;@6;)
                i32.const 0
                i32.const 1
                i32.store8 offset=13452
                local.get 0
                local.set 7
                i32.const 11
                local.set 0
                br 1 (;@5;)
              end
              i32.const 0
              local.get 0
              i32.const 1
              i32.add
              local.tee 5
              i32.store offset=13460
              local.get 0
              i32.const 3
              i32.shl
              local.tee 7
              i32.const 38084
              i32.add
              local.get 4
              i32.store
              local.get 7
              i32.const 38080
              i32.add
              local.get 3
              i32.store
              local.get 5
              local.set 7
              local.get 0
              i32.const 2
              i32.shl
              i32.const 1
              i32.or
              local.set 0
            end
            local.get 2
            local.set 4
            local.get 0
            local.tee 0
            local.set 10
            local.get 6
            i32.const 38084
            i32.add
            i32.load
            local.tee 6
            local.set 3
            local.get 0
            local.set 5
            local.get 2
            local.set 2
            local.get 7
            local.set 0
            local.get 6
            i32.const 3
            i32.and
            i32.const 1
            i32.eq
            br_if 0 (;@4;)
            br 2 (;@2;)
          end
        end
        local.get 0
        local.get 1
        call $compile
        i32.const 3
        local.set 0
        block  ;; label = @3
          local.get 3
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 2
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38084
          i32.add
          i32.load
          local.set 0
        end
        i32.const 0
        local.set 5
        block  ;; label = @3
          local.get 0
          local.tee 0
          i32.const 3
          i32.and
          i32.const 1
          i32.ne
          br_if 0 (;@3;)
          local.get 0
          local.set 0
          i32.const 0
          local.set 2
          loop  ;; label = @4
            local.get 0
            i32.const 1
            i32.shl
            i32.const -8
            i32.and
            local.tee 0
            i32.const 38080
            i32.add
            i32.load
            local.get 1
            call $compile
            local.get 2
            i32.const 1
            i32.add
            local.tee 2
            local.set 5
            local.get 0
            i32.const 38084
            i32.add
            i32.load
            local.tee 3
            local.set 0
            local.get 2
            local.set 2
            local.get 3
            i32.const 3
            i32.and
            i32.const 1
            i32.eq
            br_if 0 (;@4;)
          end
        end
        local.get 5
        local.set 2
        block  ;; label = @3
          block  ;; label = @4
            i32.const 0
            i32.load offset=13468
            local.tee 0
            i32.const 65535
            i32.gt_u
            br_if 0 (;@4;)
            i32.const 0
            local.get 0
            i32.const 1
            i32.add
            i32.store offset=13468
            local.get 0
            i32.const 2
            i32.shl
            i32.const 2135232
            i32.add
            i32.const 8
            i32.store
            br 1 (;@3;)
          end
          i32.const 0
          i32.const 1
          i32.store8 offset=13456
        end
        block  ;; label = @3
          i32.const 0
          i32.load offset=13468
          local.tee 0
          i32.const 65535
          i32.gt_u
          br_if 0 (;@3;)
          i32.const 0
          local.get 0
          i32.const 1
          i32.add
          i32.store offset=13468
          local.get 0
          i32.const 2
          i32.shl
          i32.const 2135232
          i32.add
          local.get 2
          i32.store
          return
        end
        i32.const 0
        i32.const 1
        i32.store8 offset=13456
        return
      end
      local.get 10
      local.set 2
      local.get 4
      local.set 3
      block  ;; label = @2
        block  ;; label = @3
          i32.const 0
          i32.load offset=13460
          local.tee 0
          i32.const 262144
          i32.lt_u
          br_if 0 (;@3;)
          i32.const 0
          i32.const 1
          i32.store8 offset=13452
          i32.const 11
          local.set 0
          br 1 (;@2;)
        end
        i32.const 0
        local.get 0
        i32.const 1
        i32.add
        i32.store offset=13460
        local.get 0
        i32.const 3
        i32.shl
        local.tee 5
        i32.const 38084
        i32.add
        i32.const 3
        i32.store
        local.get 5
        i32.const 38080
        i32.add
        local.get 9
        i32.store
        local.get 0
        i32.const 2
        i32.shl
        i32.const 1
        i32.or
        local.set 0
      end
      local.get 0
      local.set 5
      block  ;; label = @2
        block  ;; label = @3
          i32.const 0
          i32.load offset=13460
          local.tee 0
          i32.const 262144
          i32.lt_u
          br_if 0 (;@3;)
          i32.const 0
          i32.const 1
          i32.store8 offset=13452
          i32.const 11
          local.set 0
          br 1 (;@2;)
        end
        i32.const 0
        local.get 0
        i32.const 1
        i32.add
        i32.store offset=13460
        local.get 0
        i32.const 3
        i32.shl
        local.tee 6
        i32.const 38084
        i32.add
        local.get 5
        i32.store
        local.get 6
        i32.const 38080
        i32.add
        local.get 3
        i32.store
        local.get 0
        i32.const 2
        i32.shl
        i32.const 1
        i32.or
        local.set 0
      end
      local.get 0
      local.set 3
      block  ;; label = @2
        block  ;; label = @3
          i32.const 0
          i32.load offset=13460
          local.tee 0
          i32.const 262144
          i32.lt_u
          br_if 0 (;@3;)
          i32.const 0
          i32.const 1
          i32.store8 offset=13452
          i32.const 11
          local.set 0
          br 1 (;@2;)
        end
        i32.const 0
        local.get 0
        i32.const 1
        i32.add
        i32.store offset=13460
        local.get 0
        i32.const 3
        i32.shl
        local.tee 5
        i32.const 38084
        i32.add
        local.get 3
        i32.store
        local.get 5
        i32.const 38080
        i32.add
        local.get 8
        i32.store
        local.get 0
        i32.const 2
        i32.shl
        i32.const 1
        i32.or
        local.set 0
      end
      local.get 0
      local.set 3
      block  ;; label = @2
        i32.const 0
        i32.load offset=13460
        local.tee 0
        i32.const 262144
        i32.lt_u
        br_if 0 (;@2;)
        i32.const 0
        i32.const 1
        i32.store8 offset=13452
        i32.const 11
        local.set 2
        br 1 (;@1;)
      end
      i32.const 0
      local.get 0
      i32.const 1
      i32.add
      i32.store offset=13460
      local.get 0
      i32.const 3
      i32.shl
      local.tee 5
      i32.const 38084
      i32.add
      local.get 2
      i32.store
      local.get 5
      i32.const 38080
      i32.add
      local.get 3
      i32.store
      local.get 0
      i32.const 2
      i32.shl
      i32.const 1
      i32.or
      local.set 2
      br 0 (;@1;)
    end)
  (func $run (type 0) (result i32)
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0
    i32.load offset=17592
    local.set 0
    i32.const 0
    i32.load offset=13460
    local.set 1
    i32.const 0
    i32.load8_u offset=13452
    local.set 2
    i32.const 0
    local.set 3
    i32.const 0
    local.set 4
    i32.const 3
    local.set 5
    i32.const 0
    local.set 6
    i32.const 0
    global.set $icount
    loop  ;; label = @1
      global.get $icount
      i32.const 1
      i32.add
      global.set $icount
      local.get 8
      local.set 7
      local.get 6
      local.set 6
      local.get 5
      local.set 9
      local.get 4
      local.set 4
      local.get 3
      local.set 10
      local.get 1
      local.set 8
      block  ;; label = @2
        local.get 2
        local.tee 11
        i32.const 1
        i32.and
        i32.eqz
        br_if 0 (;@2;)
        i32.const 11
        return
      end
      local.get 8
      local.set 1
      local.get 11
      local.set 2
      local.get 10
      local.set 3
      local.get 4
      local.set 5
      local.get 9
      local.set 12
      local.get 6
      i32.const 1
      i32.add
      local.tee 13
      local.set 14
      local.get 7
      local.set 15
      block  ;; label = @2
        block  ;; label = @3
          block  ;; label = @4
            block  ;; label = @5
              block  ;; label = @6
                block  ;; label = @7
                  block  ;; label = @8
                    block  ;; label = @9
                      block  ;; label = @10
                        block  ;; label = @11
                          block  ;; label = @12
                            block  ;; label = @13
                              block  ;; label = @14
                                block  ;; label = @15
                                  block  ;; label = @16
                                    block  ;; label = @17
                                      block  ;; label = @18
                                        block  ;; label = @19
                                          block  ;; label = @20
                                            local.get 6
                                            i32.const 2
                                            i32.shl
                                            local.tee 16
                                            i32.const 2135232
                                            i32.add
                                            i32.load
                                            br_table 10 (;@10;) 0 (;@20;) 1 (;@19;) 2 (;@18;) 3 (;@17;) 4 (;@16;) 5 (;@15;) 6 (;@14;) 7 (;@13;) 8 (;@12;) 9 (;@11;) 17 (;@3;)
                                          end
                                          local.get 16
                                          i32.const 2135240
                                          i32.add
                                          local.set 12
                                          local.get 9
                                          local.set 1
                                          block  ;; label = @20
                                            local.get 13
                                            i32.const 2
                                            i32.shl
                                            i32.const 2135232
                                            i32.add
                                            i32.load
                                            local.tee 2
                                            i32.eqz
                                            br_if 0 (;@20;)
                                            local.get 2
                                            local.set 2
                                            local.get 9
                                            local.set 3
                                            loop  ;; label = @21
                                              local.get 2
                                              local.set 2
                                              i32.const 3
                                              local.set 1
                                              block  ;; label = @22
                                                local.get 3
                                                local.tee 3
                                                i32.const 3
                                                i32.and
                                                i32.const 1
                                                i32.ne
                                                br_if 0 (;@22;)
                                                local.get 3
                                                i32.const 1
                                                i32.shl
                                                i32.const -8
                                                i32.and
                                                i32.const 38084
                                                i32.add
                                                i32.load
                                                local.set 1
                                              end
                                              local.get 2
                                              i32.const -1
                                              i32.add
                                              local.tee 5
                                              local.set 2
                                              local.get 1
                                              local.tee 1
                                              local.set 3
                                              local.get 1
                                              local.set 1
                                              local.get 5
                                              br_if 0 (;@21;)
                                            end
                                          end
                                          local.get 12
                                          i32.load
                                          local.set 2
                                          i32.const 3
                                          local.set 3
                                          block  ;; label = @20
                                            local.get 1
                                            local.tee 1
                                            i32.const 3
                                            i32.and
                                            i32.const 1
                                            i32.ne
                                            br_if 0 (;@20;)
                                            local.get 1
                                            i32.const 1
                                            i32.shl
                                            i32.const -8
                                            i32.and
                                            i32.const 38080
                                            i32.add
                                            i32.load
                                            local.set 3
                                          end
                                          local.get 3
                                          local.set 1
                                          block  ;; label = @20
                                            block  ;; label = @21
                                              local.get 2
                                              br_if 0 (;@21;)
                                              local.get 1
                                              local.set 1
                                              br 1 (;@20;)
                                            end
                                            local.get 2
                                            local.set 2
                                            local.get 1
                                            local.set 3
                                            loop  ;; label = @21
                                              local.get 2
                                              local.set 2
                                              i32.const 3
                                              local.set 1
                                              block  ;; label = @22
                                                local.get 3
                                                local.tee 3
                                                i32.const 3
                                                i32.and
                                                i32.const 1
                                                i32.ne
                                                br_if 0 (;@22;)
                                                local.get 3
                                                i32.const 1
                                                i32.shl
                                                i32.const -8
                                                i32.and
                                                i32.const 38084
                                                i32.add
                                                i32.load
                                                local.set 1
                                              end
                                              local.get 2
                                              i32.const -1
                                              i32.add
                                              local.tee 5
                                              local.set 2
                                              local.get 1
                                              local.tee 1
                                              local.set 3
                                              local.get 1
                                              local.set 1
                                              local.get 5
                                              br_if 0 (;@21;)
                                            end
                                          end
                                          i32.const 3
                                          local.set 2
                                          block  ;; label = @20
                                            local.get 1
                                            local.tee 1
                                            i32.const 3
                                            i32.and
                                            i32.const 1
                                            i32.ne
                                            br_if 0 (;@20;)
                                            local.get 1
                                            i32.const 1
                                            i32.shl
                                            i32.const -8
                                            i32.and
                                            i32.const 38080
                                            i32.add
                                            i32.load
                                            local.set 2
                                          end
                                          local.get 4
                                          i32.const 2
                                          i32.shl
                                          i32.const 2397376
                                          i32.add
                                          local.get 2
                                          i32.store
                                          local.get 8
                                          local.set 1
                                          local.get 11
                                          local.set 2
                                          local.get 10
                                          local.set 3
                                          local.get 4
                                          i32.const 1
                                          i32.add
                                          local.set 5
                                          local.get 9
                                          local.set 12
                                          local.get 6
                                          i32.const 3
                                          i32.add
                                          local.set 14
                                          br 10 (;@9;)
                                        end
                                        block  ;; label = @19
                                          block  ;; label = @20
                                            local.get 13
                                            i32.const 2
                                            i32.shl
                                            i32.const 2135232
                                            i32.add
                                            i32.load
                                            i32.const 2
                                            i32.shl
                                            i32.const 13472
                                            i32.add
                                            i32.load
                                            local.tee 1
                                            i32.const 15
                                            i32.eq
                                            local.tee 17
                                            i32.eqz
                                            br_if 0 (;@20;)
                                            local.get 4
                                            local.set 4
                                            i32.const 11
                                            local.set 7
                                            br 1 (;@19;)
                                          end
                                          local.get 4
                                          i32.const 2
                                          i32.shl
                                          i32.const 2397376
                                          i32.add
                                          local.get 1
                                          i32.store
                                          local.get 4
                                          i32.const 1
                                          i32.add
                                          local.set 4
                                          local.get 7
                                          local.set 7
                                        end
                                        local.get 8
                                        local.set 1
                                        local.get 11
                                        local.set 2
                                        local.get 10
                                        local.set 3
                                        local.get 4
                                        local.tee 4
                                        local.set 5
                                        local.get 9
                                        local.set 12
                                        local.get 6
                                        i32.const 2
                                        i32.add
                                        local.tee 13
                                        local.set 14
                                        local.get 7
                                        local.tee 16
                                        local.set 15
                                        local.get 8
                                        local.set 6
                                        local.get 11
                                        local.set 8
                                        i32.const 0
                                        local.set 11
                                        local.get 10
                                        local.set 10
                                        local.get 4
                                        local.set 7
                                        local.get 9
                                        local.set 9
                                        local.get 13
                                        local.set 13
                                        local.get 16
                                        local.set 16
                                        local.get 17
                                        i32.eqz
                                        br_if 15 (;@3;)
                                        br 16 (;@2;)
                                      end
                                      local.get 13
                                      i32.const 2
                                      i32.shl
                                      i32.const 2135232
                                      i32.add
                                      i32.load
                                      i32.const 2
                                      i32.shl
                                      local.tee 1
                                      i32.const 13472
                                      i32.add
                                      local.get 4
                                      i32.const 2
                                      i32.shl
                                      i32.const 2397372
                                      i32.add
                                      local.tee 2
                                      i32.load
                                      i32.store
                                      local.get 2
                                      local.get 1
                                      i32.const 2
                                      i32.or
                                      i32.store
                                      local.get 8
                                      local.set 1
                                      local.get 11
                                      local.set 2
                                      local.get 10
                                      local.set 3
                                      local.get 4
                                      local.set 5
                                      local.get 9
                                      local.set 12
                                      local.get 6
                                      i32.const 2
                                      i32.add
                                      local.set 14
                                      br 8 (;@9;)
                                    end
                                    local.get 8
                                    local.set 1
                                    local.get 11
                                    local.set 2
                                    local.get 10
                                    local.set 3
                                    local.get 4
                                    i32.const -1
                                    i32.add
                                    local.set 5
                                    local.get 9
                                    local.set 12
                                    local.get 13
                                    local.set 14
                                    br 7 (;@9;)
                                  end
                                  local.get 8
                                  local.set 1
                                  local.get 11
                                  local.set 2
                                  local.get 10
                                  local.set 3
                                  local.get 4
                                  local.set 5
                                  local.get 9
                                  local.set 12
                                  local.get 13
                                  i32.const 2
                                  i32.shl
                                  i32.const 2135232
                                  i32.add
                                  i32.load
                                  local.set 14
                                  br 6 (;@9;)
                                end
                                block  ;; label = @15
                                  local.get 4
                                  i32.const -1
                                  i32.add
                                  local.tee 4
                                  i32.const 2
                                  i32.shl
                                  i32.const 2397376
                                  i32.add
                                  i32.load
                                  i32.const 3
                                  i32.eq
                                  br_if 0 (;@15;)
                                  local.get 8
                                  local.set 1
                                  local.get 11
                                  local.set 2
                                  local.get 10
                                  local.set 3
                                  local.get 4
                                  local.set 5
                                  local.get 9
                                  local.set 12
                                  local.get 6
                                  i32.const 2
                                  i32.add
                                  local.set 14
                                  br 6 (;@9;)
                                end
                                local.get 8
                                local.set 1
                                local.get 11
                                local.set 2
                                local.get 10
                                local.set 3
                                local.get 4
                                local.set 5
                                local.get 9
                                local.set 12
                                local.get 13
                                i32.const 2
                                i32.shl
                                i32.const 2135232
                                i32.add
                                i32.load
                                local.set 14
                                br 5 (;@9;)
                              end
                              block  ;; label = @14
                                block  ;; label = @15
                                  local.get 8
                                  i32.const 262144
                                  i32.lt_u
                                  br_if 0 (;@15;)
                                  i32.const 0
                                  i32.const 1
                                  i32.store8 offset=13452
                                  local.get 8
                                  local.set 1
                                  i32.const 1
                                  local.set 2
                                  i32.const 11
                                  local.set 3
                                  br 1 (;@14;)
                                end
                                i32.const 0
                                local.get 8
                                i32.const 1
                                i32.add
                                local.tee 1
                                i32.store offset=13460
                                local.get 8
                                i32.const 3
                                i32.shl
                                local.tee 2
                                i32.const 38084
                                i32.add
                                i32.const 3
                                i32.store
                                local.get 2
                                i32.const 38080
                                i32.add
                                local.get 9
                                i32.store
                                local.get 1
                                local.set 1
                                local.get 11
                                local.set 2
                                local.get 8
                                i32.const 2
                                i32.shl
                                i32.const 1
                                i32.or
                                local.set 3
                              end
                              local.get 3
                              local.set 3
                              local.get 2
                              local.set 5
                              block  ;; label = @14
                                block  ;; label = @15
                                  local.get 1
                                  local.tee 1
                                  i32.const 262144
                                  i32.lt_u
                                  br_if 0 (;@15;)
                                  i32.const 0
                                  i32.const 1
                                  i32.store8 offset=13452
                                  local.get 1
                                  local.set 2
                                  i32.const 1
                                  local.set 3
                                  i32.const 11
                                  local.set 1
                                  br 1 (;@14;)
                                end
                                local.get 16
                                i32.const 2135240
                                i32.add
                                i32.load
                                local.set 2
                                i32.const 0
                                local.get 1
                                i32.const 1
                                i32.add
                                local.tee 8
                                i32.store offset=13460
                                local.get 1
                                i32.const 3
                                i32.shl
                                local.tee 11
                                i32.const 38084
                                i32.add
                                local.get 3
                                i32.store
                                local.get 11
                                i32.const 38080
                                i32.add
                                local.get 2
                                i32.const 2
                                i32.shl
                                i32.store
                                local.get 8
                                local.set 2
                                local.get 5
                                local.set 3
                                local.get 1
                                i32.const 2
                                i32.shl
                                i32.const 1
                                i32.or
                                local.set 1
                              end
                              local.get 1
                              local.set 5
                              local.get 3
                              local.set 3
                              block  ;; label = @14
                                block  ;; label = @15
                                  local.get 2
                                  local.tee 1
                                  i32.const 262144
                                  i32.lt_u
                                  br_if 0 (;@15;)
                                  i32.const 0
                                  i32.const 1
                                  i32.store8 offset=13452
                                  local.get 1
                                  local.set 2
                                  i32.const 1
                                  local.set 3
                                  i32.const 11
                                  local.set 1
                                  br 1 (;@14;)
                                end
                                local.get 13
                                i32.const 2
                                i32.shl
                                i32.const 2135232
                                i32.add
                                i32.load
                                local.set 2
                                i32.const 0
                                local.get 1
                                i32.const 1
                                i32.add
                                local.tee 8
                                i32.store offset=13460
                                local.get 1
                                i32.const 3
                                i32.shl
                                local.tee 11
                                i32.const 38084
                                i32.add
                                local.get 5
                                i32.store
                                local.get 11
                                i32.const 38080
                                i32.add
                                local.get 2
                                i32.const 2
                                i32.shl
                                i32.store
                                local.get 8
                                local.set 2
                                local.get 3
                                local.set 3
                                local.get 1
                                i32.const 2
                                i32.shl
                                i32.const 1
                                i32.or
                                local.set 1
                              end
                              local.get 1
                              local.set 5
                              local.get 3
                              local.set 3
                              block  ;; label = @14
                                block  ;; label = @15
                                  local.get 2
                                  local.tee 1
                                  i32.const 262144
                                  i32.lt_u
                                  br_if 0 (;@15;)
                                  i32.const 0
                                  i32.const 1
                                  i32.store8 offset=13452
                                  local.get 1
                                  local.set 2
                                  i32.const 1
                                  local.set 3
                                  i32.const 11
                                  local.set 1
                                  br 1 (;@14;)
                                end
                                i32.const 0
                                local.get 1
                                i32.const 1
                                i32.add
                                local.tee 2
                                i32.store offset=13460
                                local.get 1
                                i32.const 3
                                i32.shl
                                local.tee 8
                                i32.const 38084
                                i32.add
                                local.get 5
                                i32.store
                                local.get 8
                                i32.const 38080
                                i32.add
                                local.get 0
                                i32.store
                                local.get 2
                                local.set 2
                                local.get 3
                                local.set 3
                                local.get 1
                                i32.const 2
                                i32.shl
                                i32.const 1
                                i32.or
                                local.set 1
                              end
                              local.get 4
                              i32.const 2
                              i32.shl
                              i32.const 2397376
                              i32.add
                              local.get 1
                              i32.store
                              local.get 2
                              local.set 1
                              local.get 3
                              local.set 2
                              local.get 10
                              local.set 3
                              local.get 4
                              i32.const 1
                              i32.add
                              local.set 5
                              local.get 9
                              local.set 12
                              local.get 6
                              i32.const 3
                              i32.add
                              local.set 14
                              br 4 (;@9;)
                            end
                            local.get 6
                            i32.const 2
                            i32.add
                            local.set 18
                            block  ;; label = @13
                              local.get 4
                              local.get 13
                              i32.const 2
                              i32.shl
                              i32.const 2135232
                              i32.add
                              i32.load
                              local.tee 19
                              i32.sub
                              local.tee 14
                              i32.const 2
                              i32.shl
                              i32.const 2397372
                              i32.add
                              i32.load
                              local.tee 17
                              i32.const 3
                              i32.and
                              local.tee 1
                              i32.const 3
                              i32.ne
                              br_if 0 (;@13;)
                              local.get 17
                              i32.const -16
                              i32.add
                              i32.const 43
                              i32.gt_u
                              br_if 0 (;@13;)
                              block  ;; label = @14
                                local.get 4
                                local.get 14
                                i32.gt_s
                                br_if 0 (;@14;)
                                local.get 8
                                local.set 15
                                local.get 11
                                local.set 13
                                i32.const 3
                                local.set 16
                                br 8 (;@6;)
                              end
                              local.get 4
                              i32.const 2
                              i32.shl
                              i32.const 2397372
                              i32.add
                              local.set 1
                              local.get 8
                              local.set 6
                              local.get 11
                              local.set 2
                              local.get 4
                              local.set 3
                              i32.const 3
                              local.set 5
                              local.get 8
                              local.set 8
                              loop  ;; label = @14
                                local.get 5
                                local.set 5
                                local.get 3
                                local.set 3
                                local.get 2
                                local.set 11
                                local.get 6
                                local.set 2
                                local.get 1
                                local.set 6
                                block  ;; label = @15
                                  block  ;; label = @16
                                    local.get 8
                                    local.tee 1
                                    i32.const 262144
                                    i32.lt_u
                                    br_if 0 (;@16;)
                                    i32.const 0
                                    i32.const 1
                                    i32.store8 offset=13452
                                    local.get 2
                                    local.set 2
                                    i32.const 1
                                    local.set 5
                                    local.get 1
                                    local.set 8
                                    i32.const 11
                                    local.set 1
                                    br 1 (;@15;)
                                  end
                                  i32.const 0
                                  local.get 1
                                  i32.const 1
                                  i32.add
                                  local.tee 8
                                  i32.store offset=13460
                                  local.get 1
                                  i32.const 3
                                  i32.shl
                                  local.tee 2
                                  i32.const 38084
                                  i32.add
                                  local.get 5
                                  i32.store
                                  local.get 2
                                  i32.const 38080
                                  i32.add
                                  local.get 6
                                  i32.load
                                  i32.store
                                  local.get 8
                                  local.set 2
                                  local.get 11
                                  local.set 5
                                  local.get 8
                                  local.set 8
                                  local.get 1
                                  i32.const 2
                                  i32.shl
                                  i32.const 1
                                  i32.or
                                  local.set 1
                                end
                                local.get 2
                                local.tee 2
                                local.set 15
                                local.get 5
                                local.tee 5
                                local.set 13
                                local.get 1
                                local.tee 11
                                local.set 16
                                local.get 6
                                i32.const -4
                                i32.add
                                local.set 1
                                local.get 2
                                local.set 6
                                local.get 5
                                local.set 2
                                local.get 3
                                i32.const -1
                                i32.add
                                local.tee 12
                                local.set 3
                                local.get 11
                                local.set 5
                                local.get 8
                                local.set 8
                                local.get 12
                                local.get 14
                                i32.le_s
                                br_if 8 (;@6;)
                                br 0 (;@14;)
                              end
                            end
                            i32.const 0
                            local.set 6
                            block  ;; label = @13
                              local.get 1
                              i32.const 1
                              i32.ne
                              local.tee 20
                              br_if 0 (;@13;)
                              local.get 17
                              i32.const 1
                              i32.shl
                              i32.const -8
                              i32.and
                              i32.const 38080
                              i32.add
                              i32.load
                              local.get 0
                              i32.eq
                              local.set 6
                            end
                            block  ;; label = @13
                              local.get 6
                              br_if 0 (;@13;)
                              local.get 8
                              local.set 1
                              local.get 11
                              local.set 6
                              i32.const 0
                              local.set 17
                              local.get 10
                              local.set 3
                              local.get 4
                              local.set 4
                              br 6 (;@7;)
                            end
                            block  ;; label = @13
                              local.get 4
                              local.get 14
                              i32.gt_s
                              br_if 0 (;@13;)
                              local.get 8
                              local.set 15
                              local.get 11
                              local.set 13
                              i32.const 3
                              local.set 16
                              br 5 (;@8;)
                            end
                            local.get 4
                            i32.const 2
                            i32.shl
                            i32.const 2397372
                            i32.add
                            local.set 1
                            local.get 8
                            local.set 6
                            local.get 11
                            local.set 2
                            local.get 4
                            local.set 3
                            i32.const 3
                            local.set 5
                            local.get 8
                            local.set 8
                            loop  ;; label = @13
                              local.get 5
                              local.set 5
                              local.get 3
                              local.set 3
                              local.get 2
                              local.set 11
                              local.get 6
                              local.set 2
                              local.get 1
                              local.set 6
                              block  ;; label = @14
                                block  ;; label = @15
                                  local.get 8
                                  local.tee 1
                                  i32.const 262144
                                  i32.lt_u
                                  br_if 0 (;@15;)
                                  i32.const 0
                                  i32.const 1
                                  i32.store8 offset=13452
                                  local.get 2
                                  local.set 2
                                  i32.const 1
                                  local.set 5
                                  local.get 1
                                  local.set 8
                                  i32.const 11
                                  local.set 1
                                  br 1 (;@14;)
                                end
                                i32.const 0
                                local.get 1
                                i32.const 1
                                i32.add
                                local.tee 8
                                i32.store offset=13460
                                local.get 1
                                i32.const 3
                                i32.shl
                                local.tee 2
                                i32.const 38084
                                i32.add
                                local.get 5
                                i32.store
                                local.get 2
                                i32.const 38080
                                i32.add
                                local.get 6
                                i32.load
                                i32.store
                                local.get 8
                                local.set 2
                                local.get 11
                                local.set 5
                                local.get 8
                                local.set 8
                                local.get 1
                                i32.const 2
                                i32.shl
                                i32.const 1
                                i32.or
                                local.set 1
                              end
                              local.get 2
                              local.tee 2
                              local.set 15
                              local.get 5
                              local.tee 5
                              local.set 13
                              local.get 1
                              local.tee 11
                              local.set 16
                              local.get 6
                              i32.const -4
                              i32.add
                              local.set 1
                              local.get 2
                              local.set 6
                              local.get 5
                              local.set 2
                              local.get 3
                              i32.const -1
                              i32.add
                              local.tee 12
                              local.set 3
                              local.get 11
                              local.set 5
                              local.get 8
                              local.set 8
                              local.get 12
                              local.get 14
                              i32.le_s
                              br_if 5 (;@8;)
                              br 0 (;@13;)
                            end
                          end
                          local.get 8
                          local.set 1
                          local.get 11
                          local.set 2
                          local.get 10
                          i32.const -1
                          i32.add
                          local.tee 6
                          local.set 3
                          local.get 4
                          local.set 5
                          local.get 6
                          i32.const 2
                          i32.shl
                          local.tee 4
                          i32.const 10659520
                          i32.add
                          i32.load
                          local.set 12
                          local.get 4
                          i32.const 2659520
                          i32.add
                          i32.load
                          local.set 14
                          br 2 (;@9;)
                        end
                        local.get 8
                        local.set 6
                        local.get 11
                        local.set 8
                        i32.const 0
                        local.set 11
                        local.get 10
                        local.set 10
                        local.get 4
                        local.set 7
                        local.get 9
                        local.set 9
                        local.get 13
                        local.set 13
                        local.get 4
                        i32.const 2
                        i32.shl
                        i32.const 2397372
                        i32.add
                        i32.load
                        local.set 16
                        br 8 (;@2;)
                      end
                      local.get 4
                      i32.const 2
                      i32.shl
                      i32.const 2397376
                      i32.add
                      local.get 13
                      i32.const 2
                      i32.shl
                      i32.const 2135232
                      i32.add
                      i32.load
                      i32.store
                      local.get 8
                      local.set 1
                      local.get 11
                      local.set 2
                      local.get 10
                      local.set 3
                      local.get 4
                      i32.const 1
                      i32.add
                      local.set 5
                      local.get 9
                      local.set 12
                      local.get 6
                      i32.const 2
                      i32.add
                      local.set 14
                    end
                    local.get 7
                    local.set 15
                    br 5 (;@3;)
                  end
                  local.get 16
                  local.set 3
                  local.get 13
                  local.set 5
                  local.get 15
                  local.set 1
                  i32.const 3
                  local.set 6
                  block  ;; label = @8
                    local.get 20
                    br_if 0 (;@8;)
                    local.get 17
                    i32.const 1
                    i32.shl
                    i32.const -8
                    i32.and
                    i32.const 38084
                    i32.add
                    i32.load
                    local.set 6
                  end
                  i32.const 3
                  local.set 2
                  block  ;; label = @8
                    local.get 6
                    local.tee 6
                    i32.const 3
                    i32.and
                    i32.const 1
                    i32.ne
                    br_if 0 (;@8;)
                    local.get 6
                    i32.const 1
                    i32.shl
                    i32.const -8
                    i32.and
                    i32.const 38084
                    i32.add
                    i32.load
                    local.set 2
                  end
                  i32.const 3
                  local.set 6
                  block  ;; label = @8
                    local.get 2
                    local.tee 2
                    i32.const 3
                    i32.and
                    i32.const 1
                    i32.ne
                    br_if 0 (;@8;)
                    local.get 2
                    i32.const 1
                    i32.shl
                    i32.const -8
                    i32.and
                    i32.const 38084
                    i32.add
                    i32.load
                    local.set 6
                  end
                  i32.const 3
                  local.set 2
                  block  ;; label = @8
                    local.get 6
                    local.tee 6
                    i32.const 3
                    i32.and
                    i32.const 1
                    i32.ne
                    br_if 0 (;@8;)
                    local.get 6
                    i32.const 1
                    i32.shl
                    i32.const -8
                    i32.and
                    i32.const 38080
                    i32.add
                    i32.load
                    local.set 2
                  end
                  local.get 2
                  local.set 6
                  block  ;; label = @8
                    block  ;; label = @9
                      local.get 1
                      i32.const 262144
                      i32.lt_u
                      br_if 0 (;@9;)
                      i32.const 0
                      i32.const 1
                      i32.store8 offset=13452
                      local.get 1
                      local.set 6
                      i32.const 1
                      local.set 2
                      i32.const 11
                      local.set 1
                      br 1 (;@8;)
                    end
                    i32.const 0
                    local.get 1
                    i32.const 1
                    i32.add
                    local.tee 2
                    i32.store offset=13460
                    local.get 1
                    i32.const 3
                    i32.shl
                    local.tee 8
                    i32.const 38084
                    i32.add
                    local.get 6
                    i32.store
                    local.get 8
                    i32.const 38080
                    i32.add
                    local.get 3
                    i32.store
                    local.get 2
                    local.set 6
                    local.get 5
                    local.set 2
                    local.get 1
                    i32.const 2
                    i32.shl
                    i32.const 1
                    i32.or
                    local.set 1
                  end
                  local.get 1
                  local.set 5
                  local.get 2
                  local.set 2
                  local.get 6
                  local.set 1
                  local.get 4
                  local.get 19
                  i32.const -1
                  i32.xor
                  i32.add
                  local.set 4
                  block  ;; label = @8
                    local.get 10
                    i32.const 2000000
                    i32.lt_u
                    local.tee 3
                    br_if 0 (;@8;)
                    local.get 1
                    local.set 1
                    local.get 2
                    local.set 6
                    local.get 3
                    local.set 17
                    local.get 10
                    local.set 3
                    local.get 4
                    local.set 4
                    br 1 (;@7;)
                  end
                  local.get 10
                  i32.const 2
                  i32.shl
                  local.tee 6
                  i32.const 10659520
                  i32.add
                  local.get 9
                  i32.store
                  local.get 6
                  i32.const 2659520
                  i32.add
                  local.get 18
                  i32.store
                  i32.const 3
                  local.set 6
                  block  ;; label = @8
                    local.get 20
                    br_if 0 (;@8;)
                    local.get 17
                    i32.const 1
                    i32.shl
                    i32.const -8
                    i32.and
                    i32.const 38084
                    i32.add
                    i32.load
                    local.set 6
                  end
                  i32.const 3
                  local.set 9
                  block  ;; label = @8
                    local.get 6
                    local.tee 6
                    i32.const 3
                    i32.and
                    i32.const 1
                    i32.ne
                    br_if 0 (;@8;)
                    local.get 6
                    i32.const 1
                    i32.shl
                    i32.const -8
                    i32.and
                    i32.const 38080
                    i32.add
                    i32.load
                    local.set 9
                  end
                  local.get 1
                  local.set 1
                  local.get 2
                  local.set 6
                  local.get 3
                  local.set 17
                  local.get 10
                  i32.const 1
                  i32.add
                  local.set 3
                  local.get 4
                  local.set 4
                  local.get 5
                  local.set 8
                  local.get 9
                  i32.const 2
                  i32.shr_s
                  local.set 9
                  br 2 (;@5;)
                end
                local.get 9
                local.set 8
                local.get 18
                local.set 9
                i32.const 11
                local.set 10
                br 2 (;@4;)
              end
              local.get 13
              local.set 5
              local.get 15
              local.set 1
              i32.const 3
              local.set 6
              block  ;; label = @6
                local.get 16
                local.tee 2
                i32.const 3
                i32.and
                i32.const 1
                i32.ne
                local.tee 3
                br_if 0 (;@6;)
                local.get 2
                i32.const 1
                i32.shl
                i32.const -8
                i32.and
                i32.const 38080
                i32.add
                i32.load
                local.set 6
              end
              local.get 6
              local.set 8
              i32.const 3
              local.set 6
              block  ;; label = @6
                local.get 3
                br_if 0 (;@6;)
                local.get 2
                i32.const 1
                i32.shl
                i32.const -8
                i32.and
                i32.const 38084
                i32.add
                i32.load
                local.set 6
              end
              local.get 17
              i32.const 2
              i32.shr_u
              local.set 11
              i32.const 3
              local.set 2
              block  ;; label = @6
                local.get 6
                local.tee 6
                i32.const 3
                i32.and
                i32.const 1
                i32.ne
                br_if 0 (;@6;)
                local.get 6
                i32.const 1
                i32.shl
                i32.const -8
                i32.and
                i32.const 38080
                i32.add
                i32.load
                local.set 2
              end
              local.get 2
              local.set 12
              local.get 1
              local.set 6
              local.get 5
              local.set 2
              i32.const 11
              local.set 3
              block  ;; label = @6
                block  ;; label = @7
                  block  ;; label = @8
                    block  ;; label = @9
                      block  ;; label = @10
                        block  ;; label = @11
                          block  ;; label = @12
                            block  ;; label = @13
                              block  ;; label = @14
                                block  ;; label = @15
                                  block  ;; label = @16
                                    block  ;; label = @17
                                      local.get 11
                                      i32.const -4
                                      i32.add
                                      br_table 0 (;@17;) 1 (;@16;) 2 (;@15;) 3 (;@14;) 4 (;@13;) 5 (;@12;) 6 (;@11;) 7 (;@10;) 8 (;@9;) 9 (;@8;) 10 (;@7;) 11 (;@6;)
                                    end
                                    block  ;; label = @17
                                      local.get 1
                                      i32.const 262144
                                      i32.lt_u
                                      br_if 0 (;@17;)
                                      i32.const 0
                                      i32.const 1
                                      i32.store8 offset=13452
                                      local.get 1
                                      local.set 6
                                      i32.const 1
                                      local.set 2
                                      i32.const 11
                                      local.set 3
                                      br 11 (;@6;)
                                    end
                                    i32.const 0
                                    local.get 1
                                    i32.const 1
                                    i32.add
                                    local.tee 6
                                    i32.store offset=13460
                                    local.get 1
                                    i32.const 3
                                    i32.shl
                                    local.tee 2
                                    i32.const 38084
                                    i32.add
                                    local.get 12
                                    i32.store
                                    local.get 2
                                    i32.const 38080
                                    i32.add
                                    local.get 8
                                    i32.store
                                    local.get 6
                                    local.set 6
                                    local.get 5
                                    local.set 2
                                    local.get 1
                                    i32.const 2
                                    i32.shl
                                    i32.const 1
                                    i32.or
                                    local.set 3
                                    br 10 (;@6;)
                                  end
                                  block  ;; label = @16
                                    local.get 8
                                    i32.const 3
                                    i32.and
                                    i32.const 1
                                    i32.eq
                                    br_if 0 (;@16;)
                                    local.get 1
                                    local.set 6
                                    local.get 5
                                    local.set 2
                                    i32.const 3
                                    local.set 3
                                    br 10 (;@6;)
                                  end
                                  local.get 1
                                  local.set 6
                                  local.get 5
                                  local.set 2
                                  local.get 8
                                  i32.const 1
                                  i32.shl
                                  i32.const -8
                                  i32.and
                                  i32.const 38080
                                  i32.add
                                  i32.load
                                  local.set 3
                                  br 9 (;@6;)
                                end
                                block  ;; label = @15
                                  local.get 8
                                  i32.const 3
                                  i32.and
                                  i32.const 1
                                  i32.eq
                                  br_if 0 (;@15;)
                                  local.get 1
                                  local.set 6
                                  local.get 5
                                  local.set 2
                                  i32.const 3
                                  local.set 3
                                  br 9 (;@6;)
                                end
                                local.get 1
                                local.set 6
                                local.get 5
                                local.set 2
                                local.get 8
                                i32.const 1
                                i32.shl
                                i32.const -8
                                i32.and
                                i32.const 38084
                                i32.add
                                i32.load
                                local.set 3
                                br 8 (;@6;)
                              end
                              local.get 1
                              local.set 6
                              local.get 5
                              local.set 2
                              local.get 12
                              local.get 8
                              i32.const -4
                              i32.and
                              i32.add
                              i32.const -4
                              i32.and
                              local.set 3
                              br 7 (;@6;)
                            end
                            local.get 1
                            local.set 6
                            local.get 5
                            local.set 2
                            local.get 8
                            local.get 12
                            i32.const -4
                            i32.and
                            i32.sub
                            i32.const -4
                            i32.and
                            local.set 3
                            br 6 (;@6;)
                          end
                          local.get 1
                          local.set 6
                          local.get 5
                          local.set 2
                          local.get 12
                          i32.const 2
                          i32.shr_u
                          local.get 8
                          i32.const -4
                          i32.and
                          i32.mul
                          local.set 3
                          br 5 (;@6;)
                        end
                        local.get 1
                        local.set 6
                        local.get 5
                        local.set 2
                        i32.const 7
                        i32.const 3
                        local.get 8
                        local.get 12
                        i32.eq
                        select
                        local.set 3
                        br 4 (;@6;)
                      end
                      local.get 1
                      local.set 6
                      local.get 5
                      local.set 2
                      i32.const 7
                      i32.const 3
                      local.get 8
                      i32.const 2
                      i32.shr_s
                      local.get 12
                      i32.const 2
                      i32.shr_s
                      i32.lt_s
                      select
                      local.set 3
                      br 3 (;@6;)
                    end
                    local.get 1
                    local.set 6
                    local.get 5
                    local.set 2
                    i32.const 7
                    i32.const 3
                    local.get 8
                    i32.const 3
                    i32.eq
                    select
                    local.set 3
                    br 2 (;@6;)
                  end
                  local.get 1
                  local.set 6
                  local.get 5
                  local.set 2
                  i32.const 7
                  i32.const 3
                  local.get 8
                  i32.const 3
                  i32.and
                  i32.const 1
                  i32.eq
                  select
                  local.set 3
                  br 1 (;@6;)
                end
                local.get 1
                local.set 6
                local.get 5
                local.set 2
                i32.const 7
                i32.const 7
                i32.const 3
                local.get 8
                i32.const 3
                i32.and
                i32.const 1
                i32.eq
                select
                local.get 8
                i32.const 3
                i32.eq
                select
                local.set 3
              end
              local.get 4
              local.get 19
              i32.const -1
              i32.xor
              i32.add
              i32.const 2
              i32.shl
              i32.const 2397376
              i32.add
              local.get 3
              i32.store
              local.get 6
              local.set 1
              local.get 2
              local.set 6
              i32.const 1
              local.set 17
              local.get 10
              local.set 3
              local.get 14
              local.set 4
              local.get 9
              local.set 8
              local.get 18
              local.set 9
            end
            local.get 7
            local.set 10
          end
          local.get 1
          local.tee 11
          local.set 1
          local.get 6
          local.tee 7
          local.set 2
          local.get 3
          local.tee 13
          local.set 3
          local.get 4
          local.tee 4
          local.set 5
          local.get 8
          local.tee 16
          local.set 12
          local.get 9
          local.tee 18
          local.set 14
          local.get 10
          local.tee 19
          local.set 15
          local.get 11
          local.set 6
          local.get 7
          local.set 8
          i32.const 0
          local.set 11
          local.get 13
          local.set 10
          local.get 4
          local.set 7
          local.get 16
          local.set 9
          local.get 18
          local.set 13
          local.get 19
          local.set 16
          local.get 17
          i32.eqz
          br_if 1 (;@2;)
        end
        local.get 1
        local.set 6
        local.get 2
        local.set 8
        i32.const 1
        local.set 11
        local.get 3
        local.set 10
        local.get 5
        local.set 7
        local.get 12
        local.set 9
        local.get 14
        local.set 13
        local.get 15
        local.set 16
      end
      local.get 6
      local.set 1
      local.get 8
      local.set 2
      local.get 10
      local.set 3
      local.get 7
      local.set 4
      local.get 9
      local.set 5
      local.get 13
      local.set 6
      local.get 16
      local.tee 9
      local.set 8
      local.get 9
      local.set 9
      local.get 11
      br_if 0 (;@1;)
    end
    local.get 9)
  (func $print_val (type 3) (param i32)
    (local i32 i32 i32 i32 i32)
    global.get $__stack_pointer
    i32.const 16
    i32.sub
    local.tee 1
    global.set $__stack_pointer
    block  ;; label = @1
      block  ;; label = @2
        block  ;; label = @3
          local.get 0
          i32.const 3
          i32.and
          local.tee 2
          br_if 0 (;@3;)
          block  ;; label = @4
            block  ;; label = @5
              local.get 0
              i32.const 2
              i32.shr_s
              local.tee 0
              i32.const -1
              i32.le_s
              br_if 0 (;@5;)
              local.get 0
              local.set 0
              br 1 (;@4;)
            end
            block  ;; label = @5
              i32.const 0
              i32.load offset=13448
              local.tee 2
              i32.const 4095
              i32.gt_u
              br_if 0 (;@5;)
              local.get 2
              i32.const 9344
              i32.add
              i32.const 45
              i32.store8
              i32.const 0
              local.get 2
              i32.const 1
              i32.add
              i32.store offset=13448
            end
            i32.const 0
            local.get 0
            i32.sub
            local.set 0
          end
          local.get 0
          local.tee 0
          i32.eqz
          br_if 1 (;@2;)
          i32.const 0
          local.set 2
          local.get 0
          local.set 3
          loop  ;; label = @4
            local.get 1
            i32.const 4
            i32.add
            local.get 2
            local.tee 2
            i32.add
            local.get 3
            local.tee 0
            local.get 0
            i32.const 10
            i32.div_u
            local.tee 3
            i32.const 10
            i32.mul
            i32.sub
            i32.const 48
            i32.or
            i32.store8
            local.get 2
            i32.const 1
            i32.add
            local.tee 4
            local.set 2
            local.get 3
            local.set 3
            local.get 0
            i32.const 10
            i32.ge_u
            br_if 0 (;@4;)
          end
          local.get 1
          i32.const 4
          i32.add
          i32.const -1
          i32.add
          local.set 5
          local.get 4
          local.set 0
          i32.const 0
          i32.load offset=13448
          local.set 2
          loop  ;; label = @4
            local.get 0
            local.set 0
            block  ;; label = @5
              block  ;; label = @6
                local.get 2
                local.tee 2
                i32.const 4095
                i32.le_u
                br_if 0 (;@6;)
                local.get 2
                local.set 2
                br 1 (;@5;)
              end
              i32.const 0
              local.get 2
              i32.const 1
              i32.add
              local.tee 3
              i32.store offset=13448
              local.get 2
              i32.const 9344
              i32.add
              local.get 5
              local.get 0
              i32.add
              i32.load8_u
              i32.store8
              local.get 3
              local.set 2
            end
            local.get 0
            i32.const -1
            i32.add
            local.tee 3
            local.set 0
            local.get 2
            local.set 2
            local.get 3
            br_if 0 (;@4;)
            br 3 (;@1;)
          end
        end
        block  ;; label = @3
          block  ;; label = @4
            block  ;; label = @5
              local.get 0
              i32.const -3
              i32.add
              br_table 0 (;@5;) 2 (;@3;) 2 (;@3;) 2 (;@3;) 1 (;@4;) 2 (;@3;)
            end
            i32.const 1
            local.set 2
            i32.const 40
            local.set 0
            i32.const 0
            i32.load offset=13448
            local.set 3
            loop  ;; label = @5
              local.get 0
              local.set 4
              local.get 2
              local.set 0
              block  ;; label = @6
                block  ;; label = @7
                  local.get 3
                  local.tee 2
                  i32.const 4095
                  i32.le_u
                  br_if 0 (;@7;)
                  local.get 2
                  local.set 3
                  br 1 (;@6;)
                end
                local.get 2
                i32.const 9344
                i32.add
                local.get 4
                i32.store8
                i32.const 0
                local.get 2
                i32.const 1
                i32.add
                local.tee 2
                i32.store offset=13448
                local.get 2
                local.set 3
              end
              local.get 0
              i32.const 1
              i32.add
              local.tee 4
              local.set 2
              local.get 0
              i32.const 1137
              i32.add
              i32.load8_u
              local.set 0
              local.get 3
              local.set 3
              local.get 4
              i32.const 3
              i32.ne
              br_if 0 (;@5;)
              br 4 (;@1;)
            end
          end
          i32.const 0
          i32.load offset=13448
          local.tee 0
          i32.const 4095
          i32.gt_u
          br_if 2 (;@1;)
          local.get 0
          i32.const 9344
          i32.add
          i32.const 116
          i32.store8
          i32.const 0
          local.get 0
          i32.const 1
          i32.add
          i32.store offset=13448
          br 2 (;@1;)
        end
        block  ;; label = @3
          local.get 0
          i32.const -5
          i32.and
          i32.const 11
          i32.ne
          br_if 0 (;@3;)
          i32.const 1
          local.set 2
          i32.const 60
          local.set 0
          i32.const 0
          i32.load offset=13448
          local.set 3
          loop  ;; label = @4
            local.get 0
            local.set 4
            local.get 2
            local.set 0
            block  ;; label = @5
              block  ;; label = @6
                local.get 3
                local.tee 2
                i32.const 4095
                i32.le_u
                br_if 0 (;@6;)
                local.get 2
                local.set 3
                br 1 (;@5;)
              end
              local.get 2
              i32.const 9344
              i32.add
              local.get 4
              i32.store8
              i32.const 0
              local.get 2
              i32.const 1
              i32.add
              local.tee 2
              i32.store offset=13448
              local.get 2
              local.set 3
            end
            local.get 0
            i32.const 1
            i32.add
            local.tee 4
            local.set 2
            local.get 0
            i32.const 1098
            i32.add
            i32.load8_u
            local.set 0
            local.get 3
            local.set 3
            local.get 4
            i32.const 8
            i32.ne
            br_if 0 (;@4;)
            br 3 (;@1;)
          end
        end
        block  ;; label = @3
          local.get 2
          i32.const 3
          i32.ne
          br_if 0 (;@3;)
          local.get 0
          i32.const -16
          i32.add
          i32.const 43
          i32.gt_u
          br_if 0 (;@3;)
          i32.const 1
          local.set 2
          i32.const 60
          local.set 0
          i32.const 0
          i32.load offset=13448
          local.set 3
          loop  ;; label = @4
            local.get 0
            local.set 4
            local.get 2
            local.set 0
            block  ;; label = @5
              block  ;; label = @6
                local.get 3
                local.tee 2
                i32.const 4095
                i32.le_u
                br_if 0 (;@6;)
                local.get 2
                local.set 3
                br 1 (;@5;)
              end
              local.get 2
              i32.const 9344
              i32.add
              local.get 4
              i32.store8
              i32.const 0
              local.get 2
              i32.const 1
              i32.add
              local.tee 2
              i32.store offset=13448
              local.get 2
              local.set 3
            end
            local.get 0
            i32.const 1
            i32.add
            local.tee 4
            local.set 2
            local.get 0
            i32.const 1106
            i32.add
            i32.load8_u
            local.set 0
            local.get 3
            local.set 3
            local.get 4
            i32.const 12
            i32.ne
            br_if 0 (;@4;)
            br 3 (;@1;)
          end
        end
        block  ;; label = @3
          local.get 2
          i32.const 2
          i32.ne
          br_if 0 (;@3;)
          local.get 0
          i32.const -4
          i32.and
          i32.const 33984
          i32.add
          i32.load
          local.tee 2
          i32.eqz
          br_if 2 (;@1;)
          local.get 0
          i32.const 2
          i32.shr_u
          i32.const 4
          i32.shl
          i32.const 17600
          i32.add
          local.set 0
          local.get 2
          local.set 2
          i32.const 0
          i32.load offset=13448
          local.set 3
          loop  ;; label = @4
            local.get 2
            local.set 4
            local.get 0
            local.set 0
            block  ;; label = @5
              block  ;; label = @6
                local.get 3
                local.tee 2
                i32.const 4095
                i32.le_u
                br_if 0 (;@6;)
                local.get 2
                local.set 3
                br 1 (;@5;)
              end
              i32.const 0
              local.get 2
              i32.const 1
              i32.add
              local.tee 3
              i32.store offset=13448
              local.get 2
              i32.const 9344
              i32.add
              local.get 0
              i32.load8_u
              i32.store8
              local.get 3
              local.set 3
            end
            local.get 0
            i32.const 1
            i32.add
            local.set 0
            local.get 4
            i32.const -1
            i32.add
            local.tee 4
            local.set 2
            local.get 3
            local.set 3
            local.get 4
            br_if 0 (;@4;)
            br 3 (;@1;)
          end
        end
        local.get 2
        i32.const 1
        i32.ne
        br_if 1 (;@1;)
        block  ;; label = @3
          local.get 0
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          i32.const 38080
          i32.add
          i32.load
          i32.const 0
          i32.load offset=17592
          i32.ne
          br_if 0 (;@3;)
          i32.const 1
          local.set 2
          i32.const 60
          local.set 0
          i32.const 0
          i32.load offset=13448
          local.set 3
          loop  ;; label = @4
            local.get 0
            local.set 4
            local.get 2
            local.set 0
            block  ;; label = @5
              block  ;; label = @6
                local.get 3
                local.tee 2
                i32.const 4095
                i32.le_u
                br_if 0 (;@6;)
                local.get 2
                local.set 3
                br 1 (;@5;)
              end
              local.get 2
              i32.const 9344
              i32.add
              local.get 4
              i32.store8
              i32.const 0
              local.get 2
              i32.const 1
              i32.add
              local.tee 2
              i32.store offset=13448
              local.get 2
              local.set 3
            end
            local.get 0
            i32.const 1
            i32.add
            local.tee 4
            local.set 2
            local.get 0
            i32.const 1118
            i32.add
            i32.load8_u
            local.set 0
            local.get 3
            local.set 3
            local.get 4
            i32.const 9
            i32.ne
            br_if 0 (;@4;)
            br 3 (;@1;)
          end
        end
        local.get 2
        i32.const 1
        i32.ne
        br_if 1 (;@1;)
        block  ;; label = @3
          i32.const 0
          i32.load offset=13448
          local.tee 2
          i32.const 4095
          i32.gt_u
          br_if 0 (;@3;)
          local.get 2
          i32.const 9344
          i32.add
          i32.const 40
          i32.store8
          i32.const 0
          local.get 2
          i32.const 1
          i32.add
          i32.store offset=13448
        end
        i32.const 0
        local.set 2
        local.get 0
        local.set 3
        loop  ;; label = @3
          local.get 3
          local.set 0
          block  ;; label = @4
            local.get 2
            i32.const 1
            i32.and
            i32.eqz
            br_if 0 (;@4;)
            i32.const 0
            i32.load offset=13448
            local.tee 2
            i32.const 4095
            i32.gt_u
            br_if 0 (;@4;)
            local.get 2
            i32.const 9344
            i32.add
            i32.const 32
            i32.store8
            i32.const 0
            local.get 2
            i32.const 1
            i32.add
            i32.store offset=13448
          end
          local.get 0
          i32.const 1
          i32.shl
          i32.const -8
          i32.and
          local.tee 0
          i32.const 38080
          i32.add
          i32.load
          call $print_val
          i32.const 1
          local.set 2
          local.get 0
          i32.const 38084
          i32.add
          i32.load
          local.tee 0
          local.set 3
          local.get 0
          i32.const 3
          i32.and
          i32.const 1
          i32.eq
          br_if 0 (;@3;)
        end
        block  ;; label = @3
          local.get 0
          i32.const 3
          i32.eq
          br_if 0 (;@3;)
          i32.const 1
          local.set 3
          i32.const 32
          local.set 2
          i32.const 0
          i32.load offset=13448
          local.set 4
          loop  ;; label = @4
            local.get 2
            local.set 5
            local.get 3
            local.set 2
            block  ;; label = @5
              block  ;; label = @6
                local.get 4
                local.tee 3
                i32.const 4095
                i32.le_u
                br_if 0 (;@6;)
                local.get 3
                local.set 4
                br 1 (;@5;)
              end
              local.get 3
              i32.const 9344
              i32.add
              local.get 5
              i32.store8
              i32.const 0
              local.get 3
              i32.const 1
              i32.add
              local.tee 3
              i32.store offset=13448
              local.get 3
              local.set 4
            end
            local.get 2
            i32.const 1
            i32.add
            local.tee 5
            local.set 3
            local.get 2
            i32.const 1140
            i32.add
            i32.load8_u
            local.set 2
            local.get 4
            local.set 4
            local.get 5
            i32.const 4
            i32.ne
            br_if 0 (;@4;)
          end
          local.get 0
          call $print_val
        end
        i32.const 0
        i32.load offset=13448
        local.tee 0
        i32.const 4095
        i32.gt_u
        br_if 1 (;@1;)
        local.get 0
        i32.const 9344
        i32.add
        i32.const 41
        i32.store8
        i32.const 0
        local.get 0
        i32.const 1
        i32.add
        i32.store offset=13448
        br 1 (;@1;)
      end
      i32.const 0
      i32.load offset=13448
      local.tee 0
      i32.const 4095
      i32.gt_u
      br_if 0 (;@1;)
      local.get 0
      i32.const 9344
      i32.add
      i32.const 48
      i32.store8
      i32.const 0
      local.get 0
      i32.const 1
      i32.add
      i32.store offset=13448
    end
    local.get 1
    i32.const 16
    i32.add
    global.set $__stack_pointer)
  (table (;0;) 1 1 funcref)
  (memory (;0;) 512)
  (global $__stack_pointer (mut i32) (i32.const 18725056))
  (global $icount (export "icount") (mut i32) (i32.const 0))
  (export "memory" (memory 0))
  (export "input_ptr" (func $input_ptr))
  (export "output_ptr" (func $output_ptr))
  (export "eval_source" (func $eval_source))
  (data $.rodata (i32.const 1024) "let\00cons\00cdr\00car\00begin\00nil\00quote\00%closure\00define\00lambda\00list?\00pair?\00null?\00<error>\00<primitive>\00<lambda>\00=\00<\00-\00+\00*\00()\00 . \00"))
