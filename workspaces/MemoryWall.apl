#! /usr/local/bin/apl --script

  ⍝ MemoryWall.apl -- find, for every scalar primitive, the core count at
  ⍝ which adding more cores stops helping (the "memory wall": all cores
  ⍝ compete for the same shared memory bandwidth, so beyond some point
  ⍝ extra cores buy nothing and can even hurt -- see README-8-parallel
  ⍝ section 5, "the optimal number of cores seems to be N-1"). Sibling of
  ⍝ ScalarBenchmark.apl (which tunes the sequential-vs-parallel
  ⍝ break-even *length*); this instead fixes a large length and sweeps
  ⍝ the *core count*, writing a file 'memory_wall' with, per primitive,
  ⍝ the core count at which the wall was hit and the speedup reached
  ⍝ there.
  ⍝
  ⍝ tunable parameters for this benchmark program
  ⍝
  LEN_MW←1000000        ⍝ vector length -- large enough that per-item
                        ⍝ noise averages out within a single pass, so
                        ⍝ (unlike ScalarBenchmark's tiny-N regime) only
                        ⍝ a few repeat iterations are needed per point
  ILRC_MW←3             ⍝ max. repeat count per (primitive, core count)
  TIME_LIMIT_MW←3000    ⍝ max. time per (primitive, core count) pass (ms)

)COPY 5 FILE_IO

∇Z←SYL_ROW TXT;LABELS
  ⍝⍝ return the (⎕IO-relative) row index of the ⎕SYL row whose label
  ⍝⍝ contains TXT -- see ScalarBenchmark.apl's SYL_ROW for why this is
  ⍝⍝ looked up by label text instead of a hardcoded row number.
  ⍝⍝
  LABELS←⎕SYL[;1]
  Z←({∨/TXT⍷⍵}¨LABELS)⍳1
  →(Z≤≢LABELS)⍴0
  ⎕←'*** ⎕SYL row "',TXT,'" not found -- is this GNU APL built with parallel execution support?'
  ⎕←'*** (./configure CORE_COUNT_WANTED=... , see "make parallel"/"make parallel1")'
  ⍎')OFF'
∇

 ⍝ expressions to be benchmarked -- identical to ScalarBenchmark.apl's
 ⍝ MON_EXPR/DYA_EXPR (same primitives, same homogeneous-type rows, same
 ⍝ type restrictions and their rationale -- see that workspace for the
 ⍝ full commentary); duplicated here rather than shared because GNU APL
 ⍝ scripts are standalone (no include/source mechanism), the same way
 ⍝ Scalar2.apl/Scalar3.apl already duplicate this workspace's INIT_DATA.
 ⍝
∇Z←MON_EXPR
  Z←⍬
  ⍝     A          OP    B          N CN              STAT
  ⍝-------------------------------------------------------
  Z←Z,⊂ ""         "+"   "Int"      1 "F12_PLUS"       0
  Z←Z,⊂ ""         "+"   "Real"     1 "F12_PLUS"       0
  Z←Z,⊂ ""         "+"   "Comp"     1 "F12_PLUS"       0
  Z←Z,⊂ ""         "-"   "Int"      1 "F12_MINUS"      1
  Z←Z,⊂ ""         "-"   "Real"     1 "F12_MINUS"      1
  Z←Z,⊂ ""         "-"   "Comp"     1 "F12_MINUS"      1
  Z←Z,⊂ ""         "×"   "Int"      1 "F12_TIMES"      2
  Z←Z,⊂ ""         "×"   "Real"     1 "F12_TIMES"      2
  Z←Z,⊂ ""         "×"   "Comp"     1 "F12_TIMES"      2
  Z←Z,⊂ ""         "÷"   "Int1"     1 "F12_DIVIDE"     3
  Z←Z,⊂ ""         "÷"   "Real1"    1 "F12_DIVIDE"     3
  Z←Z,⊂ ""         "÷"   "Comp1"    1 "F12_DIVIDE"     3
  Z←Z,⊂ ""         "∼"   "Bool"     1 "F12_WITHOUT"    4
  Z←Z,⊂ ""         "⌈"   "Int"      1 "F12_RND_UP"     5
  Z←Z,⊂ ""         "⌈"   "Real"     1 "F12_RND_UP"     5
  Z←Z,⊂ ""         "⌈"   "Comp"     1 "F12_RND_UP"     5
  Z←Z,⊂ ""         "⌊"   "Int"      1 "F12_RND_DN"     6
  Z←Z,⊂ ""         "⌊"   "Real"     1 "F12_RND_DN"     6
  Z←Z,⊂ ""         "⌊"   "Comp"     1 "F12_RND_DN"     6
  Z←Z,⊂ ""         "!"   "Int2"     1 "F12_BINOM"      7
  Z←Z,⊂ ""         "!"   "Real"     1 "F12_BINOM"      7
  Z←Z,⊂ ""         "!"   "Comp"     1 "F12_BINOM"      7
  Z←Z,⊂ ""         "⋆"   "Int"      1 "F12_POWER"      8
  Z←Z,⊂ ""         "⋆"   "Real"     1 "F12_POWER"      8
  Z←Z,⊂ ""         "⋆"   "Comp"     1 "F12_POWER"      8
  Z←Z,⊂ ""         "⍟"   "Int1"     1 "F12_LOGA"       9
  Z←Z,⊂ ""         "⍟"   "Real1"    1 "F12_LOGA"       9
  Z←Z,⊂ ""         "⍟"   "Comp1"    1 "F12_LOGA"       9
  Z←Z,⊂ ""         "○"   "Int"      1 "F12_CIRCLE"    10
  Z←Z,⊂ ""         "○"   "Real"     1 "F12_CIRCLE"    10
  Z←Z,⊂ ""         "○"   "Comp"     1 "F12_CIRCLE"    10
  Z←Z,⊂ ""         "∣"   "Int"      1 "F12_STILE"     11
  Z←Z,⊂ ""         "∣"   "Real"     1 "F12_STILE"     11
  Z←Z,⊂ ""         "∣"   "Comp"     1 "F12_STILE"     11
  Z←Z,⊂ ""         "?"   "Int2"     1 "F12_ROLL"      12
∇

∇Z←DYA_EXPR
  Z←⍬
  ⍝     A          OP    B          N CN              STAT
  ⍝-------------------------------------------------------
  Z←Z,⊂ "Int"      "+"   "Int"      2 "F12_PLUS"      13
  Z←Z,⊂ "Real"     "+"   "Real"     2 "F12_PLUS"      13
  Z←Z,⊂ "Comp"     "+"   "Comp"     2 "F12_PLUS"      13
  Z←Z,⊂ "Int"      "-"   "Int"      2 "F12_MINUS"     14
  Z←Z,⊂ "Real"     "-"   "Real"     2 "F12_MINUS"     14
  Z←Z,⊂ "Comp"     "-"   "Comp"     2 "F12_MINUS"     14
  Z←Z,⊂ "Int"      "×"   "Int"      2 "F12_TIMES"     15
  Z←Z,⊂ "Real"     "×"   "Real"     2 "F12_TIMES"     15
  Z←Z,⊂ "Comp"     "×"   "Comp"     2 "F12_TIMES"     15
  Z←Z,⊂ "Int"      "÷"   "Int1"     2 "F12_DIVIDE"    16
  Z←Z,⊂ "Real"     "÷"   "Real1"    2 "F12_DIVIDE"    16
  Z←Z,⊂ "Comp"     "÷"   "Comp1"    2 "F12_DIVIDE"    16
  Z←Z,⊂ "Bool"     "∧"   "Bool1"    2 "F2_AND"        17
  Z←Z,⊂ "Int"      "⊤∧"  "Int"      2 "F2_AND_B"      18
  Z←Z,⊂ "Bool"     "∨"   "Bool1"    2 "F2_OR"         19
  Z←Z,⊂ "Int"      "⊤∨"  "Int"      2 "F2_OR_B"       20
  Z←Z,⊂ "Bool"     "⍲"   "Bool1"    2 "F2_NAND"       21
  Z←Z,⊂ "Int"      "⊤⍲"  "Int"      2 "F2_NAND_B"     22
  Z←Z,⊂ "Bool"     "⍱"   "Bool1"    2 "F2_NOR"        23
  Z←Z,⊂ "Int"      "⊤⍱"  "Int"      2 "F2_NOR_B"      24
  Z←Z,⊂ "Int"      "⌈"   "Int"      2 "F12_RND_UP"    25
  Z←Z,⊂ "Real"     "⌈"   "Real"     2 "F12_RND_UP"    25
  Z←Z,⊂ "Int"      "⌊"   "Int"      2 "F12_RND_DN"    26
  Z←Z,⊂ "Real"     "⌊"   "Real"     2 "F12_RND_DN"    26
  Z←Z,⊂ "Int"      "!"   "Int"      2 "F12_BINOM"     27
  Z←Z,⊂ "Real"     "!"   "Real"     2 "F12_BINOM"     27
  Z←Z,⊂ "Comp"     "!"   "Comp"     2 "F12_BINOM"     27
  Z←Z,⊂ "Int"      "⋆"   "Int"      2 "F12_POWER"     28
  Z←Z,⊂ "Real"     "⋆"   "Real"     2 "F12_POWER"     28
  Z←Z,⊂ "Comp"     "⋆"   "Comp"     2 "F12_POWER"     28
  Z←Z,⊂ "Int1"     "⍟"   "Int1"     2 "F12_LOGA"      29
  Z←Z,⊂ "Real1"    "⍟"   "Real1"    2 "F12_LOGA"      29
  Z←Z,⊂ "Comp1"    "⍟"   "Comp1"    2 "F12_LOGA"      29
  Z←Z,⊂ "Int"      "<"   "Int"      2 "F2_LESS"       30
  Z←Z,⊂ "Real"     "<"   "Real"     2 "F2_LESS"       30
  Z←Z,⊂ "Comp"     "<"   "Comp"     2 "F2_LESS"       30
  Z←Z,⊂ "Int"      "≤"   "Int"      2 "F2_LEQU"       31
  Z←Z,⊂ "Real"     "≤"   "Real"     2 "F2_LEQU"       31
  Z←Z,⊂ "Comp"     "≤"   "Comp"     2 "F2_LEQU"       31
  Z←Z,⊂ "Int"      "="   "Int"      2 "F2_EQUAL"      32
  Z←Z,⊂ "Real"     "="   "Real"     2 "F2_EQUAL"      32
  Z←Z,⊂ "Comp"     "="   "Comp"     2 "F2_EQUAL"      32
  Z←Z,⊂ "Int"      "⊤="  "Int"      2 "F2_EQUAL_B"    33
  Z←Z,⊂ "Int"      "≠"   "Int"      2 "F2_UNEQU"      34
  Z←Z,⊂ "Real"     "≠"   "Real"     2 "F2_UNEQU"      34
  Z←Z,⊂ "Comp"     "≠"   "Comp"     2 "F2_UNEQU"      34
  Z←Z,⊂ "Int"      "⊤≠"  "Int"      2 "F2_UNEQ_B"     35
  Z←Z,⊂ "Int"      ">"   "Int"      2 "F2_GREATER"    36
  Z←Z,⊂ "Real"     ">"   "Real"     2 "F2_GREATER"    36
  Z←Z,⊂ "Comp"     ">"   "Comp"     2 "F2_GREATER"    36
  Z←Z,⊂ "Int"      "≥"   "Int"      2 "F2_MEQU"       37
  Z←Z,⊂ "Real"     "≥"   "Real"     2 "F2_MEQU"       37
  Z←Z,⊂ "Comp"     "≥"   "Comp"     2 "F2_MEQU"       37
  Z←Z,⊂ "1"        "○"   "Int"      2 "F12_CIRCLE"    38
  Z←Z,⊂ "1"        "○"   "Real"     2 "F12_CIRCLE"    38
  Z←Z,⊂ "1"        "○"   "Comp"     2 "F12_CIRCLE"    38
  Z←Z,⊂ "Int"      "∣"   "Int"      2 "F12_STILE"     39
  Z←Z,⊂ "Real"     "∣"   "Real"     2 "F12_STILE"     39
  Z←Z,⊂ "Comp"     "∣"   "Comp"     2 "F12_STILE"     39
  Z←Z,⊂ "1 2 3"    "⋸"   "Int"      2 "F2_FIND"       40
  Z←Z,⊂ "MatInt"   "+.×" "MatInt"   3 "OPER2_INNER"   41
  Z←Z,⊂ "MatReal"  "+.×" "MatReal"  3 "OPER2_INNER"   41
  Z←Z,⊂ "MatComp"  "+.×" "MatComp"  3 "OPER2_INNER"   41
  Z←Z,⊂ "VecInt"   "∘.×" "VecInt"   3 "OPER2_OUTER"   42
  Z←Z,⊂ "VecReal"  "∘.×" "VecReal"  3 "OPER2_OUTER"   42
  Z←Z,⊂ "VecComp"  "∘.×" "VecComp"  3 "OPER2_OUTER"   42
∇

∇INIT_DATA LEN;POOL;IntPool;RealPool;CompPool
  ⍝⍝ homogeneous (packed) data via ⎕RVAL -- see ScalarBenchmark.apl's
  ⍝⍝ INIT_DATA for the full rationale (packed-ravel fast paths, Int's
  ⍝⍝ ¯50..49 residue bound to avoid overflow-promotion, the POOL-sized
  ⍝⍝ draw before filtering so Int1/Int2/Real1/Comp1/etc. can never empty
  ⍝⍝ out by chance).
  ⍝⍝
  Int  ← 50 - 100 | ⎕RVAL 1 LEN (⊂0 1 0 0 0) 0
  Real ←            ⎕RVAL 1 LEN (⊂0 0 1 0 0) 0
  Comp ←            ⎕RVAL 1 LEN (⊂0 0 0 1 0) 0

  POOL     ← 4000 ⌈ LEN
  IntPool  ← 50 - 100 | ⎕RVAL 1 POOL (⊂0 1 0 0 0) 0
  RealPool ←            ⎕RVAL 1 POOL (⊂0 0 1 0 0) 0
  CompPool ←            ⎕RVAL 1 POOL (⊂0 0 0 1 0) 0

  Int1 ← LEN ⍴ (IntPool≠0)/IntPool
  Int2 ← LEN ⍴ (IntPool>0)/IntPool
  Bool ← 2 ∣ Int
  Bool1← 1 ⌽ Bool
  Real1← LEN ⍴ (RealPool≠0)/RealPool
  Comp1← LEN ⍴ (CompPool≠0)/CompPool

  MatInt   ← (2⍴⌈LEN⋆0.35)⍴Int
  MatReal  ← (2⍴⌈LEN⋆0.35)⍴Real
  MatComp  ← (2⍴⌈LEN⋆0.35)⍴Comp
  VecInt   ← (⌈LEN⋆0.5)⍴Int
  VecReal  ← (⌈LEN⋆0.5)⍴Real
  VecComp  ← (⌈LEN⋆0.5)⍴Comp
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

∇Z←C ONE_PASS_MW EXPR;JOB;ITER;ZZ;T0;T1;C0;C1
  ⍝ min-of-ILRC_MW elapsed cycles for EXPR at LEN_MW elements, with
  ⍝ "cores used" already set to C by the caller (SWEEP_CORES) and data
  ⍝ already set up by INIT_DATA. Cycles are measured the same way as
  ⍝ ScalarBenchmark.apl's ONE_PASS: bracket the whole ⍎JOB call with
  ⍝ ⎕FIO ¯1 rather than reading back FIO∆get_statistics, which races
  ⍝ across parallel worker threads (see that workspace for the details).
  ⍝
  JOB←TITLE EXPR
  ITER←0
  ZZ←⍬
  T0←24 60 60 1000⊥¯4↑⎕TS
L:
  C0←⎕FIO ¯1 ◊ Q←⍎JOB ◊ C1←⎕FIO ¯1
  ZZ←ZZ,C1-C0
  T1←24 60 60 1000⊥¯4↑⎕TS
  →((ITER≥1) ∧ TIME_LIMIT_MW<T1-T0)⍴DONE   ⍝ don't let it run too long
  →(ILRC_MW≥ITER←ITER+1)/L
DONE:
  Z←⌊/ZZ
  'core count=' C 'iter=' (≢ZZ) 'min. cycles=' Z
∇

∇Z←SWEEP_CORES EXPR;OP;TH1;TH2;MAXC;c;CURVE;S;BEST_S;BEST_C;TXT
  ⍝ ----------------------------------------------------
  ⍝ measure EXPR at LEN_MW elements once per core count 1..MAXC (⎕SYL
  ⍝ 'cores available'), then find the SPEEDUP curve's peak: the core
  ⍝ count beyond which adding more cores does not improve throughput
  ⍝ any further is where the memory wall was hit -- consistent with
  ⍝ core count 1 behaving as the true sequential baseline (GNU APL only
  ⍝ dispatches to the parallel path when the active core count is > 1,
  ⍝ so "cores used=1" never actually forks worker threads).
  ⍝
  TXT←78↑'  ===================== ', (TITLE EXPR), '  ', 80⍴ '='
  '' ◊ TXT ◊ ''

  OP←⊃EXPR[2]
  TH1←1 FIO∆set_monadic_threshold OP
  TH2←1 FIO∆set_dyadic_threshold  OP

  INIT_DATA LEN_MW
  MAXC←⎕SYL[SYL_ROW 'cores available';2]
  CURVE←MAXC⍴0
  c←1
LOOP:
  ⎕SYL[SYL_ROW 'cores used';2] ← c
  CURVE[c]←c ONE_PASS_MW EXPR
  →(MAXC≥c←c+1)/LOOP

  ⊣ TH1 FIO∆set_monadic_threshold OP
  ⊣ TH2 FIO∆set_dyadic_threshold  OP
  ⎕SYL[SYL_ROW 'cores used';2] ← 0

  S←CURVE[1]÷CURVE                 ⍝ speedup(c), relative to 1 core
  BEST_S←⌈/S
  BEST_C←S⍳BEST_S                  ⍝ smallest core count reaching the peak

  ''
  'cores:   ', ⍕⍳MAXC
  'cycles:  ', ⍕CURVE
  'speedup: ', ⍕S
  'memory wall hit at ', (⍕BEST_C), ' cores, speedup ', ⍕BEST_S

  ALL_EXPR←ALL_EXPR,⊂EXPR
  ALL_WALL←ALL_WALL,⊂BEST_C,BEST_S
  Z←BEST_C,BEST_S
∇

∇WRITE_MEMW ARITY;i;j;n;EXPR;KEY;SEEN;BESTC;BESTS;FIRST;WALL;OUT
  ⍝ write one line per distinct (arity, primitive) key seen in
  ⍝ ALL_EXPR/ALL_WALL, restricted to the given ARITY (1=monadic,
  ⍝ 2=dyadic, 3=operators) -- mirrors ScalarBenchmark.apl's
  ⍝ WRITE_THRESHOLDS. Where a primitive was measured for more than one
  ⍝ homogeneous type (Int/Real/Comp), the SMALLEST (most conservative)
  ⍝ core-count-at-wall across those types is kept, together with the
  ⍝ speedup measured for THAT type at THAT core count -- the wall is
  ⍝ hit for the whole primitive as soon as it is hit for any one type.
  ⍝
  SEEN←⍬
  n←≢ALL_EXPR
  i←1
LOOP:
  →(i>n)/DONE
  EXPR←i⊃ALL_EXPR
  →(ARITY≠EXPR[4])/SKIP
  KEY←(⍕EXPR[4]),⊃EXPR[5]
  →((⊂KEY)∊SEEN)/SKIP        ⍝ already handled this primitive
  SEEN←SEEN,⊂KEY

  FIRST←1
  j←1
JLOOP:
  →(j>n)/JDONE
  →(KEY≢(⍕(j⊃ALL_EXPR)[4]),⊃(j⊃ALL_EXPR)[5])/JNEXT
  WALL←j⊃ALL_WALL
  →(~FIRST)⍴NOTFIRST
  BESTC←WALL[1] ◊ BESTS←WALL[2] ◊ FIRST←0 ◊ →JNEXT
NOTFIRST:
  →(WALL[1]≥BESTC)/JNEXT     ⍝ keep the smallest core-count-at-wall
  BESTC←WALL[1] ◊ BESTS←WALL[2]
JNEXT:
  j←j+1 ◊ →JLOOP
JDONE:

  OUT←'memw_',(⍕EXPR[4])
  OUT←OUT, 16↑'(',(⊃EXPR[5]),','
  OUT←OUT, 6↑'_',((-1+0<⍴⊃EXPR[1])↑'AB'),','
  OUT←OUT, 10↑(TITLE1 EXPR),','
  OUT←OUT, 6↑(⍕BESTC),','
  OUT←OUT, (10↑⍕(⌊0.5+100×BESTS)÷100),')',⎕UCS ,10
  ⊣ OUT FIO∆fwrite_utf8 TH_FILE
SKIP:
  i←i+1 ◊ →LOOP
DONE:
∇

  ⍝ ----------------------------------------------------

∇GO;MAXC;ALL_EXPR;ALL_WALL;TH_FILE

  MAXC←⎕SYL[SYL_ROW 'cores available';2]
  'Running MemoryWall with up to' MAXC 'cores, ' LEN_MW 'items per pass...'

  ⍝ check that the core count can be set
  ⍝
  ⎕SYL[SYL_ROW 'cores used';2] ← MAXC
  →(MAXC = ⎕SYL[SYL_ROW 'cores used';2])⍴CORES_OK
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
  ⎕SYL[SYL_ROW 'cores used';2] ← 0

  ALL_EXPR←⍬
  ALL_WALL←⍬

  ⊣ SWEEP_CORES ¨ MON_EXPR
  ⊣ SWEEP_CORES ¨ DYA_EXPR

  TH_FILE←"w" FIO∆fopen "memory_wall"

  ⊣ "\n" FIO∆fwrite_utf8 TH_FILE
  WRITE_MEMW 1

  ⊣ "\n" FIO∆fwrite_utf8 TH_FILE
  WRITE_MEMW 2
  WRITE_MEMW 3

  ⊣ "\n" FIO∆fwrite_utf8 TH_FILE
  ⊣ "#undef memw_1\n" FIO∆fwrite_utf8 TH_FILE
  ⊣ "#undef memw_2\n" FIO∆fwrite_utf8 TH_FILE
  ⊣ "#undef memw_3\n" FIO∆fwrite_utf8 TH_FILE
  ⊣ "\n" FIO∆fwrite_utf8 TH_FILE

  ⊣ FIO∆fclose TH_FILE

 ''
 'Done -- wrote memory_wall'
∇


  GO

  )OFF
