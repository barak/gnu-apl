#! /usr/local/bin/apl --script
# vim: et:ts=4:sw=4

  ⍝ run like:
  ⍝ src/apl -f workspaces/Scalar2.apl -- -c 3,6 -d 200×⍳20 -f 38

  ⍝ tunable parameters for this benchmark program
  ⍝
  ⍙PROFILE←4000 2000 50  ⍝ fractions of Integer, Real, and Complex numbers

⊣⍎')COPY 5 FILE_IO.apl FIO∆set_monadic_threshold FIO∆set_dyadic_threshold'
⊣⍎')COPY 5 FILE_IO.apl FIO∆clear_statistics FIO∆get_statistics'

 ⍝ expressions to be benchmarked. The integer STAT is the statistics
 ⍝ as in Performance.def, i.e. 0 for F12_PLUS, 1 for F12_MINUS, and so on.
 ⍝
∇Z←MON_EXPR
  Z←⍬
  ⍝     A            OP     B            N   CN          STAT
  ⍝----------------------------------------------------------
  Z←Z,⊂ ""           "+"    "⍙Mix_IRC"   1   "F12_PLUS"     0
  Z←Z,⊂ ""           "-"    "⍙Mix_IRC"   1   "F12_MINUS"    1
  Z←Z,⊂ ""           "×"    "⍙Mix_IRC"   1   "F12_TIMES"    2
  Z←Z,⊂ ""           "÷"    "⍙Mix1_IRC"  1   "F12_DIVIDE"   3
  Z←Z,⊂ ""           "∼"    "⍙Bool"      1   "F12_WITHOUT"  4
  Z←Z,⊂ ""           "⌈"    "⍙Mix_IR"    1   "F12_RND_UP"   5
  Z←Z,⊂ ""           "⌊"    "⍙Mix_IR"    1   "F12_RND_DN"   6
  Z←Z,⊂ ""           "!"    "⍙Int2"      1   "F12_BINOM"    7
  Z←Z,⊂ ""           "⋆"    "⍙Mix_IRC"   1   "F12_POWER"    8
  Z←Z,⊂ ""           "⍟"    "⍙Mix1_IRC"  1   "F12_LOGA"     9
  Z←Z,⊂ ""           "○"    "⍙Mix_IRC"   1   "F12_CIRCLE"  10
  Z←Z,⊂ ""           "∣"    "⍙Mix_IR"    1   "F12_STILE"   11
  Z←Z,⊂ ""           "?"    "⍙Int2"      1   "F12_ROLL"    12
∇

∇Z←DYA_EXPR
  Z←⍬
  ⍝     A            OP     B            N  CN           STAT
  ⍝----------------------------------------------------------
  Z←Z,⊂ "⍙Mix_IRC"   "+"    "⍙Mix_IRC"   2  "F12_PLUS"     13
  Z←Z,⊂ "⍙Mix_IRC"   "-"    "⍙Mix_IRC"   2  "F12_MINUS"    14
  Z←Z,⊂ "⍙Mix_IRC"   "×"    "⍙Mix1_IRC"  2  "F12_TIMES"    15
  Z←Z,⊂ "⍙Mix1_IRC"  "÷"    "⍙Mix1_IRC"  2  "F12_DIVIDE"   16
  Z←Z,⊂ "⍙Bool"      "∧"    "⍙Bool1"     2  "F2_AND"       17
  Z←Z,⊂ "⍙Int"       "⊤∧"   "⍙Int"       2  "F2_AND_B"     18
  Z←Z,⊂ "⍙Bool"      "∨"    "⍙Bool1"     2  "F2_OR"        19
  Z←Z,⊂ "⍙Int"       "⊤∨"   "⍙Int"       2  "F2_OR_B"      20
  Z←Z,⊂ "⍙Bool"      "⍲"    "⍙Bool1"     2  "F2_NAND"      21
  Z←Z,⊂ "⍙Int"       "⊤⍲"   "⍙Int"       2  "F2_NAND_B"    22
  Z←Z,⊂ "⍙Bool"      "⍱"    "⍙Bool1"     2  "F2_NOR"       23
  Z←Z,⊂ "⍙Int"       "⊤⍱"   "⍙Int"       2  "F2_NOR_B"     24
  Z←Z,⊂ "⍙Mix_IR"    "⌈"    "⍙Mix_IR"    2  "F12_RND_UP"   25
  Z←Z,⊂ "⍙Mix_IR"    "⌊"    "⍙Mix_IR"    2  "F12_RND_DN"   26
  Z←Z,⊂ "⍙Mix_IRC"   "!"    "⍙Mix_IRC"   2  "F12_BINOM"    27
  Z←Z,⊂ "⍙Mix_IRC"   "⋆"    "⍙Mix_IRC"   2  "F12_POWER"    28
  Z←Z,⊂ "⍙Mix1_IRC"  "⍟"    "⍙Mix1_IRC"  2  "F12_LOGA"     29
  Z←Z,⊂ "⍙Mix_IR "   "<"    "⍙Mix_IR"    2  "F2_LESS"      30
  Z←Z,⊂ "⍙Mix_IR "   "≤"    "⍙Mix_IR"    2  "F2_LEQ"       31
  Z←Z,⊂ "⍙MMix_IRC"  "="    "⍙Mix_IRC"   2  "F2_EQUAL"     32
  Z←Z,⊂ "⍙MInt"      "⊤="   "⍙Int"       2  "F2_EQUAL_B"   33
  Z←Z,⊂ "⍙MInt2"     "≠"    "⍙Mix_IRC"   2  "F2_UNEQ"      34
  Z←Z,⊂ "⍙MInt"      "⊤≠"   "⍙Int"       2  "F2_UNEQ_B"    35
  Z←Z,⊂ "⍙Mix_IR"    ">"    "⍙Mix_IR"    2  "F2_GREATER"   36
  Z←Z,⊂ "⍙Mix_IR"    "≥"    "⍙Mix_IR"    2  "F2_MEQ"       37
  Z←Z,⊂ "¯6"         "○"    "⍙Mix_IRC"   2  "F12_CIRCLE"   38
  Z←Z,⊂ "⍙Mix_IRC"   "∣"    "⍙Mix_IRC"   2  "F12_STILE"    39
  Z←Z,⊂ "1 2 3"      "⋸"    "⍙Int"       2  "F12_FIND"     40
  Z←Z,⊂ "⍙Mat1_IRC"  "+.×"  "⍙Mat1_IRC"  3  "OPER2_INNER"  41
  Z←Z,⊂ "⍙Vec1_IRC"  "∘.×"  "⍙Vec1_IRC"  3  "OPER2_OUTER"  42
∇
⍝ ====================================================================
∇Z←A contains B
 Z←(A⍳⊂B)≤⍴A
∇

∇decode_ARGS;ARGS;OPT_C;OPT_D;OPT_H
 ⍝
 ⍝⍝ decode the command line arguments (if any) with which the script was called.
 ⍝
 ARGS←⎕ARG↓⍨⎕ARG⍳⊂'--'
 OPT_C←ARGS↓⍨ARGS⍳⊂'-c'
 OPT_D←ARGS↓⍨ARGS⍳⊂'-d'
 OPT_F←ARGS↓⍨ARGS⍳⊂'-f'
 OPT_H←(ARGS contains '-h') ∨ (ARGS contains '--help')

 ⍝ pick option arg, or its default if not provided
 ⍝
 OPT_C←⊃↑OPT_C, ⊂"2,4,8,12"
 OPT_D←⊃↑OPT_D, ⊂"⍳20"
 OPT_F←⊃↑OPT_F, ⊂" 1"

  ∆CORES←⍎OPT_C ◊ '∆CORES:' ∆CORES
  ∆DLENS←⍎OPT_D ◊ '∆DLENS:' ∆DLENS
  ∆FUNC ←⍎OPT_F ◊ '∆FUNC: ' ∆FUNC
  ∆FUNC ←1⌈(⍬⍴⍴MON_EXPR,DYA_EXPR)⌊∆FUNC
  EXPR: (MON_EXPR,DYA_EXPR)[∆FUNC]
  SPEEDUPS←0 2⍴0

 →0↓⍨(⎕ARG contains '-h') ∨ ⎕ARG contains '--help'

 ⊃¨"""
Usage: apl -f <path-to-workspace/> -- options
options:
    -h, --help       print this text
    -c corecounts               e.g. -c 3,4,7  for core counts 3, 4, and 7
    -d datalengths              e.g. -d 2+⍳20  for data lengths 2, 3, ... 22
    -f function (STAT) number   e.g. -f 39 for Z ← ¯6○ ⍙Mix_IRC
   """
∇

∇INIT_DATA LEN;N;Ilen;Rlen;Clen
  ⍝⍝
  ⍝⍝ setup variables used in benchmark expressions:
  ⍝⍝ ⍙Int:  ¯2 ... 9
  ⍝⍝ ⍙Int1: nonzero Int
  ⍝⍝ ⍙Real: ¯10 to 10 or so
  ⍝⍝
  (Ilen Rlen Clen)←⍙PROFILE           ⍝ relative amounts of int, real, complex
  ⍙Int  ← 10 - ? Ilen ⍴ 12            ⍝ Integers -2 ... 10
  ⍙Int1 ← Ilen ⍴ (⍙Int≠0)/⍙Int        ⍝ non-zero Integers -10 ... 10
  ⍙Int2 ← Ilen ⍴ (⍙Int>0) / ⍙Int      ⍝ positive Integers 1 ... 10
  ⍙Bool ← 2 ∣ ⍙Int                    ⍝ Booleans
  ⍙Bool1← 1 ⌽ ⍙Bool                   ⍝ also Booleans
  ⍙Real ← Rlen ⍴ ⍙Int + 3 ÷ ○1        ⍝ reals
  ⍙Real1← Rlen ⍴ (⍙Real≠0)/⍙Real      ⍝ non-zero reals
  ⍙Real2← Rlen ⍴ (⍙Real>0)/⍙Real      ⍝ positive reals 1 ... 10
  ⍙Comp ← Clen ⍴ ⍙Real + 0J1×1⌽⍙Real  ⍝ complex
  ⍙Comp1← Clen ⍴ (⍙Comp≠0)/⍙Comp      ⍝ non-zero complex

  ⍙Mix_IR   ←⍙Int,⍙Real          ◊ ⍙Mix_IR  [N?N←⍴⍙Mix_IR  ]  ← ⍙Mix_IR
  ⍙Mix_IRC  ←⍙Int,⍙Real,⍙Comp    ◊ ⍙Mix_IRC [N?N←⍴⍙Mix_IRC ]  ← ⍙Mix_IRC
  ⍙Mix1_IRC ←⍙Int1,⍙Real1,⍙Comp1 ◊ ⍙Mix1_IRC[N?N←⍴⍙Mix1_IRC]  ← ⍙Mix1_IRC

  ⍙Int      ← LEN ⍴ ⍙Int
  ⍙Int1     ← LEN ⍴ ⍙Int1
  ⍙Int2     ← LEN ⍴ ⍙Int2
  ⍙Bool     ← LEN ⍴ ⍙Bool
  ⍙Bool1    ← LEN ⍴ ⍙Bool1
  ⍙Real     ← LEN ⍴ ⍙Real
  ⍙Real1    ← LEN ⍴ ⍙Real1
  ⍙Real2    ← LEN ⍴ ⍙Real2
  ⍙Comp     ← LEN ⍴ ⍙Comp
  ⍙Comp1    ← LEN ⍴ ⍙Comp1
  ⍙Mix_IR   ← LEN ⍴ ⍙Mix_IR
  ⍙Mix_IRC  ← LEN ⍴ ⍙Mix_IRC
  ⍙Mix1_IRC ← LEN ⍴ ⍙Mix1_IRC
  ⍙Mat1_IRC ← (2⍴⌈LEN⋆0.35)⍴⍙Mix1_IRC
  ⍙Vec1_IRC ← (⌈LEN⋆0.5)⍴⍙Mix1_IRC
∇

⍝ ====================================================================
∇Z←ONE_PASS EXPR;A;JOB;OP;SID;TH1;TH2;STAT;JOBZ
  ⍝ Run one pass.  ⍝ return (VL CYCLES), CYCLES is CPU cycles for all VL items
  ⍝
  ⍝ EXPR: A         OP  B          CAT SIDNAME    SID
  ⍝  e.g. "Mix_IRC" "+" "Mix_IRC"  2   "F12_PLUS" 13
  ⍝
  JOB←'JOBZ←',⊃,/EXPR[1 2 3]
  A←⊃EXPR[1]      ⍝ "" for monadic
  OP←⊃EXPR[2]      ⍝ e.g. +
  SID←EXPR[6]
  SID←42 - A≡""   ⍝ SCALAR_AB / SCALAR_B

  ⍝ save current thresholds and set current thresholds to 1
  ⍝
  TH1← 1 FIO∆set_monadic_threshold OP
  TH2← 1 FIO∆set_dyadic_threshold  OP

  FIO∆clear_statistics SID ◊ ⊣⍎JOB ◊ STAT ← FIO∆get_statistics SID

  ⍝ restore saved thresholds
  ⊣ TH1 FIO∆set_monadic_threshold OP
  ⊣ TH2 FIO∆set_dyadic_threshold  OP

 Z←(⊂JOBZ),STAT[3 4 5]   ⍝ count / sum / sum2
∇

⍝ ====================================================================
∇ATTS←PLOT_WIN_ATTS;A;OP;B
 ⍝
 ⍝⍝ return attributes for ⎕PLOT (one core count, variable length)
 ⍝
 (A OP B)←3↑EXPR

 ATTS.caption      ← A, ' ', OP, ' ', B, ' on ', (⍕CORES), ' cores'
 ATTS.pw_pos_X     ← 200+30×CORES
 ATTS.pw_pos_Y     ← 200+30×CORES

 ATTS.pa_border_L   ← 60
 ATTS.legend_X      ← 80
 ATTS.legend_Y      ← 45
 ATTS.legend_name_1 ← "measured cycles sequential"
 ATTS.legend_name_2 ← "linearized cycles sequential"
 ATTS.legend_name_3 ← "measured cycles parallel"
 ATTS.legend_name_4 ← "linearized cycles parallel"
 ATTS.point_color_1 ← "#00C000"
 ATTS. line_color_1 ← "#00C000"
 ATTS.point_color_2 ← "#00C000"
 ATTS. line_color_2 ← "#00C000"
 ATTS.point_color_3 ← "#C00000"
 ATTS. line_color_3 ← "#C00000"
 ATTS.point_color_4 ← "#C00000"
 ATTS.line_color_4  ← "#C00000"

 ATTS.line_width_1  ← 0
 ATTS.line_width_2  ← 2
 ATTS.line_width_3  ← 0
 ATTS.line_width_4  ← 2

 ATTS.point_size_2  ← 0
 ATTS.point_size_4  ← 0
∇
⍝ ====================================================================
∇ATTS←SPEEDUP_ATTS
 ⍝
 ⍝⍝ return attributes for ⎕PLOT (core count vs. speedup)
 ⍝
 ATTS.caption       ← 'Parallel Speedup vs. Core Count'
 ATTS.pw_pos_X      ← 200
 ATTS.pw_pos_Y      ← 200
 ATTS. format_X ← "%d\nCores"
 ATTS.legend_X      ← 50
 ATTS.legend_Y      ← 200
 ATTS.legend_name_1 ← "parallel speedup"
 ATTS.point_color_1 ← "#0000FF"
 ATTS.pa_border_B   ← 50
∇
⍝ ====================================================================
∇ONE_CORE_COUNT CORES;EXPR;DLEN;DATA;P_pit;P_sup;S_pit;S_sup;I;LEN;SUM;SUM2;Q;PZ;SZ
4 ⎕CR ∆FUNC
⍴∆FUNC
  EXPR←⊃((MON_EXPR⍪DYA_EXPR)[∆FUNC])
4 ⎕CR EXPR
  DATA←((⍴∆DLENS), 6)⍴0

  I←1 ◊ INIT_DATA 100 ◊ ⊣ ONE_PASS EXPR   ⍝ cache warm-up
Loop:
  DLEN←∆DLENS[I]
  INIT_DATA DLEN

  ⎕SYL[26;2] ← 0   ⍝ sequential
  (PZ LEN SUM SUM2) ← ONE_PASS EXPR
  DATA[I;1 2]←DLEN, SUM

  ⎕SYL[26;2] ← CORES   ⍝ parallel
  (SZ LEN SUM SUM2) ← ONE_PASS EXPR
  DATA[I;4 5]←DLEN, SUM
  →((⍴∆DLENS) ≥ I←I+1)⍴Loop

  ⎕SYL[26;2] ← 0   ⍝ back to sequential

   →(PZ≡SZ)⍴SAME_DATA
   '***FATAL: data mismatch'
   'PZ:' PZ
   'SZ:' SZ
   →0

SAME_DATA:
  Q←1,⍪DATA[;1]
  (S_sup S_pit)←⌊DATA[;2]⌹Q
  'Sequential: Startup' (0 0⍕S_sup) 'Per-Item:' (0 0⍕S_pit)
  (P_sup P_pit)←⌊DATA[;5]⌹Q
P_sup
  'Parallel:   Startup'(0 0⍕P_sup) 'Per-Item:' (0 0⍕P_pit)
  'Speedup:    ' (0 2⍕S_pit÷P_pit) ' for' CORES 'cores'
  SPEEDUPS←SPEEDUPS⍪ CORES, S_pit÷P_pit

  →(P_sup < S_sup)⍴GOON   ⍝ parallel setup is faster:    nonsense
  →(P_pit ≥ S_pit)⍴GOON   ⍝ parallel per item is slower: nonsense
  'Break-even: ' (⌈(P_sup - S_sup)÷(S_pit - P_pit))

GOON:
  DATA[;3]← S_sup + S_pit×DATA[;1]   ⍝ linearized line
  DATA[;6]← P_sup + P_pit×DATA[;1]   ⍝ linearized line

  DATA[;2 3 5 6] ← DATA[;2 3 5 6] × 0J1
  DATA[;2 3] ← DATA[;2 3] +[1] DATA[;1]
  DATA[;5 6] ← DATA[;5 6] +[1] DATA[;4]

⍝ D12←D12[1],D12                     ⍝ double unlinearized first point
⍝ D13←(0J1×S_sup),D13                ⍝ add X=0 
⍝ D45←D45[1],D45                     ⍝ double unlinearized first point
⍝ D46←(0J1×P_sup),D46                ⍝ add X=0 

   DATA←DATA[1;]⍪DATA                ⍝ replicate first plot point
   DATA[1;3] ← 0J1×S_sup             ⍝ drag plot line 3 to X=0
   DATA[1;6] ← 0J1×P_sup             ⍝ drag plot line 6 to X=0

⍝ DATA
  ⊣ PLOT_WIN_ATTS ⎕PLOT ⍉DATA[;2 3 5 6]
∇
⍝ ====================================================================
∇GO CORE_COUNTS
L: ONE_CORE_COUNT ↑CORE_COUNTS
   CORE_COUNTS←1↓CORE_COUNTS
   →(0<⍴CORE_COUNTS)⍴L
∇

⍝ ====================================================================
⍝ ====================================================================
decode_ARGS
GO ∆CORES

 'Speedups: ' SPEEDUPS
 ⊣ SPEEDUP_ATTS ⎕PLOT SPEEDUPS[;1] + 0J1×SPEEDUPS[;2]

)MORE
'Hit RETURN to close windows and exit'
⎕
⎕PLOT ¯3   ⍝ close plot windows
)FNS
)VARS
)OFF
