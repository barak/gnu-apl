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

#ifndef __TOKENIZER_HH_DEFINED__
#define __TOKENIZER_HH_DEFINED__

#include "Token.hh"
#include "UCS_string.hh"

class Token;

//────────────────────────────────────────────────────────────────────────────
/// An iterator for UCS_string
class Unicode_source
{
public:
   /// constructor: iterate over the entire string.
   /// @param s the UCS string to iterate over
   Unicode_source(const UCS_string & s)
   : str(s),
   idx(0),
   end(s.size())
   {}

   /// constructor: iterate from \b from to \b to
   /// @param src parent Unicode_source providing the underlying string
   /// @param from start offset relative to src's current position
   /// @param to number of characters to expose from \b from
   Unicode_source(const Unicode_source & src, int from, int to)
   : str(src.str),
     idx(src.idx + from),
     end(src.idx + from + to)
   {
     if (end > src.str.ssize())   end = src.str.size();
     if (idx > end)   idx = end;
   }

   /// return the number of remaining items
   int rest_len() const
      { return end - idx; }

   /// return true iff there arer any remaining items
   bool has_more() const
      { return idx < end; }

   /// if the next char is \b uni then skip it and return true, otherwise false.
   /// @param uni Unicode character to match and skip
   bool skip_if(Unicode uni)
      {
        if (!has_more() || str[idx] != uni)   return false;
        ++idx;   return true;
      }

   /// lookup next item
   /// @param i offset from current position to look up
   const Unicode & operator[](int i) const
      { i += idx;   Assert(i < end);   return str[i]; }

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
   /// @param new_rest new remaining length (must be <= current rest_len())
   void set_rest(int new_rest)
      { Assert(new_rest <= rest_len());   end = idx + new_rest; }

   /// skip \b count elements
   /// @param count number of elements to advance past
   void skip(int count)
      { idx += count;   if (idx > end)   idx = end; }

   /// return the underlying UCS_string
   const UCS_string & data() const
      { return str; }

   /// return the current position
   int get_pos() const
      { return idx; }

protected:
   /// the source string
   const UCS_string & str;

   /// the current position
   int idx;

   /// the end position (excluding)
   int end;
};
//────────────────────────────────────────────────────────────────────────────
/// The converter from APL input characters to APL tokens
class Tokenizer
{
public:
   /// Constructor
   /// @param pm parse mode controlling tokenization behaviour
   /// @param _loc caller location string for diagnostics
   /// @param mac true if tokenizing macro (not user-entered) code
   Tokenizer(ParseMode pm, const char * _loc, bool mac)
   : pmode(pm),
     macro(mac),
     loc(_loc)
   {}

   /// an APL_Integer or a APL_Float.
   struct Int_or_Double
      {
         /// default constructor for uninitialized Int_or_Double
         Int_or_Double()
            : is_double(false),
              is_valid(false)
            {   value.APL_int = 0; }

         /// constructor for integer
         /// @param aint integer value to store
         Int_or_Double(APL_Integer aint)
            : is_double(false),
              is_valid(true)
            {   value.APL_int = aint; }

         /// constructor for double
         /// @param aflt floating-point value to store
         Int_or_Double(APL_Float aflt)
            : is_double(true),
              is_valid(true)
            {   value.APL_flt = aflt; }

        /// return value as APL_Float
        double get_double() const
           { return is_double ? value.APL_flt : value.APL_int; }

         const bool is_double;   ///< true if so.
         const bool is_valid;    ///< true if so.

         /// an APL_Float or an APL_Float
         union
            {
              APL_Integer APL_int;
              APL_Float   APL_flt;
            } value;
      };

   /// tokenize UTF-8 string \b input into token string \b tos.
   /// @param input UCS string of APL source to tokenize
   /// @param tos token string that receives the resulting tokens
   ErrorCode tokenize(const UCS_string & input, Token_string & tos) const;

   /// tokenize a primitive (1-character) function
   /// @param uni Unicode character representing the primitive function
   static Token tokenize_function(Unicode uni);

   /// tokenize a real number (integer or floating point).
   /// @param src Unicode source to read digits from
   static Int_or_Double tokenize_real(Unicode_source &src);

protected:
   /// tokenize UCS string \b input into token string \b tos.
   /// @param input UCS string of APL source to tokenize
   /// @param tos token string that receives the resulting tokens
   /// @param rest_2 remaining character count for 2-byte sequences
   void do_tokenize(const UCS_string & input, Token_string & tos,
                    size_t & rest_2) const;

   /// tokenize a function
   /// @param src Unicode source positioned at the function character
   /// @param tos token string that receives the resulting token
   void tokenize_function(Unicode_source & src, Token_string & tos) const;

   /// tokenize a Quad function or variable
   /// @param src Unicode source positioned after the ⎕ character
   /// @param tos token string that receives the resulting token
   void tokenize_quad(Unicode_source & src, Token_string & tos) const;

   /// tokenize a single quoted 'string'
   /// @param src Unicode source positioned after the opening quote
   /// @param tos token string that receives the resulting token
   /// @param rest_2 remaining character count for 2-byte sequences
   void tokenize_string1(Unicode_source & src, Token_string & tos,
                         size_t & rest_2) const;

   /// tokenize a double quoted "string"
   /// @param src Unicode source positioned after the opening double-quote
   /// @param tos token string that receives the resulting token
   /// @param rest_2 remaining character count for 2-byte sequences
   void tokenize_string2(Unicode_source & src, Token_string & tos,
                         size_t & rest_2) const;

   /// tokenize a number (integer, floating point, or complex).
   /// @param src Unicode source positioned at the first digit or sign
   /// @param tos token string that receives the resulting token
   /// @param rest_2 remaining character count for 2-byte sequences
   void tokenize_number(Unicode_source & src, Token_string & tos,
                        size_t & rest_2) const;

   /// tokenize a hex number (integer).
   /// @param src Unicode source positioned at the first hex digit
   static Int_or_Double tokenize_hex(Unicode_source &src);

   /// a locale-independent sscanf()
   /// @param strg null-terminated string to parse
   /// @param result APL_Float that receives the parsed value
   /// @param E_pos position of exponent character in \b strg, or -1
   /// @param minus_pos position of minus sign in \b strg, or -1
   static int scan_real(const char * strg, APL_Float & result,
                        int E_pos, int minus_pos);

   /// tokenize a symbol
   /// @param src Unicode source positioned at the first character of the symbol
   /// @param tos token string that receives the resulting token
   void tokenize_symbol(Unicode_source & src, Token_string & tos) const;

   /// the parsing mode of this parser
   const ParseMode pmode;

   /// tokenize macro code
   const bool macro;

   /// caller of this Tokenizer
   const char * loc;
};

#endif // __TOKENIZER_HH_DEFINED__
