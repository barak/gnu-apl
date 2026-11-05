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

#ifndef __BIF_F2_INDEX_HH_DEFINED__
#define __BIF_F2_INDEX_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** System function index (⌷) */
/// The class implementing ⌷
class Bif_F2_INDEX : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F2_INDEX()
   : NonscalarFunction(TOK_F2_INDEX)
   {}

   /// overloaded Function::eval_AB()
   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_AXB()
   /// @param A  the left APL argument value
   /// @param X  the axis specification value
   /// @param B  the right APL argument value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// overloaded Function::get_selectivity(): dyadic ⌷ (Index/squad),
   /// with or without axis, both genuinely select (confirmed
   /// empirically for each individually); there is no monadic ⌷.
   virtual Fun_selectivity get_selectivity() const
      { return Fun_selectivity(SEL_DYA | SEL_DYA_X); }

   /// overloaded Function::has_monadic_form(): there is no monadic ⌷ at
   /// all (only eval_AB/eval_AXB are implemented) -- see that method's
   /// doc comment for why this must be reported explicitly.
   virtual bool has_monadic_form() const   { return false; }

   static Bif_F2_INDEX  fun;   ///< Built-in function
protected:
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F2_INDEX_HH_DEFINED__
