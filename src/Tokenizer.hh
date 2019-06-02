/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2015  Dr. Jürgen Sauermann

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

#ifndef __TOKENIZER_HH_DEFINED__
#define __TOKENIZER_HH_DEFINED__

#include "Token.hh"
#include "UCS_string.hh"

class Token;

//-----------------------------------------------------------------------------
/// An iterator for UCS_string
class Unicode_source
{
public:
   /// constructor: iterate over the entire string.
   Unicode_source(const UCS_string & s)
   : str(s),
   idx(0),
   end(s.size())
   {}

   /// constructor: iterate from \b from to \b to
   Unicode_source(const Unicode_source & src, int32_t from, int32_t to)
   : str(src.str),
     idx(src.idx + from),
     end(src.idx + from + to)
   {
     if (end > src.str.size())   end = src.str.size();
     if (idx > end)   idx = end;
   }

   /// return the number of remaining items
   int32_t rest() const
      { return end - idx; }

   /// lookup next item
   const Unicode & operator[](int32_t i) const
      { i += idx;   Assert(uint32_t(i) < uint32_t(end));   return str[i]; }

   /// get next item
   const Unicode & get()
      { Assert(idx < end);   return str[idx++]; }

   /// lookup next item without removing it
   const Unicode & operator *() const
      { Assert(idx < end);   return str[idx]; }

   /// skip the first element
   void operator ++()
      { Assert(idx < end);   ++idx; }

   /// undo skip of the current element
   void operator --()
      { Assert(idx > 0);   --idx; }

   /// shrink the source to rest \b new_rest
   void set_rest(int32_t new_rest)
      { Assert(new_rest <= rest());   end = idx + new_rest; }

   /// skip \b count elements
   void skip(int32_t count)
      { idx += count;   if (idx > end)   idx = end; }

protected:
   /// the source string
   const UCS_string & str;

   /// the current position
   int32_t idx;

   /// the end position (excluding)
   int32_t end;
};
//-----------------------------------------------------------------------------
/// The converter from APL input characters to APL tokens
class Tokenizer
{
public:
   /// Constructor
   Tokenizer(ParseMode pm, const char * _loc, bool mac)
   : pmode(pm),
     macro(mac),
     loc(_loc),
     rest_1(0),
     rest_2(0)
   {}

   /// tokenize UTF-8 string \b input into token string \b tos.
   ErrorCode tokenize(const UCS_string & input, Token_string & tos);

   /// tokenize a primitive (1-character) function
   static Token tokenize_function(Unicode uni);

protected:
   /// tokenize UCS string \b input into token string \b tos.
   void do_tokenize(const UCS_string & input, Token_string & tos);

   /// tokenize a function
   void tokenize_function(Unicode_source & src, Token_string & tos);

   /// tokenize a Quad function or variable
   void tokenize_quad(Unicode_source & src, Token_string & tos);

   /// tokenize a single quoted string
   void tokenize_string1(Unicode_source & src, Token_string & tos);

   /// tokenize a double quoted string
   void tokenize_string2(Unicode_source & src, Token_string & tos);

   /// tokenize a number (integer, floating point, or complex).
   void tokenize_number(Unicode_source & src, Token_string & tos);

   /// tokenize a real number (integer or floating point).
   bool tokenize_real(Unicode_source &src, bool & need_float,
                      APL_Float & flt_val, APL_Integer & int_val);

   /// a locale-independent sscanf()
   static int scan_real(const char * strg, APL_Float & result, 
                        int E_pos, int minus_pos);

   /// tokenize a symbol
   void tokenize_symbol(Unicode_source & src, Token_string & tos);

   /// the parsing mode of this parser
   const ParseMode pmode;

   /// tokenize macro code
   const bool macro;

   /// caller of this Tokenizer
   const char * loc;

   /// the characters afer caret 1
   int rest_1;

   /// the characters afer caret 2
   int rest_2;
};

#endif // __TOKENIZER_HH_DEFINED__
