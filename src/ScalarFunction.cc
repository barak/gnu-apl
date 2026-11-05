
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

#include "ArgCheck.hh"
#include "ArrayIterator.hh"
#include "Avec.hh"
#include "Bif_F12_EQUIV.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IndexExpr.hh"
#include "IndexIterator.hh"
#include "IntCell.hh"
#include "PJob.hh"
#include "Parallel.hh"
#include "PointerCell.hh"
#include "PrintOperator.hh"
#include "ScalarFunction.hh"
#include "Value.hh"
#include "Workspace.hh"

// scalar function instances
//
Bif_F2_AND      Bif_F2_AND     ::fun;                // ∧
Bif_F2_AND_B    Bif_F2_AND_B   ::fun;                // ∧∧
Bif_F12_BINOM   Bif_F12_BINOM  ::fun;                // !
//════════════════════════════════════════════════════════════════════════════
Bif_F12_CIRCLE  Bif_F12_CIRCLE ::fun(false);         // ○
Bif_F12_CIRCLE  Bif_F12_CIRCLE ::fun_inverse(true);  // A inverted
Bif_F12_DIVIDE  Bif_F12_DIVIDE ::fun;                // ÷
Bif_F2_EQUAL    Bif_F2_EQUAL   ::fun;                // =
Bif_F2_EQUAL_B  Bif_F2_EQUAL_B ::fun;                // ==
Bif_F2_FIND     Bif_F2_FIND    ::fun;                // ⍷ (almost scalar)
Bif_F2_GREATER  Bif_F2_GREATER ::fun;                // >
Bif_F2_LEQU     Bif_F2_LEQU    ::fun;                // ≤
Bif_F2_LESS     Bif_F2_LESS    ::fun;                // <
Bif_F12_LOGA    Bif_F12_LOGA   ::fun;                // ⍟
Bif_F2_MEQU     Bif_F2_MEQU    ::fun;                // ≥
Bif_F12_MINUS   Bif_F12_MINUS  ::fun;                // -
Bif_F2_NAND     Bif_F2_NAND    ::fun;                // ⍲
Bif_F2_NAND_B   Bif_F2_NAND_B  ::fun;                // ⍲⍲
Bif_F2_NOR      Bif_F2_NOR     ::fun;                // ⍱
Bif_F2_NOR_B    Bif_F2_NOR_B   ::fun;                // ⍱⍱
Bif_F2_OR       Bif_F2_OR      ::fun;                // ∨
Bif_F2_OR_B     Bif_F2_OR_B    ::fun;                // ∨∨
Bif_F12_PLUS    Bif_F12_PLUS   ::fun(false);         // +
Bif_F12_PLUS    Bif_F12_PLUS   ::fun_inverse(true);  // +
Bif_F12_POWER   Bif_F12_POWER  ::fun;                // ⋆
Bif_F12_RND_UP  Bif_F12_RND_UP ::fun;                // ⌈
Bif_F12_RND_DN  Bif_F12_RND_DN ::fun;                // ⌊
Bif_F12_ROLL    Bif_F12_ROLL   ::fun;                // ? (monadic is scalar)
Bif_F12_STILE   Bif_F12_STILE  ::fun;                // ∣
Bif_F12_TIMES   Bif_F12_TIMES  ::fun(false);         // ×
Bif_F12_TIMES   Bif_F12_TIMES  ::fun_inverse(true);  // ×
Bif_F2_UNEQU    Bif_F2_UNEQU   ::fun;                // ≠
Bif_F2_UNEQ_B   Bif_F2_UNEQ_B  ::fun;                // ≠
Bif_F12_WITHOUT Bif_F12_WITHOUT::fun;                // ∼ (monadic is scalar)

const IntCell ScalarFunction::integer_0(0);
const IntCell ScalarFunction::integer_1(1);
const FloatCell ScalarFunction::float_min(-BIG_FLOAT);
const FloatCell ScalarFunction::float_max(BIG_FLOAT);

#ifdef PARALLEL_ENABLED

/// a lock that prevents different threads to access the same data structure
/// at the same time
Parallel::parallel_lock_t jobs_lock = LOCK_INITIALIZER;

#endif   // PARALLEL_ENABLED

PJob_scalar_AB * job_AB = 0;
PJob_scalar_B  * job_B  = 0;

//────────────────────────────────────────────────────────────────────────────
Token
ScalarFunction::do_eval_fill_AB(cValue_R A, cValue_R B) const
{
   /* eval_fill_AB() is called when A and/or B is empty and the non-empty
      argument (if any) is non-scalar (!!!). The scalar case for the non-empty
      argument of a dyadic scalar function is handled by do_eval_fill_B().

       NOTE however, that even though non-empty arguments of scalar functions
       must be scalars (and are therefore are not handled here but instead in
       do_eval_fill_B() below) A or B may still be non-scalar and non-empty
       when called from other places (such as  Bif_OPER2_INNER::fill()).

       NOTE also that there seems to be some confusion in lrm regarding
       fill functions. On p. 56 a description and examples for the fill
       function is given and IBM APL2 seem to follow that description.

       On p. 110/Figure 20 of lrm a different definition for the fill
       function for scalar functions is given, i.e.:

       Z←(R) ≠ (L)   with R←↑A and L←↑B   (*)

       The example on page 56 of lrm, i.e.:

       W←(ι0) ⌈ ι0
       DISPALY W

       gives:

       ┌⊖┐
       │0│
       └─┘

       in both APL2 (PC version) and in GNU APL while

      (↑⍳0) ≠ (↑⍳0)   (according to the definition on lrm p. 110/Figure 20)

      gives the simple numeric scalar

      0

      in both APL2 (PC version) and in GNU APL. Therefore the definition
      given on p. 110/Figure 20 looks wrong.
    */

   if (B.element_count() == 0)   return do_eval_fill_B(B);
   if (A.element_count() == 0)   return do_eval_fill_B(A);
   return do_eval_fill_B(A);
}
//────────────────────────────────────────────────────────────────────────────
Token
ScalarFunction::do_eval_fill_B(cValue_R B) const
{
   /* eval_fill_B() is called for:

      1.  a monadic scalar function with empty B, or
      2a. a dyadic (!) scalar function with empty A and scalar B, or
      2b. a dyadic (!) scalar function with scalar A and empty B.
    */

   // lrm p. 56: When the prototypes of the empty arguments are simple
   //            scalars, return a zero prototype
   //
   if (B.is_numeric(0) || B.is_character_cell(0))
      {
        // the comment above says "simple scalars", but B.get_shape() can
        // be non-scalar here: Bif_OPER1_EACH calls this with B already
        // replaced by Bif_F12_TAKE::first(B) -- for an empty nested B
        // that's the disclosed (and possibly non-scalar) prototype item,
        // not B itself. Fill every cell, not just cell 0, or cells
        // 1..n-1 are left as uninitialized garbage.
        //
        Value_P Z(B.get_shape(), LOC);
        while (Z->more())   Z->next_ravel_0();
        if (!Z->element_count())   Z->set_ravel_Int(0, 0);   // prototype
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   /* Apply do_eval_fill_B() recursively. Value::prototype() does not work
      here, so we clone() and to_type(true) where true forces numeric 0
      for character Cells.
    */
Value_P Z = B.clone(LOC);
   Z->check_value(LOC);

   // lrm p. 56: the fill function for primitive scalar functions is numeric
   // even if the (empty) argument has type character.
   //
   Z->to_type(/* force_numeric */ true);
   Z->check_value(LOC);

   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
/// build the )MORE arity-prefix "A<glyph>B" (e.g. "A+B") for \b fun's own
/// conforming_shape() checks -- ScalarFunction is the shared base of every
/// dyadic scalar primitive, so the primitive's glyph is only known via
/// its get_name() at the actual call site, not at compile time.
static UTF8_string
conforming_where(const Function * fun)
{
UCS_string ucs;
   ucs << "A" << fun->get_name() << "B";
   return UTF8_string(ucs);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
ScalarFunction::do_scalar_AB(ErrorCode & ec, cValue_R A, cValue_R B,
                             prim_f2 fun) const
{
const Shape * shape_Z = conforming_shape(ec, conforming_where(this).c_str(),
                                         A.get_shape(), B.get_shape());
   if (ec)   return Value_P();

const ShapeItem len_Z = shape_Z->get_volume();
   if (len_Z == 0)   return do_eval_fill_AB(A, B).get_apl_val();

Value_P Z(*shape_Z, LOC);

   // create a worklist with one top-level item that computes Z. If nested
   // values are detected while computing Z, then jobs for them are added
   // to the worklist.
   //
   {
     const PJob_scalar_AB job(Z, A, B);
     Thread_context::get_master().joblist_AB.start(job, LOC);
   }

#if PARALLEL_ENABLED
const bool maybe_parallel = Parallel::run_parallel &&
                           Thread_context::get_active_core_count() > 1;
#endif

   for (;;)
       {
         loop(a, Thread_context::get_active_core_count())
             {
               job_AB = Thread_context::get_context(CoreNumber(a))
                                   ->joblist_AB.next_job();
               if (job_AB)   break;
             }
         if (job_AB == 0)   break;   // all jobs done

#if PARALLEL_ENABLED
         if (maybe_parallel && job_AB->len_Z > get_dyadic_threshold())
            {
              // parallel execution...
              //
              job_AB->fun = this;
              job_AB->fun2 = fun;
              { const vv_f2f_t fvv = get_vv_f2f();
                job_AB->float64_fvv =
                   (fvv &&
                    !job_AB->value_A->get_pointer_cell_count() &&
                    !job_AB->value_B->get_pointer_cell_count() &&
                    job_AB->value_A->get_ravel_type() == RPT_FLOAT64 &&
                    job_AB->value_B->get_ravel_type() == RPT_FLOAT64)
                   ? fvv : nullptr;
              }
              { const vv_i2i_t fii = get_vv_i2i();
                job_AB->int64_fvv =
                   (!job_AB->float64_fvv &&
                    fii &&
                    !job_AB->value_A->get_pointer_cell_count() &&
                    !job_AB->value_B->get_pointer_cell_count() &&
                    job_AB->value_A->get_ravel_type() == RPT_INT64 &&
                    job_AB->value_B->get_ravel_type() == RPT_INT64)
                   ? fii : nullptr;
              }
              Thread_context::do_work = PF_scalar_AB;
              Thread_context::M_fork("eval_scalar_AB");   // start pool
              PF_scalar_AB(Thread_context::get_master());
PERFORMANCE_START(start_M_join)
              Thread_context::M_join();
              if (job_AB->float64_fvv)
                 job_AB->value_Z->commit_ravel_Float64(job_AB->len_Z);
              else if (job_AB->int64_fvv)
                 job_AB->value_Z->commit_ravel_Int64(job_AB->len_Z);
              ec = job_AB->error;
              if (ec != E_NO_ERROR)   return Value_P();
PERFORMANCE_END(fs_M_join_AB, start_M_join, 1);
            }
         else
#endif // PARALLEL_ENABLED
            {
              // sequential execution — try packed fast paths first
              //
PERFORMANCE_START(start_fast_AB)
              const bool fast_path_done =
                 job_AB->value_A->apply_fast_dyadic(*this,
                                                    *job_AB->value_B,
                                                    job_AB->inc_A,
                                                    job_AB->inc_B,
                                                    *job_AB->value_Z,
                                                    job_AB->len_Z);
              if (fast_path_done)
                 {
CELL_PERFORMANCE_END(get_statistics_AB(), start_fast_AB, false)
                 }
              if (!fast_path_done)
                 loop(z, job_AB->len_Z)
                    {
                   Cell cache_A, cache_B;
                   const Cell & cell_A = job_AB->A_at(z, cache_A);
                   const Cell & cell_B = job_AB->B_at(z, cache_B);
                   Cell & cell_Z       = job_AB->Z_at(z);

                   if (cell_A.is_pointer_cell() && cell_B.is_pointer_cell())
                      {
                        /* both cell_A and cell_B are nested, pointing to
                           Values A1 and B1 respectively.
                           cell_Z is nested, pointing to Z1←A1 fun B1.
                         */
                        Value_P A1 = cell_A.get_pointer_value();
                        Value_P B1 = cell_B.get_pointer_value();
                        const Shape * sh_Z1 =
                              conforming_shape(ec, conforming_where(this).c_str(),
                                              A1->get_shape(), B1->get_shape());
                        if (ec)   return Value_P();

                        const ShapeItem len_Z1 = sh_Z1->get_volume();
                        if (len_Z1 == 0)
                           {
                             // unlike every sibling branch here (both-simple,
                             // A-only-nested, B-only-nested, all of which
                             // write cell_Z in place via placement new),
                             // this one used to append to value_Z's *next*
                             // free ravel slot instead of writing cell_Z
                             // (position z) directly -- landing the result
                             // in slot 0 (or wherever an earlier empty pair
                             // had already advanced valid_ravel_items to)
                             // and leaving the real slot z uninitialised.
                             // See Bugs27 #10.
                             //
                             Value_P Z1 = do_eval_fill_AB(*A1, *B1).get_apl_val();
                             new (&cell_Z) PointerCell(Z1.get(),
                                                       *job_AB->value_Z);
                             continue;
                           }

                        Value_P Z1(*sh_Z1, LOC);
                        new (&cell_Z)
                            PointerCell(Z1.get(), *job_AB->value_Z, 0x6B616769);

                        const PJob_scalar_AB j1(Z1, *A1, *B1);
                        Thread_context::get_master().joblist_AB.add_job(j1);
                      }
                   else if (cell_A.is_pointer_cell())
                      {
                        /* cell_A is nested, pointing to Values A1.
                           cell_Z is nested, pointing to Z1←A1 fun B where B
                           is a scalar according to cell_B.
                         */
                        Value_P A1 = cell_A.get_pointer_value();
                        Value_P B1(cell_B, LOC);

                        const ShapeItem len_Z1 = A1->element_count();
                        if (len_Z1 == 0)
                           {
                             Value_P Z1 = do_eval_fill_B(*A1).get_apl_val();
                             new (&cell_Z) PointerCell(Z1.get(),
                                                       *job_AB->value_Z);
                           }
                        else
                           {
                              Value_P Z1(A1->get_shape(), LOC);
                              if (!Z1)   WS_FULL;
                              new (&cell_Z)
                                  PointerCell(Z1.get(),*job_AB->value_Z,
                                              0x6B616769);

                              const PJob_scalar_AB j1(Z1, *A1, *B1);
                              Thread_context::get_master().joblist_AB
                                                          .add_job(j1);
                           }
                      }
                   else if (cell_B.is_pointer_cell())
                      {
                        /* cell_B is nested, pointing to Values B1.
                           cell_Z is nested, pointing to Z1←A fun B1 where A
                           is a scalar according to cell_A.
                         */
                        // B is nested, A is simple
                        //
                        Value_P B1 = cell_B.get_pointer_value();

                        const ShapeItem len_Z1 = B1->element_count();
                        if (len_Z1 == 0)
                           {
                             Value_P Z1 =
                                     do_eval_fill_B(*B1).get_apl_val();
                             new (&cell_Z) PointerCell(Z1.get(),
                                                       *job_AB->value_Z);
                           }
                        else
                           {
                             Value_P A1(cell_A, LOC);
                             Value_P Z1(B1->get_shape(), LOC);

                             new (&cell_Z)
                                 PointerCell(Z1.get(), *job_AB->value_Z,
                                             0x6B616769);

                             const PJob_scalar_AB j1(Z1, *A1, *B1);
                             Thread_context::get_master().joblist_AB
                                                         .add_job(j1);
                          }
                      }
                   else
                      {
                        /* cell_A and cell_B are both simple.
                           Compute cell_Z = cell_A fun cell_B
                         */
PERFORMANCE_START(start_2)

                        ec = (cell_B.*fun)(&cell_Z, &cell_A);
                        if (ec != E_NO_ERROR)
                           {
                             // give up the ownership of A, B, and Z
                             *job_AB = PJob_scalar_AB();
                             job_AB = 0;
                             return Value_P();
                           }
CELL_PERFORMANCE_END(get_statistics_AB(), start_2, z)
                      }
                 }
           }
        job_AB->value_Z->try_pack();
        job_AB->value_Z->check_value(LOC);
        *job_AB = PJob_scalar_AB();   // give up ownership of A, B, and Z.
        job_AB = 0;
      }

   Z->set_default(B, LOC);
   Z->check_value(LOC);

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
ScalarFunction::do_scalar_B(ErrorCode & ec, cValue_R B, prim_f1 fun) const
{
Value_P Z(B.get_shape(), LOC);

   // create a worklist with one top-level item job_B that computes Z.
   // If nested values are detected while computing Z, then new jobs for
   // them are added to the worklist.
   //
   {
     const PJob_scalar_B job_B(Z, B);
     Thread_context::get_master().joblist_B.start(job_B, LOC);
   }

#if PARALLEL_ENABLED
const bool maybe_parallel = Parallel::run_parallel &&
                            may_parallel()         &&
                            Thread_context::get_active_core_count() > 1;
#endif   // PARALLEL_ENABLED

   for (;;)
       {
         loop(a, Thread_context::get_active_core_count())
             {
               job_B = Thread_context::get_context(CoreNumber(a))
                                       ->joblist_B.next_job();
               if (job_B)   break;
             }
         if (job_B == 0)   break;   // all jobs done

#if PARALLEL_ENABLED
         if (maybe_parallel && job_B->len_Z > get_monadic_threshold())
            {
              // parallel execution...
              //
              job_B->fun = this;
              job_B->fun1 = fun;
              { const v_f2f_t fv = get_v_f2f();
                job_B->float64_fv =
                   (fv &&
                    !job_B->value_B->get_pointer_cell_count() &&
                    job_B->value_B->get_ravel_type() == RPT_FLOAT64)
                   ? fv : nullptr;
              }
              { const v_i2i_t fi = get_v_i2i();
                job_B->int64_fv =
                   (!job_B->float64_fv &&
                    fi &&
                    !job_B->value_B->get_pointer_cell_count() &&
                    job_B->value_B->get_ravel_type() == RPT_INT64)
                   ? fi : nullptr;
              }
              Thread_context::do_work = PF_scalar_B;
              Thread_context::M_fork("eval_scalar_B");   // start pool
              PF_scalar_B(Thread_context::get_master());
PERFORMANCE_START(start_M_join)
              Thread_context::M_join();
              if (job_B->float64_fv)
                 job_B->value_Z->commit_ravel_Float64(job_B->len_Z);
              else if (job_B->int64_fv)
                 job_B->value_Z->commit_ravel_Int64(job_B->len_Z);
              if (job_B->error != E_NO_ERROR)
                 {
                   ec = job_B->error;
                   return Value_P();
                 }
PERFORMANCE_END(fs_M_join_B, start_M_join, 1);
            }
         else
#endif // PARALLEL_ENABLED
            {
              // sequential execution — try packed fast paths first
              //
PERFORMANCE_START(start_fast_B)
              const bool fast_path_done =
                 job_B->value_B->apply_fast_monadic(*this,
                                                    *job_B->value_Z,
                                                    job_B->len_Z);
              if (fast_path_done)
                 {
CELL_PERFORMANCE_END(get_statistics_B(), start_fast_B, false)
                 }
              if (!fast_path_done)
                 loop(z, job_B->len_Z)
                    {
                   Cell cache_B;
                   const Cell & cell_B = job_B->B_at(z, cache_B);
                   Cell & cell_Z       = job_B->Z_at(z);

                   if (Value_P B1 = cell_B.try_pointer_value())   // nested B-item
                      {
                        Value_P Z1(B1->get_shape(), LOC);
                        new (&cell_Z) PointerCell(Z1.get(),
                                                  *job_B->value_Z, 0x6B616769);

                        const PJob_scalar_B j1(Z1, *B1);
                        Thread_context::get_master().joblist_B.add_job(j1);
                      }
                   else                            // simple B-item
                      {
PERFORMANCE_START(start_2)
                        ec = (cell_B.*fun)(&cell_Z);
                        if (ec != E_NO_ERROR)
                           {
                             *job_B = PJob_scalar_B();   // give up ownership
                                                          // of B and Z
                             job_B = 0;
                             return Value_P();
                           }
CELL_PERFORMANCE_END(get_statistics_B(), start_2, z)
                      }
                 }
           }
        job_B->value_Z->try_pack();
        job_B->value_Z->check_value(LOC);
        *job_B = PJob_scalar_B();   // give up ownership of B and Z.
        job_B = 0;
      }

   Z->check_value(LOC);

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Token
ScalarFunction::eval_scalar_AB(cValue_R A, cValue_R B, prim_f2 fun) const
{
PERFORMANCE_START(start)

ErrorCode ec = E_NO_ERROR;
Value_P Z;
   try
      {
        Z = do_scalar_AB(ec, A, B, fun);
      }
   catch (...)
      {
        // DOMAIN_ERROR and friends thrown from deep inside a scalar
        // cell-op (e.g. 'a'∨1) unwind straight past the ec != E_NO_ERROR
        // check below, so without this catch the retained jobs/current_job
        // (see PJob::cancel_jobs()) are never released -- Bugs18 #3.
        //
        Thread_context::cancel_all_dyadic_jobs();
        throw;
      }
   if (ec != E_NO_ERROR)
      {
        Thread_context::cancel_all_dyadic_jobs();
        throw_apl_error(ec, LOC);
      }

PERFORMANCE_END(fs_SCALAR_AB, start, Z->nz_element_count());

   loop(a, Thread_context::get_active_core_count())
       Assert(Thread_context::get_context(CoreNumber(a))
                            ->joblist_AB.get_size() == 0);

   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
ScalarFunction::eval_scalar_AXB(cValue_R A, cValue_R X, cValue_R B,
                                prim_f2 fun) const
{
PERFORMANCE_START(start_1)

   {
     int sec = 0;
     if (A.is_scalar_extensible())   ++sec;
     if (B.is_scalar_extensible())   ++sec;

     /* avoid a conflict between scalar extension and axis of 1-element
        values with different ranks, e.g.

        (1 1⍴'A') = [1] (1⍴'B')

         which could lead to the wrong (smaller rank) shape of the result
      */
     if (sec == 1 ||
         (sec == 2 && A.same_rank(B))   // ← conflict if ranks differ
        ) return eval_scalar_AB(A, B, fun);
   }

   if (X.get_rank() > 1)   AXIS_ERROR;

const APL_Integer qio = Workspace::get_IO();
const sRank rank_A = A.get_rank();
const sRank rank_B = B.get_rank();
AxesBitmap axes_X = 0;   // bitmap of axes in X
const ShapeItem len_X = X.element_count();

   loop(iX, len_X)
       {
         // apl2lrm.txt "Conditions for Axis Specification" (p.45): axis
         // evaluation requires the bracket data to be "the proper type"
         // (near-integer); otherwise AXIS ERROR, not the DOMAIN ERROR
         // that a raw get_near_int() would throw for e.g. a character.
         if (!X.is_near_int(iX))           AXIS_ERROR;
         APL_Integer i = X.get_near_int(iX) - qio;
         if (i < 0)                        AXIS_ERROR;   // axis i too small
         if (i >= rank_A && i >= rank_B)   AXIS_ERROR;   // axis i too large
         if (axes_X & 1 << i)              AXIS_ERROR;   // axis i used twice
         axes_X |= 1 << i;
       }

   // if A and B have the same rank, then all axes must be in and A f[X} B
   // is the same as A f B.
   //
   if (rank_A == rank_B)
      {
        if (rank_A != len_X)   AXIS_ERROR;
        return eval_scalar_AB(A, B, fun);
      }

   if (rank_A < rank_B)
      {
        if (rank_A != len_X)   AXIS_ERROR;

        Value_P Z = eval_scalar_AXB(A, axes_X, B, fun, false);
PERFORMANCE_END(fs_SCALAR_AB, start_1, Z->nz_element_count())
        return Token(TOK_APL_VALUE1, Z);
      }
    else
      {
        if (rank_B != len_X)   AXIS_ERROR;

        Value_P Z = eval_scalar_AXB(B, axes_X, A, fun, true);
PERFORMANCE_END(fs_SCALAR_AB, start_1, Z->nz_element_count())
        return Token(TOK_APL_VALUE1, Z);
      }
}
//════════════════════════════════════════════════════════════════════════════
Value_P
ScalarFunction::eval_scalar_AXB(cValue_R A, AxesBitmap axes_X, cValue_R B,
                                prim_f2 fun, bool reversed) const
{
   // A is the value with the smaller rank (possibly a scalar).
   // B the value with the larger rank (neve a scalar).
   //
   // If (reversed) then A and B have changed roles (in order to
   // make A the value with the smaller rank).

   // 1. check that A and B agree on the axes in X (common axes of A and B
   // nust have the same length).
   //
   {
     sRank rA = 0;
     loop(rB, B.get_rank())
         {
            if (axes_X & 1 << rB)
               {
                 // if the axis is in X then the corresponding shape items in
                 // A and B must agree.
                 //
                 if (B.get_shape_item(rB) != A.get_shape_item(rA++))
                    LENGTH_ERROR;
               }
         }
   }

Shape weights_A = A.get_shape().get_weights();

Value_P Z(B.get_shape(), LOC);

ShapeItem bI = 0;

   for (ArrayIterator it_B(B.get_shape()); it_B.has_more(); ++it_B)
       {
         ShapeItem wA = 0;   // weigth of the A axes in X
         sRank rA = 0;
         loop(rB, B.get_rank())
             {
               if (axes_X & 1 << rB)
                  {
                    wA += weights_A.get_shape_item(rA++)
                       * it_B.get_shape_offset(rB);
                  }
             }

         Cell cache_A, cache_B;
         const Cell & cA = A.get_cravel(wA, cache_A);
         const Cell & cB = B.get_cravel(bI++, cache_B);

         // restore the original order of A and B
         //
         if (reversed)   expand_nested(Z.get(), cB, cA, fun);
         else            expand_nested(Z.get(), cA, cB, fun);
       }

   Z->set_default(B, LOC);
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Token
ScalarFunction::eval_scalar_B(cValue_R B, prim_f1 fun) const
{
const ShapeItem len_Z = B.element_count();
   if (len_Z == 0)   return do_eval_fill_B(B);

PERFORMANCE_START(start)

ErrorCode ec = E_NO_ERROR;
Value_P Z;
   try
      {
        Z = do_scalar_B(ec, B, fun);   // sets ec
      }
   catch (...)
      {
        // see the matching catch in eval_scalar_AB() -- Bugs18 #3.
        //
        Thread_context::cancel_all_monadic_jobs();
        throw;
      }
   if (ec != E_NO_ERROR)
      {
        Thread_context::cancel_all_monadic_jobs();
        throw_apl_error(ec, LOC);
      }

PERFORMANCE_END(fs_SCALAR_B, start, Z->nz_element_count());

   loop(a, Thread_context::get_active_core_count())
       Assert(Thread_context::get_context(CoreNumber(a))
                            ->joblist_B.get_size() == 0);

   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
void
ScalarFunction::expand_nested(Value * Z, const Cell & cell_A,
                                         const Cell & cell_B, prim_f2 fun) const
{
   if (cell_A.is_pointer_cell())
      {
        if (cell_B.is_pointer_cell())   // nested A and nested B
           {
             Value_P value_A = cell_A.get_pointer_value();
             Value_P value_B = cell_B.get_pointer_value();
             Token token = eval_scalar_AB(*value_A, *value_B, fun);
             Z->next_ravel_Pointer(token.get_apl_val().get());
           }
        else                             // nested A and simple B
           {
             Value_P value_A = cell_A.get_pointer_value();
             Value_P scalar_B(cell_B, LOC);
             Token token = eval_scalar_AB(*value_A, *scalar_B, fun);
             Z->next_ravel_Pointer(token.get_apl_val().get());
           }
      }
   else                                  // A is simple
      {
        if (cell_B.is_pointer_cell())   // simple A and nested B
           {
             Value_P scalar_A(cell_A, LOC);
             Value_P value_B = cell_B.get_pointer_value();
             Token token = eval_scalar_AB(*scalar_A, *value_B, fun);
             Z->next_ravel_Pointer(token.get_apl_val().get());
           }
        else                             // simple A and simple B
          {
            const ShapeItem pos = Z->get_valid_item_count();
            Z->next_ravel_0();   // pre-init with 0
            // the returned ErrorCode used to be silently discarded here
            // (siblings do_scalar_AB/PF_scalar_AB check it) -- on
            // E_DOMAIN_ERROR the cell just kept its pre-init 0 instead of
            // raising an error, e.g. "1 2÷[1]2 2⍴0" silently gave 0 0/0 0.
            const ErrorCode ec = (cell_B.*fun)(&Z->get_wravel(pos), &cell_A);
            if (ec != E_NO_ERROR)   throw_apl_error(ec, LOC);
          }
      }
}
//────────────────────────────────────────────────────────────────────────────
const Shape *
ScalarFunction::conforming_shape(ErrorCode & ec, const char * where,
                                 const Shape & shape_A, const Shape & shape_B)
{
   return ArgCheck::check_same_shape_or_scalar_extensible(ec, where,
                                                           shape_A, shape_B);
}
//────────────────────────────────────────────────────────────────────────────
Token
ScalarFunction::eval_scalar_identity_fun(cValue_R B, sAxis axis,
                                         const Cell & FI0)
{
   // for scalar functions the result of the identity function for scalar
   // function F is defined as follows (lrm p. 210)
   //
   // Z←SRρB+F/ι0    with SR ←→ ⍴Z
   //
   // The term F/ι0 is passed as argument FI0, so that the above becomes
   //
   // Z←SRρB+FI0
   //

const Shape shape_Z = B.get_shape().without_axis(axis);

Value_P Z(shape_Z, LOC);

   if (Z->is_empty())
      {
        // Bugs28 #36: an empty array's prototype must be its *type*
        // (0 or ' '), not the reduction's identity element -- using
        // FI0 (e.g. 1 for ×/, ¯MAX_DOUBLE for ⌈/) as the prototype
        // made it visible through ≡, 2 ⎕TF, )SAVE/)DUMP and other
        // prototype-inspecting paths.
        //
        Z->set_default(B, LOC);
      }
   else
      {
        while (Z->more())   Z->next_ravel_Cell(FI0);
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
void
ScalarFunction::PF_scalar_AB(Thread_context & tctx)
{
#if PARALLEL_ENABLED

   /*
      the following functions are not thread-safe and must therefore be locked.
      We use different locks so that independent items do not lock each other.

      * DynamicObject (and therefore Value() and Value_P() constructors
      * PJob_scalar_AB::add_job(), PJob_scalar_B::add_job()
      * PointerCell constructors (change their parents)
    */

const CoreCount cores = Thread_context::get_active_core_count();

const ShapeItem slice_len = (job_AB->len_Z + cores - 1) / cores;
ShapeItem z = tctx.get_N() * slice_len;

ShapeItem end_z = z + slice_len;
   if (end_z > job_AB->len_Z)   end_z = job_AB->len_Z;

   if (job_AB->float64_fvv)   // parallel float64 fast path
      {
        if (z < end_z)
           {
PERFORMANCE_START(start_fast_ABp)
             double       * pZ = reinterpret_cast<double *>(
                                     &job_AB->value_Z->get_wfirst()) + z;
             const double * pA = job_AB->value_A->cravel_float64()
                                 + z * job_AB->inc_A;
             const double * pB = job_AB->value_B->cravel_float64()
                                 + z * job_AB->inc_B;
             job_AB->float64_fvv(pZ, pA, job_AB->inc_A,
                                 pB, job_AB->inc_B, end_z - z);
CELL_PERFORMANCE_END(job_AB->fun->get_statistics_AB(), start_fast_ABp, false)
           }
        return;
      }

   if (job_AB->int64_fvv)   // parallel int64 fast path
      {
        if (z < end_z)
           {
PERFORMANCE_START(start_fast_ABp)
             int64_t       * pZ = reinterpret_cast<int64_t *>(
                                      &job_AB->value_Z->get_wfirst()) + z;
             const int64_t * pA = job_AB->value_A->cravel_int64()
                                  + z * job_AB->inc_A;
             const int64_t * pB = job_AB->value_B->cravel_int64()
                                  + z * job_AB->inc_B;
             job_AB->int64_fvv(pZ, pA, job_AB->inc_A,
                                pB, job_AB->inc_B, end_z - z);
CELL_PERFORMANCE_END(job_AB->fun->get_statistics_AB(), start_fast_ABp, false)
           }
        return;
      }

   // cache_A/cache_B are this thread's own stack-local materialisation
   // slots for A_at()/B_at() on packed operands. value_A/value_B are a
   // single Value shared by every worker thread of this job; a plain
   // (shared, non-caller-owned) fetch would materialise packed cells into
   // that shared Value's ravel cache, which multiple threads then race on
   // concurrently -- confirmed live as a SEGFAULT for a primitive without
   // a packed fast path (falls through to this cell-by-cell loop) applied
   // to packed int64 operands.
   //
Cell cache_A, cache_B;
   for (; z < end_z; ++z)
       {
         const Cell & cell_A = job_AB->A_at(z, cache_A);
         const Cell & cell_B = job_AB->B_at(z, cache_B);
         Cell & cell_Z       = job_AB->Z_at(z);

         const bool scalar_A = ! cell_A.is_pointer_cell();
         const bool scalar_B = ! cell_B.is_pointer_cell();

         if (scalar_A && scalar_B)   // simple A and simple B
            {
PERFORMANCE_START(start_2)

              job_AB->error = (cell_B.*job_AB->fun2)(&cell_Z, &cell_A);
              if (job_AB->error != E_NO_ERROR)   return;

CELL_PERFORMANCE_END(job_AB->fun->get_statistics_AB(), start_2, z)
            }
         else                        // nested A and/or nested B
            {
              Parallel::acquire_lock(jobs_lock);
              if (scalar_B)   // nested A and simple B
                 {
                   Value_P A1 = cell_A.get_pointer_value();
                   Value_P B1(cell_B, LOC);
                   if (const ShapeItem len_Z1 = A1->element_count())
                      {
                        Value_P Z1(A1->get_shape(), LOC);
                        new (&cell_Z) PointerCell(Z1.get(), *job_AB->value_Z);

                        const PJob_scalar_AB j1(Z1, *A1, *B1);
                        tctx.joblist_AB.add_job(j1);
                      }
                   else               // empty A1 and simple B
                      {
                        Value_P Z1 = job_AB->fun->eval_fill_AB(*A1, *B1)
                                                 .get_apl_val();
                        new (&cell_Z) PointerCell(Z1.get(), *job_AB->value_Z);
                      }
                 }
              else if (scalar_A)   // simple A and nested B
                 {
                   Value_P A1(cell_A, LOC);
                   Value_P B1 = cell_B.get_pointer_value();
                   if (const ShapeItem len_Z1 = B1->element_count())
                      {
                        Value_P Z1(B1->get_shape(), LOC);
                        new (&cell_Z) PointerCell(Z1.get(), *job_AB->value_Z);

                        const PJob_scalar_AB j1(Z1, *A1, *B1);
                        tctx.joblist_AB.add_job(j1);
                     }
                   else            // simple A and empty B1
                      {
                        Value_P Z1 = job_AB->fun->eval_fill_AB(*A1, *B1)
                                                 .get_apl_val();
                        new (&cell_Z)   PointerCell(Z1.get(), *job_AB->value_Z);
                      }
                 }
              else                // nested A and nested B
                 {
                   Value_P A1 = cell_A.get_pointer_value();
                   Value_P B1 = cell_B.get_pointer_value();
                   const Shape * sh_Z1 =
                         conforming_shape(job_AB->error,
                                         conforming_where(job_AB->fun).c_str(),
                                         A1->get_shape(), B1->get_shape());
                   if (job_AB->error)
                      {
                        Parallel::release_lock(jobs_lock);
                        return;
                      }

                   if (const ShapeItem len_Z1 = sh_Z1->get_volume())
                      {
                        Value_P Z1(*sh_Z1, LOC);
                        new (&cell_Z) PointerCell(Z1.get(), *job_AB->value_Z);

                        const PJob_scalar_AB j1(Z1, *A1, *B1);
                        tctx.joblist_AB.add_job(j1);
                      }
                   else   // empty B1/Z1
                      {
                        Token result = job_AB->fun->eval_fill_AB(*A1, *B1);
                        if (result.get_tag() == TOK_ERROR)
                           {
                             job_AB->error = ErrorCode(result.get_int_val());
                             Parallel::release_lock(jobs_lock);
                             return;
                           }
                      }
                 }

              Parallel::release_lock(jobs_lock);
            }
       }

#endif   // PARALLEL_ENABLED
}
//════════════════════════════════════════════════════════════════════════════
void
ScalarFunction::PF_scalar_B(Thread_context & tctx)
{
#if PARALLEL_ENABLED

   /*
      the following functions are not thread-safe and must therefore be locked.
      We use different locks so that independent items do not lock each other.

      * DynamicObject (and therefore Value() and Value_P() constructors
      * PJob_scalar_AB::add_job(), PJob_scalar_B::add_job()
      * PointerCell constructors (change their parents)
    */

const int cores = Thread_context::get_active_core_count();

const ShapeItem slice_len = (job_B->len_Z + cores - 1) / cores;
ShapeItem z = tctx.get_N() * slice_len;
ShapeItem end_z = z + slice_len;
   if (end_z > job_B->len_Z)   end_z = job_B->len_Z;

   if (job_B->float64_fv)   // parallel float64 fast path
      {
        if (z < end_z)
           {
PERFORMANCE_START(start_fast_Bp)
             double       * pZ = reinterpret_cast<double *>(
                                     &job_B->value_Z->get_wfirst()) + z;
             const double * pB = job_B->value_B->cravel_float64() + z;
             job_B->float64_fv(pZ, pB, end_z - z);
CELL_PERFORMANCE_END(job_B->fun->get_statistics_B(), start_fast_Bp, false)
           }
        return;
      }

   if (job_B->int64_fv)   // parallel int64 fast path
      {
        if (z < end_z)
           {
PERFORMANCE_START(start_fast_Bp)
             int64_t       * pZ = reinterpret_cast<int64_t *>(
                                      &job_B->value_Z->get_wfirst()) + z;
             const int64_t * pB = job_B->value_B->cravel_int64() + z;
             job_B->int64_fv(pZ, pB, end_z - z);
CELL_PERFORMANCE_END(job_B->fun->get_statistics_B(), start_fast_Bp, false)
           }
        return;
      }

   // cache_B is this thread's own stack-local materialisation slot for
   // B_at() on packed operands -- see PF_scalar_AB's cache_A/cache_B
   // comment for why the plain (no-cache) B_at() is unsafe here.
   //
Cell cache_B;
   for (; z < end_z; ++z)
       {
         const Cell & cell_B = job_B->B_at(z, cache_B);
         Cell & cell_Z       = job_B->Z_at(z);

         const bool scalar_B = ! cell_B.is_pointer_cell();

         if (scalar_B)
            {
PERFORMANCE_START(start_2)

                  job_B->error = (cell_B.*job_B->fun1)(&cell_Z);
                  if (job_B->error != E_NO_ERROR)   return;

CELL_PERFORMANCE_END(job_B->fun->get_statistics_B(), start_2, z)
            }
         else                            // B is nested
            {
              Parallel::acquire_lock(jobs_lock);

              // B is nested
              //
              Value_P B1 = cell_B.get_pointer_value();

              if (const ShapeItem len_Z1 = B1->element_count())
                 {
                   Value_P Z1(B1->get_shape(), LOC);
                   new (&cell_Z) PointerCell(Z1.get(), *job_B->value_Z);

                   const PJob_scalar_B j1(Z1, *B1);
                   tctx.joblist_B.add_job(j1);
                 }
              else   // empty B1
                 {
                   Value_P Z1 = CLONE_P(B1, LOC);

                   // lrm p. 56: the fill function for primitive scalar
                   // functions is numeric even if the (empty) argumenti
                   //  has type character.
                   //
                   Z1->to_type(/* force_numeric */ true);
                   new (&cell_Z) PointerCell(Z1.get(), *job_B->value_Z);
                 }
              Parallel::release_lock(jobs_lock);
            }
       }
#endif   // PARALLEL_ENABLED
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F2_UNEQU::eval_B(cValue_R B) const
{
   // ≠ B : nub sievce. Return a boolean vector Z, corresponding to B, where 
   // Z[i] is 1 iff B[i] is the first occurrence in B. Aka.(B⍳B)=(⍳⍴B).
   //
const double qct = Workspace::get_CT();
const ShapeItem len_B = B.element_count();
vector<const Cell *> firsts;

   // firsts[] below stores raw Cell pointers that must stay valid across
   // many get_cravel() calls, so a single reused cache slot won't do
   // (each fetch would invalidate the previous one's pointer). Give every
   // element its own stable slot in this caller-owned buffer instead.
   //
vector<uint8_t> stable_B_mem(len_B * sizeof(Cell));
Cell * stable_B = reinterpret_cast<Cell *>(stable_B_mem.data());

Value_P Z(B.get_shape(), LOC);
   loop(b, len_B)
       {
         const Cell & cell_B = B.get_cravel(b, stable_B[b]);

         // sequentially search cell_B in firsts...
         //
         bool existing_item = false;
         loop(f, firsts.size())
             {
               const Cell & cell_F = *firsts[f];
               if (cell_B.is_pointer_cell())   // nested B
                  {
                    if (cell_F.is_pointer_cell())
                       {
                         Value_P vB = cell_B.get_pointer_value();
                         Value_P vF = cell_F.get_pointer_value();
                         const Token equiv = Bif_F12_EQUIV::fun.eval_AB(*vB, *vF);
                         Cell cache;
                         if (equiv.get_apl_val()->get_cfirst(cache)
                                  .get_int_value() == 0)   continue;
                       }
                    else
                       {
                         continue;
                       }
                  }
               else   // simple B
                  {
                    if (cell_F.is_pointer_cell())     continue;
                    if (!cell_B.equal(cell_F, qct))   continue;
                  }

               existing_item = true;
               break;   // loop(f, ...
             }

         if (existing_item)
            {
              Z->next_ravel_0();
            }
         else
            {
              Z->next_ravel_1();
              firsts.push_back(&cell_B);   // and remember it
            }
       }

   Z->set_default(B, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F2_FIND::eval_AB(cValue_R A, cValue_R B) const
{
PERFORMANCE_START(start_1)

const double qct = Workspace::get_CT();
Value_P Z(B.get_shape(), LOC);
Shape shape_A;

const ShapeItem len_Z = Z->element_count();

   if (A.get_rank() > B.get_rank())   // then Z is all zeros.
      {
        loop(z, len_Z)   Z->next_ravel_0();
        goto done;
      }

   // Reshape A to match rank B if necessary...
   //
   {
     const sRank rank_diff = B.get_rank() - A.get_rank();
     loop(d, rank_diff)       shape_A.add_shape_item(1);
     loop(r, A.get_rank())   shape_A.add_shape_item(A.get_shape_item(r));
   }

   // if any dimension of A is longer than that of B, then A cannot be found.
   //
   loop(r, B.get_rank())
       {
         if (shape_A.get_shape_item(r) > B.get_shape_item(r))
            {
              loop(z, len_Z)   Z->next_ravel_0();
              goto done;
            }
       }

   for (ArrayIterator zi(B.get_shape()); zi.has_more(); ++zi)
       {
PERFORMANCE_START(start_2)
         if (contained(shape_A, A,
                       B, zi.get_shape_offsets(), qct))
            Z->next_ravel_1();
         else
            Z->next_ravel_0();

CELL_PERFORMANCE_END(get_statistics_AB(), start_2, B.get_shape().get_volume())
       }

done:
   Z->check_value(LOC);

PERFORMANCE_END(fs_SCALAR_AB, start_1, len_Z);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
bool
Bif_F2_FIND::contained(const Shape & shape_A, cValue_R A,
                       cValue_R B, const Shape & idx_B, double qct)
{
   /* quick check (along each  axis): before comparing any ravel elements,
      we check that A, when offset by idx_B, fits into B:

       ├─── idx B ───┤├────── A ──────┤
       ├───────────────── B ─────────────────┤


    */
   loop(r, B.get_rank())
       {
         if ((idx_B.get_shape_item(r) + shape_A.get_shape_item(r))
             > B.get_shape_item(r))    return false;
       }

const Shape weights_B = B.get_shape().get_weights();

   for (ArrayIterator ai(shape_A); ai.has_more(); ++ai)
       {
         const Shape & pos_A = ai.get_shape_offsets();
         ShapeItem pos_B = 0;
         loop(r, B.get_rank())   pos_B += weights_B.get_shape_item(r)
                                         * (idx_B.get_shape_item(r)
                                         + pos_A.get_shape_item(r));

         Cell cache_A, cache_B;
         if (!A.get_cravel(ai.get_ravel_offset(), cache_A)
              .equal(B.get_cravel(pos_B, cache_B), qct))
            return false;
       }

   return true;
}
//════════════════════════════════════════════════════════════════════════════
// Inverse functions...

//════════════════════════════════════════════════════════════════════════════
cFunction_P
Bif_F12_POWER::get_monadic_inverse() const
{
   return &Bif_F12_LOGA::fun;
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
Bif_F12_POWER::get_dyadic_inverse() const
{
   return &Bif_F12_LOGA::fun;
}
//════════════════════════════════════════════════════════════════════════════
cFunction_P
Bif_F12_PLUS::get_dyadic_inverse() const
{
   return this == &fun ? &fun_inverse : &fun;
}
//════════════════════════════════════════════════════════════════════════════
cFunction_P
Bif_F12_MINUS::get_monadic_inverse() const
{
   // - is self-inverse: B = --B
   return &Bif_F12_MINUS::fun;
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
Bif_F12_MINUS::get_dyadic_inverse() const
{
   // - is self-inverse: B = (A-(A-B))
   return &Bif_F12_MINUS::fun;
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_ROLL::eval_B(cValue_R B) const
{
   // the standard wants ? to be atomic. We therefore check beforehand
   // that all elements of B are proper, and throw an error if not
   //
   if (check_B(B, Workspace::get_CT()))   DOMAIN_ERROR;

   return eval_scalar_B(B, &Cell::bif_roll);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_ROLL::eval_AB(cValue_R A, cValue_R B) const
{
   // draw A items  from the set { ⎕IO ... ⎕IO+ B } (i.e. without repetitions)
   //
   if (!A.is_scalar_extensible())   RANK_ERROR;
   if (!B.is_scalar_extensible())   RANK_ERROR;

const ShapeItem zlen = A.get_near_int(0);
APL_Integer set_size = B.get_near_int(0);
   if (zlen > set_size)         DOMAIN_ERROR;
   if (zlen <  0)               DOMAIN_ERROR;
   if (set_size <  0)           DOMAIN_ERROR;
   if (set_size > 0x7FFFFFFF)   DOMAIN_ERROR;

   // at this point, A and B are OK. We use a bitmap 'used' to keep track
   // of items alrady drawn.
   //
Value_P Z(zlen, LOC);

   // set_size can be rather big, so we allocate it dynamically. A vector
   // (rather than new[]/delete[]) so that Workspace::get_RL() throwing
   // (attention/interrupt) inside the loop below cannot leak it (Blake
   // McBride, Bugs14 #14d); operator new[] itself never returns 0 (it
   // throws std::bad_alloc), so the old null check was already dead
   // code (Bugs14 #14c).
   //
vector<uint8_t> used((set_size + 7)/8, 0);

   loop(z, zlen)
       {
         const uint64_t rnd = Workspace::get_RL(set_size);

         if (used[rnd >> 3] & 1 << (rnd & 7))   // already drawn: try again
            {
              --z;
              continue;
            }
         used[rnd >> 3] |= 1 << (rnd & 7);   // remember rnd
         Z->next_ravel_Int(rnd + Workspace::get_IO());
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
bool
Bif_F12_ROLL::check_B(const cValue & B, const double qct)
{
const ShapeItem count = B.nz_element_count();

   loop(b, count)
      {
       Cell cache;
       const Cell & C = B.get_cravel(b, cache);
       if (Value_P sub_val = C.try_pointer_value())
          {
            if (check_B(*sub_val, qct))   return true;   // check sub_val failed
            continue;
          }

       if (!C.is_near_int())               return true;
       if (C.get_checked_near_int() < 0)   return true;
      }

   return false;
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_WITHOUT::eval_AB(cValue_R A, cValue_R B) const
{
   if (A.get_rank() > 1)
      {
        MORE_ERROR() << "A∼B: A must be a scalar or a vector";
        RANK_ERROR;
      }
   // B can have any rank: APL2 LRM p. 258 and L∼R ←→ (∼L∊R)/L, unlike
   // ISO 13751 10.2.16 (Blake McBride, Bugs35 #27).

const ShapeItem len_A = A.element_count();
const ShapeItem len_B = B.element_count();

   // if called with (⍳N) ∼ (⍳2×N) then the break-even point where
   // large_eval_AB() becomes faster than plain eval_AB() is N=61.
   //
   if (len_A*len_B > 60*60)
      return Token(TOK_APL_VALUE1, large_eval_AB(A, B));

const double qct = Workspace::get_CT();
Value_P Z(len_A, LOC);

ShapeItem len_Z = 0;

   loop(a, len_A)
      {
        bool found = false;
        Cell cache_A;
        const Cell & cell_A = A.get_cravel(a, cache_A);
        loop(b, len_B)
            {
              Cell cache_B;
              if (cell_A.equal(B.get_cravel(b, cache_B), qct))
                 {
                   found = true;
                   break;
                 }
            }

        if (!found)
           {
             Z->next_ravel_Cell(cell_A);
             ++len_Z;
           }
      }

   Z->set_shape_item(0, len_Z);

   Z->set_default(A, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_WITHOUT::eval_identity_fun(cValue_R B, sAxis axis) const
{
   // axis is already normalized to IO←0

   // Figure 28 (apl2lrm.txt p.212) literally gives the identity FUNCTION
   // for ∼ as ⍳0, with the table's own Z←SR⍴⊂.... header applying: the
   // actual identity value is SR⍴⊂⍳0 (SR = the frame shape, i.e. B's
   // shape without axis) -- one enclosed empty vector PER remaining
   // position, via the same enclosed_identity() broadcast every other
   // Figure 28 identity uses (PrimitiveFunction.hh). This used to
   // return CLONE(&B) -- B entirely unchanged -- based on conflating
   // this table entry with a DIFFERENT LRM fact (p.42: dyadic ⍳0⊃B ≡ B,
   // Pick's own empty-left-argument behavior, not the Figure 28
   // reduce-identity). A prior attempt at fixing this reached for
   // NonscalarFunction_default_identity::eval_identity_fun() -- the
   // OTHER, ,-specific default with its own DOMAIN_ERROR volume guard
   // -- and hit that guard spuriously; enclosed_identity() (the shared
   // static helper ⍴/,/⍉/↑/⌹ already use) has no such guard, since it
   // is exactly the general "broadcast one item per frame position"
   // case with no extra restriction. See Bugs27 #27.
   //
const sRank rank_B = B.get_rank();
   if (rank_B < 1)       RANK_ERROR;   // identity restriction, lrm p. 212
   if (axis >= rank_B)   RANK_ERROR;

   return NonscalarFunction_default_identity::enclosed_identity
             (B, axis, Idx0(LOC));
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_F12_WITHOUT::large_eval_AB(const cValue & A, const cValue & B)
{
const ShapeItem len_A = A.element_count();
const ShapeItem len_B = B.element_count();

vector<const Cell *> cells_A;
vector<const Cell *> cells_Z;
vector<const Cell *> cells_B;

   // cells_A/cells_B/cells_Z below store raw Cell pointers that must stay
   // valid after this loop (the final compare_ptr sort reconstructs A's
   // original order), so a single reused cache slot won't do. Use
   // per-element stable storage for packed operands instead.
   //
vector<char> stable_A_mem, stable_B_mem;

   try {
         cells_A.reserve(len_A);
         cells_B.reserve(len_B);
         cells_Z.reserve(len_A);
         stable_A_mem.resize(len_A * sizeof(Cell));
         stable_B_mem.resize(len_B * sizeof(Cell));

       }   catch (std::bad_alloc &) { WS_FULL; }
           catch (...)              { FIXME; }

   {
     Cell * p = reinterpret_cast<Cell *>(stable_A_mem.data());
     loop(a, len_A)   cells_A.push_back(&A.get_cravel(a, p[a]));
   }

   {
     Cell * p = reinterpret_cast<Cell *>(stable_B_mem.data());
     loop(b, len_B)   cells_B.push_back(&B.get_cravel(b, p[b]));
   }

   // sort the A-cells and the B-cells ascendingly
   //
   Heapsort<const Cell *>::sort(cells_A, Cell::compare_stable, 0);
   Heapsort<const Cell *>::sort(cells_B, Cell::compare_stable, 0);

   // store those cells_A pointers that are not in cells_B into cells_Z. Use
   // the fact that cells_A and cells_B are sorted.
   {
     ShapeItem idx_A = 0;
     ShapeItem idx_B = 0;
     const double qct = Workspace::get_CT();

     while (idx_A < len_A && idx_B < len_B)
           {
             const Cell & ref_A = *cells_A[idx_A];
             const Cell & ref_B = *cells_B[idx_B];
             if (ref_A.equal(ref_B, qct))     ++idx_A;   // A is in B → not in Z
             else if (ref_A.greater(ref_B))   ++idx_B;
            else cells_Z.push_back(cells_A[idx_A++]);    // A is in Z
           }

     // the rest of A is in Z
     //
     while (idx_A < len_A)   cells_Z.push_back(cells_A[idx_A++]);
   }

   // sort cells_Z by position so that the original order in A is reconstructed
   //
   Heapsort<const Cell *>::sort(cells_Z, Cell::compare_ptr, 0);

Value_P Z(cells_Z.size(), LOC);

   loop(z, cells_Z.size())   Z->next_ravel_Cell(*cells_Z[z]);

   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
cFunction_P
Bif_F12_TIMES::get_dyadic_inverse() const
{
   return this == &fun ? &fun_inverse : &fun;
}
//════════════════════════════════════════════════════════════════════════════
cFunction_P
Bif_F12_DIVIDE::get_monadic_inverse() const
{
   // ÷ is self-inverse: B = ÷÷B
   return &Bif_F12_DIVIDE::fun;
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
Bif_F12_DIVIDE::get_dyadic_inverse() const
{
   // ÷ is self-inverse: B = (A÷(A÷B))
   return &Bif_F12_DIVIDE::fun;
}
//════════════════════════════════════════════════════════════════════════════
cFunction_P
Bif_F12_CIRCLE::get_monadic_inverse() const
{
   return this == &fun ? &fun_inverse : &fun;
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
Bif_F12_CIRCLE::get_dyadic_inverse() const
{
   return this == &fun ? &fun_inverse : &fun;
}
//════════════════════════════════════════════════════════════════════════════
cFunction_P
Bif_F12_LOGA::get_monadic_inverse() const
{
   return &Bif_F12_POWER::fun;
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
Bif_F12_LOGA::get_dyadic_inverse() const
{
   return &Bif_F12_POWER::fun;
}
//════════════════════════════════════════════════════════════════════════════

