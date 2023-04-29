#!/usr/local/bin/apl --script

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
⍝
⍝ xml 2016-08-31 09:29:08 (GMT-5)
⍝
⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝

⍝ xml, a workspace to create xml schmeas
⍝ Copyright (C) 2016  Bill Daly

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



)copy_once 3 DALY/lex

)copy_once 5 DALY/utl


∇t←xml∆whitespace
  t←⎕tc,⎕av[10 11 33]
∇

∇attr←key xml∆lex2attr value
  ⍝ Fn to prepare the text of an element's attribute from the key and value
  →(utl∆numberp value)/numeric
text:
  attr←(utl∆clean key),'="',(utl∆clean value),'"'
  →0
numeric:
  attr←(utl∆clean key),'="',(⍕''⍴value),'"'
  →0
∇

∇rs←prefix xml∆MkTagFns tag;txt;attr
  ⍝ Function to create a tag function
  txt←⊂'txt←attr ',prefix,'∆',tag,' val'
  txt←txt,⊂'⍝ Function returns a ',tag,' tag'
  txt←txt,⊂'⍎(2≠⎕nc ''attr'')/''attr←lex∆init'''
  txt←txt,⊂'txt←(attr xml∆start ''',tag,'''),val,xml∆end ''',tag,''''
  rs←⎕fx txt
∇

∇rs←prefix xml∆MkClosedTagFns tag;txt
  ⍝ Function to create a closed tag function
  txt←⊂'txt←',prefix,'∆',tag,' attr'
  txt←txt,⊂'⍝ Function returns a closed ',tag,' tag'
  txt←txt,⊂'txt←attr xml∆closed ''',tag,''''
  rs←⎕fx txt

∇
∇elm←attr xml∆closed tag
  ⍝ Function to create a closed element
  →(2≠⎕nc 'attr')/noAttr
  →(lex∆isempty attr)/noAttr
withAttr:
  elm←'<',tag,(∊ {' ',⍵ xml∆lex2attr attr lex∆lookup ⍵}¨lex∆keys attr),'/>'
  →0
noAttr:
  elm←'<',tag,'/>'
  →0
∇

∇elm←xml∆end tag
  ⍝ Function returnss an ending xml element tag
  elm←'</',tag,'>'
∇

∇elm←attr xml∆start tag
  ⍝ Function to assemble the start tag of an xml element.
  →(2≠⎕nc 'attr')/noAttr
  →(lex∆isempty attr)/noAttr
withAttr:
  elm←'<',tag,∊(' ',¨{⍵ xml∆lex2attr attr lex∆lookup ⍵}¨lex∆keys attr),'>'
  →0
noAttr:
  elm←'<',tag,'>'
  →0
∇

∇rule←xml∆mkRule l
  ⍝ Function to create a cascading style sheet rule from a lexicon
  rule←⎕av[11] utl∆join {(⍵,': '),(l lex∆lookup ⍵),';'}¨lex∆keys l
∇

∇sheet←xml∆mkSheet l
  ⍝ Functio to assemble a style sheet from a lexicon of selectors and rules.
  sheet←⎕av[11] utl∆join {⍵,' {',⎕av[11],(xml∆mkRule l lex∆lookup ⍵),⎕av[11],'}'}¨lex∆keys l
∇

∇lex←xml∆parseStylesheet txts;a;lb;i
⍝ Function to parse a cascading stylesheet and return a nested lexicon
txts←utl∆clean txts
lex←lex∆init
→(0=⍴⍴a←¯1↓ '{' utl∆split ¨ '}' utl∆split txts)/0
lb←((⍴a)⍴st),0
i←1
st:
lex←lex lex∆assign (⊂utl∆clean (i,1)⊃a),⊂xml∆parseRule (i,2)⊃a
→lb[i←i+1]
∇

∇rule←xml∆parseRule txta;lb;i
  ⍝ Function to parse the rule portion of a cascading style and return
  ⍝ a lexicon of properties and settings
  txta←(~txta∊xml∆whitespace)/txta
  rule←lex∆init
  txta←¯1↓ ':'utl∆split ¨ ';' utl∆split txta
  lb←((⍴txta)⍴st),0
  i←1
st:
  rule←rule lex∆assign i⊃txta
  →lb[i←i+1]
∇

∇Z←xml⍙metadata
  Z←0 2⍴⍬
  Z←Z⍪'Author'          'Bill Daly'
  Z←Z⍪'BugEmail'        'bugs@dalywebandedit.com'
  Z←Z⍪'Documentation'   'doc/apl-library.info'
  Z←Z⍪'Download'        'https://sourceforge.net/p/apl-library'
  Z←Z⍪'License'         'GPL v3.0'
  Z←Z⍪'Portability'     'L1'
  Z←Z⍪'Provides'        ''
  Z←Z⍪'Requires'        'utl lex'
  Z←Z⍪'Version'                  '0 1 5'
  Z←Z⍪'Last update'          '2021-02-14'
∇
