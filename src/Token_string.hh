/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2026  Dr. Jürgen Sauermann

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/** @file
*/

#ifndef __TOKEN_STRING_HH_DEFINED__
#define __TOKEN_STRING_HH_DEFINED__

#include "Error.hh"
#include "Token.hh"

//════════════════════════════════════════════════════════════════════════════
/// A sequence of Token
class Token_string : public  std::vector<Token_loc>
{
public:
   /// construct an empty string
   Token_string()   {}

   /// construct a string of \b len Token, starting at \b data.
   /// @param data  pointer to first Token in the source array
   /// @param len   number of tokens to copy
   Token_string(const Token * data, ShapeItem len)
      { loop(l, len)   emplace_back(data[l], Function_PC_invalid); }

   /// construct a string of \b len Token from another token string
   /// @param other  source token string
   /// @param pos    starting position in \b other
   /// @param len    number of tokens to copy
   Token_string(const Token_string & other, size_t pos, size_t len)
      { loop(l, len)   push_back(other[pos++]); }

   /// make size() signed
   ShapeItem ssize() const
      { return ShapeItem(vector<Token_loc>::size()); }

   using std::vector<Token_loc>::operator[];

   /// the token at position \b pc of an executed body. There is no non-const
   /// variant: a body indexed by Prefix_PCs is immutable (it is modified
   /// only at ⎕FX time, using Function_PCs).
   const Token_loc & operator[](Prefix_PC pc) const
      { return vector<Token_loc>::operator[](size_t(pc)); }

   /// the Prefix_PC at the end of an executed body
   Prefix_PC end_PC() const
      { return Prefix_PC(size()); }

   /// check that every [, (, resp. { has a matching ], ), resp. },
   /// and no invalid combination like [ ( ] ) is present. Throws an
   /// Error (full statement text + carets, same visual convention as
   /// Prefix.cc/Path A errors -- see build_error_line_2() below and
   /// devel_doc/Carets.txt) if not.
   void all_brackets_closed() const;

   /// throw a SYNTAX ERROR (with line 2) if a ; is outside [...] (except
   /// for the trailing local variables ;A;B... of a lambda body)
   /// @param lambda_locals true if trailing ;A;B... are allowed
   void check_semicolons(bool lambda_locals) const;

   /// build error_message_2 (rendered token text of *this) and the
   /// left_caret/right_caret positions for tokens [lo, hi] (inclusive,
   /// plain left-to-right index order, i.e. the order a freshly
   /// tokenized statement is naturally in -- unlike the reordered
   /// representation Executable::set_error_info() expects, see
   /// devel_doc/Carets.txt). A single point (lo == hi) yields one
   /// highlighted token.
   /// @param error error to fill in
   /// @param lo index of the leftmost token to highlight
   /// @param hi index of the rightmost token to highlight
   void build_error_line_2(Error & error, int lo, int hi) const;

   /// find the closing bracket for \b tos[pos]; throw error if not found
   /// @param pos  position of the opening bracket token
   int find_closing_bracket(int pos) const;

   /// find the closing paranthesis for \b tos[pos]; throw error if not found.
   /// @param pos  position of the opening parenthesis token
   int find_closing_parent(int pos) const;

   /// find the opening bracket for \b tos[pos]; throw error if not found
   /// @param pos  position of the closing bracket token
   int find_opening_bracket(int pos) const;

   /// find the opening curly bracket for \b tos[pos]; throw error if not found.
   /// @param pos  position of the closing curly bracket token
   int find_opening_curly(int pos) const;

   /// find the opening paranthesis for \b tos[pos]; throw error if not found.
   /// @param pos  position of the closing parenthesis token
   int find_opening_parent(int pos) const;

   /// print \b this token string
   /// @param out      output stream
   /// @param details  bitmask controlling output verbosity (bit 0: PC, bit 1: value)
   void print(ostream & out, int details) const;

   /// insert one TOK_VOID after \b pos
   /// @param pos  position after which the void token is inserted
   void insert_1(int pos);

   /// insert two TOK_VOID after \b pos
   /// @param pos  position after which the two void tokens are inserted
   void insert_2(int pos);

   /// remove all TOK_VOID from \b this Token_string, return the numer
   /// of tokens removed.
   VoidCount remove_TOK_VOID();

   /// replace the segment starting a \b pos with \b src. Return the new pos.
   /// @param src  source token string whose tokens overwrite the segment
   /// @param pos  starting position where replacement begins
   ShapeItem replace_segment(const Token_string & src, ShapeItem pos);

   /// append \b tok as the END (or RETURN) token of the last statement. In
   /// the (reversed) body that token is the left end of the statement and
   /// gets the PC of the leftmost token of the statement, or else (for an
   /// empty statement) \b empty_pc (the PC of the ◊ or of the end of line)
   void append_statement_end(const Token & tok, Function_PC empty_pc)
      {
        if (size() && back().get_Class() != TC_END &&
                      back().get_Class() != TC_RETURN)
           {
             const Token_loc tl(tok, back());
             push_back(tl);
           }
        else
           {
             emplace_back(tok, empty_pc);
           }
      }

   /// reverse the token order from \b from to \b to (including)
   /// @param from  index of first token to reverse
   /// @param to    index of last token to reverse (inclusive)
   void reverse_from_to(ShapeItem from, ShapeItem to);

private:
   /// prevent accidental copying
   Token_string & operator =(const Token_string & other) = delete;
};
//════════════════════════════════════════════════════════════════════════════
#endif // __TOKEN_STRING_HH_DEFINED__
// EOF
//
