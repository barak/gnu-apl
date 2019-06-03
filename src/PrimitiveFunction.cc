/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2019  Dr. Jürgen Sauermann

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

#include <string.h>
#include <stdio.h>

#include "ArrayIterator.hh"
#include "Avec.hh"
#include "Bif_F12_SORT.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Bif_OPER1_EACH.hh"
#include "Bif_OPER1_REDUCE.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "Command.hh"
#include "FloatCell.hh"
#include "Id.hh"
#include "IndexExpr.hh"
#include "IndexIterator.hh"
#include "IntCell.hh"
#include "LvalCell.hh"
#include "Macro.hh"
#include "Output.hh"
#include "PointerCell.hh"
#include "PrimitiveFunction.hh"
#include "PrintOperator.hh"
#include "StateIndicator.hh"
#include "UserFunction.hh"
#include "Value.hh"
#include "Workspace.hh"

// primitive function instances
//
Bif_F0_ZILDE      Bif_F0_ZILDE     ::_fun;    // ⍬
Bif_F1_EXECUTE    Bif_F1_EXECUTE   ::_fun;    // ⍎
Bif_F2_INDEX      Bif_F2_INDEX     ::_fun;    // ⌷
Bif_F12_PARTITION Bif_F12_PARTITION::_fun;    // ⊂
Bif_F12_PICK      Bif_F12_PICK     ::_fun;    // ⊃
Bif_F12_COMMA     Bif_F12_COMMA    ::_fun;    // ,
Bif_F12_COMMA1    Bif_F12_COMMA1   ::_fun;    // ⍪
Bif_F12_ELEMENT   Bif_F12_ELEMENT  ::_fun;    // ∈
Bif_F12_EQUIV     Bif_F12_EQUIV    ::_fun;    // ≡
Bif_F12_NEQUIV    Bif_F12_NEQUIV   ::_fun;    // ≢
Bif_F12_ENCODE    Bif_F12_ENCODE   ::_fun;    // ⊤
Bif_F12_DECODE    Bif_F12_DECODE   ::_fun;    // ⊥
Bif_F12_DOMINO    Bif_F12_DOMINO   ::_fun;    // ⌹
Bif_F12_ROTATE    Bif_F12_ROTATE   ::_fun;    // ⌽
Bif_F12_ROTATE1   Bif_F12_ROTATE1  ::_fun;    // ⊖
Bif_F12_TRANSPOSE Bif_F12_TRANSPOSE::_fun;    // ⍉
Bif_F12_INDEX_OF  Bif_F12_INDEX_OF ::_fun;    // ⍳
Bif_F12_RHO       Bif_F12_RHO      ::_fun;    // ⍴
Bif_F2_INTER      Bif_F2_INTER     ::_fun;    // ∩
Bif_F12_UNION     Bif_F12_UNION    ::_fun;    // ∪
Bif_F2_LEFT       Bif_F2_LEFT      ::_fun;    // ⊣
Bif_F2_RIGHT      Bif_F2_RIGHT     ::_fun;    // ⊢

// primitive function pointers
//
Bif_F0_ZILDE      * Bif_F0_ZILDE     ::fun = &Bif_F0_ZILDE     ::_fun;
Bif_F1_EXECUTE    * Bif_F1_EXECUTE   ::fun = &Bif_F1_EXECUTE   ::_fun;
Bif_F2_INDEX      * Bif_F2_INDEX     ::fun = &Bif_F2_INDEX     ::_fun;
Bif_F12_PARTITION * Bif_F12_PARTITION::fun = &Bif_F12_PARTITION::_fun;
Bif_F12_PICK      * Bif_F12_PICK     ::fun = &Bif_F12_PICK     ::_fun;
Bif_F12_COMMA     * Bif_F12_COMMA    ::fun = &Bif_F12_COMMA    ::_fun;
Bif_F12_COMMA1    * Bif_F12_COMMA1   ::fun = &Bif_F12_COMMA1   ::_fun;
Bif_F12_ELEMENT   * Bif_F12_ELEMENT  ::fun = &Bif_F12_ELEMENT  ::_fun;
Bif_F12_EQUIV     * Bif_F12_EQUIV    ::fun = &Bif_F12_EQUIV    ::_fun;
Bif_F12_NEQUIV    * Bif_F12_NEQUIV   ::fun = &Bif_F12_NEQUIV   ::_fun;
Bif_F12_ENCODE    * Bif_F12_ENCODE   ::fun = &Bif_F12_ENCODE   ::_fun;
Bif_F12_DECODE    * Bif_F12_DECODE   ::fun = &Bif_F12_DECODE   ::_fun;
Bif_F12_DOMINO    * Bif_F12_DOMINO   ::fun = &Bif_F12_DOMINO   ::_fun;
Bif_F12_ROTATE    * Bif_F12_ROTATE   ::fun = &Bif_F12_ROTATE   ::_fun;
Bif_F12_ROTATE1   * Bif_F12_ROTATE1  ::fun = &Bif_F12_ROTATE1  ::_fun;
Bif_F12_TRANSPOSE * Bif_F12_TRANSPOSE::fun = &Bif_F12_TRANSPOSE::_fun;
Bif_F12_INDEX_OF  * Bif_F12_INDEX_OF ::fun = &Bif_F12_INDEX_OF ::_fun;
Bif_F12_RHO       * Bif_F12_RHO      ::fun = &Bif_F12_RHO      ::_fun;
Bif_F2_INTER      * Bif_F2_INTER     ::fun = &Bif_F2_INTER     ::_fun;
Bif_F12_UNION     * Bif_F12_UNION    ::fun = &Bif_F12_UNION    ::_fun;
Bif_F2_LEFT       * Bif_F2_LEFT      ::fun = &Bif_F2_LEFT      ::_fun;
Bif_F2_RIGHT      * Bif_F2_RIGHT     ::fun = &Bif_F2_RIGHT     ::_fun;

const CharCell PrimitiveFunction::c_filler(UNI_ASCII_SPACE);
const IntCell  PrimitiveFunction::n_filler(0);

int Bif_F1_EXECUTE::copy_pending = 0;

//-----------------------------------------------------------------------------
Token
PrimitiveFunction::eval_fill_AB(Value_P A, Value_P B)
{
   return eval_AB(A, B);
}
//-----------------------------------------------------------------------------
Token
PrimitiveFunction::eval_fill_B(Value_P B)
{
   return eval_B(B);
}
//-----------------------------------------------------------------------------
ostream &
PrimitiveFunction::print(ostream & out) const
{
   return out << get_Id();
}
//-----------------------------------------------------------------------------
void
PrimitiveFunction::print_properties(ostream & out, int indent) const
{
UCS_string ind(indent, UNI_ASCII_SPACE);
   out << ind << "System Function ";
   print(out);
   out << endl;
}
//=============================================================================
Token
Bif_F0_ZILDE::eval_()
{
   return Token(TOK_APL_VALUE1, Idx0(LOC));
}
//=============================================================================
Token
Bif_F12_RHO::eval_B(Value_P B)
{
Value_P Z(B->get_rank(), LOC);

   loop(r, B->get_rank())   new (Z->next_ravel()) IntCell(B->get_shape_item(r));

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_RHO::eval_AB(Value_P A, Value_P B)
{
#ifdef PERFORMANCE_COUNTERS_WANTED
const uint64_t start_1 = cycle_counter();
#endif

const Shape shape_Z(A.get(), 0);

   // check that shape_Z is positive
   //
   loop(r, shape_Z.get_rank())
      {
        if (shape_Z.get_shape_item(r) < 0)   DOMAIN_ERROR;
      }

const ShapeItem len_Z = shape_Z.get_volume();

   if (len_Z <= B->element_count()   &&
       B->get_owner_count() == 2 && 
       this == Workspace::SI_top()->get_prefix().get_dyadic_fun())
      {
        // at this point Z is not greater than B and B has only 2 owners:
        // the prefix (who will discard it after we return) and our Value_P B
        //
        // Since we will give up our ownership) and prefix will
        //  pop_args_push_result() in reduce_A_F_B_(), B is no longer used
        // and we can reshape it in place instead of copying B into a new Z.
        //
        Log(LOG_optimization) CERR << "optimizing A⍴B" << endl;

        // release unused cells
        //
        const ShapeItem len_B = B->element_count();
        ShapeItem rest = len_Z;
        if (rest == 0)
           {
             rest = 1;
             if (B->get_ravel(0).is_pointer_cell())
                {
                  B->get_ravel(0).get_pointer_value()->to_proto();
                }
             else
                {
                   B->get_ravel(0).init_type(B->get_ravel(0), B.getref(), LOC);
                }
           }

        while (rest < len_B)   B->get_ravel(rest++).release(LOC);

        B->set_shape(shape_Z);

#ifdef PERFORMANCE_COUNTERS_WANTED
const uint64_t end_1 = cycle_counter();
   Performance::fs_F12_RHO_AB.add_sample(end_1 - start_1,
                                         B->nz_element_count());
#endif

        return Token(TOK_APL_VALUE1, B);
      }

Token ret = do_reshape(shape_Z, *B);

#ifdef PERFORMANCE_COUNTERS_WANTED
const uint64_t end_1 = cycle_counter();
   Performance::fs_F12_RHO_AB.add_sample(end_1 - start_1,
                                         shape_Z.get_volume());
#endif

   return ret;
}
//-----------------------------------------------------------------------------
Token
Bif_F12_RHO::do_reshape(const Shape & shape_Z, const Value & B)
{
const ShapeItem len_B = B.element_count();
const ShapeItem len_Z = shape_Z.get_volume();

Value_P Z(shape_Z, LOC);

   if (len_B == 0)   // empty B: use prototype
      {
        loop(z, len_Z)
            Z->next_ravel()->init_type(B.get_ravel(0), Z.getref(), LOC);
      }
   else
      {
        loop(z, len_Z)
            B.get_ravel(z % len_B).init_other(Z->next_ravel(), Z.getref(), LOC);
      }

   Z->set_default(B, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_INDEX_OF::eval_B(Value_P B)
{
   if (B->get_rank() > 1)   RANK_ERROR;

const APL_Integer qio = Workspace::get_IO();
const ShapeItem ec = B->element_count();

   if (ec == 1)
      {
        // interval (standard ⍳N)
        //
        const APL_Integer len = B->get_ravel(0).get_near_int();
        if (len < 0)   DOMAIN_ERROR;

        Value_P Z(len, LOC);

        loop(z, len)   new (Z->next_ravel()) IntCell(qio + z);

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // generalized ⍳B a la Dyalog APL...
   //
   if (ec == 0)
      {
        Value_P Z(LOC);
        new (Z->next_ravel()) PointerCell(Idx0(LOC).get(), Z.getref());
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

Shape sh(B.get(), 0);
   loop(b, ec)
      {
        if (sh.get_shape_item(b) < 0)   DOMAIN_ERROR;
      }

   // at this point sh is correct and ⍳ cannot fail.
   //
Value_P Z(sh, LOC);
   loop(z, Z->element_count())
      {
        Value_P ZZ(sh.get_rank(), LOC);
        ShapeItem N = z;
        loop(r, sh.get_rank())
            {
              const ShapeItem q = sh.get_shape_item(ec - r - 1);
              new (&ZZ->get_ravel(ec - r - 1))   IntCell(N%q + qio);
              N /= q;
            }
        ZZ->check_value(LOC);
        new (Z->next_ravel())   PointerCell(ZZ.get(), Z.getref());
      }

   if (Z->element_count() == 0)   // empty result
      {
        Value_P ZZ(ec, LOC);
        loop(r, ec)   new (ZZ->next_ravel())   IntCell(0);
        ZZ->check_value(LOC);
        new (&Z->get_ravel(0))   PointerCell(ZZ.get(), Z.getref());   // prototype
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_INDEX_OF::eval_AB(Value_P A, Value_P B)
{
const double qct = Workspace::get_CT();
const APL_Integer qio = Workspace::get_IO();

   // Index of
   //
   if (!A->is_scalar_or_vector())   RANK_ERROR;

const uint64_t len_A  = A->element_count();
const uint64_t len_BZ = B->element_count();

Value_P Z(B->get_shape(), LOC);

   loop(bz, len_BZ)
       {
         const Cell & cell_B = B->get_ravel(bz);
         APL_Integer z = len_A;   // assume cell_B is not in A
         loop(a, len_A)
             {
               if (cell_B.equal(A->get_ravel(a), qct))
                  {
                    z = a;
                    break;
                  }
             }
         new (Z->next_ravel()) IntCell(qio + z);
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_COMMA::ravel(const Shape & new_shape, Value_P B)
{
Value_P Z(new_shape, LOC);

const ShapeItem count = B->element_count();
   Assert(count == Z->element_count());

   loop(c, count)   Z->next_ravel()->init(B->get_ravel(c), Z.getref(), LOC);

   Z->set_default(*B.get(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Value_P
Bif_COMMA::prepend_scalar(const Cell & cell_A, uAxis axis, Value_P B)
{
   if (B->is_empty())
      {
        Shape shape_Z = B->get_shape();
        shape_Z.set_shape_item(axis, shape_Z.get_shape_item(axis) + 1);
        Value_P Z(shape_Z, LOC);
        if (Z->is_empty())
           {
              Z->get_ravel(0).init(cell_A, Z.getref(), LOC);   // prototype
           }
        else
           {
             loop(z, Z->element_count())
                 Z->next_ravel()->init(cell_A, Z.getref(), LOC);
           }
        return Z;
      }

   if (B->is_scalar())
      {
        Value_P Z(2, LOC);
        Z->next_ravel()->init(cell_A, Z.getref(), LOC);
        Z->next_ravel()->init(B->get_ravel(0), Z.getref(), LOC);
        Z->check_value(LOC);
        return Z;
      }

   if (axis >= B->get_rank())   INDEX_ERROR;

Shape shape_Z(B->get_shape());
   shape_Z.set_shape_item(axis, shape_Z.get_shape_item(axis) + 1);

Value_P Z(shape_Z, LOC);

const Shape3 shape_B3(B->get_shape(), axis);
   const ShapeItem slice_a = shape_B3.l();
   const ShapeItem slice_b = shape_B3.l() * B->get_shape_item(axis);

const Cell * cB = &B->get_ravel(0);

   loop(hz, shape_B3.h())
       {
         loop(lz, slice_a)   Z->next_ravel()->init(cell_A, Z.getref(), LOC);

         Cell::copy(*Z.get(), cB, slice_b);
       }

   Z->check_value(LOC);
   return Z;
}
//-----------------------------------------------------------------------------
Value_P
Bif_COMMA::append_scalar(Value_P A, uAxis axis, const Cell & cell_B)
{
   if (A->is_empty())
      {
        Shape shape_Z = A->get_shape();
        shape_Z.set_shape_item(axis, shape_Z.get_shape_item(axis) + 1);
        Value_P Z(shape_Z, LOC);
        if (Z->is_empty())
           {
              Z->get_ravel(0).init(cell_B, Z.getref(), LOC);   // prototype
           }
        else
           {
             loop(z, Z->element_count())
                 Z->next_ravel()->init(cell_B, Z.getref(), LOC);
           }
        return Z;
      }

   // A->is_scalar() is handled by prepend_scalar()

   if (axis >= A->get_rank())   INDEX_ERROR;

Shape shape_Z(A->get_shape());
   shape_Z.set_shape_item(axis, shape_Z.get_shape_item(axis) + 1);

Value_P Z(shape_Z, LOC);

const Shape3 shape_A3(A->get_shape(), axis);
const ShapeItem slice_a = shape_A3.l() * A->get_shape_item(axis);
const ShapeItem slice_b = shape_A3.l();

const Cell * cA = &A->get_ravel(0);

   loop(hz, shape_A3.h())
       {
         Cell::copy(*Z.get(), cA, slice_a);
         loop(lz, slice_b)   Z->next_ravel()->init(cell_B, Z.getref(), LOC);
       }

   Z->check_value(LOC);
   return Z;
}
//-----------------------------------------------------------------------------
Token
Bif_COMMA::catenate(Value_P A, Axis axis, Value_P B)
{
   // NOTE: the case A->is_scalar() && B->is_scalar() was supposedly ruled out
   //       before calling catenate()

   if (A->is_scalar())
      {
        const Cell & cell_A = A->get_ravel(0);
        Value_P Z = prepend_scalar(cell_A, axis, B);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (B->is_scalar())
      {
        const Cell & cell_B = B->get_ravel(0);
        Value_P Z = append_scalar(A, axis, cell_B);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if ((A->get_rank() + 1) == B->get_rank())
      {
        Shape shape_Z;

        // check shape conformance.
        {
          ShapeItem ra = 0;
          loop(rb, B->get_rank())
             {
               if (rb != axis)
                  {
                    shape_Z.add_shape_item(B->get_shape_item(rb));
                    if (A->get_shape_item(ra) != B->get_shape_item(rb))
                       {
                         LENGTH_ERROR;
                       }
                    ++ra;
                  }
               else
                  {
                    shape_Z.add_shape_item(B->get_shape_item(rb) + 1);
                  }
             }
        }

        Value_P Z(shape_Z, LOC);

        Z->set_default(*B.get(), LOC);

        const Shape3 shape_B3(B->get_shape(), axis);
        const ShapeItem slice_a = shape_B3.l();
        const ShapeItem slice_b = shape_B3.l() * B->get_shape_item(axis);

        const Cell * cA = &A->get_ravel(0);
        const Cell * cB = &B->get_ravel(0);

        loop(hz, shape_B3.h())
            {
              Cell::copy(*Z.get(), cA, slice_a);
              Cell::copy(*Z.get(), cB, slice_b);
            }

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (A->get_rank() == (B->get_rank() + 1))
      {
        // e.g.	        ∆∆∆ , 3
        //              ∆∆∆   4
        //
        Shape shape_Z;

        // check shape conformance. We step ranks ra and rb, except for the
        // axis where only ra is incremented.
        {
          uint32_t rb = 0;
          loop(ra, A->get_rank())
             {
               if (ra != axis)
                  {
                    if (A->get_shape_item(ra) != B->get_shape_item(rb))
                       LENGTH_ERROR;

                    shape_Z.add_shape_item(A->get_shape_item(ra));
                    ++rb;
                  }
               else
                  {
                    shape_Z.add_shape_item(A->get_shape_item(ra) + 1);
                  }
             }
        }

        Value_P Z(shape_Z, LOC);

        Z->set_default(*B.get(), LOC);

        const Shape3 shape_A3(A->get_shape(), axis);
        const ShapeItem slice_a = shape_A3.l() * A->get_shape_item(axis);
        const ShapeItem slice_b = shape_A3.l();

        const Cell * cA = &A->get_ravel(0);
        const Cell * cB = &B->get_ravel(0);

        loop (hz, shape_A3.h())
            {
              Cell::copy(*Z.get(), cA, slice_a);
              Cell::copy(*Z.get(), cB, slice_b);
            }

        Z->set_default(*B.get(), LOC);

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (A->get_rank() != B->get_rank())   RANK_ERROR;

   // A and B have the same rank. The shapes need to agree, except for the
   // dimension corresponding to the axis.
   //
Shape shape_Z;

   loop(r, A->get_rank())
       {
         if (r != axis)
            {
              if (A->get_shape_item(r) != B->get_shape_item(r))
                 LENGTH_ERROR;

              shape_Z.add_shape_item(A->get_shape_item(r));
            }
         else
            {
              shape_Z.add_shape_item(A->get_shape_item(r) +
                                   + B->get_shape_item(r));
            }
       }

const Shape3 shape_A3(A->get_shape(), axis);

const Cell * cA = &A->get_ravel(0);
const Cell * cB = &B->get_ravel(0);
const ShapeItem slice_a = shape_A3.l() * A->get_shape_item(axis);
const ShapeItem slice_b = shape_A3.l() * B->get_shape_item(axis);

Value_P Z(shape_Z, LOC);
   loop(hz, shape_A3.h())
       {
         Cell::copy(*Z.get(), cA, slice_a);
         Cell::copy(*Z.get(), cB, slice_b);
       }

   Z->set_default(*B.get(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_COMMA::laminate(Value_P A, Axis axis, Value_P B)
{
   // shapes of A and B must be the same, unless one of them is a scalar.
   //
   if (!A->is_scalar() && !B->is_scalar())
      A->get_shape().check_same(B->get_shape(),
                                E_INDEX_ERROR, E_LENGTH_ERROR, LOC);

const Shape shape_Z = A->is_scalar() ? B->get_shape().insert_axis(axis, 2)
                                    : A->get_shape().insert_axis(axis, 2);

Value_P Z(shape_Z, LOC);

const Shape3 shape_Z3(shape_Z, axis);
   if (shape_Z3.m() != 2)   AXIS_ERROR;

const Cell * cA = &A->get_ravel(0);
const Cell * cB = &B->get_ravel(0);

   if (A->is_scalar())
      {
        if (B->is_scalar())
           {
              Z->next_ravel()->init(*cA, Z.getref(), LOC);
              Z->next_ravel()->init(*cB, Z.getref(), LOC);
           }
        else
           {
             loop(h, shape_Z3.h())
                 {
                   loop(l, shape_Z3.l())
                       Z->next_ravel()->init(*cA, Z.getref(), LOC);
                   Cell::copy(*Z.get(), cB, shape_Z3.l());
                }
           }
      }
   else
      {
        if (B->is_scalar())
           {
             loop(h, shape_Z3.h())
                 {
                   Cell::copy(*Z.get(), cA, shape_Z3.l());
                   loop(l, shape_Z3.l())
                       Z->next_ravel()->init(*cB, Z.getref(), LOC);
                }
           }
        else
           {
             loop(h, shape_Z3.h())
                 {
                   Cell::copy(*Z.get(), cA, shape_Z3.l());
                   Cell::copy(*Z.get(), cB, shape_Z3.l());
                }
           }
      }

   Z->set_default(*B.get(), LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_COMMA::ravel_axis(Value_P X, Value_P B, uAxis axis)
{
const APL_Integer qio = Workspace::get_IO();

   if (!X)   SYNTAX_ERROR;

   // I must be a (integer or real) scalar or simple integer vector
   //
   if (X->get_rank() > 1)   INDEX_ERROR;

   // There are 3 variants, determined by I:
   //
   // 1) I is empty:             append a new last axis of length 1
   // 2) I is a fraction:        append a new axis before I
   // 3a) I is integer scalar:   return B
   // 3b) I is integer vector:   combine axes

   // case 1:   ,['']B or ,[⍳0]B : append new first or last axis of length 1.
   //
   if (X->element_count() == 0)
      {
        if (B->get_rank() == MAX_RANK)   AXIS_ERROR;

        const Shape shape_Z = B->get_shape().insert_axis(axis, 1);
        return ravel(shape_Z, B);
      }

   // case 2:   ,[x.y]B : insert axis before axis x+1
   //
   if (!X->get_ravel(0).is_near_int())  // fraction: insert an axis
      {
        if (B->get_rank() == MAX_RANK)   INDEX_ERROR;

        const APL_Float new_axis = X->get_ravel(0).get_real_value() - qio;
        Axis axis = new_axis;   if (new_axis < 0.0)   axis = -1;
        const Shape shape_Z = B->get_shape().insert_axis(axis + 1, 1);
        return ravel(shape_Z, B);
      }

   // case 3a: ,[n]B : return B (combine single axis doesn't change anything)
   //
   if (X->is_scalar_or_len1_vector())   // single int: return B->
      {
        Token result(TOK_APL_VALUE1, B->clone(LOC));
        return result;
      }

   // case 3b: ,[n1 ... nk]B : combine axes.
   //
const Shape axes(X.get(), qio);

const ShapeItem from = axes.get_shape_item(0);
   if (from < 0)   AXIS_ERROR;

const ShapeItem to   = axes.get_last_shape_item();
   if (to >= B->get_rank())   AXIS_ERROR;

   // check that the axes are contiguous and compute the number of elements
   // in the combined axes
   //
ShapeItem count = 1;
   loop(a, axes.get_rank())
      {
        if (axes.get_shape_item(a) != (from + a))   AXIS_ERROR;
        count *= B->get_shape_item(from + a);
      }

Shape shape_Z;
   loop (r, from)   shape_Z.add_shape_item(B->get_shape_item(r));
   shape_Z.add_shape_item(count);
   for (uRank r = to + 1; r < B->get_rank(); ++r)
       shape_Z.add_shape_item(B->get_shape_item(r));

   return ravel(shape_Z, B);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_COMMA::eval_B(Value_P B)
{
   if (B->get_ravel(0).is_picked_lval_cell())
      {
// CERR << "*** PICKED ***" << endl;
        Cell * ptr = B->get_ravel(0).get_lval_value();
        Assert(ptr);
        Value_P target = ptr->get_pointer_value()->get_cellrefs(LOC);
        return eval_B(target);
      }

const Shape shape_Z(B->element_count());

   if (B->get_owner_count() == 2 && 
       this == Workspace::SI_top()->get_prefix().get_monadic_fun())
      {
        Log(LOG_optimization)
           CERR << "optimizing ,B (len="
                << B->nz_element_count() << ")" << endl;

        B->set_shape(shape_Z);
        return Token(TOK_APL_VALUE1, B);
      }

   return ravel(shape_Z, B);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_COMMA::eval_AB(Value_P A, Value_P B)
{
  if (A->is_scalar() && B->is_scalar())
     {
       Value_P Z(2, LOC);
       Z->next_ravel()->init(A->get_ravel(0), Z.getref(), LOC);
       Z->next_ravel()->init(B->get_ravel(0), Z.getref(), LOC);
       Z->check_value(LOC);
       return Token(TOK_APL_VALUE1, Z);
     }

uRank max_rank = A->get_rank();
   if (max_rank < B->get_rank())  max_rank = B->get_rank(); 
   return catenate(A, max_rank - 1, B);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_COMMA::eval_AXB(Value_P A, Value_P X, Value_P B)
{
  if (A->is_scalar() && B->is_scalar())   RANK_ERROR;

   // catenate or laminate
   //
   if (!X->is_scalar_or_len1_vector())   AXIS_ERROR;

const Cell & cX = X->get_ravel(0);
const APL_Integer qio = Workspace::get_IO();

   if (cX.is_near_int())   // catenate along existing axis
      {
        const Axis axis = cX.get_checked_near_int() - qio;
        if (axis < 0)                                         AXIS_ERROR;
        if (uAxis(axis) >= A->get_rank() && uAxis(axis) >= B->get_rank())
           AXIS_ERROR;
        return catenate(A, axis, B);
      }

const APL_Float axis = cX.get_real_value() - qio;
   if (axis <= -1.0)   AXIS_ERROR;
   if (axis >= (A->get_rank() + 1.0) &&
       axis >= (B->get_rank() + 1.0))   AXIS_ERROR;
   return laminate(A, Axis(axis + 1.0), B);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_COMMA1::eval_B(Value_P B)
{
   // turn B into a matrix
   //
ShapeItem c1 = 1;   // assume B is scalar
ShapeItem c2 = 1;   // assume B is scalar;

   if (B->get_rank() >= 1)
      {
        c1 = B->get_shape_item(0);
        for (uRank r = 1; r < B->get_rank(); ++r)   c2 *= B->get_shape_item(r);
      }

Shape shape_Z(c1, c2);
   if (B->get_owner_count() == 2 && 
       this == Workspace::SI_top()->get_prefix().get_monadic_fun())
      {
        Log(LOG_optimization) CERR << "optimizing ,B" << endl;

        B->set_shape(shape_Z);
        return Token(TOK_APL_VALUE1, B);
      }
   return ravel(shape_Z, B);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_COMMA1::eval_AB(Value_P A, Value_P B)
{
  if (A->is_scalar() && B->is_scalar())
     {
       Value_P Z(2, LOC);
       Z->next_ravel()->init(A->get_ravel(0), Z.getref(), LOC);
       Z->next_ravel()->init(B->get_ravel(0), Z.getref(), LOC);
       Z->check_value(LOC);
       return Token(TOK_APL_VALUE1, Z);
     }

   return catenate(A, 0, B);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_COMMA1::eval_AXB(Value_P A, Value_P X, Value_P B)
{
   return Bif_F12_COMMA::fun->eval_AXB(A, X, B);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_B(Value_P B)
{
   if (B->is_scalar())
      {
        Value_P Z(LOC);

        B->get_ravel(0).bif_reciprocal(Z->next_ravel());
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (B->get_rank() == 1)   // inversion at the unit sphere
      {
        const double qct = Workspace::get_CT();
        const ShapeItem len = B->get_shape_item(0);
        APL_Complex r2(0.0);
        loop(l, len)
            {
              const APL_Complex b = B->get_ravel(l).get_complex_value();
              r2 += b*b;
            }

        if (r2.real() < qct && r2.real() > -qct && 
            r2.imag() < qct && r2.imag() > -qct)
            DOMAIN_ERROR;
            
        Value_P Z(len, LOC);

        if (r2.imag() < qct && r2.imag() > -qct)   // real result
           {
             loop(l, len)
                 {
                   const APL_Float b = B->get_ravel(l).get_real_value();
                   new (Z->next_ravel())   FloatCell(b / r2.real());
                 }
           }
        else                                       // complex result
           {
             loop(l, len)
                 {
                   const APL_Complex b = B->get_ravel(l).get_complex_value();
                   new (Z->next_ravel())   ComplexCell(b / r2);
                 }
           }

        Z->set_default(*B.get(), LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (B->get_rank() > 2)   RANK_ERROR;

const ShapeItem rows = B->get_shape_item(0);
const ShapeItem cols = B->get_shape_item(1);
   if (cols > rows)   LENGTH_ERROR;

   // create an identity matrix I and call eval_AB(I, B).
   //
const Shape shape_I(rows, rows);
Value_P I(shape_I, LOC);

   loop(r, rows)
   loop(c, rows)
       new (I->next_ravel()) FloatCell((c == r) ? 1.0 : 0.0);

Token result = eval_AB(I, B);
   return result;
}
//-----------------------------------------------------------------------------
extern void divide_matrix(Cell * cZ, bool need_complex,
                          ShapeItem rows, ShapeItem cols_A, const Cell * cA,
                          ShapeItem cols_B, const Cell * cB);

Token
Bif_F12_DOMINO::eval_AB(Value_P A, Value_P B)
{
ShapeItem rows_A = 1;
ShapeItem cols_A = 1;

Shape shape_Z;

   // if rank of A or B is < 2 then treat it as a 
   // 1 by n (or 1 by 1) matrix..
   //
ShapeItem rows_B = 1;
ShapeItem cols_B = 1;
   switch(B->get_rank())
      {
         case 0:  break;

         case 1:  rows_B = B->get_shape_item(0);
                  break;

         case 2:  cols_B = B->get_shape_item(1);
                  rows_B = B->get_shape_item(0);
                  shape_Z.add_shape_item(cols_B);
                  break;

         default: RANK_ERROR;
      }

   switch(A->get_rank())
      {
         case 0:  break;

         case 1:  rows_A = A->get_shape_item(0);
                  break;

         case 2:  cols_A = A->get_shape_item(1);
                  rows_A = A->get_shape_item(0);
                  shape_Z.add_shape_item(cols_A);
                  break;

         default: RANK_ERROR;
      }

   if (rows_B <  cols_B)   LENGTH_ERROR;
   if (rows_A != rows_B)   LENGTH_ERROR;

const bool need_complex = A->is_complex(true) || B->is_complex(true);
Value_P Z(shape_Z, LOC);
   divide_matrix(&Z->get_ravel(0), need_complex,
                 rows_A, cols_A, &A->get_ravel(0),
                 cols_B, &B->get_ravel(0));

   Z->set_default(*B.get(), LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_fill_B(Value_P B)
{
   return Bif_F12_TRANSPOSE::fun->eval_B(B);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_fill_AB(Value_P A, Value_P B)
{
Shape shape_Z;
   loop(r, A->get_rank() - 1)  shape_Z.add_shape_item(A->get_shape_item(r + 1));
   loop(r, B->get_rank() - 1)  shape_Z.add_shape_item(B->get_shape_item(r + 1));

Value_P Z(shape_Z, LOC);
   while (Z->more())   new (Z->next_ravel())   IntCell(0);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_ROTATE::reverse(Value_P B, Axis axis)
{
   if (B->is_scalar())
      {
        Token result(TOK_APL_VALUE1, B->clone(LOC));
        return result;
      }

const Shape3 shape_B3(B->get_shape(), axis);

Value_P Z(B->get_shape(), LOC);

   loop(h, shape_B3.h())
   loop(l, shape_B3.l())
       {
         const ShapeItem hl = h * shape_B3.m() * shape_B3.l() + l;
         const Cell * cB = &B->get_ravel(hl);
         Cell * cZ = &Z->get_ravel(hl);
         loop(m, shape_B3.m())
            cZ[m*shape_B3.l()]
               .init(cB[(shape_B3.m() - m - 1)*shape_B3.l()], Z.getref(), LOC);
       }

   Z->set_default(*B.get(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_ROTATE::rotate(Value_P A, Value_P B, Axis axis)
{
int32_t gsh = 0;   // global shift (scalar A); 0 means local shift (A) used.

const Shape3 shape_B3(B->get_shape(), axis);
const Shape shape_A2(shape_B3.h(), shape_B3.l());

   if (A->is_scalar_or_len1_vector())
      {
        gsh = A->get_ravel(0).get_near_int();
        if (gsh == 0)   // nothing to do.
           {
             Token result(TOK_APL_VALUE1, B->clone(LOC));
             return result;
           }
      }
   else   // otherwise shape A must be shape B with axis removed.
      {
        A->get_shape().check_same(B->get_shape().without_axis(axis),
                                 E_RANK_ERROR, E_LENGTH_ERROR, LOC);
      }


Value_P Z(B->get_shape(), LOC);

   loop(h, shape_B3.h())
   loop(m, shape_B3.m())
   loop(l, shape_B3.l())
       {
         ShapeItem src = gsh;
         if (!src)   src = A->get_ravel(l + h*shape_B3.l()).get_near_int();
         src += shape_B3.m() + m;
         while (src < 0)               src += shape_B3.m();
         while (src >= shape_B3.m())   src -= shape_B3.m();
         Z->next_ravel()
          ->init(B->get_ravel(shape_B3.hml(h, src, l)), Z.getref(), LOC);
       }

   Z->set_default(*B.get(), LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_ROTATE::eval_XB(Value_P X, Value_P B)
{
const Rank axis = Value::get_single_axis(X.get(), B->get_rank());
   return reverse(B, axis);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_ROTATE::eval_AXB(Value_P A, Value_P X, Value_P B)
{
const Rank axis = Value::get_single_axis(X.get(), B->get_rank());
   return rotate(A, B, axis);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_ROTATE1::eval_XB(Value_P X, Value_P B)
{
const Rank axis = Value::get_single_axis(X.get(), B->get_rank());
   return reverse(B, axis);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_ROTATE1::eval_AXB(Value_P A, Value_P X, Value_P B)
{
const Rank axis = Value::get_single_axis(X.get(), B->get_rank());
   return rotate(A, B, axis);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_TRANSPOSE::eval_B(Value_P B)
{
Shape shape_A;

   // monadic transpose is A⍉B with A = ... 4 3 2 1 0
   //
   loop(r, B->get_rank())   shape_A.add_shape_item(B->get_rank() - r - 1);

Value_P Z = transpose(shape_A, B);
   Z->set_default(*B.get(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_TRANSPOSE::eval_AB(Value_P A, Value_P B)
{
   // A should be a scalar or vector.
   //
   if (A->get_rank() > 1)   RANK_ERROR;

   if (B->is_scalar())   // B is a scalar (so A should be empty)
      {
        if (A->element_count() != 0)   LENGTH_ERROR;
        Value_P Z = B->clone(LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

const Shape shape_A(A.get(), Workspace::get_IO());
   if (shape_A.get_rank() != B->get_rank())   LENGTH_ERROR;

   // the elements in A shall be valid axes of B->
   loop(r, A->get_rank())
      {
        if (shape_A.get_shape_item(r) < 0)               DOMAIN_ERROR;
        if (shape_A.get_shape_item(r) >= B->get_rank())   DOMAIN_ERROR;
      }

Value_P Z = (shape_A.get_rank() == B->get_rank() && is_permutation(shape_A))
          ? transpose(shape_A, B) : transpose_diag(shape_A, B);

   Z->set_default(*B.get(), LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Value_P
Bif_F12_TRANSPOSE::transpose(const Shape & A, Value_P B)
{
const Shape shape_Z = permute(B->get_shape(), inverse_permutation(A));
Value_P Z(shape_Z, LOC);

   if (shape_Z.is_empty())
      {
         Z->set_default(*B.get(), LOC);
         return Z;
      }

const Cell * cB = &B->get_ravel(0);

   for (ArrayIterator it_Z(shape_Z); it_Z.more(); ++it_Z)
       {
         const Shape idx_B = permute(it_Z.get_offsets(), A);
         const ShapeItem b = B->get_shape().ravel_pos(idx_B);
         Z->next_ravel()->init(cB[b], Z.getref(), LOC);
       }

   return Z;
}
//-----------------------------------------------------------------------------
Value_P
Bif_F12_TRANSPOSE::transpose_diag(const Shape & A, Value_P B)
{
   // check that: ∧/(⍳⌈/0,A)∈A
   //
   // I.e. 0, 1, ... max_A are in A
   // we search sequentially, since A is small.
   // we construct shape_Z as we go.
   //
Shape shape_Z;
   {
     // check that 0 ≤ A[a] ≤ ⍴⍴B, and compute max_A = ⌈/A
     // i.e. ∧/(ι/0,L)∈L   lrm p. 253
     ShapeItem max_A = 0;   // the largest index in A (- qio).
     loop(ra, A.get_rank())
         {
           if (A.get_shape_item(ra) < 0)                DOMAIN_ERROR;
           if (A.get_shape_item(ra) >= B->get_rank())   DOMAIN_ERROR;
           if (max_A < A.get_shape_item(ra))   max_A = A.get_shape_item(ra);
         }

     // check that every m < max_A is in A.
     // the smallest axis in B found is added to shape_Z.
     //
     loop(m, max_A + 1)
         {
           ShapeItem min_Bm = -1;
           loop(ra, A.get_rank())
               {
                if (m != A.get_shape_item(ra))   continue;
                const ShapeItem B_ra = B->get_shape_item(ra);
                if (min_Bm == -1)   min_Bm = B_ra;
                else if (min_Bm > B_ra)   min_Bm = B_ra;
               }

           if (min_Bm == -1)   DOMAIN_ERROR;   // m not in A

           shape_Z.add_shape_item(min_Bm);
         }
   }

Value_P Z(shape_Z, LOC);
   if (Z->is_empty())
      {
         Z->set_default(*B.get(), LOC);
        return Z;
      }

const Cell * cB = &B->get_ravel(0);

   for (ArrayIterator it_Z(shape_Z); it_Z.more(); ++it_Z)
       {
         const Shape idx_B = permute(it_Z.get_offsets(), A);
         const ShapeItem b = B->get_shape().ravel_pos(idx_B);
         Z->next_ravel()->init(cB[b], Z.getref(), LOC);
       }

   return Z;
}
//-----------------------------------------------------------------------------
Shape
Bif_F12_TRANSPOSE::inverse_permutation(const Shape & sh)
{
ShapeItem rho[MAX_RANK];

   // first, set all items to -1.
   //
   loop(r, sh.get_rank())   rho[r] = -1;

   // then, set all items to the shape items of sh
   //
   loop(r, sh.get_rank())
       {
         const ShapeItem rx = sh.get_shape_item(r);
         if (rx < 0)                 DOMAIN_ERROR;
         if (rx >= sh.get_rank())    DOMAIN_ERROR;
         if (rho[rx] != -1)          DOMAIN_ERROR;

         rho[rx] = r;
       }

   return Shape(sh.get_rank(), rho);
}
//-----------------------------------------------------------------------------
Shape
Bif_F12_TRANSPOSE::permute(const Shape & sh, const Shape & perm)
{
Shape ret;

   loop(r, perm.get_rank())
      {
        ret.add_shape_item(sh.get_shape_item(perm.get_shape_item(r)));
      }

   return ret;
}
//-----------------------------------------------------------------------------
bool
Bif_F12_TRANSPOSE::is_permutation(const Shape & sh)
{
ShapeItem rho[MAX_RANK];

   // first, set all items to -1.
   //
   loop(r, sh.get_rank())   rho[r] = -1;

   // then, set all items to the shape items of sh
   //
   loop(r, sh.get_rank())
       {
         const ShapeItem rx = sh.get_shape_item(r);
         if (rx < 0)                return false;
         if (rx >= sh.get_rank())   return false;
         if (rho[rx] != -1)         return false;
          rho[rx] = r;
       }

   return true;
}
//=============================================================================
Token
Bif_F12_DECODE::eval_AB(Value_P A, Value_P B)
{
   // ρZ  is: (¯1↓ρA),1↓ρB
   // ρρZ is: (0⌈¯1+ρρA) + (0⌈¯1+ρρB)
   //
Shape shape_A1;
   if (!A->is_scalar())
      shape_A1 = A->get_shape().without_axis(A->get_rank() - 1);

Shape shape_B1(B->get_shape());
   if (shape_B1.get_rank() > 0)   shape_B1 = B->get_shape().without_axis(0);

const ShapeItem l_len_A = A->get_rank() ? A->get_last_shape_item() : 1;
const ShapeItem h_len_B = B->get_rank() ? B->get_shape_item(0)     : 1;

const ShapeItem h_len_A = shape_A1.get_volume();
const ShapeItem l_len_B = shape_B1.get_volume();

   if (l_len_A == 0 || h_len_B == 0)   // empty result
      {
        const Shape shape_Z(shape_A1 + shape_B1);
        Value_P Z(shape_Z, LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if ((l_len_A != 1) && (h_len_B != 1) && (l_len_A != h_len_B))   LENGTH_ERROR;

const double qct = Workspace::get_CT();

const Shape shape_Z = shape_A1 + shape_B1;

Value_P Z(shape_Z, LOC);

const Cell * cA = &A->get_ravel(0);

   loop(h, h_len_A)
       {
         // cA ... cA + len_A are used. See if they are complex.
         //
         bool complex_A = false;
         bool integer_A = true;
         loop(aa, l_len_A)
             {
                if (!cA[aa].is_near_real())
                   {
                     complex_A = true;
                     integer_A = false;
                     break;
                   }

                if (!cA[aa].is_near_int())   integer_A = false;
             }

         loop(l, l_len_B)
             {
                // cB, cB + l_len_B, ... are used. See if they are complex
                //
                bool complex_B = false;
                bool integer_B = true;
                loop(bb, h_len_B)
                    {
                      if (!B->get_ravel(l + bb*l_len_B).is_near_real())
                         {
                           complex_B = true;
                           integer_B = false;
                           break;
                         }

                      if (!B->get_ravel(l + bb*l_len_B).is_near_int())
                         integer_B = false;
                    }

               const Cell * cB = &B->get_ravel(l);
               Cell * cZ = Z->next_ravel();
               if (integer_A && integer_B)
                  {
                    const bool overflow = decode_int(cZ, l_len_A, cA,
                                                     h_len_B, cB, l_len_B);
                     if (!overflow)   continue;

                     // otherwise: compute as float
                  }

               if (complex_A || complex_B)
                  decode_complex(cZ, l_len_A, cA, h_len_B, cB, l_len_B, qct);
               else
                  decode_real(cZ, l_len_A, cA, h_len_B, cB, l_len_B, qct);
             }
         cA += l_len_A;
       }

   Z->set_default(*B.get(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
bool
Bif_F12_DECODE::decode_int(Cell * cZ, ShapeItem len_A, const Cell * cA,
                       ShapeItem len_B, const Cell * cB, ShapeItem dB)
{
APL_Integer value = 0;
APL_Float value_f = 0.0;

APL_Integer weight = 1;
APL_Float weight_f = 1.0;
const ShapeItem len = (len_A == 1) ? len_B : len_A;

   cA += len_A - 1;         // cA points to the lowest item in A
   cB += dB*(len_B - 1);    // cB points to the lowest item in B
   loop(l, len)
      {
        if (weight_f > LARGE_INT)   return true;
        if (weight_f < SMALL_INT)   return true;

        const APL_Integer vB = cB[0].get_near_int();
        value   = value   + weight   * vB;
        value_f = value_f + weight_f * vB;
        if (value_f > LARGE_INT)   return true;
        if (value_f < SMALL_INT)   return true;

        weight   = weight   * cA[0].get_near_int();
        weight_f = weight_f * cA[0].get_near_int();
        if (len_A != 1)   --cA;
        if (len_B != 1)   cB -= dB;
      }

   new (cZ)   IntCell(value);

   return false;   // no overflow
}
//-----------------------------------------------------------------------------
void
Bif_F12_DECODE::decode_real(Cell * cZ, ShapeItem len_A, const Cell * cA,
                       ShapeItem len_B, const Cell * cB, ShapeItem dB,
                       double qct)
{
APL_Float value = 0.0;
APL_Float weight = 1.0;
const ShapeItem len = (len_A == 1) ? len_B : len_A;

   cA += len_A - 1;         // cA points to the lowest item in A
   cB += dB*(len_B - 1);    // cB points to the lowest item in B
   loop(l, len)
      {
        value += weight*cB[0].get_real_value();
        weight *= cA[0].get_real_value();
        if (len_A != 1)   --cA;
        if (len_B != 1)   cB -= dB;
      }

   if (value < LARGE_INT &&
       value > SMALL_INT &&
       Cell::is_near_int(value))
      new (cZ)   IntCell(Cell::near_int(value));
   else
      new (cZ)   FloatCell(value);
}
//-----------------------------------------------------------------------------
void
Bif_F12_DECODE::decode_complex(Cell * cZ, ShapeItem len_A, const Cell * cA,
                       ShapeItem len_B, const Cell * cB, ShapeItem dB,
                       double qct)
{
APL_Complex value(0.0, 0.0);
APL_Complex weight(1.0, 0.0);
const ShapeItem len = (len_A == 1) ? len_B : len_A;

   cA += len_A - 1;         // cA points to the lowest item in A
   cB += dB*(len_B - 1);    // cB points to the lowest item in B
   loop(l, len)
      {
        value += weight*cB[0].get_complex_value();
        weight *= cA[0].get_complex_value();
        if (len_A != 1)   --cA;
        if (len_B != 1)   cB -= dB;
      }

   if (value.imag() > qct)
      new (cZ)   ComplexCell(value);
   else if (value.imag() < -qct)
      new (cZ)   ComplexCell(value);
   else if (value.real() < LARGE_INT
         && value.real() > SMALL_INT
         && Cell::is_near_int(value.real()))
      new (cZ)   IntCell(value.real());
   else
      new (cZ)   FloatCell(value.real());
}
//-----------------------------------------------------------------------------
Token
Bif_F12_ENCODE::eval_AB(Value_P A, Value_P B)
{
   if (A->is_scalar())   return Bif_F12_STILE::fun->eval_AB(A, B);

const ShapeItem ec_A = A->element_count();
const ShapeItem ec_B = B->element_count();
   if (ec_A == 0 || ec_B == 0)   // empty A or B
      {
        const Shape shape_Z(A->get_shape() + B->get_shape());
        Value_P Z(shape_Z, LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

const ShapeItem ah = A->get_shape_item(0);    // first dimension
const ShapeItem al = A->element_count()/ah;   // remaining dimensions

const double qct = Workspace::get_CT();

const Shape shape_Z = A->get_shape() + B->get_shape();
const ShapeItem dZ = shape_Z.get_volume()/shape_Z.get_shape_item(0);

Value_P Z(shape_Z, LOC);

   loop(a1, al)
      {
        const Cell * cB = &B->get_ravel(0);

        // find largest Celltype in a1...
        //
        CellType ct_a1 = CT_INT;
        loop(h, ah)
            {
              const CellType ct = A->get_ravel(a1 + h*al).get_cell_type();
              if (ct == CT_INT)            ;
              else if (ct == CT_FLOAT)     { if (ct_a1 == CT_INT)  ct_a1 = ct; }
              else if (ct == CT_COMPLEX)   ct_a1 = CT_COMPLEX;
              else                         DOMAIN_ERROR;
            }

        loop(b, ec_B)
            {
              CellType ct = ct_a1;
              const CellType ct_b = cB->get_cell_type();
              if (ct_b == CT_INT)            ;
              else if (ct_b == CT_FLOAT)     { if (ct == CT_INT)  ct = ct_b; }
              else if (ct_b == CT_COMPLEX)   ct = CT_COMPLEX;
              else                           DOMAIN_ERROR;

              if (ct == CT_INT)   // both cells are integer
                 encode(dZ, Z->next_ravel(), ah, al,
                        &A->get_ravel(a1), cB++->get_int_value());
              else if (ct == CT_FLOAT)
                 encode(dZ, Z->next_ravel(), ah, al,
                        &A->get_ravel(a1), cB++->get_real_value(), qct);
              else
                 encode(dZ, Z->next_ravel(), ah, al,
                        &A->get_ravel(a1), cB++->get_complex_value(), qct);
            }
       }

   Z->set_default(*B.get(), LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
void
Bif_F12_ENCODE::encode(ShapeItem dZ, Cell * cZ, ShapeItem ah, ShapeItem al,
                       const Cell * cA, APL_Integer b)
{
   // we work downwards from the higher indices...
   //
   cA += ah*al;   // the end of A
   cZ += ah*dZ;   // the end of Z

   loop(l, ah)
       {
         cA -= al;
         cZ -= dZ;

        const IntCell cC(b);
        cC.bif_residue(cZ, cA);

        if (cA->get_int_value() == 0)
           {
             b = 0;
           }
         else
           {
             b -= cZ->get_int_value();
             b /= cA->get_int_value();
           }
       }
}
//-----------------------------------------------------------------------------
void
Bif_F12_ENCODE::encode(ShapeItem dZ, Cell * cZ, ShapeItem ah, ShapeItem al,
                       const Cell * cA, APL_Float b, double qct)
{
   // we work downwards from the higher indices...
   //
   cA += ah*al;   // the end of A
   cZ += ah*dZ;   // the end of Z

   loop(l, ah)
       {
         cA -= al;
         cZ -= dZ;

        const FloatCell cC(b);
        cC.bif_residue(cZ, cA);

        if (cA->is_near_zero())
           {
             b = 0.0;
           }
         else
           {
             b -= cZ->get_real_value();
             b /= cA->get_real_value();
           }
       }
}
//-----------------------------------------------------------------------------
void
Bif_F12_ENCODE::encode(ShapeItem dZ, Cell * cZ, ShapeItem ah, ShapeItem al,
                       const Cell * cA, APL_Complex b, double qct)
{
   // we work downwards from the higher indices...
   //
   cA += ah*al;   // the end of A
   cZ += ah*dZ;   // the end of Z

   loop(l, ah)
       {
         cA -= al;
         cZ -= dZ;

        const ComplexCell cC(b);
        cC.bif_residue(cZ, cA);

        if (cA->is_near_zero())
           {
             b = APL_Complex(0, 0);
           }
         else
           {
             b -= cZ->get_complex_value();
             b /= cA->get_complex_value();
           }
       }
}
//-----------------------------------------------------------------------------
Token
Bif_F12_ELEMENT::eval_B(Value_P B)
{
   // enlist
   //
   if (B->element_count() == 0)   // empty argument
      {
        ShapeItem N = 0;
        Value_P Z(N, LOC);   // empty vector with prototype ' ' or '0'
        const Cell * C = &B->get_ravel(0);
        bool left = false;
        for (;;)
            {
              if (C->is_pointer_cell())
                 {
                   C = &C->get_pointer_value()->get_ravel(0);
                   continue;
                 }

              if (left && C->is_lval_cell())
                 {
                   C = C->get_lval_value();
                   if (C == 0)
                      {
                        CERR << "0-pointer at " LOC << endl;
                        FIXME;
                      }
                   else if (C->is_pointer_cell())
                      {
                        C = &C->get_pointer_value()->get_ravel(0);
                      }
                   else
                      {
                        Value * owner = C->cLvalCell().get_cell_owner();
                        new (&Z->get_ravel(0))
                            LvalCell(C->get_lval_value(), owner);
                        break;
                      }
                 }

              if (C->is_numeric())
                 {
                   new (&Z->get_ravel(0)) CharCell(UNI_ASCII_0);
                   break;
                 }

              if (C->is_character_cell())
                 {
                   new (&Z->get_ravel(0)) CharCell(UNI_ASCII_SPACE);
                   break;
                 }

              if (C->is_lval_cell())
                 {
                   left = C->cLvalCell().get_cell_owner();
                   C = C->get_lval_value();
                   continue;
                 }

               // not reached
               //
               FIXME;
            }

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

const ShapeItem len_Z = B->get_enlist_count();
Value_P Z(len_Z, LOC);

Cell * z = &Z->get_ravel(0);
   B->enlist(z, Z.getref(), B->get_lval_cellowner());

   Z->set_default(*B.get(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_ELEMENT::eval_AB(Value_P A, Value_P B)
{
   // member
   //
const double qct = Workspace::get_CT();
const ShapeItem len_Z = A->element_count();
const ShapeItem len_B = B->element_count();
Value_P Z(A->get_shape(), LOC);

   loop(z, len_Z)
       {
         const Cell & cell_A = A->get_ravel(z);
         APL_Integer same = 0;
         loop(b, len_B)
             if (cell_A.equal(B->get_ravel(b), qct))
                {
                  same = 1;
                  break;
                }
         new (Z->next_ravel())   IntCell(same);
       }
   
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_PARTITION::eval_AXB(Value_P A, Value_P X, Value_P B)
{
const Rank axis = Value::get_single_axis(X.get(), B->get_rank());
   return partition(A, B, axis);
}
//-----------------------------------------------------------------------------
Value_P
Bif_F12_PARTITION::do_eval_B(Value_P B)
{
   if (B->is_simple_scalar())   return B;

Value_P Z(LOC);
   new (Z->next_ravel()) PointerCell(B->clone(LOC).get(), Z.getref());
   Z->check_value(LOC);
   return Z;
}
//-----------------------------------------------------------------------------
Value_P
Bif_F12_PARTITION::do_eval_XB(Value_P X, Value_P B)
{
const Shape shape_X = Value::to_shape(X.get());

   return enclose_with_axes(shape_X, B);
}
//-----------------------------------------------------------------------------
Value_P
Bif_F12_PARTITION::enclose_with_axes(const Shape & shape_X, Value_P B)
{
Shape item_shape;
Shape it_weight;
Shape shape_Z;
Shape weight_Z;

const Shape weight_B = B->get_shape().reverse_scan();

   // put the dimensions mentioned in X into item_shape and the
   // others into shape_Z
   //

   loop(r, B->get_rank())        // the axes not in shape_X
       {
         if (!shape_X.contains_axis(r))
            {
              shape_Z.add_shape_item(B->get_shape_item(r));
              weight_Z.add_shape_item(weight_B.get_shape_item(r));
            }
       }

int X_axes_used = 0;
   loop(r, shape_X.get_rank())   // the axes in shape_X
       {
         const ShapeItem x_r = shape_X.get_shape_item(r);

         // check that X∈⍳⍴⍴B
         //
         if (x_r < 0)                  AXIS_ERROR;
         if (x_r >= B->get_rank())     AXIS_ERROR;
         if (X_axes_used & 1 << x_r)
            {
              MORE_ERROR() = "Duplicate axis";
              AXIS_ERROR;
            }
         X_axes_used |= 1 << x_r;

         item_shape.add_shape_item(B->get_shape_item(x_r));
         it_weight.add_shape_item(weight_B.get_shape_item(x_r));
       }

   if (item_shape.get_rank() == 0)   // empty axes
      {
        //  ⊂[⍳0]B   ←→   ⊂¨B
        Token part(TOK_FUN1, Bif_F12_PARTITION::fun);
        return Bif_OPER1_EACH::fun->eval_LB(part, B).get_apl_val();
      }

Value_P Z(shape_Z, LOC);
   if (Z->is_empty())
      {
         Z->set_default(*B.get(), LOC);
         Z->check_value(LOC);
         return Z;
      }

   for (ArrayIterator it_Z(shape_Z); it_Z.more(); ++it_Z)
      {
        const ShapeItem off_Z = it_Z.multiply(weight_Z);   // offset in Z

        Value_P vZ(item_shape, LOC);
        new (Z->next_ravel()) PointerCell(vZ.get(), Z.getref());

        if (item_shape.is_empty())
           {
             vZ->get_ravel(0).init(B->get_ravel(0), vZ.getref(), LOC);
           }
        else
           {
             for (ArrayIterator it_it(item_shape); it_it.more(); ++it_it)
                 {
                   const ShapeItem off_B =  // offset in B
                         it_it.multiply(it_weight);
                   vZ->next_ravel()->init(B->get_ravel(off_Z + off_B),
                                                       vZ.getref(), LOC);
                 }
           }

        vZ->check_value(LOC);
      }

   Z->check_value(LOC);
   return Z;
}
//-----------------------------------------------------------------------------
Token
Bif_F12_PARTITION::partition(Value_P A, Value_P B, Axis axis)
{
   if (A->get_rank() > 1)    RANK_ERROR;
   if (B->get_rank() == 0)   RANK_ERROR;

   if (A->is_scalar())
      {
        APL_Integer val = A->get_ravel(0).get_near_int();
        if (val == 0)
           {
             return Token(TOK_APL_VALUE1, Idx0(LOC));
           }

        return eval_B(B);
      }

   if (A->get_shape_item(0) != B->get_shape_item(axis))   LENGTH_ERROR;

   // determine the length of the partitioned dimension...
   //
ShapeItem len_Z = 0;
   {
     ShapeItem prev = 0;
     loop(l, A->get_shape_item(0))
        {
           const APL_Integer am = A->get_ravel(l).get_near_int();
           if (am < 0)   DOMAIN_ERROR;
           if (am  > prev)   ++len_Z;
           prev = am;
        }
   }

Shape shape_Z(B->get_shape());
   shape_Z.set_shape_item(axis, len_Z);

Value_P Z(shape_Z, LOC);


   if (Z->is_empty())
      {
        Z->set_default(*B.get(), LOC);

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

const Shape3 shape_B3(B->get_shape(), axis);
Cell * cZ;

   loop(h, shape_B3.h())
   loop(l, shape_B3.l())
       {
         ShapeItem from = -1;
         ShapeItem prev_am = 0;
         ShapeItem zm = 0;
         cZ = &Z->get_ravel(h*len_Z*shape_B3.l() + l);

         loop(m, shape_B3.m())
             {
               const APL_Integer am = A->get_ravel(m).get_near_int();
               Assert(am >= 0);   // already verified above.

               if (am == 0)   // skip element m (and following)
                  {
                    if (from != -1)   // an old segment is pending
                       {
                         copy_segment(cZ + zm, Z.getref(), h,
                                      from, m, shape_B3.m(),
                                      l, shape_B3.l(), B);
                         zm += shape_B3.l();
                       }
                    from = -1;
                  }
               else if (am > prev_am)   // new segment
                  {
                    if (from != -1)   // an old segment is pending
                       {
                         copy_segment(cZ + zm, Z.getref(), h, from, m,
                                      shape_B3.m(), l, shape_B3.l(), B);
                         zm += shape_B3.l();
                       }
                    from = m;
                  }
               prev_am = am;
             }

         if (from != -1)   // an old segment is pending
            {
              copy_segment(cZ + zm, Z.getref(), h, from, shape_B3.m(),
                           shape_B3.m(), l, shape_B3.l(), B);

              // zm += shape_B3.l();   not needed since we are done.
            }
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
void
Bif_F12_PARTITION::copy_segment(Cell * dest, Value & dest_owner,
                                ShapeItem h, ShapeItem m_from,
                                ShapeItem m_to, ShapeItem m_len,
                                ShapeItem l, ShapeItem l_len, Value_P B)
{
   Assert(m_from < m_to);
   Assert(m_to <= m_len);
   Assert(l < l_len);

Value_P V(m_to - m_from, LOC);

Cell * vv = &V->get_ravel(0);
   for (ShapeItem m = m_from; m < m_to; ++m)
       {
         const Cell & cb = B->get_ravel(l + (m + h*m_len)*l_len);
         vv++->init(cb, V.getref(), LOC);
       }

   V->check_value(LOC);
   new (dest) PointerCell(V.get(), dest_owner);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_PICK::disclose(Value_P B, bool rank_tolerant)
{
   if (B->is_simple_scalar())   return Token(TOK_APL_VALUE1, B);

const Shape item_shape = compute_item_shape(B, rank_tolerant);
const Shape shape_Z = B->get_shape() + item_shape;

const ShapeItem len_B = B->element_count();
   if (len_B == 0)
      {
         Value_P first = Bif_F12_TAKE::first(B);
         Token result = disclose(first, rank_tolerant);
         if (result.get_Class() == TC_VALUE)   // success
            result.get_apl_val()->set_shape(shape_Z);
         return result;
      }

Value_P Z(shape_Z, LOC);

const ShapeItem llen = item_shape.get_volume();

   if (llen == 0)   // empty enclosed value
      {
        const Cell & B0 = B->get_ravel(0);
         if (B0.is_pointer_cell())
            {
              Value_P vB = B0.get_pointer_value();
              Value_P B_proto = vB->prototype(LOC);
              Z->get_ravel(0).init(B_proto->get_ravel(0), Z.getref(), LOC);
            }
         else
            {
              Z->get_ravel(0).init(B0, Z.getref(), LOC);
            }

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   loop(h, len_B)
       {
         const Cell & B_item = B->get_ravel(h);
         Cell * Z_from = &Z->get_ravel(h*llen);
         if (B_item.is_pointer_cell())
            {
              Value_P vB = B_item.get_pointer_value();
              Bif_F12_TAKE::fill(item_shape, Z_from, Z.getref(), vB);
            }
         else if (B_item.is_lval_cell())
            {
              const Cell & pointee = *B_item.get_lval_value();
               if (pointee.is_pointer_cell())   // pointer to nested
                  {
                    Value_P vB = pointee.get_pointer_value();
                    Value_P ref_B = vB->get_cellrefs(LOC);
                    Bif_F12_TAKE::fill(item_shape, Z_from, Z.getref(), ref_B);
                  }
               else                             // pointer to simple scalar
                  {
                    Z_from->init(B_item, Z.getref(), LOC);
                    for (ShapeItem c = 1; c < llen; ++c)
                        new (Z_from + c) LvalCell(0, 0);
                  }
            }
         else if (B_item.is_character_cell())   // simple char scalar
            {
              Z_from->init(B_item, Z.getref(), LOC);
              for (ShapeItem c = 1; c < llen; ++c)
                  (Z_from + c)->init(c_filler, Z.getref(), LOC);
            }
         else                                   // simple numeric scalar
            {
              Z->get_ravel(h*llen).init(B_item, Z.getref(), LOC);
              for (ShapeItem c = 1; c < llen; ++c)
                  (Z_from + c)->init(n_filler, Z.getref(), LOC);
            }
       }

   Z->set_default(*B.get(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_PICK::disclose_with_axis(const Shape & axes_X, Value_P B,
                                 bool rank_tolerant)
{
   // disclose with axis

   // all items of B must have the same rank. item_shape is the smallest
   // shape that can contain each item of B.
   //
const Shape item_shape = compute_item_shape(B, rank_tolerant);

   // the number of items in X must be the number of axes in item_shape
   if (item_shape.get_rank() != axes_X.get_rank())   AXIS_ERROR;

   // distribute shape_B and item_shape into 2 shapes perm and shape_Z.
   // if a dimension is mentioned in axes_X then it goes into shape_Z and
   // otherwise into perm.
   //
Shape perm;
Shape shape_Z;
   {
     ShapeItem B_idx = 0;
     //
     loop(z, B->get_rank() + item_shape.get_rank())
        {
          // check if z is in X, remembering its position if so.
          //
          bool z_in_X = false;
          ShapeItem x_pos = -1;
          loop(x, axes_X.get_rank())
             if (axes_X.get_shape_item(x) == z)
                {
                  z_in_X = true;
                  x_pos = x;
                  break;
                }

          if (z_in_X)   // z is an item dimension: put it in shape_Z
             {
               shape_Z.add_shape_item(item_shape.get_shape_item(x_pos));
             }
          else          // z is a B dimension: put it in perm
             {
               if (B_idx >= B->get_rank())   INDEX_ERROR;
               perm.add_shape_item(z);
               shape_Z.add_shape_item(B->get_shape_item(B_idx++));
             }
        }

     // append X to perm with each X item reduced by the B items before it.
     loop(x, axes_X.get_rank())
        {
          Rank before_x = 0;   // items before X that are not in X
          loop(x1, x - 1)
             if (!axes_X.contains_axis(x1))   ++before_x;

          perm.add_shape_item(axes_X.get_shape_item(x) + before_x);
        }
   }

Value_P Z(shape_Z, LOC);
   if (Z->is_empty())
      {
         Z->set_default(*B.get(), LOC);
         return Token(TOK_APL_VALUE1, Z);
      }

   // loop over sources and place them in the result.
   //
PermutedArrayIterator it_Z(shape_Z, perm);

   for (ArrayIterator it_B(B->get_shape()); it_B.more(); ++it_B)
      {
        const Cell & B_item = B->get_ravel(it_B());
        const Cell * src = 0;
        if (B_item.is_pointer_cell())
           {
             Value_P vB = B_item.get_pointer_value();
             ArrayIterator vB_it(vB->get_shape());
             for (ArrayIterator it_it(item_shape); it_it.more(); ++it_it)
                 {
                   if (vB->get_shape().contains(it_it.get_offsets()))
                      {
                        src = &vB->get_ravel(vB_it());
                        ++vB_it;
                      }
                   else if (vB->get_ravel(0).is_character_cell())  // char
                        src = &c_filler;
                   else                                   // simple numeric
                        src = &n_filler;

                   Z->get_ravel(it_Z()).init(*src, Z.getref(), LOC);
                   ++it_Z;
                 }
           }
        else
           {
             for (ArrayIterator it_it(item_shape); it_it.more(); ++it_it)
                 {
                   if (it_it() == 0)   // first element: use B_item
                      {
                        src = &B_item;
                      }
                   else if (B_item.is_character_cell())   // simple char scalar
                      {
                        src = &c_filler;
                      }
                   else                                // simple numeric scalar
                      {
                        src = &n_filler;
                      }

                   Z->get_ravel(it_Z()).init(*src, Z.getref(), LOC);
                   ++it_Z;
                 }
            }
      }

   Z->set_default(*B.get(), LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Shape
Bif_F12_PICK::compute_item_shape(Value_P B, bool rank_tolerant)
{
   // all items of B are scalars or (nested) arrays of the same rank R.
   // return the shape with rank R and the (per-dimension) max. of
   // each shape item
   //
const ShapeItem len_B = B->nz_element_count();

Shape ret;   // of the first non-scalar in B

   loop(b, len_B)
       {
         Value_P v;
         {
           const Cell & cB = B->get_ravel(b);
           if (cB.is_pointer_cell())
              {
                v = cB.get_pointer_value();
              }
           else if (cB.is_lval_cell())
              {
                const Cell & pointee = *cB.get_lval_value();
                if (!pointee.is_pointer_cell())   continue;  // ptr to scalar
                v = pointee.get_pointer_value();
              }
           else
              {
                continue;   // simple scalar
              }
         }

         if (ret.get_rank() == 0)   // first non-scalar
            {
              ret = v->get_shape();
              continue;
            }

         // the items of B must have the same rank, unless we are rank_tolerant
         //
         if (ret.get_rank() != v->get_rank())
            {
              if (!rank_tolerant)   RANK_ERROR;

              if (ret.get_rank() < v->get_rank())
                 ret.expand_rank(v->get_rank());

              // if ret.get_rank() > v->get_rank() then we are OK because
              // only the dimensions present in v are expanded below.
            }

         loop(r, v->get_rank())
             {
               if (ret.get_shape_item(r) < v->get_shape_item(r))
                  {
                    ret.set_shape_item(r, v->get_shape_item(r));
                  }
             }
       }

   return ret;
}
//-----------------------------------------------------------------------------
Token
Bif_F12_PICK::eval_AB(Value_P A, Value_P B)
{
   if (A->get_rank() > 1)    RANK_ERROR;

const ShapeItem ec_A = A->element_count();

   // if A is empty, return B
   //
   if (ec_A == 0)   return Token(TOK_APL_VALUE1, B);

const APL_Integer qio = Workspace::get_IO();

Value * B_cellowner = B->get_lval_cellowner();
Value_P Z = pick(&A->get_ravel(0), ec_A, B, qio, B_cellowner);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Value_P
Bif_F12_PICK::pick(const Cell * cA, ShapeItem len_A, Value_P B,
                   APL_Integer qio, Value * cell_owner)
{
ShapeItem c = 0;

   if (cA->is_pointer_cell())   // then B shall be a 1-dimensional array
      {
        Value_P A = cA->get_pointer_value();
        if (A->get_rank() > 1)   RANK_ERROR;

        const ShapeItem len_A = A->element_count();
        if (B->get_rank() != len_A)   RANK_ERROR;

        const Shape weight = B->get_shape().reverse_scan();
        const Shape A_as_shape(A.get(), qio);

        loop(r, A->element_count())
            {
              const ShapeItem ar = A_as_shape.get_shape_item(r);
              if (ar < 0)                       INDEX_ERROR;
              if (ar >= B->get_shape_item(r))   INDEX_ERROR;
              c += weight.get_shape_item(r) * ar;
            }
      }
   else   // A is a scalar, so B must be a vector.
      {
        if (B->get_rank() != 1)         RANK_ERROR;
        const APL_Integer a = cA->get_near_int() - qio;
        if (a < 0)                       INDEX_ERROR;
        if (a >= B->get_shape_item(0))   INDEX_ERROR;
        c = a;
      }

const Cell * cB = &B->get_ravel(c);

   if (len_A > 1)   // more levels coming.
      {
        if (cB->is_pointer_cell())
           {
             return pick(cA + 1, len_A - 1, cB->get_pointer_value(),
                         qio, cell_owner);
           }

        if (cB->is_lval_cell())
           {
             Assert(cell_owner);
             Cell & cell = *cB->get_lval_value();
             if (!cell.is_pointer_cell())   DOMAIN_ERROR;

             Value_P subval = cell.get_pointer_value();
             Value_P subrefs = subval->get_cellrefs(LOC);
             Value * sub_cellowner = subrefs->get_lval_cellowner();
             Assert(sub_cellowner);
             return pick(cA + 1, len_A - 1, subrefs, qio, sub_cellowner);
           }

        // at this point the depth implied by A is greater than the
        // depth of B.
        //
        RANK_ERROR;   // ISO p.166
      }

   // at this point, cB is the cell in B pick'ed by A->
   //
   if (cB->is_pointer_cell())
      {
        Value_P Z = cB->get_pointer_value()->clone(LOC);
        return Z;
      }

   if (cB->is_lval_cell())   // e.g. (A⊃B) ← C
      {
        Assert(cell_owner);

        Cell * cell = cB->get_lval_value();
        Assert(cell);

        Value_P Z(LOC);
        if (cell->is_pointer_cell())
           new (Z->next_ravel())   LvalCell_picked(cell, cell_owner);
        else
           new (Z->next_ravel())   LvalCell(cell, cell_owner);
        return Z;
      }
   else
      {
        Value_P Z(LOC);
        Z->next_ravel()->init(*cB, Z.getref(), LOC);
        return Z;
      }
}
//-----------------------------------------------------------------------------
Token
Bif_F2_INDEX::eval_AB(Value_P A, Value_P B)
{
   if (A->get_rank() > 1)   RANK_ERROR;

const ShapeItem ec_A = A->element_count();
   if (ec_A != B->get_rank())   RANK_ERROR;

   // index_expr is in reverse order!
   //
IndexExpr index_expr(ASS_none, LOC);
   loop(a, ec_A)
      {
         const Cell & cell = A->get_ravel(ec_A - a - 1);
         Value_P val;
         if (cell.is_pointer_cell())
            {
              val = cell.get_pointer_value()->clone(LOC);
              if (val->compute_depth() > 1)   DOMAIN_ERROR;
            }
        else
            {
              const APL_Integer i = cell.get_near_int();
              val = Value_P(LOC);
              if (i < 0)   DOMAIN_ERROR;

              new (&val->get_ravel(0))   IntCell(i);
            }

        index_expr.add(val);
      }

   // the index() do set_default() and check_value(), so we return immediately
   //
   index_expr.quad_io = Workspace::get_IO();

   if (index_expr.value_count() == 1)   // one-dimensional index
      {
        Value_P single_index = index_expr.extract_value(0);
        Value_P Z = B->index(single_index);
        return Token(TOK_APL_VALUE1, Z);
      }
   else
      {
        Value_P Z = B->index(index_expr);
        return Token(TOK_APL_VALUE1, Z);
      }
}
//-----------------------------------------------------------------------------
Token
Bif_F2_INDEX::eval_AXB(Value_P A, Value_P X, Value_P B)
{
   if (A->get_rank() > 1)   RANK_ERROR;

const Shape axes_present = Value::to_shape(X.get());

const ShapeItem ec_A = A->element_count();
   if (ec_A != axes_present.get_rank())   RANK_ERROR;
   if (ec_A > B->get_rank())              RANK_ERROR;

   // index_expr is in reverse order!
   //
const APL_Integer qio = Workspace::get_IO();
IndexExpr index_expr(ASS_none, LOC);
   loop(rb, B->get_rank())   index_expr.add(Value_P());
   index_expr.quad_io = qio;

   loop(a, ec_A)
      {
         const Rank axis = axes_present.get_shape_item(a);

         const Cell & cell = A->get_ravel(a);
         Value_P val;
         if (cell.is_pointer_cell())
            {
              val = cell.get_pointer_value()->clone(LOC);
              if (val->compute_depth() > 1)   DOMAIN_ERROR;
            }
        else
            {
              const APL_Integer i = cell.get_near_int();
              val = Value_P(LOC);
              if (i < 0)   DOMAIN_ERROR;

              new (&val->get_ravel(0))   IntCell(i);
            }

        index_expr.set_value(axis, val);
      }

   if (index_expr.value_count() == 1)
      {
        Value_P single_index = index_expr.extract_value(0);
        Value_P Z = B->index(single_index);

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }
   else
      {
        Value_P Z = B->index(index_expr);

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }
}
//=============================================================================
Token
Bif_F12_EQUIV::eval_B(Value_P B)
{
const Depth depth = B->compute_depth();

Value_P Z(LOC);
   new (Z->next_ravel()) IntCell(depth);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_EQUIV::eval_AB(Value_P A, Value_P B)
{
   // match
   //

const double qct = Workspace::get_CT();
const ShapeItem count = A->nz_element_count();  // compare at least prototype

   if (!A->same_shape(*B))   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));   // no match

   loop(c, count)
       if (!A->get_ravel(c).equal(B->get_ravel(c), qct))
          {
            return Token(TOK_APL_VALUE1, IntScalar(0, LOC));   // no match
          }

   return Token(TOK_APL_VALUE1, IntScalar(1, LOC));   // match
}
//=============================================================================
Token
Bif_F12_NEQUIV::eval_B(Value_P B)
{
   // Tally
   //
const ShapeItem len = B->is_scalar() ? 1 : B->get_shape().get_shape_item(0);

   return Token(TOK_APL_VALUE1, IntScalar(len, LOC));   // match
}
//-----------------------------------------------------------------------------
Token
Bif_F12_NEQUIV::eval_AB(Value_P A, Value_P B)
{
   // match
   //

const double qct = Workspace::get_CT();
const ShapeItem count = A->nz_element_count();  // compare at least prototype

   if (!A->same_shape(*B))   return Token(TOK_APL_VALUE1, IntScalar(1, LOC));   // no match

   loop(c, count)
       if (!A->get_ravel(c).equal(B->get_ravel(c), qct))
          {
            return Token(TOK_APL_VALUE1, IntScalar(1, LOC));   // no match
          }

   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));   // match
}
//=============================================================================
Token
Bif_F1_EXECUTE::eval_B(Value_P B)
{
   if (B->get_rank() > 1)   RANK_ERROR;

UCS_string statement(*B.get());

   if (statement.size() == 0)   return Token(TOK_NO_VALUE);

   return execute_statement(statement);
}
//-----------------------------------------------------------------------------
Token
Bif_F1_EXECUTE::execute_statement(UCS_string & statement)
{
   statement.remove_leading_and_trailing_whitespaces();

   // check for commands
   //
   if (statement.size() &&
       (statement[0] == UNI_ASCII_R_PARENT ||
        statement[0] == UNI_ASCII_R_BRACK))   return execute_command(statement);

ExecuteList * fun = ExecuteList::fix(statement.no_pad(), LOC);
   if (fun == 0)   SYNTAX_ERROR;

   Log(LOG_UserFunction__execute)   fun->print(CERR);

   // important special case: ⍎ of an APL literal value
   //
   if (fun->get_body().size() == 2 &&
       fun->get_body()[0].get_Class() == TC_VALUE)
      {
        Value_P Z = fun->get_body()[0].get_apl_val();
        delete fun;
        return Token(TOK_APL_VALUE1, Z);
      }

   Workspace::push_SI(fun, LOC);

   Log(LOG_StateIndicator__push_pop)
      {
        Workspace::SI_top()->info(CERR, LOC);
      }

   return Token(TOK_SI_PUSHED);
}
//-----------------------------------------------------------------------------
Token
Bif_F1_EXECUTE::execute_command(UCS_string & command)
{
   if (copy_pending &&
       (
//      command.starts_iwith(")COPY")    ||
        command.starts_iwith(")ERASE")   ||
        command.starts_iwith(")FNS")     ||
        command.starts_iwith(")NMS")     ||
        command.starts_iwith(")QLOAD")   ||
        command.starts_iwith(")SYMBOLS") ||
        command.starts_iwith(")VARS")))
      {
        throw_apl_error(E_COPY_PENDING, LOC);
      }

   if (command.starts_iwith(")LOAD")  ||
       command.starts_iwith(")QLOAD") ||
       command.starts_iwith(")CLEAR") ||
       command.starts_iwith(")SIC"))
      {
        // the command modifies the SI stack. We throw E_COMMAND_PUSHED
        // but without displaying it. That should bring us back to
        // Command::do_APL_expression() with token.get_tag() == TOK_ERROR
        //
        Workspace::push_Command(command);
        Error error(E_COMMAND_PUSHED, LOC);
        throw error;
      }

UTF8_ostream out;
const bool user_cmd = Command::do_APL_command(out, command);
   if (user_cmd)   return execute_statement(command);

UTF8_string result_utf8 = out.get_data();
   if (result_utf8.size() == 0 ||
       result_utf8.back() != UNI_ASCII_LF)
      result_utf8 += '\n';

std::vector<ShapeItem> line_starts;
   line_starts.push_back(0);
   loop(r, result_utf8.size())
      {
        if (result_utf8[r] == UNI_ASCII_LF)   line_starts.push_back(r + 1);
      }

Value_P Z(ShapeItem(line_starts.size() - 1), LOC);
   loop(l, line_starts.size() - 1)
      {
        ShapeItem len;
        if (l < ShapeItem(line_starts.size() - 1))
           len = line_starts[l + 1] - line_starts[l] - 1;
        else
           len = result_utf8.size() - line_starts[l];

        UTF8_string line_utf8(&result_utf8[line_starts[l]], len);
        UCS_string line_ucs(line_utf8);
        Value_P ZZ(line_ucs, LOC);
        new (Z->next_ravel())   PointerCell(ZZ.get(), Z.getref());
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_UNION::eval_B(Value_P B)
{
   if (B->get_rank() > 1)   RANK_ERROR;

const double qct = Workspace::get_CT();
const ShapeItem len_B = B->element_count();
   if (len_B <= 1)   return Token(TOK_APL_VALUE1, B->clone(LOC));
   if (len_B >= 20)
      {
        const Cell ** cells_B = new const Cell *[2*len_B];
        const Cell ** cells_Z = cells_B + len_B;

        loop(b, len_B)   cells_B[b] = &B->get_ravel(b);

        Heapsort<const Cell *>::sort(cells_B, len_B, 0, Cell::compare_stable);

        ShapeItem idx_B = 0;
        ShapeItem len_Z = 0;
        while(idx_B < len_B)
            {
              const Cell * ref = cells_B[idx_B++];
              cells_Z[len_Z++] = ref;   // always take first
              while (idx_B < len_B && ref->equal(*cells_B[idx_B], qct)) ++idx_B;
            }

        // sort cells_Z by position so that the original order in B is
        // reconstructed
        //
        Heapsort<const Cell *>::sort(cells_Z, len_Z, 0, Cell::compare_ptr);

        Value_P Z(len_Z, LOC);
        loop(z, len_Z)   Z->next_ravel()->init(*cells_Z[z], Z.getref(), LOC);
        delete[] cells_B;
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // B is small, so an iterative search of unique elements is faster
   //
std::vector<const Cell *> items_Z;
   items_Z.reserve(len_B);

   loop(b, len_B)
      {
        const Cell & cell = B->get_ravel(b);
        if (is_unique(cell, items_Z, qct))   items_Z.push_back(&cell);
      }

   // build result value Z
   //
Value_P Z(items_Z.size(), LOC);
   loop(z, items_Z.size())
       Z->next_ravel()->init(*(items_Z[z]), Z.getref(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F2_INTER::eval_AB(Value_P A, Value_P B)
{
   if (A->get_rank() > 1)   RANK_ERROR;
   if (B->get_rank() > 1)   RANK_ERROR;

const ShapeItem len_A = A->element_count();
const ShapeItem len_B = B->element_count();

const double qct = Workspace::get_CT();

   if (len_A*len_B > 60*60)
      {
        // large A or B: sort A and B to speed up searches
        //
        const Cell ** cells_A = new const Cell *[2*len_A + len_B];
        const Cell ** cells_Z = cells_A + len_A;
        const Cell ** cells_B = cells_A + 2*len_A;

        loop(a, len_A)   cells_A[a] = &A->get_ravel(a);
        loop(b, len_B)   cells_B[b] = &B->get_ravel(b);

        Heapsort<const Cell *>::sort(cells_A, len_A, 0, Cell::compare_stable);
        Heapsort<const Cell *>::sort(cells_B, len_B, 0, Cell::compare_stable);

        ShapeItem len_Z = 0;
        ShapeItem idx_B = 0;
        loop(idx_A, len_A)
            {
              const Cell * ref = cells_A[idx_A];
              while (idx_B < len_B)
                  {
                    if (ref->equal(*cells_B[idx_B], qct))
                       {
                         cells_Z[len_Z++] = ref;   // A is in B
                         break;   // for idx_B → next idx_A
                       }

                    // B is much (by ⎕CT) smaller or greater than A
                    //
                    if (ref->greater(*cells_B[idx_B]))    ++idx_B;
                    else                                  break;
                 }
            }

        // sort cells_Z by position so that the original order in A is
        //  reconstructed
        //
        Heapsort<const Cell *>::sort(cells_Z, len_Z, 0, Cell::compare_ptr);
        Value_P Z(len_Z, LOC);
        loop(z, len_Z)   Z->next_ravel()->init(*cells_Z[z], Z.getref(), LOC);

        Z->set_default(*B, LOC);
        Z->check_value(LOC);
        delete[] cells_A;
        return Token(TOK_APL_VALUE1, Z);
      }
    else
      {
        // small A and B: use quadratic algorithm.
        //
        const Cell ** cells_Z = new const Cell *[len_A];
        ShapeItem len_Z = 0;

        loop(a, len_A)
        loop(b, len_B)
            {
              if (A->get_ravel(a).equal(B->get_ravel(b), qct))
                 {
                   cells_Z[len_Z++] = &A->get_ravel(a);
                   break;   // loop(b)
                 }
            }
        Value_P Z(len_Z, LOC);
        loop(z, len_Z)
            Z->next_ravel()->init(*cells_Z[z], Z.getref(), LOC);

        Z->set_default(*B, LOC);
        Z->check_value(LOC);
        delete[] cells_Z;
        return Token(TOK_APL_VALUE1, Z);
      }
}
//-----------------------------------------------------------------------------
Token
Bif_F12_UNION::eval_AB(Value_P A, Value_P B)
{
   if (A->get_rank() > 1)   RANK_ERROR;
   if (B->get_rank() > 1)   RANK_ERROR;

   // A ∪ B ←→ A,B∼A
   //
Token BwoA = Bif_F12_WITHOUT::fun->eval_AB(B, A);

const ShapeItem len_A = A->element_count();
const ShapeItem len_B = BwoA.get_apl_val()->element_count();
Value_P Z(len_A + len_B, LOC);

   loop(a, len_A)   Z->next_ravel()->init(A->get_ravel(a), Z.getref(), LOC);
   loop(b, len_B)   Z->next_ravel()->init(B->get_ravel(b), Z.getref(), LOC);
   Z->set_default(*B, LOC);
   Z->check_value(LOC);
   return Bif_F12_COMMA::fun->eval_AB(A, BwoA.get_apl_val());
}
//=============================================================================
Token
Bif_F2_RIGHT::eval_AXB(Value_P A, Value_P X, Value_P B)
{
   // select corresponding items of A or B according to X. A, B, and X
   // must have matching shapes
   //
const int inc_A = A->is_scalar_extensible() ? 0 : 1;
const int inc_B = B->is_scalar_extensible() ? 0 : 1;
const int inc_X = X->is_scalar_extensible() ? 0 : 1;

   if (inc_X == 0)   // single item X: pick entire A or B according to X
      {
        const APL_Integer x0 = X->get_ravel(0).get_int_value();
        if (x0 == 0)   return Token(TOK_APL_VALUE1, A->clone(LOC));
        if (x0 == 1)   return Token(TOK_APL_VALUE1, B->clone(LOC));
        DOMAIN_ERROR;
      }

   // X is non-scalar, so it must match any non-scalar A and B
   //
   if (inc_A && ! X->same_shape(*A))
      {
        if (A->get_rank() != X->get_rank())   RANK_ERROR;
        else                                  LENGTH_ERROR;
      }

   if (inc_B && ! X->same_shape(*B))
      {
        if (B->get_rank() != X->get_rank())   RANK_ERROR;
        else                                  LENGTH_ERROR;
      }

Value_P Z(X->get_shape(), LOC);
   loop(z, Z->nz_element_count())
       {
        const APL_Integer xz = X->get_ravel(z).get_int_value();
        Cell * cZ = Z->next_ravel();
        if      (xz == 0)   cZ->init(A->get_ravel(z*inc_A), Z.getref(), LOC);
        else if (xz == 1)   cZ->init(B->get_ravel(z*inc_B), Z.getref(), LOC);
        else                DOMAIN_ERROR;
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//=============================================================================

