#!./apl --script

      )WSID GPROF_LOOP

∇LOOP COUNT
L: ⊣A⌹B ◊ →(0 < COUNT←COUNT-1 )⍴L
∇

      loop_count←1000000         ⍝ keep APL busy with A ⌹ B
      N←10                       

      A←⎕RVAL 1 (,N) (0 0 0 1)    ⍝ a random 5 element complex vector
      B←⎕RVAL 2 (N,N) (0 0 0 1)   ⍝ a random 5×5 complex matrix

      LOOP loop_count   ⍝ run the loop loop_count times

      )OFF
