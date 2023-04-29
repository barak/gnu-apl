#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ test_data.apl Workspace to find and supply test data
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
)copy_once 3 DALY/lex
)copy_once 5 DALY/utf8
)copy_once 5 DALY/pref
)copy_once 5 DALY/aparse
)copy_once 5 FILE_IO

∇dir←test_data
  ⍝ Function returns our best guess about where test data may be found.
  dir←test∆from_find
  →(dir test∆contains 'corrupt_shakespeare.txt')/0
  utl∆es 'test_data NOT FOUND.'
∇

∇b←dirname test∆contains fname;files
  ⍝ Function searches for fname (a file) in dirname, a directory
  →(utl∆numberp files←utf8∆dir dirname)/noDir
  b←fname utl∆stringMember files
  →0
noDir:
  b←0
  →0
∇

∇test_dir←test∆from_prefs;prefs;home;w;ws
  ⍝ Functions tries to find the test_data directory from preference file.
  prefs←pref∆current pref∆find_prefs
  w←⍴home←2⊃,⎕env'HOME'
  ws←(∊{∧/(3↑⍵)='LIB'}¨ws)/ws←lex∆keys prefs
  ws←(⊂prefs) lex∆lookup ¨ ws
  ws←(∊{∧/(w↑⍵)=home}¨ws)/ws
  test_dir←(∊{⍵ test∆contains 'test_data'}¨ws)/ws
  test_dir←{⍵,'/test_data'}¨test_dir
  test_dir←,⊃(∊{⍵ test∆contains 'corrupt_shakespeare.txt'}¨test_dir)/test_dir
∇

∇test_dir←test∆from_arg;pos;parser
  ⍝ Function tries to find the test_data directory from ⎕arg
  test_dir←⍬
  parser←ap∆init
  parser←parser ap∆new_option 'name' 'dir' 'type' 'string' 'help' 'Path to test files'
  parser← ap∆parse parser
  →(ap∆has_errors parser)/err
  →(ap∆has_help parser)/help
  test_dir←parser ap∆get_option_value 'dir'
  →(0=⍴test_dir)/no_dir
  →0
no_dir:
  →((⍴⎕arg)<pos←⎕arg utl∆listSearch '--script')/use_prefs
  test_dir←'/' utl∆split (pos+1)⊃⎕arg
  test_dir←¯1↓test_dir
  test_dir←'/' utl∆join test_dir,⊂'test_data'
  →0
use_prefs:
  test_dir←1⊃test∆from_prefs
  →0
err:
  'Try: find_test_data.apl -- --help'
  →0
help:
  ap∆help parser
  →0
∇

∇ test_dir←test∆from_find;sp;ls;incl;dirs
  ⍝ Function uses find
  dirs←FIO∆pipefrom 'find ',(2⊃,⎕env'HOME'),' -name test_data 2>&1 | grep -v "Permission denied"'
  dirs←'/' utl∆split ¨ ⎕tc[3] utl∆split dirs
  sp←∊⍴¨dirs
  ls←{⍵ utl∆listSearch 'library_tc'}¨dirs
  →(0=⍴dirs←(incl←ls≤sp)/dirs)/none
some:
  sp←incl/sp
  test_dir←'/' utl∆join 1⊃dirs[⍋sp]
  →0
none:
  test_dir←dirs
  →0
∇

∇version←test∆instance_version dir
  ⍝ Function returns the contents of Version.txt
  version←utf8∆read dir,'/Version.txt'
∇

∇Z←test⍙metadata
  Z←0 2⍴⍬
  Z←Z⍪'Author'          'Bill Daly'
  Z←Z⍪'BugEmail'        'bugs@dalywebandedit.com'
  Z←Z⍪'Documentation'   'doc/apl-library.info'
  Z←Z⍪'Download'        'https://sourceforge.net/p/apl-library'
  Z←Z⍪'License'         'GPL v3.0'
  Z←Z⍪'Portability'     'L3'
  Z←Z⍪'Provides'        'utl lex utf8 preferences arg_parser'
  Z←Z⍪'Requires'        ''
  Z←Z⍪'Version'                  '0 0 1'
  Z←Z⍪'Last update'          '2019-07-03'
∇


