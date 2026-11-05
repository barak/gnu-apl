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

#ifndef __PRIMITIVE_FUNCTION_HH_DEFINED__
#define __PRIMITIVE_FUNCTION_HH_DEFINED__

#include "Common.hh"
#include "Function.hh"
#include "Performance.hh"
#include "Value.hh"
#include "Id.hh"

//════════════════════════════════════════════════════════════════════════════
/**
    Base class for the APL system functions (Quad functions and primitives
    like +, -, ...) and operators

    The individual system functions are derived from this class
 */
/// Base class for all internal functions of the interpreter
class PrimitiveFunction : public Function
{
public:
   /// Construct a PrimitiveFunction with \b TokenTag \b tag
   /// @param tag      the token tag identifying this primitive
   /// @param stat_AB  optional dyadic cell-function statistics object
   /// @param stat_B   optional monadic cell-function statistics object
   PrimitiveFunction(TokenTag tag,
                     CellFunctionStatistics * stat_AB = 0,
                     CellFunctionStatistics * stat_B = 0)
   : Function(tag),
       statistics_AB(stat_AB),
       statistics_B(stat_B)
   {}

   /// return the dyadic cell statistics of \b this (scalar) function
   CellFunctionStatistics *
   get_statistics_AB() const   { return statistics_AB; }

   /// return the monadic cell statistics of \b this (scalar) function
   CellFunctionStatistics *
   get_statistics_B() const   { return statistics_B; }

   /// overloaded Function::has_result()
   virtual bool has_result() const   { return true; }

   /// overloaded Function::eval_fill_AB()
   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   virtual Token eval_fill_AB(cValue_R A, cValue_R B) const
      { return eval_AB(A, B); }

protected:
   /// overloaded Function::eval_fill_B()
   /// @param B  the right APL argument value
   virtual Token eval_fill_B(cValue_R B) const
      { return eval_B(B); }

   /// Print the name of \b this PrimitiveFunction to \b out
   /// @param out  output stream to print the function name to
   virtual ostream & print(ostream & out) const
      { return out << get_Id(); }

   /// overloaded Function::print_properties()
   /// @param out     output stream for property display
   /// @param indent  indentation level for nested output
   virtual void print_properties(ostream & out, int indent) const
      {
        UCS_string ind(indent, UNI_SPACE);
        out << ind << "System Function ";
        print(out);
        out << endl;
      }

   /// performance statistics for eval_B()
   CellFunctionStatistics * statistics_AB;

   /// performance statistics for dyadic calls
   CellFunctionStatistics * statistics_B;
};
//────────────────────────────────────────────────────────────────────────────
/// Base class for all internal non-scalar functions of the interpreter
class NonscalarFunction : public PrimitiveFunction
{
public:
   /// Constructor
   /// @param tag  the token tag identifying this non-scalar function
   NonscalarFunction(TokenTag tag)
   : PrimitiveFunction(tag)
   {}
};
//────────────────────────────────────────────────────────────────────────────
/// Base class for all internal non-scalar functions of the interpreter
// that have the default identity function
class NonscalarFunction_default_identity : public NonscalarFunction
{
public:
   /// Constructor
   /// @param tag  the token tag identifying this function
   NonscalarFunction_default_identity(TokenTag tag)
   : NonscalarFunction(tag)
   {}

   /// Overloaded Function::eval_identity_fun()
   /// @param B     the right APL argument value
   /// @param axis  the axis along which to apply the identity
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      {
        // axis is already normalized to IO←0
        // return Z←,/B0 where (B0 , B) is B.

        const Shape & shape_B = B.get_shape();
        const sRank rank_B = shape_B.get_rank();
        if (rank_B < 1)       RANK_ERROR;   // identity restriction, lrm p. 212
        if (axis >= rank_B)   RANK_ERROR;

        // eval_identity_fun() is supposedly only called when the length of
        // axis is 0
        const ShapeItem axis_len = shape_B.get_shape_item(axis);
        Assert(axis_len == 0);

        const Shape shape_Z = shape_B.without_axis(axis);

        /* the removal of the reduction axis must not create a non-empty
           result.

           In IBM APL2:

                        ┌───── reduction axis
                        │
                 ⍴ ,/ 0 0⍴42   → 0
                 ⍴ ,/ 0 3⍴42   → 0 (not happening here since axis_len == 0)
                 ⍴ ,/ 3 0⍴42   → DOMAIN ERROR (volume would be 3)
                 ⍴ ,/   0⍴42   → DOMAIN ERROR (volume would be 1)
           but:  ⍴ +/   0⍴42   → ⍬   and +/   0⍴42   → 0 (integer scalar 0)
                 ⍴ ×/   0⍴42   → ⍬   and +/   0⍴42   → 1 (integer scalar 1)
         */
        if (shape_Z.get_rank() && shape_Z.get_volume() > 0)   DOMAIN_ERROR;

        Value_P Z(shape_Z, LOC);
        Z->set_default(B, LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   /// implementation of eval_identity_fun(), so that non-derived functions
   /// may use it as well.
   /// @param B     the right APL argument value
   /// @param axis  the axis along which to apply the identity
   static Token do_eval_identity_fun(cValue_R B, sAxis axis);

   /// shared helper for the several Figure 28 (apl2lrm.txt p.212)
   /// identity items that are simply ⊂B (Reshape, Transpose, Take,
   /// Catenate) -- B is guaranteed non-scalar here (eval_identity_fun()
   /// is only ever called for an axis of length 0, which requires B to
   /// have at least that one axis), so no "enclose of a simple scalar
   /// is identity" special-casing is needed.
   ///
   /// This used to ignore the frame entirely and always return a bare
   /// scalar ⊂B, e.g. giving ,/3 0⍴0 a single enclosed item instead of a
   /// 3-item vector (one identity item per position of the axes NOT
   /// being reduced) -- reduction along an empty axis must give the
   /// identity item at EVERY position of the remaining axes (⍴Z ←→ (⍴B)
   /// without the axis), not once overall. Since axis has 0 elements
   /// regardless of the remaining axes' position, the 1-D slice along
   /// axis at every position is the SAME empty, B-typed vector -- so
   /// every item of Z is ⊂ of that one empty vector, repeated shape_Z
   /// times. See Bugs27 #27.
   /// @param B     the right APL argument value
   /// @param axis  the (0-length) axis being reduced
   /// @param item  the (already-enclosed) identity value for a single
   ///              position -- callers differ on what this is (plain
   ///              ⊂B-typed-empty-vector for ,/⍴/⍉/↑, a 0×0 matrix for
   ///              ⌹), only the repeat-across-the-frame part is shared
   static Token enclosed_identity(cValue_R B, sAxis axis, Value_P item)
      {
        const Shape shape_Z = B.get_shape().without_axis(axis);

        Value_P Z(shape_Z, LOC);
        loop(z, shape_Z.get_volume())
            Z->next_ravel_Pointer(CLONE(item.get(), LOC).get());

        // shape_Z can itself have a 0-length axis (e.g. ,/0 0⍴0: axis 1
        // has length 0, so shape_Z is (0) and the loop above never runs),
        // leaving Z with no explicit prototype -- item is what every real
        // position WOULD have been, so it is Z's prototype too, the same
        // way the loop above would have set it (Blake McBride, Bugs28
        // #68). Without this, Z's prototype defaulted to a bare untyped
        // scalar (≡Z 1, Z←0⍴0) instead of ≡Z 2, Z←0⍴⊂⍬.
        //
        if (Z->is_empty())
           new (&Z->get_wproto()) PointerCell(CLONE(item.get(), LOC).get(),
                                              *Z);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   /// convenience overload of enclosed_identity() above for the common
   /// case (,/⍴/⍉/↑): the per-position item is ⊂ of an empty, B-typed
   /// vector (see the general comment above).
   /// @param B     the right APL argument value
   /// @param axis  the (0-length) axis being reduced
   static Token enclosed_identity(cValue_R B, sAxis axis)
      {
        Value_P item(Shape(ShapeItem(0)), LOC);   // empty, B-typed vector
        item->set_default(B, LOC);
        item->check_value(LOC);
        return enclosed_identity(B, axis, item);
      }
};
//════════════════════════════════════════════════════════════════════════════

#endif // __PRIMITIVE_FUNCTION_HH_DEFINED__
