        i32.const 0
        local.set 2
        loop  ;; label = @3
          local.get 1
          local.tee 3
          i32.const 1
          i32.add
          local.set 1
          local.get 3
          i32.const 2
          i32.shl
          local.tee 4
          i32.const 2986112
          i32.add
          local.set 5
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
                                    local.get 4
                                    i32.load offset=2986112
                                    br_table 11 (;@5;) 0 (;@16;) 1 (;@15;) 2 (;@14;) 3 (;@13;) 4 (;@12;) 5 (;@11;) 6 (;@10;) 7 (;@9;) 8 (;@8;) 9 (;@7;) 10 (;@6;) 12 (;@4;)
                                  end
                                  local.get 5
                                  i32.const 8
                                  i32.add
                                  local.set 8
                                  local.get 7
                                  local.set 4
                                  block  ;; label = @16
                                    local.get 1
                                    i32.const 2
                                    i32.shl
                                    i32.load offset=2986112
                                    local.tee 1
                                    i32.eqz
                                    br_if 0 (;@16;)
                                    local.get 1
                                    local.set 6
                                    block  ;; label = @17
                                      local.get 1
                                      i32.const 1
                                      i32.and
                                      i32.eqz
                                      br_if 0 (;@17;)
                                      local.get 1
                                      i32.const 1
                                      i32.sub
                                      local.set 6
                                      i32.const 3
                                      local.set 4
                                      local.get 7
                                      i32.const 3
                                      i32.and
                                      i32.const 1
                                      i32.ne
                                      br_if 0 (;@17;)
                                      local.get 7
                                      i32.const 1
                                      i32.shl
                                      i32.const -8
                                      i32.and
                                      i32.load offset=102500
                                      local.set 4
                                    end
                                    local.get 1
                                    i32.const 1
                                    i32.eq
                                    br_if 0 (;@16;)
                                    local.get 4
                                    local.set 5
                                    loop  ;; label = @17
                                      i32.const 3
                                      local.set 4
                                      i32.const 3
                                      local.set 1
                                      local.get 5
                                      i32.const 3
                                      i32.and
                                      i32.const 1
                                      i32.eq
                                      if  ;; label = @18
