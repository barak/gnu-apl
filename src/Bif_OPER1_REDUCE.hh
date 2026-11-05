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

#ifndef __BIF_OPER1_REDUCE_HH_DEFINED__
#define __BIF_OPER1_REDUCE_HH_DEFINED__

#include "PrimitiveOperator.hh"

//════════════════════════════════════════════════════════════════════════════
/** Primitive operator reduce (common part for all reducr variants)
 */
/// Base class for / and ⌿
class Bif_REDUCE : public PrimitiveOperator
{
public:
   /// Constructor.
   /// @param tag token tag identifying this operator variant
   Bif_REDUCE(TokenTag tag) : PrimitiveOperator(tag) {}

   bool has_result() const
      { return true; }

   /// common implementation of reduce() and reduce_n_wise.
   /// @param shape_Z result shape
   /// @param Z3 three-part shape decomposition around the reduce axis
   /// @param a axis index being reduced
   /// @param LO left-operand function applied during reduction
   /// @param B right-argument APL value
   /// @param bm number of elements along the reduced axis
   static Token do_reduce(const Shape & shape_Z, const Shape3 & Z3,
                          ShapeItem a, cFunction_P LO, Value_P B, ShapeItem bm);

   /// LO-reduce B along axis.
   /// @param LO left-operand function applied during reduction
   /// @param B right-argument APL value
   /// @param axis axis along which to reduce
   static Token reduce(Token & LO, Value_P B, uAxis axis);

   /// overloaded Function::get_selectivity(): dyadic A/B or A⌿B
   /// (Replicate), with or without axis, both genuinely select -- LRM
   /// Figure 6's "Derived Functions" row for / and ⌿ (ch03) is this
   /// plain dyadic-function role (A a simple integer array), not the
   /// unrelated Reduce-operator role this same class also implements
   /// (LO a function; may_push_SI() above is already false for that
   /// role and would reject a defined LO regardless).
   virtual Fun_selectivity get_selectivity() const
      { return Fun_selectivity(SEL_DYA | SEL_DYA_X); }

   /// overloaded Function::has_dyadic_form(): PrimitiveOperator defaults
   /// this to false (a bare, unbound operator symbol), but / and ⌿ ALSO
   /// double as the genuine plain dyadic function Replicate (eval_AB()
   /// above is a real implementation, not phrase_error()); there is no
   /// analogous monadic role, so has_monadic_form() stays false.
   virtual bool has_dyadic_form() const   { return true; }

protected:
   /// overloaded Function::may_push_SI()
   virtual bool may_push_SI() const
      { return false; }

   /// LO-reduce B n-wise along axis.
   /// @param A left-argument window size
   /// @param LO left-operand function applied during reduction
   /// @param B right-argument APL value
   /// @param axis axis along which to reduce n-wise
   Token reduce_n_wise(Value_P A, Token & LO, Value_P B, uAxis axis) const;

   /// Replicate B according to A along axis.
   /// @param A left-argument replication counts
   /// @param B right-argument APL value
   /// @param axis axis along which to replicate
   static Token replicate(cValue_R A, cValue_R B, uAxis axis);

};
//────────────────────────────────────────────────────────────────────────────
/** Primitive operator reduce along last axis.
 */
/// The class implementing /
class Bif_OPER1_REDUCE : public Bif_REDUCE
{
public:
   /// Constructor.
   Bif_OPER1_REDUCE() : Bif_REDUCE(TOK_OPER1_REDUCE) {}

   /// Overloaded Function::eval_AB().
   /// @param A left-argument replication counts
   /// @param B right-argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return replicate(A, B, B.get_rank() - 1); }

   /// Overloaded Function::eval_ALB().
   /// @param A left-argument window size
   /// @param LO left-operand function
   /// @param B right-argument APL value
   virtual Token eval_ALB(cValue_R A, Token & LO, cValue_R B) const
      { return reduce_n_wise(CLONE(&A, LOC), LO, CLONE(&B, LOC), B.get_rank() - 1); }

   /// Overloaded Function::eval_LB().
   /// @param LO left-operand function
   /// @param B right-argument APL value
   virtual Token eval_LB(Token & LO, cValue_R B) const
      { return reduce(LO, CLONE(&B, LOC), B.get_rank() - 1); }

   /// Overloaded Function::eval_ALXB().
   /// @param A left-argument window size
   /// @param LO left-operand function
   /// @param X axis specification
   /// @param B right-argument APL value
   virtual Token eval_ALXB(cValue_R A, Token & LO, cValue_R X, cValue_R B) const;

   /// Overloaded Function::eval_AXB().
   /// @param A left-argument replication counts
   /// @param X axis specification
   /// @param B right-argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// Overloaded Function::eval_LXB().
   /// @param LO left-operand function
   /// @param X axis specification
   /// @param B right-argument APL value
   virtual Token eval_LXB(Token & LO, cValue_R X, cValue_R B) const;

   static Bif_OPER1_REDUCE  fun;    ///< Built-in function.

protected:
};
//────────────────────────────────────────────────────────────────────────────
/** Primitive operator reduce along first axis.
 */
/// The class implementing ⌿
class Bif_OPER1_REDUCE1 : public Bif_REDUCE
{
public:
   /// Constructor.
   Bif_OPER1_REDUCE1() : Bif_REDUCE(TOK_OPER1_REDUCE1) {}

   /// Overloaded Function::eval_AB().
   /// @param A left-argument replication counts
   /// @param B right-argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return replicate(A, B, 0); }

   /// Overloaded Function::eval_ALB().
   /// @param A left-argument window size
   /// @param LO left-operand function
   /// @param B right-argument APL value
   virtual Token eval_ALB(cValue_R A, Token & LO, cValue_R B) const
      { return reduce_n_wise(CLONE(&A, LOC), LO, CLONE(&B, LOC), 0); }

   /// Overloaded Function::eval_LB().
   /// @param LO left-operand function
   /// @param B right-argument APL value
   virtual Token eval_LB(Token & LO, cValue_R B) const
      { return reduce(LO, CLONE(&B, LOC), 0); }

   /// Overloaded Function::eval_ALXB().
   /// @param A left-argument window size
   /// @param LO left-operand function
   /// @param X axis specification
   /// @param B right-argument APL value
   virtual Token eval_ALXB(cValue_R A, Token & LO, cValue_R X, cValue_R B) const;

   /// Overloaded Function::eval_AXB().
   /// @param A left-argument replication counts
   /// @param X axis specification
   /// @param B right-argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// Overloaded Function::eval_LXB().
   /// @param LO left-operand function
   /// @param X axis specification
   /// @param B right-argument APL value
   virtual Token eval_LXB(Token & LO, cValue_R X, cValue_R B) const;

   static Bif_OPER1_REDUCE1  fun;   ///< Built-in function.

protected:
};
//════════════════════════════════════════════════════════════════════════════

#endif // __BIF_OPER1_REDUCE_HH_DEFINED__
