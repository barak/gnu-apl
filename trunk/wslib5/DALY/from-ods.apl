#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ from-ods.apl  Workspace to import data from an open office spreadsheet
⍝ Copyright (C) 2022 Bill Daly

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

⍝ 20230320 Current procedure
⍝ doc←ods∆import∆file '/full/path/to/file'
⍝ array←N ods∆table∆extract doc
⍝ count←ods∆document∆table_count

⍝ ********************************************************************

)copy 5 DALY/dom
)copy 5 DALY/utf8
)copy 5 FILE_IO

 ∇ meta←ods_meta fname;xml;root;children;ix;label;cur
  ⍝ Function to parse meta.xml.  Function returns a lexicon
  xml←dom∆parse utf8∆read fname
  root←1⊃dom∆node∆getChildren dom∆document∆rootElement xml
  meta←lex∆init
  children←dom∆node∆getChildren root
  label←((⍴children)⍴st),ed
  ix←1
st:
  →(dom∆node∆hasAttributes cur←ix⊃children)/attr
  meta←meta lex∆assign (⊂dom∆node∆nodeName cur),⊂dom∆node∆nodeValue 1⊃dom∆node∆getChildren cur
  →nxt
attr:
  meta←meta,[1]ods_attr2lex dom∆node∆attributes cur
  →nxt
nxt:
  →label[ix←ix+1]
ed:
∇

∇ doc←ods∆parseContent fname;txt
  ⍝ Function parses the named file and returns an xml document
  doc←dom∆parse utf8∆read fname
∇

∇ doc←ods∆import∆file fname;tmpName;sink
  ⍝ Function returns a document object from an ods file.
  tmpName←'/tmp/',2⊃ utl∆fileName∆parse fname
  →(0≠FIO∆mkdir_777 tmpName)/err0
  →(~utf8∆fileExists fname)/err1
  sink←FIO∆pipefrom 'unzip ',fname,' content.xml -d ',tmpName
  doc←ods∆parseContent tmpName,'/content.xml'
  →(0≠FIO∆unlink tmpName,'/content.xml')/err2
  →(0≠FIO∆rmdir tmpName)/err3
  →0
err0:
  ⍞←doc←'Unable to create working directory.'
  →0
err1:
  ⍞←doc←'Unable to access file, ',fname,'.'
  →0
err2:
  ⍞←'Error removing work file ',tmpName,'/content.xml.  Please remove manually.'
  →0
err3:
  ⍞←'Error removing work directory ',tmpName,'. Please remove manually.'
  →0
∇

∇ count←attr ods∆numericAttr cell
  ⍝ Function returns the amount of numberican attribute, including 0
  ⍝ when the attribute is not declared or nil
  →(0=⍴count←cell dom∆node∆getAttribute attr)/nil
  count←⍎dom∆node∆nodeValue count
  →0
nil:
  count←0
  →0
∇

∇ clean←ods∆array∆rm_blanks array
  ⍝ Function removes empty columns from the final array
  clean← {(~∊⌽∧\⌽ods∆cell∆isNil ¨⍵)/⍵}¨array
∇

⍝ ********************************************************************
⍝ Document
⍝ ********************************************************************

∇count←ods∆document∆table_count doc
  ⍝ Function returns a count of the tables in a zip file.
  count←⍴ods∆document∆tables doc
∇

∇ tables←ods∆document∆tables doc
  ⍝ Function returns the tables in an ods file.
  tables←'table:table' dom∆document∆getElementsByTagName doc
∇

∇ xmlns←ods∆document∆xmlns doc
  ⍝ Function returns a list of xml name spaces declared for the document.
  xmlns←dom∆node∆attributes dom∆document∆rootElement doc
∇

⍝ ********************************************************************
⍝ Table
⍝ ********************************************************************
∇table←nth ods∆table∆extract document;dom_table;rrows;hrows
  ⍝ Function returns an array from the nth table in a document
  dom_table←nth⊃'table:table' dom∆document∆getElementsByTagName document
  children←dom∆node∆getChildren dom_table
  →(0=⍴hrows←ods∆header_row∆find children)/find_rows
  hrows←dom∆node∆getChildren hrows
find_rows:
  rrows←ods∆row∆find children
  table←⊃ods∆array∆rm_blanks ods∆row∆assemble ¨ hrows, rrows
∇

∇ cols←ods∆table∆cols table;children
  ⍝ Function returns the table-column children of table.
  children←dom∆node∆getChildren table
  cols←('table:table-column' ods∆match∆nodeName children)/children
∇

∇ rows←ods∆table∆rows table;children
  ⍝ Function returns the table-column children of table.
  children←dom∆node∆getChildren table
  rows←('table:table-row' ods∆match∆nodeName children)/children
∇

⍝ ********************************************************************
⍝ Header rows
⍝ ********************************************************************

∇ array ods∆header_row∆extract header
  ⍝ Function returns an array from a header_rows element
  array←⊃ods∆row∆rm_blanks ods∆row∆assemble ¨ header
∇

∇noderows←ods∆header_row∆find children;parent
  ⍝ Function returns a nodeList of header_rows elements from a list of
  ⍝ children
  parent←('table:table-header-rows'ods∆match∆nodeName children) /children
  →(0=⍴parent)/not_found
found:
  noderows←dom∆node∆getChildren parent
  →0
not_found:
  noderows←0⍴⍬
∇

⍝ ********************************************************************
⍝ row
⍝ ********************************************************************
∇ count←ods∆row∆repeated row
  ⍝ Function returns the number of rows repeated.
  count←'table:number-rows-repeated' ods∆cell∆numericAttr cell
∇

∇ count←ods∆row∆spanned row
  ⍝ Function returns table:number-rows-spanned
  count←'table:number-rows-spanned' ods∆numericAttr cell
∇

∇ b←ods∆row∆trailingNil row
  ⍝ Function returns a vector where trailing nil cells are false.
  b←⌽~∧\⌽ods∆cell∆isNil ¨ row
∇

∇ rows←ods∆row∆find node_list
  ⍝ Function returns the rows in node_list
  rows←('table:table-row' ods∆match∆nodeName node_list)/node_list
∇

∇ row←ods∆row∆assemble dom_row;cell;children;lb;ix;count
  ⍝ Function assembles a row from a dom_node
  children ← dom∆node∆getChildren dom_row
  ix←1
  lb←((¯1+⍴children)⍴st),ed
  row←''
  →lb[ix]
st:
  count←1⌈ods∆cell∆repeated cell←ix⊃children
  row←row,count ⍴ ⊂ ods∆cell∆assemble cell
  →lb[ix←ix+1]
ed:				⍝ Last cell
  row←row,⊂ods∆cell∆assemble ix⊃children
  →0
∇

⍝ ********************************************************************
⍝ cell
⍝ ********************************************************************
∇ type←ods∆cell∆type cell;type
  ⍝ Function returns the office:value-type of a cell.
  →(0=⍴type← cell dom∆node∆getAttribute 'office:value-type')/nil
  type←dom∆node∆nodeValue type
  →0
nil:
  type←'nil'
  →0
∇

∇ txt←ods∆cell∆string_value cell;text_elm;children
  ⍝ Function returns the text of a string cell
  txt←ods∆cc∆text_p ¨ dom∆node∆getChildren cell
  txt←' ' utl∆join txt
∇

∇ float←ods∆cell∆float_value cell
  ⍝ Function returns the value of float cell
  float←⍎ dom∆node∆nodeValue cell dom∆node∆getAttribute 'office:value'
∇

∇ cell←ods∆cell∆assemble dom_cell;type
  ⍝ Function assembles a cell from a dom node.
  type←1↑ods∆cell∆type dom_cell	⍝Either 'f'loat, 's'tring, or 'n'il
  →('sfn'=type)/string,float,nil
  cell←ods∆cell∆nil
  →0
string:
  cell←ods∆cell∆string_value dom_cell
  →0
float:
  cell←ods∆cell∆float_value dom_cell
  →0
nil:
  cell←ods∆cell∆nil
  →0
∇

⍝ Numeric attributes where the default value is 0.
⍝   1. table:number-columns-spanned.
⍝   2. table:number-rows-spanned 
⍝   3. table:number-columns-repeated
⍝   4. tabel:number-rows-repeated

⍝ Text attributes where default value is ' '.
⍝   1. table:formula

∇ count←ods∆cell∆repeated cell
  ⍝ Function returns the number of cells repeated.
  count←'table:number-columns-repeated' ods∆numericAttr cell
∇

∇ count←ods∆cell∆spanned cell
  ⍝ Function returns table:number-columns-spanned
  count←'table:number-columns-spanned' ods∆numericAttr cell
∇

∇ cell←ods∆cell∆nil
  ⍝ Function returns a value for a nil cell
  cell←⊂' '
∇

∇b←ods∆cell∆isNil cell
  ⍝ Function test to see if the cell is nil
  →(~b←0=⍴cell)/0
  b←' '=cell
∇

⍝ ********************************************************************
⍝ Cell Content
⍝ These functions represent elements not part of the spreadsheet
⍝ structure that will return cell content
⍝ ********************************************************************

∇txt←ods∆cc∆text_tab 
  ⍝ Function returns a tab character to replace <text:tab/>
  txt←⎕av[10]
∇

∇ txt←ods∆cc∆text_space
  ⍝ Function returns a space character to replace <text:s/>
  txt←' '
∇

∇ txt←ods∆cc∆text_heading
  ⍝ Function calls ods∆cc∆text_p for the contents of a <text:h/>,
  ⍝ heading tag.
  txt←ods∆cc∆text_heading
∇

∇ txt←ods∆cc∆text_line_break
  ⍝ Function returns a space to replace <text:line-break/>.
  txt←' '
∇

∇ txt←ods∆cc∆text_p elm;name
  ⍝ Function returns a line of code for ods∆cc∆text-p to execute for
  ⍝ elm.
  →(dom∆TEXT_NODE ≠ dom∆node∆nodeType elm)/nx1
  txt←dom∆node∆nodeValue elm
  →0
nx1:
  name←dom∆node∆nodeName elm
  →(~'text:p' utl∆stringEquals name)/nx2
  txt←' ' utl∆join ods∆cc∆text_p ¨ dom∆node∆getChildren elm
  →0
nx2:
  →(~'text:tab' utl∆stringEquals name)/nx3
  txt←ods∆cc∆text_tab
  →0
nx3:
  →(~'text:s' utl∆stringEquals name)/nx4
  txt←ods∆cc∆text_space
  →0
nx4:
  →(~'text:h' utl∆stringEquals name)/nx5
  txt←ods∆cc∆text_heading
  →0
nx5:
  →(~'text:line-break' utl∆stringEquals name)/nx6
  txt←ods∆cc∆text_line_break
  →0
nx6:
  name←dom∆node∆nodeName elm
  →(~'text:' utl∆stringEquals name)/nx7
  txt←' ' utl∆join ods∆cc∆text_p ¨ dom∆node∆getChildren elm
  →0
nx7:
  txt←'A node named ',name,' was unexpectedly found.'
  →0
∇
⍝ ********************************************************************
⍝ Predicates
⍝ ********************************************************************

∇ b←name ods∆match∆nodeName nodes
  ⍝ Function tests for the given name
  b←∊{name utl∆stringEquals dom∆node∆nodeName ⍵ }¨nodes
∇

⍝ ********************************************************************
⍝ Ad hoc functions which really belong somewhere else
⍝ ********************************************************************

∇ rs←ad_hoc_showList nodeList
  ⍝ Function returns xml for each node in the list.
  rs←1⌽∊{⎕tc[3],dom∆node∆toxml ⍵}¨ nodeList
∇

∇ rs←ad_hoc_showChildren node
  ⍝  Function returns a list of xml nodes that are children of node.
  rs←ad_hoc_showList dom∆node∆getChildren node
∇
