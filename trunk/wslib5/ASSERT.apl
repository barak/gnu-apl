#!/usr/local/bin/apl --script

∇Assert B;COND;LOC;VAR
 →(1≡B)⍴0
 ' '
 COND←7↓,¯2 ⎕SI 4
 LOC←,¯2 ⎕SI 3
 '************************************************'
 ' '
 '*** Assertion (', COND, ') failed at ',LOC
 ''

 ⍝ show stack
 ⍝
 ' '
 'Stack:'
 7 ⎕CR ⊃¯1↓⎕SI 3
 ' '
 '************************************************</pre>'
 →
∇


