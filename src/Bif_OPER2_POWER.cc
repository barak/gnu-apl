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
#include "Bif_OPER2_POWER.hh"
#include "IntCell.hh"
#include "Macro.hh"
#include "PointerCell.hh"
#include "Workspace.hh"

Bif_OPER2_POWER   Bif_OPER2_POWER::fun;

//════════════════════════════════════════════════════════════════════════════
void
Bif_OPER2_POWER::unstrand_RO_B(const UCS_string & LO_name, Value_P N_B,
                                Value_P & N, Value_P & B)
{
   /* Dyalog defines the reduction patterns for the POWER OPERATOR (Form 1) as:
`
      Z ← (A) (f ⍣ N) B

      i.e. A may be optional (depending on f) and the parentheses in (f ⍣ N)
      are required. GNU APL allows (like for the RANK operator in ISO) f ⍣ N
      without parentheses (provided that N is a scalar or 1-item value)
    */

   if (N_B->get_rank() > 1)
      {
        MORE_ERROR() << LO_name << "⍣N B: Bad rank " << N_B->get_rank()
                     << " (expecting ⍴⍴N B ≤ 1)";
        RANK_ERROR;
      }

   if (N_B->element_count() == 1)   // N_B is not a strand, e.g. (f ⍣ N) B
      {
        N = N_B;
        return;
      }

   if (N_B->element_count() != 2)
      {
        MORE_ERROR() << LO_name << "⍣N B: Bad length " << N_B->element_count()
                     << " (expecting ⍴N B = 2)";
        LENGTH_ERROR;
      }

Cell cache;
const Cell & first = N_B->get_cfirst(cache);
   if (!(first.is_numeric() && first.is_near_int()))
      {
        MORE_ERROR() << LO_name << "⍣N B: Bad type of argument N"
                        " (expecting an integer scalar)";
        DOMAIN_ERROR;
      }

   N = IntScalar(first.get_int_value(), LOC);
Cell cache2;
const Cell & second = N_B->get_cravel(1, cache2);
   if (Value_P v = second.try_pointer_value())   // f ⍣ N (B): B, not ⊂B
      {
        B = v;
        return;
      }
   B = Value_P(LOC);
   B->next_ravel_Cell(second);
   B->check_value(LOC);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_OPER2_POWER::eval_ALRB(cValue_R A, Token & LO, Token & RO, cValue_R B) const
{
   if (RO.get_ValueType() == TV_VAL)   // integer count
      return eval_form_1(CLONE(&A, LOC), LO, RO.get_apl_val(), CLONE(&B, LOC));
   if (RO.get_ValueType() == TV_INT)   // integer count
      return eval_form_1(CLONE(&A, LOC), LO, IntScalar(RO.get_int_val(), LOC), CLONE(&B, LOC));

   // Boolean termonation function
   return eval_form_2(CLONE(&A, LOC), LO, RO, CLONE(&B, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_OPER2_POWER::eval_LRB(Token & LO, Token & RO, cValue_R B) const
{
   if (RO.get_ValueType() == TV_VAL)   // integer count
      return eval_form_1(Value_P(), LO, RO.get_apl_val(), CLONE(&B, LOC));
   else                               // Boolean termonation function
      return eval_form_2(Value_P(), LO, RO, CLONE(&B, LOC));
}
//────────────────────────────────────────────────────────────────────────────
// the eval_form_1() function is for LO ⍣ N B and A LO ⍣ N B variants
// (with numeric RO and worker function LO)
Token
Bif_OPER2_POWER::eval_form_1(Value_P A, Token & _LO, Value_P N, Value_P B)
{
cFunction_P LO = _LO.get_function();
   Assert(LO);

   Assert(N);
   if (N->element_count() != 1)   // RANK ERROR like Dyalog (Bugs35 #38)
      {
        UCS_string & more = MORE_ERROR()
                       << "f⍣N B: N must be a scalar or 1-element vector; ";
        if (N->get_rank() > 1)   more << "⍴⍴N is " << N->get_rank();
        else                     more << "N has " << N->element_count()
                                      << " items";
        RANK_ERROR;
      }

   if (!N->is_near_int(0))
      {
        MORE_ERROR() << "f⍣N B: N is not an integer";
        DOMAIN_ERROR;
      }
Cell cache;
ShapeItem repeat_cnt = N->get_cfirst(cache).get_checked_near_int();

   // special cases: 0, negative, and 1
   //
   if (repeat_cnt == 0)
      {
        Value_P Z(B);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (repeat_cnt < 0)   // inverse
      {
        // A monadic call (!A) needs LO's MONADIC inverse, not its dyadic
        // one -- the two can differ (e.g. × has a real dyadic inverse,
        // A÷B, but no monadic one: signum isn't invertible). Using
        // get_dyadic_inverse() unconditionally here borrowed the dyadic
        // inverse and applied it monadically below, silently succeeding
        // with a meaningless result instead of the DOMAIN ERROR a
        // missing monadic inverse should give. See Bugs27 #22.
        //
        cFunction_P inverse = A ? LO->get_dyadic_inverse()
                                 : LO->get_monadic_inverse();
        if (inverse == 0)
           {
             MORE_ERROR() << "f⍣N B: f has no known inverse (needed for"
                             " N < 0); f is " << LO->get_name();
             DOMAIN_ERROR;   // no inverse for LO
           }

        LO = inverse;

        // negating repeat_cnt's own most negative value overflows (the
        // same hazard Bif_OPER1_REDUCE::do_reduce() guards for n_wise);
        // left unchecked, repeat_cnt stays negative, and the loop below
        // (`if (--repeat_cnt == 0) return ...`) then wraps it from
        // INT64_MIN down through INT64_MAX and applies the inverse
        // ~2⁶³ times -- a near-infinite loop.
        const ShapeItem neg_repeat_cnt = - repeat_cnt;
        if (Cell::diff_overflow(neg_repeat_cnt, 0, repeat_cnt))
           {
             MORE_ERROR() << "f⍣N B: N = " << repeat_cnt
                          << " overflows on negation";
             DOMAIN_ERROR;
           }
        repeat_cnt = neg_repeat_cnt;
      }

   if (repeat_cnt == 1)
      {
        if (!A)   return  LO->eval_B(*B);
        else      return  LO->eval_AB(*A, *B);
      }

   // at this point, repeat_cnt > 1
   //
   if (LO->may_push_SI())   // user-defined or macro
      {
        Token N(TOK_APL_VALUE1, IntScalar(repeat_cnt, LOC));
        if (LO->has_result())
           if (!A)   return Macro::get_macro(Macro::MAC_Z__LO_POWER_N_B)
                                 ->eval_LRB (   _LO, N, *B);
           else      return Macro::get_macro(Macro::MAC_Z__A_LO_POWER_N_B)
                                 ->eval_ALRB(*A, _LO, N, *B);
        else
           if (!A)   return Macro::get_macro(Macro::MAC_LO_POWER_N_B)
                                 ->eval_LRB (   _LO, N, *B);
           else      return Macro::get_macro(Macro::MAC_A_LO_POWER_N_B)
                                 ->eval_ALRB(*A, _LO, N, *B);
      }

   for (;;)
       {
         Token result = !A ? LO->eval_B(*B)
                           : LO->eval_AB(*A, *B);

         if (result.get_tag() == TOK_ERROR)   return result;
         Assert(result.get_Class() == TC_VALUE);
         if (--repeat_cnt == 0)   return result;
         B = result.get_apl_val();
       }
}
//────────────────────────────────────────────────────────────────────────────
// the eval_form_2() function is for LO ⍣ N B and A LO ⍣ N B variants
// (with condition function RO and worker function LO)
Token
Bif_OPER2_POWER::eval_form_2(Value_P A, Token & _LO, Token & _RO, Value_P B)
{
cFunction_P LO = _LO.get_function();   Assert(LO);
cFunction_P RO = _RO.get_function();   Assert(RO);

   if (!LO->has_result())
      {
        MORE_ERROR() << "f⍣g B: f must return a result; f is "
                     << LO->get_name();
        DOMAIN_ERROR;
      }
   if (!RO->has_result())
      {
        MORE_ERROR() << "f⍣g B: g must return a result; g is "
                     << RO->get_name();
        DOMAIN_ERROR;
      }

   if (LO->may_push_SI() || RO->may_push_SI())   // user-defined or macro
      {
        if (!A)   return Macro::get_macro(Macro::MAC_Z__LO_POWER_RO_B)
                              ->eval_LRB (_LO, _RO, *B);
        else      return Macro::get_macro(Macro::MAC_Z__A_LO_POWER_RO_B)
                              ->eval_ALRB(*A, _LO, _RO, *B);
      }

   // primitive LO and RO
   //
   for (;;)
       {
         const Token result_LO = !A ? LO->eval_B(*B) : LO->eval_AB(*A, *B);
         if (result_LO.get_tag() == TOK_ERROR)   return result_LO;

         Assert(result_LO.get_Class() == TC_VALUE);
         Value_P LO_Z = result_LO.get_apl_val();

         const Token result_RO = RO->eval_AB(*LO_Z, *B);
         if (result_RO.get_tag() == TOK_ERROR)   return result_RO;

         Assert(result_RO.get_Class() == TC_VALUE);
         Value_P condition = result_RO.get_apl_val();
         if (!condition->is_scalar_extensible() || !condition->is_near_bool(0))
            {
              MORE_ERROR() << "(LO⍣RO)B: the condition function RO must "
                              "return a Boolean scalar; ⍴,RO's result is "
                           << condition->get_shape();
              DOMAIN_ERROR;
            }
         if (condition->get_near_int(0) == 1)
            return Token(TOK_APL_VALUE1, LO_Z);

         if (InterruptContext::interrupt_is_raised())
            {
              InterruptContext::clear_attention_raised(LOC);
              InterruptContext::clear_interrupt_raised(LOC);
              INTERRUPT
            }

        B = LO_Z;
        LO_Z.clear(LOC);
      }
}
//════════════════════════════════════════════════════════════════════════════
