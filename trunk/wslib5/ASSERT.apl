#!/usr/local/bin/apl --script
 ⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
⍝                                                                    ⍝
⍝ ASSERT                               2025-11-14  12:09:31 (GMT+1)  ⍝
⍝                                                                    ⍝
 ⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝

∇ASSERT B;COND;LOC;VAR
 ⍝
 ⍝⍝ check that B ≡ 1 and complain if not.
 ⍝
 →(1≡B)⍴0                ⍝ OK: B is 1
 COND←7↓,¯2 ⎕SI 4        ⍝ the condition text
 LOC←,¯2 ⎕SI 3           ⍝ where the condition failed
 ' '
 '************************************************'
 ' '
 '*** Assertion (', COND, ') failed at ',LOC
 
 ⍝ show the )SI stack
 ⍝
 ' '
 'SI Stack:' ◊ '─────────' ◊ 7 ⎕CR ' ',' ',' ',⊃¯1↓⎕SI 3
 ' '
 '************************************************</pre>'
 →
∇

∇TEST_ASSERT B
 ASSERT B < 4
 TEST_ASSERT B + 1
∇

