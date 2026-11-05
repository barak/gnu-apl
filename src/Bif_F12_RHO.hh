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

#ifndef __BIF_F12_RHO_HH_DEFINED__
#define __BIF_F12_RHO_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** primitive functions reshape and shape */
/// The class implementing ⍴
class Bif_F12_RHO : public NonscalarFunction_default_identity
{
public:
   /// Constructor
   Bif_F12_RHO()
   : NonscalarFunction_default_identity(TOK_F12_RHO)
   {}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_B()
   virtual Token eval_B(cValue_R B) const;

   /// overloaded NonscalarFunction_default_identity::eval_identity_fun().
   /// Figure 28 (apl2lrm.txt p.212): the identity item for ⍴ is ⊂B (the
   /// generic default-fill fallback this class normally inherits gives
   /// a bare prototype scalar instead, which is wrong here).
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return enclosed_identity(B, axis); }

   /// overloaded Function::get_selectivity(): dyadic ⍴ (Reshape)
   /// genuinely selects (confirmed against the LRM's own worked wrap-
   /// around example, apl2lrm.txt2:26612 ff.); monadic ⍴ (shape-of) does
   /// not -- it describes B, it does not select any item of it.
   virtual Fun_selectivity get_selectivity() const
      { return SEL_DYA; }

   /// Reshape B according to rank and shape
   static Token do_reshape(const Shape & shape, const cValue & B);

   static Bif_F12_RHO  fun;   ///< Built-in function
protected:
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F12_RHO_HH_DEFINED__
