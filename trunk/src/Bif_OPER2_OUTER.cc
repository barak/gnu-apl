/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2022  Dr. Jürgen Sauermann

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

#include "Bif_OPER1_REDUCE.hh"
#include "Bif_OPER2_OUTER.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Macro.hh"
#include "Workspace.hh"

Bif_JOT          Bif_JOT        ::_fun;
Bif_OPER2_OUTER  Bif_OPER2_OUTER::_fun;

Bif_JOT         * Bif_JOT        ::fun = &Bif_JOT        ::_fun;
Bif_OPER2_OUTER * Bif_OPER2_OUTER::fun = &Bif_OPER2_OUTER::_fun;

Bif_OPER2_OUTER::PJob_product Bif_OPER2_OUTER::job;

//----------------------------------------------------------------------------
Token
Bif_OPER2_OUTER::eval_ALRB(Value_P A, Token & LO, Token & _RO, Value_P B) const
{
   if (!_RO.is_function())    SYNTAX_ERROR;

Function_P RO = _RO.get_function();
   Assert(RO);

   if (!RO->has_result())   DOMAIN_ERROR;

Value_P Z(A->get_shape() + B->get_shape(), LOC);

   // an important (and the most likely) special case is RO being a scalar
   // function. This case can be implemented in a far simpler fashion than
   // the general case.
   //
   if (RO->get_scalar_f2() && A->is_simple() && B->is_simple())
      {
        job.cZ     = &Z->get_wfirst();
        job.cA     = &A->get_cfirst();
        job.ZAh    = A->element_count();
        job.RO     = RO->get_scalar_f2();
        job.cB     = &B->get_cfirst();
        job.ZBl    = B->element_count();
        job.ec     = E_NO_ERROR;

        scalar_outer_product();
        if (job.ec != E_NO_ERROR)   throw_apl_error(job.ec, LOC);

        Z->set_default(*B.get(), LOC);
 
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (Z->is_empty())
      {
        Value_P Fill_A = Bif_F12_TAKE::first(*A);
        Value_P Fill_B = Bif_F12_TAKE::first(*B);

        Value_P Z1 = RO->eval_fill_AB(Fill_A, Fill_B).get_apl_val();
        Z->set_ravel_Cell(0, Z1->get_cfirst());
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (RO->may_push_SI())   // user defined LO
      {
        return Macro::get_macro(Macro::MAC_Z__A_LO_OUTER_B)
                    ->eval_ALB(A, _RO, B);
      }

const ShapeItem len_B = B->element_count();
const ShapeItem len_Z = A->element_count() * len_B;

Value_P RO_A;
Value_P RO_B;

   loop(z, len_Z)
      {
        const Cell * cA = &A->get_cravel(z / len_B);
        const Cell * cB = &B->get_cravel(z % len_B);

        if (cA->is_pointer_cell())
           {
             RO_A = cA->get_pointer_value();
           }
        else
           {
             RO_A = Value_P(LOC);   // scalar RO_A
             RO_A->set_ravel_Cell(0, *cA);
           }

        if (cB->is_pointer_cell())
           {
             RO_B = cB->get_pointer_value();
           }
        else
           {
             RO_B = Value_P(LOC);   // scalar RO_B
             RO_B->set_ravel_Cell(0, *cB);
           }

        Token result = RO->eval_AB(RO_A, RO_B);

      // if RO was a primitive function, then result may be a value.
      // if RO was a user defined function then result may be
      // TOK_SI_PUSHED. In both cases result could be TOK_ERROR.
      //
      if (result.get_Class() == TC_VALUE)
         {
           Value_P ZZ = result.get_apl_val();
           Z->next_ravel_Value(ZZ.get());
           continue;
         }

      if (result.get_tag() == TOK_ERROR)   return result;

        Q1(result);   FIXME;
      }

   Z->set_default(*B.get(), LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
void
Bif_OPER2_OUTER::scalar_outer_product() const
{
#ifdef PERFORMANCE_COUNTERS_WANTED
const uint64_t start_1 = cycle_counter();
#endif

  // the empty cases have been handled already in eval_ALRB()

   job.ec = E_NO_ERROR;

#if PARALLEL_ENABLED
   if (  Parallel::run_parallel
      && Thread_context::get_active_core_count() > 1
      && job.ZAh * job.ZBl > get_dyadic_threshold())
      {
        job.cores = Thread_context::get_active_core_count();
        Thread_context::do_work = PF_scalar_outer_product;
        Thread_context::M_fork("scalar_outer_product");   // start pool
        PF_scalar_outer_product(Thread_context::get_master());
        Thread_context::M_join();
      }
   else
#endif // PARALLEL_ENABLED
      {
        job.cores = CCNT_1;
        PF_scalar_outer_product(Thread_context::get_master());
      }

#ifdef PERFORMANCE_COUNTERS_WANTED
const uint64_t end_1 = cycle_counter();
   Performance::fs_OPER2_OUTER_AB.add_sample(end_1 - start_1,
                                             job.ZAh * job.ZBl);
#endif
}
//----------------------------------------------------------------------------
void
Bif_OPER2_OUTER::PF_scalar_outer_product(Thread_context & tctx)
{
const ShapeItem Z_len = job.ZAh * job.ZBl;

const ShapeItem slice_len = (Z_len + job.cores - 1)/job.cores;
ShapeItem z = tctx.get_N() * slice_len;
ShapeItem end_z = z + slice_len;
   if (end_z > Z_len)   end_z = Z_len;

   for (; z < end_z; ++z)
       {
        const ShapeItem zah = z/job.ZBl;
        const ShapeItem zbl = z - zah*job.ZBl;
        job.ec = ((job.cB + zbl)->*job.RO)(job.cZ + z, job.cA + zah);
        if (job.ec != E_NO_ERROR)   return;
       }
}
//----------------------------------------------------------------------------
