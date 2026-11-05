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

#ifndef __BIF_F12_TRANSPOSE_HH_DEFINED__
#define __BIF_F12_TRANSPOSE_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** System function transpose */
/// The class implementing ⍉
class Bif_F12_TRANSPOSE : public NonscalarFunction_default_identity
{
public:
   /// Constructor
   Bif_F12_TRANSPOSE()
   : NonscalarFunction_default_identity(TOK_F12_TRANSPOSE)
   {}

   /// overloaded Function::eval_B()
   /// @param B  the right APL argument value
   virtual Token eval_B(cValue_R B) const
      { return do_eval_B(B); }

   /// overloaded Function::eval_AB()
   /// @param A  the left APL argument value (axis permutation vector)
   /// @param B  the right APL argument value (array to transpose)
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded NonscalarFunction_default_identity::eval_identity_fun().
   /// Figure 28 (apl2lrm.txt p.212): the identity item for ⍉ is ⊂B.
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return enclosed_identity(B, axis); }

   /// overloaded Function::get_selectivity(): monadic ⍉ (reversed axes)
   /// and dyadic ⍉ (general transpose) both genuinely select; ⍉ has no
   /// bracket-axis variant.
   virtual Fun_selectivity get_selectivity() const
      { return Fun_selectivity(SEL_MON | SEL_DYA); }

   /// implementation of eval_B()
   /// @param B  the APL array to reverse-transpose (raw pointer)
   static Token do_eval_B(cValue_R B);

   /// Transpose B according to axes A (without diagonals)
   /// @param A  the permutation shape specifying new axis order
   /// @param B  the APL array to transpose (raw pointer)
   static Value_P transpose(const Shape & A, cValue_R B);

   static Bif_F12_TRANSPOSE  fun;   ///< Built-in function

protected:
   /// for \b sh being a permutation of 0, 1, ... rank - 1,
   /// return the inverse permutation sh⁻¹
   /// @param sh  the permutation shape to invert
   static Shape inverse_permutation(const Shape & sh);

   /// return sh permuted according to permutation perm
   /// @param sh    the shape to permute
   /// @param perm  the permutation to apply
   static Shape permute(const Shape & sh, const Shape & perm);

   /// Transpose B according to axes A (with diagonals)
   /// @param A  the permutation shape (may map multiple axes to one)
   /// @param B  the APL array to transpose (raw pointer)
   static Value_P transpose_diag(const Shape & A, cValue_R B);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F12_TRANSPOSE_HH_DEFINED__
