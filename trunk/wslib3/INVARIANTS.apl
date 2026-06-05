#!./apl --script

      0 0⍴ «««

      This workspace provides function INV∆CHECK and INV∆EQUIV,
      which are helper functions for testing invariants.

      DESCRIPTION for INV∆CHECK
      -------------------------

      This workspace provides function INV∆CHECK and INV∆EQUIV, which are
      helper functions for testing invariants. Use it like e.g.:

      )COPY_ONCE 3 INVARIANTS.apl   ⍝ defines INV∆CHECK
      ...
      A INV∆CHECK B      (simple invariant string A)  -or-
      A INV∆CHECK[X] B   (invariant  string A with interator)
      ...
      A INV_EQUIV B      (values A and B are equivalent)

      where:

      A is a text string, usually taken literally from the IBM APL2
      Language Reference Manual (aka. lrm) that claims an invariant.

      B is (L R Z) the optional left argument L, the mandatory right argument R,
      and the result Z of some function for which the invariant is claimed.

      X is an optional string that may specify an iterator. If X is present,
      then the variable I (in the invaliant) is set to its values and the
      invariant is then sequentially checked with every item in I.

      If the invariant holds, then INV∆CHECK remains silent. Otherwise it
      complains and displays some details as to how it was called.
      This makes INV∆CHECK useful in priticular for automated testcases.

      Note that lrm uses L and R (and sometimes others, like H below)
      for functions arguments. In contrast GNU APL (and also the ISO
      standard 13751) use A and B. The GNU APL testcases typically use
      the same variables in lrm for examples from lst and A and B for
      examples from somewhere else.

      Note also, that some invariants claimed in lrm are actually not
      (at least according to the PC version of IBM APL2, version 1.0,
      service level 7).

      EXAMPLES for INV∆CHECK
      ----------------------

      Page 251 of lrm claims two (correct) invariants for the APL primitive
      function Transpose - dyadic ⍉ - in the case of repeated axes in A:

      I⊃ρZ ←→ ⌊/(L=I)/ρR    for each IϵιρρZ    invariant #1 (iterator IϵιρρZ)
      ⍴⍴Z  ←→ ,+/(A⍳A) = ⍳⍴A                   invariant #2 (without iterator)

      These invariants can be verified for the examples given in lrm like this:

      A←1 2 1                                  ⍝ arguments from example...
      H←2 3 4ρ'ABCDEFGHIJKL',ι12
      Z← (A←1 2 1) ⍉ (H←2 3 4ρ'ABCDEFGHIJKL',ι12)

      INV1 ← "I⊃ρZ ←→ ⌊/(L=I)/ρR"              ⍝ invariant #1
      ITER ← "⍴⍴L"                             ⍝ iterator for invariant #1
      INV2 ← "⍴⍴Z  ←→ ,+/(L⍳L) = ⍳⍴L"          ⍝ invariant 32 (without iterator)

      INV1  INV∆CHECK[ITER]  A H Z←A⍉H     ⍝ test invariant #1
      INV2  INV∆CHECK        A H Z←A⍉H     ⍝ test invariant #2


      DESCRIPTION for INV∆EQUIV
      -------------------------
      A INV∆EQUIV B checs that A ≡ B and complains if not. Unlike INV∆CHECK,
      A and B are APL values (not strings) that should be equivalent.

      EXAMPLES for INV∆EQUIV
      ----------------------

             'FACE' INV∆EQUIV ('F' 'A' 'C' 'E')
      ('IN')('OUT') INV∆EQUIV ('IN')'OUT'

           »»»

⍝-----------------------------------------------------------------------------
∇A INV∆CHECK[X] B;I;L;R;A1;A2;Z1;Z2;⎕CT
 ⍝
 ⍝⍝ check that the invariant A is valid for (L R Z)←B, possibly for
 ⍝⍝ multiple I as defined by X. Be quiet if so, complain if not.
 ⍝
 (R Z)←¯2↑B ◊ L←↑B
 ⍎(2≠⎕NC 'X')/"X←',0'" ◊ ⍎'X←',X          ⍝ set X ← ,0 if not provided

   ⎕CT←1E¯10   ⍝ otherwise some tests fail

 ⍝ split string A (the invariant) into A1 left of ←→ and A2 right of ←→
 (A1 A2)←(1+∨\"←→"⍷A)⊂A ◊ A2←2↓A2

LOOP_I: I←↑X ◊ X←1↓X
 Z1←⍎A1 ◊ Z2←⍎A2 ◊ →(Z1≢Z2)/ERROR         ⍝ compute and compare

 →(0≠⍴X)⍴LOOP_I ◊ →0   ⍝ DONE

ERROR:
 ⊢ZZ←"FAILED INVARIANT:    ", A
 (0 0⍴0) ⊢[2=⎕NC 'A' ] 'L:' (8 ⎕CR L)
 'R:' (8 ⎕CR R)
∇

⍝-----------------------------------------------------------------------------
∇Z←L INV∆STRUCT R
⍝⍝ 1 iff L and R have identical structure: same shape and nesting at every
⍝⍝ level. Values are not compared here.
 Z←0
 →((⍴L)≢⍴R)⍴0                       ⍝ shapes must agree
 →((≡L)≠≡R)⍴0                        ⍝ depths must agree
 →(1≥≡L)⍴OK                          ⍝ simple (depth ≤ 1): structure OK
 Z←∧/(,L){⍺ INV∆STRUCT ⍵}¨,R        ⍝ nested: recurse on each item pair
 →0
OK: Z←1
∇

⍝-----------------------------------------------------------------------------
∇Z←L INV∆NEAR R
⍝⍝ 1 iff L and R are equivalent: structure checked recursively via INV∆STRUCT,
⍝⍝ all leaf values compared via (∊L)=∊R — = uses ⎕CT for numerics and
⍝⍝ works for characters without DOMAIN ERROR.
 Z←0
 →(~Z←L INV∆STRUCT R)⍴0    ⍝ structure must match exactly
 Z←∧/(∊L)=∊R               ⍝ all leaf values agree (⎕CT for nums, exact for chars)
∇

⍝-----------------------------------------------------------------------------
∇L INV∆EQUIV R
⍝⍝ Check that L and R are equivalent (see INV∆NEAR), complain if not.
⍝⍝ Structure (shape, nesting) is checked exactly; numeric values within ⎕CT.
 →(L ≡ R)⍴0 ⍝ OK (fast path: exact match)
 →(L INV∆NEAR R)⍴0 ⍝ OK (slow path: match within ⎕CT)
 '*** L INV∆EQUIV R failed'

 '≡L:'       (≡L)
 '≡R:'       (≡R)
 '⍴L:'       (⍴L)
 '⍴R:'       (⍴R)
 'L:'        (L)
 'R:'        (R)
 '4 ⎕CR L:'  (4 ⎕CR L)
 '4 ⎕CR R:'  (4 ⎕CR R)
 '10 ⎕CR L:' (10 ⎕CR 'L')
 '10 ⎕CR R:' (10 ⎕CR 'R')
 '2 ⎕TF L:'  (2 ⎕TF 'L')
 '2 ⎕TF R:'  (2 ⎕TF 'R')
 →((⍴L)≢⍴R)⍴0   ⍝ cannot compare element-wise if shapes differ
 'L=R:'     (L=R)
∇

