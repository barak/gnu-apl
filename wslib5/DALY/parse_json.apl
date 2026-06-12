#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ parse_json Workspace to parse a json file
⍝ Copyright (C) 2023 Bill Daly

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

)copy 5 DALY/utf8
)copy 5 DALY/utl
)copy 3 DALY/lex

⍝ Return type codes
json_return_array ← 'a'
json_return_obj ← 'o'
json_return_error ← 'e'
json_return_not_implemented ← 'z'

∇ rs←json_parse buffer;index;value;type
  ⍝ Function parses a json buffer and returns the data structure
  ⍝ found.
  buffer←json_whitespace buffer
  (type index value)←1 json_dispatch buffer
  rs←value
∇

∇ rs←json_whitespace buffer
  ⍝ Function returns the buffer with no white space.
  rs←(~(buffer ∊ ⎕av[10 11 33])∧~≠\buffer='"')/buffer
∇

∇ rs←index json_dispatch buffer;value
  ⍝ Function looks at the next character to decide whcich function to
  ⍝ call. It returns the type of the value, the current pointer into
  ⍝ the array, and the value found.
  type←'[{' ⍳ buffer[index]
  →(array,obj,oh_no)[type]
array:
  (index value)←(index + 1) json_array buffer
  rs←json_return_array index value
  →0
obj:
  (index value)←(index + 1) json_object buffer
  rs←json_return_obj index value
  →0
oh_no:
  rs←json_return_error 0 ⍬
  →0
∇

∇ rs←index json_array buffer;array;value;return_code
  ⍝ Function returns the index of the character following an array and
  ⍝ the array itself.
  array←⍬
st:
  →(buffer[index]∊'[{')/dispatch
  (index value)←(index  + buffer[index]=',')json_value buffer
  →loop
dispatch:
  (return_code index value)← index json_dispatch buffer
  →loop
loop:
  index←index + buffer[index]=','
  array←array,⊂value
  →(buffer[index]=']')/end
  →st
end:
  rs←(index + 1),⊂ array
∇

∇ rs←index json_object buffer;key;value;obj;return_code
  ⍝ Function returns json_return_obj, the index of the first character
  ⍝ after an object and the object begining at index.
  obj←lex∆init
st:
  (index key)←(index + buffer[index]=',') json_key buffer
  →(buffer[index + 1]∊'[{')/dispatch
  (index value)←(index + 1) json_value buffer
  →loop
dispatch:
  (return_code index value)←(index + 1) json_dispatch buffer
  →loop
loop:
  obj←obj lex∆assign key value
  →(buffer[index]='}')/end
  →st
end:
  rs←(index + 1) obj
∇

∇ rs←index json_key buffer;l;k;i
  ⍝ Function returns the key of a key-value by delimited  ':'
  l←+/∧\((i←index-1)↓buffer)≠':'
  rs←(+/index  l),⊂ utl∆strip_quotes l↑i↓buffer
∇

∇ rs←index json_value buffer;l;v;i;true;false
  ⍝ Function called when the parser expects either a number of a
  ⍝ string and buffer[index] should be either '"' or a digit.\
  true←1 ◊ false←0
  l←+/∧\~((i←index-1)↓buffer)∊',]}'
  →('"'=buffer[index])/str
no:
  v←⍎l↑i↓buffer
  →end
str:
  v←utl∆strip_quotes l↑i↓buffer
  →end
end:
  rs←(index + l) v
∇
