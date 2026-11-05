" Vim syntax file
" Language:     APL
" Maintainer:   Jürgen Sauermann <bug-apl@gnu.org>
" Last Change:  2026 Jul 14

" for this file to work, you may want to, for example:
" 1. copy this file to /etv/vim, and
" 2. add the following lines (uncommented) to file /etc/vim/vimrc
"
" Source a GNU APL syntax checking if available
" if filereadable("/etc/vim/apl.vim")
"   source /etc/vim/apl.vim
" endif

" Vim's default syntax syncing heuristics assume blank lines are safe
" places to (re-)start highlighting from, which is wrong here: multi-line
" strings/literals routinely contain blank lines as part of their value
" (e.g. an empty row of a multi-line literal, or a blank line inside a
" """ string), and syncing from such a line would misinterpret string
" content as code. Always parse from the start of the buffer instead.
syn sync fromstart

" standard APL comment
syn region aplComment     start="⍝" end="$"

" APL Doxy comment
syn region aplDoxy        start="⍝⍝" end="$"

" non-standard APL comment to make APL scriptable
syn region aplComment     start="#" end="$"

" New-style multi-line string: a """ at the end of a line starts it, a
" (possibly indented) """ at the start of a later line ends it. Must be
" defined before the single-line '"...\"' rule below so that a plain
" '"' start (with negative lookahead) does not try to swallow it.
syn region aplMLString    start='"""\s*$' end='^\s*"""' keepend

" Multi-line comment idiom (apl.texi "Multi-Line Comments"): a ⊣«««...»»»
" multi-line string immediately preceded by monadic ⊣ is a GNU-APL
" idiom, not an ordinary string value -- ⊣ discards the string, and GNU
" APL recognises and optimises the whole construct away entirely (no
" runtime cost), so despite the string syntax it functions as, and reads
" as, a comment. Coloured the same as aplComment below rather than as a
" string. The optional trailing ⍝⍝⍝ on the ⊣«««/»»» lines is purely
" decorative (see apl.texi) and has no effect either way, so it is
" allowed but not required -- and so is ordinary explanatory text
" directly after «««, e.g. "⊣««« ⍝ Function FOO: does X.", which is
" this project's actual, universal convention throughout its own .tc
" files (found the hard way: an earlier version of this pattern only
" allowed *exactly* an optional bare ⍝⍝⍝ before end-of-line, so it
" never matched real content there at all -- the region silently never
" started, and every ⊣«««...»»» block in every .tc file rendered with
" no comment colouring whatsoever, not even in the html2tex pipeline
" used for the journal article's appendix). '.*$' accepts any trailing
" text on the opening line, decorative or explanatory alike. Defined
" before aplMLString2 below: since this pattern starts one token
" earlier (at the ⊣), Vim's leftmost-match rule would already prefer
" it over aplMLString2 for the same «««, but defining it first also
" documents that ordering intent explicitly.
syn region aplMLComment   start='⊣\s*«««.*$' end='^\s*»»»\%(\s*⍝⍝⍝\)\=' keepend

" « » string: the multi-line form ««« ... »»» (optionally followed by
" the recommended, purely decorative ⍝⍝⍝ marker, or by ordinary
" explanatory text -- see the aplMLComment note above, same fix
" applies here) and the single-line « ... » form. « ... » is an
" alternative to "..." that avoids the "forgotten closing quote
" swallows the rest of the script" problem.
syn region aplMLString2   start='«««.*$' end='^\s*»»»' keepend
syn region aplString2     oneline start='«\%(««\)\@!' end='»\%(»»\)\@!'

" Multi-line literal: <<< ... >>>, e.g.
"       ⎕ ← <<<
"           1 2 3
"           4 5 6
"           >>>
" Multi-line literals may recursively contain further multi-line
" literals (e.g. a nested sub-matrix) as well as strings, numbers etc.,
" hence contains=ALL. (A same-name self-reference alone, e.g.
" contains=aplMLLiteral, also nests correctly but loses number/string
" sub-highlighting; an *explicit list* of several contained groups
" alongside the self-reference breaks nesting depth tracking instead --
" a literal containing a nested literal then incorrectly ends at the
" *inner* literal's closing >>>. contains=ALL avoids both problems --
" verified empirically.)
syn region aplMLLiteral   start='<<<' end='^\s*>>>' contains=ALL

" standard APL string. The negative lookahead on '"' excludes the start
" of a """ multi-line string (handled by aplMLString above) so that a
" lone stray '"' is never left hunting for its closing quote far away.
syn region aplString     oneline start='"\%("\)\@!' end='"'
syn region aplString     oneline start="'" end="'"

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
hi def link aplMLComment     CursorColumn
hi def link aplDoxy          FoldColumn
hi def link aplString        String
hi def link aplMLString      String
hi def link aplString2       String
hi def link aplMLString2     String
hi def link aplMLLiteral     Special
hi def link aplNumber        Number
hi def link aplIdentifier    Identifier
hi def link aplStatement     Statement

" A background marker on ∇/{}-boundary lines was tried (to set long
" function definitions apart from immediate execution) and went
" through several iterations -- a whole-body background (abandoned:
" Vim's :syn highlighting doesn't cascade a background through nested
" items, and truly blank lines can't be coloured via :syn at all),
" then a matchadd()-based boundary-only marker with a few different
" grey shades -- but it never read as anything other than "too dark"
" on at least one real terminal, even after the colour values were
" verified correct both in the raw escape codes and via Vim's own `:hi`
" report; likely that terminal's own palette rendering of the specific
" ctermbg index, which Vim has no control over. Dropped in favour of
" ∇-lines/dfn boundaries simply looking like any other line.

let b:current_syntax = "apl"
