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

#ifndef __BIF_F12_ROTATE_HH_DEFINED__
#define __BIF_F12_ROTATE_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** primitive functions rotate and reverse */
/// Base class for implementing ⌽ and ⊖
class Bif_ROTATE : public NonscalarFunction_default_identity
{
public:
   /// Constructor.
   /// @param tag  the token tag identifying this rotate/reverse function
   Bif_ROTATE(TokenTag tag)
   : NonscalarFunction_default_identity(tag)
   {}

   /// overloaded NonscalarFunction_default_identity::eval_identity_fun().
   /// Figure 28 (apl2lrm.txt p.212): the identity item for ⌽/⊖ is B's
   /// own prototype, one bare (not enclosed) item per position of the
   /// axes NOT being reduced -- rotating/reversing an empty (0-length)
   /// slice by any amount is a no-op, giving back that same empty slice,
   /// so per remaining-axis position the result is just B's prototype
   /// value -- same convention and same frame fix as ↓ (Bif_F12_DROP)
   /// above. The inherited default (a plain scalar, or DOMAIN ERROR for
   /// any non-empty frame) is wrong here: e.g. ⌽/3 0⍴0 used to raise
   /// DOMAIN ERROR instead of returning 3⍴0. See Bugs27 #27.
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      {
        const Shape shape_Z = B.get_shape().without_axis(axis);
        Cell cache;
        const Cell & proto = B.get_cproto(cache);

        Value_P Z(shape_Z, LOC);
        loop(z, shape_Z.get_volume())   Z->next_ravel_Cell(proto);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   /// overloaded Function::get_selectivity(): monadic ⌽/⊖ (Reverse) and
   /// dyadic ⌽/⊖ (Rotate), with or without axis, all 4 genuinely select
   /// (confirmed empirically for each of the 4 individually).
   virtual Fun_selectivity get_selectivity() const
      { return Fun_selectivity(SEL_MON | SEL_MON_X | SEL_DYA | SEL_DYA_X); }

protected:
   /// Reverse B along axis
   /// @param B     the right APL argument value (array to reverse)
   /// @param axis  the axis along which to reverse
   static Token reverse(cValue_R B, sAxis axis);

   /// Rotate B according to A along axis
   /// @param A     the left APL argument value (rotation amounts)
   /// @param B     the right APL argument value (array to rotate)
   /// @param axis  the axis along which to rotate
   static Token rotate(cValue_R A, cValue_R B, sAxis axis);
};
//────────────────────────────────────────────────────────────────────────────
/** primitive functions rotate and reverse along last axis */
/// The class implementing ⌽
class Bif_F12_ROTATE : public Bif_ROTATE
{
public:
   /// Constructor
   Bif_F12_ROTATE()
   : Bif_ROTATE(TOK_F12_ROTATE)
   {}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return rotate(A, B, B.get_rank() - 1); }

   /// overloaded Function::eval_B()
   virtual Token eval_B(cValue_R B) const
      { return reverse(B, B.get_rank() - 1); }

   /// overloaded Function::eval_AXB()
   /// @param A  the left APL argument value (rotation amounts)
   /// @param X  the axis specification value
   /// @param B  the right APL argument value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// overloaded Function::eval_XB()
   /// @param X  the axis specification value
   /// @param B  the right APL argument value
   virtual Token eval_XB(cValue_R X, cValue_R B) const;

   static Bif_F12_ROTATE  fun;   ///< Built-in function
protected:
};
//────────────────────────────────────────────────────────────────────────────
/** primitive functions rotate and reverse along first axis */
/// The class implementing ⊖
class Bif_F12_ROTATE1 : public Bif_ROTATE
{
public:
   /// Constructor
   Bif_F12_ROTATE1()
   : Bif_ROTATE(TOK_F12_ROTATE1)
   {}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return rotate(A, B, 0); }

   /// overloaded Function::eval_B()
   virtual Token eval_B(cValue_R B) const
      { return reverse(B, 0); }

   /// overloaded Function::eval_AXB()
   /// @param A  the left APL argument value (rotation amounts)
   /// @param X  the axis specification value
   /// @param B  the right APL argument value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// overloaded Function::eval_XB()
   /// @param X  the axis specification value
   /// @param B  the right APL argument value
   virtual Token eval_XB(cValue_R X, cValue_R B) const;

   static Bif_F12_ROTATE1  fun;   ///< Built-in function
protected:
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F12_ROTATE_HH_DEFINED__
