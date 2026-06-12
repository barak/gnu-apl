#!/usr/local/bin/apl script --script
⍝ ********************************************************************
⍝ wsinit.apl; Workspace to set the current working directory to a home
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

)copy_once 5 FILE_IO
)copy_once 5 DALY/utl

∇b←wsinit∆athome testdir
   ⍝ Function to test if the current working directory is home
   b←testdir utl∆stringEquals FIO∆getcwd
∇

∇wsinit∆gohome newdir
   ⍝ Function to reset the current working dir, presumably to home
   utl∆es (0<FIO∆chdir newdir)/'ERROR SETING WORKING DIRECTORY TO ',newdir
∇

∇wsinit∆lx
   ⍝ Function for ⎕lx
   →(wsinit∆athome wsinit∆home)/0
   wsinit∆gohome wsinit∆home
∇


wsinit∆home←2⊃(⎕env 'HOME')[1;]

wsinit∆lx
