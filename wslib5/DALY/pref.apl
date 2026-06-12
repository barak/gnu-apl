#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ preferences.apl Workspace to lookup currently active preferences
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
)copy_once 5 FILE_IO

∇prefs←pref∆parse fname;parsed;keys;values;klen
  ⍝ Function returns a lexicon for the preferences in fname
  parsed←utf8∆read fname
  parsed←⎕tc[3] utl∆split parsed
  parsed←((∊{0≠⍴⍴⍵}¨parsed)∧∊{0≠⍴⍵}¨parsed)/parsed
  parsed←(∊{'#'≠⍵[1]}¨parsed)/parsed
  parsed←utl∆clean¨parsed
  klen←{+/∧\⍵≠' '}¨parsed
  keys←klen ↑¨parsed
  values←(1+klen)↓¨parsed
  prefs←lex∆from_alist ,keys,[1.1]values
∇

∇paths←pref∆find_prefs;etc;home
  ⍝ Function returns a list of apl preference files.
  etc←FIO∆pipefrom 'apl --show_etc_dir'
  etc←(etc≠⎕tc[3])/etc
  home←2⊃,⎕env'HOME'
  paths←⍬
tryEtc:
  →(~'gnu-apl.d' utl∆stringMember utf8∆dir etc)/tryHome
  →(~'preferences' utl∆stringMember utf8∆dir etc,'/gnu-apl.d')/tryHome
  paths←paths,⊂etc,'/gnu-apl.d/preferences'
tryHome:
  →(~'.gnu-apl' utl∆stringMember utf8∆dir home)/dotConfig
  →(~'preferences' utl∆stringMember utf8∆dir home,'/.gnu-apl')/dotConfig
  paths←paths,⊂home,'/.gnu-apl/preferences'
dotConfig:
  →(~'gnu-apl' utl∆stringMember utf8∆dir home,'/.config')/0
  →(~'preferences' utl∆stringMember utf8∆dir home,'/.config/gnu-apl')/0
  paths←paths,⊂home,'/.config/gnu-apl/preferences'
∇

∇prefs←pref∆current paths;tmp;lb;ix;keys
  ⍝ Function assembles a lexicon of preferences from a list of
  ⍝ preferences files. Files lower in the list will replace
  ⍝ name--value pairs in files higher on the list.
  prefs←pref∆parse 1⊃paths
  →(1=⍴paths)/0
  tmp←pref∆current 1↓paths
  keys←lex∆keys tmp
  lb←((⍴keys)⍴st),0
  ix←1
st:
  prefs←prefs lex∆assign keys[ix],⊂tmp lex∆lookup ix⊃keys
  →lb[ix←ix+1]  
∇

  
∇Z←pref⍙metadata
  Z←0 2⍴⍬
  Z←Z⍪'Author'          'Bill Daly'
  Z←Z⍪'BugEmail'        'bugs@dalywebandedit.com'
  Z←Z⍪'Documentation'   'doc/apl-library.info'
  Z←Z⍪'Download'        'https://sourceforge.net/p/apl-library/code/ci/master/tree/preferences.apl'
  Z←Z⍪'License'         'GPL v3.0'
  Z←Z⍪'Portability'     'L3'
  Z←Z⍪'Provides'        ''
  Z←Z⍪'Requires'        'utl,lex,utf8'
  Z←Z⍪'Version'                  '0 0 1'
  Z←Z⍪'Last update'          '2019-06-30'
∇
  
