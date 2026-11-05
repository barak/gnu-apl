
]log 25
]log 26

∇Z←ID N
 ⍝ Z is the N×N identity matrix
 Z←N∘.=N←⍳N
∇


∇Z←A CMP_TOL CT_B;B;CT;MX;⎕CT
 ⍝ Z is A with small items (in relation to B) replaced by 0
 (CT B)←CT_B ◊ MX←⌈/∣,B ◊ ⎕CT←CT ◊ Z←A×MX≠MX+A
∇


Failed_Assertions←0
∇Assert B;COND;LOC
 ⍝ check that B is 1 and complain if not
 →(1≡B)⍴0 ◊ Failed_Assertions←Failed_Assertions + 1
 ' '
 COND←7↓,¯2 ⎕SI 4            ⍝ the condition that was tested
 COND←(∧\'⍝'≠COND)/COND      ⍝ with comments removed
 COND←(∼⌽∧\' '=⌽COND)/COND   ⍝ and without trailing spaces
 LOC←,¯2 ⎕SI 3               ⍝ the location where the assertion failed
 →('◊'=↑LOC)↓↑⎕LC+1 ◊ LOC←'IMMEDIATE EXECUTION'
 →('⍎'=↑LOC)↓↑⎕LC+1 ◊ LOC←' EXECUTE (⍎)'
 ' '
 '*** Assertion:  ', COND, '  failed at: ', LOC, ' ***'
∇

⎕←B←5 4⍴4J6 6J3 5J10 3J2 8J10 3J4 5J10 5J8 3J1 2J3 4J5 1J3 1J4 9J9 9J6 2J7 2J10 7J6 9J8 10J10

⎕←B←?4 3⍴⍳10
⎕←B←3 3⍴ 1 1 3 2 4 2 4 8 7

(Q R Ri)←⌹[⎕CT]B ◊ Q←⍉Q
4 ⎕CR 0 4⍕ Q ◊ Q

⍝ verify that Q is orthogonal
Assert ~0∈,(ID ↑⍴Q)=(Q +.×⍉Q) CMP_TOL ⎕CT 1

⍝ verify that R is upper triangle...
U←1E¯12<∣R ◊ UT←⊃(∘.≤)/⍳¨⍴R ◊ Assert ∼0∈,U≤UT

⍝ verify factorization
Assert B≡Q+.×R

R←R  CMP_TOL ⎕CT 1
4 ⎕CR 0 4⍕ R

Q+.×R

(⍕Failed_Assertions) 'failed Assertions'

'Performance'
 B1←?(N,N)⍴N←10
 ('⌹B       (⍴B=', (⍕N), '×', (⍕N), '):') ({ ⌹⍵      } ⎕FIO[¯1] B1)
 ('⌹B[⎕CT]  (⍴B=', (⍕N), '×', (⍕N), '):') ({ ⌹[⎕CT]⍵ } ⎕FIO[¯1] B1)

4 ⎕CR 0 4⍕ Ri
⍝ verify inversion of R
4 ⎕CR 0 4⍕ Ri +.×R


)OFF

