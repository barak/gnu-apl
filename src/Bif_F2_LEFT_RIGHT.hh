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

#ifndef __BIF_F2_LEFT_RIGHT_HH_DEFINED__
#define __BIF_F2_LEFT_RIGHT_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** System function left (⊣) */
/// The class implementing ⊣
class Bif_F2_LEFT : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F2_LEFT()
   : NonscalarFunction(TOK_F2_LEFT)
   {}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return Token(TOK_APL_VALUE1, A.clone(LOC)); }

   /// overloaded Function::eval_B()
   virtual Token eval_B(cValue_R B) const
      { return Token(TOK_APL_VALUE2, IntScalar(0, LOC)); }

   static Bif_F2_LEFT  fun;   ///< Built-in function
protected:
};
//────────────────────────────────────────────────────────────────────────────
/** System function right (⊢) */
/// The class implementing ⊢
class Bif_F2_RIGHT : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F2_RIGHT()
   : NonscalarFunction(TOK_F2_RIGHT)
   {}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return Token(TOK_APL_VALUE1, B.clone(LOC)); }

   /// overloaded Function::eval_B()
   virtual Token eval_B(cValue_R B) const
      { return Token(TOK_APL_VALUE1, B.clone(LOC)); }

   /// overloaded Function::eval_AXB()
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   static Bif_F2_RIGHT  fun;   ///< Built-in function
protected:
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F2_LEFT_RIGHT_HH_DEFINED__
