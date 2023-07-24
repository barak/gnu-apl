#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ utf8 workspace to read and write utf8 files
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
)copy_once 5 DALY/utl 

∇err←txt utf8∆write fname;fh;size
⍝ Function to write utf8 files
  utl∆es (0>fh ←'w' ⎕FIO[3] fname)/fname,' NOT FOUND.'
  size←txt ⎕FIO[23] fh
  err←⎕FIO[4] fh
∇

∇txt←utf8∆convertBuffer buff;bv4;bv3;bv2;bv;ix4;ix3;ix2;ix;sink
  ⍝ Helper for utf8∆read.  Functions converts utf8 files to unicode.
  txt←buff
  bv4←txt>240
  bv3←(txt≤240)∧txt>224
  bv2←(txt≤224)∧txt>192
fourBytes:
  →(0=+/bv4)/threeBytes
  ix4←(bv4/⍳⍴txt)∘.+¯1+⍳4
  txt[ix4[;1]]←utf8∆fourBytes ¨ ⊂[2]txt[ix4]
  txt←(bv4←~(⍳⍴txt)∊,ix4[;2 3 4])/txt
  bv3←bv4/bv3
  bv2←bv4/bv2
threeBytes:
  →(0=+/bv3)/twoBytes
  ix3←(bv3/⍳⍴txt)∘.+¯1+⍳3
  txt[ix3[;1]]←utf8∆threeBytes ¨ ⊂[2]txt[ix3]
  txt←(bv3←~(⍳⍴txt)∊,ix3[;2 3])/txt
  bv2←bv3/bv2
twoBytes:
  →(0=+/bv2)/oneByte
  ix2←(bv2/⍳⍴txt)∘.+0 1
  txt[ix2[;1]]←utf8∆twoBytes ¨ ⊂[2]txt[ix2]
  txt←(~(⍳⍴txt)∊,ix2[;2])/txt
oneByte:
  txt←⎕ucs txt
∇  

∇txt← utf8∆read fname;fh;size;stat;sink;bv4;bv3;bv2;bv;ix4;ix3;ix2;ix;sink
 ⍝ Function to read utf8 files
 utl∆es (0>fh←'r' ⎕FIO[3] fname)/fname,' NOT FOUND.'
 stat←⎕FIO[18] fh
 txt← stat[8] ⎕FIO[6] fh
 sink←⎕FIO[4] fh
 ⎕es (0≠sink)/'Error closing ',fname
 bv4←txt>240
 bv3←(txt≤240)∧txt>224
 bv2←(txt≤224)∧txt>192
 fourBytes:
 →(0=+/bv4)/threeBytes
 ix4←(bv4/⍳⍴txt)∘.+¯1+⍳4
 txt[ix4[;1]]←utf8∆fourBytes ¨ ⊂[2]txt[ix4]
 txt←(bv4←~(⍳⍴txt)∊,ix4[;2 3 4])/txt
 bv3←bv4/bv3
 bv2←bv4/bv2
 threeBytes:
 →(0=+/bv3)/twoBytes
 ix3←(bv3/⍳⍴txt)∘.+¯1+⍳3
 txt[ix3[;1]]←utf8∆threeBytes ¨ ⊂[2]txt[ix3]
 txt←(bv3←~(⍳⍴txt)∊,ix3[;2 3])/txt
 bv2←bv3/bv2
 twoBytes:
 →(0=+/bv2)/oneByte
 ix2←(bv2/⍳⍴txt)∘.+0 1
 txt[ix2[;1]]←utf8∆twoBytes ¨ ⊂[2]txt[ix2]
 txt←(~(⍳⍴txt)∊,ix2[;2])/txt
 oneByte:
 txt←⎕ucs txt
∇

∇txt← utf8∆nested∆read fname
  ⍝ Function returns a nested array of all lines from a text file.
  txt←⎕tc[3] utl∆split utf8∆read fname
∇

∇c←utf8∆fourBytes bytes
  ⍝ Function converst four bytes into the unicaode character at that
  ⍝ code point
  c←32 64 64 64 ⊥ bytes ⊤∧ 31 63 63 63
∇

∇c←utf8∆threeBytes bytes
 ⍝ Function converts three bytes into the unicode character at that
 ⍝ code point.
 c←32 64 64 ⊥ bytes ⊤∧ 31 63 63
∇

∇c←utf8∆twoBytes bytes
 ⍝ Function converts three bytes into the unicode character at that
 ⍝ code point.
 c←32 64 ⊥ bytes ⊤∧ 31 63
∇

∇flist←utf8∆dir path
  ⍝ Function lists files in path (but not . or ..).
  flist ← ⎕FIO[29] path
∇

∇b←utf8∆fileExists fullpath;files;pos;fname;parts
  ⍝ Function tests the existance of a file.
  parts←utl∆fileName∆parse fullpath
  fname←'.' utl∆join 1↓parts
  files← utf8∆dir 1⊃parts
  pos←files utl∆listSearch fname
  b←pos≤⍴files
∇

∇ varName utf8∆saveVar fname;txt
  ⍝ Function saves a workspace variable as a script file to be copied
  ⍝ into other workspaces.
  txt← ⎕tc[3] utl∆join (⊂'#! /usr/local/bin/apl'),10 ⎕cr varName
  txt utf8∆write fname
∇

∇Z←utf8⍙metadata
  Z←0 2⍴⍬
  Z←Z⍪'Author'          'Bill Daly'
  Z←Z⍪'BugEmail'        'bugs@dalywebandedit.com'
  Z←Z⍪'Documentation'   'info apl-library'
  Z←Z⍪'Download'        'https://sourceforge.net/projects/apl-library'
  Z←Z⍪'License'         'GPL v3.0'
  Z←Z⍪'Portability'     'L3'
  Z←Z⍪'Provides'        'Functions to read and write utf8 files.'
  Z←Z⍪'Requires'        'utl'
  Z←Z⍪'File name'	'utf8.apl'
  Z←Z⍪'Location'	'5 DALY'
  Z←Z⍪'Version'         '1 3 0'
  Z←Z⍪'Last update'     '2023 07 15'
∇

