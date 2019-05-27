/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2015  Dr. Jürgen Sauermann

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

#ifndef __BIF_OPER1_REDUCE_HH_DEFINED__
#define __BIF_OPER1_REDUCE_HH_DEFINED__

#include "PrimitiveOperator.hh"

//-----------------------------------------------------------------------------
/** Primitive operator reduce (common part for all reducr variants)
 */
/// Base class for / and ⌿
class Bif_REDUCE : public PrimitiveOperator
{
public:
   /// Constructor.
   Bif_REDUCE(TokenTag tag) : PrimitiveOperator(tag) {}

   /// common implementation of reduce() and reduce_n_wise.
   static Token do_reduce(const Shape & shape_Z, const Shape3 & Z3, ShapeItem a,
                          Function * LO, Value_P B, ShapeItem bm);

protected:
   /// Replicate B according to A along axis.
   Token replicate(Value_P A, Value_P B, uAxis axis);

   /// LO-reduce B along axis.
   Token reduce(Token & _LO, Value_P B, uAxis axis);

   /// LO-reduce B n-wise along axis.
   Token reduce_n_wise(Value_P A, Token & _LO, Value_P B, uAxis axis);

protected:
   /// overloaded Function::may_push_SI()
   virtual bool may_push_SI() const
      { return true; }
};
//-----------------------------------------------------------------------------
/** Primitive operator reduce along last axis.
 */
/// The class implementing /
class Bif_OPER1_REDUCE : public Bif_REDUCE
{
public:
   /// Constructor.
   Bif_OPER1_REDUCE() : Bif_REDUCE(TOK_OPER1_REDUCE) {}

   /// Overloaded Function::eval_AB().
   virtual Token eval_AB(Value_P A, Value_P B)
      { return replicate(A, B, B->get_rank() - 1); }

   /// Overloaded Function::eval_AXB().
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B);

   /// Overloaded Function::eval_LB().
   virtual Token eval_LB(Token & LO, Value_P B)
      { return reduce(LO, B, B->get_rank() - 1); }

   /// Overloaded Function::eval_ALB().
   virtual Token eval_ALB(Value_P A, Token & LO, Value_P B)
      { return reduce_n_wise(A, LO, B, B->get_rank() - 1); }

   /// Overloaded Function::eval_LXB().
   virtual Token eval_LXB(Token & LO, Value_P X, Value_P B);

   /// Overloaded Function::eval_ALXB().
   virtual Token eval_ALXB(Value_P A, Token & LO, Value_P X, Value_P B);

   static Bif_OPER1_REDUCE * fun;    ///< Built-in function.
   static Bif_OPER1_REDUCE  _fun;    ///< Built-in function.

protected:
};
//-----------------------------------------------------------------------------
/** Primitive operator reduce along first axis.
 */
/// The class implementing ⌿
class Bif_OPER1_REDUCE1 : public Bif_REDUCE
{
public:
   /// Constructor.
   Bif_OPER1_REDUCE1() : Bif_REDUCE(TOK_OPER1_REDUCE1) {}

   /// Overloaded Function::eval_AB().
   virtual Token eval_AB(Value_P A, Value_P B)
      { return replicate(A, B, 0); }

   /// Overloaded Function::eval_AXB().
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B);

   /// Overloaded Function::eval_LB().
   virtual Token eval_LB(Token & LO, Value_P B)
      { return reduce(LO, B, 0); }

   /// Overloaded Function::eval_ALB().
   virtual Token eval_ALB(Token & LO, Value_P B)
      { return reduce(LO, B, 0); }

   /// Overloaded Function::eval_ALB().
   virtual Token eval_ALB(Value_P A, Token & LO, Value_P B)
      { return reduce_n_wise(A, LO, B, 0); }

   /// Overloaded Function::eval_LXB().
   virtual Token eval_LXB(Token & LO, Value_P X, Value_P B);

   /// Overloaded Function::eval_ALXB().
   virtual Token eval_ALXB(Value_P A, Token & LO, Value_P X, Value_P B);

   static Bif_OPER1_REDUCE1 * fun;   ///< Built-in function.
   static Bif_OPER1_REDUCE1  _fun;   ///< Built-in function.

protected:
};
//-----------------------------------------------------------------------------

#endif // __BIF_OPER1_REDUCE_HH_DEFINED__
