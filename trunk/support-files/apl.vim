" Vim syntax file
" Language:     APL
" Maintainer:   Jürgen Sauermann <bug-apl@gnu.org>
" Last Change:  2012 Mar 3

" for this file to work, you may want to, for example:
" 1. copy this file to /etv/vim, and
" 2. add the following lines (uncommented) to file /etc/vim/vimrc
"
" Source a GNU APL syntax checking if available
" if filereadable("/etc/vim/apl.vim")
"   source /etc/vim/apl.vim
" endif

" standard APL comment
syn region aplComment     start="⍝" end="$"

" APL Doxy comment
syn region aplDoxy        start="⍝⍝" end="$"

" non-standard APL comment to make APL scriptable
syn region aplComment     start="#" end="$"

" standard APL string
syn region aplString     oneline start="'" end="'"
syn region aplString     oneline start='"' end='"'

" Numbers...
"                         +----- optional mantissa sign          +-- J
"                         |  +-- mantissa  --+ +-- exponent --+  |   (optional)
"                         |  |               | |   (optional) |  |
"                         |  | a) .nnn       | |              |  |
"                         |  | b) nnn.mmm    | | +- expo sign |  |
"                         |  |               | | |  (opt.)    |  |
"                         |  |               | | |            |  |
syn match aplNumber      "¯\=\.[0-9]\+\(E¯\=[0-9]\+\)\=J\="
syn match aplNumber      "¯\=[0-9]\+\(\.[0-9]*\)\=\(E¯\=[0-9]\+\)\=J\="

" user defined and distinguished names
syn match aplIdentifier	 "[A-Z_a-z∆⍙⎕][0-9A-Za-z_∆⍙]*"
syn match aplIdentifier	 "⍞"

syn match aplStatement   "[∇⍫◊]"
syn match aplStatement	 "^ *[A-Z_a-z∆⍙⎕][0-9A-Za-z_∆⍙]*:"
syn match aplStatement	 "^ *\[[0-9.]*\]"

hi def link aplComment       CursorColumn
hi def link aplDoxy          FoldColumn
hi def link aplString        String
hi def link aplNumber        Number
hi def link aplIdentifier    Identifier
hi def link aplStatement     Statement

let b:current_syntax = "apl"

