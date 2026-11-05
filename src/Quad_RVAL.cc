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

#include <stdlib.h>
#include <string.h>

#include "Avec.hh"
#include "Common.hh"
#include "ComplexCell.hh"
#include "Quad_RVAL.hh"
#include "Value.hh"

vector<int> Quad_RVAL::desired_ranks;
Shape       Quad_RVAL::desired_shape;
vector<int> Quad_RVAL::desired_types;
int         Quad_RVAL::desired_maxdepth;
ShapeItem   Quad_RVAL::desired_max_ecount;
int         Quad_RVAL::desired_purpose = 0;
char        Quad_RVAL::state[256];
size_t      Quad_RVAL::N = 256;

const FunctionGroup::function_info Quad_RVAL::subfunction_infos[] =
{
#define rvaldef(N, fun, comm_2) { N, #fun, "", comm_2, -1 },
  rvaldef(0, state,  "get (B=⍬) or set (B≠⍬) the random number generator state" )
  rvaldef(1, rank,   "set the rank of subsequently returned random values"       )
  rvaldef(2, shape,  "set the shape of subsequently returned random values"      )
  rvaldef(3, type,   "set the data type of subsequently returned random values"  )
  rvaldef(4, depth,  "set the depth of subsequently returned random values"      )
  rvaldef(5, ecount,     "set the max. element count (⍴Z) per random value (0=∞)"  )
  rvaldef(6, primitives, "return the primitive arity/stimulus/constraint table"     )
  rvaldef(7, conform,    "generate a value conforming to B (scalar or ⍴B)"          )
  rvaldef(8, purpose,    "set the purpose of subsequently returned values (0 or 1)" )
};

Quad_RVAL  Quad_RVAL::fun;

// ⎕RVAL depends on glibc, so we use it only in development mode

//════════════════════════════════════════════════════════════════════════════
Quad_RVAL::Quad_RVAL()
   : QuadFunction(TOK_Quad_RVAL)
{
enum { count = sizeof(subfunction_infos) / sizeof(*subfunction_infos) };
   init_function_group(subfunction_infos, count, "⎕RVAL");

   N = 8;
   desired_maxdepth = 4;
   desired_max_ecount = 0;   // 0 = unlimited
   memset(state, 0, sizeof(state));
#if ! MINGW_SRC
   initstate(1, state, N);
#endif // ! MINGW_SRC

   while (desired_shape.get_rank() < MAX_RANK)
         desired_shape.add_shape_item(1);

   desired_ranks.push_back(0);

   desired_types.push_back(0);   // no chars
   desired_types.push_back(1);   // ints
   desired_types.push_back(0);   // no real
   desired_types.push_back(0);   // no complex
   desired_types.push_back(0);   // no nested
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_RVAL::eval_AB(cValue_R A, cValue_R B) const
{
const sAxis subfunction = value_to_subfun(A);

Value_P Z = do_eval_AB(subfunction, B);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_RVAL::eval_B(cValue_R B) const
{
   if (B.is_str0())    return list_functions(CERR);
   if (B.is_zilde())   return list_mappings(CERR);
   if (B.is_empty())   DOMAIN_ERROR;
   if (B.get_rank() > 1)
      {
        MORE_ERROR() << "⎕RVAL B: rank of B must be ≤ 1.";
        RANK_ERROR;
      }

   return Token(TOK_APL_VALUE1, do_eval_B(B, 0, -1));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_RVAL::eval_XB(cValue_R X, cValue_R B) const
{
   return eval_AB(X, B);
}
//────────────────────────────────────────────────────────────────────────────
void Quad_RVAL::print_fun_syntax(ostream & out,
                                 const function_info & info) const
{
   out << "    ⎕RVAL[" << info.axis << "] B   ⍝ " << info.comment_fun << endl;
}
//────────────────────────────────────────────────────────────────────────────
void Quad_RVAL::print_map_syntax(ostream & out,
                                 const function_info & info) const
{
const char * name = info.function_name;
const UCS_string blanks(max_function_name_length - strlen(name), UNI_SPACE);

   out << "   ⎕RVAL[" << info.axis << "]  ←→  ⎕RVAL['" << name << "']"
       << blanks << "  ←→  ⎕RVAL." << name << endl;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::do_eval_B(const cValue & B, int depth, ShapeItem budget) const
{
ShapeItem len_B = B.element_count();

   if (len_B > 5)
      {
        MORE_ERROR() << "monadic ⎕RVAL B expects at most 5 properties B←"
                        "(rank, (shape), (type), maxdepth, and ecount)";
        LENGTH_ERROR;
      }

   if (B.is_scalar())
      {
        // ⎕RVAL 0 (a scalar 0): no property overrides, i.e. return the
        // next random value according to the properties set with dyadic
        // A ⎕RVAL B (as documented). Any other scalar B is a 1-element
        // property vector (the rank), e.g. ⎕RVAL 2 is ⎕RVAL ,2 and returns
        // a matrix. Note that ⎕RVAL ,0 (a vector) therefore still forces
        // rank 0, i.e. a scalar, regardless of the current rank setting.
        //
        Cell cache;
        const Cell & cB = B.get_cfirst(cache);
        len_B = (cB.is_near_int() && cB.get_near_int() == 0) ? 0 : 1;
      }

   // save properties so that we can restore them
   //
vector<int> old_desired_ranks     = desired_ranks;
Shape       old_desired_shape     = desired_shape;
vector<int> old_desired_types     = desired_types;
int         old_desired_maxdepth  = desired_maxdepth;
ShapeItem   old_desired_max_ecount = desired_max_ecount;
bool need_restore = false;

   try {
         need_restore = true;   // ensure restore on any exception below

         if (len_B >= 1)   // rank: scalar or enclosed vector (distribution)
            {
              if (Value_P v = B.try_pointer_value(0))
                 {
                   do_eval_AB(1, *v);
                 }
              else   // rank as scalar
                 {
                   Value_P rank = IntScalar(B.get_int_value(0), LOC);
                   do_eval_AB(1, *rank);
                 }
            }

         if (len_B >= 2)   // shape: scalar or enclosed vector
            {
              if (Value_P v = B.try_pointer_value(1))
                 {
                   do_eval_AB(2, *v);
                 }
              else   // shape as scalar: Z← (rank⍴ec_B)⍴random_data
                 {
                   Value_P rank = IntScalar(B.get_int_value(1), LOC);
                   do_eval_AB(2, *rank);
                 }
            }

         if (len_B >= 3)   // type: always enclosed vector (distribution)
            {
              Value_P v = B.try_pointer_value(2);
              if (!v)
                 {
                   MORE_ERROR() << "⎕RVAL B: B[2] (type distribution) must be "
                                   "an enclosed vector";
                   DOMAIN_ERROR;
                 }
              do_eval_AB(3, *v);
            }

         if (len_B >= 4)   // maxdepth: scalar or 1-element vector
            {
              if (Value_P v = B.try_pointer_value(3))   // maxdepth as 1-element vector
                 {
                   do_eval_AB(4, *v);
                 }
              else   // maxdepth as scalar
                 {
                   Value_P rank = IntScalar(B.get_int_value(3), LOC);
                   do_eval_AB(4, *rank);
                 }
            }

         if (len_B >= 5)   // ecount: scalar or 1-element vector
            {
              if (Value_P v = B.try_pointer_value(4))   // ecount as 1-element vector
                 {
                   do_eval_AB(5, *v);
                 }
              else   // ecount as scalar
                 {
                   Value_P ec = IntScalar(B.get_int_value(4), LOC);
                   do_eval_AB(5, *ec);
                 }
            }
       }
    catch (Error &)
      {
        desired_ranks      = old_desired_ranks;
        desired_shape      = old_desired_shape;
        desired_types      = old_desired_types;
        desired_maxdepth   = old_desired_maxdepth;
        desired_max_ecount = old_desired_max_ecount;
        throw;
      }
    catch (std::bad_alloc &)
      {
        desired_ranks      = old_desired_ranks;
        desired_shape      = old_desired_shape;
        desired_types      = old_desired_types;
        desired_maxdepth   = old_desired_maxdepth;
        desired_max_ecount = old_desired_max_ecount;
        throw;
      }
    catch (...)
      { FIXME; }

   Log(LOG_Quad_RVAL)
      {
        CERR << "⎕RVAL B:" << endl << "desired_ranks:   ";
        loop(r, desired_ranks.size())   CERR << " " << desired_ranks[r];
        CERR << endl << "desired_shape:    " << desired_shape << endl;
        CERR << "desired_types:   ";
        loop(t, desired_types.size())   CERR << " " << desired_types[t];
        CERR << endl << "desired_maxdepth: " << desired_maxdepth << endl;
      }

   // At the top level (depth 0), the total recursive budget for the
   // whole (sub-)tree is the ecount just parsed from B above. Recursive
   // calls (depth > 0, from random_nested()) already receive their share
   // of the parent's budget via the budget parameter, so they must not
   // be overridden here.
   //
   if (depth == 0)
      budget = (desired_max_ecount > 0) ? desired_max_ecount : -1;

const sRank rank = choose_integer(desired_ranks);
Shape shape;
   for (sRank r = MAX_RANK - rank; r < MAX_RANK; ++r)
       {
         vector<int> vsh_r;
         vsh_r.push_back(desired_shape.get_shape_item(r));
         const int sh_r = choose_integer(vsh_r);
         shape.add_shape_item(sh_r);
       }

   // Clip element count to budget (negative = no limit). Produce the
   // top-level ravel as before, but bounded by this sub-value's own
   // (possibly recursively divided) budget rather than the raw ecount.
   // If the randomly chosen shape exceeds the limit, shrink axes
   // (largest first) until ×/shape ≤ budget or every axis is 1.
   //
   // Shrinking only the *last* axis (as this used to do) truncates via
   // integer division, which silently rounds to 0 whenever the product
   // of the OTHER axes alone already exceeds budget (common for rank
   // ≥ 2 shapes at small budgets) -- a much bigger, more surprising
   // change than "cap the size" was ever meant to make (confirmed via
   // a real fuzzer sweep: this alone was responsible for the bulk of
   // A⍋B/A⍒B/A⌷B's remaining DOMAIN/INDEX ERRORs, since downstream
   // code generally isn't prepared for a zero-length axis appearing
   // out of nowhere). Simply flooring that one axis at 1 instead (an
   // earlier version of this fix) creates a different problem: the
   // *other* axes are left at their full, un-shrunk random draw, so
   // the shape can end up enormously over budget (confirmed: this
   // alone was enough to blow AllPrimitives.tc's self-timed 5-second
   // fuzz budget out to 20+ seconds). Shrinking the largest axis
   // repeatedly instead keeps every axis budget-aware, the same
   // guarantee the "last axis only" approach was trying to give,
   // without either failure mode.
   //
   if (budget >= 0 && rank > 0)
      {
        // saturate rather than compute shape's raw (potentially huge)
        // volume unchecked: a randomly drawn rank-N shape can overflow
        // ShapeItem (confirmed via UBSan/AllPrimitives.tc), and a wrapped
        // (possibly negative) ec_trial can then fail "ec_trial > budget"
        // immediately, skipping the whole shrinking loop below and
        // handing an enormous shape downstream -- exactly the failure
        // mode this loop exists to prevent (Bugs18 #9).
        //
        ShapeItem ec_trial = 1;
        loop(r, rank)
            {
              const ShapeItem s = shape.get_shape_item(r);
              if (s == 0)   { ec_trial = 0;   break; }
              if (ec_trial > budget / s)   { ec_trial = budget + 1;   break; }
              ec_trial *= s;
            }
        while (ec_trial > budget)
           {
             sRank biggest = 0;
             loop(r, rank)
                if (shape.get_shape_item(r) > shape.get_shape_item(biggest))
                   biggest = r;

             if (shape.get_shape_item(biggest) <= 1)   break;   // every axis is 1

             shape.set_shape_item(biggest, shape.get_shape_item(biggest) - 1);

             // Recompute the (saturating) volume from scratch rather than
             // rescale ec_trial in place: ec_trial may have started life
             // as the sentinel budget+1 (set above when the raw volume
             // overflowed the saturation check), not the real volume, and
             // rescaling that sentinel via /=/*= let this loop exit with a
             // shape still enormously over budget (Blake McBride, Bugs20 #3).
             //
             ec_trial = 1;
             loop(r, rank)
                {
                  const ShapeItem s = shape.get_shape_item(r);
                  if (s == 0)   { ec_trial = 0;   break; }
                  if (ec_trial > budget / s)   { ec_trial = budget + 1;   break; }
                  ec_trial *= s;
                }
           }
      }

Value_P Z(shape, LOC);

const ShapeItem ec = Z->element_count();

   // Pass 1: decide (but do not yet generate) the type of every cell, and
   // virtually split the ravel into S simple and N nested cells. Every
   // simple cell costs 1 unit of budget; every nested cell needs at
   // least 1 unit for its own (recursive) sub-value. If there is not
   // enough budget left for that (S + N > budget, i.e. N > budget - S),
   // demote the excess nested cells to a simple type. This is what
   // bounds the *total* (recursive) element count to budget, instead of
   // every nested sub-value independently claiming up to the full
   // budget again (which is what caused the exponential blow-up).
   //
vector<int> types(ec);
ShapeItem S = 0;
ShapeItem N = 0;
   loop(z, ec)
      {
        int type_z;  do    { type_z = choose_integer(desired_types); }
                      while (depth == desired_maxdepth && type_z == 4);
        types[z] = type_z;
        if (type_z == 4)   ++N;   else   ++S;
      }

   if (budget >= 0 && N > (budget - S))
      {
        ShapeItem excess = N - (budget - S);
        if (excess > N)   excess = N;   // budget < S: demote all nested cells

        // a distribution over the simple types only (nested excluded)
        vector<int> simple_types(desired_types.begin(),
                                  desired_types.begin() + 4);
        const bool any_simple = simple_types[0] || simple_types[1]
                              || simple_types[2] || simple_types[3];

        loop(z, ec)
           {
             if (excess == 0)      break;
             if (types[z] != 4)    continue;
             types[z] = any_simple ? choose_integer(simple_types) : 1;
             --excess;   --N;   ++S;
           }
      }

   // Pass 2: generate the cells. Nested cells fairly share what is left
   // of the budget (after the S simple cells) among the still-
   // unprocessed nested cells, and recurse with that sub-budget.
   //
ShapeItem remaining_budget = (budget >= 0) ? (budget - S) : -1;
ShapeItem remaining_N = N;
   loop(z, ec)
      {
         switch(types[z])
            {
               case 0:   random_character(*Z);          continue;
               case 1:   random_integer(*Z);            continue;
               case 2:   random_float(*Z);              continue;
               case 3:   random_complex(*Z);            continue;
               case 4:
                    {
                      const ShapeItem SB = (remaining_budget >= 0)
                                          ? remaining_budget / remaining_N
                                          : -1;
                      random_nested(*Z, B, depth, SB);
                      if (remaining_budget >= 0)   remaining_budget -= SB;
                      --remaining_N;
                      continue;
                    }
               default:  FIXME;
            }
      }

   if (need_restore)
      {
        desired_ranks      = old_desired_ranks;
        desired_shape      = old_desired_shape;
        desired_types      = old_desired_types;
        desired_maxdepth   = old_desired_maxdepth;
        desired_max_ecount = old_desired_max_ecount;
      }

   if (ec == 0)   Z->set_proto_Int();

   Z->check_value(LOC);
   Z->try_pack(true);   // force-pack even below threshold (stresses packed-ravel paths)
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_RVAL::random_nested(Value & Z, const cValue & B, int depth,
                         ShapeItem budget) const
{
Value_P Zsub;

   // do_eval_B() can legitimately be forced to produce a simple scalar
   // no matter how many times it's retried: once the recursive budget is
   // small enough, Pass 1's demotion (see do_eval_B, "if there is not
   // enough budget left for that... demote the excess nested cells to a
   // simple type") downgrades every candidate nested cell to simple on
   // every attempt. This retry-until-nested loop then never terminates
   // (found via a real, reproducible hang, not a hypothetical). Bound
   // the retries and accept whatever was generated on the last attempt.
   // next_ravel_Value (unlike next_ravel_Pointer) handles a still-simple
   // Zsub safely by writing it as a plain cell instead of asserting.
   //
enum { MAX_RETRIES = 20 };
int retries = 0;
   do Zsub = do_eval_B(B, depth + 1, budget);
   while (Zsub->is_simple_scalar() && ++retries < MAX_RETRIES);

   Z.next_ravel_Value(Zsub.get());
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::conform_value(const cValue & Bref) const
{
   // temporarily force the rank/shape distributions to either scalar or
   // exactly Bref's shape, then generate through the normal machinery.
   //
   // Also force the type distribution to the types actually present in
   // Bref's own (top-level) ravel: do_eval_B() below always reverts
   // desired_types to whatever it was *before* this call once it is
   // done (see the unconditional restore at the end of do_eval_B) --
   // including after the very call that generated Bref in the first
   // place. So by the time we get here, desired_types no longer
   // reflects Bref's type mask at all; it holds leftover state from
   // whatever unrelated ⎕RVAL call last ran (found via a real fuzzer
   // sweep: A_stim="~B" rows like A+B were generating A with char/
   // nested cells even though B's own type mask excluded them,
   // producing spurious DOMAIN ERRORs). Deriving the mask straight
   // from Bref's actual cells is self-sufficient and matches "conform
   // to B" regardless of what ⎕RVAL call preceded this one.
   //
vector<int> old_ranks = desired_ranks;
Shape       old_shape = desired_shape;
vector<int> old_types = desired_types;

   {
     vector<int> types_seen(5, 0);
     const ShapeItem ec = Bref.nz_element_count();
     Cell cache;
     loop(b, ec)
        {
          const Cell & cell = Bref.get_cravel(b, cache);
          if      (cell.is_pointer_cell())     types_seen[4] = 1;
          else if (cell.is_character_cell())   types_seen[0] = 1;
          else if (cell.is_complex_cell())     types_seen[3] = 1;
          else if (cell.is_float_cell())       types_seen[2] = 1;
          else if (cell.is_integer_cell())     types_seen[1] = 1;
          else                                 types_seen[1] = 1;
        }

     const bool any_simple = types_seen[0] || types_seen[1]
                           || types_seen[2] || types_seen[3];
     if (any_simple)   desired_types = types_seen;
     // else: Bref was all-nested or empty -- leave desired_types as-is
     // rather than pass result_type() a nested-only (or all-0) mask,
     // both of which it rejects with DOMAIN_ERROR.
   }

   if (rand17() & 1)   // 50%: scalar
      {
        desired_ranks.clear();
        desired_ranks.push_back(0);
      }
   else                // 50%: exactly Bref's shape
      {
        desired_ranks.clear();
        desired_ranks.push_back(Bref.get_rank());

        // do_eval_B() reads shape items from the *trailing* 'rank'
        // positions of desired_shape (see result_shape()), so leading
        // positions must be padded with 1s, not trailing ones.
        //
        Shape new_shape;
        loop(r, MAX_RANK - Bref.get_rank())   new_shape.add_shape_item(1);
        loop(r, Bref.get_rank())
            new_shape.add_shape_item(Bref.get_shape_item(r));
        desired_shape = new_shape;
      }

   // Idx0() (⍬) has element_count() 0, so do_eval_B() parses none of its
   // (rank, shape, type, depth, ecount) properties from it and instead
   // uses the ranks/shape just forced above (together with whatever
   // type/depth/ecount is already configured).
   //
Value_P Z = do_eval_B(*Idx0(LOC), 0, -1);

   desired_ranks = old_ranks;
   desired_shape = old_shape;
   desired_types = old_types;
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
int
Quad_RVAL::choose_integer(const vector<int> & dist)
{
const int dist_len = dist.size();
   Assert(dist_len > 0);

   /* a distribution with a single value N shall mean:

       N > 0: fixed value N
       N < 0: equal distribution 0..N-1
    */
    if (dist_len == 1)   // fixed value or equal distribution 0, 1, ... n
      {
        const int desired = dist[0];
        if (desired >= 0)   return desired;   // fixed value
        const int rand = rand17();
        return rand % (1 - desired);          // = 0 ... |desired|
      }

   // dist is a distribution...

   // 1. compute the sum of the weights (each weight is < 2⋆31, so the sum
   //    of at most MAX_RANK + 1 weights fits into an int64_t).
   //
int64_t sum = 0;
   for (size_t d = 0; d < dist.size(); ++d)   sum += dist[d];

   // an all-zero distribution (every weight 0, e.g. ⎕RVAL (⊂0 0)) makes
   // sum == 0, and "% sum" below is then a division by zero (SIGFPE),
   // confirmed directly. There is no meaningful weighted choice among
   // all-zero weights, so DOMAIN_ERROR rather than silently picking
   // index 0.
   if (sum <= 0)
      {
        MORE_ERROR() << "⎕RVAL: a weight distribution must have a "
                        "positive sum (all weights were 0 or negative)";
        DOMAIN_ERROR;
      }

   // 2. pick a random number 0...sum
   //
   //    A single rand17() has only 16 good bits: with a sum > 0xFFFF the
   //    later items could never be chosen (Bugs35 #21). Use 64 bits.
   //
uint64_t rnd = 0;
   loop(r, 4)   rnd = (rnd << 16) | (rand17() & 0xFFFF);
const int64_t rand = rnd % uint64_t(sum);

   // 3. return the index of rand in +\dist
   //
   sum = 0;
   for (size_t d = 0; d < dist.size(); ++d)
       {
          sum += dist[d];
          if (rand < sum)   return d;
       }

   FIXME;   // not reached
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::do_eval_AB(int subfunction, const cValue & B)
{
   switch(subfunction)
      {
        case 0: return generator_state(B);
        case 1: return result_rank(B);
        case 2: return result_shape(B);
        case 3: return result_type(B);
        case 4: return result_maxdepth(B);
        case 5: return result_ecount(B);
        case 6: return prim_table_value(B);
        case 7: return fun.conform_value(B);
        case 8: return result_purpose(B);
      }

   fun.bad_subfun_number_ERROR(subfunction);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::generator_state(const cValue & B)
{
   // expect an empty, 8, 16, 32, 64, 128, or 256 byte integer vector
   //
   if (B.get_rank() != 1)   RANK_ERROR;

const ShapeItem new_N = B.element_count();
   if (new_N !=   0 && new_N !=   8 && new_N !=  16 &&
       new_N !=  32 && new_N !=  64 && new_N != 128 && new_N != 256)
      {
        MORE_ERROR() << "bad new_N (aka. ⍴B) in generator_state()";
        LENGTH_ERROR;
      }

   // always return the previous state
   //
Value_P Z(N, LOC);
   loop(n, N)   Z->next_ravel_Int(state[n] & 0xFF);
   Z->check_value(LOC);

   if (new_N)   // set generator state
      {
        // make sure that all values are bytes
        //
        loop(b, new_N)
            {
              const APL_Integer byte = B.get_int_value(b);
              if ((byte < -256) || (byte >  255))
                 {
                   MORE_ERROR() << "Bad right argument B in 0 ⎕RVAL B,"
                                   "expecting bytes (integers -256...255)";
                   DOMAIN_ERROR;
                 }
            }
        N = new_N;
        loop(b, N)
            {
               state[b] = B.get_int_value(b);
            }
#if ! MINGW_SRC
         setstate(state);
#endif // ! MINGW_SRC
      }

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
uint64_t
Quad_RVAL::rand17()
{
#if ! MINGW_SRC
const int32_t rnd = random();

   // the lower bits are less random, so we xor the upper 16 bits into
   // the lower 16 bits and return them.
   return (rnd ^ (rnd >> 16)) & 0x1FFFF;
#else
   return 0;
#endif // ! MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_RVAL::random_character(Value & Z)
{
const int32_t rnd = rand17();
   if (desired_purpose == 1)   // APL2 mode: a character of IBM APL2's ⎕AV
      {
        // Avec::IBM_quad_AV() (GNU APL's table of IBM APL2's ⎕AV, used by
        // )IN and )OUT) differs from the real ⎕AV of IBM APL2 at a few
        // positions (verified with APL2 itself): 0x0B is a control character
        // (not ⍬), 0x9E is ₧ (not ⍶), 0xD1 is ⍷ (not ⋸), and 0xE1 is ß (not
        // ⍹). The table is left as is (backward compatibility of )IN and
        // )OUT); APL2 mode simply avoids these positions, and also the
        // control characters and DEL (which break testcase files).
        //
        static std::vector<Unicode> apl2_chars;
        if (apl2_chars.size() == 0)
           {
             const Unicode * av = Avec::IBM_quad_AV();
             loop(p, 256)
                {
                  if (p < 0x20 || p == 0x7F)   continue;   // control, DEL
                  if (p == 0x9E || p == 0xD1 || p == 0xE1)   continue;
                  apl2_chars.push_back(av[p]);
                }
           }
        Z.next_ravel_Char(apl2_chars[rnd % apl2_chars.size()]);
        return;
      }

   // bits 0-12: codepoint base (0..8191); bit 13 adds 0x1000 (skips C0/C1 ranges)
   Z.next_ravel_Char(Unicode((rnd & 0x1FFF) + ((rnd & 0x2000) >> 1)));
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_RVAL::random_complex(Value & Z)
{
   Z.next_ravel_Complex(random_ieee(), random_ieee());
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_RVAL::random_float(Value & Z)
{
   Z.next_ravel_Float(random_ieee());
}
//────────────────────────────────────────────────────────────────────────────
double
Quad_RVAL::random_ieee()
{
double f;
   do {
        const int64_t rand64 = rand17()
                             ^ (rand17() << 16)
                             ^ (rand17() << 32)
                             ^ (rand17() << 48);

        enum { BIAS     = 1023,          // IEEE
               EXPO     = 0 + BIAS,      // 2^0
             };

        // sign +, exponent EXPO, and a random 52 bit fraction (bits 0-47 and
        // 56-59 of rand64). Assemble the bits in a uint64_t and memcpy() them
        // into f, rather than storing bytes into a union, so that the result
        // does not depend on the host byte order.
        //
        const uint64_t bits = uint64_t(EXPO) << 52
                            | (uint64_t(rand64) >> 56 & 0x0F) << 48
                            | (uint64_t(rand64) & 0xFFFFFFFFFFFFULL);
        memcpy(&f, &bits, sizeof(f));
      } while (!isnormal(f));

   // at this point: 1.0 < f < 2.0
   //
   if (desired_purpose == 1)   // APL2 mode: an IEEE 32-bit (float) value
      return double(float(f - 1.0));

   return f - 1.0;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_RVAL::random_integer(Value & Z)
{
   if (desired_purpose == 1)   // APL2 mode: a (signed) 32-bit integer
      {
        const int32_t rnd32 = int32_t(uint32_t(rand17() ^ (rand17() << 16)));
        Z.next_ravel_Int(rnd32);
        return;
      }

const int64_t rnd = rand17()
                  ^ (rand17() << 16)
                  ^ (rand17() << 32)
                  ^ (rand17() << 48);
   Z.next_ravel_Int(rnd);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::result_maxdepth(const cValue & B)
{
   if (B.get_rank() > 1)        RANK_ERROR;
   if (B.element_count() > 1)   LENGTH_ERROR;

   // result Z is the current max. depth
   //
Value_P Z = IntScalar(desired_maxdepth, LOC);

   if (B.element_count())   // set the desired maxdepth
      {
        const APL_Integer mxd = B.get_int_value(0);
        if (mxd < 0 || mxd > INT_MAX)   // was truncated to int (Bugs34 #30)
           {
             MORE_ERROR() << "bad max.depth";
             DOMAIN_ERROR;
           }
        desired_maxdepth = mxd;

        Log(LOG_Quad_RVAL)
           {
             CERR << "set desired_maxdepth to" << desired_maxdepth << endl;
           }
      }

   return Z;   // previous desired_maxdepth
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::result_ecount(const cValue & B)
{
   if (B.get_rank() > 1)        RANK_ERROR;
   if (B.element_count() > 1)   LENGTH_ERROR;

Value_P Z = IntScalar(desired_max_ecount, LOC);   // return previous value

   if (B.element_count())   // set the limit
      {
        const APL_Integer ec = B.get_int_value(0);
        if (ec < 0)
           {
             MORE_ERROR() << "5 ⎕RVAL B: B must be ≥ 0 (0 = unlimited)";
             DOMAIN_ERROR;
           }
        desired_max_ecount = ec;

        Log(LOG_Quad_RVAL)
           {
             CERR << "set desired_max_ecount to " << desired_max_ecount << endl;
           }
      }

   return Z;   // previous desired_max_ecount
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::result_purpose(const cValue & B)
{
   if (B.get_rank() > 1)        RANK_ERROR;
   if (B.element_count() > 1)   LENGTH_ERROR;

Value_P Z = IntScalar(desired_purpose, LOC);   // return previous value

   if (B.element_count())   // set the purpose
      {
        const APL_Integer purpose = B.get_int_value(0);
        if (purpose != 0 && purpose != 1)
           {
             MORE_ERROR() << "8 ⎕RVAL B: B must be 0 or 1";
             DOMAIN_ERROR;
           }
        desired_purpose = purpose;

        Log(LOG_Quad_RVAL)
           {
             CERR << "set desired_purpose to " << desired_purpose << endl;
           }
      }

   return Z;   // previous desired_purpose
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::result_rank(const cValue & B)
{
   // B is a single rank or a distribution of ranks 0, 1, ...
   if (B.get_rank() > 1)   RANK_ERROR;

   // result Z is the current rank
   //
Value_P Z(desired_ranks.size(), LOC);
   loop(z, desired_ranks.size())   Z->next_ravel_Int(desired_ranks[z]);
   Z->check_value(LOC);

   if (B.is_scalar())   // single rank (fixed or equal distribution)
      {
        ShapeItem rk = B.get_int_value(0);

        if ((rk < -MAX_RANK) || (rk > MAX_RANK))
           {
             MORE_ERROR() << "a scalar right argument B of 1 ⎕RVAL B should "
                             "be an integer from ¯" << MAX_RANK << " to "
                          << MAX_RANK;
             DOMAIN_ERROR;
           }

         desired_ranks.clear();
         desired_ranks.push_back(rk);
      }
   else if (B.element_count())   // distribution of ranks
      {
        if (B.element_count() > MAX_RANK + 1)
           {
             MORE_ERROR() << "a vector right argument B of 1 ⎕RVAL B "
                             "must have at most " << (MAX_RANK + 1)
                          << " items (one per rank 0.." << MAX_RANK << ")";
             LENGTH_ERROR;
           }

        vector<int>new_ranks;
        loop(b, B.element_count())
            {
              const APL_Integer rank_b_A = B.get_int_value(b);
              if (rank_b_A > 0x7FFFFFFF)   // Bugs35 #21: not truncated
                 {
                   MORE_ERROR() << "the weight " << rank_b_A << " in a vector right argument B of 1 ⎕RVAL B"
                                   " is too large (max. 2147483647)";
                   DOMAIN_ERROR;
                 }
              const int rank_b = rank_b_A;
              if (rank_b < 0)
                 {
                   MORE_ERROR() << "a vector right argument B of 1 ⎕RVAL B "
                              "should be a distribution (integers ≥ 0)";
                   DOMAIN_ERROR;
                 }
              new_ranks.push_back(rank_b);
            }

        desired_ranks = new_ranks;
      }

   Log(LOG_Quad_RVAL)
      {
        CERR << "set desired_ranks to";
        loop(j, desired_ranks.size())   CERR << " " << desired_ranks[j];
        CERR << endl;
      }

   return Z;   // previous rank
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::result_shape(const cValue & B)
{
   if (B.get_rank() > 1)   RANK_ERROR;

const ShapeItem len_B = B.element_count();
   if (len_B > MAX_RANK)   LENGTH_ERROR;   // to many shape items

   // result Z is the current shape limits
   //
Value_P Z(MAX_RANK, LOC);
   loop(r, MAX_RANK)
       Z->next_ravel_Int(desired_shape.get_shape_item(r));
   Z->check_value(LOC);

   if (len_B)   // len_B > 0: set the current shape
      {
        Shape new_shape;

        if (B.is_scalar())   // scalar-extend B
           {
              const APL_Integer len = B.get_int_value(0);
              loop(b, MAX_RANK)   new_shape.add_shape_item(len);
           }
        else                 // vector B: prepend 1s as needed
           {
             // fill leading dimensions with 1
             //
             loop(b, MAX_RANK - len_B)   new_shape.add_shape_item(1);

             // fill lower dimensions with B
              loop(b, len_B)
                  new_shape.add_shape_item(B.get_int_value(b));
           }

        // the shape items are used as int (see choose_integer()); a larger
        // one was silently truncated, e.g. 2⋆32 became 0 (Bugs34 #30)
        //
        loop(r, new_shape.get_rank())
           {
             const ShapeItem item = new_shape.get_shape_item(r);
             if (item > INT_MAX || item <= -INT_MAX)
                {
                  MORE_ERROR() << "2 ⎕RVAL B: bad shape item " << item;
                  DOMAIN_ERROR;
                }
           }

        desired_shape = new_shape;
      }

   Log(LOG_Quad_RVAL)
      {
        CERR << "set desired_shape to";
        loop(j, desired_shape.get_rank())
            {
              CERR << " " << desired_shape.get_shape_item(j);
            }
        CERR << endl;
      }

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::result_type(const cValue & B)
{
   // B is a distribution of cell types char (0), int (1), real (2),
   // complex (3), // or nested (4).

   if (!B.is_vector())
      {
        MORE_ERROR() << "the right argument B of 3 ⎕RVAL B "
        "should be a vector of up to 5 integers (the\nrelative "
        "probabilities for types CHAR INT REAL COMPLEX or NESTED respectively)";
          RANK_ERROR;
      }

const ShapeItem len_B = B.element_count();
   if (len_B > 5)
      {
        MORE_ERROR() << "the right argument B of 3 ⎕RVAL B "
        "should be a vector of up to 5 integers (the\nrelative "
        "probabilities for types CHAR INT REAL COMPLEX or NESTED respectively)";
          LENGTH_ERROR;
      }

   // result Z is the current types
   //
Value_P Z(desired_types.size(), LOC);
   loop(z, desired_types.size())   Z->next_ravel_Int(desired_types[z]);
   Z->check_value(LOC);

   if (len_B)   // len_B > 0: distribution of depths
      {
        vector<int>new_types;
        bool B_has_simple = false;
        loop(b, B.element_count())
            {
              const APL_Integer type_b_A = B.get_int_value(b);
              if (type_b_A > 0x7FFFFFFF)   // Bugs35 #21: not truncated
                 {
                   MORE_ERROR() << "the weight " << type_b_A << " in the right argument B of 3 ⎕RVAL B"
                                   " is too large (max. 2147483647)";
                   DOMAIN_ERROR;
                 }
              const int type_b = type_b_A;
              if (type_b < 0)
                 {
                   MORE_ERROR() << "the right argument B of 3 ⎕RVAL B "
                   "should be a distribution (vector of integers ≥ 0)";
                   DOMAIN_ERROR;
                 }
              if (type_b && b < 4)   B_has_simple = true;
              new_types.push_back(type_b);
            }

        // if all simple types had a probability of 0 then random_nested()
        // would loop forever, attempting to choose one. Do not allow that.
        //
        if (!B_has_simple)
           {
             MORE_ERROR() << "the right argument B of 3 ⎕RVAL B should contain "
                   "at least one simple type with a non-zero probability";
             DOMAIN_ERROR;
           }

        while (new_types.size() < 5)    new_types.push_back(0);   // make 5=⍴Z
        desired_types = new_types;

        Log(LOG_Quad_RVAL)
           {
             CERR << "set desired_types to";
             const char * types[] = { "char", "int", "float",
                                      "complex", "nested" };
             loop(j, desired_types.size())
                 {
                   CERR << " " << types[j] << "=" << desired_types[j];
                 }
             CERR << endl;
           }
      }

   return Z;
}
//════════════════════════════════════════════════════════════════════════════


// prim_rows[][5]: arity, A_stim, B_stim, X_stim, Z_constraint
// rval.def stores (arity, B_stim, A_stim, X_stim, constraint), so
// the macro swaps A_stim and B_stim to put A before B in the matrix.
//
// B_stim/A_stim/X_stim are plain quoted string literals (not stringified
// macro args) because unquoted APL glyphs -- even the lone ¯ that every
// row here relies on -- are tokenized by the preprocessor before
// rval.def's #include ever reaches the macro body, and clang rejects
// ¯ there outright (gcc happens to tolerate it, but no other glyph on
// either compiler, see rval.def's own header comment); quoting removes
// the preprocessor from that path entirely (Paul Rockwell, macOS/clang).
#define prim_def(arity, b_stim, a_stim, x_stim, constraint)    { arity, a_stim, b_stim, x_stim, constraint },
static const char * const prim_rows[][5] =
{
#include "rval.def"
};
#undef prim_def
static const int prim_row_count = sizeof(prim_rows) / sizeof(*prim_rows);

//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RVAL::prim_table_value(const cValue & B)
{
const Shape sh(prim_row_count, 5);
Value_P Z(sh, LOC);
   loop(r, prim_row_count)
      loop(c, 5)
         {
           const char * src = prim_rows[r][c];
           const UCS_string ucs(*src ? UTF8_string(src) : UTF8_string("⍬"));
           Value_P cell(ucs, LOC);
           Z->next_ravel_Pointer(cell.get());
         }
   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
// EOF
