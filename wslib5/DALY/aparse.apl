#! apl --script
⍝ ********************************************************************
⍝ <one line to give the program's name and a brief idea of what it does.>
⍝ Copyright (C) <year>  <name of author>

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

⍝                        Options
⍝ ********************************************************************
⍝ Options are a lexicon of name, value, type, and help.

)copy_once 5 DALY/utl
)copy_once 3 DALY/lex

∇ opt←aparse∆opt∆new arg
  ⍝ Function returns a new option named 1⊃arg of type 2⊃arg with,
  ⍝ optionally help 3⊃arg
  opt←lex∆init
  opt←opt lex∆assign (⊂'name'),⊂ 1⊃arg
  ⍎(∧/'type'=4↑2⊃arg)/'opt←opt lex∆assign ''value'' 0'
  opt←opt lex∆assign (⊂'type'),⊂ 2⊃arg
  opt←opt lex∆assign (⊂'help'),⊂ 3⊃3↑arg
∇

⍝		       Option accessor methods

∇b←aparse∆opt∆is option
  ⍝ Function test whether option is valid
  →(~b←lex∆is option)/0
  b←∧/(option lex∆haskey 'name'),(option lex∆haskey 'type')
∇

∇name←aparse∆opt∆get_name opt
  name←opt lex∆lookup 'name'
∇

∇b←aparse∆opt∆has_help opt 
  b←~0=⍴opt lex∆lookup 'help'
∇

∇help←aparse∆opt∆get_help opt
  help←opt lex∆lookup 'help'
∇

∇new←old aparse∆opt∆set_help help
  new←old lex∆assign 'help' help
∇

∇b←aparse∆opt∆has_value opt
  b←opt lex∆haskey 'value'
∇

∇value←aparse∆opt∆get_value opt
  value←opt lex∆lookup 'value'
∇

∇new←old aparse∆opt∆set_value value
  new←old lex∆assign 'value' value
∇

∇type←aparse∆opt∆get_type opt
  type←opt lex∆lookup 'type'
∇

∇new←old aparse∆opt∆set_type type
  new←old lex∆assign 'type' type
∇

∇msg←aparse∆opt∆get_help_msg opt
  ⍝ Function compiles a help message
  msg←'--',(15↑aparse∆opt∆get_name opt),aparse∆opt∆get_help opt
∇

⍝		      Options instance accessors
∇options←aparse∆opts∆init
  ⍝ Functions creates an empty option list
  options←lex∆init
∇

∇b←aparse∆opts∆is options
  b←lex∆is options
∇

∇options←old aparse∆opts∆add_opt opt;name
  ⍝ function to add an exisitng option
  name←aparse∆opt∆get_name opt
  options←old lex∆assign name opt
∇

∇b←options aparse∆opts∆has_opt name
  b←options lex∆haskey name
∇

∇opt←options aparse∆opts∆get_opt name
  opt←options lex∆lookup name
∇

∇options←old aparse∆opts∆set_opt opt
  options←old lex∆assign (⊂aparse∆opt∆get_name opt),⊂opt
∇

⍝		      Parser instance accessors

∇ao←aparse∆init
  ⍝ Functions returns a new instance of parser data.
  ao←lex∆init
  ao←ao lex∆assign 'error' ''
  ao←ao lex∆assign 'name' ''
  ao←ao lex∆assign 'options' aparse∆opts∆init
∇

∇instance←old aparse∆set_name name
  instance←old lex∆assign 'name' name
∇

∇name ←aparse∆get_name instance
  ⍝ Function returns the parser instance's name
  name←instance lex∆lookup 'name'
∇

∇options←aparse∆get_options instance
  options←instance lex∆lookup 'options'
∇

∇instance←old aparse∆set_options options
  instance←old lex∆assign 'options' options
∇

∇value←instance aparse∆get_option_value name;opt
  ⍝ Function returns the value of an option
  opt←(aparse∆get_options instance) aparse∆opts∆get_opt name
  value←aparse∆opt∆get_value opt
∇

∇b←aparse∆has_errors instance
  ⍝ Function test to see if an error condition has been defined for
  ⍝ the currennt instance.
  b←0≠⍴instance lex∆lookup 'error'
∇

∇errs←aparse∆get_errors instance
  errs←instance lex∆lookup 'error'
∇

∇b←aparse∆has_help instance
  ⍝ Function test to see if help has been requested
  b←instance lex∆haskey 'help'
∇

∇help←aparse∆get_help instance
  ⍝ Function returns the help message
  help←instance lex∆lookup 'help'
∇

∇instance←old aparse∆add_error msg
  ⍝ Function to add an error to the current instance.
  ⍎(1=≡msg)/'msg←⊂msg'
  instance←old lex∆assign (⊂'error'),⊂ msg,old lex∆lookup 'error'
∇

∇instance←old aparse∆add_option option;options;name
  ⍝ Add on option to the parser instance's list
  options←aparse∆get_options old
  options←options aparse∆opts∆add_opt option
  instance←old aparse∆set_options options
∇

∇b←instance aparse∆has_option name;options
  ⍝ Function test whether the parser instance has an option
  b←(aparse∆get_options instance) aparse∆opts∆has_opt name
∇

∇instance←old aparse∆new_option args;options;name
  ⍝ Function to add a new option. Args may either be an option, or an alist.
  instance←old
  options←aparse∆get_options instance
  ⍝ Option test
  →(aparse∆opt∆is args)/l1
  ⍎(0≠2|⍴args←,args)/'instance←instance aparse∆add_error ''OPTION LIST IS INCOMPLETE''◊→0'
l1:
  →(aparse∆opt∆is args←lex∆from_alist args)/l2
  instance←instance aparse∆add_error 'SUPPLIED ATTRIBUTES ARE ',(' ' utl∆join lex∆keys args),' name, type, AND value ARE REQUIRED.'
  →0
l2:
  ⍎('boolean' utl∆stringEquals args lex∆lookup 'type')/'args←args lex∆assign ''value'' 0'
  instance←instance aparse∆add_option args
  →0
∇

∇instance←cmds aparse∆parse old;options;ct
  ⍝ Function to parse the command line and set the options values."
  ⍎(0=⎕nc'cmds')/'cmds←⎕arg'
  ct←cmds utl∆listSearch '--'
  instance←old aparse∆set_name (ct-1)⊃cmds
  cmds←ct↓cmds ⍝ Drop  inerpretor commands
  options←(aparse∆get_options old) aparse∆next_option cmds
  →(0=1↑⍴options)/0
  instance←instance aparse∆set_options options
  →(options aparse∆opts∆has_opt 'error')/errs
  →(options aparse∆opts∆has_opt 'help')/help
  →0
errs:
  instance←instance  lex∆assign (⊂'error'),⊂ options aparse∆opts∆get_opt 'error'
  →0
help:
  instance←instance lex∆assign (⊂'help'),⊂aparse∆help instance
  →0
∇

∇options←old aparse∆next_option args;name;value;opt
  ⍝ Function recursively updates options from the command line.
  options←old
  →(0=⍴args)/0
  options←options aparse∆next_option 1↓args
  →(∧/'--'=2↑1⊃args)/syn_good
  options←options aparse∆add_error 'SYNTAX ERROR, ',(1⊃args),' SHOULD BEGIN WITH --'
  →0
syn_good:
  name←(~∧\'-'=name)/name←1⊃args
  name←'=' utl∆split name
  →(∧/'help'=4↑1⊃name)/help
  →(1=⍴name)/single
double:
  value←2⊃name
  name←1⊃name
  →(options aparse∆opts∆has_opt name)/db2
  options←options aparse∆add_error 'SYNTAX ERROR, ',name,' NOT A VALID OPTION.'
  →0
db2:
  opt←options aparse∆opts∆get_opt name
  →(∧/'string'=6↑ aparse∆opt∆get_type options aparse∆opts∆get_opt name)/db3
  options←options aparse∆add_error 'SYNTAX ERROR, ',name,' SHOULD NOT HAVE A VALUE.'
  →0
db3:
  opt←opt aparse∆opt∆set_value value
  options←options aparse∆opts∆set_opt opt
  →0
single:
  name←,⊃name
  →(options aparse∆opts∆has_opt name)/s2
  options←options aparse∆add_error 'SYNTAX ERROR, ',name,' IS NOT A VALID OPTION.'
  →0
s2:
  opt←options aparse∆opts∆get_opt name
  opt←opt aparse∆opt∆set_value 1
  options←options aparse∆opts∆set_opt opt
  →0
help:
  options←options aparse∆opts∆set_opt aparse∆opt∆new 'help' 'string'
  →0
∇

∇ msg←aparse∆help instance;lb;ix;opts;opt;names;name
  ⍝ Functions extracts the help message for each option and compiles a
  ⍝ short report
  opts←aparse∆get_options instance
  names←lex∆keys opts
  msg←'Usage: ',(aparse∆get_name instance),' -- [options]',⎕av[11 11]
  lb←((1↑⍴opts)⍴st),ed
  ix←1
st:
  opt←opts lex∆lookup name←ix⊃names
  →(('help' utl∆stringEquals name)∨'error' utl∆stringEquals name)/lp
  msg←msg,⎕av[10],(aparse∆opt∆get_help_msg opt),⎕av[11]
lp:
  →lb[ix←ix+1]
ed:
  →0
∇

∇Z←ap⍙metadata
  Z←0 2⍴⍬
  Z←Z⍪'Author'          'Bill Daly'
  Z←Z⍪'BugEmail'        'bugs@dalywebandedit.com'
  Z←Z⍪'Documentation'   'doc/apl-library.info'
  Z←Z⍪'Download'        'https://sourceforge.net/p/apl-library/code/ci/master/tree/arg_parser.apl'
  Z←Z⍪'License'         'GPL v3.0'
  Z←Z⍪'Portability'     'L3'
  Z←Z⍪'Provides'        ''
  Z←Z⍪'Requires'        'lex,utl'
  Z←Z⍪'Version'                  '0 0 2'
  Z←Z⍪'Last update'          '2019-06-30'
  Z←Z⍪'WSID'            'arg_parser.apl'
∇
