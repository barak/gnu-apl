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

#include "Bif_F12_PARTITION_PICK.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Bif_OPER2_INNER.hh"
#include "Bif_OPER1_REDUCE.hh"
#include "Macro.hh"
#include "PointerCell.hh"
#include "Workspace.hh"

Bif_OPER2_INNER   Bif_OPER2_INNER::fun;

Bif_OPER2_INNER::PJob_product Bif_OPER2_INNER::job;

//════════════════════════════════════════════════════════════════════════════
Token
Bif_OPER2_INNER::eval_ALRB(cValue_R A, Token & _LO, Token & _RO, cValue_R B) const
{
   if (!_LO.is_function() || !_RO.is_function())   SYNTAX_ERROR;

cFunction_P LO = _LO.get_function();
cFunction_P RO = _RO.get_function();
   Assert1(LO);
   Assert1(RO);

   // LO and RO must both be dyadic and must return a result.
   //
   // a monadic LO or RO: VALENCE ERROR, like for A F/ B (Bugs37 #67)
   //
   if (LO->get_fun_valence() != 2 || RO->get_fun_valence() != 2)
      {
        const cFunction_P bad = LO->get_fun_valence() != 2 ? LO : RO;
        MORE_ERROR() << "A f.g B : Bad function " << bad->get_name()
                     << " (expecting a dyadic function)";
        VALENCE_ERROR;
      }
   if (!LO->has_result() || !RO->has_result())
      {
        MORE_ERROR() << "A LO.RO B: both LO and RO must return a result;"
                        " LO is " << LO->get_name() << ", RO is "
                     << RO->get_name();
        DOMAIN_ERROR;
      }

   if (!A.is_scalar_extensible() &&
       !B.is_scalar_extensible() &&
       A.get_rank() > 1          &&
       B.get_rank() > 1          &&
       A.get_shape().get_last_shape_item() !=
       B.get_shape().get_shape_item(0)
      )
      {
        MORE_ERROR() << "A LO.RO B: expecting ¯1↑⍴A = 1↑⍴B; ¯1↑⍴A is "
                     << A.get_shape().get_last_shape_item()
                     << ", 1↑⍴B is " << B.get_shape().get_shape_item(0);
        LENGTH_ERROR;
      }

const Shape shape_A1 =A.get_shape().without_last_axis();
const ShapeItem len_A = A.get_last_shape_item();

const Shape shape_B1 = B.get_shape().without_first_axis();
const ShapeItem len_B = B.get_first_shape_item();

   // we do not check len_A == len_B here, since a non-scalar LO may
   // accept different lengths of its left and right arguments

const ShapeItem items_A1 = shape_A1.get_volume();
const ShapeItem items_B1 = shape_B1.get_volume();

   if (items_A1 == 0 || items_B1 == 0)   // empty result
      {
        // the outer product portion of LO.RO is empty.
        // Apply the fill function of RO
        //
        const Shape shape_Z = shape_A1 + shape_B1;
        return fill(shape_Z, CLONE(&A, LOC), RO, CLONE(&B, LOC), LOC);
      }

   if (LO->may_push_SI() || RO->may_push_SI())   // user defined LO or RO
      {
        // ISO: if A1 and B1 are both vectors, return f/A1 g B1.
        //
        if (A.get_rank() <= 1 && B.get_rank() <= 1)
           return Macro::get_macro(Macro::MAC_Z__vA_LO_INNER_RO_vB)
                       ->eval_ALRB(A, _LO, _RO, B);
        else
           return Macro::get_macro(Macro::MAC_Z__A_LO_INNER_RO_B)
                       ->eval_ALRB(A, _LO, _RO, B);
      }

Value_P Z(shape_A1 + shape_B1, LOC);

   // an important (and the most likely) special case is LO and RO being scalar
   // functions. This case can be implemented in a far simpler fashion than
   // the general case.
   //
   job.LO = LO->get_scalar_f2();
   job.RO = RO->get_scalar_f2();

   // must match the element_count()==1 test job.incA uses below (not
   // A.is_scalar(), i.e. rank==0): a 1-element but non-scalar A (e.g.
   // (1⍴5), rank 1) is scalar-extended by incA==0 below but was sized
   // as if it weren't here, truncating the inner product to LO_len==1
   // term. (1⍴5)+.×1 2 3 gave 5 instead of 30.
   //
const ShapeItem fast_LO_len = (len_A == 1) ? len_B : len_A;

   // fast_LO_len == 0 only when len_A == 1 (len_A itself is nonzero, per
   // the len_A check below) and len_B == 0: a 1-element/scalar A with a
   // 0-length inner axis on B. That reduces over zero terms, which has a
   // well-defined identity-element result, but the loop in
   // PF_scalar_inner_product() below writes nothing when LO_len == 0, so
   // route it through the general (enclose + Bif_OPER1_REDUCE) path
   // instead, which already handles empty-axis reduction correctly.
   //
   if (job.LO && job.RO && A.is_simple() && B.is_simple() && len_A
       && fast_LO_len)
      {
        job.incA   = (A.element_count() == 1) ? 0 : 1;
        job.incB   = (B.element_count() == 1) ? 0 : 1;

   // len_A must be len_B, unless at least one length is 1
   //
        if (len_A != len_B && job.incA && job.incB)
           {
             MORE_ERROR() << "A LO.RO B: expecting ¯1↑⍴A = 1↑⍴B (or one of"
                             " them 1); ¯1↑⍴A is " << len_A << ", 1↑⍴B is "
                          << len_B;
             LENGTH_ERROR;
           }

        job.VZ     = Z.get();
        job.idxZ   = 0;
        job.VA     = &A;
        job.idxA   = 0;
        job.ZAh    = items_A1;
        job.LO_len = fast_LO_len;
        job.VB     = &B;
        job.idxB   = 0;
        job.ZBl    = items_B1;
        job.ec     = E_NO_ERROR;

        scalar_inner_product();
        if (job.ec != E_NO_ERROR)   throw_apl_error(job.ec, LOC);

        Z->set_default(B, LOC);
 
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

const bool A_enclosed = A.get_rank() > 1;
const bool B_enclosed = B.get_rank() > 1;

Value_P A_enclosed_holder;
Value_P B_enclosed_holder;

const cValue * pA = &A;   // may be rebound if A_enclosed
const cValue * pB = &B;   // may be rebound if B_enclosed

   // enclose last axis of A if necessary
   //
   if (A_enclosed)
      {
        const Shape last_axis(A.get_rank() - 1);
        A_enclosed_holder = Bif_F12_PARTITION::enclose_with_axes(last_axis, CLONE(&A, LOC));
        pA = A_enclosed_holder.get();
      }

   // enclose first axis of B if necessary
   //
   if (B_enclosed)
      {
        const Shape first_axis(0);
        B_enclosed_holder = Bif_F12_PARTITION::enclose_with_axes(first_axis, CLONE(&B, LOC));
        pB = B_enclosed_holder.get();
      }

   loop (a, items_A1)
   loop (b, items_B1)
      {
        Value_P RO_A = CLONE(pA, LOC);
        if (A_enclosed)   RO_A = pA->get_pointer_value(a);

        Value_P RO_B = CLONE(pB, LOC);
        if (B_enclosed)   RO_B = pB->get_pointer_value(b);

        const Token T1 = RO->eval_AB(*RO_A, *RO_B);

        if (T1.get_tag() == TOK_ERROR)   return T1;

        Value_P A_RO_B = T1.get_apl_val();

        if (A_RO_B->is_simple_scalar())   // A_RO_B is A RO B
           {
             // A RO B has returned a scalar, so LO/A_RO_B is A_RO_B
             //
             Cell cache;
             Z->next_ravel_Cell(A_RO_B->get_cfirst(cache));
           }
        else
           {
             // A RO B has returned vector A_RO_B, compute LO/A_RO_B
             //
             const Token T2 = Bif_OPER1_REDUCE::reduce(_LO, A_RO_B,
                                                       A_RO_B->get_rank() - 1);
             if (T2.get_tag() == TOK_ERROR)   return T2;

             // V2 (the LO-reduction of A_RO_B) can be a *nested* scalar
             // (rank 0, e.g. ,/ of a non-scalar LO reducing to a single
             // enclosed item, or scalar-LO on nested arguments) --
             // is_simple_scalar() (rank 0 AND depth 0) is false for
             // that too, so the is_simple_scalar()/else split above
             // wrongly took the "wrap in a new PointerCell" branch for
             // an already-scalar V2, adding a spurious extra level of
             // enclosure (Bugs28 #34: 1 2 3,.,4 5 6 gave ⊂(⊂6⍴...)
             // instead of ⊂6⍴...). is_scalar() (rank 0, any depth) is
             // the right test: any scalar's sole cell -- simple or
             // itself a PointerCell -- can be copied into Z directly;
             // only a genuine (rank > 0) array needs a fresh
             // PointerCell wrapping it. (Note this is *not* the same
             // rule as next_ravel_Value()'s: that one is specifically
             // for strand-notation semantics, where an already-nested
             // scalar item deliberately gets one more level of nesting.)
             //
             Value_P V2 = T2.get_apl_val();
             if (V2->is_scalar())
                { Cell cache; Z->next_ravel_Cell(V2->get_cfirst(cache)); }
             else
                Z->next_ravel_Pointer(V2.get());
           }
      }

   Z->set_default(*pB, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_OPER2_INNER::scalar_inner_product() const
{
#ifdef cfg_PERFORMANCE_COUNTERS_WANTED
const uint64_t start_1 = cycle_counter();
#endif

  // the empty cases have been ruled out already in inner_product()

   job.ec = E_NO_ERROR;

#if PARALLEL_ENABLED
   if (  Parallel::run_parallel
      && Thread_context::get_active_core_count() > 1
      && job.ZAh * job.ZBl > get_dyadic_threshold())
      {
        job.cores = Thread_context::get_active_core_count();
        Thread_context::do_work = PF_scalar_inner_product;
        Thread_context::M_fork("scalar_inner_product");   // start pool
        PF_scalar_inner_product(Thread_context::get_master());
        Thread_context::M_join();
      }
   else
#endif // PARALLEL_ENABLED
      {
        job.cores = CCNT_1;
        PF_scalar_inner_product(Thread_context::get_master());
      }

#ifdef cfg_PERFORMANCE_COUNTERS_WANTED
const uint64_t end_1 = cycle_counter();
   Performance::fs_OPER2_INNER_AB.add_sample(end_1-start_1, job.ZAh * job.ZBl);
#endif
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_OPER2_INNER::fill(const Shape shape_Z, Value_P A, cFunction_P fun,
                      Value_P B, const char * loc)
{
   // this function is called from A f.g B when A fun B is called with an
   // empty A or B. In this case shape_Z is empty since A->get_shape() or
   // B->get_shape() (or both) contain axes of length 0.

Value_P Fill_A;   // argument A of the fill function
Value_P Fill_B;   // argument B of the fill function

   if (A->is_empty())   Fill_A = A->prototype(LOC);
   else                 Fill_A = Bif_F12_TAKE::first(*A);

   if (B->is_empty())   Fill_B = B->prototype(LOC);
   else                 Fill_B = Bif_F12_TAKE::first(*B);

Token tok = fun->eval_fill_AB(*Fill_A, *Fill_B);

   if (tok.get_Class() != TC_VALUE)   return tok;

Value * Z = tok.get_apl_val().get();

Value_P Z1(shape_Z, LOC);   // shape_Z is empty

   // Bugs28 #38: init_from_value() copied Z (RO's fill result, e.g.
   // the real value of Fill_A,Fill_B for a non-scalar RO like ,) into
   // Z1's prototype completely unconverted -- its actual cell VALUES
   // (not just its shape/nesting, which init_from_value() does handle
   // via its own is_simple_scalar()/PointerCell split) leaked into an
   // empty result's prototype, e.g. 0 'a' 'b' instead of 0 ' ' ' '.
   // set_ravel_Value() + to_type(false) mirrors the OUTER product fix
   // for the same class of bug: to_type() recurses through any nested
   // PointerCell, zeroing every leaf to its type-correct default (0 or
   // a space, never force-converted to numeric -- a fill result can
   // legitimately be character, as in the ,.,'ab' example above).
   //
   Z1->set_ravel_Value(0, Z);
   Z1->check_value(LOC);
   Z1->to_type(false);
   return Token(TOK_APL_VALUE1, Z1);
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_OPER2_INNER::PF_scalar_inner_product(Thread_context & tctx)
{
const ShapeItem Z_len = job.ZAh * job.ZBl;

const ShapeItem slice_len = (Z_len + job.cores - 1)/job.cores;
ShapeItem z = tctx.get_N() * slice_len;
ShapeItem end_z = z + slice_len;
   if (end_z > Z_len)   end_z = Z_len;

   for (; z < end_z; ++z)
       {
        const ShapeItem zah = z/job.ZBl;         // z row = A row
        const ShapeItem zbl = z - zah*job.ZBl;   // z column = B column
        ShapeItem ridxA = job.idxA + job.incA*((zah + 1) * job.LO_len);
        ShapeItem cidxB = job.idxB + job.incB*(zbl + job.LO_len*job.ZBl);

        // compute Z[z] ← LO / (row_A RO colB)
        //   e.g.  Z[z] ← +/ (row_A × colB)
        //
        // we use Z[z] as accumulator for LO /
        //
        // we use the terms sum and product as if Z←A LO.RO B were A +.* B
        //
        Cell * sum = &job.VZ->get_wravel(job.idxZ + z);
        loop(l, job.LO_len)
           {
             ridxA -= job.incA;
             cidxB -= job.incB*job.ZBl;

             Cell cacheA;
             Cell cacheB;
             const Cell & cellA = job.VA->get_cravel(ridxA, cacheA);
             const Cell & cellB = job.VB->get_cravel(cidxB, cacheB);
             if (l == 0)   // store first product in Z[z]
                {
                  job.ec = (cellB.*job.RO)(sum, &cellA);
                  if (job.ec != E_NO_ERROR)   return;
                }
             else          // add subsequent product to Z[z]
                {
                  Cell product;   // the result of RO, e.g. of × in +/×
                  job.ec = (cellB.*job.RO)(&product, &cellA);
                  if (job.ec != E_NO_ERROR)   return;

                  job.ec = (sum->*job.LO)(sum, &product);
                  if (job.ec != E_NO_ERROR)   return;
                }
           }
       }
}
//════════════════════════════════════════════════════════════════════════════
