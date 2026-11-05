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

#ifndef __Bif_F12_PARTITION_PICK_HH_DEFINED__
#define __Bif_F12_PARTITION_PICK_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//════════════════════════════════════════════════════════════════════════════
/** primitive functions partition and enclose */
/// The class implementing ⊂
class Bif_F12_PARTITION : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_PARTITION()
   : NonscalarFunction(TOK_F12_PARTITION)
   {}

   /// overloaded Function::eval_B()
   /// @param B right argument APL value
   virtual Token eval_B(cValue_R B) const
      { return Token(TOK_APL_VALUE1, do_eval_B(B)); }

   /// implementation of eval_B()
   /// @param B right argument APL value
   static Value_P do_eval_B(cValue_R B);

   /// overloaded Function::eval_AB()
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return Token(TOK_APL_VALUE1, partition(A, B, B.get_rank() - 1)); }

   /// overloaded Function::eval_XB()
   /// @param X axis argument APL value
   /// @param B right argument APL value
   virtual Token eval_XB(cValue_R X, cValue_R B) const
      {
        X.to_bitmap("⊂[X]B", B.get_rank());   // check X
        const Shape shape_X = Value::to_shape(&X);
        return Token(TOK_APL_VALUE1, enclose_with_axes(shape_X, CLONE(&B, LOC)));
      }

   /// implementation of eval_XB()
   /// @param X axis argument APL value
   /// @param B right argument APL value
   static Value_P do_eval_XB(cValue_R X, cValue_R B);

   /// overloaded Function::eval_AXB()
   /// @param A left argument APL value
   /// @param X axis argument APL value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// overloaded Function::get_selectivity(): monadic ⊂ (Enclose)
   /// genuinely selects -- (⊂R)←N scalar-extends a conforming N across
   /// every item of R and rejects a non-conforming N, confirmed
   /// empirically, even though the LRM's own Figure 6 selective-
   /// specification table omits it. Dyadic ⊂ (Partition) does not: it
   /// can drop items of B entirely (a 0 in A) and vary the number of
   /// result items, so it has no fixed, position-preserving selection
   /// to offer. Enclose-with-axis (⊂[X]) is NOT claimed: an attempt to
   /// verify it empirically gave RANK ERROR with the RHS shape tried,
   /// inconclusive rather than confirmed either way -- left as SEL_NONE
   /// for that bit rather than guess.
   virtual Fun_selectivity get_selectivity() const
      { return SEL_MON; }

   static Bif_F12_PARTITION  fun;   ///< Built-in function

   /// enclose_with_axes
   /// @param shape_X axes along which to enclose
   /// @param B right argument APL value
   static Value_P enclose_with_axes(const Shape & shape_X, Value_P B);

protected:
   /// one partition (along an axis of B)
   struct Partition
      {
        ShapeItem start;   ///< the start position on the B-axis (including)
        ShapeItem end;     ///< the end position on the B-axis (excluding)

        /// the number of items in \b this partition
        ShapeItem length() const    { return end - start; }
      };

   /// enclose B with axis
   /// @param B right argument APL value
   /// @param X axis argument APL value
   static Token enclose_with_axis(Value_P B, Value_P X);

   /// Partition B according to A
   /// @param A left argument APL value (partition mask)
   /// @param B right argument APL value
   /// @param axis axis along which to partition
   static Value_P partition(cValue_R A, cValue_R B, sAxis axis);
};
//════════════════════════════════════════════════════════════════════════════
/** primitive functions pick and disclose */
/// The class implementing ⊃
class Bif_F12_PICK : public NonscalarFunction
{  
public:
   /// Constructor
   Bif_F12_PICK()
   : NonscalarFunction(TOK_F12_PICK) 
   {}

   /// overloaded Function::eval_AB()
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_B()
   /// @param B right argument APL value
   virtual Token eval_B(cValue_R B) const
      { return Token(TOK_APL_VALUE1, disclose(B, true)); }

   /// overloaded Function::eval_identity_fun(). Figure 28 (apl2lrm.txt
   /// p.212) literally gives the identity FUNCTION for ⊃ as ⍳0, with the
   /// table's own Z←SR⍴⊂.... header applying: the actual identity value
   /// is SR⍴⊂⍳0 (SR = the frame shape, i.e. B's shape without axis) --
   /// one enclosed empty vector PER remaining position, the same
   /// per-frame-position broadcast every other Figure 28 identity uses
   /// (see enclosed_identity() above). This used to return CLONE(&B) --
   /// B entirely unchanged, rank and all -- based on conflating this
   /// table entry with a DIFFERENT LRM fact (p.42: dyadic ⍳0⊃B ≡ B, the
   /// empty-left-argument behavior of Pick itself, not the Figure 28
   /// reduce-identity). See Bugs27 #27.
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      {
        return NonscalarFunction_default_identity::enclosed_identity
                  (B, axis, Idx0(LOC));
      }

   /// ⊃B
   /// @param B right argument APL value
   /// @param rank_tolerant if true, allow rank mismatches when padding items
   static Value_P disclose(cValue_R B, bool rank_tolerant);

   /// create a copy of B_item, pad as needed to have item_shape, and
   /// store it in Z, starteding at Z_start.
   /// @param Z result value being built
   /// @param b ravel index of the item in B
   /// @param item_shape target shape for each disclosed item
   /// @param item_len number of cells in item_shape
   /// @param B_item the cell from B to disclose
   static void disclose_item(Value & Z, ShapeItem b,
                             const Shape & item_shape, ShapeItem item_len,
                             const Cell & B_item);

   /// overloaded Function::eval_XB()
   /// @param X axis argument APL value
   /// @param B right argument APL value
   virtual Token eval_XB(cValue_R X, cValue_R B) const
      { return Token(TOK_APL_VALUE1, disclose_with_axis(X, B)); }

   /// ⊃[X]B
   /// @param X axes along which to disclose
   /// @param B right argument APL value
   static Value_P disclose_with_axis(cValue_R X, cValue_R B);

   /// overloaded Function::get_selectivity(): monadic ⊃ (Disclose),
   /// with or without axis, genuinely selects (confirmed empirically
   /// for each individually), even though the LRM's own Figure 6
   /// selective-specification table omits it entirely; dyadic ⊃ (Pick)
   /// genuinely selects too -- pick()'s own doc-comment already gives
   /// (2 1⊃B)←'TR' as a worked lvalue example. Pick has no axis-bracket
   /// form.
   virtual Fun_selectivity get_selectivity() const
      { return Fun_selectivity(SEL_MON | SEL_MON_X | SEL_DYA); }

   static Bif_F12_PICK  fun;   ///< Built-in function

protected:
   /// the shape of the items being disclosed
   /// @param B right argument APL value
   /// @param rank_tolerant if true, allow rank mismatches when padding items
   static Shape compute_item_shape(cValue_R B, bool rank_tolerant);

   /// Pick from B according to cA and len_A. \b cell_owner is non-zero
   /// if a left-vlues is picked, (e.g (2 1⊃B)←'TR')
   /// @param A0 pointer to the first cell of the index array A
   /// @param idx_A current index position in A
   /// @param len_A total number of index cells in A
   /// @param B right argument APL value
   /// @param qio current value of ⎕IO
   static Value_P pick(cValue_R A, ShapeItem idx_A, ShapeItem len_A,
                       cValue_R B, APL_Integer qio);

   /// compute the offset of the Cell in B that shall be picked.
   /// @param A0 pointer to the first cell of the index array A
   /// @param idx_A current index position in A
   /// @param len_A total number of index cells in A
   /// @param B right argument APL value
   /// @param qio current value of ⎕IO
   static ShapeItem pick_offset(cValue_R A, ShapeItem idx_A,
                                ShapeItem len_A, cValue_R B,
                                APL_Integer qio);
};
//════════════════════════════════════════════════════════════════════════════

#endif // __Bif_F12_PARTITION_PICK_HH_DEFINED__

