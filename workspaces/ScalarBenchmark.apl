#! /usr/local/bin/apl --script

  ⍝ tunable parameters for this benchmark program
  ⍝
  DO_PLOT←0             ⍝ do/don't plot the results of start-up cost
  ILRC←1000             ⍝ max. repeat count for the inner loop of start-up cost
  LEN_PI←100000         ⍝ vector length for measuring the per-item cost
  PROFILE←4000 2000 50  ⍝ fractions of Integer, Real, and Complex numbers
  CORES←3               ⍝ number of cores used for parallel execution
  TIME_LIMIT←2000       ⍝ max. time per pass (milliseconds)

)COPY 5 FILE_IO

 ⍝ expressions to be benchmarked. The integer STAT is the statistics
 ⍝ as in Performance.def, i.e. 0 for F12_PLUS, 1 for F12_MINUS, and so on.
 ⍝
∇Z←MON_EXPR
  Z←⍬
  ⍝     A          OP    B          N CN              STAT
  ⍝-------------------------------------------------------
  Z←Z,⊂ ""         "+"   "Mix_IRC"  1 "F12_PLUS"       0
  Z←Z,⊂ ""         "-"   "Mix_IRC"  1 "F12_MINUS"      1
⍝ Z←Z,⊂ ""         "×"   "Mix_IRC"  1 "F12_TIMES"      2
⍝ Z←Z,⊂ ""         "÷"   "Mix1_IRC" 1 "F12_DIVIDE"     3
⍝ Z←Z,⊂ ""         "∼"   "Bool"     1 "F12_WITHOUT"    4
⍝ Z←Z,⊂ ""         "⌈"   "Mix_IR"   1 "F12_RND_UP"     5
⍝ Z←Z,⊂ ""         "⌊"   "Mix_IR"   1 "F12_RND_DN"     6
⍝ Z←Z,⊂ ""         "!"   "Int2"     1 "F12_BINOM"      7
⍝ Z←Z,⊂ ""         "⋆"   "Mix_IRC"  1 "F12_POWER"      8
⍝ Z←Z,⊂ ""         "⍟"   "Mix1_IRC" 1 "F12_LOGA"       9
⍝ Z←Z,⊂ ""         "○"   "Mix_IRC"  1 "F12_CIRCLE"    10
⍝ Z←Z,⊂ ""         "∣"   "Mix_IR"   1 "F12_STILE"     11
⍝ Z←Z,⊂ ""         "?"   "Int2"     1 "F12_ROLL"      12
∇

∇Z←DYA_EXPR
  Z←⍬
  ⍝     A          OP    B          N CN              STAT
  ⍝-------------------------------------------------------
  Z←Z,⊂ "Mix_IRC"  "+"   "Mix_IRC"  2 "F12_PLUS"      13
  Z←Z,⊂ "Mix_IRC"  "-"   "Mix_IRC"  2 "F12_MINUS"     14
⍝ Z←Z,⊂ "Mix_IRC"  "×"   "Mix1_IRC" 2 "F12_TIMES"     15
⍝ Z←Z,⊂ "Mix1_IRC" "÷"   "Mix1_IRC" 2 "F12_DIVIDE"    16
⍝ Z←Z,⊂ "Bool"     "∧"   "Bool1"    2 "F2_AND"        17
⍝ Z←Z,⊂ "Int"      "⊤∧"  "Int"      2 "F2_AND_B"      18
⍝ Z←Z,⊂ "Bool"     "∨"   "Bool1"    2 "F2_OR"         19
⍝ Z←Z,⊂ "Int"      "⊤∨"  "Int"      2 "F2_OR_B"       20
⍝ Z←Z,⊂ "Bool"     "⍲"   "Bool1"    2 "F2_NAND"       21
⍝ Z←Z,⊂ "Int"      "⊤⍲"  "Int"      2 "F2_NAND_B"     22
⍝ Z←Z,⊂ "Bool"     "⍱"   "Bool1"    2 "F2_NOR"        23
⍝ Z←Z,⊂ "Int"      "⊤⍱"  "Int"      2 "F2_NOR_B"      24
⍝ Z←Z,⊂ "Mix_IR"   "⌈"   "Mix_IR"   2 "F12_RND_UP"    25
⍝ Z←Z,⊂ "Mix_IR"   "⌊"   "Mix_IR"   2 "F12_RND_DN"    26
⍝ Z←Z,⊂ "Mix_IRC"  "!"   "Mix_IRC"  2 "F12_BINOM"     27
⍝ Z←Z,⊂ "Mix_IRC"  "⋆"   "Mix_IRC"  2 "F12_POWER"     28
⍝ Z←Z,⊂ "Mix1_IRC" "⍟"   "Mix1_IRC" 2 "F12_LOGA"      29
⍝ Z←Z,⊂ "Mix_IR "  "<"   "Mix_IR"   2 "F2_LESS"       30
⍝ Z←Z,⊂ "Mix_IR "  "≤"   "Mix_IR"   2 "F2_LEQ"        31
⍝ Z←Z,⊂ "Mix_IRC"  "="   "Mix_IRC"  2 "F2_EQUAL"      32
⍝ Z←Z,⊂ "Int"      "⊤="  "Int"      2 "F2_EQUAL_B"    33
⍝ Z←Z,⊂ "Int2"     "≠"   "Mix_IRC"  2 "F2_UNEQ"       34
⍝ Z←Z,⊂ "Int"      "⊤≠"  "Int"      2 "F2_UNEQ_B"     35
⍝ Z←Z,⊂ "Mix_IR"   ">"   "Mix_IR"   2 "F2_GREATER"    36
⍝ Z←Z,⊂ "Mix_IR"   "≥"   "Mix_IR"   2 "F2_MEQ"        37
  Z←Z,⊂ "1"        "○"   "Mix_IRC"  2 "F12_CIRCLE"    38
⍝ Z←Z,⊂ "Mix_IRC"  "∣"   "Mix_IRC"  2 "F12_STILE"     39
⍝ Z←Z,⊂ "1 2 3"    "⋸"   "Int"      2 "F12_FIND"      40
⍝ Z←Z,⊂ "Mat1_IRC" "+.×" "Mat1_IRC" 3 "OPER2_INNER"   41
⍝ Z←Z,⊂ "Vec1_IRC" "∘.×" "Vec1_IRC" 3 "OPER2_OUTER"   42
∇

∇INIT_DATA LEN;N;Ilen;Rlen;Clen
  ⍝⍝
  ⍝⍝ setup variables used in benchmark expressions:
  ⍝⍝ Int:  ¯2 ... 9
  ⍝⍝ Int1: nonzero Int
  ⍝⍝ Real: ¯10 to 10 or so
  ⍝⍝
  (Ilen Rlen Clen)←PROFILE         ⍝ relative amounts of int, real, and complex
  Int  ← 10 - ? Ilen ⍴ 12          ⍝ Integers -10 ... 10
  Int1 ← Ilen ⍴ (Int≠0)/Int        ⍝ non-zero Integers -10 ... 10
  Int2 ← Ilen ⍴ (Int>0) / Int      ⍝ positive Integers 1 ... 10
  Bool ← 2 ∣ Int                   ⍝ Booleans
  Bool1← 1 ⌽ Bool                  ⍝ also Booleans
  Real ← Rlen ⍴ Int + 3 ÷ ○1       ⍝ reals
  Real1← Rlen ⍴ (Real≠0)/Real      ⍝ non-zero reals
  Real2← Rlen ⍴ (Real>0)/Real      ⍝ positive reals 1 ... 10
  Comp ← Clen ⍴ Real + 0J1×1⌽Real  ⍝ complex
  Comp1← Clen ⍴ (Comp≠0)/Comp      ⍝ non-zero complex

  Mix_IR   ←Int,Real         ◊ Mix_IR   [N?N←⍴Mix_IR  ] ← Mix_IR
  Mix_IRC  ←Int,Real,Comp    ◊ Mix_IRC  [N?N←⍴Mix_IRC ] ← Mix_IRC
  Mix1_IRC ←Int1,Real1,Comp1 ◊ Mix1_IRC [N?N←⍴Mix1_IRC] ← Mix1_IRC

  Int      ← LEN ⍴ Int
  Int1     ← LEN ⍴ Int1
  Int2     ← LEN ⍴ Int2
  Bool     ← LEN ⍴ Bool
  Bool1    ← LEN ⍴ Bool1
  Real     ← LEN ⍴ Real
  Real1    ← LEN ⍴ Real1
  Real2    ← LEN ⍴ Real2
  Comp     ← LEN ⍴ Comp
  Comp1    ← LEN ⍴ Comp1
  Mix_IR   ← LEN ⍴ Mix_IR
  Mix_IRC  ← LEN ⍴ Mix_IRC
  Mix1_IRC ← LEN ⍴ Mix1_IRC
  Mat1_IRC ← (2⍴⌈LEN⋆0.35)⍴Mix1_IRC
  Vec1_IRC ← (⌈LEN⋆0.5)⍴Mix1_IRC
∇

'libaplplot' ⎕FX  'PLOT'

∇EXPR PLOT_P DATA;PLOTARG
  ⍝⍝
  ⍝⍝ plot data if enabled by DO_PLOT
  ⍝⍝
 →DO_PLOT↓0
  PLOTARG←'xcol 0;'
  PLOTARG←PLOTARG,'xlabel "result length";'
  PLOTARG←PLOTARG,'ylabel "CPU cycles";'
  PLOTARG←PLOTARG,'draw l;'
  PLOTARG←PLOTARG,'plwindow ' , TITLE EXPR
  ⊣ PLOTARG PLOT DATA
  ⍞
∇

∇Z←Average[X] B
 ⍝⍝ return the average of B along axis X
 Z←(+/[X]B) ÷ (⍴B)[X]
∇

∇Z←TITLE EXPR;A;OP;B
  (A OP B)←3↑EXPR
  Z←OP, ' ', B
  →(0=⍴A)/0
  Z←A,' ',Z
∇

∇Z←TITLE1 EXPR;A;OP;B;Z1
  (A OP B)←3↑EXPR
  Z←OP, ' B"'    ◊ Z1←'"'
  →(0=⍴A)/1+↑⎕LC ◊ Z1←'"A '
  Z←Z1,Z
∇

∇Z←X LSQRL Y;N;XY;XX;Zb;Za;SX;SXX;SY;SXY
 ⍝⍝ return the least square regression line (a line a + b×N with minimal
 ⍝⍝ distance from samples Y(X))
 N←⍴X
 XY←X×Y ◊ XX←X×X
 SX←+/X ◊ SY←+/Y ◊ SXY←+/XY ◊ SXX←+/XX
 Zb←( (N×SXY) - SX×SY ) ÷ ((N×SXX) - SX×SX)
 Za←(SY - Zb×SX) ÷ N
 Z←Za, Zb
∇

∇Z←VL ONE_PASS EXPR;OP;SID;STAT;ITER;ZZ;TH1;TH2;CYCLES;T0;T1
  ⍝ ----------------------------------------------------
  ⍝ Run one pass (for one vector length VL)
  ⍝ return (VL CYCLES), CYCLES is CPU cycles for all VL items
  ⍝
  ⍝ VL:   length of the vector
  ⍝ EXPR: one line in MON_EXPR or DYA_EXPR,
  ⍝       e.g. "+" "Mix_IRC"  1 "F12_PLUS"
  ⍝
  OP←⊃EXPR[2]      ⍝ e.g. +
  SID←EXPR[6]     ⍝ statistics ID, e.g. 2  for F12_PLUS
  JOB←TITLE EXPR   ⍝ e.g. "+ Mix_IRC"
  ⍝ '###' OP ':' (FIO∆get_statistics SID)[2] ':' (⎕FIO ¯15)[SID+⎕IO;]

  ⍝ save current thresholds
  ⍝
  TH1← 1 FIO∆set_monadic_threshold OP
  TH2← 1 FIO∆set_dyadic_threshold  OP

  ITER←0
  TOT_CNT←0
  TOT_CYCLES←0
  ZZ←2 0⍴0
  T0←24 60 60 1000⊥¯4↑⎕TS
L:
  FIO∆clear_statistics SID ◊ Q←⍎JOB ◊ STAT ← FIO∆get_statistics SID
  TOT_CNT←TOT_CNT + VL
  TOT_CYCLES←TOT_CYCLES + CYCLES←STAT[4]
  ZZ←ZZ,⍪VL, CYCLES
  T1←24 60 60 1000⊥¯4↑⎕TS
  →((ITER≥2) ∧ TIME_LIMIT<T1-T0)⍴DONE   ⍝ don't let it run too long
  →(ILRC≥ITER←ITER+1)/L
DONE:

  ⍝ restore thresholds
  ⊣ TH1 FIO∆set_monadic_threshold OP
  ⊣ TH2 FIO∆set_dyadic_threshold  OP

  FAST ← ⌊⌿ZZ[2;]
  FIDX ← ZZ[2;] ⍳ FAST
  Z←VL, ZZ[2;FIDX]

  'PASS: Len=' VL 'iter=' ITER 'min. cycles=' Z[2] 'total cycles' TOT_CYCLES
∇

  ⍝ ----------------------------------------------------
  ⍝ figure start-up times for sequential and parallel execution.
  ⍝ We use small vector sizes for better precision
  ⍝
∇Z←FIGURE_A EXPR;LENGTHS;I;LEN;ZS;ZP;SA;SB;PA;PB;H1;H2;P;TXT
  TXT←78↑'  ===================== ', (TITLE EXPR), '  ', 80⍴ '='
  '' ◊ TXT ◊ ''
  Z←0 3⍴0
  LL←⍴LENGTHS←⌽⍳20 ⍝ outer loop vector lengths
  'Benchmarking start-up cost for "', (TITLE EXPR), '" ...'

  I←1 ◊ ZS←0 2⍴0
  ⎕SYL[26;2] ← 0   ⍝ sequential
LS: INIT_DATA LEN←LENGTHS[I]
  ZS←ZS⍪LEN ONE_PASS EXPR
  →(LL≥I←I+1)⍴LS

  I←1 ◊ ZP←0 2⍴0
  ⎕SYL[26;2] ← CORES   ⍝ parallel
LP: INIT_DATA LEN←LENGTHS[I]
  ZP←ZP⍪LEN ONE_PASS EXPR
  →(LL≥I←I+1)⍴LP

  (SA SB)←⌊ ZS[;1] LSQRL ZS[;2]
  (PA PB)←⌊ ZP[;1] LSQRL ZP[;2]

  ⍝ print and plot result
  ⍝
  P←ZS,ZP[;2]            ⍝ sequential and parallel cycles
  P←P,(SA+ZS[;1]×SB)     ⍝ sequential least square regression line
  P←P,(PA+ZP[;1]×PB)     ⍝ parallel least square regression line
  H1←'Length' '  Sequ Cycles' '  Para Cycles' '  Linear Sequ' 'Linear Para'
  H2←'======' '  ===========' '  ===========' '  ===========' '==========='
  H1⍪H2⍪P

  ''
  'regression line sequential:     ', (¯8↑⍕SA), ' + ', (⍕SB),'×N cycles'
  'regression line parallel:       ', (¯8↑⍕PA), ' + ', (⍕PB),'×N cycles'

  ⍝ xdomain of aplplot seems not to work for xy plots - create a dummy x=0 line
  P←(0, SA, PA, SA, PA)⍪P

  EXPR PLOT_P P
  Z←SA,PA
∇

  ⍝ ----------------------------------------------------
  ⍝ figure per-item times for sequential and parallel execution.
  ⍝ We use one LARGE vector
  ⍝
∇Z←SUP_A FIGURE_B EXPR;SOFF;POFF;SCYC;PCYC;LEN
  (SOFF POFF)←SUP_A
  'Benchmarking per-item cost for ', (TITLE EXPR), ' ...'
  SUMMARY←SUMMARY,⊂'-------------- ', (TITLE EXPR), ' -------------- '
  SUMMARY←SUMMARY,⊂'vector length:                  ', (¯8↑⍕⌈LEN_PI)
  SUMMARY←SUMMARY,⊂'average sequential startup cost:', (¯8↑⍕⌈SOFF), ' cycles'
  SUMMARY←SUMMARY,⊂'average parallel startup cost:  ', (¯8↑⍕⌈POFF), ' cycles'

  INIT_DATA LEN_PI
  ⎕SYL[26;2] ← 0       ⍝ sequential
  (LEN SCYC)←LEN_PI ONE_PASS EXPR
  ⎕SYL[26;2] ← CORES   ⍝ parallel
  (LEN PCYC)←LEN_PI ONE_PASS EXPR
  Z←⊂TITLE EXPR
  Z←Z, ⌈ SCYC - SOFF
  Z←Z, ⌈ PCYC - POFF
  TS←'per item cost sequential:       ',(¯8↑⍕Z[2]), ' cycles'
  TP←'per item cost parallel:         ',(¯8↑⍕Z[3]), ' cycles'
  SUMMARY←SUMMARY,(⊂TS),(⊂TP)

  SUP_A BREAK_EVEN (⊂EXPR),Z
∇

∇SUP BREAK_EVEN PERI;EXPR;OP;ICS;ICP;SUPS;SUPP;T1;T2;BE;OUT
  (SUPS SUPP)←SUP   ⍝ start-up cost
  (EXPR OP ICS ICP)←PERI ⍝ per-item cost
  T1←'parallel break-even length:     '
  T2←'     not reached' ◊ T3←'8888888888888888888ULL'
  →(ICP ≥ ICS)⍴1+↑⎕LC ◊ T2←¯8↑BE←⍕⌈ (SUPP - SUPS) ÷ ICS - ICP ◊ T3←21↑BE
  SUMMARY←SUMMARY,(⊂T1,T2),⊂''

  OUT←'perfo_',(⍕EXPR[4])
  OUT←OUT, 16↑'(',(⊃EXPR[5]),','
  OUT←OUT, 6↑'_',((-1+0<⍴⊃EXPR[1])↑'AB'),','
  OUT←OUT, 10↑(TITLE1 EXPR),','
  OUT←OUT, T3,')',⎕UCS ,10
  ⊣ OUT FIO∆fwrite_utf8 TH_FILE
∇

  ⍝ ----------------------------------------------------

∇GO;DYA_A;MON_A;SUMMARY;TH_FILE
  CORES←CORES ⌊ ⎕SYL[25;2]
  'Running ScalarBenchmark_2 with' CORES 'cores...'

  ⍝ check that the core count can be set
  ⍝
  ⎕SYL[26;2] ← CORES
  →(CORES = ⎕SYL[26;2])⍴CORES_OK
  '*** CPU core count could not be set!'
  '*** This is usually a configuration or platform problem.'
  '***'
  '***  try "make parallel1" in the top-level directory'
  '***'
  '*** the relevant ./configure options (used by make parallel1) are:'
  '***      PERFORMANCE_COUNTERS_WANTED=yes'
  '***      CORE_COUNT_WANTED=SYL'
  '***'
  '*** NOTE: parallel GNU APL currently requires linux and a recent Intel CPU'
  '***'
  →0

CORES_OK:
  ⎕SYL[26;2] ← 0

  ⍝ figure start-up costs
  ⍝
  MON_A←Average[1] ⊃ FIGURE_A ¨ MON_EXPR
  DYA_A←Average[1] ⊃ FIGURE_A ¨ DYA_EXPR

  ⍝ figure per-item costs. we can do that only after computing MON_A/DYA_A
  ⍝
  ''
  SUMMARY←0⍴''
  TH_FILE←"w" FIO∆fopen "parallel_thresholds"

  ⊣ "\n" FIO∆fwrite_utf8 TH_FILE

  ⊣ (⊂MON_A) FIGURE_B ¨ MON_EXPR

  ⊣ "\n" FIO∆fwrite_utf8 TH_FILE

  ⊣ (⊂DYA_A) FIGURE_B ¨ DYA_EXPR

  ⊣ "\n" FIO∆fwrite_utf8 TH_FILE
  ⊣ "#undef perfo_1\n" FIO∆fwrite_utf8 TH_FILE
  ⊣ "#undef perfo_2\n" FIO∆fwrite_utf8 TH_FILE
  ⊣ "#undef perfo_3\n" FIO∆fwrite_utf8 TH_FILE
  ⊣ "\n" FIO∆fwrite_utf8 TH_FILE

  ⊣ FIO∆fclose TH_FILE

 ''
 78↑' ============================  SUMMARY  ',80⍴'='
 ''
  ⊣ { ⎕←⍵ }¨SUMMARY
∇


  GO

  ⍝ )CHECK
  ]PSTAT
  )OFF

