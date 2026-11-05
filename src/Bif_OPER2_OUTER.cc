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

#include "Bif_OPER1_REDUCE.hh"
#include "Bif_OPER2_OUTER.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Macro.hh"
#include "Workspace.hh"

Bif_JOT          Bif_JOT        ::fun;   // ∘
Bif_OPER2_OUTER  Bif_OPER2_OUTER::fun;   // A ∘.f B

Bif_OPER2_OUTER::PJob_product Bif_OPER2_OUTER::job;

//════════════════════════════════════════════════════════════════════════════
Token
Bif_JOT::eval_AB(cValue_R A, cValue_R B) const
{
   // compute A∘B ←→ A +.× B
   // the rank of A and B is 0..2
   //
   if (A.get_rank() > 2)
      {
        MORE_ERROR() << "A∘B: expecting ⍴⍴A ≤ 2; ⍴⍴A is " << A.get_rank();
        RANK_ERROR;
      }
   if (B.get_rank() > 2)
      {
        MORE_ERROR() << "A∘B: expecting ⍴⍴B ≤ 2; ⍴⍴B is " << B.get_rank();
        RANK_ERROR;
      }

   if (A.is_scalar() || B.is_scalar())
      return Bif_F12_TIMES::fun.eval_AB(A, B);

   // ranks are valid. check that A and B are numeric (so that we can depend
   // on it below instead of testing it multiple times.
   //
   loop(a, A.nz_element_count())
       {
         if (!A.is_numeric(a))
            {
              MORE_ERROR() << "A∘B: Bad type of argument A"
                              " (expecting numeric)";
              DOMAIN_ERROR;
            }
       }

   loop(b, B.nz_element_count())
       {
         if (!B.is_numeric(b))
            {
              MORE_ERROR() << "A∘B: Bad type of argument B"
                              " (expecting numeric)";
              DOMAIN_ERROR;
            }
       }

ShapeItem rows_A = A.get_rows();
ShapeItem cols_A = A.get_cols();
ShapeItem rows_B = B.get_rows();
ShapeItem cols_B = B.get_cols();

   // we allow A resp. B to be row- resp. column-vectors
   //
   if (A.get_rank() == 1)   // (row-)vector A ∘ matrix B
      {
        rows_A = 1;
        cols_A = A.element_count();
        rows_B = B.get_rows();
        cols_B = B.get_cols();
      }

   if (B.get_rank() == 1)   // matrix A ∘ (column-)vector B
      {
        rows_A = A.get_rows();
        cols_A = A.get_cols();
        rows_B = B.element_count();
        cols_B = 1;
      }

   // NOTE (reverting r3469 / L14 from the external Bugs.md audit): a
   // stricter "cols_A != rows_B -> LENGTH_ERROR" check was tried here
   // (matching how +.× itself behaves) and reverted twice now: per
   // apl.texi, A∘B (unlike A +.× B) has no requirement that cols_A
   // equal rows_B — it implicitly zero-pads the shorter of the two,
   // and min() below is exactly that: elements beyond len never enter
   // the sum, equivalent to multiplying by 0-columns/0-rows that are
   // "optimized away". testcases/Domino_stress.tc's ⌹[3]
   // (QR-factorization) path relies on this: B ≡ Q∘R where Q is the
   // full M×M orthogonal factor and R is the compact N×N triangular
   // factor (M>N) — mathematically correct because R's rows past N
   // are zero, so Q∘R ←→ Q[:,1:N]∘R, exactly the zero-pad identity.
   //
const ShapeItem len = min(cols_A, rows_B);

Shape shape_Z(rows_A, cols_B);
Value_P Z(shape_Z, LOC);

   const RavelType rt_A = A.get_ravel_type();
   const RavelType rt_B = B.get_ravel_type();

   if ((rt_A & RPT_real) && (rt_B & RPT_real))   // both real-only — no complex checks needed
      {
        loop(a, rows_A)
            loop(b, cols_B)
               {
                 APL_Float sum = 0;
                 loop(ab, len)
                    sum += A.get_real_value(a*cols_A+ab) * B.get_real_value(b+ab*cols_B);
                 Z->next_ravel_Number(sum);
               }
      }
   else
      {
        loop(a, rows_A)
            {
              loop(b, cols_B)
                  {
                    APL_Float sum_real = 0;
                    APL_Float sum_imag = 0;
                    bool need_complex = false;
                    Cell aa_cache;
                    Cell bb_cache;
                    loop(ab, len)   // column of A × row of B
                       {
                         const Cell & aa = A.get_cravel(a * cols_A + ab, aa_cache);
                         const Cell & bb = B.get_cravel(b + ab*cols_B, bb_cache);
                         sum_real += aa.get_real_value() * bb.get_real_value();
                         if (aa.is_complex_cell())   // complex aa and any b
                            {
                              need_complex = true;
                              if (bb.is_complex_cell())   // complex aa and bb
                                 {
                                   sum_real -= aa.get_imag_value() *
                                               bb.get_imag_value();
                                   sum_imag += aa.get_real_value() *
                                               bb.get_imag_value();
                                   sum_imag += aa.get_imag_value() *
                                               bb.get_real_value();
                                 }
                              else                        // complex aa and real bb
                                 {
                                   sum_imag += aa.get_imag_value() *
                                               bb.get_real_value();
                                 }
                            }
                         else if (bb.is_complex_cell())   // real aa and complex bb
                            {
                              need_complex = true;
                              sum_imag += aa.get_real_value() * bb.get_imag_value();
                            }
                       }
                    if (need_complex)   Z->next_ravel_Complex(sum_real, sum_imag);
                    else                Z->next_ravel_Number(sum_real);
                  }
            }
      }

   Z->set_default(B, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_OPER2_OUTER::eval_ALRB(cValue_R A, Token & LO, Token & _RO, cValue_R B) const
{
   if (!_RO.is_function())    SYNTAX_ERROR;

cFunction_P RO = _RO.get_function();
   Assert(RO);

   if (!RO->has_result())
      {
        MORE_ERROR() << "A∘.RO B: RO must return a result; RO is "
                     << RO->get_name();
        DOMAIN_ERROR;
      }

Value_P Z(A.get_shape() + B.get_shape(), LOC);

   // is_empty() checked BEFORE the scalar fast path below: that fast
   // path, for an empty result, ran 0 iterations and then set Z's
   // prototype from B alone (Z->set_default(B,LOC)) -- skipping RO's own
   // fill function entirely and losing any contribution from A, unlike
   // the general (non-scalar-RO) path just below, which correctly
   // computes the fill element via RO->eval_fill_AB(). Checking here
   // routes the empty case through that existing, correct logic instead,
   // with no effect on the (far more common) non-empty case, which still
   // takes the fast path exactly as before.
   if (Z->is_empty())
      {
        Value_P Fill_A = Bif_F12_TAKE::first(A);
        Value_P Fill_B = Bif_F12_TAKE::first(B);

        Value_P Z1 = RO->eval_fill_AB(*Fill_A, *Fill_B).get_apl_val();

        // Bugs28 #37: Z1 (RO's fill result, e.g. A f¨B's enclosed fill
        // item for a non-scalar RO like ,) can itself be a non-scalar
        // or nested value, not just a simple scalar cell -- copying
        // only Z1's first CELL (as the old set_ravel_Cell() call below
        // did unconditionally) silently dropped everything but that
        // one cell/level. set_ravel_Value() does the right thing for
        // either case (copy the cell for a simple scalar, else wrap in
        // a fresh PointerCell), mirroring EACH's own empty-fill logic.
        // to_type() below then converts a numeric fill result of the
        // wrong VALUE (e.g. RO's identity element, 3) into the correct
        // wproto-derived default (0), matching what f¨ already does.
        //
        Z->set_ravel_Value(0, Z1.get());
        Z->check_value(LOC);

        // to_type(false), NOT to_type(true): a user-defined RO's fill
        // result (UserFunction::eval_fill_AB() just returns B's own
        // fill cell unevaluated) can legitimately be a *character*
        // cell, e.g. ⍬∘.{⍺+⍵}'ab' -- its empty result's type must stay
        // character (a space), not get force-converted to numeric 0.
        //
        Z->to_type(false);
        return Token(TOK_APL_VALUE1, Z);
      }

   // an important (and the most likely) special case is RO being a scalar
   // function. This case can be implemented in a far simpler fashion than
   // the general case.
   //
   if (RO->get_scalar_f2() && A.is_simple() && B.is_simple())
      {
        job.VZ     = Z.get();
        job.idxZ   = 0;
        job.VA     = &A;
        job.idxA   = 0;
        job.ZAh    = A.element_count();
        job.RO     = RO->get_scalar_f2();
        job.VB     = &B;
        job.idxB   = 0;
        job.ZBl    = B.element_count();
        job.ec     = E_NO_ERROR;

        scalar_outer_product();
        if (job.ec != E_NO_ERROR)   throw_apl_error(job.ec, LOC);

        Z->set_default(B, LOC);
        Z->try_pack();   // result type matches RO's output for homogeneous inputs
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (RO->may_push_SI())   // user defined LO
      {
        return Macro::get_macro(Macro::MAC_Z__A_LO_OUTER_B)
                    ->eval_ALB(A, _RO, B);
      }

const ShapeItem len_B = B.element_count();
const ShapeItem len_Z = A.element_count() * len_B;

Value_P RO_A;
Value_P RO_B;

   Cell cA_cache;
   Cell cB_cache;
   loop(z, len_Z)
      {
        const Cell & cA = A.get_cravel(z / len_B, cA_cache);
        const Cell & cB = B.get_cravel(z % len_B, cB_cache);

        if (Value_P v = cA.try_pointer_value())
           {
             RO_A = v;
           }
        else
           {
             RO_A = Value_P(LOC);   // scalar RO_A
             RO_A->set_ravel_Cell(0, cA);
             RO_A->check_value(LOC);
           }

        if (Value_P v = cB.try_pointer_value())
           {
             RO_B = v;
           }
        else
           {
             RO_B = Value_P(LOC);   // scalar RO_B
             RO_B->set_ravel_Cell(0, cB);
             RO_B->check_value(LOC);
           }

        Token result = RO->eval_AB(*RO_A, *RO_B);

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

   Z->set_default(B, LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_OPER2_OUTER::scalar_outer_product() const
{
#ifdef cfg_PERFORMANCE_COUNTERS_WANTED
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

#ifdef cfg_PERFORMANCE_COUNTERS_WANTED
const uint64_t end_1 = cycle_counter();
   Performance::fs_OPER2_OUTER_AB.add_sample(end_1 - start_1,
                                             job.ZAh * job.ZBl);
#endif
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_OPER2_OUTER::PF_scalar_outer_product(Thread_context & tctx)
{
const ShapeItem Z_len = job.ZAh * job.ZBl;

const ShapeItem slice_len = (Z_len + job.cores - 1)/job.cores;
ShapeItem z = tctx.get_N() * slice_len;
ShapeItem end_z = z + slice_len;
   if (end_z > Z_len)   end_z = Z_len;

   Cell cacheA;
   Cell cacheB;
   for (; z < end_z; ++z)
       {
        const ShapeItem zah = z/job.ZBl;
        const ShapeItem zbl = z - zah*job.ZBl;
        const Cell & cellA = job.VA->get_cravel(job.idxA + zah, cacheA);
        const Cell & cellB = job.VB->get_cravel(job.idxB + zbl, cacheB);
        job.ec = (cellB.*job.RO)(&job.VZ->get_wravel(job.idxZ + z), &cellA);
        if (job.ec != E_NO_ERROR)   return;
       }
}
//════════════════════════════════════════════════════════════════════════════
