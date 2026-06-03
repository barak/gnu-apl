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

#ifndef __Bif_OPER1_SCAN_HH_DEFINED__
#define __Bif_OPER1_SCAN_HH_DEFINED__

#include "PrimitiveOperator.hh"

//────────────────────────────────────────────────────────────────────────────
/** Primitive operator scan.
 */
/// Base class for \ and ⍀
class Bif_SCAN : public PrimitiveOperator
{
public:
   /// Constructor.
   /// @param tag token tag identifying this operator variant
   Bif_SCAN(TokenTag tag) : PrimitiveOperator(tag) {}

protected:
   /// Compute the LO-scan of B.
   /// @param LO   left operand function token
   /// @param B    APL value to scan
   /// @param axis axis along which to scan
   Token scan(Token & LO, Value_P B, uAxis axis) const;

   /// Expand B according to A.
   /// @param A    expansion vector
   /// @param B    APL value to expand
   /// @param axis axis along which to expand
   static Token expand(Value_P A, Value_P B, uAxis axis);

   /// Compute one scan item and store result in Z.
   /// @param Z     result cell pointer
   /// @param LO    left operand function
   /// @param B     source cell pointer
   /// @param m_len middle dimension length
   /// @param l_len last dimension length
   static void scan_item(Cell * Z, cFunction_P LO, const Cell * B,
                         uint32_t m_len, uint32_t l_len);
};
//────────────────────────────────────────────────────────────────────────────
/** Primitive operator \ (scan along last axis)
 */
/// The class implementing \.
class Bif_OPER1_SCAN : public Bif_SCAN
{
public:
   /// Constructor.
   Bif_OPER1_SCAN() : Bif_SCAN(TOK_OPER1_SCAN) {}

   /// Overloaded Function::eval_AB().
   /// @param A left argument APL value (expansion vector)
   /// @param B right argument APL value
   virtual Token eval_AB(Value_P A, Value_P B) const
      { return expand(A, B, B->get_rank() - 1); }

   /// Overloaded Function::eval_LB().
   /// @param LO left operand function token
   /// @param B  right argument APL value
   virtual Token eval_LB(Token & LO, Value_P B) const
      { return scan(LO, B, B->get_rank() - 1); }

   /// Overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification
   /// @param B right argument APL value
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// Overloaded Function::eval_LXB().
   /// @param LO left operand function token
   /// @param X  axis specification
   /// @param B  right argument APL value
   virtual Token eval_LXB(Token & LO, Value_P X, Value_P B) const;

   static Bif_OPER1_SCAN  fun;      ///< Built-in function.

protected:
   /// overloaded Function::may_push_SI()
   virtual bool may_push_SI() const
      { return false; }

};
//────────────────────────────────────────────────────────────────────────────
/** Primitive operator ⍀ (scan along first axis)
 */
/// The class implementing ⍀
class Bif_OPER1_SCAN1 : public Bif_SCAN
{
public:
   /// Constructor.
   Bif_OPER1_SCAN1() : Bif_SCAN(TOK_OPER1_SCAN1) {}

   /// Overloaded Function::eval_AB().
   /// @param A left argument APL value (expansion vector)
   /// @param B right argument APL value
   virtual Token eval_AB(Value_P A, Value_P B) const
      { return expand(A, B, 0); }

   /// Overloaded Function::eval_ALB().
   /// @param LO left operand function token
   /// @param B  right argument APL value
   virtual Token eval_LB(Token & LO, Value_P B) const
      { return scan(LO, B, 0); }

   /// Overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification
   /// @param B right argument APL value
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// Overloaded Function::eval_ALXB().
   /// @param LO left operand function token
   /// @param X  axis specification
   /// @param B  right argument APL value
   virtual Token eval_LXB(Token & LO, Value_P X, Value_P B) const;

   static Bif_OPER1_SCAN1  fun;     ///< Built-in function.

protected:
};
//────────────────────────────────────────────────────────────────────────────


#endif // __Bif_OPER1_SCAN_HH_DEFINED__

