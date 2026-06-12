#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ stack.apl implement a stack in apl; You're just lucky I didn't name
⍝ them cons, car and cdr.
⍝ Copyright (C) 2019 Bill Daly

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

∇stack←stack∆init
  ⍝ Function sets up the stack
  stack←⍬
∇

∇stack←old stack∆push item
  stack←(⊂item),old
∇

∇item←stack∆peek stack
  item←1⊃stack
∇

∇stack←old stack∆poke item
  ⍝ Function to replace the top of the stack
  stack←old
  stack[1]←⊂item
∇


∇stack←stack∆pop old
  ⍎(0=⍴old)/'stack←old◊→0'
  stack←1↓old
∇

∇len←stack∆length stack
  len←⍴stack
∇

∇item←stack stack∆nth ix;l
  ⍝ Function returns the nth item on the stack
  utl∆es (ix > l←stack∆length stack)/'PEEKING PAST END OF STACK'
  item ←ix⊃stack
∇

∇Z←st⍙metadata
  Z←0 2⍴⍬
  Z←Z⍪'Author'          'Bill Daly'
  Z←Z⍪'BugEmail'        'bugs@DalyWebAndEdit.com'
  Z←Z⍪'Documentation'   'doc/apl-library.info'
  Z←Z⍪'Download'        'sourceforge.net/projects/apl-library/'
  Z←Z⍪'License'         'GPL v3'
  Z←Z⍪'Portability'     'L1'
  Z←Z⍪'Provides'        'st'
  Z←Z⍪'Requires'        ''
  Z←Z⍪'Version'         '0 0 1'
∇

