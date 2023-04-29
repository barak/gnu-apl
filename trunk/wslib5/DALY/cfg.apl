#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ config Routines to parse a configuration file
⍝ Copyright (C) 2018 Bill Daly

⍝ This program is free software: you can redistribute it and/or modify
⍝ it under the terms of the GNU General Public License as published by
⍝ the Free Software Foundation, either version 3 of the License, or
⍝ (at your option) any later version.

⍝ This program is distributed in the hope that it will be useful,
⍝ but WITHOUT ANY WARRANTY; without even the implied warranty of
⍝ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
⍝ GNU General Public License for more details.

⍝ You should have received a copy of the GNU General Public License
⍝ along with this program.  If not, see <http://www.gnu.org/licenses/>.

⍝ ********************************************************************

)copy_once 3 DALY/lex
)copy_once 5 DALY/utl
)copy_once 5 DALY/utf8

∇lx←delim cfg∆section∆helper txt;pos;key;values
⍝ Function recursively converts a configuration file to a nested
⍝ array of sections.
→(0=⍴txt)/ex
pos←'\[[^\]]*' ⎕re['↓'] txt
pos←1 ¯1 + pos
key← pos[2]↑pos[1]↓txt
txt←(+/1,pos)↓txt
pos←¯2↑((⍴txt),0),'\[[^\]]*' ⎕re['↓'] txt
values← delim cfg∆section∆parse pos[1]↑txt
lx←(delim cfg∆section∆helper pos[1]↓txt) lex∆assign key values
→0
ex:
lx←lex∆init
→0
∇

∇ lx←old_key cfg∆section∆find txt;pos;new_key;value;delim
  ⍝ Function returns a nested array of configuration file sections
  delim←'=:'[1+(+/'='=txt)<+/':'=txt]
  lx←delim cfg∆section∆helper txt
∇

∇sec←delim cfg∆section∆parse txt;t1;alist;bv;b2
  ⍝ Funtion to parse a section of a config file.
  bv←txt=⎕tc[3]
  txt←(b2←~⌽∧\⌽bv)/txt
  bv←b2/bv
  txt[bv/⍳⍴txt]←delim
  t1←delim utl∆split txt
  t1←(∊0≠⍴¨t1)/t1
  alist←cfg∆clean ¨ t1
  sec←lex∆from_alist alist
∇

∇dt←cmt cfg∆rm∆comments txt;pos
  ⍝ Function to remove comments from the text of a file
  ⍎(2≠⎕nc 'cmt')/'cmt←'';'''
  dt←''
st:
   →(0=⍴txt)/ed
  →(0=⍴pos←(cmt,'.*',⎕tc[3]) ⎕re['↓'] txt)/ed
  dt←(pos[1]↑txt),dt
  txt←(+/¯1,pos)↓txt
  →st
ed:
  dt←dt,txt
  →0
∇

∇t1←cfg∆rm∆cr txt
  ⍝ Function to remove carriage returns from DOS text.
t1←(∼(⎕tc[2]=txt)∧1⌽⎕tc[3]=txt)/txt
∇

∇t1←cfg∆clean txt;b;ix
  ⍝ Admended utl∆clean which also removes surrounding quotes.
  ix←(txt∊⎕tc,⎕av[10])/⍳⍴txt←,txt
  txt[ix]←' '
  txt←(~(1⌽b)∧b←txt=' ')/txt
   t1←(txt[1]=' ')↓(-txt[⍴txt]=' ')↓txt
  →(~(t1[1]=t1[⍴t1])∧t1[1]∊'"''')/0
  t1←1↓¯1↓t1
∇

∇lx←cfg∆parse_file name;t1
  ⍝ Function to parse a configuration file. Function returns a lexicon
  ⍝ of lexicons.
  t1←utf8∆read name
  t1←'#' cfg∆rm∆comments cfg∆rm∆cr t1
  lx←cfg∆section∆find t1
∇

∇Z←cfg⍙metadata
Z←0 2⍴⍬
Z←Z⍪'Author'          'Bill Daly'
Z←Z⍪'BugEmail'        'bugs@dalywebandedit'
Z←Z⍪'Documentation'   'apl-library.info'
Z←Z⍪'Download'        'https://sourceforge.net/projects/apl-library'
Z←Z⍪'License'         'GPL v3'
Z←Z⍪'Portability'     'L3'
Z←Z⍪'Provides'        ''
Z←Z⍪'Requires'        'utl lex utf8'
Z←Z⍪'Version'         '0 0 2'
∇
