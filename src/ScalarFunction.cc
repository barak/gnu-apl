/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2017  Dr. Jürgen Sauermann

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
#include "Avec.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IndexExpr.hh"
#include "IndexIterator.hh"
#include "IntCell.hh"
#include "PJob.hh"
#include "Parallel.hh"
#include "PointerCell.hh"
#include "ScalarFunction.hh"
#include "Value.hh"
#include "Workspace.hh"

// scalar function instances
//
Bif_F2_AND      Bif_F2_AND     ::_fun;                // ∧
Bif_F2_AND_B    Bif_F2_AND_B   ::_fun;                // ∧∧
Bif_F12_BINOM   Bif_F12_BINOM  ::_fun;                // !
Bif_F12_CIRCLE  Bif_F12_CIRCLE ::_fun(false);         // ○
Bif_F12_CIRCLE  Bif_F12_CIRCLE ::_fun_inverse(true);  // A inverted
Bif_F12_DIVIDE  Bif_F12_DIVIDE ::_fun;                // ÷
Bif_F2_EQUAL    Bif_F2_EQUAL   ::_fun;                // =
Bif_F2_EQUAL_B  Bif_F2_EQUAL_B ::_fun;                // ==
Bif_F2_FIND     Bif_F2_FIND    ::_fun;                // ⋸ (almost scalar)
Bif_F2_GREATER  Bif_F2_GREATER ::_fun;                // >
Bif_F2_LEQ      Bif_F2_LEQ     ::_fun;                // ≤
Bif_F2_LESS     Bif_F2_LESS    ::_fun;                // <
Bif_F12_LOGA    Bif_F12_LOGA   ::_fun;                // ⍟
Bif_F2_MEQ      Bif_F2_MEQ     ::_fun;                // ≥
Bif_F12_MINUS   Bif_F12_MINUS  ::_fun;                // -
Bif_F2_NAND     Bif_F2_NAND    ::_fun;                // ⍲
Bif_F2_NAND_B   Bif_F2_NAND_B  ::_fun;                // ⍲⍲
Bif_F2_NOR      Bif_F2_NOR     ::_fun;                // ⍱
Bif_F2_NOR_B    Bif_F2_NOR_B   ::_fun;                // ⍱⍱
Bif_F2_OR       Bif_F2_OR      ::_fun;                // ∨
Bif_F2_OR_B     Bif_F2_OR_B    ::_fun;                // ∨∨
Bif_F12_PLUS    Bif_F12_PLUS   ::_fun(false);         // +
Bif_F12_PLUS    Bif_F12_PLUS   ::_fun_inverse(true);  // +
Bif_F12_POWER   Bif_F12_POWER  ::_fun;                // ⋆
Bif_F12_RND_UP  Bif_F12_RND_UP ::_fun;                // ⌈
Bif_F12_RND_DN  Bif_F12_RND_DN ::_fun;                // ⌊
Bif_F12_ROLL    Bif_F12_ROLL   ::_fun;                // ? (monadic is scalar)
Bif_F12_STILE   Bif_F12_STILE  ::_fun;                // ∣
Bif_F12_TIMES   Bif_F12_TIMES  ::_fun(false);         // ×
Bif_F12_TIMES   Bif_F12_TIMES  ::_fun_inverse(true); // ×
Bif_F2_UNEQ     Bif_F2_UNEQ    ::_fun;                // ≠
Bif_F2_UNEQ_B   Bif_F2_UNEQ_B  ::_fun;                // ≠
Bif_F12_WITHOUT Bif_F12_WITHOUT::_fun;                // ∼ (monadic is scalar)

// scalar function pointers
//
Bif_F2_AND      * Bif_F2_AND     ::fun         = &Bif_F2_AND     ::_fun;
Bif_F2_AND_B    * Bif_F2_AND_B   ::fun         = &Bif_F2_AND_B   ::_fun;
Bif_F12_BINOM   * Bif_F12_BINOM  ::fun         = &Bif_F12_BINOM  ::_fun;
Bif_F12_CIRCLE  * Bif_F12_CIRCLE ::fun         = &Bif_F12_CIRCLE ::_fun;
Bif_F12_CIRCLE  * Bif_F12_CIRCLE ::fun_inverse = &Bif_F12_CIRCLE ::_fun_inverse;
Bif_F12_DIVIDE  * Bif_F12_DIVIDE ::fun         = &Bif_F12_DIVIDE ::_fun;
Bif_F2_EQUAL    * Bif_F2_EQUAL   ::fun         = &Bif_F2_EQUAL   ::_fun;
Bif_F2_EQUAL_B  * Bif_F2_EQUAL_B ::fun         = &Bif_F2_EQUAL_B ::_fun;
Bif_F2_FIND     * Bif_F2_FIND    ::fun         = &Bif_F2_FIND    ::_fun;
Bif_F2_GREATER  * Bif_F2_GREATER ::fun         = &Bif_F2_GREATER ::_fun;
Bif_F2_LESS     * Bif_F2_LESS    ::fun         = &Bif_F2_LESS    ::_fun;
Bif_F2_LEQ      * Bif_F2_LEQ     ::fun         = &Bif_F2_LEQ     ::_fun;
Bif_F12_LOGA    * Bif_F12_LOGA   ::fun         = &Bif_F12_LOGA   ::_fun;
Bif_F2_MEQ      * Bif_F2_MEQ     ::fun         = &Bif_F2_MEQ     ::_fun;
Bif_F12_MINUS   * Bif_F12_MINUS  ::fun         = &Bif_F12_MINUS  ::_fun;
Bif_F2_NAND     * Bif_F2_NAND    ::fun         = &Bif_F2_NAND    ::_fun;
Bif_F2_NAND_B   * Bif_F2_NAND_B  ::fun         = &Bif_F2_NAND_B  ::_fun;
Bif_F2_NOR      * Bif_F2_NOR     ::fun         = &Bif_F2_NOR     ::_fun;
Bif_F2_NOR_B    * Bif_F2_NOR_B   ::fun         = &Bif_F2_NOR_B   ::_fun;
Bif_F2_OR       * Bif_F2_OR      ::fun         = &Bif_F2_OR      ::_fun;
Bif_F2_OR_B     * Bif_F2_OR_B    ::fun         = &Bif_F2_OR_B    ::_fun;
Bif_F12_PLUS    * Bif_F12_PLUS   ::fun         = &Bif_F12_PLUS   ::_fun;
Bif_F12_PLUS    * Bif_F12_PLUS   ::fun_inverse = &Bif_F12_PLUS   ::_fun_inverse;
Bif_F12_POWER   * Bif_F12_POWER  ::fun         = &Bif_F12_POWER  ::_fun;
Bif_F12_RND_UP  * Bif_F12_RND_UP ::fun         = &Bif_F12_RND_UP ::_fun;
Bif_F12_RND_DN  * Bif_F12_RND_DN ::fun         = &Bif_F12_RND_DN ::_fun;
Bif_F12_ROLL    * Bif_F12_ROLL   ::fun         = &Bif_F12_ROLL   ::_fun;
Bif_F12_TIMES   * Bif_F12_TIMES  ::fun         = &Bif_F12_TIMES  ::_fun;
Bif_F12_TIMES   * Bif_F12_TIMES  ::fun_inverse = &Bif_F12_TIMES  ::_fun_inverse;
Bif_F12_STILE   * Bif_F12_STILE  ::fun         = &Bif_F12_STILE  ::_fun;
Bif_F2_UNEQ     * Bif_F2_UNEQ    ::fun         = &Bif_F2_UNEQ    ::_fun;
Bif_F2_UNEQ_B   * Bif_F2_UNEQ_B  ::fun         = &Bif_F2_UNEQ_B  ::_fun;
Bif_F12_WITHOUT * Bif_F12_WITHOUT::fun         = &Bif_F12_WITHOUT::_fun;

#ifdef PARALLEL_ENABLED
static volatile _Atomic_word parallel_jobs_lock = 0;
#endif

PJob_scalar_AB * job_AB = 0;
PJob_scalar_B  * job_B  = 0;

//-----------------------------------------------------------------------------
Token
ScalarFunction::eval_scalar_B(Value_P B, prim_f1 fun)
{
const ShapeItem len_Z = B->element_count();
   if (len_Z == 0)   return eval_fill_B(B);

PERFORMANCE_START(start)

ErrorCode ec = E_NO_ERROR;
Value_P Z = do_scalar_B(ec, B, fun);
   if (ec != E_NO_ERROR)
      {
        loop(a, Thread_context::get_active_core_count())
            Thread_context::get_context(CoreNumber(a))->joblist_B.cancel_jobs();
        throw_apl_error(ec, LOC);
      }

PERFORMANCE_END(fs_SCALAR_B, start, Z->nz_element_count());

   loop(a, Thread_context::get_active_core_count())
       Assert(Thread_context::get_context(CoreNumber(a))
                            ->joblist_B.get_size() == 0);

   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Value_P
ScalarFunction::do_scalar_B(ErrorCode & ec, Value_P B, prim_f1 fun)
{
Value_P Z(B->get_shape(), LOC);

   // create a worklist with one item that computes Z. If nested values are
   // detected while computing Z then jobs for them are added to the worklist.
   //
   {
     PJob_scalar_B job_B(Z.get(), B.getref());
     Thread_context::get_master().joblist_B.start(job_B, LOC);
   }

#if PARALLEL_ENABLED
const bool maybe_parallel = Parallel::run_parallel &&
                            Thread_context::get_active_core_count() > 1;
#endif

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
              Thread_context::do_work = PF_scalar_B;
              Thread_context::M_fork("eval_scalar_B");   // start pool
              PF_scalar_B(Thread_context::get_master());
PERFORMANCE_START(start_M_join)
              Thread_context::M_join();
              if (job_B->error != E_NO_ERROR)
                 {
                   ec =job_B->error;
                   return Value_P();
                 }
PERFORMANCE_END(fs_M_join_B, start_M_join, 1);
            }
         else
#endif // PARALLEL_ENABLED
            {
              // sequential execution...
              //
              loop(z, job_B->len_Z)
                 {
                   const Cell & cell_B = job_B->B_at(z);
                   Cell & cell_Z       = job_B->Z_at(z);

                   if (cell_B.is_pointer_cell())
                      {
                        Value_P B1 = cell_B.get_pointer_value();
                        Value_P Z1(B1->get_shape(), LOC);
                        new (&cell_Z) PointerCell(Z1.get(), *job_B->value_Z);

                        PJob_scalar_B j1(Z1.get(), B1.getref());
                        Thread_context::get_master().joblist_B.add_job(j1);
                      }
                   else
                      {
PERFORMANCE_START(start_2)
                        ec = (cell_B.*fun)(&cell_Z);
                        if (ec != E_NO_ERROR)   return Value_P();
CELL_PERFORMANCE_END(get_statistics_B(), start_2, z)
                      }
                 }
           }
        job_B->value_Z->check_value(LOC);
      }

   Z->check_value(LOC);

   return Z;
}
//-----------------------------------------------------------------------------
void
ScalarFunction::PF_scalar_B(Thread_context & tctx)
{
const int cores = Thread_context::get_active_core_count();

const ShapeItem slice_len = (job_B->len_Z + cores - 1) / cores;
ShapeItem z = tctx.get_N() * slice_len;
ShapeItem end_z = z + slice_len;
   if (end_z > job_B->len_Z)   end_z = job_B->len_Z;

   for (; z < end_z; ++z)
       {
         const Cell & cell_B = job_B->B_at(z);
         Cell & cell_Z       = job_B->Z_at(z);

         if (cell_B.is_pointer_cell())
            {
              // B is nested
              //
              Value_P B1 = cell_B.get_pointer_value();

              const ShapeItem len_Z1 = B1->element_count();
              if (len_Z1 == 0)
                 {
                   POOL_LOCK(parallel_jobs_lock,
                             Value_P Z1 = B1->clone(LOC))
                    Z1->to_proto();
                    new (&cell_Z) PointerCell(Z1.get(), *job_B->value_Z);
                 }
              else
                 {
                   POOL_LOCK(parallel_jobs_lock,
                             Value_P Z1(B1->get_shape(), LOC))
                   new (&cell_Z) PointerCell(Z1.get(), *job_B->value_Z);

                   PJob_scalar_B j1(Z1.get(), B1.getref());
                   tctx.joblist_B.add_job(j1);
                 }
            }
         else
            {
                  // B not nested: execute fun
                  //
PERFORMANCE_START(start_2)
                  job_B->error = (cell_B.*job_B->fun1)(&cell_Z);
                  if (job_B->error != E_NO_ERROR)   return;

CELL_PERFORMANCE_END(job_B->fun->get_statistics_B(), start_2, z)
            }
       }
}
//-----------------------------------------------------------------------------
void
ScalarFunction::expand_pointers(Cell * cell_Z, Value & Z_owner,
                                const Cell * cell_A, const Cell * cell_B,
                                prim_f2 fun)
{
   if (cell_A->is_pointer_cell())
      {
        if (cell_B->is_pointer_cell())   // A and B are both pointers
           {
             Value_P value_A = cell_A->get_pointer_value();
             Value_P value_B = cell_B->get_pointer_value();
             POOL_LOCK(parallel_jobs_lock,
                       Token token = eval_scalar_AB(value_A, value_B, fun))
             new (cell_Z) PointerCell(token.get_apl_val().get(), Z_owner);
           }
        else                             // A is pointer, B is simple
           {
             Value_P value_A = cell_A->get_pointer_value();
             Value_P scalar_B(*cell_B, LOC);
             POOL_LOCK(parallel_jobs_lock,
                       Token token = eval_scalar_AB(value_A, scalar_B, fun))
             new (cell_Z) PointerCell(token.get_apl_val().get(), Z_owner);
           }
      }
   else                                  // A is simple
      {
        if (cell_B->is_pointer_cell())   // A is simple, B is pointer
           {
             Value_P scalar_A(*cell_A, LOC);
             Value_P value_B = cell_B->get_pointer_value();
             POOL_LOCK(parallel_jobs_lock,
                       Token token = eval_scalar_AB(scalar_A, value_B, fun))
             new (cell_Z) PointerCell(token.get_apl_val().get(), Z_owner);
           }
   else                                // A and B are both plain
         {
           (cell_B->*fun)(cell_Z, cell_A);
         }
      }
}
//-----------------------------------------------------------------------------
Token
ScalarFunction::eval_scalar_AB(Value_P A, Value_P B, prim_f2 fun)
{
PERFORMANCE_START(start)

ErrorCode ec = E_NO_ERROR;
Value_P Z = do_scalar_AB(ec, A, B, fun);
   if (ec != E_NO_ERROR)
      {
        loop(a, Thread_context::get_active_core_count())
           Thread_context::get_context(CoreNumber(a))->joblist_AB.cancel_jobs();
        throw_apl_error(ec, LOC);
      }

PERFORMANCE_END(fs_SCALAR_AB, start, Z->nz_element_count());

   loop(a, Thread_context::get_active_core_count())
       Assert(Thread_context::get_context(CoreNumber(a))
                            ->joblist_AB.get_size() == 0);

   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Value_P
ScalarFunction::do_scalar_AB(ErrorCode & ec, Value_P A, Value_P B, prim_f2 fun)
{
const int inc_A = A->get_increment();
const int inc_B = B->get_increment();

const Shape * shape_Z = 0;
   if      (A->is_scalar())      shape_Z = &B->get_shape();
   else if (B->is_scalar())      shape_Z = &A->get_shape();
   else if (inc_A == 0)          shape_Z = &B->get_shape();
   else if (inc_B == 0)          shape_Z = &A->get_shape();
   else if (A->same_shape(*B))   shape_Z = &B->get_shape();
   else 
      {
        if (!A->same_rank(*B))   ec = E_RANK_ERROR;
        else                     ec = E_LENGTH_ERROR;
        return Value_P();
      }

const ShapeItem len_Z = shape_Z->get_volume();
   if (len_Z == 0)   return eval_fill_AB(A, B).get_apl_val();

Value_P Z(*shape_Z, LOC);

   // create a worklist with one item that computes Z. If nested values are
   // detected while computing Z then jobs for them are added to the worklist.
   //
   {
     PJob_scalar_AB job_AB(Z.get(), &A->get_ravel(0), inc_A,
                                    &B->get_ravel(0), inc_B);
     Thread_context::get_master().joblist_AB.start(job_AB, LOC);
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
              Thread_context::do_work = PF_scalar_AB;
              Thread_context::M_fork("eval_scalar_AB");   // start pool
              PF_scalar_AB(Thread_context::get_master());
PERFORMANCE_START(start_M_join)
              Thread_context::M_join();
              ec = job_AB->error;
              if (ec != E_NO_ERROR)   return Value_P();
PERFORMANCE_END(fs_M_join_AB, start_M_join, 1);
            }
         else
#endif // PARALLEL_ENABLED
            {
              // sequential execution...
              //
              loop(z, job_AB->len_Z)
                 {
                   const Cell & cell_A = job_AB->A_at(z);
                   const Cell & cell_B = job_AB->B_at(z);
                   Cell & cell_Z       = job_AB->Z_at(z);

                   if (cell_A.is_pointer_cell())
                      if (cell_B.is_pointer_cell())
                         {
                           // both A and B are nested
                           //
                           Value_P A1 = cell_A.get_pointer_value();
                           Value_P B1 = cell_B.get_pointer_value();
                           const int inc_A1 = A1->get_increment();
                           const int inc_B1 = B1->get_increment();
                           const Shape * sh_Z1 = &B1->get_shape();
                           if      (A1->is_scalar())  sh_Z1 = &B1->get_shape();
                           else if (B1->is_scalar())  sh_Z1 = &A1->get_shape();
                           else if (inc_B1 == 0)      sh_Z1 = &A1->get_shape();

                           if (inc_A1 && inc_B1 && !A1->same_shape(*B1))
                              {
                                if (!A1->same_rank(*B1))   ec = E_RANK_ERROR;
                                else                       ec = E_LENGTH_ERROR;
                                return Value_P();
                              }

                           const ShapeItem len_Z1 = sh_Z1->get_volume();
                           if (len_Z1 == 0)
                              {
                                Value_P Z1 =
                                        eval_fill_AB(A1, B1).get_apl_val();
                                new (&cell_Z) PointerCell(Z1.get(),
                                                          *job_AB->value_Z);
                                continue;
                              }

                           Value_P Z1(*sh_Z1, LOC);
                           new (&cell_Z)
                               PointerCell(Z1.get(), *job_AB->value_Z,
                                           0x6B616769);

                           PJob_scalar_AB j1(Z1.get(),
                                             &A1->get_ravel(0), inc_A1,
                                             &B1->get_ravel(0), inc_B1);
                           Thread_context::get_master()
                                          .joblist_AB.add_job(j1);
                         }
                      else
                         {
                           // A is nested, B is not
                           //
                           Value_P A1 = cell_A.get_pointer_value();
                           const int inc_A1 = A1->get_increment();

                           const ShapeItem len_Z1 = A1->element_count();
                           if (len_Z1 == 0)
                              {
                                Value_P Z1 = eval_fill_AB(A1, B).get_apl_val();
                                new (&cell_Z) PointerCell(Z1.get(),
                                                          *job_AB->value_Z);
                              }
                           else
                              {
                                 Value_P Z1(A1->get_shape(), LOC);
                                 new (&cell_Z)
                                     PointerCell(Z1.get(),*job_AB->value_Z,
                                                 0x6B616769);

                                 PJob_scalar_AB j1(Z1.get(),
                                                   &A1->get_ravel(0), inc_A1,
                                                   &cell_B, 0);
                                 Thread_context::get_master().joblist_AB
                                                             .add_job(j1);
                              }
                         }
                   else
                      if (cell_B.is_pointer_cell())
                         {
                           // B is nested, A is not
                           //
                           Value_P B1 = cell_B.get_pointer_value();
                           const int inc_B1 = B1->get_increment();

                           const ShapeItem len_Z1 = B1->element_count();
                           if (len_Z1 == 0)
                              {
                                Value_P Z1 = eval_fill_AB(A, B1).get_apl_val();
                                new (&cell_Z) PointerCell(Z1.get(),
                                                          *job_AB->value_Z);
                              }
                           else
                              {
                                Value_P Z1(B1->get_shape(), LOC);

                                new (&cell_Z)
                                    PointerCell(Z1.get(), *job_AB->value_Z,
                                                0x6B616769);

                                PJob_scalar_AB j1(Z1.get(), &cell_A, 0,
                                                  &B1->get_ravel(0), inc_B1);
                                Thread_context::get_master()
                                               .joblist_AB.add_job(j1);
                             }
                         }
                      else
                         {
                           // neither A nor B are nested: execute fun
                           //
PERFORMANCE_START(start_2)

                           ec = (cell_B.*fun)(&cell_Z, &cell_A);
                           if (ec != E_NO_ERROR)   return Value_P();
CELL_PERFORMANCE_END(get_statistics_AB(), start_2, z)
                         }
                 }
           }
        job_AB->value_Z->check_value(LOC);
      }

   Z->set_default(*B.get(), LOC);
   Z->check_value(LOC);

   return Z;
}
//-----------------------------------------------------------------------------
void
ScalarFunction::PF_scalar_AB(Thread_context & tctx)
{
const CoreCount cores = Thread_context::get_active_core_count();

const ShapeItem slice_len = (job_AB->len_Z + cores - 1) / cores;
ShapeItem z = tctx.get_N() * slice_len;

ShapeItem end_z = z + slice_len;
   if (end_z > job_AB->len_Z)   end_z = job_AB->len_Z;

   for (; z < end_z; ++z)
       {
             const Cell & cell_A = job_AB->A_at(z);
             const Cell & cell_B = job_AB->B_at(z);
             Cell & cell_Z       = job_AB->Z_at(z);

             if (cell_A.is_pointer_cell())
                if (cell_B.is_pointer_cell())
                   {
                     // both A and B are nested
                     //
                     Value_P A1 = cell_A.get_pointer_value();
                     Value_P B1 = cell_B.get_pointer_value();
                     const int inc_A1 = A1->get_increment();
                     const int inc_B1 = B1->get_increment();
                     const Shape * sh_Z1 = &B1->get_shape();
                     if      (A1->is_scalar())   sh_Z1 = &B1->get_shape();
                     else if (B1->is_scalar())   sh_Z1 = &A1->get_shape();
                     else if (inc_B1 == 0)       sh_Z1 = &A1->get_shape();

                     if (inc_A1 && inc_B1 && !A1->same_shape(*B1))
                        {
                          job_AB->error = A1->same_rank(*B1) ? E_LENGTH_ERROR
                                                         : E_RANK_ERROR;
                          return;
                        }

                     const ShapeItem len_Z1 = sh_Z1->get_volume();
                     if (len_Z1 == 0)
                        {
                          Token result =job_AB->fun->eval_fill_AB(A1, B1);
                          if (result.get_tag() == TOK_ERROR)
                             {
                               job_AB->error = ErrorCode(result.get_int_val());
                               return;
                             }
                        }

                     POOL_LOCK(parallel_jobs_lock,
                               Value_P Z1(*sh_Z1, LOC))
                        new (&cell_Z) PointerCell(Z1.get(), *job_AB->value_Z);

                        PJob_scalar_AB j1(Z1.get(),
                                          &A1->get_ravel(0), inc_A1,
                                          &B1->get_ravel(0), inc_B1);
                        tctx.joblist_AB.add_job(j1);
                   }
                else
                   {
                     // A is nested, B is not
                     //
                     Value_P A1 = cell_A.get_pointer_value();
                     const int inc_A1 = A1->get_increment();

                     const ShapeItem len_Z1 = A1->element_count();
                     if (len_Z1 == 0)
                        {
                          Value_P B(LOC);   // a scalar
                          B->get_ravel(0).init(cell_B, B.getref(), LOC);
                          Value_P Z1 = job_AB->fun->eval_fill_AB(A1, B)
                                                   .get_apl_val();
                          new (&cell_Z) PointerCell(Z1.get(),
                                                    *job_AB->value_Z);
                        }
                     else
                        {
                          Value_P Z1(A1->get_shape(), LOC);
                          new (&cell_Z) PointerCell(Z1.get(),
                                                    *job_AB->value_Z);

                          PJob_scalar_AB j1(Z1.get(),
                                            &A1->get_ravel(0), inc_A1,
                                            &cell_B, 0);
                          tctx.joblist_AB.add_job(j1);
                        }
                   }
             else
                if (cell_B.is_pointer_cell())
                   {
                     // A is not nested, B is nested
                     //
                     Value_P B1 = cell_B.get_pointer_value();
                     const int inc_B1 = B1->get_increment();

                     const ShapeItem len_Z1 = B1->element_count();
                     if (len_Z1 == 0)
                        {
                          Value_P A(LOC);   // a scalar
                          A->get_ravel(0).init(cell_A, A.getref(), LOC);
                          Value_P Z1 = job_AB->fun->eval_fill_AB(A, B1)
                                                   .get_apl_val();
                          new (&cell_Z) PointerCell(Z1.get(),
                                                    *job_AB->value_Z);
                        }
                     else
                        {
                          POOL_LOCK(parallel_jobs_lock,
                                    Value_P Z1(B1->get_shape(), LOC))
                          new (&cell_Z) PointerCell(Z1.get(),
                                                    *job_AB->value_Z);

                          PJob_scalar_AB j1(Z1.get(), &cell_A, 0,
                                           &B1->get_ravel(0), inc_B1);
                          tctx.joblist_AB.add_job(j1);
                       }
                   }
                else
                   {
                     // neither A nor B are nested: execute fun
                     //
PERFORMANCE_START(start_2)

                     job_AB->error = (cell_B.*job_AB->fun2)(&cell_Z, &cell_A);
                     if (job_AB->error != E_NO_ERROR)   return;

CELL_PERFORMANCE_END(job_AB->fun->get_statistics_AB(), start_2, z)
                   }
       }
}
//-----------------------------------------------------------------------------
Token
ScalarFunction::eval_fill_AB(Value_P A, Value_P B)
{
   // eval_fill_AB() is called when A or B (or both) are empty.
   //
   if (B->element_count() == 0)   // B is empty
      {
        if (B->get_ravel(0).is_numeric() ||
            B->get_ravel(0).is_character_cell())
           {
             Value_P Z(B->get_shape(), LOC);
             new (&Z->get_ravel(0))   IntCell(0);
             Z->check_value(LOC);
             return Token(TOK_APL_VALUE1, Z);
           }

        Value_P Z = B->clone(LOC);
        Z->to_proto();
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (A->element_count() == 0)   // A is empty
      {
        if (A->get_ravel(0).is_numeric() ||
            A->get_ravel(0).is_character_cell())
           {
             Value_P Z(A->get_shape(), LOC);
             new (&Z->get_ravel(0))   IntCell(0);
             Z->check_value(LOC);
             return Token(TOK_APL_VALUE1, Z);
           }
        Value_P Z = A->clone(LOC);
        Z->to_proto();
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // both A and B are empty
   //
   Assert(A->same_shape(*B));   // has been checked already

   // Value::prototype() does not work here, so we clone() and to_proto()
   //
Value_P Z = B->clone(LOC);
   Z->to_proto();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);

//   return Bif_F2_UNEQ::fun.eval_AB(A, B);
}
//-----------------------------------------------------------------------------
Token
ScalarFunction::eval_fill_B(Value_P B)
{
   // eval_fill_B() is called when a scalar function with empty B is called
   //
   // lrm p. 56: When the prototypes of the empty arguments are simple
   //            scalars, return a zero prototype
   //
   if (B->get_ravel(0).is_numeric() ||
       B->get_ravel(0).is_character_cell())
      {
        Value_P Z(B->get_shape(), LOC);
        new (&Z->get_ravel(0))   IntCell(0);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // Value::prototype() does not work here, so we clone() and to_proto()
   //
Value_P Z = B->clone(LOC);
   Z->to_proto();
   Z->check_value(LOC);

   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
ScalarFunction::eval_scalar_identity_fun(Value_P B, Axis axis, Value_P FI0)
{
   // for scalar functions the result of the identity function for scalar
   // function F is defined as follows (lrm p. 210)
   //
   // Z←SRρB+F/ι0    with SR ↔ ⍴Z
   //
   // The term F/ι0 is passed as argument FI0, so that the above becomes
   //
   // Z←SRρB+FI0
   //
   // Since F is scalar, the ravel elements of B (if any) are 0 and
   // therefore B+FI0 becomes (⍴B)⍴FI0.
   //
   if (!FI0->is_scalar())   Q1(FI0->get_shape())

const Shape shape_Z = B->get_shape().without_axis(axis);

Value_P Z(shape_Z, LOC);

const Cell & proto_B = B->get_ravel(0);
const Cell & cell_FI0 = FI0->get_ravel(0);

   if (proto_B.is_pointer_cell())
      {
        // create a value like ↑B but with all ravel elements like FI0...
        //
        POOL_LOCK(parallel_jobs_lock,
           Value_P sub(proto_B.get_pointer_value()->get_shape(),LOC))
        const ShapeItem len_sub = sub->nz_element_count();
        loop(s, len_sub)   sub->next_ravel()->init(cell_FI0, sub.getref(), LOC);
        sub->check_value(LOC);

        while (Z->more())
           new (Z->next_ravel()) PointerCell(sub->clone(LOC).get(), Z.getref());
      }
   else
      {
        while (Z->more())   Z->next_ravel()->init(cell_FI0, Z.getref(), LOC);
        if (Z->is_empty())  Z->get_ravel(0).init(cell_FI0, Z.getref(), LOC);
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
ScalarFunction::eval_scalar_AXB(Value_P A, Value_P X, Value_P B, prim_f2 fun)
{
PERFORMANCE_START(start_1)

   if (!X || A->is_scalar_extensible() || B->is_scalar_extensible())
      return eval_scalar_AB(A, B, fun);

   if (X->get_rank() > 1)   AXIS_ERROR;

const APL_Integer qio = Workspace::get_IO();
const Rank rank_A = A->get_rank();
const Rank rank_B = B->get_rank();
bool axis_in_X[MAX_RANK];
   loop(r, MAX_RANK)   axis_in_X[r] = false;

const ShapeItem len_X = X->element_count();

   loop(iX, len_X)
       {
         APL_Integer i = X->get_ravel(iX).get_near_int() - qio;
         if (i < 0)                        AXIS_ERROR;   // too small
         if (i >= rank_A && i >= rank_B)   AXIS_ERROR;   // too large
         if (axis_in_X[i])                 AXIS_ERROR;   // twice
         axis_in_X[i] = true;
       }

   if (rank_A == rank_B)
      {
        if (rank_A != len_X)   AXIS_ERROR;
        return eval_scalar_AB(A, B, fun);
      }

   if (rank_A < rank_B)
      {
        if (rank_A != len_X)   AXIS_ERROR;

        Value_P Z = eval_scalar_AXB(A, axis_in_X, B, fun, false);
PERFORMANCE_END(fs_SCALAR_AB, start_1, Z->nz_element_count())
        return Token(TOK_APL_VALUE1, Z);
      }
    else
      {
        if (rank_B != len_X)   AXIS_ERROR;

        Value_P Z = eval_scalar_AXB(B, axis_in_X, A, fun, true);
PERFORMANCE_END(fs_SCALAR_AB, start_1, Z->nz_element_count())
        return Token(TOK_APL_VALUE1, Z);
      }
}
//-----------------------------------------------------------------------------
Value_P
ScalarFunction::eval_scalar_AXB(Value_P A, bool * axis_in_X,
                                Value_P B, prim_f2 fun, bool reversed)
{
   // A is the value with the smaller rank.
   // B the value with the larger rank.
   //
   // If (reversed) then A and B have changed roles (in order to
   /// make A the value with the smaller rank).

   // check that A and B agree on the axes in X
   //
   {
     Rank rA = 0;
     loop(rB, B->get_rank())
         {
            if (axis_in_X[rB])
               {
                 // if the axis is in X then the corresponding shape items in
                 // A and B must agree.
                 //
                 if (B->get_shape_item(rB) != A->get_shape_item(rA++))
                    LENGTH_ERROR;
               }
         }
   }

Shape weight_A = A->get_shape().reverse_scan();

Value_P Z(B->get_shape(), LOC);

const Cell * cB = &B->get_ravel(0);

   for (ArrayIterator it(B->get_shape()); it.more(); ++it)
       {
         ShapeItem a = 0;
         Rank rA = 0;
         loop(rB, B->get_rank())
             {
               if (axis_in_X[rB])
                  {
                    a += weight_A.get_shape_item(rA++)
                       * it.get_offset(rB);
                  }
             }

         const Cell * cA = &A->get_ravel(a);
         if (reversed)
            expand_pointers(Z->next_ravel(), Z.getref(), cB++, cA, fun);
         else
            expand_pointers(Z->next_ravel(), Z.getref(), cA, cB++, fun);
       }

   Z->set_default(*B.get(), LOC);

   Z->check_value(LOC);
   return Z;
}
//=============================================================================
Token
Bif_F2_FIND::eval_AB(Value_P A, Value_P B)
{
PERFORMANCE_START(start_1)

const double qct = Workspace::get_CT();
Value_P Z(B->get_shape(), LOC);
Shape shape_A;

const ShapeItem len_Z = Z->element_count();

   if (A->get_rank() > B->get_rank())   // then Z is all zeros.
      {
        loop(z, len_Z)   new (Z->next_ravel())   IntCell(0);
        goto done;
      }

   // Reshape A to match rank B if necessary...
   //
   {
     const Rank rank_diff = B->get_rank() - A->get_rank();
     loop(d, rank_diff)       shape_A.add_shape_item(1);
     loop(r, A->get_rank())   shape_A.add_shape_item(A->get_shape_item(r));
   }

   // if any dimension of A is longer than that of B, then A cannot be found.
   //
   loop(r, B->get_rank())
       {
         if (shape_A.get_shape_item(r) > B->get_shape_item(r))
            {
              loop(z, len_Z)   new (Z->next_ravel())   IntCell(0);
              goto done;
            }
       }

   for (ArrayIterator zi(B->get_shape()); zi.more(); ++zi)
       {
PERFORMANCE_START(start_2)
         if (contained(shape_A, &A->get_ravel(0), B, zi.get_offsets(), qct))
            new (&Z->get_ravel(zi()))   IntCell(1);
         else
            new (&Z->get_ravel(zi()))   IntCell(0);

CELL_PERFORMANCE_END(get_statistics_AB(), start_2, B->get_shape().get_volume())
       }

done:
   Z->check_value(LOC);

PERFORMANCE_END(fs_SCALAR_AB, start_1, len_Z);
   return Token(TOK_APL_VALUE1, Z);
}
//=============================================================================
bool
Bif_F2_FIND::contained(const Shape & shape_A, const Cell * cA,
                       Value_P B, const Shape & idx_B, double qct)
{
   loop(r, B->get_rank())
       {
         if ((idx_B.get_shape_item(r) + shape_A.get_shape_item(r))
             > B->get_shape_item(r))    return false;
       }

const Shape weight = B->get_shape().reverse_scan();

   for (ArrayIterator ai(shape_A); ai.more(); ++ai)
       {
         const Shape & pos_A = ai.get_offsets();
         ShapeItem pos_B = 0;
         loop(r, B->get_rank())   pos_B += weight.get_shape_item(r)
                                         * (idx_B.get_shape_item(r)
                                         + pos_A.get_shape_item(r));

         if (!cA[ai()].equal(B->get_ravel(pos_B), qct))
            return false;
       }

   return true;
}
//=============================================================================
Token
Bif_F12_ROLL::eval_AB(Value_P A, Value_P B)
{
   // draw A items  from the set [quad-IO ... B]
   //
   if (!A->is_scalar_extensible())   RANK_ERROR;
   if (!B->is_scalar_extensible())   RANK_ERROR;

const ShapeItem zlen = A->get_ravel(0).get_near_int();
APL_Integer set_size = B->get_ravel(0).get_near_int();
   if (zlen > set_size)         DOMAIN_ERROR;
   if (zlen <  0)               DOMAIN_ERROR;
   if (set_size <  0)           DOMAIN_ERROR;
   if (set_size > 0x7FFFFFFF)   DOMAIN_ERROR;

Value_P Z(zlen, LOC);
   new (&Z->get_ravel(0))   IntCell(0);   // prototype

   // set_size can be rather big, so we new/delete it
   //
uint8_t * used = new uint8_t[(set_size + 7)/8];
   if (used == 0)   throw_apl_error(E_WS_FULL, LOC);
   memset(used, 0, (set_size + 7)/8);

   loop(z, zlen)
       {
         const uint64_t rnd = Workspace::get_RL(set_size);

         if (used[rnd >> 3] & 1 << (rnd & 7))   // already drawn
            {
              --z;
              continue;
            }
         used[rnd >> 3] |= 1 << (rnd & 7);
         new (Z->next_ravel()) IntCell(rnd + Workspace::get_IO());
       }

   delete [] used;

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_ROLL::eval_B(Value_P B)
{
   // the standard wants ? to be atomic. We therefore check beforehand
   // that all elements of B are proper, and throw an error if not
   //
   if (check_B(*B, Workspace::get_CT()))   DOMAIN_ERROR;

   return eval_scalar_B(B, &Cell::bif_roll);
}
//-----------------------------------------------------------------------------
bool
Bif_F12_ROLL::check_B(const Value & B, const double qct)
{
const ShapeItem count = B.nz_element_count();
const Cell * C = &B.get_ravel(0);

   loop(b, count)
      {
       if (C->is_pointer_cell())
          {
            Value_P sub_val = C->get_pointer_value();
            if (check_B(*sub_val, qct))   return true;   // check sub_val failed
            continue;
          }

       if (!C->is_near_int())               return true;
       if (C->get_checked_near_int() < 0)   return true;

        ++C;
      }

   return false;
}
//=============================================================================
Token
Bif_F12_WITHOUT::eval_AB(Value_P A, Value_P B)
{
   if (A->get_rank() > 1)   RANK_ERROR;
   if (B->get_rank() > 1)   RANK_ERROR;

const ShapeItem len_A = A->element_count();
const ShapeItem len_B = B->element_count();

   // if called with (⍳N) ∼ (⍳2×N) then the break-even point where
   // large_eval_AB() becomes faster than plain eval_AB() is N=61.
   //
   if (len_A*len_B > 60*60)
      return Token(TOK_APL_VALUE1, large_eval_AB(A.get(), B.get()));

const double qct = Workspace::get_CT();
Value_P Z(len_A, LOC);

ShapeItem len_Z = 0;

   loop(a, len_A)
      {
        bool found = false;
        const Cell & cell_A = A->get_ravel(a);
        loop(b, len_B)
            {
              if (cell_A.equal(B->get_ravel(b), qct))
                 {
                   found = true;
                   break;
                 }
            }

        if (!found)
           {
             Z->next_ravel()->init(cell_A, Z.getref(), LOC);
             ++len_Z;
           }
      }

   Z->set_shape_item(0, len_Z);

   Z->set_default(*A.get(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Value_P
Bif_F12_WITHOUT::large_eval_AB(const Value * A, const Value * B)
{
const ShapeItem len_A = A->element_count();
const ShapeItem len_B = B->element_count();
const Cell ** cells_A = new const Cell *[2*len_A + len_B];
const Cell ** cells_Z = cells_A + len_A;
const Cell ** cells_B = cells_A + 2*len_A;

   loop(a, len_A)   cells_A[a] = &A->get_ravel(a);
   loop(b, len_B)   cells_B[b] = &B->get_ravel(b);

   Heapsort<const Cell *>::sort(cells_A, len_A, 0, Cell::compare_stable);
   Heapsort<const Cell *>::sort(cells_B, len_B, 0, Cell::compare_stable);

   // set pointers in A to 0 if they are also in B. Count remaining entries
   // in A in len_Z.
ShapeItem len_Z = 0;
ShapeItem idx_B = 0;
const double qct = Workspace::get_CT();

   loop(idx_A, len_A)
       {
         const Cell * ref = cells_A[idx_A];
         while (idx_B < len_B)
               {
                 if (ref->equal(*cells_B[idx_B], qct))
                    {
                         break;   // for idx_B → next idx_A
                    }

                 // B is much (by ⎕CT) smaller or greater than A
                 //
                 if (ref->greater(*cells_B[idx_B]))    ++idx_B;
                 else   // B > A, so A is in A∼B
                    {
                      cells_Z[len_Z++] = ref;   // A is in B
                      break;
                    }
               }
       }

   // sort cells_Z by position so that the original order in A is reconstructed
   //
   Heapsort<const Cell *>::sort(cells_Z, len_Z, 0, Cell::compare_ptr);

Value_P Z(len_Z, LOC);

   loop(z, len_Z)   Z->next_ravel()->init(*cells_Z[z], Z.getref(), LOC);

   delete[] cells_A;   // incl. cells_Z and cells_B

   Z->check_value(LOC);
   return Z;
}
//=============================================================================
// Inverse functions...

//-----------------------------------------------------------------------------
Function *
Bif_F12_POWER::get_monadic_inverse() const
{
   return Bif_F12_LOGA::fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_POWER::get_dyadic_inverse() const
{
   return Bif_F12_LOGA::fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_LOGA::get_monadic_inverse() const
{
   return Bif_F12_POWER::fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_LOGA::get_dyadic_inverse() const
{
   return Bif_F12_POWER::fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_TIMES::get_dyadic_inverse() const
{
   if (this == fun)   return fun_inverse;
   else               return fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_DIVIDE::get_monadic_inverse() const
{
   // ÷ is self-inverse: B = ÷÷B
   return Bif_F12_DIVIDE::fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_DIVIDE::get_dyadic_inverse() const
{
   // ÷ is self-inverse: B = (A÷(A÷B))
   return Bif_F12_DIVIDE::fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_PLUS::get_dyadic_inverse() const
{
   if (this == fun)   return fun_inverse;
   else               return fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_MINUS::get_monadic_inverse() const
{
   // - is self-inverse: B = --B
   return Bif_F12_PLUS::fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_MINUS::get_dyadic_inverse() const
{
   // - is self-inverse: B = (A-(A-B))
   return Bif_F12_PLUS::fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_CIRCLE::get_monadic_inverse() const
{
   if (this == fun)   return fun_inverse;
   else               return fun;
}
//-----------------------------------------------------------------------------
Function *
Bif_F12_CIRCLE::get_dyadic_inverse() const
{
   if (this == fun)   return fun_inverse;
   else               return fun;
}
//=============================================================================

