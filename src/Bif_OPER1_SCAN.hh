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

//════════════════════════════════════════════════════════════════════════════
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
   /// @param where arity-prefix for )MORE text, e.g. "A\\B" or "A⍀[X]B"
   /// @param A    expansion vector
   /// @param B    APL value to expand
   /// @param axis axis along which to expand
   static Token expand(const char * where, cValue_R A, cValue_R B,
                       uAxis axis);

public:
   /// overloaded Function::get_selectivity(): dyadic A\B or A⍀B
   /// (Expand), with or without axis, both genuinely select -- see the
   /// matching comment on Bif_REDUCE::get_selectivity() (Bif_OPER1_REDUCE.hh):
   /// this is the plain dyadic-function role, not the unrelated Scan-
   /// operator role this same class also implements.
   virtual Fun_selectivity get_selectivity() const
      { return Fun_selectivity(SEL_DYA | SEL_DYA_X); }

   /// overloaded Function::has_dyadic_form(): see the matching override
   /// on Bif_REDUCE (Bif_OPER1_REDUCE.hh) -- \ and ⍀ ALSO double as the
   /// genuine plain dyadic function Expand.
   virtual bool has_dyadic_form() const   { return true; }
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
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return expand("A\\B", A, B, B.get_rank() - 1); }

   /// Overloaded Function::eval_LB().
   /// @param LO left operand function token
   /// @param B  right argument APL value
   virtual Token eval_LB(Token & LO, cValue_R B) const
      { return scan(LO, CLONE(&B, LOC), B.get_rank() - 1); }

   /// Overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// Overloaded Function::eval_LXB().
   /// @param LO left operand function token
   /// @param X  axis specification
   /// @param B  right argument APL value
   virtual Token eval_LXB(Token & LO, cValue_R X, cValue_R B) const;

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
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return expand("A⍀B", A, B, 0); }

   /// Overloaded Function::eval_ALB().
   /// @param LO left operand function token
   /// @param B  right argument APL value
   virtual Token eval_LB(Token & LO, cValue_R B) const
      { return scan(LO, CLONE(&B, LOC), 0); }

   /// Overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// Overloaded Function::eval_ALXB().
   /// @param LO left operand function token
   /// @param X  axis specification
   /// @param B  right argument APL value
   virtual Token eval_LXB(Token & LO, cValue_R X, cValue_R B) const;

   static Bif_OPER1_SCAN1  fun;     ///< Built-in function.

protected:
};
//════════════════════════════════════════════════════════════════════════════


#endif // __Bif_OPER1_SCAN_HH_DEFINED__

