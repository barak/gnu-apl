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

#ifndef __BIF_F12_ELEMENT_HH_DEFINED__
#define __BIF_F12_ELEMENT_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** primitive functions member and enlist */
/// The class implementing ϵ
class Bif_F12_ELEMENT : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_ELEMENT()
   : NonscalarFunction(TOK_F12_ELEMENT)
   {}

   /// overloaded Function::eval_B()
   /// @param B  the right APL argument value
   virtual Token eval_B(cValue_R B) const
      { return Token(TOK_APL_VALUE1, do_eval_B(B)); }

   /// overloaded Function::eval_AB()
   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// implementation of eval_B()
   /// @param B  the right APL value (raw pointer)
   static Value_P do_eval_B(cValue_R B);

   /// overloaded Function::get_selectivity(): monadic ∊ (Enlist)
   /// genuinely selects (it preserves B's nesting shape while replacing
   /// the enlisted values); dyadic ∊ (Membership) does not -- it
   /// computes an unrelated boolean result.
   virtual Fun_selectivity get_selectivity() const
      { return SEL_MON; }

   static Bif_F12_ELEMENT  fun;   ///< Built-in function

protected:
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F12_ELEMENT_HH_DEFINED__
