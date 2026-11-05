#! /usr/local/bin/apl --script

⍝ ********************************************************************
⍝ arg_parser_test.apl Script to test arg_parser
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

)load 1 arg_parser

⍞←⊃ABOUT ←⎕inp 'EOM'
This workspaces test the arg_parser workspace.  Function test_one sets
up a parser object that expects arguments for --first --second and
--third and will dispaly the values of those arguments on the command
line.
 
The command line must contain an argument '--'  which acts as a
delimter between the command line arguments passed to apl and those
passed to this script.  So a test run might be:
 
arg_parser_script -- --first=last --third=first

EOM

∇test_one;args;opts;keys
  args←ap∆init
  args←args ap∆new_option 'name' 'first' 'type' 'string' 'help' 'The first thing we must do'
  args←args ap∆new_option 'name' 'second' 'type' 'boolean' 'help' 'Do we do the second thing'
  args←args ap∆new_option 'name' 'third' 'type' 'string' 'help' 'The third thing to do.'
  args←ap∆parse args
try_errors:
  →(~ap∆has_errors args)/try_help
  ⍞←⎕tc[3] utl∆join ap∆get_errors args
  →0
try_help:
  →(~ap∆has_help args)/show_result
  ⍞←ap∆get_help args
  →0
show_result:
  opts←ap∆get_options args
  ⍞←⎕tc[3],test_result args
  ⍞←'Script name: ',ap∆get_name args
∇

∇msg←test_result args;opts;names;opt;ix;lb
  ⍝ Function displays option values
  opts←ap∆get_options args
  names←lex∆keys opts
  msg←⍬
  lb←((⍴names)⍴st),ed
  ix←1
st:
  →('error' utl∆stringEquals ix⊃names)/lp
  msg←msg, ((ix⊃names),⎕av[10],args ap∆get_option_value ix⊃names),⎕av[11]
lp:
  →lb[ix←ix+1]
ed:
  →0
∇

⍝ ∇args←test_two
⍝   args←ap∆init
⍝   args←args ap∆new_option 'name' 'first' 'type' 'string' 'help' 'The first thing we must do'
⍝   args←args ap∆new_option 'name' 'second' 'type' 'boolean' 'help' 'Do we do the second thing'
⍝   args←args ap∆new_option 'name' 'third' 'type' 'string' 'help' 'The third thing to do.'
⍝ ∇

test_one

)off

