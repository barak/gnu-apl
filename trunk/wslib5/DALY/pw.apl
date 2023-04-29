#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ password.apl APL workspace to generate random passwords.
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

∇txt←pw∆all
  ⍝ Function returns a vector of all characters. ie alpha,numeric,alpha
  txt←pw∆alpha,pw∆numeric,pw∆dingbats
∇

∇txt←pw∆alphanumeric
  ⍝ Function returns a vector of alpha numeric characters ie alpha,numeric
  txt←pw∆alpha,pw∆numeric
∇

∇p←l pw∆password set;⎕rl
  ⍝ Function randomly select l characters from set for a password
  ⎕rl←⎕fio[60] 8
  ⍎(2≠⎕nc 'l')/'l←8'
  p←set[l?⍴set]
∇

∇Z←pw⍙metadata
  Z←0 2⍴⍬
  Z←Z⍪'Author'          'Bill Daly'
  Z←Z⍪'BugEmail'        'bugs@dalywebandedit.com'
  Z←Z⍪'Documentation'   'Source code'
  Z←Z⍪'Download'        'sourceforge.net/projects/apl-library'
  Z←Z⍪'License'         'GPL v3.0'
  Z←Z⍪'Portability'     'L3'
  Z←Z⍪'Provides'        'pw'
  Z←Z⍪'Requires'        ''
  Z←Z⍪'Version'                  '0 1 0'
  Z←Z⍪'Last update'          '2020-06-01'
∇

pw∆alpha←'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'

pw∆dingbats←'~!@#$%^&*()-_=+><'

pw∆numeric←'0123456789' 
