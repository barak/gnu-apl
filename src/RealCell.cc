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

#include <math.h>

#include "Value.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "RealCell.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
ErrorCode
RealCell::bif_circle_fun(Cell * Z, const Cell * A) const
{
   if (!A->is_near_int())   return E_DOMAIN_ERROR;

   // do_bif_circle_fun() has several cases (e.g. fun 3 = tan, 4, 8, ¯8)
   // that can overflow to Inf for large but perfectly ordinary finite B
   // (confirmed: 4○1E200 silently returned Inf); mirror the finiteness
   // check ComplexCell::bif_circle_fun_c() already applies for the
   // complex-B case, instead of checking isfinite() in every branch.
   //
   // capture b before z0(Z): Z and this alias for an in-place accumulator
   // (e.g. Bif_REDUCE's A○/B), so zeroing Z first and reading b afterwards
   // would silently read back the zero. See Bugs27 #18.
   //
const APL_Float b = get_real_value();
   IntCell::z0(Z);
const ErrorCode ret = do_bif_circle_fun(Z, A->get_checked_near_int(), b);
   if (!Z->is_finite())   return E_DOMAIN_ERROR;
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
RealCell::bif_circle_fun_inverse(Cell * Z, const Cell * A) const
{
   if (!A->is_near_int())   return E_DOMAIN_ERROR;
const APL_Integer fun = A->get_checked_near_int();
const APL_Float b = get_real_value();   // see bif_circle_fun() above
   switch (fun)
      {
        case  1: case -1:
        case  2: case -2:
        case  3: case -3:
        case  4: case -4:
        case  5: case -5:
        case  6: case -6:
        case  7: case -7:
                 {
                   IntCell::z0(Z);
                   const ErrorCode ret = do_bif_circle_fun(Z, -fun, b);
                   if (!Z->is_finite())   return E_DOMAIN_ERROR;
                   return ret;
                 }
        case -10:
                 {
                   IntCell::z0(Z);
                   const ErrorCode ret = do_bif_circle_fun(Z, fun, b);
                   if (!Z->is_finite())   return E_DOMAIN_ERROR;
                   return ret;
                 }
        default: return E_DOMAIN_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
RealCell::bif_logarithm(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;
   if (A->is_complex_cell())
      return ComplexCell::bif_logarithm_cc(Z, A->get_complex_value(),
                                              get_complex_value());
   return FloatCell::bif_logarithm_ff(Z, A->get_real_value(),
                                           get_real_value());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
RealCell::do_bif_circle_fun(Cell * Z, APL_Integer fun, APL_Float b)
{
   switch(fun)
      {
        case -12:
              return ComplexCell(0, b).bif_exponential(Z);

        case -11:
             return ComplexCell::zC(Z, 0.0, b);

        case -10:
             return FloatCell::zF(Z,       b);

        case -9:
             return FloatCell::zF(Z,       b);

        case -8:
             {
               // -(b² + 1) is never ≥ 0 (b² ≥ 0 always), so the result
               // is always the pure-imaginary sqrt(b²+1) -- computed
               // via hypot(1.0, b) (Bugs28 #85), which avoids the
               // intermediate b*b overflowing for huge |b| even though
               // the true result (≈|b|) is perfectly finite.
               //
               const APL_Float root = hypot(1.0, b);   // (1 + R⋆2)⋆0.5
               if (b < 0.0)   return ComplexCell::zC(Z, 0.0, -root);
               else           return ComplexCell::zC(Z, 0.0,  root);
             }

        case -7:
             if (b > -1.0 && b < 1.0)   return FloatCell::zF(Z, atanh(b));
             if (b == -1.0 || b == 1.0)   return E_DOMAIN_ERROR;
             return ComplexCell::do_bif_circle_fun(Z, -7, APL_Complex(b));

        case  -6:
              if (b > 1.0)   return FloatCell::zF(Z, acosh(b));
              // for real b in [¯1,1], acosh(b) is exactly pure
              // imaginary (0J(acos b)) -- computing it via the general
              // complex formula (do_bif_circle_fun() below, still used
              // for b < ¯1) carries floating-point noise on the real
              // part that should be an exact 0 (e.g. ¯6○0.5 should be
              // exactly 0J1.047197551, not that plus ~1E¯17 noise).
              // See Bugs27 #59(b).
              if (b >= -1.0)   return ComplexCell::zC(Z, 0.0, acos(b));
              return ComplexCell::do_bif_circle_fun(Z, -6, APL_Complex(b));

        case -5:
             return FloatCell::zF(Z, asinh(b));

        case -4:
             {
               const double abs_b = b < 0.0 ? -b : b;
               if (abs_b >= 1.0)
                  {
                    // |b|×sqrt(1-1÷b²), not sqrt(b²-1) (Bugs28 #85): the
                    // latter overflows the intermediate b*b for huge
                    // |b| even though the true result (≈|b|) is
                    // perfectly finite; dividing by b² instead of
                    // squaring b keeps every intermediate bounded.
                    //
                    // b ≤ ¯1 gives the negative root (APL2 LRM Figure
                    // 18, Bugs35 #26), see ComplexCell case -4.
                    //
                    const double arg = 1.0 - 1.0/(b*b);
                    const double mag = abs_b * sqrt(arg < 0.0 ? 0.0 : arg);
                    return FloatCell::zF(Z, b < 0.0 ? 0.0 - mag : mag);
                  }
               return ComplexCell::do_bif_circle_fun(Z, -4, APL_Complex(b));
             }
        case -3:
             return FloatCell::zF(Z, atan (b));

        case -2:
              if (b >= -1.0 && b <= 1.0)  return FloatCell::zF(Z, acos (b));
              return ComplexCell::do_bif_circle_fun(Z, -2, APL_Complex(b));

        case -1:
             if (b >= -1.0 && b <= 1.0)  return FloatCell::zF(Z, asin (b));
             return ComplexCell::do_bif_circle_fun(Z, -1, APL_Complex(b));

        case 0:
             {
               const APL_Float b2 = 1.0 - b*b;
               if (b2 >= 0.0)   return FloatCell::zF(Z, sqrt(b2));
               return ComplexCell::do_bif_circle_fun(Z, 0, APL_Complex(b));
             }

        case 1:
             return FloatCell::zF(Z, sin(b));

        case 2:
             return FloatCell::zF(Z, cos(b));

        case 3:
             return FloatCell::zF(Z, tan(b));

        case 4:
             // hypot(1.0, b), not sqrt(1+b*b) (Bugs28 #85): the latter
             // overflows the intermediate b*b for huge |b| (b*b alone
             // exceeds DBL_MAX above roughly 1.34E154) even though the
             // true result (≈|b|) is perfectly finite; hypot() avoids
             // that intermediate overflow.
             return FloatCell::zF(Z, hypot(1.0, b));

        case   5: return FloatCell::zF(Z, sinh(b));

        case 6:
             return FloatCell::zF(Z, cosh(b));

        case 7:
             return FloatCell::zF(Z, tanh(b));

        case 8:
             {
               // same as case -8 above (mirrored sign) -- always the
               // pure-imaginary sqrt(b²+1), via the overflow-safe
               // hypot(1.0, b) (Bugs28 #85).
               //
               const APL_Float root = hypot(1.0, b);   // (1 + R⋆2)⋆0.5
               if (b < 0.0)   return ComplexCell::zC(Z, 0.0,  root);
               else           return ComplexCell::zC(Z, 0.0, -root);
             }

        case 9:
             return FloatCell::zF(Z, b);

        case 10:
              if (b < 0.0)   return FloatCell::zF(Z, -b);
              else           return FloatCell::zF(Z,  b);

        case 11:
             return FloatCell::zF(Z, 0.0);

        case 12:
             return FloatCell::zF(Z, (b < 0.0) ? M_PI : 0.0);
      }

   // invalid fun
   //
   return E_DOMAIN_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
