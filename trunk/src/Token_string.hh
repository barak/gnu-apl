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

#include "Token.hh"

//----------------------------------------------------------------------------
/// A sequence of Token
class Token_string : public  std::vector<Token>
{
public:
   /// construct an empty string
   Token_string()   {}

   /// make size() signed
   ShapeItem ssize() const
      { return ShapeItem(vector<Token>::size()); }

   /// construct a string of \b len Token, starting at \b data.
   /// @param data  pointer to first Token in the source array
   /// @param len   number of tokens to copy
   Token_string(const Token * data, ShapeItem len)
      { loop(l, len)   push_back(data[l]); }

   /// construct a string of \b len Token from another token string
   /// @param other  source token string
   /// @param pos    starting position in \b other
   /// @param len    number of tokens to copy
   Token_string(const Token_string & other, uint32_t pos, uint32_t len)
      { loop(l, len)   push_back(other[pos++]); }

   /// reverse the token order from \b from to \b to (including)
   /// @param from  index of first token to reverse
   /// @param to    index of last token to reverse (inclusive)
   void reverse_from_to(ShapeItem from, ShapeItem to);

   /// replace the segment starting a \b pos with \b src. Return the new pos.
   /// @param src  source token string whose tokens overwrite the segment
   /// @param pos  starting position where replacement begins
   ShapeItem replace_segment(const Token_string & src, ShapeItem pos);

   /// find the opening bracket for \b tos[pos]; throw error if not found
   /// @param pos  position of the closing bracket token
   int find_opening_bracket(int pos) const;

   /// find the closing bracket for \b tos[pos]; throw error if not found
   /// @param pos  position of the opening bracket token
   int find_closing_bracket(int pos) const;

   /// find the opening paranthesis for \b tos[pos]; throw error if not found.
   /// @param pos  position of the closing parenthesis token
   int find_opening_parent(int pos) const;

   /// find the closing paranthesis for \b tos[pos]; throw error if not found.
   /// @param pos  position of the opening parenthesis token
   int find_closing_parent(int pos) const;

   /// find the opening curly bracket for \b tos[pos]; throw error if not found.
   /// @param pos  position of the closing curly bracket token
   int find_opening_curly(int pos) const;

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

private:
   /// prevent accidental copying
   Token_string & operator =(const Token_string & other);
};
//----------------------------------------------------------------------------
#endif // __TOKEN_STRING_HH_DEFINED__
// EOF
//
