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

#ifndef __BIF_F12_EQUIV_HH_DEFINED__
#define __BIF_F12_EQUIV_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** primitive functions match and depth */
/// The class implementing ≡
class Bif_F12_EQUIV : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_EQUIV()
   : NonscalarFunction(TOK_F12_EQUIV)
   {}

   /// overloaded Function::eval_AB() : A ≡ B
   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return Token(TOK_APL_VALUE1,
                     IntScalar((do_eval_AB(A, B) ? 1 : 0), LOC)); }

   /// overloaded Function::eval_B() : B
   /// @param B  the right APL argument value
   virtual Token eval_B(cValue_R B) const;

   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   static bool do_eval_AB(cValue_R A, cValue_R B);

   static Bif_F12_EQUIV  fun;   ///< Built-in function

protected:
   /// return the depth of B
   /// @param B  the APL value whose depth to compute
   Token depth(Value_P B);
};
//────────────────────────────────────────────────────────────────────────────
/** primitive function natch (≢) */
/// The class implementing ≡
class Bif_F12_NEQUIV : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_NEQUIV()
   : NonscalarFunction(TOK_F12_NEQUIV)
   {}

   /// overloaded Function::eval_AB()
   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_B()
   /// @param B  the right APL argument value
   virtual Token eval_B(cValue_R B) const;

   static Bif_F12_NEQUIV  fun;   ///< Built-in function
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F12_EQUIV_HH_DEFINED__
