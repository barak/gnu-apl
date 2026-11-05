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

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <complex>

#include "Common.hh"
#include "ComplexCell.hh"
#include "Error.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "UTF8_string.hh"
#include "Workspace.hh"

#include "Cell.icc"
#include "Value.hh"

//════════════════════════════════════════════════════════════════════════════
bool
FloatCell::greater(const Cell & other) const
{
const APL_Float this_val  = get_real_value();

   switch(other.get_cell_type())
      {
        case CT_INT:
             {
               const APL_Float other_val(other.get_int_value());
               if (this_val == other_val)   return this > &other;
               return this_val > other_val;
             }

        case CT_FLOAT:
             {
               const APL_Float other_val = other.get_real_value();
               if (this_val == other_val)   return this > &other;
               return this_val > other_val;
             }

        case CT_CHAR:    return true;
        case CT_COMPLEX: break;
        case CT_POINTER: return false;
        case CT_CELLREF: DOMAIN_ERROR;
        default:         Assert(0 && "Bad celltype");
      }

const Comp_result comp = compare(other);
   if (comp == COMP_EQ)   return this > &other;
   return (comp == COMP_GT);
}
//────────────────────────────────────────────────────────────────────────────
bool
FloatCell::equal(const Cell & A, double qct) const
{
   if (!A.is_numeric())       return false;
   if (A.is_complex_cell())   return A.equal(*this, qct);

   if (qct == 0.0 && A.is_integer_cell())
      {
        // Bugs31 #8 (Blake McBride): A.get_real_value() below converts
        // A's exact int64 through double first, losing precision beyond
        // 2⋆53 and silently equating values that are not equal (or vice
        // versa) -- same class of bug as IntCell::equal()'s own
        // int64-vs-int64 fast path fixed for Bugs30 #3. Compare exactly
        // instead, the same way Cell::compare_int_float() already does
        // for grading and for FloatCell::compare().
        //
        return Cell::compare_int_float(A.get_int_value(), dfval()) == 0;
      }

   return tolerantly_equal(A.get_real_value(), get_real_value(), qct);
}
//────────────────────────────────────────────────────────────────────────────
// dyadic built-in functions...
//
// where possible a function with non-real A is delegated to the corresponding
// member function of A. For numeric cells that is the ComplexCell function
// and otherwise the default function (that returns E_DOMAIN_ERROR.
//
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_add(Cell * Z, const Cell * A) const
{
   if (A->is_real_cell())
      {
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (const APL_Integer denom_B = get_denominator())
   if (const APL_Integer denom_A = A->get_denominator())
      {
        // both A and B are rational
        //
        if (Cell::prod_overflow(denom_A, denom_B))   goto big;

        // compute common denominator...
        const APL_Integer gcd_AB   = gcd(denom_A, denom_B);
        const APL_Integer mult_A  = denom_A / gcd_AB;
        const APL_Integer mult_B  = denom_B / gcd_AB;
        const APL_Integer denom_AB = denom_A * mult_B;
        if (Cell::prod_overflow(denom_AB, mult_B))                goto big;

        // compute numerators...
        const APL_Integer numer_A = A->get_numerator();
        if (Cell::prod_overflow(numer_A, mult_B))                goto big;
        const APL_Integer numer_A1 = numer_A * mult_B;
        const APL_Integer numer_B = get_numerator();
        if (Cell::prod_overflow(numer_B, mult_A))                goto big;
        const APL_Integer numer_B1 = numer_B * mult_A;

        const APL_Integer sum_AB = numer_A1 + numer_B1;
        if (Cell::sum_overflow(sum_AB, numer_A1, numer_B1))      goto big;
        const APL_Integer sum_gcd = gcd(sum_AB, denom_AB);
        if (sum_gcd == denom_AB)   return IntCell::zI(Z, sum_AB / denom_AB);
        if (sum_gcd == 1)   return FloatCell::zR(Z, sum_AB, denom_AB);
        return FloatCell::zR(Z, sum_AB/sum_gcd, denom_AB/sum_gcd);
      }
      big:

#endif
        return FloatCell::bif_add_ff(Z, A->get_real_value(), dfval());
      }

   if (A->is_complex_cell())
      return ComplexCell::bif_add_cc(Z, A->get_complex_value(),
                                       APL_Complex(dfval(), 0));
   return E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_ceiling(Cell * Z) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (const APL_Integer denom = get_denominator())
      {
        const APL_Integer numer = get_numerator();
        APL_Integer quotient = numer / denom;
        if (numer > (quotient * denom))   ++quotient;
        return IntCell::zI(Z, quotient);
      }
#endif
   return FloatCell::bif_ceiling_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_conjugate(Cell * Z) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   // convert quotients to double
#endif
   return FloatCell::bif_conjugate_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_direction(Cell * Z) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (const APL_Integer denom = get_denominator())
      {
        if (get_numerator() > 0)   return IntCell::zI(Z,  1);
        if (get_numerator() < 0)   return IntCell::zI(Z, -1);
        return FloatCell::zF(Z, 0);
      }
#endif
   return FloatCell::bif_direction_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_divide(Cell * Z, const Cell * A) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (const APL_Integer B_denom = get_denominator())  // B is rational
      {
        const APL_Integer B_numer = get_numerator();
        if (B_numer == 0)   // A ÷ 0
           {
             if (A->is_near_zero())   return IntCell::z1(Z);
             return E_DOMAIN_ERROR;
           }
        const FloatCell inv_B(B_denom, B_numer);
        return inv_B.bif_multiply(Z, A);
      }
#endif

   if (!A->is_numeric())   return E_DOMAIN_ERROR;

   if (A->is_complex_cell())
      return ComplexCell::bif_divide_cc(Z, A->get_complex_value(),
                                          APL_Complex(dfval(), 0));
   return FloatCell::bif_divide_ff(Z, A->get_real_value(), dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_exponential(Cell * Z) const
{
   return FloatCell::bif_exponential_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_factorial(Cell * Z) const
{
   return FloatCell::bif_factorial_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_floor(Cell * Z) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (const APL_Integer denom = get_denominator())
      {
        const APL_Integer numer = get_numerator();
        APL_Integer quotient = numer / denom;
        if (numer < (quotient * denom))   --quotient;
        return IntCell::zI(Z, quotient);
      }
#endif
   return FloatCell::bif_floor_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_magnitude(Cell * Z) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (const APL_Integer denom = get_denominator())
      {
        const APL_Integer numer = get_numerator();
        return FloatCell::zR(Z, numer < 0 ? -numer : numer, denom);
      }
#endif
   return FloatCell::bif_magnitude_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_multiply(Cell * Z, const Cell * A) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (APL_Integer denom_B = get_denominator())
   if (APL_Integer denom_A = A->get_denominator())
      {
        APL_Integer numer_A = A->get_numerator();
        APL_Integer numer_B = get_numerator();
        const APL_Integer gcd_A_B = gcd(numer_A, denom_B);
         if (gcd_A_B > 1)   { numer_A /= gcd_A_B;   denom_B /= gcd_A_B; }

        const APL_Integer gcd_B_A = gcd(numer_B, denom_A);
         if (gcd_B_A > 1)   { numer_B /= gcd_B_A;   denom_A /= gcd_B_A; }

        if (Cell::prod_overflow(numer_A, numer_B))   goto big;
        if (Cell::prod_overflow(denom_A, denom_B))   goto big;

        const APL_Integer numer = numer_A * numer_B;
        const APL_Integer denom = denom_A * denom_B;
        const APL_Integer prod_gcd = gcd(numer, denom);
        if (prod_gcd == denom)   return IntCell::zI(Z, numer / denom);
        if (prod_gcd == 1)       return FloatCell::zR(Z, numer, denom);
        return FloatCell::zR(Z, numer/prod_gcd, denom/prod_gcd);
      }
      big:

#endif

   if (!A->is_numeric())   return E_DOMAIN_ERROR;

   if (A->is_complex_cell())
      return ComplexCell::bif_multiply_cc(Z, A->get_complex_value(),
                                            APL_Complex(dfval(), 0));
   return FloatCell::bif_multiply_ff(Z, A->get_real_value(), dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_power(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;
   if (A->is_complex_cell())
      return ComplexCell::bif_power_cc(Z, A->get_complex_value(),
                                         APL_Complex(dfval(), 0));
   return FloatCell::bif_power_ff(Z, A->get_real_value(), dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_nat_log(Cell * Z) const
{
   return FloatCell::bif_nat_log_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_negative(Cell * Z) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (const APL_Integer denom = get_denominator())
      return FloatCell::zR(Z, -get_numerator(), denom);
#endif
   return FloatCell::bif_negative_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_pi_times(Cell * Z) const
{
   return FloatCell::bif_pi_times_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_pi_times_inverse(Cell * Z) const
{
   return FloatCell::bif_pi_times_inverse_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_reciprocal(Cell * Z) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (const APL_Integer denom = get_denominator())
      {
        if (uint64_t(denom) < 0x8000000000000000ULL)
           {
             const APL_Integer numer = get_numerator();
             if (numer == 1)    return IntCell::zI(Z,  denom);
             if (numer == -1)   return IntCell::zI(Z, -denom);
             if (numer < 0)     return FloatCell::zR(Z, -denom, -numer);
             else               return FloatCell::zR(Z, denom, numer);
           }
      }
#endif
   return FloatCell::bif_reciprocal_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_roll(Cell * Z) const
{
   return FloatCell::bif_roll_f(Z, dfval());
}
//════════════════════════════════════════════════════════════════════════════
Comp_result
FloatCell::compare(const Cell & other) const
{
   if (other.is_integer_cell())   // integer
      {
        const double qct = Workspace::get_CT();
        if (qct == 0.0)
           {
             // Bugs31 #8 (Blake McBride): equal()'s get_real_value()
             // round-trip and the plain dfval()<=... comparison below
             // both convert other's exact int64 through double first,
             // losing precision beyond 2⋆53 and silently misordering or
             // misequating -- exactly the same bug already fixed for
             // grading (⍋/⍒, Bugs30 #4) via Cell::compare_int_float().
             //
             const int cr = Cell::compare_int_float(other.get_int_value(),
                                                     dfval());
             if (cr < 0)   return COMP_GT;   // other < this
             if (cr > 0)   return COMP_LT;   // other > this
             return COMP_EQ;
           }
        if (equal(other, qct))   return COMP_EQ;
        return (dfval() <= other.get_int_value())  ? COMP_LT : COMP_GT;
      }

   if (other.is_float_cell())
      {
        const double qct = Workspace::get_CT();
        if (equal(other, qct))   return COMP_EQ;
        return (dfval() <= other.get_real_value()) ? COMP_LT : COMP_GT;
      }

   if (other.is_complex_cell())   return Comp_result(-other.compare(*this));

   if (other.is_character_cell())   return COMP_GT;   // numeric > char
   if (other.is_pointer_cell())     return COMP_LT;   // numeric < nested
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_maximum(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;
   if (A->is_complex_cell())
      return ComplexCell::bif_maximum_cc(Z, A->get_complex_value(),
                                            APL_Complex(dfval(), 0));

   if (A->is_integer_cell() && Workspace::get_CT() == 0.0)
      {
        // Bugs31 #8 (Blake McBride): A->get_real_value() below converts
        // A's exact int64 through double first, losing precision beyond
        // 2⋆53 -- same bug as FloatCell::compare(). Compare exactly
        // and, if A (the IntCell) wins, return its ORIGINAL exact value
        // rather than a lossily re-converted double.
        //
        if (Cell::compare_int_float(A->get_int_value(), dfval()) >= 0)
           return IntCell::zI(Z, A->get_int_value());
        return FloatCell::zF(Z, dfval());
      }
   return FloatCell::bif_maximum_ff(Z, A->get_real_value(), dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_minimum(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;
   if (A->is_complex_cell())
      return ComplexCell::bif_minimum_cc(Z, A->get_complex_value(),
                                            APL_Complex(dfval(), 0));

   if (A->is_integer_cell() && Workspace::get_CT() == 0.0)
      {
        // see FloatCell::bif_maximum() above (Bugs31 #8)
        //
        if (Cell::compare_int_float(A->get_int_value(), dfval()) <= 0)
           return IntCell::zI(Z, A->get_int_value());
        return FloatCell::zF(Z, dfval());
      }
   return FloatCell::bif_minimum_ff(Z, A->get_real_value(), dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_residue(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;
   if (A->get_imag_value() != 0.0)
      return ComplexCell::bif_residue_cc(Z, A->get_complex_value(),
                                            APL_Complex(dfval(), 0));

   // an IntCell A and an integral B: exact int64 residue, mirroring
   // IntCell::bif_residue() for an integral FloatCell A (Bugs27 #25).
   // Converting A to double rounded e.g. 9007199254740993 to
   // 9007199254740992 (Blake McBride, Bugs32 #15).
   //
const APL_Float b = dfval();
   if (A->is_integer_cell() && Cell::is_near_int64_t(b) && b == nearbyint(b))
      return IntCell::bif_residue_ii(Z, A->get_int_value(), APL_Integer(b));

   return FloatCell::bif_residue_ff(Z, A->get_real_value(), dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_subtract(Cell * Z, const Cell * A) const
{
   if (A->is_complex_cell())
      return ComplexCell::bif_subtract_cc(Z, A->get_complex_value(),
                                             APL_Complex(dfval(), 0));
   if (!A->is_real_cell())   return E_DOMAIN_ERROR;
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (const APL_Integer denom_B = get_denominator())
   if (const APL_Integer denom_A = A->get_denominator())
      {
        if (Cell::prod_overflow(denom_A, denom_B))   goto big;
        const APL_Integer gcd_AB   = gcd(denom_A, denom_B);
        const APL_Integer mult_A  = denom_A / gcd_AB;
        const APL_Integer mult_B  = denom_B / gcd_AB;
        const APL_Integer denom_AB = denom_A * mult_B;
        const APL_Integer numer_A = A->get_numerator();
        if (Cell::prod_overflow(numer_A, mult_B))                goto big;
        const APL_Integer numer_A1 = numer_A * mult_B;
        const APL_Integer numer_B = get_numerator();
        if (Cell::prod_overflow(numer_B, mult_A))                goto big;
        const APL_Integer numer_B1 = numer_B * mult_A;
        const APL_Integer diff_AB = numer_A1 - numer_B1;
        if (Cell::diff_overflow(diff_AB, numer_A1, numer_B1))    goto big;
        const APL_Integer diff_gcd = gcd(diff_AB, denom_AB);
        if (diff_gcd == denom_AB)   return IntCell::zI(Z, diff_AB / denom_AB);
        if (diff_gcd == 1)   return FloatCell::zR(Z, diff_AB, denom_AB);
        return FloatCell::zR(Z, diff_AB/diff_gcd, denom_AB/diff_gcd);
      }
      big:
#endif
   return FloatCell::bif_subtract_ff(Z, A->get_real_value(), dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_add_inverse(Cell * Z, const Cell * A) const
{
   return A->bif_subtract(Z, this);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_multiply_inverse(Cell * Z, const Cell * A) const
{
   return A->bif_divide(Z, this);
}
//────────────────────────────────────────────────────────────────────────────
bool
FloatCell::get_near_bool()  const
{
   if (dfval() > INTEGER_TOLERANCE)   // maybe 1
      {
        if (dfval() > (1.0 + INTEGER_TOLERANCE))   DOMAIN_ERROR;
        if (dfval() < (1.0 - INTEGER_TOLERANCE))   DOMAIN_ERROR;
        return true;
      }

   // maybe 0. We already know that dfval() ≤ qct
   //
   if (dfval() < -INTEGER_TOLERANCE)   DOMAIN_ERROR;
   return false;
}
//────────────────────────────────────────────────────────────────────────────
// monadic built-in functions...
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_near_int64_t(Cell * Z) const
{
   return FloatCell::bif_near_int64_t_f(Z, dfval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_within_quad_CT(Cell * Z) const
{
   return FloatCell::bif_within_quad_CT_f(Z, dfval());
}
/* ╔═════════════════════════════════════════════════════════════════════════╗
   ║ throw/nothrow boundary. Functions above MUST NOT (directly or           ║
   ║ indirectly) throw while funcions below MAY throw.                       ║
   ╚═════════════════════════════════════════════════════════════════════════╝
 */
PrintBuffer
FloatCell::character_representation(const PrintContext & pctx) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   if (const APL_Integer denom = get_denominator())
      {
        if (Workspace::get_v_Quad_PS().get_print_quotients())   // show A÷B
           {
             ColInfo info;
             info.flags |= CT_FLOAT;

             UCS_string ucs;
             APL_Integer numer = get_numerator();
             if (numer < 0)
                {
                  ucs << UNI_OVERBAR;
                  numer = -numer;
                }
             ucs << UCS_string::from_uint(numer);
             info.int_len = ucs.size();

             ucs << UNI_DIVIDE;

             ucs << UCS_string::from_uint(denom);
             info.denom_len = ucs.size() - info.int_len;
             info.real_len = ucs.size();
             return PrintBuffer(ucs, info);
           }
      }
#endif

bool scaled = pctx.get_scaled();   // may be changed by print function
UCS_string ucs = UCS_string(dfval(), scaled, pctx);

ColInfo info;
   info.flags |= CT_FLOAT;
   if (scaled)   info.flags |= real_has_E;

   // assume integer.
   //
int int_fract = ucs.size();
   info.real_len = ucs.size();
   info.int_len = ucs.size();
   loop(u, ucs.size())
      {
        if (ucs[u] == UNI_FULLSTOP)
           {
             info.int_len = u;
             if (!pctx.get_scaled())   break;
             continue;
           }

        if (ucs[u] == UNI_E)
           {
             if (info.int_len > u)   info.int_len = u;
             int_fract = u;
             break;
           }
      }

   info.fract_len = int_fract - info.int_len;

   return PrintBuffer(ucs, info);
}
//────────────────────────────────────────────────────────────────────────────
bool
FloatCell::is_big(APL_Float val, int quad_pp)
{
static const APL_Float big[MAX_Quad_PP + 1] =
{
                  1ULL, // not used since MIN_Quad_PP == 1
                 10ULL,
                100ULL,
               1000ULL,
              10000ULL,
             100000ULL,
            1000000ULL,
           10000000ULL,
          100000000ULL,
         1000000000ULL,
        10000000000ULL,
       100000000000ULL,
      1000000000000ULL,
     10000000000000ULL,
    100000000000000ULL,
   1000000000000000ULL,
  10000000000000000ULL,
 100000000000000000ULL,
};

   return val >= big[quad_pp] || val <= -big[quad_pp];
}
//────────────────────────────────────────────────────────────────────────────
bool
FloatCell::need_scaling(APL_Float val, int quad_pp)
{
   // A number is printed in scaled format if (see lrm pp. 11-13) either:
   //
   // (1) its integer part is longer that quad-PP, or

   // (2a) is non-zero, and
   // (2b) its integer part is 0, and
   // (2c) its fractional part starts with at least 5 zeroes.
   //
   if (val < 0.0)   val = - val;   // simplify comparisons

   // the decision is made on the number as displayed, i.e. rounded to
   // quad_pp significant digits (lrm p. 12, verified with IBM APL2): e.g.
   // with ⎕PP←5, 9.99999E¯6 is displayed as 0.00001 (not 1E¯5), and
   // 99999.6 as 1E5 (not as the 6 digits 100000).
   //
   if (val != 0.0 && isfinite(val))
      {
        char cc[40];
        snprintf(cc, sizeof(cc), "%.*e", quad_pp - 1, val);
        val = strtod(cc, 0);
      }

   if (is_big(val, quad_pp))        return true;    // case 1.

   if (val == 0.0)                  return false;   // not 2a.

   // (2c): at least 5 zeroes after the decimal point means val < 1E¯5,
   // e.g. 0.00000356 (verified with IBM APL2: 3.56E¯6 is scaled while 6E¯5
   // is not). The previous test val < 0.000001 required 6 zeroes.
   //
   if (val < 0.00001)               return true;    // case 2

   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
FloatCell::map_FC(UCS_string & ucs)
{
   loop(u, ucs.size())
      {
        switch(ucs[u])
           {
             case UNI_FULLSTOP: ucs[u] = Workspace::get_FC(0);   break;
             case UNI_OVERBAR:        ucs[u] = Workspace::get_FC(5);   break;
             default:                 break;
           }
      }
}
//════════════════════════════════════════════════════════════════════════════
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_add_ff(Cell * Z, APL_Float a, APL_Float b)
{
const APL_Float z = a + b;
   if (!isfinite(z))   return E_DOMAIN_ERROR;
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_subtract_ff(Cell * Z, APL_Float a, APL_Float b)
{
const APL_Float z = a - b;
   if (!isfinite(z))   return E_DOMAIN_ERROR;
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_multiply_ff(Cell * Z, APL_Float a, APL_Float b)
{
const APL_Float z = a * b;
   if (!isfinite(z))   return E_DOMAIN_ERROR;
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_divide_ff(Cell * Z, APL_Float a, APL_Float b)
{
   if (b == 0.0)
      {
        if (a != 0.0)   return E_DOMAIN_ERROR;
        return IntCell::z1(Z);
      }
const APL_Float real = a / b;
   return isfinite(real) ? FloatCell::zF(Z, real) : E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_power_fi(Cell * Z, APL_Float a, APL_Integer b)
{
   // a (float base) to the b (integer) power — preserves parity for negative a
   //
const bool invert_Z = b < 0;
   if (invert_Z)   b = -b;

   if (b == 0)   return IntCell::z1(Z);

   if (b == 1)
      {
        if (invert_Z)
           {
             if (a == 0.0)   return E_DOMAIN_ERROR;
             const APL_Float z = 1.0 / a;
             if (!isfinite(z))   return E_DOMAIN_ERROR;
             return FloatCell::zF(Z, z);
           }
        return FloatCell::zF(Z, a);
      }

const bool negate_Z = (a < 0.0) && (b & 1);
   if (a < 0.0)   a = -a;

   // Pass the ORIGINAL (negative, when invert_Z) exponent to pow()
   // directly rather than computing pow(a,b) and then inverting:
   // pow(1.5, 2000.0) alone already overflows to inf (b was negated to
   // a positive 2000 above), which used to fail the isfinite() check
   // right here and reject with DOMAIN ERROR -- even though the true
   // result (1.5⋆¯2000 = 1/1.5⋆2000) is representable (≈0, but finite).
   // See Bugs27 #23.
   //
APL_Float z = pow(a, invert_Z ? -APL_Float(b) : APL_Float(b));
   if (negate_Z)   z = -z;
   // z == 0.0 here is a genuine (if imprecise) representable result --
   // A⋆¯N underflowing all the way to 0 for a large N, e.g. 1.5⋆¯2000
   // -- not an error; only non-finite (inf/nan) is. See Bugs27 #23.
   if (!isfinite(z))   return E_DOMAIN_ERROR;
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_power_ff(Cell * Z, APL_Float a, APL_Float b)
{
   if (a == 0.0)
      {
        if (b == 0.0)   return IntCell::z1(Z);
        if (b  > 0.0)   return IntCell::z0(Z);
        return E_DOMAIN_ERROR;
      }
   if (a == 1.0)   return IntCell::z1(Z);
   if (a >= 0.0)
      {
        const APL_Float z = pow(a, b);
        return isfinite(z) ? FloatCell::zF(Z, z) : E_DOMAIN_ERROR;
      }
   // a < 0, b a near-integer FloatCell (e.g. 3.0, or any computed float
   // that happens to be integral, like 1E14): go through the exact
   // integer-exponent path (parity trick, repeated squaring) instead of
   // complex_power()'s exp(b×ln(a)) below. exp(b×ln(a)) needs b×πi's
   // imaginary part to land back on a real axis exactly for an integral
   // b, but the absolute rounding error in computing b×π is
   // proportional to b -- for |b| ≳ 1E14 that error already exceeds 1
   // radian, so the result is noise (e.g. ¯1*1E14 came out
   // 0.9999...J¯0.0113... instead of the exact 1), not mere imprecision.
   // See Bugs27 #24.
   //
   // is_near_int64_t(), not is_near_int(): the latter also returns true
   // for values beyond int64 range (nothing left to round off at that
   // magnitude), which near_int() below would then reject with its own
   // DOMAIN_ERROR -- is_near_int64_t() is the one that actually agrees
   // with what near_int() can convert.
   //
   // b == nearbyint(b), not just is_near_int64_t(b): the latter allows b
   // to be within INTEGER_TOLERANCE of an integer, not only exactly one.
   // A b that is deliberately, even if very slightly, non-integral
   // should still go through the genuinely-complex path below rather
   // than being silently snapped to the nearest integer exponent (see
   // the matching fix/comment in IntCell::bif_residue for the same
   // class of bug, caught via Quad_CT.tc regressing).
   //
   if (Cell::is_near_int64_t(b) && b == nearbyint(b))
      return FloatCell::bif_power_fi(Z, a, APL_Integer(b));

   // a < 0, b an exact integer too large to fit int64_t (so
   // bif_power_fi() above, which needs an APL_Integer b, isn't
   // reachable): every double of magnitude ≥ 2⋆53 is necessarily an
   // EVEN integer -- a double's mantissa has only 52 bits, so once the
   // exponent pushes the magnitude past 2⋆53 the ulp itself is already
   // ≥ 2, and no odd value is representable any more. a⋆b is therefore
   // real and positive, exactly pow(|a|,b), with no parity trick or
   // complex_power() needed (whose exp(b×ln a) is pure rounding noise
   // at this magnitude of b -- see Bugs27 #24). See Bugs28 #39.
   //
   if (b == nearbyint(b))
      {
        const APL_Float z = pow(-a, b);
        return isfinite(z) ? FloatCell::zF(Z, z) : E_DOMAIN_ERROR;
      }

   // a < 0, b not an integer: complex result
   //
const APL_Complex z = complex_power(APL_Complex(a, 0.0), b);
   if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
   if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_logarithm_ff(Cell * Z, APL_Float a, APL_Float b)
{
   // a = base (A), b = argument (B = this)
   //
   // Base validity (a == 0 or a == 1) must be checked before the b==a
   // short-circuit below -- otherwise 0⍟0 and 1⍟1 both incorrectly
   // return 1 despite an illegal base (1⍟5, with the same illegal base
   // 1, does raise DOMAIN ERROR -- internally inconsistent). a == 0 also
   // needs its own explicit guard: log(0.0) is -inf, so log(b)/-inf is
   // -0.0, which passes the isfinite() check below and returns 0
   // instead of erroring (Blake McBride, Bugs26 #2).
   if (a == 0.0)   return E_DOMAIN_ERROR;

   // ⎕CT-tolerant equality to 1, not a fixed absolute INTEGER_TOLERANCE
   // (Bugs28 #87): the fixed threshold rejected any base within 1E¯10
   // of 1 even when ⎕CT itself (the default 1E¯13) would not consider
   // that base tolerantly equal to 1 at all -- e.g. (1+1E¯11)⍟5 was
   // rejected even though (1+1E¯11)=1 is 0.
   //
   if (Cell::tolerantly_equal(a, 1.0, Workspace::get_CT()))
      return E_DOMAIN_ERROR;
   if (b == a)    return IntCell::z1(Z);
   if (b == 0.0)  return E_DOMAIN_ERROR;

   if (a >= 0.0 && b >= 0.0)
      {
        const APL_Float z = log(b) / log(a);
        if (!isfinite(z))   return E_DOMAIN_ERROR;
        return FloatCell::zF(Z, z);
      }

   // complex result
   //
const APL_Complex z = log(APL_Complex(b, 0.0)) / log(APL_Complex(a, 0.0));
   if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
   if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_maximum_ff(Cell * Z, APL_Float a, APL_Float b)
{
   // FloatCell::zF() (unrounded), not ComplexCell::zV(): zV rounds a
   // value within ⎕CT-tolerance of an integer to an IntCell instead of
   // storing the selected operand verbatim -- dyadic max/min must return
   // one of its two arguments exactly. 2.00000000005⌈1 gave 2 instead of
   // 2.00000000005.
   return FloatCell::zF(Z, a >= b ? a : b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_minimum_ff(Cell * Z, APL_Float a, APL_Float b)
{
   return FloatCell::zF(Z, a <= b ? a : b);
}
// ISO residue: R ← P - (×P) ⌊ |Q × ⌊ |P ÷ Q   (cf. FloatCell.cc)
static double
nc_p_modulo_q(double P, double Q)
{
const APL_Float quotient = P / Q;
   if (!isfinite(quotient))   return 0.0;

   if (!isfinite(Q / P))
      return ((P < 0) == (Q < 0)) ? P : 0.0;

   {
     const double qct = Workspace::get_CT();
     // (Blake McBride, Bugs22 #1 -- see Cell::integral_within()'s own
     // fix for the actual near-zero-quotient bug this guards against.)
     if ((qct != 0) && Cell::integral_within(quotient, qct))   return 0.0;
   }

const APL_Float null(0.0);
const APL_Float abs_quotient = quotient < null ? -quotient : quotient;
   if (abs_quotient > 4.5E15)   return 0.0;

   if (abs_quotient < 1.0)
      return (P < null) == (Q < null) ? P : Q + P;

const APL_Float floor_quotient = floor(abs_quotient);
const APL_Float prod           = Q * floor_quotient;
const APL_Float abs_prod       = prod < null ? -prod : prod;
const APL_Float prod2          = P < 0 ? -abs_prod : abs_prod;
   return P - prod2;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_residue_ff(Cell * Z, APL_Float a, APL_Float b)
{
   if (a == 0.0)   return FloatCell::zF(Z, b);
   if (b == 0.0)   return IntCell::z0(Z);

   // Bugs28 #100(q): a and b may be EXACT integers even though they
   // arrived here as doubles (e.g. 1E17, a float literal, is exactly
   // representable and exactly equal to the integer
   // 100000000000000000 -- 7|1E17 and 7|100000000000000000 must agree)
   // -- nc_p_modulo_q() below intentionally gives up (returns 0)
   // whenever |b÷a| exceeds 4.5E15, since its own floor()/multiply
   // steps lose precision beyond that magnitude for a genuinely
   // fractional quotient. That guard has no bearing when both operands
   // are exact integers: the residue is then exact integer arithmetic
   // with no precision left to lose, so compute it exactly instead of
   // giving up (same rationale as the analogous exact-integer fast
   // paths already added for GCD/factorial/power elsewhere).
   //
   if (a == floor(a) && b == floor(b) &&
       fabs(a) < BIG_INT64_F && fabs(b) < BIG_INT64_F)
      return IntCell::bif_residue_ii(Z, APL_Integer(a), APL_Integer(b));

   // the same at ⎕CT←0 for integral a and b beyond the int64 range
   // (every double ≥ 2⋆53 is an integer): fmod() is exact in IEEE 754,
   // so the residue is too. nc_p_modulo_q() below gave up and returned 0,
   // e.g. 3|1E19 was 0 instead of 1 (Bugs34 #11). With ⎕CT≠0 the
   // tolerant residue stays (e.g. 1E100|1E200 is 0).
   //
   if (Workspace::get_CT() == 0.0 && a == floor(a) && b == floor(b))
      {
        APL_Float r = fmod(b, a);   // exact; has the sign of b
        if (r != 0.0 && ((r < 0.0) != (a < 0.0)))
           {
             // NOT exact: rounds to a if |r| < ulp(a)÷2. The volatile
             // forces the rounding to double before the comparison below;
             // with x87 arithmetic (32-bit i386) the sum would otherwise
             // remain in an 80-bit register where it still differs from a.
             volatile APL_Float sum = r + a;
             r = sum;
             // a is no residue modulo a (Bugs35 #11, e.g. 1E19|¯1). The
             // exact residue is not representable; return the closest
             // double strictly between 0 and a.
             if (r == a)   r = nextafter(a, 0.0);
           }
        if (r == 0.0)   return IntCell::z0(Z);
        if (fabs(r) < BIG_INT64_F)   return IntCell::zI(Z, APL_Integer(r));
        return FloatCell::zF(Z, r);
      }

const APL_Float null(0.0);
const APL_Float z = nc_p_modulo_q(b, a);
Assert(isfinite(z));

APL_Float r2;
   if      (z < null && a < null)   r2 = z;
   else if (z > null && a > null)   r2 = z;
   else                             r2 = z + a;

Assert(isfinite(r2));

   if (r2 == null)   return IntCell::z0(Z);
   if (r2 == a)      return IntCell::z0(Z);
   return FloatCell::zF(Z, r2);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_ceiling_f(Cell * Z, APL_Float b)
{
   if (b >= LARGE_INT)   return FloatCell::zF(Z, b);
   if (b <= SMALL_INT)   return FloatCell::zF(Z, b);

APL_Integer bi = b;
   while (bi < b)         ++bi;
   while ((bi - 1) > b)   --bi;
   if (bi == b)   return IntCell::zI(Z, bi);

   // bi is the candidate ceiling (smallest integer >= b). If b is
   // tolerantly equal to bi-1 (the floor), APL2's tolerant ceiling
   // snaps down to it -- mirroring the RELATIVE tolerance `=` uses
   // (ISO 13751 p.19, apl.texi's "equal to integer I within ⎕CT" is
   // |(I-R)÷I| < ⎕CT), not the ABSOLUTE distance check this replaced
   // (D >= 1-⎕CT for D = bi-b), which disagreed with `=` on the same
   // value once b's magnitude made an absolute ⎕CT too tight. See
   // Bugs27 #41.
   //
   // Exception: when bi-1 is 0, a relative tolerance can never match
   // (dist(b,0) == mag(b), so mag(b) < ⎕CT×mag(b) only for ⎕CT>1) --
   // same "comparison against zero is exact" rule as tolerantly_equal()
   // and integral_within(), but ⌊/⌈ still need to snap a value whose
   // FRACTIONAL part is within ⎕CT of a whole unit even when that
   // neighbouring integer is 0 (testcases/ZZZ0_Standard_08x.tc, LRM
   // p.82: ⌊¯0.5E¯10 is 0, not ¯1, at ⎕CT←1E¯10) -- so this one case
   // keeps the original absolute check on the fractional distance.
   //
const APL_Integer floor_candidate = bi - 1;
   if (floor_candidate == 0)
      {
        const APL_Float D = bi - b;
        if (D >= (1.0 - Workspace::get_CT()))   --bi;
      }
   else if (Cell::tolerantly_equal(b, APL_Float(floor_candidate),
                                    Workspace::get_CT()))
      {
        --bi;
      }
   return IntCell::zI(Z, bi);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_floor_f(Cell * Z, APL_Float b)
{
   if (b >= LARGE_INT)   return FloatCell::zF(Z, b);
   if (b <= SMALL_INT)   return FloatCell::zF(Z, b);

APL_Integer bi = b;
   while (bi > b)         --bi;
   while ((bi + 1) < b)   ++bi;
   if (bi == b)   return IntCell::zI(Z, bi);

   // see bif_ceiling_f() above: relative tolerance, mirroring `=`, with
   // the same zero-candidate exception. See Bugs27 #41.
   //
const APL_Integer ceil_candidate = bi + 1;
   if (ceil_candidate == 0)
      {
        const APL_Float D = b - bi;
        if (D >= (1.0 - Workspace::get_CT()))   ++bi;
      }
   else if (Cell::tolerantly_equal(b, APL_Float(ceil_candidate),
                                    Workspace::get_CT()))
      {
        ++bi;
      }
   return IntCell::zI(Z, bi);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_conjugate_f(Cell * Z, APL_Float b)
{
   return FloatCell::zF(Z, b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_direction_f(Cell * Z, APL_Float b)
{
   if (b > 0.0)   return IntCell::z1(Z);
   if (b < 0.0)   return IntCell::z_1(Z);
   return IntCell::z0(Z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_exponential_f(Cell * Z, APL_Float b)
{
const APL_Float z = exp(b);
   // sibling bif_factorial_f() below already does this; ⋆1000 gave ∞
   // instead of DOMAIN_ERROR here.
   if (!isfinite(z))   return E_DOMAIN_ERROR;
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_factorial_f(Cell * Z, APL_Float b)
{
   // the "b > 170.0" pre-check that used to sit here (Bugs28 #83)
   // rejected values up to 170.62 or so even though tgamma(b+1) is
   // still finite there (e.g. !170.5 is a genuine, finite ≈9.5E307) --
   // the isfinite() check below, mirroring bif_exponential_f() above,
   // already catches the real overflow once b+1 truly exceeds gamma's
   // finite range.
   //
const APL_Float z = tgamma(b + 1.0);
   if (!isfinite(z))   return E_DOMAIN_ERROR;
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_magnitude_f(Cell * Z, APL_Float b)
{
   return FloatCell::zF(Z, b < 0.0 ? -b : b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_nat_log_f(Cell * Z, APL_Float b)
{
   if (b == 0.0)   return E_DOMAIN_ERROR;
   if (b > 0.0)    return FloatCell::zF(Z, log(b));
   return ComplexCell::zC(Z, log(APL_Complex(b, 0)));
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_negative_f(Cell * Z, APL_Float b)
{
   return FloatCell::zF(Z, -b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_pi_times_f(Cell * Z, APL_Float b)
{
const APL_Float z = b * M_PI;
   if (!isfinite(z))   return E_DOMAIN_ERROR;   // ○1E308 gave ∞
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_pi_times_inverse_f(Cell * Z, APL_Float b)
{
const APL_Float z = b / M_PI;
   if (!isfinite(z))   return E_DOMAIN_ERROR;
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_reciprocal_f(Cell * Z, APL_Float b)
{
const APL_Float z = 1.0 / b;
   if (!isfinite(z))   return E_DOMAIN_ERROR;
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_roll_f(Cell * Z, APL_Float b)
{
   if (!Cell::is_near_int(b))   return E_DOMAIN_ERROR;
const APL_Integer set_size = (b < 0.0) ? APL_Integer(b - 0.3)
                                        : APL_Integer(b + 0.3);
   if (set_size <= 0)   return E_DOMAIN_ERROR;
const uint64_t rnd = Workspace::get_RL(set_size);
   return IntCell::zI(Z, Workspace::get_IO() + APL_Integer(rnd % set_size));
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_near_int64_t_f(Cell * Z, APL_Float b)
{
   if (!Cell::is_near_int64_t(b))   return E_DOMAIN_ERROR;
   return IntCell::zI(Z, APL_Integer(nearbyint(b)));
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
FloatCell::bif_within_quad_CT_f(Cell * Z, APL_Float b)
{
   if (b > LARGE_INT)   return E_DOMAIN_ERROR;
   if (b < SMALL_INT)   return E_DOMAIN_ERROR;

const double max_diff = Workspace::get_CT() * fabs(b);
const APL_Float val_dn = floor(b);
   if (b < (val_dn + max_diff))   return FloatCell::zF(Z, val_dn);
const APL_Float val_up = ceil(b);
   if (b > (val_up - max_diff))   return FloatCell::zF(Z, val_up);
   return E_DOMAIN_ERROR;
}
