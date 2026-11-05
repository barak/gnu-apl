#! /usr/local/bin/apl --script

  ⍝ tunable parameters for this benchmark program
  ⍝
  DO_PLOT←0             ⍝ do/don't plot the results of start-up cost
  ILRC←1000             ⍝ max. repeat count for the inner loop of start-up cost
  LEN_PI←100000         ⍝ vector length for measuring the per-item cost
  CORES←3               ⍝ number of cores used for parallel execution
  TIME_LIMIT←2000       ⍝ max. time per pass (milliseconds)
  SENTINEL←999999999    ⍝ internal "no break-even found" marker (see BREAK_EVEN);
                        ⍝ any value larger than LEN_PI works, the file gets the
                        ⍝ real 8888888888888888888ULL constant instead of this

)COPY 5 FILE_IO

∇Z←SYL_ROW TXT;LABELS
  ⍝⍝ return the (⎕IO-relative) row index of the ⎕SYL row whose label
  ⍝⍝ contains TXT. ⎕SYL rows shift as entries are added/removed (e.g.
  ⍝⍝ "cores available"/"cores used" used to be rows 25/26 and are now
  ⍝⍝ 26/27 -- a stale hardcoded row number silently reads/writes the
  ⍝⍝ wrong ⎕SYL row instead of failing loudly), so look the row up by
  ⍝⍝ its label text instead of trusting a fixed row number.
  ⍝⍝
  LABELS←⎕SYL[;1]
  Z←({∨/TXT⍷⍵}¨LABELS)⍳1
  →(Z≤≢LABELS)⍴0
  ⎕←'*** ⎕SYL row "',TXT,'" not found -- is this GNU APL built with parallel execution support?'
  ⎕←'*** (./configure CORE_COUNT_WANTED=... , see "make parallel"/"make parallel1")'
  ⍎')OFF'
∇

 ⍝ expressions to be benchmarked. The integer STAT is the statistics
 ⍝ as in Performance.def, i.e. 0 for F12_PLUS, 1 for F12_MINUS, and so on.
 ⍝
 ⍝ B (and A, for dyadic rows) name variables set up by INIT_DATA. Int,
 ⍝ Real, and Comp are homogeneous (packed) ravels -- all-int, all-float,
 ⍝ and all-complex respectively -- unlike the "Mix_IRC" arrays used here
 ⍝ before: concatenating different cell types forces an unpacked,
 ⍝ per-Cell ravel, so a "Mix_IRC" benchmark never actually exercised the
 ⍝ packed-ravel fast paths (see Ravel.cc IntRavel/FloatRavel/ComplexRavel
 ⍝ ::apply_fast_dyadic/monadic) that both sequential and parallel
 ⍝ execution try first. Primitives that support all three homogeneous
 ⍝ types below get one row per type; primitives restricted to a subset
 ⍝ (booleans, positive integers, ...) keep a single, already-homogeneous
 ⍝ source, unchanged. Each row's break-even is measured separately and
 ⍝ then the WORST (largest, i.e. most conservative) of the per-type
 ⍝ break-evens for a given primitive is what ends up in the generated
 ⍝ parallel_thresholds file, since Function::parallel_thresholds[2]
 ⍝ has only one slot per primitive (not one per element type).
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
  ⍝ ∼ (logical not) is defined for booleans only
  ⍝
  Z←Z,⊂ ""         "∼"   "Bool"     1 "F12_WITHOUT"    4
  Z←Z,⊂ ""         "⌈"   "Int"      1 "F12_RND_UP"     5
  Z←Z,⊂ ""         "⌈"   "Real"     1 "F12_RND_UP"     5
  Z←Z,⊂ ""         "⌈"   "Comp"     1 "F12_RND_UP"     5
  Z←Z,⊂ ""         "⌊"   "Int"      1 "F12_RND_DN"     6
  Z←Z,⊂ ""         "⌊"   "Real"     1 "F12_RND_DN"     6
  Z←Z,⊂ ""         "⌊"   "Comp"     1 "F12_RND_DN"     6
  ⍝ !B (factorial) has poles at negative integers (confirmed: !¯3 is a
  ⍝ DOMAIN ERROR); Int2 (positive only) avoids them. Real/Comp are drawn
  ⍝ from ⎕RVAL's continuous [0,1) magnitude and essentially never land
  ⍝ exactly on a negative integer, so they are safe unfiltered.
  ⍝
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
  ⍝ ?B (roll/deal) needs a positive integer upper bound
  ⍝
  Z←Z,⊂ ""         "?"   "Int2"     1 "F12_ROLL"      12
∇

∇Z←DYA_EXPR
  Z←⍬
  ⍝     A          OP    B          N CN              STAT
  ⍝-------------------------------------------------------
  ⍝ NOTE: F2_LEQU/F2_UNEQU/F2_MEQU/F2_FIND (not F2_LEQ/F2_UNEQ/F2_MEQ/
  ⍝ F12_FIND) are the correct C++ names from Performance.def -- an
  ⍝ earlier version of this script used the wrong (truncated) names for
  ⍝ 3 of these 4, which meant UserPreferences::read_threshold_file()'s
  ⍝ strncmp() match never fired for them and any measured value in
  ⍝ parallel_thresholds was silently ignored at start-up.
  ⍝
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
  ⍝ ∧ ∨ ⍲ ⍱ (logical) are defined for booleans only; their ⊤-prefixed
  ⍝ siblings are the bit-wise variants and operate on integers.
  ⍝
  Z←Z,⊂ "Bool"     "∧"   "Bool1"    2 "F2_AND"        17
  Z←Z,⊂ "Int"      "⊤∧"  "Int"      2 "F2_AND_B"      18
  Z←Z,⊂ "Bool"     "∨"   "Bool1"    2 "F2_OR"         19
  Z←Z,⊂ "Int"      "⊤∨"  "Int"      2 "F2_OR_B"       20
  Z←Z,⊂ "Bool"     "⍲"   "Bool1"    2 "F2_NAND"       21
  Z←Z,⊂ "Int"      "⊤⍲"  "Int"      2 "F2_NAND_B"     22
  Z←Z,⊂ "Bool"     "⍱"   "Bool1"    2 "F2_NOR"        23
  Z←Z,⊂ "Int"      "⊤⍱"  "Int"      2 "F2_NOR_B"      24
  ⍝ dyadic ⌈/⌊ (max/min) are DOMAIN ERROR for complex operands -- unlike
  ⍝ monadic ⌈/⌊, which rounds a single complex number's real/imag parts
  ⍝ independently and works fine, "max"/"min" of two complex numbers is
  ⍝ undefined (no natural total order) -- confirmed via (2J3)⌈(1J1).
  ⍝
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
  ⍝ <, ≤, >, ≥ (ordering) unexpectedly do NOT domain-error for complex in
  ⍝ GNU APL -- confirmed via (2J3)<(1J1) etc. all returning 0/1 -- so
  ⍝ Comp is included here too, even though ISO leaves complex ordering
  ⍝ undefined.
  ⍝
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
  ⍝ ⋸ (find/interval membership) is structural, not about numeric type;
  ⍝ a single small literal A searched for within a homogeneous B is the
  ⍝ natural (and only meaningful) test here.
  ⍝
  Z←Z,⊂ "1 2 3"    "⋸"   "Int"      2 "F2_FIND"       40
  Z←Z,⊂ "MatInt"   "+.×" "MatInt"   3 "OPER2_INNER"   41
  Z←Z,⊂ "MatReal"  "+.×" "MatReal"  3 "OPER2_INNER"   41
  Z←Z,⊂ "MatComp"  "+.×" "MatComp"  3 "OPER2_INNER"   41
  Z←Z,⊂ "VecInt"   "∘.×" "VecInt"   3 "OPER2_OUTER"   42
  Z←Z,⊂ "VecReal"  "∘.×" "VecReal"  3 "OPER2_OUTER"   42
  Z←Z,⊂ "VecComp"  "∘.×" "VecComp"  3 "OPER2_OUTER"   42
∇

∇INIT_DATA LEN;POOL;IntPool;RealPool;CompPool
  ⍝⍝
  ⍝⍝ setup homogeneous (packed) variables used in benchmark expressions,
  ⍝⍝ using ⎕RVAL rather than fixed/hand-rolled data:
  ⍝⍝
  ⍝⍝   ⎕RVAL B  with  B←(rank shape (⊂type_dist) maxdepth)
  ⍝⍝
  ⍝⍝ rank=1 and shape=LEN (a scalar, broadcast to the one axis) force an
  ⍝⍝ exact length-LEN vector; maxdepth=0 together with a type distribution
  ⍝⍝ that is all-0 except for one type forces a simple, homogeneous ravel
  ⍝⍝ of exactly that type -- see Quad_RVAL::do_eval_B / result_type. The
  ⍝⍝ type distribution order is CHAR INT REAL COMPLEX NESTED.
  ⍝⍝
  ⍝⍝ Int is bounded to ¯50..49 via residue (not returned raw from ⎕RVAL,
  ⍝⍝ which draws a full-range random int64): summing two full-range int64
  ⍝⍝ values overflows almost certainly, forcing IntCell::bif_add_ii's
  ⍝⍝ overflow-promote-to-float path on nearly every element, which would
  ⍝⍝ measure the promotion path's cost instead of the intended packed
  ⍝⍝ int64 fast path's steady-state cost. Real/Comp need no such bounding:
  ⍝⍝ ⎕RVAL's random_ieee() already returns magnitudes in [0,1), and
  ⍝⍝ floating-point op cost does not depend on operand magnitude the way
  ⍝⍝ integer overflow-promotion does.
  ⍝⍝
  Int  ← 50 - 100 | ⎕RVAL 1 LEN (⊂0 1 0 0 0) 0    ⍝ homogeneous ints,  ¯50..49
  Real ←            ⎕RVAL 1 LEN (⊂0 0 1 0 0) 0    ⍝ homogeneous floats, [0,1)
  Comp ←            ⎕RVAL 1 LEN (⊂0 0 0 1 0) 0    ⍝ homogeneous complex

  ⍝ Int1/Int2/Real1/Comp1 filter out zero/non-positive elements and then
  ⍝ reshape back up to LEN. For the tiny lengths FIGURE_A uses to measure
  ⍝ start-up cost (LEN as low as 1), filtering Int/Real/Comp directly can
  ⍝ legitimately empty out by chance (e.g. LEN=1 and that one int is 0),
  ⍝ and LEN⍴⍬ is a DOMAIN ERROR -- confirmed via a real crash on "÷ Int1"
  ⍝ at LEN=1. Filter from a POOL-sized draw instead (independent of LEN,
  ⍝ large enough that "everything filtered out" is astronomically
  ⍝ unlikely) and reshape *that* down/up to LEN, same as the pre-⎕RVAL
  ⍝ version of this script did with its fixed-size profile pools.
  ⍝
  POOL     ← 4000 ⌈ LEN
  IntPool  ← 50 - 100 | ⎕RVAL 1 POOL (⊂0 1 0 0 0) 0
  RealPool ←            ⎕RVAL 1 POOL (⊂0 0 1 0 0) 0
  CompPool ←            ⎕RVAL 1 POOL (⊂0 0 0 1 0) 0

  Int1 ← LEN ⍴ (IntPool≠0)/IntPool   ⍝ non-zero ints
  Int2 ← LEN ⍴ (IntPool>0)/IntPool   ⍝ positive ints
  Bool ← 2 ∣ Int                     ⍝ booleans
  Bool1← 1 ⌽ Bool                    ⍝ also booleans
  Real1← LEN ⍴ (RealPool≠0)/RealPool ⍝ non-zero reals (RealPool≥0 already)
  Comp1← LEN ⍴ (CompPool≠0)/CompPool ⍝ non-zero complex

  ⍝ homogeneous matrix/vector variants for +.×/∘.× (OPER2_INNER/OUTER);
  ⍝ Mat1_IRC/Vec1_IRC (mixed Int,Real,Comp) are kept only because they
  ⍝ were part of the original, pre-⎕RVAL data set and nothing above
  ⍝ still references them.
  ⍝
  MatInt   ← (2⍴⌈LEN⋆0.35)⍴Int
  MatReal  ← (2⍴⌈LEN⋆0.35)⍴Real
  MatComp  ← (2⍴⌈LEN⋆0.35)⍴Comp
  VecInt   ← (⌈LEN⋆0.5)⍴Int
  VecReal  ← (⌈LEN⋆0.5)⍴Real
  VecComp  ← (⌈LEN⋆0.5)⍴Comp
  Mat1_IRC ← (2⍴⌈LEN⋆0.35)⍴Int,Real,Comp
  Vec1_IRC ← (⌈LEN⋆0.5)⍴Int,Real,Comp
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

∇Z←VL ONE_PASS EXPR;OP;JOB;ITER;ZZ;TH1;TH2;CYCLES;T0;T1;C0;C1
  ⍝ ----------------------------------------------------
  ⍝ Run one pass (for one vector length VL)
  ⍝ return (VL CYCLES), CYCLES is CPU cycles for all VL items
  ⍝
  ⍝ VL:   length of the vector
  ⍝ EXPR: one line in MON_EXPR or DYA_EXPR,
  ⍝       e.g. "+" "Int"  1 "F12_PLUS"
  ⍝
  ⍝ CYCLES is measured by bracketing the whole ⍎JOB call with ⎕FIO ¯1
  ⍝ (the raw CPU cycle counter) rather than by going through
  ⍝ FIO∆clear_statistics/FIO∆get_statistics (the interpreter's internal,
  ⍝ per-primitive CellFunctionStatistics "first"/"subsequent" counters).
  ⍝ Those counters are updated via CELL_PERFORMANCE_END from every
  ⍝ per-core worker thread (ScalarFunction.cc PF_scalar_AB/PF_scalar_B)
  ⍝ with no lock around the add_sample() call, so reading them back
  ⍝ from here immediately after a parallel ⍎JOB races the still-running
  ⍝ (or just-finished, not-yet-visible-in-this-thread) worker threads --
  ⍝ confirmed live: with cores>1 this occasionally produced a "min.
  ⍝ cycles" reading for 100000 packed elements two orders of magnitude
  ⍝ smaller than physically possible, corrupting ⌊⌿ZZ[2;]'s minimum.
  ⍝ ⎕FIO ¯1 measures elapsed cycles for the entire call from the calling
  ⍝ (this) thread only, so it is immune to that race.
  ⍝
  OP←⊃EXPR[2]      ⍝ e.g. +
  JOB←TITLE EXPR   ⍝ e.g. "+ Int"

  ⍝ save current thresholds
  ⍝
  TH1← 1 FIO∆set_monadic_threshold OP
  TH2← 1 FIO∆set_dyadic_threshold  OP

  ITER←0
  ZZ←2 0⍴0
  T0←24 60 60 1000⊥¯4↑⎕TS
L:
  C0←⎕FIO ¯1 ◊ Q←⍎JOB ◊ C1←⎕FIO ¯1
  CYCLES←C1-C0
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

  'PASS: Len=' VL 'iter=' ITER 'min. cycles=' Z[2] 'total cycles' (+/ZZ[2;])
∇

  ⍝ ----------------------------------------------------
  ⍝ figure start-up times for sequential and parallel execution.
  ⍝ We use small vector sizes for better precision
  ⍝
∇Z←FIGURE_A EXPR;LENGTHS;I;LEN;ZS;ZP;SA;SB;PA;PB;H1;H2;P;TXT
  TXT←78↑'  ===================== ', (TITLE EXPR), '  ', 80⍴ '='
  '' ◊ TXT ◊ ''
  Z←0 3⍴0
  ⍝ Sample lengths entirely at or above "min. element count for packed
  ⍝ ravels" (⎕SYL), so every sample uses the packed representation.
  ⍝ Lengths straddling that threshold would mix two different regimes
  ⍝ (unpacked cell-by-cell below it, packed above) into one linear fit,
  ⍝ producing a corrupted (sometimes even negative) start-up-cost
  ⍝ intercept -- confirmed live: with LENGTHS 1..20 (this threshold is
  ⍝ 12 on this machine) nearly every primitive's break-even came out as
  ⍝ either 0 or "not reached", neither of which matches the more
  ⍝ plausible mid-range values (tens to low hundreds) seen before this
  ⍝ fix. Consistent with FIGURE_B, which always measures at LEN_PI (also
  ⍝ packed).
  ⍝
  LL←⍴LENGTHS←⌽(⎕SYL[SYL_ROW 'min. element count for packed ravels';2])+⍳20
  'Benchmarking start-up cost for "', (TITLE EXPR), '" ...'

  I←1 ◊ ZS←0 2⍴0
  ⎕SYL[SYL_ROW 'cores used';2] ← 0   ⍝ sequential
LS: INIT_DATA LEN←LENGTHS[I]
  ZS←ZS⍪LEN ONE_PASS EXPR
  →(LL≥I←I+1)⍴LS

  I←1 ◊ ZP←0 2⍴0
  ⎕SYL[SYL_ROW 'cores used';2] ← CORES   ⍝ parallel
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
  ⎕SYL[SYL_ROW 'cores used';2] ← 0       ⍝ sequential
  (LEN SCYC)←LEN_PI ONE_PASS EXPR
  ⎕SYL[SYL_ROW 'cores used';2] ← CORES   ⍝ parallel
  (LEN PCYC)←LEN_PI ONE_PASS EXPR
  Z←⊂TITLE EXPR
  ⍝ SCYC/PCYC are the *whole* LEN_PI-element call's elapsed cycles (see
  ⍝ ONE_PASS); dividing by LEN_PI after removing the fixed per-call
  ⍝ overhead (SOFF/POFF) turns that into a genuine per-item rate, the
  ⍝ units BREAK_EVEN's (Ac-A0)÷(B0-Bc) formula and FIGURE_A's B0/Bc
  ⍝ slopes need. This division was missing here even before the switch
  ⍝ away from FIO∆get_statistics -- unnoticed only because STAT[4]
  ⍝ happened to already report a single cell's cost regardless of LEN.
  ⍝
  Z←Z, ⌈ (SCYC - SOFF) ÷ LEN_PI
  Z←Z, ⌈ (PCYC - POFF) ÷ LEN_PI
  TS←'per item cost sequential:       ',(¯8↑⍕Z[2]), ' cycles'
  TP←'per item cost parallel:         ',(¯8↑⍕Z[3]), ' cycles'
  SUMMARY←SUMMARY,(⊂TS),(⊂TP)

  SUP_A BREAK_EVEN (⊂EXPR),Z
∇

∇SUP BREAK_EVEN PERI;EXPR;OP;ICS;ICP;SUPS;SUPP;T1;T2;BE
  ⍝ compute the break-even length for one (primitive, type) row and
  ⍝ record it (grouped/aggregated across types per primitive, see
  ⍝ WRITE_THRESHOLDS) instead of writing it to the file directly.
  ⍝
  (SUPS SUPP)←SUP   ⍝ start-up cost
  (EXPR OP ICS ICP)←PERI ⍝ per-item cost
  T1←'parallel break-even length:     '
  T2←'     not reached' ◊ BE←SENTINEL
  →(ICP ≥ ICS)⍴1+↑⎕LC ◊ T2←¯8↑⍕BE←⌈ (SUPP - SUPS) ÷ ICS - ICP
  SUMMARY←SUMMARY,(⊂T1,T2),⊂''

  ALL_EXPR←ALL_EXPR,⊂EXPR
  ALL_BE←ALL_BE,BE
∇

∇WRITE_THRESHOLDS ARITY;i;j;n;EXPR;KEY;SEEN;BEST;OUT
  ⍝ write one line per distinct (arity, primitive) key seen in
  ⍝ ALL_EXPR/ALL_BE, restricted to the given ARITY (1=monadic,
  ⍝ 2=dyadic, 3=operators). The value written is the WORST (largest)
  ⍝ break-even across every row (i.e. every homogeneous type) measured
  ⍝ for that primitive -- the most conservative choice, since
  ⍝ Function::parallel_thresholds[] has only one slot per primitive,
  ⍝ not one per element type. Rows in their original (first-seen) order
  ⍝ determine the order of the generated lines.
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

  BEST←0        ⍝ NOT SENTINEL: this is the seed for a MAX-reduction
                 ⍝ below, and SENTINEL is already the largest value any
                 ⍝ row can have, so seeding with it would make BEST⌈...
                 ⍝ stick at SENTINEL forever regardless of any real
                 ⍝ measurement -- confirmed live: every primitive came
                 ⍝ out "not reached" even when every one of its measured
                 ⍝ rows had a real, small break-even length.
  j←1
JLOOP:
  →(j>n)/JDONE
  →(KEY≢(⍕(j⊃ALL_EXPR)[4]),⊃(j⊃ALL_EXPR)[5])/JNEXT
  BEST←BEST⌈j⊃ALL_BE
JNEXT:
  j←j+1 ◊ →JLOOP
JDONE:

  OUT←'perfo_',(⍕EXPR[4])
  OUT←OUT, 16↑'(',(⊃EXPR[5]),','
  OUT←OUT, 6↑'_',((-1+0<⍴⊃EXPR[1])↑'AB'),','
  OUT←OUT, 10↑(TITLE1 EXPR),','
  →(BEST<SENTINEL)⍴REAL
  OUT←OUT, '8888888888888888888ULL)',⎕UCS ,10
  →WOUT
REAL:
  OUT←OUT, (21↑⍕BEST),')',⎕UCS ,10
WOUT:
  ⊣ OUT FIO∆fwrite_utf8 TH_FILE
SKIP:
  i←i+1 ◊ →LOOP
DONE:
∇

  ⍝ ----------------------------------------------------

∇GO;DYA_A;MON_A;SUMMARY;TH_FILE;ALL_EXPR;ALL_BE

  CORES←CORES ⌊ ⎕SYL[SYL_ROW 'cores available';2]
  'Running ScalarBenchmark_2 with' CORES 'cores...'

  ⍝ check that the core count can be set
  ⍝
  ⎕SYL[SYL_ROW 'cores used';2] ← CORES
  →(CORES = ⎕SYL[SYL_ROW 'cores used';2])⍴CORES_OK
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
  ALL_BE←⍬

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
  WRITE_THRESHOLDS 1

  ⊣ "\n" FIO∆fwrite_utf8 TH_FILE

  ⊣ (⊂DYA_A) FIGURE_B ¨ DYA_EXPR
  WRITE_THRESHOLDS 2
  WRITE_THRESHOLDS 3

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
