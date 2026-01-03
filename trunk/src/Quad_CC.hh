/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

#ifndef __Quad_CC_DEFINED__
#define __Quad_CC_DEFINED__

#include "QuadFunction.hh"
#include "UCS_string.hh"

//---------------------------------------------------------------------------
class Quad_CC : public QuadFunction
{
public:
   /// Constructor.
   Quad_CC()
      : QuadFunction(TOK_Quad_CC)
   {}

   static Quad_CC  fun;          ///< Built-in function.

protected:
   /// the character classes
   enum CharClass
        {
          QCC_DIGITS = 1,   ///< 0-9
          QCC_ALPHA  = 2,   ///< A-Z
          QCC_alpha  = 3,   ///< a-x
          QCC_ASCII  = 4,   ///< 0-127
          QCC_SUPER  = 5,
          QCC_SUB    = 6,
          QCC_BOX    = 7,
          QCC_MATH   = 8,
        };

   /// overloaded Function::eval_AB()
   Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B()
   Token eval_B(Value_P B) const;

   /// return the characters of class \b cls
   static Value_P get_character_class(CharClass num);

   /// retur true if \b is contained in character class \b cls
   static bool contained_in(Unicode uni, CharClass num);
};
//---------------------------------------------------------------------------

#endif // __Quad_CC_DEFINED__
// EOF
