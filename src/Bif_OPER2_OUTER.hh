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

#ifndef __BIF_OPER2_OUTER_HH_DEFINED__
#define __BIF_OPER2_OUTER_HH_DEFINED__

#include "PrimitiveOperator.hh"

//════════════════════════════════════════════════════════════════════════════
/**
   A dummy function for the product operator.
 **/
/// The class implementing ∘
class Bif_JOT : public PrimitiveFunction
{
public:
   /// Constructor.
   Bif_JOT() : PrimitiveFunction(TOK_JOT) {}
 
   /// overloaded Function::eval_AB().
   /// @param A  left value argument
   /// @param B  right value argument
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   static Bif_JOT  fun;             ///< Built-in function.
 
protected:
   /// overloaded Function::may_push_SI()
   virtual bool may_push_SI() const
      { return false; }

public:
   /// overloaded Function::has_monadic_form(): ∘ has no eval_B() (only
   /// eval_AB(), used when ∘.F -- an outer-product token sequence, not
   /// this dummy function called on its own -- gets misparsed as a bare
   /// jot elsewhere); calling it monadically always falls to
   /// Function::eval_B()'s phrase_error() default, which never touches
   /// B, so it must be let through to that natural VALENCE ERROR rather
   /// than intercepted by a selective-specification guard.
   virtual bool has_monadic_form() const   { return false; }
};
//════════════════════════════════════════════════════════════════════════════
/** Primitive operator outer product.
 */
/// The class implementing ∘.g
class Bif_OPER2_OUTER : public PrimitiveOperator
{
public:
   /// Constructor.
   Bif_OPER2_OUTER() : PrimitiveOperator(TOK_OPER2_OUTER) {}

   /// Overloaded Function::eval_ALRB().
   /// @param A   left value argument
   /// @param LO  left operator function argument (∘ jot)
   /// @param RO  right operator function argument
   /// @param B   right value argument
   virtual Token eval_ALRB(cValue_R A, Token & LO, Token & RO, cValue_R B) const;

   static Bif_OPER2_OUTER  fun;   ///< Built-in function.

protected:
   /// the context for an outer product
   struct PJob_product
      {
        Value * VZ;             ///< result value
        ShapeItem idxZ;         ///< result base index
        const cValue * VA;      ///< left value argument
        ShapeItem idxA;         ///< left argument base index
        int incA;               ///< left argument increment (for scalar extension)
        ShapeItem ZAh;          ///< high dimensions of result length
        prim_f2 RO;             ///<< right function argument
        const cValue * VB;      ///< right value argument
        ShapeItem idxB;         ///< right argument base index
        int incB;               ///< right argument increment (for scalar extension)
        ShapeItem ZBl;          ///< low dimensions of result length
        ErrorCode ec;           ///< error code
        CoreCount cores;        ///< number of cores to be used
      };

   /// outer product for scalar RO
   inline void scalar_outer_product() const;

   /// the main loop for an outer product with scalar functions
   /// @param tctx  thread execution context
   static void PF_scalar_outer_product(Thread_context & tctx);

   /// the context for an outer product
   static PJob_product job;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __BIF_OPER2_OUTER_HH_DEFINED__
