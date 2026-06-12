#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ apl_kbd.apl Workspace to enable the windows key for apl characters.
⍝ Copyright (C) 2020 Bill Daly

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

)copy 5 FILE_IO
)copy 1 utl
)copy 1 lex
)copy 1 import
)copy 1 export

∇kbd_on
  ⍝ Function sets the 'Windows key' as the apl shift key.
  →(kbd_has_apl_dir)/real_work
  utl∆es(~1+ 511 FIO∆mkdir kbd_storage)/'ERROR CREATING ',kbd_storage
real_work:
  kbd_save_env kbd_storage,'/kbd_params.lex'
  kbd_set kbd_environment
∇

∇kbd_off
  ⍝ Function resets the 'Windows key'
  kbd_reset_env kbd_storage,'/kbd_params.lex'
∇

∇rs←kbd_environment;tmp
  ⍝ Function calls os command setxkbmap to obtain information about
  ⍝ keyboard setup. Function returns a lexicon
  utl∆es (~kbd_in_path 'setxkbmap')/'X.org command ''setxkbmap'' not available. Is Xorg running?'
  tmp←FIO∆pipefrom 'setxkbmap -query'
  rs←':' utl∆split ¨ ⎕tc[3] utl∆split tmp
  rs←⊃(∊2=⍴¨rs)/rs
∇

∇ kbd_set args;cmd;sink
  ⍝ Function to set keyboard for windows key to yeild apl
  ⍝ characters. Args is a lexicon with keys rules, model, layout and
  ⍝ variant.
  cmd←'setxkbmap -rules ',args lex∆lookup 'rules'
  cmd←cmd,' -model ',args lex∆lookup 'model'
  cmd←cmd,' -layout ',(args lex∆lookup 'layout'),',apl'
  cmd←cmd,' -variant apl'
  cmd←cmd,' -option grp:win_switch'
  sink←cmd,':',(2⍴⎕tc[3]), FIO∆pipefrom cmd
∇

∇ kbd_save_env fname;env
  ⍝ Function to save the current keyboard mapping
  env←kbd_environment
  env export∆array fname
∇

∇ kbd_reset_env fname;env;cmd;sink
  ⍝ Function resets the keyboard mapping from a tab--lineFeed
  ⍝ delimited file.
  env←import∆file fname
  cmd←'setxkbmap -rules ',env lex∆lookup 'rules'
  cmd←cmd,' -model ',env lex∆lookup 'model'
  cmd←cmd,' -layout ',(env lex∆lookup 'layout')
  sink←cmd,':',(2⍴⎕tc[3]), FIO∆pipefrom cmd
∇
  
∇b← kbd_in_path fnName;dirList;fileList;lb;ix
⍝ Function test whether fnName exists in the current PATH.
dirList←':' utl∆split 2⊃,⎕env 'PATH'
lb←((⍴dirList)⍴st),ed
ix←1
st:
→((0=1↑⍴fileList)∨0=⍴⍴fileList←(FIO∆read_directory ix⊃dirList))/tst
→(b←fileList[;5] utl∆member fnName)/ed
tst:
→lb[ix←ix+1]
ed:
→0
∇

∇h←kbd_home_dir
  ⍝ Function returns the users home directory.
  h←2⊃,⎕env 'HOME'
∇


∇b←kbd_has_apl_dir
  ⍝ Function tests for existance of .gnu-apl directory
  b←(FIO∆read_directory kbd_home_dir)[;5] utl∆member '.gnu-apl'
∇


kbd_storage←kbd_home_dir,'/.gnu-apl'
