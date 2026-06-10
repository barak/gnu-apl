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
class Bif_F12_PARTITION : public NonscalarFunction_default_identity
{
public:
   /// Constructor
   Bif_F12_PARTITION()
   : NonscalarFunction_default_identity(TOK_F12_PARTITION)
   {}

   /// overloaded Function::eval_B()
   /// @param B right argument APL value
   virtual Token eval_B(Value_P B) const
      { return Token(TOK_APL_VALUE1, do_eval_B(B)); }

   /// implementation of eval_B()
   /// @param B right argument APL value
   static Value_P do_eval_B(Value_P B);

   /// overloaded Function::eval_AB()
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(Value_P A, Value_P B) const
      { return Token(TOK_APL_VALUE1, partition(A, B, B->get_rank() - 1)); }

   /// overloaded Function::eval_XB()
   /// @param X axis argument APL value
   /// @param B right argument APL value
   virtual Token eval_XB(Value_P X, Value_P B) const
      {
        X->to_bitmap("⊂[X] B", B->get_rank());   // check X
        const Shape shape_X = Value::to_shape(X.get());
        return Token(TOK_APL_VALUE1, enclose_with_axes(shape_X, B));
      }

   /// implementation of eval_XB()
   /// @param X axis argument APL value
   /// @param B right argument APL value
   static Value_P do_eval_XB(Value_P X, Value_P B);

   /// overloaded Function::eval_AXB()
   /// @param A left argument APL value
   /// @param X axis argument APL value
   /// @param B right argument APL value
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

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
   static Value_P partition(Value_P A, Value_P B, sAxis axis);
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
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B()
   /// @param B right argument APL value
   virtual Token eval_B(Value_P B) const
      { return Token(TOK_APL_VALUE1, disclose(B, true)); }

   /// ⊃B
   /// @param B right argument APL value
   /// @param rank_tolerant if true, allow rank mismatches when padding items
   static Value_P disclose(Value_P B, bool rank_tolerant);

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
   virtual Token eval_XB(Value_P X, Value_P B) const
      {
        const Shape sh_X = Value::to_shape(X.get());
        return Token(TOK_APL_VALUE1, disclose_with_axis(sh_X, B));
      }

   /// ⊃[X]B
   /// @param axes_X axes along which to disclose
   /// @param B right argument APL value
   static Value_P disclose_with_axis(const Shape & axes_X, Value_P B);

   static Bif_F12_PICK  fun;   ///< Built-in function

protected:
   /// the shape of the items being disclosed
   /// @param B right argument APL value
   /// @param rank_tolerant if true, allow rank mismatches when padding items
   static Shape compute_item_shape(Value_P B, bool rank_tolerant);

   /// Pick from B according to cA and len_A. \b cell_owner is non-zero
   /// if a left-vlues is picked, (e.g (2 1⊃B)←'TR')
   /// @param A0 pointer to the first cell of the index array A
   /// @param idx_A current index position in A
   /// @param len_A total number of index cells in A
   /// @param B right argument APL value
   /// @param qio current value of ⎕IO
   static Value_P pick(const Cell * const A0, ShapeItem idx_A, ShapeItem len_A,
                       const Value * B, APL_Integer qio);

   /// compute the offset of the Cell in B that shall be picked.
   /// @param A0 pointer to the first cell of the index array A
   /// @param idx_A current index position in A
   /// @param len_A total number of index cells in A
   /// @param B right argument APL value
   /// @param qio current value of ⎕IO
   static ShapeItem pick_offset(const Cell * const A0, ShapeItem idx_A,
                                ShapeItem len_A, const Value * B,
                                APL_Integer qio);
};
//════════════════════════════════════════════════════════════════════════════

#endif // __Bif_F12_PARTITION_PICK_HH_DEFINED__

