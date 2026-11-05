#! /usr/local/bin/apl --script
# vim: et:ts=4:sw=4

  ⍝ tunable parameters for this benchmark program
  ⍝
  ⍙PROFILE←100 200 500  ⍝ fractions of Integer, Real, and Complex numbers

⊣⍎')COPY 5 FILE_IO.apl FIO∆set_monadic_threshold FIO∆set_dyadic_threshold'
⊣⍎')COPY 5 FILE_IO.apl FIO∆clear_statistics FIO∆get_statistics'

⍝ -----------------------------------------------------------------------------
∇SID MAKE_BENCH EXPR;TEXT
TEXT←"""
Z←BENCH L;I;STAT
I←1 ◊ Z←⍬
   TEXT[3]
LOOP:
INIT_DATA I × L
FIO∆clear_statistics
   TEXT[7]
STAT←FIO∆get_statistics
Z←Z,STAT[4]
LOOP → 20≥I←I+1
"""

TEXT[3]←⊂'INIT_DATA 100 ◊ ', EXPR, '   ⍝ cache warm-up'
TEXT[6]←⊂(⊃TEXT[6]), ' ', ⍕SID
TEXT[7]←⊂EXPR
TEXT[8]←⊂(⊃TEXT[8]), ' ', ⍕SID
⊣⎕FX TEXT
∇

⍝ -----------------------------------------------------------------------------
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
⍝ -----------------------------------------------------------------------------
∇Z←EXPR PLOT_WIN_ATTS CORES;CAPTION;Wpos_X;Wpos_Y
 ⍝⍝ return useful window attributes for ⎕PLOT

  CAPTION←⊂'caption: Z←', (⊃,/EXPR[1 2 3]), ' on ', (⍕CORES), ' cores'
  Wpos_X←⊂'pw_pos_X: ', ⍕200+30×CORES
  Wpos_Y←⊂'pw_pos_Y: ', ⍕200+30×CORES
  Z←"""
pa_border_L: 60
legend_X:    80
legend_Y:    45
legend_name-1: measured cycles sequential
legend_name-2: linearized cycles sequential
legend_name-3: measured cycles parallel
legend_name-4: linearized cycles parallel
point_color-1: #00C000
 line_color-1: #00C000
point_color-2: #00C000
 line_color-2: #00C000
point_color-3: #C00000
 line_color-3: #C00000
point_color-4: #C00000
line_color-4:  #C00000

line_width-1:  0
line_width-2:  2
line_width-3:  0
line_width-4:  2

point_size-2:  0
point_size-4:  0
      """
  Z←CAPTION,Wpos_X,Wpos_Y,Z
∇

⍝ -----------------------------------------------------------------------------
∇Z←SETUP GTK;Event;A;FUN;B;CORES;LEN;DYA;SID;EXPR;ATTS

 Event←⎕GTK 1 ⍝ ◊ Event   ⍝ Event is: name function id class
 CLICKED → Event ≡ 7 'wn_button1' 'clicked' 'button1' 'GtkButton'
 CLOSED  → Event ≡ 7 'wn_window1' 'destroy' 'window1' 'GtkWindow'

 ⍝ Event was unexpected. print it and add a match above
 ⍝
 4 ⎕CR Event
 4 ⎕CR 7 'destroy' ''  'window1' 'GtkWindow'
 ???FIXME???

CLOSED:   ⍝ the user has closed the window: return ⍬
 →⍴Z←⍬

CLICKED:   ⍝ the user clicked on the Measure ! button
 A    ← 1↓⎕GTK[GTK, 'combo_box_text1'] "get_active_text"
 FUN  ← 1↓⎕GTK[GTK, 'combo_box_text2'] "get_active_text"
 B    ← 1↓⎕GTK[GTK, 'combo_box_text3'] "get_active_text"
 CORES← 1↓⎕GTK[GTK, 'combo_box_text4'] "get_active_text"
 LEN  ← 1↓⎕GTK[GTK, 'spin_button1'   ] "get_value"
 CORES ←  ⍎2↑CORES
 LEN←2⋆⍎2↑LEN ◊ 'VECTOR LENGTH:' LEN
 DYA←A≢'-monadic-'
 A←(DYA×⍴A)⍴A
 SID←41 + DYA
 4 ⎕CR A FUN B CORES SID LEN
 SID MAKE_BENCH '⊣', A, FUN, B   ⍝ e.g. '⊣A+B'
 '' ◊ ⎕CR 'BENCH' ◊ ''
 ⊣ 1 FIO∆set_monadic_threshold FUN
 ⊣ 1 FIO∆set_dyadic_threshold  FUN

 EXPR←A, FUN, B
 ATTS←EXPR PLOT_WIN_ATTS CORES
 Z←(ATTS EXPR LEN CORES SID)
∇

⍝ -----------------------------------------------------------------------------
∇Z←STEP PLOT_LINES DATA;X;Q;Setup;Per_item;L1;L2
 ⍝⍝ for vector DATA, return (setup, per-item, L1, L2) where:
 ⍝  L1 = plot lines for DATA
 ⍝  L2 = linearized DATA (least-square approximation)
 ⍝
 X←STEP×⍳⍴DATA
 Q←1,⍪X
 (Setup Per_item)←DATA⌹Q
 L1←X+0J1×DATA
 L2←X+0J1×Setup+Per_item×X

  L2←(0J1×Setup),L2   ⍝ Add X=0 plot point

 Z←Setup Per_item  L1 L2
∇
⍝ -----------------------------------------------------------------------------
∇GTK←SETUP_GTK;GUI_file;CSS_file
 GUI_file ← 'workspaces/Scalar3.ui'
 CSS_file ← 'workspaces/Scalar3.css'
 GTK ← CSS_file ⎕GTK GUI_file   ⍝ fork Gtk_server and connect to it
⍝ ⊣GTK ⎕GTK 3   ⍝ ++verbosity in Gtk_server
⍝ ⊣GTK ⎕GTK 3   ⍝ ++verbosity in Gtk_server
∇
⍝ -----------------------------------------------------------------------------
∇MAIN;GTK;PARAMS;SEQ;PAR;SEQ_sup;SEQ_pi;PAR_sup;PAR_pi;L1;L2;L3;L4

GTK←SETUP_GTK
LOOP: PARAMS←SETUP GTK ◊ DOIT → 0 < ⍴PARAMS
   ' DONE. '
   ⎕PLOT ¯3   ⍝ close PLOT windows
   →0

DOIT:
 ⎕SYL[26;2] ← 0         ◊ SEQ←BENCH PARAMS[5]   ⍝ sequential
 ⎕SYL[26;2] ← PARAMS[4] ◊ PAR←BENCH PARAMS[5]   ⍝ parallel
 ⎕SYL[26;2] ← 0

 (SEQ_sup SEQ_pi  L1 L2)←PARAMS[3] PLOT_LINES SEQ
 'Seq: setup' SEQ_sup 'per item:' SEQ_pi

 (PAR_sup PAR_pi L3 L4)←PARAMS[3] PLOT_LINES PAR
 'Par: setup' PAR_sup 'per item:' PAR_pi

 ⊣(⊃PARAMS[1]) ⎕PLOT L1 L2 L3 L4

 LOOP → PAR_pi = 0
 'Speedup:' (SEQ_pi÷PAR_pi) 'on' PARAMS[4] 'cores for' (⊃PARAMS[2])
 →LOOP
∇

MAIN
)MORE
)VARS
)SI
)OFF

