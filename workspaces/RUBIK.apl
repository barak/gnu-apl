#!apl --script

⍝ See DESCRIPTION and/or PROGRAMMER's REFERENCE at the end of this file.

⎕PW←10000 ⍝ don't wrap APL output

⍝------------------------------------------------------------------------
∇Z←Xterm_Color B
 ⍝
 ⍝⍝ B is a single character that designates a color, i.e. Bϵ'WYORBGbw' which
 ⍝⍝ stands for White/Yellow/Orange/Red/Blue/green/black/white respectively.
 ⍝⍝ return the XTerm ESC sequence for it.
 ⍝
 →((↑B,'-')='WYORBGbw-')/cW cY cO cR cB cG cb cw cw ◊ 4 ⎕CR B ◊ +++
cW: Z←'8;2;255;255;255' ◊ →0 ◊ white  cube face
cY: Z←'8;2;255;213;85'  ◊ →0 ◊ yellow cube face
cO: Z←'8;2;255;88;0'    ◊ →0 ◊ orange cube face 
cR: Z←'8;2;196;30;58'   ◊ →0 ◊ red    cube face 
cB: Z←'8;2;0;0;224'     ◊ →0 ◊ blue   cube face 
cG: Z←'8;2;0;158;96'    ◊ →0 ◊ green  cube face 
cb: Z←'8;2;0;0;0'       ◊ →0 ◊ black  foreground (text color)
cw: Z←'8;2;176;176;176' ◊ →0 ◊ grey   cube background
∇
⍝------------------------------------------------------------------------
∇Z←foreground Bg_Fg
 ⍝
 ⍝⍝ Return the foreground for Bg_Fg. For (Bg Fg) the foreground is Fg,
 ⍝⍝ while for a single color Bg it is a color with good contrast
 ⍝
 Z←⍬⍴¯1↑Bg_Fg←,Bg_Fg ◊ →(2=⍴Bg_Fg)/0
 Z←(8 2⍴ 'WbYbObRwBwGwwb-w') ⎕MAP ⍬⍴Z
∇
⍝------------------------------------------------------------------------
∇Z←A print_field Bg_Fg;Bg;Fg
 ⍝
 ⍝⍝ Z is string A prefixed with the Xterm color string for the given
 ⍝⍝ background/foreground colors Bg_Fg. The foreground in Bg_Fg is optional
 ⍝⍝ and will be computed from the background color if ommitted.
 ⍝
 Bg←1↑Bg_Fg ◊ Fg←foreground Bg_Fg
 Z←(⎕UCS 27),'[0'                      ⍝ normal mode
 Z←Z,';3', Xterm_Color Fg    ⍝ foreground color
 Z←Z,';4', Xterm_Color Bg    ⍝ background color
 Z←Z,'m', A
∇
⍝------------------------------------------------------------------------
∇Z←find_initial_pos Color_XYZ;CX;CY;CZ
 ⍝
 ⍝⍝ find the initial position of the cube with colors Color_XYZ
 ⍝
 CX←¯1↑'-',(Color_XYZϵ"RO")/Color_XYZ
 CY←¯1↑'-',(Color_XYZϵ"BG")/Color_XYZ
 CZ←¯1↑'-',(Color_XYZϵ"WY")/Color_XYZ
 Z←''⍴(∧/∆Solved_State=[2]CX,CY,CZ)/⍳27
 →(Zϵ⍳27)/0 ◊ 4 ⎕CR RGB ◊ +++
∇
⍝------------------------------------------------------------------------
∇Z←pattern B;DX;DY;DZ;W;S1;Sz;Lx;Ly;Lz;RH;RH1;Rx;Ry;Diag;DPx;DP
 ⍝
 ⍝⍝ Z is a 3D layout (aka. a View) that can be combined with a State
 ⍝⍝ (in function 'interpret_all' in order to display the State in a colored
 ⍝⍝ 3D fashion in an Xterm (-window).
 ⍝⍝
 ⍝⍝ The argument B specifies the lengths (in characters) of the sub-cubes
 ⍝⍝ in the X, Y, and Z dimensions
 ⍝
 (DX DY DZ)←3×(Lx Ly Lz)←B ◊ S1←¯1+W←DX + DZ ◊ Sz←-S1×⎕IO-⍳Lz
 Diag←DZ+S1×¯1+⍳DZ

 Z←(W×DY+DZ)⍴0   ⍝ Background

 ⍝ top (Z-) rhombus
 ⍝
 Ry←S1×Lz ◊ RH1←(2×Lx) + 0 ¯1↓RH← Ry + DZ + Sz ∘.+ ⍳Lx
 Z[RH-Ry]←307 ◊ Z[RH+Lx-Ry]←308  ◊ Z[RH1-Ry]←309
 Z[RH   ]←304 ◊ Z[RH+Lx   ]←305  ◊ Z[RH1   ]←306
 Z[RH+Ry]←301 ◊ Z[RH+Lx+Ry]←302  ◊ Z[RH1+Ry]←303
 DP←⌊ DZ+(Lx÷2)+S1×⌊Lz÷2 ◊ DP←DP, DP+1 2×Lx ◊ DP←DP, (DP+Lz×S1), DP+2×Lz×S1
 Z[DP]←Z[DP]+1000 ◊ Z[DP+1]←Z[DP+1]+2000

 ⍝ left (X-) rhombus
 ⍝
 Ry←W×Ly ◊ Rx←-Lz×S1 ◊ RH1←Ry + ¯1 0↓RH←Rx + Ry + 1+DX+(W×DZ-1)+(W×⍳Ly)∘.-Sz
 Z[RH-Ry+Rx]←103 ◊ Z[RH-Ry]←106 ◊ Z[RH +Rx-Ry]←109
 Z[RH   -Rx]←112 ◊ Z[RH   ]←115 ◊ Z[RH +Rx   ]←118
 Z[RH1  -Rx]←121 ◊ Z[RH1  ]←124 ◊ Z[RH1+Rx   ]←127
 DP←(W×DY) ◊ DP←DP,(DP-Lz×W),DP-2×Lz×W ◊ DP←DP, (DP + Lz×S1), DP + 2×Lz×S1
 Z[DP]←Z[DP]+2000 ◊ Z[DP-1]←Z[DP-1]+1000

 ⍝ diagonals
 ⍝
 Z[Diag]←Lz/407 404 401 ◊ Z[Diag+1]←Z[Diag+1] + 4000 ◊ Diag←Diag+Lx
 Z[Diag]←Lz/507 504 501 ◊ Z[Diag+1]←Z[Diag+1] + 4000 ◊ Diag←Diag+Lx
 Z[Diag]←Lz/508 505 502 ◊ Z[Diag+1]←Z[Diag+1] + 4000 ◊ Diag←Diag+Lx
 
 Z[Diag]←Lz/609 606 603 ◊ Z[1↓Diag+1]←Z[1↓Diag+1] + 4000 ◊ Diag←Diag+W×Ly
 Z[Diag]←Lz/709 706 703 ◊ Z[1↓Diag+1]←Z[1↓Diag+1] + 4000 ◊ Diag←Diag+W×Ly
 Z[Diag]←Lz/718 715 712 ◊ Z[1↓Diag+1]←Z[1↓Diag+1] + 4000 ◊ Diag←Diag+W×Ly

 Z[Diag]←Lz/827 824 821   ⍝ lower diag

 ⍝ front (Y-) square
 ⍝
 Z←((DY+DZ), W)⍴Z
 Z[DZ + ⍳DY;⍳DX]←Ly⌿Lx/3 3⍴200+0 9 18∘.+1 2 3
 DP←DZ+⌈Ly × 0.5 1.5 2.5 ◊ DPx←⌈Lx × 0.5 1.5 2.5
 Z[DP; DPx]←Z[DP; DPx]+1000 ◊ Z[DP; DPx+1]←Z[DP; DPx+2] + 2000

 Z[DZ + ⍳DY;1+Lx × 0 1 2] ←Z[DZ + ⍳DY;1+Lx × 0 1 2] + 4000
 Z[DZ + ⍳DY;1+DX]←Z[DZ + ⍳DY;1+DX] + 4000
 Z[(DZ-Lz) + ⍳DY;1+DX+Lz]←Z[(DZ-Lz) + ⍳DY;1+DX+Lz] + 4000
 Z[(DZ-2×Lz) + ⍳DY;1+DX+2×Lz]←Z[(DZ-2×Lz) + ⍳DY;1+DX+2×Lz] + 4000
∇
⍝------------------------------------------------------------------------
∇Z←State interpret_char B;M1;Mode;Type;Pos
 →(B≠0)/1+⎕LC ◊ Z←' ' print_field 'wB' ◊ →0   ⍝ Background

 (M1 Mode Type Pos)←10 4 10 100⊤B
 Z←↑Mode↓' ',¯2↑⍕find_initial_pos State[Pos;]
 →(F,F,F,D4,D5,D6,D7,D8)[Type]

D4: Z←'◢' print_field 'w',State[Pos;3  ]     ◊ →0
D5: Z←'◢' print_field State[Pos+0 1;3  ]     ◊ →0
D6: Z←'◢' print_field State[Pos    ;3 1]     ◊ →0
D7: Z←'◢' print_field State[Pos+0 9;  1]     ◊ →0
D8: Z←'◢' print_field State[Pos    ;1  ],'w' ◊ →0

F:  →M1↓0 ◊ Z←Z print_field State[Pos; Type]
∇
⍝------------------------------------------------------------------------
∇Z←State interpret_all View;L;Max_L;C;Max_C;ZC;Row
 Z←'' ◊ L←1 ◊ Max_L←↑⍴View
LOOP_L: ZC←'' ◊ C←1 ◊ Max_C←⍴Row←View[L;]
LOOP_C: ZC←ZC,State interpret_char Row[C] ◊ →(Max_C≥C←C+1)/LOOP_C
 Z←Z,⊂ZC ◊ →(Max_L≥L←L+1)/LOOP_L
∇
⍝------------------------------------------------------------------------
∇Z←Face mirror Z
 ⍝
 ⍝⍝ mirror Z horizontally (Face=2 or 5), vertically (Face=1) or not at all.
 ⍝
 →(Face=3 4 6 1)/0 0 0,1+↑⎕LC ◊ Z←⌽Z ◊ →0
 Z←⊖Z
∇
⍝------------------------------------------------------------------------
∇Z←State face_text B;VPos;VAxis;Color_XYZ;Face;Pos
 ⍝
 ⍝⍝ Z is the 2D text (= the facet number) given the 3D position and view axis
 ⍝
 (VPos VAxis)←B ◊ Face←'YOBRGW'⍳(Color_XYZ←State[VPos;])[VAxis]
⍝ → Face_position   ⍝ comment this line tu show cubicle position

Face_number:   ⍝ number faces from 1..48 (6 faces with 8 numbers)
 Pos←1+(⌽3 3 3⊤¯1+find_initial_pos Color_XYZ)[⌽1 2 3∼(3 1 2 1 2 3)[Face]]
 Z←¯2↑'ULFRBD'[Face] ◊ →(Pos≡2 2)/0   ⍝ Pos≡2 2 is center cube
 Z←¯2↑⍕(8×Face-1)+Z-5<Z←Pos ⌷ Face mirror 3 3⍴⍳9
 → 0

Face_position: ⍝ number the faces with their cubicle index
 Z←2 0 ⍕ VPos
∇
⍝------------------------------------------------------------------------
∇Z←State row_2D B;Axis;Pos;Face;Row
 ⍝
 ⍝⍝ Z is one row (3 cubes) of a 2D face
 ⍝
 (Face Row)←B ◊ (Pos Axis)←Face ◊ Z←2 12⍴' '
 Z[;1]←⊂' ' print_field State[Pos[Row;1]; Axis]
 Z[;5]←⊂' ' print_field State[Pos[Row;2]; Axis]
 Z[;9]←⊂' ' print_field State[Pos[Row;3]; Axis]
 Z[2; 2  3]←State face_text Pos[Row;1] Axis
 Z[2; 6  7]←State face_text Pos[Row;2] Axis
 Z[2;10 11]←State face_text Pos[Row;3] Axis
∇
⍝------------------------------------------------------------------------
∇A show_cube State;View1;View2;Z1;Z2;F
 ⍝
 ⍝⍝ display the entire view, either in 2D or in 3D
 ⍝
 →A/V3D
 ⍝ 2-dimensional cube view...
 ⍝
 '' print_field 'w'
 Z1←2 12⍴⊂' ' print_field 'w' ◊ F←'' print_field 'w'
 Z2←Z1, State row_2D (face 1) 1 ◊ ϵZ2[1;],F ◊ ϵZ2[2;],F
 Z2←Z1, State row_2D (face 1) 2 ◊ ϵZ2[1;],F ◊ ϵZ2[2;],F
 Z2←Z1, State row_2D (face 1) 3 ◊ ϵZ2[1;],F ◊ ϵZ2[2;],F

 Z2←    State row_2D (face 2) 1
 Z2←Z2, State row_2D (face 3) 1
 Z2←Z2, State row_2D (face 4) 1
 Z2←Z2, State row_2D (face 5) 1 ◊ ϵZ2[1;],F ◊ ϵZ2[2;],F
 Z2←    State row_2D (face 2) 2
 Z2←Z2, State row_2D (face 3) 2
 Z2←Z2, State row_2D (face 4) 2
 Z2←Z2, State row_2D (face 5) 2 ◊ ϵZ2[1;],F ◊ ϵZ2[2;],F
 Z2←    State row_2D (face 2) 3
 Z2←Z2, State row_2D (face 3) 3
 Z2←Z2, State row_2D (face 4) 3
 Z2←Z2, State row_2D (face 5) 3 ◊ ϵZ2[1;],F ◊ ϵZ2[2;],F

 Z2←Z1, State row_2D (face 6) 1 ◊ ϵZ2[1;],F ◊ ϵZ2[2;],F
 Z2←Z1, State row_2D (face 6) 2 ◊ ϵZ2[1;],F ◊ ϵZ2[2;],F
 Z2←Z1, State row_2D (face 6) 3 ◊ ϵZ2[1;],F ◊ ϵZ2[2;],F
 →0

V3D: ⍝ 3-dimensional cube view
 '' print_field 'w' ◊ View←pattern 7 3 3

 Z1←State    interpret_all View
 Z2←(⊖State) interpret_all View
 ⊣{ ⎕←'  ',(⊃Z1[⍵]),(interpret_char 0),'    ',(⊃Z2[⍵]), interpret_char 0 }¨⍳⍴Z2
∇
⍝------------------------------------------------------------------------
∇State←State do_moves M
 ⍝
 ⍝⍝ perform move M in cube state State; return the resulting state.
 ⍝⍝ Each move M may consist of several twists, we therefore iterate over ϵ M.
 ⍝
 ⊣ {State←State twist ⍵} ¨ ϵ M
∇
⍝------------------------------------------------------------------------
∇State←State twist M;N;PE;PV;RR
 ⍝
 ⍝⍝ perform one twist U, L, F, R, B, D, u, l, f, r, b, or d
 ⍝⍝ N is 1 for UDLRFB and 3 for udlrfb (since e.g. u is UUU).
 ⍝
 N←1 + 2×Mϵ'udlrfb' ◊ →(M='UuDdLlRrFfBb  ')/2/MU MD ML MR MF MB 0 ◊ +++
MU: PV← 1  7  9  3 ◊ PE← 2  4  8  6 ◊ RR←2 1 3 ◊ →COMMON
ML: PV←19 25  7  1 ◊ PE←10 22 16  4 ◊ RR←1 3 2 ◊ →COMMON
MF: PV← 1  3 21 19 ◊ PE← 2 12 20 10 ◊ RR←3 2 1 ◊ →COMMON
MR: PV← 3  9 27 21 ◊ PE← 6 18 24 12 ◊ RR←1 3 2 ◊ →COMMON
MB: PV←25 27  9  7 ◊ PE←16 26 18  8 ◊ RR←3 2 1 ◊ →COMMON
MD: PV←21 27 25 19 ◊ PE←24 26 22 20 ◊ RR←2 1 3 ◊ →COMMON
COMMON: State[(N⌽PV),N⌽PE;RR]←State[PV,PE;] ◊ ∆TCNT←∆TCNT+1
∇
⍝------------------------------------------------------------------------
∇Z←State get_moves B;Level;Pivot
 ⍝
 ⍝ return moves that are useful at Level. The levels are:
 ⍝ 
 ⍝ L1: solving the top face cross                 (= top face edge cubes)
 ⍝ L2: solving the top face                       (+= top face corner cubes)
 ⍝ L3: solving the middle face                    (= middle layer edge cubes)
 ⍝ L4: solving the bottom face cross orientations (bottom layer edge cubes)
 ⍝ L5: solving the bottom face cross positions    (bottom face edge cubes)
 ⍝ L6: solving the bottom face                    (+= bottom face corner cubes)
 ⍝
 ⍝ The Pivot (1-4) is the cube being solved
 ⍝
 (Level Pivot)←B ◊ Z←0,'' ◊ →(L1,L2,L3,L4,L5,L6,0)[Level] ◊ +++

L1: ⍝ all twists
    Z←,¨'LRFBUDlrfbud'
    Z←10,Z ◊ →0

L2: ⍝ Apply dxDX, DXdx, or DXddx. They leave the top face intact except one
    ⍝ corner cube (into which the remaining cubes are rotated)
    ⍝    dxDX   DXdx   DXddx      - Pivot -
    Z←  'dlDL' 'DBdb' 'DBddb'   ⍝ 1:9:35  7
    Z←Z,'dbDB' 'DRdr' 'DRddr'   ⍝ 3:27:33 9
    Z←Z,'dfDF' 'DLdl' 'DLddl'   ⍝ 6:11:17 1
    Z←Z,'drDR' 'DFdf' 'DFddf'   ⍝ 8:19:25 3
    Z←10,Z ◊ →0

L3: ⍝ Apply dyDYDXdx or DXdxdyDY. They leave the upper face and 3 middle
    ⍝ edges intact and swap the remaining middle edge with a bottom edge.
    ⍝ One side of the swapped edge remains on the same face (color)
    ⍝     dyDYDXdx   DXdxdyDY    X Y  dyDYDXdx  DXdxdyDY  ---- Pivot ----
    Z←   'dlDLDBdb' 'DBdbdlDL' ⍝ B L  B:D→L     L:D→B    #1 37:12 12:37 16
    Z←Z, 'dfDFDLdl' 'DLdldfDF' ⍝ L F  L:D→F     F:D→L    #2 13:20 20:13 10
    Z←Z, 'drDRDFdf' 'DFdfdrDR' ⍝ F R  F:D→R     R:D→F    #3 21:28 28:21 12
    Z←Z, 'dbDBDRdr' 'DRdrdbDB' ⍝ R B  R:D→B     B:D→R    #4 29:36 36:29 18
    Z←((⊂'dd'),,¨'Dd'),Z
    Z←10,Z ◊ →0

L4: ⍝ Apply (3.1) XDYdyx. It rotates through bottom cross orientations,
    ⍝ leaving the bottom cross positions intact.
    ⍝     XDYdyx
    Z←'LDBdbl' 'RDFdfr' 'FDLdlf' 'BDRdrb'
    Z←10,Z ◊ →0

L5: ⍝ Apply (3.0) fDBdFDDbDBDDb or (3.2) xdXdxddXdd. Both leave the bottom
    ⍝ cross orientations intact. fDBdFDDbDBDDb  swaps 2 bottom edges and
    ⍝ 2 bottom corners. xdXdxddXdd rotates 3 bottom cross edge cube positions.
    ⍝     xdXdxddXdd
    Z←  ⊂'fDBdFDDbDBDDb'   ⍝ swap edges 44:47 and corners 46:48
    Z←Z,⊂'ldLdlddLdd'      ⍝ X=L, 45 (R) fixed
    Z←Z,⊂'rdRdrddRdd'      ⍝ X=R, 44 (L) fixed
    Z←Z,⊂'fdFdfddFdd'      ⍝ X=F, 47 (B) fixed
    Z←Z,⊂'bdBdbddBdd'      ⍝ X=B, 42 (F) fixed
    Z←10,Z ◊ →0

L6: ⍝ Apply (3.3) lDZdXDzd. It leaves the bottom cross and one bottom corner
    ⍝ intact and permutes the remaining 3 bottom corners
    ⍝     xDZdXDzd   DZdxDzdx      X Z  OR:YW:BG
    Z←   'lDRdLDrd' 'DRdlDrdL'   ⍝ L R, 32:38:48  27 fixed
    Z←Z, 'rDLdRDld' 'DLdrDldR'   ⍝ R L, 16:22:41  19 fixed
    Z←Z, 'fDBdFDbd' 'DBdfDbdF'   ⍝ F B, 14:46:40  21 fixed,
    Z←Z, 'bDFdBDfd' 'DFdbDfdB'   ⍝ B F, 30:24:43  25 fixed
    Z←(,¨'Bb'),Z
    Z←10,Z
∇
⍝------------------------------------------------------------------------
∇Z←get_level State;S;N;I;Eq;Eq1
 Eq←State = ∆Solved_State
 Z←1 'U+'  (Eq1⍳0) S←  N←+/Eq1←∧/Eq[I← 2  4  6  8;  ] ◊ →(N<⍴I)/0
 Z←2 'U'   (Eq1⍳0) S←S+N←+/Eq1←∧/Eq[I← 1  3  7  9;  ] ◊ →(N<⍴I)/0
 Z←3 'M'   (Eq1⍳0) S←S+N←+/Eq1←∧/Eq[I←16 10 12 18;  ] ◊ →(N<⍴I)/0
 Z←4 'B+o' (Eq1⍳0) S←S+N←+/Eq1←  Eq[I←20 22 24 26; 3] ◊ →(N<⍴I)/0
 Z←5 'B+'  (Eq1⍳0) S←S+N←+/Eq1←∧/Eq[I←20 22 24 26;  ] ◊ →(N<⍴I)/0
 Z←6 'B'   (Eq1⍳0) S←S+N←+/Eq1←∧/Eq[I←19 21 25 27;  ] ◊ →(N<⍴I)/0
 Z←7 'Done'     0  S
∇
⍝------------------------------------------------------------------------
∇Progress←State try_moves Moves;S0;SM
 ⍝
 ⍝⍝ perform Moves and see if they place more sub-cubes
 ⍝
 S0←(get_level State)[4] ◊ State←State do_moves Moves
 SM←(get_level State)[4] ◊ Progress←S0<SM   ⍝ 1 = progress, 0 = no progress
∇
⍝------------------------------------------------------------------------
∇Z←State propose_len_moves A;Len;Moves;Rad;Progress;N;Max_N
 ⍝
 ⍝⍝ perform all combinations of Len moves from repertoire A.
 ⍝⍝ Return the first combination (if any) that makes progress
 ⍝
 (Moves Len)←A ◊ Rad←Len⍴⍴Moves
 N←0 ◊ Max_N←(⍴Moves)⋆Len
'Moves' Moves 'Length: ' Len 'Max_N:' Max_N
LOOP: Z←Moves[1+Rad⊤N] ◊ →(State try_moves ϵZ)/0
 →(Max_N>N←N+1)/LOOP
 Z←''
∇
⍝------------------------------------------------------------------------
∇Z←print_level State;Level;LN;Pivot;Score;Moves
 (Level LN Pivot Score)←get_level State
 Moves←1↓Z←State get_moves Level Pivot
 'Level:' Level (⊃LN) 'Pivot:' Pivot 'Score:' Score 'Tcount:' ∆TCNT
 'Repertoire:', Moves
∇
⍝------------------------------------------------------------------------
∇Z←propose_moves State;Len;Max_Len;Moves;∆TCNT
 ⍝
 ⍝⍝ propose a move that makes progress (= placse more sub-cubes)
 ⍝
 →(State≡∆Solved_State)↓1+↑⎕LC ◊ ' No proposal (cube solved)' ◊ Z←'' ◊ →0
 ∆TCNT←0 ◊ Len←1 ◊ Max_Len←↑Moves←print_level State ◊ Moves←1↓Moves
LOOP: Z←State propose_len_moves Moves Len
 →(↑⍴ϵZ)/0             ⍝ success
 →(Max_Len > Len←Len+1)/LOOP
 'No moves found' ◊ Z←0 3⍴0
∇
⍝------------------------------------------------------------------------
∇Z←cube_p2 B;C0;C1
 ⍝
 ⍝⍝ return parity of edge cubicle B (e.g. 'UF', 'UR', ...)
 ⍝
 B←⊂B
 ⍝     2    4    8    6   20   22   26   24   12   10   18   16
 C1←{⌽⍵}¨C0← 'UF' 'RU' 'UB' 'LU' 'DF' 'RD' 'DB' 'LD' 'FR' 'FL' 'BR' 'BL'
 Z←0 ◊ → (BϵC0)⍴0
 Z←1 ◊ → (BϵC1)⍴0
 '*** edge cubicle' B 'is not in ' C0 'or' C1 ◊ +++
∇
⍝------------------------------------------------------------------------
∇Z←cube_p3 B;C0;C1;C2
 ⍝
 ⍝⍝ return parity of corner cubicle B (e.g. 'UFR', 'URB', ...)
 ⍝
 B←⊂B
 C0← 'UFR' 'URB' 'UBL' 'ULF' 'DRF' 'DFL' 'DLB' 'DBR'
               Z←0 ◊ → (BϵC0)⍴0
 C1←{1⌽⍵}¨C0 ◊ Z←1 ◊ → (BϵC1)⍴0
 C2←{1⌽⍵}¨C1 ◊ Z←2 ◊ → (BϵC2)⍴0
 '*** corner cubicle' B 'is not in ' C0 'or' C1 'or' C2 ◊ +++
∇
⍝------------------------------------------------------------------------
∇Edges←edge_positions State;Map
⍝
⍝⍝ return the edge cubicles of State in Up/Down/Left/Right/Front/Back format
⍝
 Map←6 2⍴'OLRRBFGBWDYU'
 State←State[∆Edges;]              ⍝ pick edges from State
 State←Map ⎕Map State              ⍝ map our colors to positions
 State←0 ¯1↓1 2 1 2 1 2 1 2 0 0 0 0⌽State
 Edges←⊂[2] ⌽State
∇
⍝------------------------------------------------------------------------
∇Corners←corner_positions State;Map
⍝
⍝⍝ return the corner cubicles of State in Up/Down/Left/Right/Front/Back format
⍝
⍝ depending on j, Corners[j] is either State[j;3 2 1] or State[j;3 1 2]
⍝
 Map←6 2⍴'OLRRBFGBWDYU'
 State←State[∆Corners;]              ⍝ pick corners from State
 State←Map ⎕Map State                ⍝ map our colors to positions
 State←⌽State
 State[2 4 5 7;2 3]←⌽State[2 4 5 7;2 3]
 Corners←⊂[2] State
∇
⍝------------------------------------------------------------------------
∇Z←parities State;S;I;Map;Edges;Corners;CP;EP
 ⍝
 ⍝⍝ Z←(CP EP), where CP is the parities of the  8 corners, and
 ⍝⍝                  EP is the parities of the 12 edges
 ⍝

 ⍝ read State in the input order of optimal.c...
 ⍝
 Edges←edge_positions State
 Corners←corner_positions State

 CP←cube_p3 ¨ Corners   ⍝ cube parity
 EP←cube_p2 ¨ Edges     ⍝ edge parity
 Z←CP EP
∇
⍝------------------------------------------------------------------------
∇State←State setup_cube Clear;C;E;Old;New;Spaces;N;S1;N1;NewState
 ⍝
 ⍝⍝ set up a cube by entering the colors of its facets (in a 2D view)
 ⍝
 →∆3D↓1+↑⎕LC ◊ 0 show_cube State
 C←1 1 1 1 0 1 1 1 1 ◊ E←0 1 1 1 0 1 1 0 1 1 1 0 0
 '' ◊ 'Please enter the colors for faces U, L, F, R, B, and D below.' ◊ ''
 ' --Face U--   --Face L--   --Face F--   --Face R--   --Face B--   --Face D-'
 Old←'' ◊ ⊣{ Old←Old, E\C/,(face ⍵) ⌷ State } ¨ ⍳6 ◊ Old←¯2↓Old
LOOP: →Clear/1+↑⎕LC ◊ (6 2⍴'YUOLBFRRGBWD') ⎕MAP Old ◊ ⍞←Old
 New←⍞ ◊ Spaces←+/∧\New=' ' ◊ Clear←0
 New←(Newϵ'YOBRGW')/New←(Spaces↑Old),Spaces↓New
 →(8=N←+/New='B')/1+↑⎕LC ◊ 'Error: need 8 blue (B) but got'   N ◊ →LOOP
 →(8=N←+/New='O')/1+↑⎕LC ◊ 'Error: need 8 orange (O) but got' N ◊ →LOOP
 →(8=N←+/New='Y')/1+↑⎕LC ◊ 'Error: need 8 yellow (Y) but got' N ◊ →LOOP
 →(8=N←+/New='R')/1+↑⎕LC ◊ 'Error: need 8 red (R) but got'    N ◊ →LOOP
 →(8=N←+/New='W')/1+↑⎕LC ◊ 'Error: need 8 white (W) but got'  N ◊ →LOOP
 →(8=N←+/New='G')/1+↑⎕LC ◊ 'Error: need 8 green (G) but got'  N ◊ →LOOP

 NewState←State ◊ ⊣{ (C/,(face ⍵) ⌷ NewState)←New[(⍳8)+8×⍵-1] } ¨ ⍳6
 S1←S1[⍋S1←{ ⍵[⍋⍵] }¨⊂[2]State] ◊ N1←N1[⍋N1←{ ⍵[⍋⍵] }¨⊂[2]NewState]
 →(S1≡¨N1)/CE   ⍝ OK: same cubes as before
 'Error: cube(s)' ,(⍕(∼S1ϵN1)/S1), 'missing. Please try again.' ◊ →LOOP

CE: →(parities NewState)↓C3
 'Error: bad edge parity (cube cannot be solved). Please try again.' ◊ →LOOP

C3: →(↑parities NewState)↓OK
 'Error: bad corner 3-parity (cube cannot be solved). Please try again.' ◊ →LOOP

OK: State←NewState
∇
⍝========================================================================

∆HELP←"""
Valid commands are:
H h ?   Help
Q q     Quit
0       reset cube to unscrambled state
2       change to 2D view
3       change to 3D view

Twists (single 90° turns):
U       twist Upper face (clockwise in 2D view)
D       twist Down  face (clockwise in 2D view)
L       twist Left  face (clockwise in 2D view)
R       twist Right face (clockwise in 2D view)
F       twist Front face (clockwise in 2D view)
B       twist Back  face (clockwise in 2D view)

u       twist upper face (counter-clockwise in 2D view)
d       twist down  face (counter-clockwise in 2D view)
l       twist left  face (counter-clockwise in 2D view)
f       twist front face (counter-clockwise in 2D view)
b       twist back  face (counter-clockwise in 2D view)

T       rotate cubicle 3 clockwise in place
t       flip cubicle 2 in place

m       do 1 random twist
M       do 6 random twists
P       propose one move (= a few twists)
p       propose and execute one move

S s     setup cube (enter colors)
"""

∇Z←face N;Pos;Axis;HL;IDX
 ⍝
 ⍝⍝ return the 3D cube numbers and the 3D axis for 2D face N
 ⍝
 IDX←(1 2 3) (1 2 3) (1 2 3)
 IDX[Axis←(3 1 2 1 2 3)[N]]←1+2×N≥4
 Z←(N mirror ((⌽IDX)⌷3 3 3⍴⍳27)) Axis
∇

∇Z←paint Colors
 ⍝
 ⍝⍝ Colors is a color string like 'OBY' or 'OB-'. Paint every colore with its
 ⍝⍝ own color
 ⍝
 Xt←Colors ◊ (('-' = Xt)/Xt)←'w'
 Z←   Colors[1] print_field Xt[1]
 Z←Z, Colors[2] print_field Xt[2]
 Z←Z, Colors[3] print_field Xt[3]
 Z←Z, '' print_field 'w'
∇

∇Z←Pos cubicle_info State; Color_XYZ;Type;Ini_Pos;Ini_XYZ;Moved;EP;CP;P
 ⍝
 ⍝⍝ return the current and the initial cubicle positions and colors of the
 ⍝⍝ cubicle at (current) position Pos (with colors Color_XYZ)
 ⍝
 Z←' ' ◊ →(Pos = 0)⍴0
 (CP EP)←parities State
 P←(↑⍴State)⍴⍬ ◊ P[∆Corners]←CP ◊ P[∆Edges]←EP ◊ P←P[Pos]
 P←((⍴,P)⍴⊂'Parity:'),2 0⍕P-3×P=2
 Color_XYZ←State[Pos;]
 Type←("Corner" "Edge  " "Middle" "Center")[⎕IO + +/Color_XYZ = '-']
 Ini_XYZ←∆Solved_State[Ini_Pos←find_initial_pos Color_XYZ;]
 Moved←(Pos≢Ini_Pos) ∨ Color_XYZ ≢ Ini_XYZ
 Moved←Moved/'(from initial position ', (2 0⍕Ini_Pos), ' ', (paint Ini_XYZ), ')'
 Z←Type (2 0⍕Pos) P (paint Color_XYZ) Moved
∇

∇info State;C;E
 ⍝
 ⍝⍝ display some information for corner and edge cubicles
 ⍝
 C←∆Corners[⍋∆Corners] ◊ E←∆Edges[⍋∆Edges]
 ⊣ { ⎕←⍵ cubicle_info State } ¨ C, 0, E
∇

∇go;State;View;Cmd;EP;CP
 ⍝
 ⍝⍝ the main program. An (almost) endless loop that gets one character 'Cmd'
 ⍝⍝ from the user and executes a command for that character
 ⍝

 State←27 3⍴'-' ◊ ⊣{ ((face ⍵) ⌷ State) ←'YOBRGW'[⍵] } ¨⍳6
 ∆Solved_State←State  ⍝ the unscrambled state
 ∆3D←1                 ⍝ initial view: 3D
 ∆TCNT←0               ⍝ twist counter
 ∆Corners←3 9 7 1 21 19 25 27
 ∆Edges←2 6 8 4 20 24 26 22 12 10 18 16


LOOP: ∆3D show_cube State
      (CP EP)←parities State ◊ 'Parity:' (3∣+/CP) (2∣+/EP)
LP1:  Cmd←⎕UCS (⎕FIO.fgetc 0)
      →(Cmdϵ'UuDdLlRrFfBb')/MOVE
      →(Cmd='Hh?Qq P',⎕UCS 10)/HELP HELP INFO 0 0 LP1 PRO STAT
      →(Cmd='023mMpSsTt')/CLR V2D V3D RAN1 RAN6 PROE SETUP SETUP TW TE
      'Invalid command:' Cmd,'. Type h for help.'                       ◊ →LP1
HELP: ⊣ {⎕←⍵}¨∆HELP                                                     ◊ →LP1
INFO: info State                                                        ◊ →LP1
STAT: ⊣print_level State                                                ◊ →LP1
PRO:  'Proposal:',P←propose_moves State                                 ◊ →LP1
MOVE: 'Turn: ', Cmd ◊ State←State twist Cmd                             ◊ →LOOP
CLR:  'Clear' ◊ State←∆Solved_State ◊ ∆TCNT←0                           ◊ →LOOP
V2D:  ∆3D←0                                                             ◊ →LOOP
V3D:  ∆3D←1                                                             ◊ →LOOP
RAN6:   '6 random twists' P←'UuDdLlRrFfBb'[?6⍴12]
      State←State do_moves P ◊ ∆TCNT←0                                  ◊ →LOOP
RAN1:   '1 random twist'  P←'UuDdLlRrFfBb'[?,12]
      State←State do_moves P ◊ ∆TCNT←0                                  ◊ →LOOP
TW:   'turn corner 3 clockwise in place (possibly making cube insolvable)'
      State[3;]←State[3;3 1 2]                                          ◊ →LOOP
TE:   'flip edge 2 in place (possibly making cube insolvable)' 
      State[2;2 3]←State[2;3 2]                                         ◊ →LOOP
PROE: 'Proposal:', P←propose_moves State ◊ State←State do_moves ϵP      ◊ →LOOP
SETUP: State←State setup_cube Cmd='S'                                   ◊ →LOOP
∇


go

)VARS
⍎(1=⍴⎕SI 2)/')OFF'
]NEXTFILE

                             DESCRIPTION
                             ===========

Purpose
-------

With this GNU APL program you can do the following:

1. show a Rubik's Cube in an (Color-) XTerm and change its state by
   performing twists (turning the faces of the cube), or

2. enter the colors of a scrambled cube and display proposals for
   bringing it back to its un-scrambled state.

Prerequitites
-------------

This program is meant to be started in a Color-XTerm (as shipped with recent
GNU/Linux distributions). If you have a different terminal then you may
be able to fix wrong colors in function Xterm_Color.

Views
-----
This program provides a 3D view (useful to compare with a physical Rubik's
cube) and a 2D view (useful for understanding individual moves). Both the
2D and the 3D view use numbers to identify cubicles, but the numbers may
be different in the 2D view and in the 3D view.

The 2D view numbers are either the same as in the 3D view or those found
in the literature; they number the facets of the cube from 1 to 48 with
no number for the center facet.
The center facet of each cube face has no number in the 2D view but
is labeled with U (for Up), L (for Left), F (for Front), R (for
Right), B (for Back), or D (for Down). In the 2D view a cubicle
has 3 numbers (corners), 2 numbers (edges), or one of U, L, F, R, B, or D
(middles)

The 3D view is more APL-like in that the cubes are numbered 3 3 3⍴⍳27;
every sub-cube has one number and the 0, 1, 2, or 3 different facets of
a sub-cube are colors B (for Blue), O (for Orange) Y (for Yellow),
R (for Red), W (for White), and G (for Green).

The 2D and 3D views are related as folows:

           2D        APL       3D        APL                 Axis
           -------------       ----------------------------------
Numbers:   1-48      ⍳48       1-27      ⍳27
Faces:     U(p)      0+⍳8      B(lue)    (3 3 3⍴⍳27)[1; ; ]  Z (up/down)
           L(eft)    8+⍳8      O(range)  (3 3 3⍴⍳27)[ ; ;1]  X (left/right)
           F(ront)  16+⍳8      Y(ellow)  (3 3 3⍴⍳27)[ ;1; ]  Y (front/back)
           R(ight)  24+⍳8      R(ed)     (3 3 3⍴⍳27)[ ; ;3]  X (left/right)
           B(ack)   32+⍳8      W(hite)   (3 3 3⍴⍳27)[ ;3; ]  Y (front/back)
           D(own)   40÷⍳8      G(reen)   (3 3 3⍴⍳27)[3; ; ]  Z (up/down)

The origin of the 3D coordinates is cube 1.

Starting the program
--------------------

The program is an APL script, so it can be started with:

      )LOAD RUBIK,apl

from APL or with:

      apl -f RUBIK.apl

from a shell.

Commands
--------

The program is interactive and understands the following 1-key commands:

H h ?   Help
Q q     Quit
0       reset cube to unscrambled state
2       change to 2D view
3       change to 3D view

U       twist Upper face (clockwise in 2D view)
D       twist Down  face (clockwise in 2D view)
L       twist Left  face (clockwise in 2D view)
R       twist Right face (clockwise in 2D view)
F       twist Front face (clockwise in 2D view)
B       twist Back  face (clockwise in 2D view)

u       twist upper face (counter-clockwise in 2D view)
d       twist down  face (counter-clockwise in 2D view)
l       twist left  face (counter-clockwise in 2D view)
f       twist front face (counter-clockwise in 2D view)
b       twist back  face (counter-clockwise in 2D view)

T       rotate cubicle 3 clockwise in place
t       flip cubicle 2 in place

m       do 1 random twist
M       do 6 random twists
P       propose one move (= a few twists)
p       propose and execute one move

S s     setup cube (enter colors)


The twist commands (U, D, L, R, F, B, u, d, l, r, f, and b) perform one
twist (quarter-turn) of the respective face of the cube.

The m and M commands perform random moves that bring the cube into a random
state (for training your capabilities).

The p and P command propose (not necessarily optimal moves towards a solved
cube.

The setup command shows the colors of the current state of the cube for
all 6 faces and expects new values. To leave the colors as they are just
press return. There are color combinations that are impossible so solve;
such illegal combinations are not accepted in the setup command.

Notes
-----

   Cube Positions and Axes (3D view):
   ==================================

            Y Axis
           /
          /
          +--------------+
         /    /    /    /|
        /  7 /  8 /  9 / |
       +----⌿----⌿----+ 9|
      /    /    /    /| /|
     /  4 /  5 /  6 / |/ |
    +----⌿----⌿----+ 6/18|
   /    /    /    /| /| /|
  /  1 /  2 /  3 / |/ |/ |
 +----+----+----+ 3/15/27+---→ X Axis
 |  1 |  2 |  3 | /| /| /
 |    |    |    |/ |/ |/
 +----+----+----+12/24/
 | 10 | 11 | 12 | /| /
 |    |    |    |/ |/
 +----+----+----+21/
 | 19 | 20 | 21 | /
 |    |    |    |/
 +--------------+
 |
 ↓
 Z Axis


   Cube Face labels (2D View):
   ===========================

           +----------+
           | 1   2   3|
           |          |
           | 4   U   5|
           |          |
           | 6   7   8|
+----------+----------+----------+----------+
| 9  10  11|17  18  19|25  26  27+33  34  35|
|          |          |          |          |
|12   L  13|20   F  21|28   R  29|36   B  37|
|          |          |          |          |
|14  15  16|22  23  24|30  31  32|38  39  40|
+----------+----------+----------+----------+
           |41  42  43|
           |          |
           |44   D  45|
           |          |
           |46  47  48|
           +----------+



                     PROGRAMMER's REFERENCE
                     ======================
⍝
⍝ A RUBIK's cube consist of 27 smaller called cubicles. The cubicles are
⍝  numbered from 1...27 and are either:
⍝
⍝ * one of  8 corner cubicles:  1, 3, 7, 9, 19, 21, 25, 27, or
⍝ * one of 12 edge cubicles:    2, 4, 6, 8, 10, 12, 16, 18, 20, 22, 24, 26, or
⍝ * one of  6 middle cubicles:  5, 11, 15, 13, 17, 23, or
⍝ * the     1 center cubicle:   14 (invisible)
⍝
⍝ The state of a cube can be visualized in 2 ways:
⍝
⍝ 3-dimensional (3D) view (see below).
⍝ The numbers in this view are cubicle numbers (1-27) as above.
⍝
⍝                 Corners                       Edges
⍝            7---------------9            +-------8-------+
⍝           /|              /|           /|              /|
⍝          / |             / |          4 |             6 |
⍝         /  |            /  |         / 16            /  18
⍝        1---|-----------3   |        +---|---2-------+   |
⍝        |   |           |   |        |   |           |   |
⍝        |  25-----------|---27       |   +------26---|---+
⍝        |  /            |  /        10  /           12  /
⍝        | /             | /          | 22            | 24
⍝        |/              |/           |/              |/
⍝       19---------------21           +------20-------+
⍝
⍝
⍝ 2-dimensional (2D) view (see below).
⍝ The numbers in this view are cubicle numbers (1-27) as above.
⍝
⍝
⍝                    ╔════╤════╤════╗
⍝                    ║  7 │  8 │  9 ║
⍝                    ╟────┼────┼────╢
⍝                    ║  4 │  U │  6 ║
⍝                    ╟────│────┼────╢
⍝                    ║  1 │  2 │  3 ║
⍝     ╔════╤════╤════╬════╪════╪════╬════╤════╤════╦════╤════╤════╗
⍝     ║  7 │  4 │  1 ║  1 │  2 │  3 ║  3 │  6 │  9 ║  9 │  8 │  7 ║
⍝     ╟────┼────┼────╫────┼────┼────╫────┼────┼────╫────┼────┼────║
⍝     ║ 16 │  L │ 10 ║ 10 │  F │ 12 ║ 12 │  R │ 18 ║ 18 │  B │ 16 ║
⍝     ╟────┼────┼────╫────┼────┼────╫────┼────┼────╫────┼────┼────║
⍝     ║ 25 │ 22 │ 19 ║ 19 │ 20 │ 21 ║ 21 │ 24 │ 27 ║ 27 │ 26 │ 25 ║
⍝     ╚════╧════╧════╬════╪════╪════╬════╧════╧════╩════╧════╧════╝
⍝                    ║ 19 │ 20 │ 21 ║
⍝                    ╟────┼────┼────╢
⍝                    ║ 22 │  D │ 24 ║
⍝                    ╟────┼────┼────╢
⍝                    ║ 25 │ 26 │ 27 ║
⍝                    ╚════╧════╧════╝
⍝
⍝ For debugging purposes, one can replace "→ Face_position" with "→ Face_number"
⍝ in function face_text to have the (faces of) the cubicles numbered like this:
⍝
⍝                    ╔════╤════╤════╗
⍝                    ║  1 │  2 │  3 ║
⍝                    ╟────┼────┼────╢
⍝                    ║  4 │  U │  5 ║
⍝                    ╟────│────┼────╢
⍝                    ║  6 │  7 │  8 ║
⍝     ╔════╤════╤════╬════╪════╪════╬════╤════╤════╦════╤════╤════╗
⍝     ║  9 │ 10 │ 11 ║ 17 │ 18 │ 19 ║ 25 │ 26 │ 27 ║ 33 │ 34 │ 35 ║
⍝     ╟────┼────┼────╫────┼────┼────╫────┼────┼────╫────┼────┼────║
⍝     ║ 12 │  L │ 13 ║ 20 │  F │ 21 ║ 28 │  R │ 29 ║ 36 │  B │ 37 ║
⍝     ╟────┼────┼────╫────┼────┼────╫────┼────┼────╫────┼────┼────║
⍝     ║ 14 │ 15 │ 16 ║ 22 │ 23 │ 24 ║ 30 │ 31 │ 32 ║ 38 │ 39 │ 40 ║
⍝     ╚════╧════╧════╬════╪════╪════╬════╧════╧════╩════╧════╧════╝
⍝                    ║ 41 │ 42 │ 43 ║
⍝                    ╟────┼────┼────╢
⍝                    ║ 44 │  D │ 45 ║
⍝                    ╟────┼────┼────╢
⍝                    ║ 46 │ 47 │ 48 ║
⍝                    ╚════╧════╧════╝
⍝
⍝ The state of a cube is represented by a 27×3 matrix State where:
⍝
⍝ State[N;] is a 3-character string indicating the colors of the cubicle
⍝ at position N (1 ≤ N ≤ 27) and (in the sense of the 3D view):
⍝
⍝ For a corner cubicle N:
⍝
⍝ State[N; 1] is the current color in the X (i.e. left-right) direction,
⍝ State[N; 2] is the current color in the Y (i.e. front-bach) direction, and
⍝ State[N; 3] is the current color in the Z (i.e. up-down) direction.
⍝
⍝ For an edge cubicle N: Like corner cubicle N, but with character '-' for
⍝ direction of the missing face.

⍝ For a middle cubicle N: Like corner cubicle N, but with character '-' for
⍝ the two directions of the missing faces.
⍝
⍝ For the center cubicle 14: "---" since all 3 directions are missing.
⍝
⍝ The algorithm implemented by this workspace always leaves the 6 middle
⍝ cubicle and the center cubicle in place.
⍝
⍝ A garbled cubic is solved by performing a sequence of elementary operation
⍝ called twists. A twist is a 90 degree clockwise turn of on of the 6 faces
⍝ of the cube where s face is one of U (for Up), D (for Down), L (for Left)m
⍝ R (for Right), F (for Front) or B (for Back),
⍝
⍝ The cube is solved by performing a sequence of moves where:
⍝
⍝ * A move consists of several twists, and
⍝ * Every move brings (at least) one more cubicle into its place, or into its position,
⍀   or both.
⍝
⍝ The algorithm is not optimal (nor is the method of placing or orienting one cube after
⍝ the other) but easier to understand for humans than optimal algorithms.
⍝
⍝                           Global variables
⍝                           ----------------
⍝
⍝ The program uses a handful of global variables:
⍝
⍝ ∆3D           : the currently selected view, 1 for the 3D view or 0 for the 2D view.
⍝ ∆Corners      : the (numbers of the) 8 corner cubicles.
⍝ ∆Edges        : the (numbers of the) 12 edge cubicles.
⍝ ∆HELP         : the text displayed by the help command.
⍝ ∆Solved_State : The state of a solved cube
⍝ ∆TCNT         : a twist counter used when proposing moves.
⍝
