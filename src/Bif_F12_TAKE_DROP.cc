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

#include "ArrayIterator.hh"
#include "Bif_OPER1_EACH.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Workspace.hh"

// primitive function instances
//
Bif_F12_TAKE      Bif_F12_TAKE     ::_fun;    // ↑
Bif_F12_DROP      Bif_F12_DROP     ::_fun;    // ↓

// primitive function pointers
//
Bif_F12_TAKE      * Bif_F12_TAKE     ::fun = &Bif_F12_TAKE     ::_fun;
Bif_F12_DROP      * Bif_F12_DROP     ::fun = &Bif_F12_DROP     ::_fun;

//=============================================================================
Value_P
Bif_F12_TAKE::first(Value_P B)
{
const Cell & first_B = B->get_ravel(0);
   if (B->element_count() == 0)   // empty value: return prototype
      {
        if (first_B.is_lval_cell())   // (
            {
              Value_P Z(LOC);
              Z->next_ravel()->init(first_B, B.getref(), LOC);
              return Z;
            }
        Value_P Z = B->prototype(LOC);
        Z->check_value(LOC);
        return Z;
      }

   if (!first_B.is_pointer_cell())   // simple cell
      {
        Value_P Z(LOC);
        Z->get_ravel(0).init(first_B, Z.getref(), LOC);
        Z->check_value(LOC);
        return Z;
      }

Value_P v1 = first_B.get_pointer_value();
Value * v1_owner = v1->get_lval_cellowner();
   if (v1_owner)   // B is a left value
      {
        Value_P B1(LOC);
        new (&B1->get_ravel(0))   PointerCell(v1.get(), B1.getref());

        Value_P Z(LOC);
        new (&Z->get_ravel(0))   LvalCell(&B1->get_ravel(0), v1_owner);

        Z->check_value(LOC);
        return Z;
      }
   else
      {
        const ShapeItem ec = v1->element_count();
        Value_P Z(v1->get_shape(), LOC);
        if (ec == 0)   Z->get_ravel(0).init(v1->get_ravel(0), Z.getref(), LOC);

        loop(e, ec)   Z->next_ravel()->init(v1->get_ravel(e), Z.getref(), LOC);

        Z->check_value(LOC);
        return Z;
      }
}
//-----------------------------------------------------------------------------
Token
Bif_F12_TAKE::eval_AXB(Value_P A, Value_P X, Value_P B)
{
   if (X->element_count() == 0)   // no axes
      {
        Token result(TOK_APL_VALUE1, B->clone(LOC));
        return result;
      }

   // A↑[X]B ←→ ⊃[X](⊂A)↑¨⊂[X]B
   //
Value_P cA = Bif_F12_PARTITION::fun->eval_B(A).get_apl_val();           // ⊂A
Value_P cB = Bif_F12_PARTITION::fun->eval_XB(X, B).get_apl_val();       // ⊂[X]B
Token take(TOK_FUN2, Bif_F12_TAKE::fun);
Value_P cT = Bif_OPER1_EACH::fun->eval_ALB(cA, take, cB).get_apl_val(); // cA↑¨cB

Token result = Bif_F12_PICK::fun->eval_XB(X, cT);
   return result;
}
//-----------------------------------------------------------------------------
Token
Bif_F12_TAKE::eval_AB(Value_P A, Value_P B)
{
Shape ravel_A1(A.get(), /* ⎕IO */ 0);   // checks that 1 ≤ ⍴⍴A and ⍴A ≤ MAX_RANK

   if (B->is_scalar())
      {
        Shape shape_B1;
        loop(a, ravel_A1.get_rank())   shape_B1.add_shape_item(1);
        Value_P B1 = B->clone(LOC);
        B1->set_shape(shape_B1);
        return do_take(ravel_A1, B1);
      }
   else
      {
        if (ravel_A1.get_rank() != B->get_rank())   LENGTH_ERROR;
        return do_take(ravel_A1, B);
      }
}
//-----------------------------------------------------------------------------
Token
Bif_F12_TAKE::do_take(const Shape shape_Zi, Value_P B)
{
   // shape_Zi can have negative items (for take from the end). Create
   // shape_Z which is a true shape with the absolute values of shape_Zi
   //
Shape shape_Z(shape_Zi);

   loop(r, shape_Zi.get_rank())
       {
         ShapeItem a = shape_Zi.get_shape_item(r);

         if (a < 0)    shape_Z.set_shape_item(r, -a);
       }

Value_P Z(shape_Z, LOC);


   fill(shape_Zi, &Z->get_ravel(0), Z.getref(), B);

   Z->set_default(*B.get(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
void
Bif_F12_TAKE::fill(const Shape & shape_Zi, Cell * cZ, Value & Z_owner,
                   Value_P B)
{
   if (shape_Zi.is_empty())   return;

   Assert(shape_Zi.get_rank() == B->get_rank());

   // compute shape_Z = abs(shape_Zi), and Q with Z[i] = B[Q + i].
   //
Shape shape_Z;
Shape Q;

   loop(r, shape_Zi.get_rank())
       {
         const ShapeItem Zi = shape_Zi.get_shape_item(r);

         if (Zi >= 0)
            {
              shape_Z.add_shape_item(Zi);
              Q.add_shape_item(0);
            }
         else
            {
              shape_Z.add_shape_item(-Zi);
              Q.add_shape_item(B->get_shape_item(r) + Zi);
            }
       }

   if (shape_Z.is_empty())   return;

const Shape weight_B = B->get_shape().reverse_scan();

   for (ArrayIterator it(shape_Z); !it.done(); ++it)
      {
        ShapeItem bpos = 0;
        bool fill = false;
        loop(r, shape_Z.get_rank())
            {
              const ShapeItem idx_B = it.get_value(r) + Q.get_shape_item(r);
              if ((idx_B <  0) || (idx_B >= B->get_shape_item(r)))
                 {
                   fill = true;
                   break;
                 }

              bpos += weight_B.get_shape_item(r) * idx_B;
            }

        if (fill)   cZ->init_type(B->get_ravel(0), Z_owner, LOC);
        else        cZ->init(B->get_ravel(bpos),   Z_owner, LOC);
        ++cZ;
      }
}
//=============================================================================
Token
Bif_F12_DROP::eval_AB(Value_P A, Value_P B)
{
Shape ravel_A(A.get(), /* ⎕IO */ 0);
   if (A->get_rank() > 1)   RANK_ERROR;

   if (B->is_scalar())
      {
        // if B is a scalar then the result rank shall be the length of A->
        // the result may be empty (shape 0 0 ... 0) if we drop something
        // or non-empty (shape 1 1 ... 1) if we drop nothing.
        //
        const ShapeItem len_Z = ravel_A.get_volume() ? 0 : 1;

        Shape shape_Z;
        loop(r, ravel_A.get_rank())   shape_Z.add_shape_item(len_Z);

        Value_P Z(shape_Z, LOC);

        Z->get_ravel(0).init(B->get_ravel(0), Z.getref(), LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (ravel_A.get_rank() == 0)   ravel_A.add_shape_item(1);   // A = ,A

   if (ravel_A.get_rank() != B->get_rank())   LENGTH_ERROR;

   loop(r, ravel_A.get_rank())
       {
         const APL_Integer a = ravel_A.get_shape_item(r);
         const ShapeItem amax = B->get_shape_item(r);

         if      (a >= amax)  ravel_A.set_shape_item(r, 0);
         else if (a >= 0)     ravel_A.set_shape_item(r, a - amax);
         else if (a > -amax)  ravel_A.set_shape_item(r, amax + a);
         else                 ravel_A.set_shape_item(r, 0);
       }

   return Bif_F12_TAKE::do_take(ravel_A, B);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_DROP::eval_AXB(Value_P A, Value_P X, Value_P B)
{
   if (X->element_count() == 0)   // no axes
      {
        Token result(TOK_APL_VALUE1, B->clone(LOC));
        return result;
      }

   if (X->get_rank() > 1)    INDEX_ERROR;

const uint64_t len_X = X->element_count();
   if (len_X > MAX_RANK)     INDEX_ERROR;
   if (len_X == 0)           INDEX_ERROR;

   if (A->get_rank() > 1)    RANK_ERROR;

uint64_t len_A = A->element_count();
   if (len_A != len_X)   LENGTH_ERROR;

const APL_Integer qio = Workspace::get_IO();

   // init ravel_A = shape_B and seen.
   //
Shape ravel_A(B->get_shape());
bool seen[MAX_RANK];
   loop(r, B->get_rank())   seen[r] = false;

   loop(r, len_X)
       {
         const APL_Integer a = A->get_ravel(r).get_near_int();
         const APL_Integer x = X->get_ravel(r).get_near_int() - qio;

         if (x >= B->get_rank())   INDEX_ERROR;
         if (seen[x])              INDEX_ERROR;
         seen[x] = true;

         const ShapeItem amax = B->get_shape_item(x);
         if      (a >= amax)   ravel_A.set_shape_item(x, 0);
         else if (a >= 0)      ravel_A.set_shape_item(x, a - amax);
         else if (a > -amax)   ravel_A.set_shape_item(x, amax + a);
         else                  ravel_A.set_shape_item(x, 0);
       }

   return Bif_F12_TAKE::do_take(ravel_A, B);
}
//=============================================================================

