#!./apl --script

      ⍝⍝ This workspace creates a number of variables that are
      ⍝⍝ used in the examples in ISO standard 13751:2000 (E).

      ⍝ numeric variabes (N2, N3, N5, N34, N233, N234
      ⍝
      ⊣'N' { ⍺←⍺,(⍕⍵),'←' ◊ ⍵←10⊥¨⍳((⍴⍕⍵)⍴10)⊤⍵ ◊ ⍎⍺,'⍵' } ¨ 2 3 5 34 233 234

      ⍝ string variables (N6)
      ⊣'c' { ⍺←⍺,(⍕⍵),'←' ◊ ⍵←⎕UCS ¨96+⍳((⍴⍕⍵)⍴10)⊤⍵ ◊ ⍎⍺,'⍵' } ¨ 6

