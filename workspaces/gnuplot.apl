#!apl --script

⍝   This file is part of GNU APL, a free implementation of the
⍝   ISO/IEC Standard 13751, "Programming Language APL, Extended"
⍝
⍝   Copyright (C) 2008-2017  Dr. Jürgen Sauermann
⍝
⍝   This program is free software: you can redistribute it and/or modify
⍝   it under the terms of the GNU General Public License as published by
⍝   the Free Software Foundation, either version 3 of the License, or
⍝   (at your option) any later version.
⍝
⍝   This program is distributed in the hope that it will be useful,
⍝   but WITHOUT ANY WARRANTY; without even the implied warranty of
⍝   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
⍝   GNU General Public License for more details.
⍝
⍝   You should have received a copy of the GNU General Public License
⍝   along with this program.  If not, see <http://www.gnu.org/licenses/>.


⍝⍝ This is an APL workspace which is part of GNU APL. It demonstrates how
⍝⍝ the gnuplot program (see www.gnuplot.org) can be used from GNU APL for
⍝⍝ plotting numerical data. As an example it plots the result of ⎕FFT.

⍝ set ∆DATA to a matrix of data points that shall be gnu-plotted. Every
⍝ row of ∆DATA will be one plot-line. Formatting of the different lines
⍝ (color, legend, etc.) can be controlled via ∆Titles and ∆With
⍝

SINES  ← { (,⊃⍺/⊂⍵),↑⍵←1○0,⍵÷⍨○2×⍳⍵-1 }   ⍝ ⍺ full sine waves sampled ⍵ times
PULSE  ← { (⍺+⍵)↑((⍺÷2),⍵)/0 1 }
SQUARE ← { (⌊⍵÷2) ⍵ (⌊⍵÷2)/1 ¯1 1 }
CONST1 ← { ⍵⍴1 }

∇Z←A SINES_N B;M
⍝⍝ add some sines of different sample rates (= 1/frequencies) B,
⍝⍝ repeat A times and symmetrize
 M←(∧/B)÷B         ⍝ multiplier for each sample rate
 Z←+⌿⊃M SINES¨ B   ⍝ sine waves for each sample rate
 Z←,⊃A/⊂Z          ⍝ repeat A times
 Z←Z,⌽Z            ⍝ make it symmetric (for a real ⎕FFT result)
∇

∇Z←∆MODE FFT_data Samples
⍝⍝ construct data matrix Z for plotting Samples and their FFT
⍝
⍝ The matrix has 4 rows:
⍝ Z[1] : X axis (0, 1, 2, ... N)
⍝ Z[2] : Samples
⍝ Z[3] : Samples × window
⍝ Z[4] : the real part of the FFT of the samples
⍝ Z[5] : the imag part of the FFT of the samples

Z←((1,⍴Samples)⍴(-⎕IO-⍳⍴Samples))   ⍝ X-axis
Z←Z ⍪ ⍉⍪Samples                     ⍝ Samples
Z←Z ⍪ ⍉⍪Samples                     ⍝ Samples × window (fallback)
→1+(∆MODE∈¯1 1)⍴⎕LC ◊ Z[3;]←(-∆MODE)⎕FFT Samples   ⍝ Samples × window
Z←Z ⍪  9○∆MODE ⎕FFT Samples         ⍝ FFT result (real)
Z←Z ⍪ 11○∆MODE ⎕FFT Samples         ⍝ FFT result (imag)
∇

∇ATTRS plot DATA;REPL;H;NX;NY;Z;Cmd;Titles;Using;With;FMT
 (Titles With)←ATTRS ◊ Titles←⊃Titles ◊ With←⊃With
 (NY NX)←⍴DATA
 Z←,⊃{⍺,"\n", ⍵}/ "
    set encoding utf8
    set grid xtics ytics
    set xlabel \"Samples/Frequency\"
    set ylabel \"Amplitude\"
                  "
 Cmd←"  plot" ◊ REPL←1
LOOP: Using←"using 1:",⍕REPL+1
 Z←Z, Cmd, " \"/tmp/plot_data\" ", Using
 Z←Z , " title \"", (⊃Titles[REPL]), "\" with lines " , (⊃With[REPL]), "\n"
 REPL←REPL+1 ◊ Cmd←"replot" ◊ →(REPL<NY)/LOOP
 Z←⎕UCS Z, "pause -1\n"

  H←"w"  ⎕FIO[3] "/tmp/plot_commands"   ⍝ open /tmp/plot_commands for writing
 ⊣ Z ⎕FIO[7] H                          ⍝ write plot settings and commands
 ⊣ (⎕FIO[4] H)                          ⍝ close /tmp/plot_commands

 FMT←⊂"%3d", (,(NY-1)⌿⍉⍪", %.2f"),"\n"
 H←"w" ⎕FIO[3]  "/tmp/plot_data"        ⍝ open /tmp/plot_data for writing
 ⊣ { (FMT, ⍵) ⎕FIO[22] H}¨⊂[1]DATA      ⍝ write plot data
 ⊣(⎕FIO[4] H)                           ⍝ close /tmp/plot_data
∇

∇Z←choose_data;L
⊃"Enter:
\   1 for FFT of 10 SINES (Hann window)
\   2 for FFT PULSE
\   3 for FFT PULSE (Hann window)
\   4 for FFT SQUARE 51
\   5 for FFT constant 1 with (Hann window)
\   6 for FFT 3 sines of different frequencies"
L←L1 L2 L3 L4 L5 L6 ◊ ⍞←'CHOOSE: ' ◊ →('123456'=↑8↓⍞)/L ◊ →¯1↑L
L1: Z←10, ⊂10 SINES 32  ◊ →0        ⍝ 10 sines, 32 samples/sine, Hamm window
L2: Z← 0, ⊂40 PULSE 61  ◊ →0        ⍝ a pulse, no window
L3: Z←10, ⊂40 PULSE 61  ◊ →0        ⍝ a pulse, Hamm window
L4: Z← 0, ⊂SQUARE  51   ◊ →0        ⍝ a square wave, no window
L5: Z←10, ⊂CONST1 101   ◊ →0        ⍝ a constant, Hamm window
L6: Z← 0, ⊂2 SINES_N 5×3 4 5 ◊ →0   ⍝ 3 sines then symmetrized, no window
∇

(∆MODE ∆DATA)←choose_data

∆Titles ← ⊂("Raw Samples") ("Samples x Window")("FFT-Real") ("FFT-Imag")
∆With   ← ⊂("lt 2 lw 2")   ("lt 2 lw 1")       ("lt 1")     ("lt 3")

(∆Titles ∆With) plot ∆MODE FFT_data ∆DATA
)HOST gnuplot /tmp/plot_commands

)VARS
⍝ )OFF

