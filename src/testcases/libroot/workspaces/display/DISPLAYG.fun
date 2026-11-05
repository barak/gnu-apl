[ 0] D←S DISPLAYG A;⎕IO;R;C;HL;HC;HT;HB;VL;VB;V;W;N;B
[ 1] ⍝ Display a general array in pictorial form
[ 2] ⍝  Normal call is monadic.  Dyadic call used only in
[ 3] ⍝    recursion to specify display rank, shape, and depth.
[ 4] ⎕IO←0
[ 5] ⍎(0=⎕NC 'S')/'S←⍴A'
[ 6] R←↑⍴,S                   ⍝ Pseudo rank
[ 7] C←'┐┌└┘'                 ⍝ UR, UL, LL, and LR corners
[ 8] HL←'─'                   ⍝ Horizontal line
[ 9] HC←HL,'⊖→',HL,'∼+∈'      ⍝ Horizontal borders
[10] HT←HC[(0<R)×1+0<↑¯1↑,S]
[11] ⍝W←,0≡¨↑0⍴⊂(1⌈⍴A)↑A
[12] HB←HC[3+3⌊('2×∼A≡⍕A' ⎕EA '1+↑∈0×(1⌈×/⍴A)↑,A')+3×1<⍴⍴S]
[13] VL←'│'                   ⍝ Vertical line
[14] VB←VL,'⌽↓'               ⍝ Vertical border
[15] V←VB[(1<R)×1+0<¯1↑¯1↓,S]
[16] ⍎(0∈⍴A)/'A←(1⌈⍴A)⍴⊂↑A'   ⍝ Show prototype of empties
[17] →(1<≡A)/GEN
[18] →(2<⍴⍴A)/D3
[19] D←⍕A                     ⍝ Simple arrays
[20] W←1↑⍴D←(¯2↑1 1,⍴D)⍴D
[21] N←¯1+1↓⍴D
[22] →(0=⍴⍴A)/SS
[23] D←(C[1],V,((W-1)⍴VL),C[2]),((HT,N⍴HL),[0]D,[0]HB,N⍴HL),C[0],(W⍴VL),C[3]
[24] →0
[25] SS:HB←((0 ' ')=↑0⍴⊂A)/' -'
[26] D←' ',(' ',[0]D,[0]HB,N⍴' '),' '
[27] →0
[28] GEN:D←⍕DISPLAYG¨A         ⍝ Enclosed arrays
[29] N←D∨.≠' '
[30] D←(N∨∼1⌽N)⌿D
[31] D←(∨⌿∼'  '⍷D)/D
[32] D←((1,⍴S)⍴S)DISPLAYG D
[33] →(2≥⍴,S)↓D3E,0
[34] D3:D←0 ¯1↓0 1↓⍕⊂A         ⍝ Multi-dimensional arrays
[35] W←1↑⍴D
[36] N←¯1+1↓⍴D
[37] D←(C[1],V,((W-1)⍴VL),C[2]),((HT,N⍴HL),[0]D,[0]HB,N⍴HL),C[0],(W⍴VL),C[3]
[38] D3E:N←¯2+⍴,S
[39] V←C[N⍴1],[0]VB[1+0<¯2↓,S],[0](((¯3+↑⍴D),N)⍴VL),[0]C[N⍴2]
[40] D←V,D
[41] ⍝⌻⍝ APL2/6000 FKEY verification tag - DO NOT REMOVE, MUST BE LAST LINE
