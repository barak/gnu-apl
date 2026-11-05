#!./apl --script

   ⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
 ⍝                                                                  ⍝
⍝ FORMAT_FUZZ                          2026-10-02  18:30:00 (GMT+2)  ⍝
⍝                                                                    ⍝
 ⍝                                                                  ⍝
   ⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝

   ⍝ Produce stimuli for comparing the APL output (display) of GNU APL with
   ⍝ IBM APL2 (via the MEGA11 harness in testcases_apl2). The output (on
   ⍝ stdout) is ONE large testcase file with one test per random value:
   ⍝
   ⍝      ⍝ N=17   L=54         N: test number, L: line of this comment
   ⍝      VAL←...               2 ⎕TF 'VAL': the stimulus (rebuilds VAL)
   ⍝      VAL                   the APL output to be compared
   ⍝ ...                        the expected (GNU APL) output of VAL
   ⍝                            (∆BLANKS empty lines)
   ⍝
   ⍝ Usage (from src):
   ⍝
   ⍝      ./apl --script -f testcases/libroot/workspaces/FORMAT_FUZZ.apl > testcases_fuzz/FORMAT_FUZZ.tc
   ⍝
   ⍝ Small values only: larger shapes or deeper nesting give no new insight
   ⍝ into the rules of APL output, but make homogeneous values unlikely.

      ∆COUNT←300                    ⍝ number of tests
      ∆BLANKS←3                     ⍝ empty lines after each test: an unexpected
                                    ⍝ extra output line must not consume the next
                                    ⍝ test's input lines

      ⍝ random seed: ∆SEED←⍬ gives a new seed (from ⎕TS) on every run. The
      ⍝ seed used is written into the header of the output, so that a file
      ⍝ can be reproduced by setting ∆SEED to it (8 integers 0..255). ⎕RL is
      ⍝ derived from the same seed.
      ⍝
      ∆SEED←⍬
      ∆SEED←∆SEED,(8×0=⍴∆SEED)⍴256|8⍴⌽⎕TS   ⍝ from ⎕TS if ∆SEED is empty
      ∆←0 ⎕RVAL ∆SEED
      ⎕RL←1+256⊥4↑∆SEED
      ∆←8 ⎕RVAL 1                   ⍝ APL2 mode: 32-bit integers and floats,
                                    ⍝ characters from IBM APL2's ⎕AV

      ⍝ The ⎕RVAL properties are given per test in ONE_ARRAY (monadic ⎕RVAL B):
      ⍝   rank 0 / 1 / 2 / 3:  10 30 45 15
      ⍝   every axis:          length 0..3
      ⍝   types:               chosen per test (see ONE_ARRAY)
      ⍝   max. depth 2, max. 9 items per level

      ⍝ Characters come from IBM APL2's ⎕AV (8 ⎕RVAL 1), except for those
      ⍝ in ∆BADCHARS: the control characters and DEL (break a .tc file),
      ⍝ ⍬ ⍶ ⍹ ⋸ (not in IBM APL2's real ⎕AV, although GNU APL's table of it,
      ⍝ which apl2_codec.py copies, has them), and | (rejected by the APL2
      ⍝ harness, testcases_apl2/extract_stmts.py). In random values they
      ⍝ are replaced by a character from ∆CHARS, which exist in IBM APL2 and
      ⍝ are mapped unambiguously by apl2_codec.py; in test N=0 by blanks.
      ⍝
      ∆BADCHARS←(⎕UCS ¯1+⍳32),(⎕UCS 127),'|⍬⍶⍹⋸'

      ⍝ IBM APL2's ⎕AV (in ⎕AV order; the same table as Avec::IBM_quad_AV()).
      ⍝ Test N=0 displays it as a 16×16 matrix (∆BADCHARS as blanks), so
      ⍝ that character issues (codec, display, harness) show up immediately.
      ⍝
      ∆AV2←⎕UCS 0 1 2 3 4 5 6 7 8 9 10 9068 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 116 117 118 119 120 121 122 123 124 125 126 127 199 252 233 226 228 224 229 231 234 235 232 239 238 236 196 197 9109 9054 9017 244 246 242 251 249 8868 214 220 248 163 8869 9078 9014 225 237 243 250 241 209 170 186 191 8968 172 189 8746 161 9045 9038 9617 9618 9619 9474 9508 9055 8710 8711 8594 9571 9553 9559 9565 8592 8970 9488 9492 9524 9516 9500 9472 9532 8593 8595 9562 9556 9577 9574 9568 9552 9580 8801 9080 8952 8757 9015 9026 9019 8866 8867 9674 9496 9484 9608 9604 166 204 9600 9082 9081 8834 8835 9053 9074 9076 9073 9021 8854 9675 8744 9075 9033 1013 8745 9023 9024 8805 8804 8800 215 247 9049 8728 9077 9067 9035 9042 175 168 160
      ∆AV2←16 16⍴(~∆AV2∊∆BADCHARS)\(~∆AV2∊∆BADCHARS)/∆AV2
      ∆CHARS←'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'
      ∆CHARS←∆CHARS,' .,;:()[]+-×÷=<>≠≤≥/\!?⍳⍴∆_¯'

∇Z←FIX V
 ⍝⍝ map the raw ⎕RVAL value V to the value classes wanted for APL output:
 ⍝⍝ characters from APL2's ⎕AV (∆BADCHARS replaced from ∆CHARS), integers
 ⍝⍝ with 1..9 digits (|Z| < 2⋆31), reals
 ⍝⍝ with a random sign and magnitude 1E¯10..1E8, complex likewise per part.
 Z←FIX1¨V
∇

∇Z←FIX1 X
 →(0≠≡X)/NESTED
 →(' '=↑0⍴X)/CHAR
 →(X≠+X)/CPLX                   ⍝ X differs from its conjugate: complex
 →(X=⌊X)/INT
 Z←FIXR X ◊ →0
INT:  Z←(×X)×⌊(10⋆?9)||X ◊ →0     ⍝ 1..9 digits, i.e. |Z| < 2⋆31
CHAR: Z←X ◊ →(~X∊∆BADCHARS)/0
      Z←∆CHARS[⎕IO+(↑⍴∆CHARS)|⎕UCS X] ◊ →0
CPLX: Z←(FIXR 9○X)+0J1×FIXR 11○X ◊ →0
NESTED: Z←⊂FIX X
∇

∇Z←FIXR X;D
 ⍝⍝ X is a random real in (0,1): keep D significant digits (mostly few, so
 ⍝⍝ that short numbers like 2.5 occur), random sign, magnitude 1E¯10..1E8.
 ⍝⍝ (Not larger: 2 ⎕TF writes an integral float like 3E9 as 3000000000,
 ⍝⍝ which GNU APL reads back as a 64-bit integer but IBM APL2 as a float.)
 D←(1 1 2 2 3 3 4 5 6 10 17)[?11]
 Z←(¯1+2×?2)×(⌊0.5+X×10⋆D)×10⋆(?19)-11+D
∇

∇Z←ONE_ARRAY N;VAL;ENC;TYPES
 ⍝⍝ one test (N is the test number): comment, stimulus, output statement
 ⍝⍝
 ⍝⍝ The type distribution is chosen per test: 60% homogeneous (one type for
 ⍝⍝ the entire value), 20% mixed per item, 20% nested. An empty value is
 ⍝⍝ kept with a probability of 15% only. Test N=0 is IBM APL2's ⎕AV.
 →(N≠0)/AGAIN
 VAL←∆AV2 ◊ →ENCODE
AGAIN: TYPES←?100
 →(TYPES>60)/MIXED
 TYPES←(⍳5)=?4 ◊ →DRAW               ⍝ one of char/int/real/complex
MIXED: →(TYPES>80)/NEST
 TYPES←25 30 25 10 0 ◊ →DRAW
NEST: TYPES←20 25 20 10 25
DRAW: VAL←FIX ⎕RVAL (10 30 45 15) (¯3 ¯3 ¯3) TYPES 2 9
 →((0=×/⍴VAL)∧85≥?100)/AGAIN
 →(INDENTED VAL)/AGAIN               ⍝ output that the runner takes as input
ENCODE: ENC←2 ⎕TF 'VAL'
 ⎕PW←10000                           ⍝ do not fold the input lines
 ⎕←'      ⍝ N=',(⍕N),'   L=',⍕∆LINE
 ⎕←'      ',ENC
 ⎕←'      VAL'
 ⍎ENC                                ⍝ VAL exactly as the testcase has it
 ⎕PW←80 ◊ ⎕←VAL                      ⍝ the expected output (default ⎕PW)
 ∆←{⎕←''⊣⍵}¨⍳∆BLANKS
 ∆LINE←∆LINE+3+∆BLANKS+NLINES VAL
 Z←N
∇

∇Z←INDENTED V;F
 ⍝⍝ 1 if a (non-blank) line of the display of V starts with 6 or more
 ⍝⍝ blanks: the testcase runner would execute it as an input line instead
 ⍝⍝ of comparing it (Blake McBride, Bugs34 #48). ⍕ is GNU APL's display,
 ⍝⍝ which can differ from IBM APL2's, so this catches most, not all cases.
 F←⍕V ◊ Z←0
 →(1≥⍴⍴F)/VEC
 F←((×/¯1↓⍴F),¯1↑⍴F)⍴F ◊ →CHK
VEC: F←(1,⍴F)⍴F
CHK: →(6>¯1↑⍴F)/0
 Z←∨/(∧/' '=F[;⍳6])∧∨/' '≠F
∇

∇Z←NLINES V;F;S;P
 ⍝⍝ the number of lines that the display of V takes (as in IBM APL2): 1 for
 ⍝⍝ a scalar or vector (also for an empty vector: an empty line); otherwise
 ⍝⍝ every row of ⍕V (also a row of width 0) plus the blank lines between
 ⍝⍝ planes: j blank lines between two items along the j-th plane axis
 ⍝⍝ (counted from the rows), but none if the width is 0.
 F←⍕V ◊ S←⍴F
 →(2≤⍴S)/MAT
 Z←1 ◊ →0
MAT: Z←×/¯1↓S
 →(0=¯1↑S)/0
 P←¯2↓S
 Z←Z++/(∧\P≠0)×(×\1,¯1↓P)×(P-1)×⌽⍳⍴P
∇

      ⎕PP←5     ⍝ also for the generator: long fractional digits are of
                ⍝ little use for the layout rules (2 ⎕TF ignores ⎕PP)
      ∆HEAD←'⍝ FORMAT_FUZZ.tc' '⍝ ----------------------------------' ''
      ∆HEAD←∆HEAD,'⍝ generated by src/testcases/libroot/workspaces/FORMAT_FUZZ.apl with' ''
      ∆HEAD←∆HEAD,('⍝    ∆SEED←',⍕∆SEED) ''
      ∆HEAD←∆HEAD,'      ⎕PP←5' ''
      ∆←{⎕←⍵}¨∆HEAD
      ∆LINE←1+⍴∆HEAD
      ∆←ONE_ARRAY¨0,⍳∆COUNT          ⍝ N=0: IBM APL2's ⎕AV, then random
      ∆←⎕←'⍝ =================================='

      )OFF
