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

#include <limits>
#include <errno.h>
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Common.hh"
#include "ComplexCell.hh"
#include "ErrorCode.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "Output.hh"
#include "Workspace.hh"

#include "Cell.icc"

//════════════════════════════════════════════════════════════════════════════
ComplexCell::ComplexCell(APL_Complex c)
{
   // + 0.0 turns an IEEE -0.0 into 0.0: APL has no signed zero, but log()
   // and atan2() return the other side of the branch cut for it, e.g.
   // ⍟-1J1×1J¯1 (-C has the imaginary part -0.0, Bugs41 #28)
   //
   value.cval[0] = c.real() + 0.0;
   value.cval[1] = c.imag() + 0.0;
}
//────────────────────────────────────────────────────────────────────────────
ComplexCell::ComplexCell(APL_Float r, APL_Float i)
{
   value.cval[0] = r + 0.0;   // no -0.0, see above
   value.cval[1] = i + 0.0;
}
//────────────────────────────────────────────────────────────────────────────
bool
ComplexCell::equal(const Cell & A, double qct) const
{
   if (!A.is_numeric())   return false;

   if (qct == 0.0 && A.is_integer_cell() && get_imag_value() == 0.0)
      {
        // Bugs31 #9 (Blake McBride): A.get_complex_value() below
        // converts A's exact int64 real part through double first,
        // losing precision beyond 2⋆53 -- same bug as
        // FloatCell::equal()'s Bugs31 #8, for a ComplexCell that is
        // really just a real number (zero imaginary part).
        //
        return Cell::compare_int_float(A.get_int_value(),
                                        get_real_value()) == 0;
      }

   return tolerantly_equal(A.get_complex_value(), get_complex_value(), qct);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_ceiling(Cell * Z) const
{
   return ComplexCell::bif_ceiling_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_conjugate(Cell * Z) const
{
   return ComplexCell::bif_conjugate_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_direction(Cell * Z) const
{
   return ComplexCell::bif_direction_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_exponential(Cell * Z) const
{
   return ComplexCell::bif_exponential_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
// monadic build-in functions...
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_factorial(Cell * Z) const
{
   return ComplexCell::bif_factorial_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_floor(Cell * Z) const
{
   return ComplexCell::bif_floor_c(Z, cval());
}
//════════════════════════════════════════════════════════════════════════════
ErrorCode
ComplexCell::bif_magnitude(Cell * Z) const
{
   return ComplexCell::bif_magnitude_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_nat_log(Cell * Z) const
{
   return ComplexCell::bif_nat_log_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_negative(Cell * Z) const
{
   return ComplexCell::bif_negative_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_pi_times(Cell * Z) const
{
   return ComplexCell::bif_pi_times_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_pi_times_inverse(Cell * Z) const
{
   return ComplexCell::bif_pi_times_inverse_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_reciprocal(Cell * Z) const
{
   return ComplexCell::bif_reciprocal_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_roll(Cell * Z) const
{
   return ComplexCell::bif_roll_c(Z, cval());
}
//════════════════════════════════════════════════════════════════════════════
// dyadic build-in functions...
//════════════════════════════════════════════════════════════════════════════
ErrorCode
ComplexCell::bif_add(Cell * Z, const Cell * A) const
{
   return ComplexCell::bif_add_cc(Z, A->get_complex_value(), cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_subtract(Cell * Z, const Cell * A) const
{
   return ComplexCell::bif_subtract_cc(Z, A->get_complex_value(), cval());
}
//────────────────────────────────────────────────────────────────────────────
/// x ÷ den ÷ s for a power of 2 s: divide by s first if s < 1, so that a
/// subnormal x is scaled up before it is divided (Bugs41 #85)
static inline APL_Float
scaled_div(APL_Float x, APL_Float den, APL_Float s)
{
   return (s < 1.0) ? (x / s) / den : (x / den) / s;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_divide(Cell * Z, const Cell * A) const
{
   return ComplexCell::bif_divide_cc(Z, A->get_complex_value(), cval());
}
//════════════════════════════════════════════════════════════════════════════
ErrorCode
ComplexCell::bif_logarithm(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;
   return ComplexCell::bif_logarithm_cc(Z, A->get_complex_value(), cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_multiply(Cell * Z, const Cell * A) const
{
   return ComplexCell::bif_multiply_cc(Z, A->get_complex_value(), cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_power(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;
   return ComplexCell::bif_power_cc(Z, A->get_complex_value(), cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_maximum(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;

   // B with an imaginary part of exactly 0 is real: use the (exact at
   // ⎕CT←0) real path, e.g. for a large IntCell A (Blake McBride, Bugs32
   // #14)
   //
   if (value.cval[1] == 0.0 && A->get_imag_value() == 0.0)
      {
        const FloatCell B_real(value.cval[0]);
        return B_real.bif_maximum(Z, A);
      }
   return ComplexCell::max_min_cc(Z, *A, *this, true);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_minimum(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;

   // B with an imaginary part of exactly 0 is real: use the (exact at
   // ⎕CT←0) real path, e.g. for a large IntCell A (Blake McBride, Bugs32
   // #14)
   //
   if (value.cval[1] == 0.0 && A->get_imag_value() == 0.0)
      {
        const FloatCell B_real(value.cval[0]);
        return B_real.bif_minimum(Z, A);
      }
   return ComplexCell::max_min_cc(Z, *A, *this, false);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_residue(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;

   // B with an imaginary part of exactly 0 is real: use the (exact at
   // ⎕CT←0) real path, e.g. for a large IntCell A (Blake McBride, Bugs32
   // #14)
   //
   if (value.cval[1] == 0.0 && A->get_imag_value() == 0.0)
      {
        const FloatCell B_real(value.cval[0]);
        return B_real.bif_residue(Z, A);
      }
   return ComplexCell::bif_residue_cc(Z, A->get_complex_value(), cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_circle_fun(Cell * Z, const Cell * A) const
{
   if (!A->is_near_int64_t())   return E_DOMAIN_ERROR;   // Bugs38 #93
   return ComplexCell::bif_circle_fun_c(Z, A->get_checked_near_int(), cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_circle_fun_inverse(Cell * Z, const Cell * A) const
{
   if (!A->is_near_int64_t())   return E_DOMAIN_ERROR;   // Bugs38 #93
   return ComplexCell::bif_circle_fun_inverse_c(Z, A->get_checked_near_int(),
                                                   cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_add_inverse(Cell * Z, const Cell * A) const
{
   return A->bif_subtract(Z, this);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_multiply_inverse(Cell * Z, const Cell * A) const
{
   return A->bif_divide(Z, this);
}
//────────────────────────────────────────────────────────────────────────────
APL_Complex
ComplexCell::gamma(APL_Float x, const APL_Float & y)
{
const APL_Complex ret = gamma_nothrow(x, y);
   if (!isfinite(ret.real()))   DOMAIN_ERROR;
   if (!isfinite(ret.imag()))   DOMAIN_ERROR;
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
APL_Complex
ComplexCell::log_gamma(APL_Float x, APL_Float y)
{
   // ln Γ(x + iy) (up to a multiple of 2πi, which does not matter for its
   // exp()), with the same Lanczos approximation as gamma_nothrow(), for
   // arguments whose Γ overflows (Bugs39 #36)
   //
const APL_Complex pi(M_PI, 0);
const APL_Complex z(x, y);
   if (x < 0.5)   // reflection: Γ(z) = π ÷ sin(πz) × Γ(1-z)
      return log(pi) - log(sin(pi * z)) - log_gamma(1.0 - x, -y);

   // Stirling's series, which is accurate to a few ulps for |w| ≥ 10. The
   // Lanczos approximation (as in gamma_nothrow()) has a relative error
   // of about 2E¯10, which the difference of three log-Γ values in K!B
   // turned into results with only 10 correct digits (Bugs41 #39).
   // For a smaller |z|: ln Γ(z) = ln Γ(z+n) - ln(z×(z+1)×...×(z+n-1)).
   //
APL_Complex w = z;
APL_Complex shift(0.0, 0.0);
   while (abs(w) < 10.0)   { shift += log(w);   w += 1.0; }

const APL_Complex c = 1.0 / w;
const APL_Complex c2 = c * c;
const APL_Complex series = c * (1.0/12.0 - c2 * (1.0/360.0 - c2 *
                               (1.0/1260.0 - c2 * (1.0/1680.0 - c2 / 1188.0))));
   return (w - 0.5) * log(w) - w + 0.5*log(2*M_PI) + series - shift;
}
//────────────────────────────────────────────────────────────────────────────
APL_Complex
ComplexCell::gamma_nothrow(APL_Float x, const APL_Float & y)
{
const APL_Complex pi(M_PI, 0);
   if (x < 0.5)
      return pi / (sin(pi * APL_Complex(x, y)) * gamma_nothrow(1.0 - x, -y));

   // coefficients for lanczos approximation of the gamma function.
   //
#define p0 APL_Complex(  1.000000000190015   )
#define p1 APL_Complex( 76.18009172947146    )
#define p2 APL_Complex(-86.50532032941677    )
#define p3 APL_Complex( 24.01409824083091    )
#define p4 APL_Complex( -1.231739572450155   )
#define p5 APL_Complex(  1.208650973866179E-3)
#define p6 APL_Complex( -5.395239384953E-6   )

   errno = 0;
   feclearexcept(FE_ALL_EXCEPT);

const APL_Complex z(x, y);
const APL_Complex z1(x + 5.5, y);
const APL_Complex z2(x + 0.5, y);

const APL_Complex ret( (complex_sqrt(APL_Complex(2*M_PI)) / z)
                      * (p0                           +
                         p1 / APL_Complex(x + 1.0, y) +
                         p2 / APL_Complex(x + 2.0, y) +
                         p3 / APL_Complex(x + 3.0, y) +
                         p4 / APL_Complex(x + 4.0, y) +
                         p5 / APL_Complex(x + 5.0, y) +
                         p6 / APL_Complex(x + 6.0, y))
                       * complex_power(z1, z2)
                       * complex_exponent(-z1)
                     );

   return ret;

#undef p0
#undef p1
#undef p2
#undef p3
#undef p4
#undef p5
#undef p6
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::do_bif_circle_fun(Cell * Z, APL_Integer fun, APL_Complex b)
{
const APL_Complex one(1.0, 0.0);
   switch(fun)
      {
        case -12: {
                    // bif_exponential() returns E_DOMAIN_ERROR for a
                    // non-finite result without writing Z; that was
                    // being discarded here, unconditionally reporting
                    // success even on overflow (Blake McBride, Bugs22
                    // #3).
                    ComplexCell cb(-b.imag(), b.real());
                    return cb.bif_exponential(Z);
                  }

        case -11: return ComplexCell::zC(Z, -b.imag(), b.real());

        case -10: return ComplexCell::zC(Z, b.real(), -b.imag());

        case  -9: return ComplexCell::zC(Z, b);
        case  -8: // ¯8○Z ←→ -8○Z
                  {
                    // same discarded-ErrorCode pattern as case -12,
                    // here on the recursive 8○Z (Bugs22 #3 appendix).
                    const ErrorCode ec = do_bif_circle_fun(Z, 8, b);
                    if (ec != E_NO_ERROR)   return ec;
                    return ComplexCell::zC(Z, -Z->get_complex_value());
                  }

        case  -7: // arctanh(z) = 0.5 (ln(1.0 + z) - ln(1.0 - z))
                  {
                    // std::atanh() is accurate for tiny b (where 1±b
                    // round to 1, Bugs30 #7) and near ±1 (Bugs31 #14),
                    // and keeps a tiny imaginary part that is the entire
                    // result, e.g. ¯7○0J1E¯11 (Blake McBride, Bugs32 #27:
                    // the real-only shortcut used before returned 0). It
                    // agrees with the formula above except ON the branch
                    // cut (Im b = 0 exactly, |Re b| > 1), where GNU APL
                    // (like the formula) has Im < 0 for Re b > 1 and Im > 0
                    // for Re b < ¯1, i.e. std's value for Im b = ∓0.
                    //
                    APL_Complex b1 = b;
                    if (b.imag() == 0.0 && fabs(b.real()) > 1.0)
                       b1 = APL_Complex(b.real(), b.real() > 0.0 ? -0.0 : 0.0);
                    return ComplexCell::zC(Z, std::atanh(b1));
                  }

        case  -6: // arccosh(z) = ln(z + sqrt(z + 1) sqrt(z - 1))
                  {
                    // log() of a sum close to 1 loses a small real part,
                    // e.g. 9○¯6○0.5J1E¯11 was wrong from the 6th digit
                    // (Bugs34 #12). std::acosh() agrees with the formula
                    // except for the sign of a zero imaginary part ON the
                    // branch cut (Im b = 0, Re b < 1), where the formula's
                    // value is std's for Im b = +0.
                    //
                    APL_Complex b1 = b;
                    if (b.imag() == 0.0)   b1 = APL_Complex(b.real(), 0.0);
                    return ComplexCell::zC(Z, std::acosh(b1));
                  }

        case  -5: // arcsinh(z) = ln(z + sqrt(z^2 + 1))
                  {
                    // std::asinh() does not overflow for huge b (Bugs30
                    // #7/#9) and keeps a tiny imaginary part that is the
                    // entire result, e.g. ¯5○0J1E¯11 (Blake McBride, Bugs32
                    // #27: the real-only shortcut used before returned 0).
                    // It agrees with the formula above except ON the branch
                    // cut (Re b = 0 exactly, |Im b| > 1), where GNU APL (like
                    // the formula) gives the real part the sign of Im b,
                    // i.e. std's value for Re b = ±0.
                    //
                    APL_Complex b1 = b;
                    if (b.real() == 0.0 && fabs(b.imag()) > 1.0)
                       b1 = APL_Complex(b.imag() > 0.0 ? 0.0 : -0.0, b.imag());
                    return ComplexCell::zC(Z, std::asinh(b1));
                  }

        case  -4:
             {
               // APL2 LRM p. 81, Figure 18: ¯4○B ←→ (¯1+B⋆2)⋆.5 for
               // X≥0, or for ¯1<X<0 and Y=0, and -(¯1+B⋆2)⋆.5 otherwise
               // (X = 9○B, Y = 11○B, ⋆.5 = principal root). This is the
               // (B+1)×((B-1)÷(B+1))⋆.5 form; it keeps ¯6○B ≡ ⍟B+¯4○B
               // and ¯4○B ≡ 5○¯6○B, and it is continuous across the
               // imaginary axis (Blake McBride, Bugs35 #26). The
               // principal root alone (Bugs26 #4a, Bugs32 #5) broke
               // both. RealCell::do_bif_circle_fun() uses the same rule.
               //
               // (¯1+B⋆2) is the same for B and -B, so the negated
               // branch is minus the principal root of -B.
               //
               if (b.real() < 0.0 && (b.real() <= -1.0 || b.imag() != 0.0))
                  {
                    const ErrorCode ret = do_bif_circle_fun(Z, -4, -b);
                    if (ret != E_NO_ERROR)   return ret;
                    const APL_Complex z = Z->get_complex_value();
                    return ComplexCell::zC(Z, APL_Complex(0.0 - z.real(),
                                                          0.0 - z.imag()));
                  }

               // the principal root (¯1+B⋆2)⋆.5 for the remaining B.
               //
               // For b with an exactly-zero imaginary part (i.e. a
               // real value that reached this complex path only
               // because RealCell delegates here for |b|<1), compute
               // b*b-1 with real arithmetic instead of complex b*b:
               // squaring b as a complex number can produce a -0.0
               // imaginary part purely from floating-point sign-of-
               // zero rules (e.g. b=(-0.5,0.0) gives b*b=(0.25,-0.0)),
               // and complex_sqrt() respects that sign as a branch-cut
               // side even though a real b was never actually
               // approaching the cut from either side -- flipping the
               // resulting branch for no mathematical reason (Blake
               // McBride, Bugs26 #4b).
               // huge near-real |b| (Bugs30 #9, incomplete Bugs28 #85):
               // b.real()*b.real() below overflows even though the true
               // result (≈|b.real()|) is finite; RealCell's own case -4
               // avoids exactly this by dividing by b² instead of
               // squaring b -- mirror it here for a real-valued arg.
               //
               // Only if the imaginary part is exactly 0 or the general
               // path would overflow: otherwise a tiny imaginary part can
               // dominate the result near |Re b| = 1, e.g. ¯4○1J1E¯11 ≈
               // 3.16E¯6J3.16E¯6, not 0 (Blake McBride, Bugs32 #27).
               //
               if (b.imag() == 0.0 ||
                   (Cell::is_near_zero(b.imag()) && fabs(b.real()) > 1E150))
                  {
                    const double b_re = b.real();
                    const double abs_b = b_re < 0.0 ? -b_re : b_re;

                    // Bugs31 #13 (Blake McBride): both shortcuts below
                    // always returned the POSITIVE-imaginary root,
                    // disagreeing with the general (non-shortcut) path
                    // just outside this near-zero band whenever a
                    // nonzero (even if tiny) b.imag() actually puts the
                    // true result on the other side of the branch cut.
                    // Im(b²-1) ≈ 2×b.real()×b.imag() for a small
                    // perturbation, so the correct sign is
                    // +sign(b.real())×sign(b.imag()); an exactly-zero
                    // b.imag() has no side to resolve and keeps the
                    // existing (arbitrary but RealCell-matching)
                    // positive convention. The tiny real part such a
                    // perturbation would also add is not reproduced
                    // here (deliberately out of scope, see memory).
                    //
                    // signbit() rather than > 0.0, so that Re b = 0
                    // gets the sign of Im b, like the general path
                    // (Blake McBride, Bugs32 #5).
                    //
                    const bool same_sign = b.imag() != 0.0 &&
                          (std::signbit(b_re) == std::signbit(b.imag()));

                    if (abs_b >= 1.0)
                       {
                         // the result is ≈ ±mag + tiny×i, with the sign
                         // only in the (dropped) imaginary part: the
                         // principal root never has a negative real part
                         // (Blake McBride, Bugs32 #5). The LRM negation
                         // for Re b < 0 is done above.
                         //
                         const double arg = 1.0 - 1.0/(b_re*b_re);
                         const double mag = abs_b < 1E150   // Bugs41 #84
                               ? sqrt((abs_b - 1.0)*(abs_b + 1.0))
                               : abs_b * sqrt(arg < 0.0 ? 0.0 : arg);
                         return ComplexCell::zC(Z, mag);
                       }

                    const APL_Complex root = complex_sqrt(   // Bugs41 #84
                           APL_Complex((b_re - 1.0)*(b_re + 1.0), 0.0));
                    if (b.imag() == 0.0)   return ComplexCell::zC(Z, root);
                    return ComplexCell::zC(Z, same_sign
                                  ? root : APL_Complex(root.real(),
                                                        -root.imag()));
                  }
               // b×b-1 = (x-1)(x+1) - y² + i×2xy for b = x+iy: no
               // cancellation of the real part near x = ±1, and 2xy keeps
               // the sign of a zero x (which decides the side of the cut)
               {
                 const double x = b.real();
                 const double y = b.imag();
                 const APL_Complex b2_1((x - 1.0)*(x + 1.0) - y*y, 2.0*x*y);
                 return ComplexCell::zC(Z, complex_sqrt(b2_1));
               }
             }

        case  -3: // arctan(z) = i/2 (ln(1 - iz) - ln(1 + iz))
                  {
                    // the formula subtracts two logarithms of nearly the
                    // same size and loses a small imaginary part, e.g.
                    // 11○¯3○1J1E¯11 was wrong from the 6th digit (Bugs34
                    // #12). std::atan() agrees with it except for the sign
                    // of a zero real part ON the branch cut (Re b = 0,
                    // |Im b| > 1), where the formula's value is std's for
                    // Re b = +0.
                    //
                    APL_Complex b1 = b;
                    if (b.real() == 0.0)   b1 = APL_Complex(0.0, b.imag());
                    return ComplexCell::zC(Z, std::atan(b1));
                  }

        case  -2: // arccos(z)
        case  -1: // arcsin(z) = -i (ln(iz + sqrt(1 - z^2)))
                  {
                    // The defining formula computed literally overflows in
                    // b*b for |b| > 1.34E154 (DOMAIN ERROR even for real
                    // b), and iz + sqrt(1-z²) cancels catastrophically for
                    // large Re b with a small positive Im b (Blake McBride,
                    // Bugs32 #12). std::asin()/std::acos() evaluate it
                    // stably, and they agree with the formula everywhere
                    // except ON the branch cut (Im b = 0 exactly, |Re b| >
                    // 1). There, GNU APL (like the formula) puts the result
                    // on the side Im < 0 for Re b > 1 and Im > 0 for
                    // Re b < ¯1 (arcsin, and arccos = π/2 - arcsin
                    // accordingly, Bugs26 #3), which is std's value for
                    // Im b = ∓0.
                    //
                    APL_Complex b1 = b;
                    if (b.imag() == 0.0 && fabs(b.real()) > 1.0)
                       b1 = APL_Complex(b.real(), b.real() > 0.0 ? -0.0 : 0.0);

                    return ComplexCell::zC(Z, fun == -1 ? std::asin(b1)
                                                        : std::acos(b1));
                  }

        case   0:
             {
               // huge near-real |b| (Bugs28 #85, mirrors case -4's
               // real-arithmetic special case above): complex b*b
               // overflows below even though the true result (≈i×|b|)
               // is perfectly finite. The imaginary part's sign only
               // depends on b² (same for b and -b), so no sign flip is
               // needed here unlike cases -4/8/¯8.
               //
               // only if the imaginary part is exactly 0 or the general
               // path would overflow (see case -4, Bugs32 #27)
               //
               if ((b.imag() == 0.0 || fabs(b.real()) > 1E150) &&
                   Cell::is_near_zero(b.imag()) && fabs(b.real()) >= 1.0)
                  {
                    const double abs_b = fabs(b.real());
                    const double arg = 1.0 - 1.0/(b.real()*b.real());
                    const double mag = abs_b < 1E150   // Bugs41 #84
                          ? sqrt((abs_b - 1.0)*(abs_b + 1.0))
                          : abs_b * sqrt(arg < 0.0 ? 0.0 : arg);

                    // Bugs31 #13 (Blake McBride): see case -4 above --
                    // same shortcut-vs-general-path sign discontinuity,
                    // but Im(1-z²) ≈ -2×b.real()×b.imag() here (opposite
                    // sign convention from case -4's b²-1), so the
                    // correct sign is -sign(b.real())×sign(b.imag()).
                    //
                    if (b.imag() == 0.0)   return ComplexCell::zC(Z, 0.0, mag);
                    const bool same_sign = (b.real() > 0.0)
                                         == (b.imag() > 0.0);
                    return ComplexCell::zC(Z, 0.0, same_sign ? -mag : mag);
                  }
               // 1-b×b = (1-x)(1+x) + y² - i×2xy for b = x+iy (see case -4)
               {
                 const double x = b.real();
                 const double y = b.imag();
                 const APL_Complex one_b2((1.0 - x)*(1.0 + x) + y*y,
                                          -2.0*x*y);
                 return ComplexCell::zC(Z, complex_sqrt(one_b2));
               }
             }

        case   1: return ComplexCell::zC(Z, sin(b));

        case   2: return ComplexCell::zC(Z, cos(b));

        case   3: return ComplexCell::zC(Z, tan(b));

        case   4:
             {
               // huge near-real |b| (Bugs30 #9, incomplete Bugs28 #85):
               // complex b*b overflows below even though the true
               // result (≈|b|) is finite; RealCell::do_bif_circle_fun()
               // case 4 already uses hypot(1.0, b) for exactly this
               // reason -- mirror it here for a real-valued ComplexCell.
               //
               if (Cell::is_near_zero(b.imag()))
                  return ComplexCell::zC(Z, hypot(1.0, b.real()));
               return ComplexCell::zC(Z, complex_sqrt(one + b*b));
             }

        case   5: return ComplexCell::zC(Z, sinh(b));

        case   6: return ComplexCell::zC(Z, cosh(b));

        case   7: return ComplexCell::zC(Z, tanh(b));

        case   8: {
                    // huge near-real |b| (Bugs30 #9, incomplete Bugs28
                    // #85): b*b below overflows even though the true
                    // magnitude (hypot(1.0, b.real()) ≈ |b|) is finite.
                    // Only the magnitude is replaced; the sign selection
                    // below is unchanged (it depends only on root's
                    // value, not on how it was computed).
                    //
                    const bool near_real = Cell::is_near_zero(b.imag());
                    const APL_Complex root = near_real
                       ? APL_Complex(0.0, hypot(1.0, b.real()))
                       : complex_sqrt(APL_Complex(-1, 0) - b*b);

                    // Bugs31 #12 (Blake McBride): the near-real shortcut
                    // above always builds a POSITIVE-imaginary root,
                    // unlike the general complex_sqrt() path, whose sign
                    // responds to b.imag()'s own (possibly tiny) value --
                    // so the sign selection below, unchanged since it
                    // was written for the general path, must not consult
                    // the actual (tiny, no-longer-reflected-in-root)
                    // b.imag() when the shortcut fired; treat it as
                    // exactly 0, matching the b.imag()==0.0 neighbour and
                    // staying continuous with the general path just
                    // outside the near-zero band.
                    //
                    const APL_Float sign_imag = near_real ? 0.0 : b.imag();

                    if (b.real()  > 0.0)
                       {
                         if (sign_imag > 0.0)  return ComplexCell::zC(Z,  root);
                         else                  return ComplexCell::zC(Z, -root);
                       }
                   else if (b.real() == 0.0)
                       {
                         if (b.imag() > 1.0)   return ComplexCell::zC(Z,  root);
                         else                  return ComplexCell::zC(Z, -root);
                       }
                   else   // b.real() < 0,0;
                       {
                         if (sign_imag >= 0.0) return ComplexCell::zC(Z,  root);
                         else                  return ComplexCell::zC(Z, -root);
                       }
                  }

        case   9: return ComplexCell::zC(Z, b.real());

        case  10:
             // hypot(), not sqrt(mag2(b)) (Bugs30 #7/#9): the latter
             // squares both parts first, which overflows for huge |b|
             // and underflows to 0 for tiny |b| even though the true
             // magnitude is finite/nonzero in both cases.
             return ComplexCell::zC(Z, hypot(b.real(), b.imag()));

        case  11: return ComplexCell::zC(Z, b.imag());

        case  12: return ComplexCell::zC(Z, atan2(b.imag(), b.real()));
      }

   // invalid fun
   //
   return E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
APL_Float
ComplexCell::get_real_value() const
{
   return value.cval[0];
}
//────────────────────────────────────────────────────────────────────────────
APL_Float
ComplexCell::get_imag_value() const
{
   return value.cval[1];
}
//────────────────────────────────────────────────────────────────────────────
bool
ComplexCell::is_near_int() const
{
   // "is this value effectively an integer", not "are both parts of
   // its Gaussian-integer representation individually near-integers"
   // -- 1J1 has an integral real part AND an integral imaginary part,
   // yet is not, itself, remotely close to any integer. The imaginary
   // part must be ≈0 (matching get_near_int()'s own, already-correct
   // requirement below) before the real part's integrality is even
   // meaningful. Getting this wrong let a genuinely complex value
   // silently pass every "is this int-like?" guard that exists
   // specifically to decide whether get_near_int() is then safe to
   // call -- which itself correctly throws DOMAIN_ERROR for a
   // non-zero imaginary part, so every such guard was purely
   // decorative for a value like 1J1 (Bugs28 #32): the "safe" branch
   // still called the throwing function, in at least one case (Token.cc
   // canonical()'s own error-message formatting for a failed [axis])
   // recursing back into the very error-reporting code that was
   // guarding against exactly this, and stack-overflowing.
   //
   return Cell::is_near_zero(value.cval[1]) &&
          Cell::is_near_int(value.cval[0]);
}
//────────────────────────────────────────────────────────────────────────────
bool
ComplexCell::is_near_int64_t() const
{
   return Cell::is_near_zero(value.cval[1]) &&
          Cell::is_near_int64_t(value.cval[0]);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_near_int64_t(Cell * Z) const
{
   return ComplexCell::bif_near_int64_t_c(Z, cval());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_within_quad_CT(Cell * Z) const
{
   return ComplexCell::bif_within_quad_CT_c(Z, cval());
}
//════════════════════════════════════════════════════════════════════════════
bool
ComplexCell::is_near_zero() const
{
   if (value.cval[0]  >=  INTEGER_TOLERANCE)   return false;
   if (value.cval[0]  <= -INTEGER_TOLERANCE)   return false;
   if (value.cval[1] >=  INTEGER_TOLERANCE)    return false;
   if (value.cval[1] <= -INTEGER_TOLERANCE)    return false;
   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
ComplexCell::is_near_one() const
{
   if (value.cval[0] >  (1.0 + INTEGER_TOLERANCE))   return false;
   if (value.cval[0] <  (1.0 - INTEGER_TOLERANCE))   return false;
   if (value.cval[1] >  INTEGER_TOLERANCE)           return false;
   if (value.cval[1] < -INTEGER_TOLERANCE)           return false;
   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
ComplexCell::is_near_real() const
{
   // relative test only (Bugs28 #79): the "I is absolutely small"
   // disjunct that used to sit here (I2 < B2 alone, regardless of R2)
   // made *every* complex number with a tiny-but-nonzero imaginary part
   // near-real, including one whose real part is comparably tiny (or
   // zero) -- so e.g. 1E¯20J1E¯20 (equal parts) displayed as if it were
   // purely real, and character_representation() dropped the J part.
   // Judging "small" only relative to the real part is the right test:
   // it still recognises a genuinely real value (I2 == 0) for any R2,
   // and a value whose I and R are comparable in magnitude, however
   // tiny both are, is correctly kept complex.
   //
const APL_Float imag = value.cval[1];
   if (imag == 0.0)   return true;   // I is exactly 0 (e.g. the literal 0J0)

   // fabs(imag) < tol*fabs(real), not I2=imag²,R2=real² (Bugs30 #6
   // sibling): squaring first underflows both to exactly 0 for a tiny
   // but comparable-magnitude imag/real pair (e.g. 5E¯201J¯5E¯201),
   // which then wrongly took the "I2==0.0" branch above and displayed
   // the value as real even though imag is NOT negligible relative to
   // real -- computing the ratio directly never needs an intermediate
   // that can underflow (or, for a huge real part, overflow).
   //
   return fabs(imag) < REAL_TOLERANCE*fabs(value.cval[0]);   // I relatively small
}
//────────────────────────────────────────────────────────────────────────────
// throw/nothrow boundary. Functions above MUST NOT (directly or indirectly)
// throw while funcions below MAY throw.
//────────────────────────────────────────────────────────────────────────────

#include "Error.hh"   // throws

//────────────────────────────────────────────────────────────────────────────
bool
ComplexCell::greater(const Cell & other) const
{
   switch(other.get_cell_type())
      {
        case CT_CHAR:    return true;
        case CT_INT:
        case CT_FLOAT:
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
Comp_result
ComplexCell::compare(const Cell & other) const
{
   if (other.is_character_cell())   return COMP_GT;   // numeric > char
   if (other.is_pointer_cell())     return COMP_LT;   // numeric < nested
   if (!other.is_numeric())   DOMAIN_ERROR;

const double qct = Workspace::get_CT();

   if (qct == 0.0 && other.is_integer_cell() && get_imag_value() == 0.0)
      {
        // Bugs31 #9 (Blake McBride): equal()'s get_complex_value()
        // round-trip below converts other's exact int64 real part
        // through double first, losing precision beyond 2⋆53 -- same
        // bug as FloatCell::compare()'s Bugs31 #8, for a ComplexCell
        // that is really just a real number (zero imaginary part).
        //
        const int cr = Cell::compare_int_float(other.get_int_value(),
                                                get_real_value());
        if (cr < 0)   return COMP_GT;   // other < this
        if (cr > 0)   return COMP_LT;   // other > this
        return COMP_EQ;
      }

   if (equal(other, qct))   return COMP_EQ;

APL_Float areal = other.get_real_value();
APL_Float breal = get_real_value();

   // we compare complex numbers by their real part unless the real parsts
   // are equal. In that case we compare the imag parts. The reason for
   // comparing this way is compatibility with the comparison of real numbers
   //
   // the real parts are compared exactly (apl.texi "Comparison Rules",
   // rule 7): a tolerant comparison made the order intransitive (Bugs39 #56)
   //
   if (areal == breal)
      {
        areal = other.get_imag_value();
        breal = get_imag_value();
      }

   return (breal < areal) ? COMP_LT : COMP_GT;
}
//════════════════════════════════════════════════════════════════════════════
PrintBuffer
ComplexCell::character_representation(const PrintContext & pctx) const
{
   if (pctx.get_PP() < MAX_Quad_PP)
      {
         // lrm p. 13: In J notation, the real or imaginary part is not
         // displayed if it is less than the other by more than ⎕PP orders
         // of magnitude (unless ⎕PP is at its maximum). As in IBM APL2
         // (verified with APL2 itself), "orders of magnitude" are the
         // decimal exponents ⌊10⍟|part|: e.g. with ⎕PP←5, 18.104 (exponent
         // 1) in 2.46E6J18.104 (exponent 6) is displayed, while 0.0055 in
         // 2.41E4J0.0055 (exponents ¯3 and 4) is not. (Comparing the ratio
         // of the parts with 10⋆⎕PP instead dropped the 18.104.) A part
         // that is exactly 0 is smaller than any other.
         //
         const Displayed_parts parts =
               exponent_rule(value.cval[0], value.cval[1], pctx.get_PP());

         if (parts == DP_REAL)
            {
              // same rationale as the mirror-image branch below:
              // give the real part its own scaled/unscaled decision
              // instead of reusing pctx's, which may reflect the
              // (now discarded) imaginary part's need for scaling.
              //
              PrintContext pctx_r(pctx);
              PrintStyle st = PrintStyle(pctx.get_style() & ~PST_SCALED);
              if (FloatCell::need_scaling(value.cval[0], pctx.get_PP()))
                 st = PrintStyle(st | PST_SCALED);
              pctx_r.set_style(st);

              const FloatCell real_cell(value.cval[0]);
              return real_cell.character_representation(pctx_r);
            }
         else if (parts == DP_IMAG)
            {
              // give the imaginary part its own scaled/unscaled
              // decision instead of reusing pctx's, which reflects
              // the (now discarded) real part's -- otherwise, e.g.
              // a real part small enough to need scaling forces
              // the imaginary part into E-format too, even though
              // it is displayed alone here (Bugs28 #80). Same
              // rationale as the non-shortcut path below.
              //
              PrintContext pctx_i(pctx);
              PrintStyle st = PrintStyle(pctx.get_style() & ~PST_SCALED);
              if (FloatCell::need_scaling(value.cval[1], pctx.get_PP()))
                 st = PrintStyle(st | PST_SCALED);
              pctx_i.set_style(st);

              const FloatCell imag_cell(value.cval[1]);
              PrintBuffer ret = imag_cell.character_representation(pctx_i);
              ret.pad_l(UNI_J, 1);
              ret.pad_l(UNI_0, 1);

              ret.get_info().flags |= CT_COMPLEX;
              ret.get_info().imag_len = 1 + ret.get_info().real_len;
              ret.get_info().int_len = 1;
              ret.get_info().fract_len = 0;
              ret.get_info().real_len = 1;
              return ret;
            }
      }

bool scaled_real = pctx.get_scaled();   // may be changed by print function
UCS_string ucs(value.cval[0], scaled_real, pctx);

ColInfo info;
   info.flags |= CT_COMPLEX;
   if (scaled_real)   info.flags |= real_has_E;
int int_fract = ucs.size();
   info.real_len = ucs.size();
   info.int_len = ucs.size();
   loop(u, ucs.size())
      {
       if (ucs[u] == UNI_FULLSTOP)
           {
             info.int_len = u;
             if (!scaled_real)   break;
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

   // the J part is only dropped by exponent_rule() above (lrm p. 13), or
   // if it is exactly 0: the former fixed is_near_real() band (|imag| <
   // 1E¯10×|real|) ignored ⎕PP, so that e.g. 9007199254740992J1000 could
   // never be displayed, not even with ⎕PP←17 (Blake McBride, Bugs32 #13).
   //
   if (value.cval[1] != 0.0)
      {
        ucs << UNI_J;
        // the imaginary part's own need for scaled (E-format) notation,
        // not the real part's/column's pctx.get_scaled() decision
        // (Bugs28 #80, Bugs27 #42 residual): reusing that shared flag
        // forced the imaginary part into E-format whenever the real
        // part (or another value in the same column) needed it, even
        // when the imaginary part's own magnitude did not -- e.g.
        // 1E¯7J0.5 gave 1E¯7J5E¯1 instead of 1E¯7J0.5.
        //
        bool scaled_imag = FloatCell::need_scaling(value.cval[1],
                                                     pctx.get_PP());
        const UCS_string ucs_i(value.cval[1], scaled_imag, pctx);

        ucs << ucs_i;

        info.imag_len = ucs.size() - info.real_len;
        if (scaled_imag)   info.flags |= imag_has_E;
      }

   return PrintBuffer(ucs, info);
}
//────────────────────────────────────────────────────────────────────────────
ComplexCell::Displayed_parts
ComplexCell::exponent_rule(APL_Float real, APL_Float imag, int quad_PP)
{
   if (quad_PP >= MAX_Quad_PP)   return DP_BOTH;

const APL_Float pos_real = real < 0.0 ? -real : real;
const APL_Float pos_imag = imag < 0.0 ? -imag : imag;
   // a non-finite part (which should never be stored, Bugs39 #31) is shown
   // with the other part: int(floor(log10(∞))) is undefined behaviour
   if (!isfinite(pos_real) || !isfinite(pos_imag))   return DP_BOTH;

const int expo_real = pos_real == 0.0 ? 0 : int(floor(log10(pos_real)));
const int expo_imag = pos_imag == 0.0 ? 0 : int(floor(log10(pos_imag)));

   if (pos_real >= pos_imag)   // pos_real dominates pos_imag
      {
        if (pos_real > 0.0 &&
            (pos_imag == 0.0 || expo_real - expo_imag > quad_PP))
           return DP_REAL;
      }
   else                        // pos_imag dominates pos_real
      {
        if (pos_real == 0.0 || expo_imag - expo_real > quad_PP)
           return DP_IMAG;
      }

   return DP_BOTH;
}
//────────────────────────────────────────────────────────────────────────────
bool
ComplexCell::need_scaling(const PrintContext &pctx) const
{
   // The scaled flag of a PrintContext only controls the real part of a
   // complex number: character_representation() decides on its own if the
   // imaginary part (or a part displayed alone) is scaled. Therefore only
   // the real part matters here (IBM APL2: 9571.5J1.824E5, not
   // 9.5715E3J1.824E5).
   //
   return FloatCell::need_scaling(value.cval[0], pctx.get_PP());
}
//────────────────────────────────────────────────────────────────────────────
bool
ComplexCell::get_near_bool()  const   
{
   if (value.cval[1] >  INTEGER_TOLERANCE)   DOMAIN_ERROR;
   if (value.cval[1] < -INTEGER_TOLERANCE)   DOMAIN_ERROR;

   if (value.cval[0] > INTEGER_TOLERANCE)   // 1 or invalid
      {
        if (value.cval[0] < (1.0 - INTEGER_TOLERANCE))   DOMAIN_ERROR;
        if (value.cval[0] > (1.0 + INTEGER_TOLERANCE))   DOMAIN_ERROR;
        return true;
      }
   else
      {
        if (value.cval[0] < -INTEGER_TOLERANCE)   DOMAIN_ERROR;
        return false;
      }
}
//────────────────────────────────────────────────────────────────────────────
APL_Integer
ComplexCell::get_near_int() const
{
   if (value.cval[1] >  INTEGER_TOLERANCE)   DOMAIN_ERROR;
   if (value.cval[1] < -INTEGER_TOLERANCE)   DOMAIN_ERROR;

const APL_Float val = value.cval[0];
const APL_Float result = round(val);
const APL_Float diff = val - result;
   if (diff > INTEGER_TOLERANCE)    DOMAIN_ERROR;
   if (diff < -INTEGER_TOLERANCE)   DOMAIN_ERROR;

   if (result > 0.0)   return   APL_Integer(result + 0.3);
   else                return - APL_Integer(0.3 - result);
}
//════════════════════════════════════════════════════════════════════════════
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_add_cc(Cell * Z, APL_Complex a, APL_Complex b)
{
const APL_Complex z = a + b;
   if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
   if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_subtract_cc(Cell * Z, APL_Complex a, APL_Complex b)
{
const APL_Complex z = a - b;
   if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
   if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_multiply_cc(Cell * Z, APL_Complex a, APL_Complex b)
{
const APL_Complex z = a * b;
   if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
   if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_divide_cc(Cell * Z, APL_Complex a, APL_Complex b)
{
   if (b.real() == 0.0 && b.imag() == 0.0)
      {
        if (a.real() != 0.0)   return E_DOMAIN_ERROR;
        if (a.imag() != 0.0)   return E_DOMAIN_ERROR;
        return IntCell::z1(Z);
      }

   // Smith's algorithm (avoids squaring b's components, unlike the naive
   // formula and unlike libstdc++'s std::complex<>::operator/() for some
   // versions/platforms) - confirmed directly: with the naive/library
   // division, (1E10J1E10) ÷ (1E¯300J0) silently underflowed to 0J0
   // instead of overflowing (the true quotient, ~1E310, does not fit in
   // a double either way, but the wrong answer was a finite 0, not Inf).
   //
   // Prescale b by s = 2⋆⌊2⍟max(|Re b|, |Im b|) (similar to
   // bif_reciprocal_c()): otherwise den below overflows to ∞ for e.g.
   // 1÷1E308J1E308, and finite÷∞ silently gives 0 (Blake McBride, Bugs32
   // #6). With the larger component of b' = b÷s in [1, 2), den is in
   // [1, 4), |a÷b'| ≤ |a|, and the final ÷s overflows or underflows only if
   // the true quotient does. A power of 2 makes the scaling exact, so that
   // ordinary quotients are not changed at all.
   //
const APL_Float s = ldexp(1.0, ilogb(fabs(b.real()) >= fabs(b.imag())
                                     ? b.real() : b.imag()));
const APL_Complex b1(b.real() / s, b.imag() / s);
APL_Complex z;
   if (fabs(b1.real()) >= fabs(b1.imag()))
      {
        const APL_Float r   = b1.imag() / b1.real();
        const APL_Float den = b1.real() + r*b1.imag();
        z = APL_Complex(scaled_div(a.real() + r*a.imag(), den, s),
                        scaled_div(a.imag() - r*a.real(), den, s));
      }
   else
      {
        const APL_Float r   = b1.real() / b1.imag();
        const APL_Float den = b1.imag() + r*b1.real();
        z = APL_Complex(scaled_div(a.real()*r + a.imag(), den, s),
                        scaled_div(a.imag()*r - a.real(), den, s));
      }

   if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
   if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_power_ci(Cell * Z, APL_Complex a, APL_Integer b)
{
   // A is really real, merely wrapped in a ComplexCell (Bugs30 #5, e.g.
   // ¯1J0⋆N for a huge integer N): delegate to FloatCell::bif_power_fi(),
   // which computes this exactly (preserving parity for a negative real
   // base) for every magnitude of the integer exponent b. The general
   // complex_power() = exp(b×ln a) path further down instead carries
   // b×π as an intermediate, whose rounding error grows with b and can
   // corrupt even an exactly ±1 real result into unit-circle noise once
   // b exceeds the moderate repeated-squaring threshold below.
   //
   if (a.imag() == 0.0)   return FloatCell::bif_power_fi(Z, a.real(), b);

   // -b below would overflow for the int64 minimum -2⋆63, and the squaring
   // loop then never ended: a⋆-2⋆63 is (÷a) squared 63 times (Bugs38 #44)
   //
   if (b == std::numeric_limits<APL_Integer>::min())
      {
        if (a == APL_Complex(0.0, 0.0))   return E_DOMAIN_ERROR;
        APL_Complex z = APL_Complex(1.0, 0.0) / a;
        loop(s, 63)   z *= z;
        if (!isfinite(z.real()) || !isfinite(z.imag()))   return E_DOMAIN_ERROR;
        return ComplexCell::zC(Z, z);
      }

const bool invert_Z = b < 0;
   if (invert_Z)   b = -b;

   if (b == 0)   return IntCell::z1(Z);

   // Bugs31 #10 (Blake McBride): a is an exact fourth root of unity
   // (0J1 or 0J¯1) -- repeated squaring below is only exact up to
   // b==1024, and complex_power()=exp(b×ln a) further down carries
   // rounding noise in b×π/2 that grows with b, corrupting even an
   // exactly representable result once b crosses that cutoff. a⋆b
   // cycles through {1, 0J1, ¯1, 0J¯1} (or the mirror-image cycle for
   // a=0J¯1) with period 4, computable exactly from b mod 4 for any
   // magnitude; a having unit modulus also makes a⋆¯N simply the
   // conjugate of a⋆N.
   //
   if (a.real() == 0.0 && (a.imag() == 1.0 || a.imag() == -1.0))
      {
        APL_Integer bb = b % 4;
        if (a.imag() == -1.0)   bb = (4 - bb) % 4;
        APL_Complex zi;
        switch (bb)
           {
             case 0:  zi = APL_Complex( 1.0,  0.0);   break;
             case 1:  zi = APL_Complex( 0.0,  1.0);   break;
             case 2:  zi = APL_Complex(-1.0,  0.0);   break;
             default: zi = APL_Complex( 0.0, -1.0);   break;
           }
        if (invert_Z)   zi = APL_Complex(zi.real(), -zi.imag());
        return ComplexCell::zC(Z, zi);
      }

   if (b == 1)
      {
        if (!invert_Z)   return ComplexCell::zC(Z, a);

        // bif_reciprocal_c(), not denom = a.real()²+a.imag()² here
        // (Bugs30 #8): that intermediate |a|² overflows to inf for a
        // huge |a| (beyond ~1E154) even though the true reciprocal is
        // perfectly finite (≈0), so dividing by it silently gave an
        // incorrect exact 0 instead of DOMAIN-ERRORing or (as here)
        // computing the tiny-but-nonzero correct answer;
        // bif_reciprocal_c() already avoids that via Smith's algorithm.
        //
        return ComplexCell::bif_reciprocal_c(Z, a);
      }

   // Exact repeated multiplication (binary exponentiation by
   // squaring) for a moderate positive integer exponent b, mirroring
   // how IntCell/FloatCell already compute their own integer-exponent
   // powers essentially exactly -- unlike complex_power() (pow() on
   // two complex<APL_Float> operands) just below, whose general
   // exp(b×ln a) formula, needed for a genuinely complex exponent,
   // carries rounding noise (~1E¯16 scale) even for an exactly
   // representable integer-exponent result, e.g. (3J4*2) should be
   // exactly ¯7J24, not ¯7J24 plus noise. Bounded to a moderate b (as
   // opposed to unconditionally, the way IntCell/FloatCell do it) so
   // this cannot reintroduce the overflow-before-inversion bug #23
   // fixed just below for a large exponent: repeated squaring of a
   // complex value can overflow to inf partway through even when the
   // final inverted result (A⋆¯N) would be finite. See Bugs27 #59(a).
   //
   if (b <= 1024)
      {
        APL_Complex zi(1.0, 0.0);
        APL_Complex a_2_n = a;
        for (APL_Integer b1 = b; b1; b1 >>= 1)
            {
              if (b1 & 1)   zi *= a_2_n;
              if (b1 == 1)   break;
              a_2_n *= a_2_n;
            }

        if (!invert_Z)
           {
             if (!isfinite(zi.real()))   return E_DOMAIN_ERROR;
             if (!isfinite(zi.imag()))   return E_DOMAIN_ERROR;
             return ComplexCell::zC(Z, zi);
           }

        // Bugs28 #100(o): if the squaring loop above already overflowed
        // zi to infinity (e.g. a=1E200J0, b=2: (1E200)² = 1E400), then
        // inverting it here (denom = |zi|², itself also infinite) gives
        // inf/inf = NaN, DOMAIN-ERRORing even though the true result
        // A⋆¯N is finite (here ≈0) -- fall through to the same
        // negative-exponent-direct-to-complex_power() path used for
        // b>1024 below (Bugs27 #23) instead of returning here.
        //
        if (isfinite(zi.real()) && isfinite(zi.imag()))
           {
             const APL_Float denom = zi.real()*zi.real()
                                    + zi.imag()*zi.imag();
             if (denom == 0.0)   return E_DOMAIN_ERROR;
             zi = APL_Complex(zi.real()/denom, -zi.imag()/denom);

             if (isfinite(zi.real()) && isfinite(zi.imag()))
                return ComplexCell::zC(Z, zi);
             // else: fall through to complex_power() below.
           }
      }

   // Pass the ORIGINAL (negative, when invert_Z) exponent to
   // complex_power() directly rather than computing complex_power(a,b)
   // and then inverting via denom = |z|^2: that intermediate power alone
   // can already overflow to inf for a large b (b was negated to
   // positive above), rejecting with DOMAIN ERROR even when the true
   // result (A⋆¯N = 1/A⋆N) is representable (possibly ≈0, but finite).
   // See Bugs27 #23 (mirrors the same fix in IntCell/FloatCell).
   //
const APL_Complex z = complex_power(a,
                          APL_Complex(invert_Z ? -APL_Float(b) : APL_Float(b),
                                      0.0));
   if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
   if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_power_cc(Cell * Z, APL_Complex a, APL_Complex b)
{
   if (a.real() == 0.0 && a.imag() == 0.0)
      {
        // 0⋆B is 1 only for B *exactly* 0J0 (Bugs28 #84): checking only
        // b.real() == 0.0 also matched any purely-imaginary B (e.g.
        // 0J1), for which 0⋆B is not 1 -- 0 raised to a non-zero power
        // with a non-positive real part is undefined (the b.real() > 0
        // case just below is the only one where 0⋆B is finite and 0).
        //
        if (b.real() == 0.0 && b.imag() == 0.0)   return IntCell::z1(Z);
        if (b.real()  > 0.0)   return IntCell::z0(Z);
        return E_DOMAIN_ERROR;
      }

   // A and B are both really real numbers, merely wrapped in
   // ComplexCells (e.g. 2J0⋆52J0, or a real A with a FloatCell-typed B
   // promoted to complex for the call like ¯1J0⋆1E14): delegate to
   // FloatCell::bif_power_ff(), which already knows how to compute
   // this exactly for every magnitude of a real B -- bif_power_fi()'s
   // exact repeated squaring for a moderate integral B, and the
   // even-magnitude trick of Bugs28 #39 for a huge one (≥ 2⋆53, where
   // bif_power_ci() below would itself fall through to the same
   // approximate complex_power() this whole fix exists to avoid) --
   // without ever going through complex exp/log at all. See Bugs27
   // #24/#59(a): the #24 fix's exact-integer path was reachable only
   // for an IntCell exponent, never for a complex-typed A or B.
   //
   if (a.imag() == 0.0 && b.imag() == 0.0)
      return FloatCell::bif_power_ff(Z, a.real(), b.real());

   // A is genuinely complex (non-zero imaginary part) and B is really
   // an integer: route through the exact binary-exponentiation path
   // bif_power_ci() instead of complex_power() = exp(b×ln a) below,
   // whose result is only ever approximate even for an integral B.
   //
   // is_near_int64_t(), not is_near_int(): the latter also accepts
   // magnitudes beyond int64 range, which bif_power_ci() (an
   // APL_Integer b) cannot represent -- see the matching comment in
   // FloatCell::bif_power_ff().
   //
   if (b.imag() == 0.0 && Cell::is_near_int64_t(b.real()) &&
       b.real() == nearbyint(b.real()))
      return ComplexCell::bif_power_ci(Z, a, APL_Integer(b.real()));

   // a purely imaginary a and a real b: (0J±m)⋆b = (m⋆b) × ¯1⋆(±b÷2),
   // like FloatCell::bif_power_ff() for a negative real a: the phase
   // b×○÷2 of exp(b×ln a) is noise for a large b (Bugs38 #48).
   //
   if (a.real() == 0.0 && b.imag() == 0.0)
      {
        const APL_Float t = (a.imag() > 0.0 ? 0.5 : -0.5) * b.real();
        const APL_Complex z = pow(fabs(a.imag()), b.real()) *
                    complex_power(APL_Complex(-1.0, 0.0), fmod(t, 2.0));
        if (!isfinite(z.real()) || !isfinite(z.imag()))   return E_DOMAIN_ERROR;
        return ComplexCell::zC(Z, z);
      }

const APL_Complex z = complex_power(a, b);
   if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
   if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_logarithm_cc(Cell * Z, APL_Complex a, APL_Complex b)
{
   // a = base (A), b = argument (B = this)
   //
   // Base validity (a == 0 or a == 1) must be checked before the b==a
   // short-circuit below -- see FloatCell::bif_logarithm_ff() for the
   // real-cell version of the same bug (Blake McBride, Bugs26 #2).
   if (a.real() == 0.0 && a.imag() == 0.0)   return E_DOMAIN_ERROR;

   // ⎕CT-tolerant equality to 1J0, not a fixed absolute
   // INTEGER_TOLERANCE on each component independently (Bugs28 #87,
   // same rationale as FloatCell::bif_logarithm_ff()).
   //
   // a base of 1 is valid only for B = 1 (APL2 LRM p. 171, Bugs38 #122)
   //
   if (Cell::tolerantly_equal(a, APL_Complex(1.0, 0.0), Workspace::get_CT()))
      {
        if (Cell::tolerantly_equal(b, APL_Complex(1.0, 0.0),
                                   Workspace::get_CT()))
           return IntCell::z1(Z);
        return E_DOMAIN_ERROR;
      }
   if (b == a)   return IntCell::z1(Z);
   if (b.real() == 0.0 && b.imag() == 0.0)   return E_DOMAIN_ERROR;

   if (a.imag() == 0.0)
      {
        // log(a.real()) -- the REAL log() -- is NaN for a negative
        // base, which then failed the isfinite() check just below and
        // rejected with DOMAIN ERROR even though A⍟B is well-defined
        // (and gives a finite complex result) for a negative real A,
        // e.g. ¯2⍟8. log(APL_Complex(a.real(),0.0)) -- the COMPLEX
        // log() -- takes the negative-real branch cut correctly, the
        // same way the a.imag()!=0.0 general case just below already
        // does for e.g. ¯2⍟8J1. See Bugs28 #41.
        //
        const APL_Complex z = log(b) / log(APL_Complex(a.real(), 0.0));
        if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
        if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
        return ComplexCell::zC(Z, z);
      }

const APL_Complex z = log(b) / log(a);
   if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
   if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
// complex near-real test: |imag| is small relative to |real|
static inline bool
nc_near_real(APL_Complex c)
{
   // fabs(), not I2=imag², R2=real² (Bugs30 #6 sibling, same fix as
   // ComplexCell::is_near_real()): squaring first underflows both to 0
   // for a tiny but comparable-magnitude imag/real pair, which then
   // wrongly took the "absolutely small" shortcut below even though
   // imag is NOT negligible relative to real.
   //
   if (fabs(c.imag()) < REAL_TOLERANCE)   return true;   // absolutely small
   return fabs(c.imag()) < REAL_TOLERANCE * fabs(c.real());   // relatively small
}
//────────────────────────────────────────────────────────────────────────────
/// true if a is greater than b in the exact order of ⌈ and ⌊: the real part
/// of a is greater than that of b, or else (equal real parts) the
/// imaginary part. Exactly, like the real ⌈ (e.g. 1⌈1+1E¯14), so that the
/// order is transitive (Bugs39 #56). Tolerantly equal values were never
/// greater, which made ⌈/ depend on the order of its items (Bugs40 #54).
static bool
complex_greater(APL_Complex a, APL_Complex b)
{
   if (a.real() != b.real())   return a.real() > b.real();
   return a.imag() > b.imag();
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::max_min_cc(Cell * Z, const Cell & A, const Cell & B, bool max)
{
   // A⌈B and A⌊B select by the same order as < (Bugs38 #124), also for
   // near-real values like 5J1E¯12 (Bugs39 #93), and return the selected
   // argument exactly, e.g. an IntCell 9007199254740993 (Bugs39 #41)
   //
   // the real parts exactly: an IntCell beyond 2⋆53 is not a double, e.g.
   // 9007199254740993 vs. 9007199254740992J1, like ⍋ (Bugs41 #49)
   //
int cr;   // the sign of real(A) - real(B)
   if (A.is_integer_cell() && B.is_integer_cell())
      cr = (A.get_int_value() > B.get_int_value()) -
           (A.get_int_value() < B.get_int_value());
   else if (A.is_integer_cell())
      cr =  Cell::compare_int_float(A.get_int_value(), B.get_real_value());
   else if (B.is_integer_cell())
      cr = -Cell::compare_int_float(B.get_int_value(), A.get_real_value());
   else
      cr = (A.get_real_value() > B.get_real_value()) -
           (A.get_real_value() < B.get_real_value());

const APL_Complex a = A.get_complex_value();
const APL_Complex b = B.get_complex_value();
const bool A_greater = cr ? cr > 0 : complex_greater(a, b);
const bool B_greater = cr ? cr < 0 : complex_greater(b, a);
const bool B_selected = max ? B_greater : A_greater;
const Cell & selected = B_selected ? B : A;

   if (selected.is_integer_cell())
      return IntCell::zI(Z, selected.get_int_value());
   if (selected.get_imag_value() == 0.0)
      return FloatCell::zF(Z, selected.get_real_value());
   return ComplexCell::zC(Z, selected.get_complex_value());
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_residue_cc(Cell * Z, APL_Complex a, APL_Complex b)
{
   if (a.real() == 0.0 && a.imag() == 0.0)   return ComplexCell::zC(Z, b);
   if (b.real() == 0.0 && b.imag() == 0.0)   return IntCell::z0(Z);

   // both operands are really real, merely wrapped in ComplexCells
   // (Bugs30 #10, e.g. 1E18|¯1J0): delegate to FloatCell::bif_residue_ff()
   // instead of the hand-rolled complex-floor selection below, whose
   // "which Gaussian neighbour" heuristic picks the wrong one when the
   // real quotient's fractional part rounds to exactly 1.0 (e.g.
   // quot≈¯1E¯18: fr=¯1, Dr=1.0 exactly, Di=0, so none of the >/< tests
   // match and the final else wrongly picks fr+1=0 instead of ¯1).
   //
   if (a.imag() == 0.0 && b.imag() == 0.0)
      return FloatCell::bif_residue_ff(Z, a.real(), b.real());

const APL_Complex quot = b / a;
const double qct = Workspace::get_CT();

   // a quotient that is a non-zero Gaussian integer up to the rounding of
   // a, b, and b÷a gives 0, like Cell::rounded_integral() on the real path
   // (Bugs38 #47), e.g. 1E¯5|0J1 (Bugs39 #34)
   //
   // beyond 2⋆52 the floor of the quotient is exact but b - a×floor(b÷a)
   // is pure cancellation noise (like FloatCell::bif_residue_ff(), Bugs40
   // #98), e.g. 0J1E¯5|0J1E15 was 0J¯0.125 at ⎕CT←0
   //
   // But per component: an exact residue in a small component is kept,
   // e.g. 1|1000.5J5000000000000000 at ⎕CT←0 (Bugs41 #68)
   //
   {
     const APL_Float qr = fabs(quot.real());
     const APL_Float qi = fabs(quot.imag());
     if ((qr > 4.5E15 || qr == 0.0) && (qi > 4.5E15 || qi == 0.0))
        return IntCell::z0(Z);
   }

   if (qct != 0)
      {
        const APL_Float nr = nearbyint(quot.real());
        const APL_Float ni = nearbyint(quot.imag());
        const APL_Float tolerance = 2 * DBL_EPSILON * abs(quot);
        if ((nr != 0.0 || ni != 0.0)              &&
            fabs(quot.real() - nr) <= tolerance   &&
            fabs(quot.imag() - ni) <= tolerance)   return IntCell::z0(Z);

        // a quotient that is a Gaussian integer within ⎕CT, like
        // Cell::integral_within() on the real path, e.g. 3|2.9999999999999J3
        // (instead of B being tolerantly equal to A×⌊B÷A, Bugs40 #45)
        //
        // A nearby Gaussian integer 0 is never a match, like on the real
        // path, e.g. 1E14|1J1 is 1J1 (Bugs41 #21)
        //
        if ((nr != 0.0 || ni != 0.0) &&
            fabs(quot.real() - nr) < qct &&
            fabs(quot.imag() - ni) < qct)   return IntCell::z0(Z);
      }

   // complex floor of quot
   //
APL_Float fr = floor(quot.real());
APL_Float Dr = quot.real() - fr;
APL_Float fi = floor(quot.imag());
APL_Float Di = quot.imag() - fi;
const double limit = 1.0 - qct;
   if (Dr > limit)   { fr += 1.0;   Dr = 0.0; }
   if (Di > limit)   { fi += 1.0;   Di = 0.0; }

APL_Complex floor_quot;
   if ((Dr + Di) < limit)     floor_quot = APL_Complex(fr,        fi);
   else if (Dr < (Di - qct))  floor_quot = APL_Complex(fr,        fi + 1.0);
   else                       floor_quot = APL_Complex(fr + 1.0,  fi);

const APL_Complex a_floor_quot = a * floor_quot;

   // if b and a×floor_quot are themselves ⎕CT-tolerantly equal, the
   // "true" residue is exactly 0 and b - a×floor_quot is pure
   // floating-point cancellation noise on the order of ⎕CT×|b| -- not
   // caught by comparing that noise against 0 or against a afterward
   // (FloatCell::bif_residue_ff()'s r2==null/r2==a snap, translated
   // literally), since tolerantly_equal(noise, 0, qct) is vacuously
   // false (comparing against an exact-zero scale) and the noise is
   // nowhere near a's own magnitude either. Comparing the two
   // *operands* of the subtraction against each other, at their own
   // natural scale, is what ISO's tolerant equality is for (Bugs28
   // #86): e.g. 0J1E¯11|1 gave ~1.11E¯16 instead of 0.
   //
   // The noise is a few ulps of |b| though, not ⎕CT×|b|: that discarded
   // every genuine residue below |b|÷1E13, e.g. 0J10|0J10000000000001 was
   // 0 (Bugs40 #45). A quotient near a Gaussian integer is handled above.
   //
APL_Complex residue = b - a_floor_quot;

   // per component (Bugs41 #68): the noise of a component is the rounding
   // of that component of a×⌊b÷a, which fma() gives exactly. An exact
   // product (e.g. for a real a and a large quotient) has no noise, so that
   // e.g. 1|1000.5J5000000000000000 is 0.5 at ⎕CT←0, while the rounding of
   // 0J1E¯5×1E20 in 0J1E¯5|0J1E15 is noise (Bugs40 #98).
   //
   {
     const APL_Float ar = a.real(),           ai = a.imag();
     const APL_Float fr = floor_quot.real(),  fi = floor_quot.imag();
     const APL_Float p_rr = ar*fr, p_ii = ai*fi, p_ri = ar*fi, p_ir = ai*fr;
     // the rounding of the 4 products, and of their sums (if both nonzero)
     const APL_Float err_r = fabs(fma(ar, fr, -p_rr)) + fabs(fma(ai, fi, -p_ii))
                + ((p_rr != 0.0 && p_ii != 0.0) ? DBL_EPSILON * fabs(p_rr - p_ii)
                                                : 0.0);
     const APL_Float err_i = fabs(fma(ar, fi, -p_ri)) + fabs(fma(ai, fr, -p_ir))
                + ((p_ri != 0.0 && p_ir != 0.0) ? DBL_EPSILON * fabs(p_ri + p_ir)
                                                : 0.0);
     APL_Float rr = residue.real();
     APL_Float ri = residue.imag();
     if (fabs(rr) <= 4 * err_r)   rr = 0.0;
     if (fabs(ri) <= 4 * err_i)   ri = 0.0;
     if (rr == 0.0 && ri == 0.0)   return IntCell::z0(Z);
     residue = APL_Complex(rr, ri);
   }

const ErrorCode ret = ComplexCell::zC(Z, residue);
   // the sibling bif_circle_fun_c() right below already does this check
   // (via the same Z->is_finite() idiom); this residue function had no
   // finiteness check on its quotient/result at all.
   if (!Z->is_finite())   return E_DOMAIN_ERROR;
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_circle_fun_c(Cell * Z, APL_Integer fun, APL_Complex b)
{
   IntCell::z0(Z);
const ErrorCode ret = ComplexCell::do_bif_circle_fun(Z, fun, b);
   if (!Z->is_finite())   return E_DOMAIN_ERROR;
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_circle_fun_inverse_c(Cell * Z, APL_Integer fun, APL_Complex b)
{
   IntCell::z0(Z);

ErrorCode ret = E_DOMAIN_ERROR;
   switch(fun)
      {
        case  1: case -1:
        case  2: case -2:
        case  3: case -3:
        case  4: case -4:
        case  5: case -5:
        case  6: case -6:
        case  7: case -7:
                 ret = ComplexCell::do_bif_circle_fun(Z, -fun, b);
                 if (!Z->is_finite())   return E_DOMAIN_ERROR;
                 return ret;

        case -10:
                 ret = ComplexCell::do_bif_circle_fun(Z, fun, b);
                 if (!Z->is_finite())   return E_DOMAIN_ERROR;
                 return ret;

        default: return E_DOMAIN_ERROR;
      }

   return E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
bool
ComplexCell::gaussian_near(APL_Complex b, double qct, APL_Complex & N)
{
   // return true if b is tolerantly equal (relative, like =) to a non-zero
   // Gaussian integer N. Then ⌊b and ⌈b are N, e.g. ⌊(1÷1E¯5)+0J1 is
   // 100000J1, not 99999J1 from the absolute window 1-⎕CT (Bugs40 #30).
   //
   N = APL_Complex(nearbyint(b.real()), nearbyint(b.imag()));
   return N != APL_Complex(0.0, 0.0) && Cell::tolerantly_equal(b, N, qct);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_ceiling_c(Cell * Z, APL_Complex b)
{
   // mirrors bif_floor_c()'s ⎕CT-tolerant snapping below, which this
   // omitted entirely: a raw ceil() jumps a full unit for any value even
   // a hair above an integer (e.g. 3.000000000000001), instead of
   // snapping back down to that integer the way tolerant floor() does
   // for a value a hair below one. ⌈3.000000000000001J2 gave 4J2 instead
   // of the tolerant 3J2.
   // a zero imaginary part: like the real ⌈ (e.g. ⌈1E15÷3J0, Bugs40 #30)
   //
   if (b.imag() == 0.0)   return FloatCell::bif_ceiling_f(Z, b.real());

const double qct = Workspace::get_CT();
APL_Complex N;
   if (gaussian_near(b, qct, N))
      return ComplexCell::zV(Z, N.real(), N.imag());

APL_Float cr = ceil(b.real());
APL_Float Dr = cr - b.real();
APL_Float ci = ceil(b.imag());
APL_Float Di = ci - b.imag();
const double limit = 1.0 - qct;
   if (Dr > limit)   { cr -= 1.0;   Dr = 0.0; }
   if (Di > limit)   { ci -= 1.0;   Di = 0.0; }
   if ((Dr + Di) < limit)   return ComplexCell::zV(Z, cr, ci);
   if (Dr < (Di - qct))     return ComplexCell::zV(Z, cr, ci - 1.0);
   return ComplexCell::zV(Z, cr - 1.0, ci);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_floor_c(Cell * Z, APL_Complex b)
{
   // a zero imaginary part: like the real ⌊ (Bugs40 #30)
   //
   if (b.imag() == 0.0)   return FloatCell::bif_floor_f(Z, b.real());

const double qct = Workspace::get_CT();
APL_Complex N;
   if (gaussian_near(b, qct, N))
      return ComplexCell::zV(Z, N.real(), N.imag());

APL_Float fr = floor(b.real());
APL_Float Dr = b.real() - fr;
APL_Float fi = floor(b.imag());
APL_Float Di = b.imag() - fi;
const double limit = 1.0 - qct;
   if (Dr > limit)   { fr += 1.0;   Dr = 0.0; }
   if (Di > limit)   { fi += 1.0;   Di = 0.0; }
   if ((Dr + Di) < limit)    return ComplexCell::zV(Z, fr, fi);
   if (Dr < (Di - qct))      return ComplexCell::zV(Z, fr, fi + 1.0);
   return ComplexCell::zV(Z, fr + 1.0, fi);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_conjugate_c(Cell * Z, APL_Complex b)
{
   return ComplexCell::zC(Z, conj(b));
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_direction_c(Cell * Z, APL_Complex b)
{
   // scale b by its larger component first: abs(b) overflows for huge
   // components and is coarse for subnormal ones, but the direction always
   // has magnitude 1 (Bugs38 #46)
   //
const APL_Float scale = std::max(fabs(b.real()), fabs(b.imag()));
   if (scale == 0.0)   return IntCell::z0(Z);
const APL_Float br = b.real() / scale;
const APL_Float bi = b.imag() / scale;
const APL_Float mag = hypot(br, bi);
   return ComplexCell::zC(Z, br/mag, bi/mag);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_exponential_c(Cell * Z, APL_Complex b)
{
const APL_Complex z = complex_exponent(b);
   if (!isfinite(z.real()) || !isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_factorial_c(Cell * Z, APL_Complex b)
{
   if (nc_near_real(b))   return FloatCell::bif_factorial_f(Z, b.real());

ErrorCode ret = ComplexCell::zC(Z, ComplexCell::gamma(b.real() + 1.0,
                                                       b.imag()));
   if (errno)   return E_DOMAIN_ERROR;
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_magnitude_c(Cell * Z, APL_Complex b)
{
const APL_Float z = abs(b);
   if (!isfinite(z))   return E_DOMAIN_ERROR;   // Bugs38 #46
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_nat_log_c(Cell * Z, APL_Complex b)
{
   // reject exactly 0J0 (log(0) is -infinity), mirroring
   // bif_reciprocal_c() below (Blake McBride, Bugs23 #3a -- ⍟0J0 gave
   // ¯∞ instead of DOMAIN ERROR, same as the real ⍟0/⍟0.0 cases already
   // reject). A tolerance-based "near zero" test used to reject this
   // exact-zero case together with any b that merely has both parts
   // individually smaller than ⎕CT's absolute floor -- but a value such
   // as 0J1E¯11 is not mathematically zero and log() of it is a
   // perfectly finite complex number (Bugs28 #77); isfinite() below
   // already catches the genuine -infinity/NaN cases (0J0 included),
   // so the upfront check only needs to short-circuit the true log(0).
   //
   if (b.real() == 0.0 && b.imag() == 0.0)   return E_DOMAIN_ERROR;

const APL_Complex z = log(b);
   if (!isfinite(z.real()) || !isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_negative_c(Cell * Z, APL_Complex b)
{
   return ComplexCell::zC(Z, -b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_pi_times_c(Cell * Z, APL_Complex b)
{
const APL_Float pi(M_PI);
const APL_Float re = b.real() * pi;
const APL_Float im = b.imag() * pi;
   // same overflow guard as FloatCell::bif_pi_times_f() (○1E308 gave ∞
   // there too, before that fix) -- Blake McBride, Bugs23 #3b.
   if (!isfinite(re) || !isfinite(im))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, re, im);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_pi_times_inverse_c(Cell * Z, APL_Complex b)
{
const APL_Float pi(M_PI);
const APL_Float re = b.real() / pi;
const APL_Float im = b.imag() / pi;
   if (!isfinite(re) || !isfinite(im))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, re, im);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_reciprocal_c(Cell * Z, APL_Complex b)
{
   // reject exactly 0J0 (division by zero); see bif_nat_log_c() above
   // for why a tolerance-based "near zero" test is wrong here -- a
   // value such as 0J1E¯11 is not mathematically zero and has a
   // perfectly finite reciprocal (Bugs28 #77). The isfinite() checks
   // further down already catch genuine overflow, including the true
   // 0J0 division-by-zero case.
   //
   if (b.real() == 0.0 && b.imag() == 0.0)   return E_DOMAIN_ERROR;

   // exactly real b (Bugs30 #6): compute 1/b.real() directly instead of
   // falling into the R2/I2 shortcut below, whose own condition needs
   // R2 = b.real()² to decide whether to take it -- for tiny |b.real()|
   // (e.g. 1E¯200) that square itself underflows to 0, so R2*B2 is also
   // 0, the "I2 < R2*B2" test spuriously fails (0 < 0), and the general
   // formula further down (also built on R2+I2) then divides by an
   // underflowed-to-0 denominator, wrongly DOMAIN-ERRORing even though
   // the true reciprocal (1E200) is perfectly finite.
   //
   if (b.imag() == 0.0)
      {
        const APL_Float z = 1.0 / b.real();
        if (!isfinite(z))   return E_DOMAIN_ERROR;
        return FloatCell::zF(Z, z);
      }

   // near-real: use simpler formula, but only when the imaginary part is
   // negligible *relative to* the real part -- the old absolute-only
   // "I2 < B2" disjunct fired whenever the imaginary part alone was
   // tiny, even with an exactly-zero (or comparably tiny) real part, so
   // e.g. 0J1E¯11 took this "real" shortcut and computed 1.0/0.0 (+∞,
   // DOMAIN ERROR) instead of falling through to the general formula
   // below, which handles a tiny-or-zero real part correctly (Bugs28
   // #77). Dropping that disjunct only narrows which values take the
   // (numerically cheaper) shortcut; every value still gets a
   // mathematically equivalent, finite result via one path or the other.
   //
   // fabs(imag) < tol*fabs(real), not I2=imag²,R2=real² (Bugs30 #6
   // sibling): squaring first underflows both to 0 for a tiny but
   // comparable-magnitude imag/real pair, wrongly taking this shortcut
   // (and its 1.0/b.real()) even when imag is NOT negligible relative
   // to real.
   //
   if (fabs(b.imag()) < REAL_TOLERANCE*fabs(b.real()))
      {
        const APL_Float z = 1.0 / b.real();
        if (!isfinite(z))   return E_DOMAIN_ERROR;
        return FloatCell::zF(Z, z);
      }

   // Smith's algorithm, not r=b.real()/(R2+I2), i=b.imag()/(R2+I2)
   // (Bugs30 #6): the intermediate |b|² = R2+I2 above overflows for a
   // huge |b| and underflows to 0 for a tiny one, in both cases wrongly
   // DOMAIN-ERRORing even though the true reciprocal is finite; dividing
   // by the larger-magnitude component first keeps every intermediate
   // bounded -- except that "bounded" still meant "as large as
   // fabs(b.real())+fabs(b.imag())" (den, just below), which itself
   // overflows once |b.real()|≈|b.imag()| both approach DBL_MAX/2
   // (Bugs31 #11, Blake McBride), even though the true reciprocal is a
   // perfectly representable (possibly subnormal) finite value. Prescale
   // both components by their common larger magnitude first: this keeps
   // every intermediate -- including den -- bounded by O(1) regardless
   // of |b|'s own magnitude, at the cost of one extra division on the
   // way back out.
   //
const APL_Float s = fabs(b.real()) >= fabs(b.imag()) ? fabs(b.real())
                                                      : fabs(b.imag());
const APL_Float br = b.real() / s;
const APL_Float bi = b.imag() / s;
APL_Float r, i;
   if (fabs(br) >= fabs(bi))
      {
        const APL_Float t = bi / br;
        const APL_Float den = br + bi*t;
        r =  1.0 / den / s;
        i = -t   / den / s;
      }
   else
      {
        const APL_Float t = br / bi;
        const APL_Float den = bi + br*t;
        r =  t   / den / s;
        i = -1.0 / den / s;
      }
   if (!isfinite(r))   return E_DOMAIN_ERROR;
   if (!isfinite(i))   return E_DOMAIN_ERROR;
   return ComplexCell::zC(Z, r, i);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_roll_c(Cell * Z, APL_Complex b)
{
   if (!Cell::is_near_int(b.real()))   return E_DOMAIN_ERROR;
   if (!Cell::is_near_int(b.imag()))   return E_DOMAIN_ERROR;
   if (nearbyint(b.imag()) != 0.0)     return E_DOMAIN_ERROR;
const APL_Integer set_size = APL_Integer(nearbyint(b.real()));
   if (set_size <= 0)   return E_DOMAIN_ERROR;
const uint64_t rnd = Workspace::get_RL(set_size);
   return IntCell::zI(Z, Workspace::get_IO() + APL_Integer(rnd % set_size));
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_near_int64_t_c(Cell * Z, APL_Complex b)
{
   if (!Cell::is_near_int64_t(b.real()))   return E_DOMAIN_ERROR;
   if (!Cell::is_near_int64_t(b.imag()))   return E_DOMAIN_ERROR;

   if (b.imag() <  INTEGER_TOLERANCE &&
       b.imag() > -INTEGER_TOLERANCE &&
       b.real() <  BIG_INT64_F       &&
       b.real() > -BIG_INT64_F)
      return IntCell::zI(Z, APL_Integer(nearbyint(b.real())));   // Bugs38 #96

   return ComplexCell::zC(Z, round(b.real()), round(b.imag()));
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::bif_within_quad_CT_c(Cell * Z, APL_Complex b)
{
const double val_r = b.real();
   if (val_r > LARGE_INT)   return E_DOMAIN_ERROR;
   if (val_r < SMALL_INT)   return E_DOMAIN_ERROR;
const double val_i = b.imag();
   if (val_i > LARGE_INT)   return E_DOMAIN_ERROR;
   if (val_i < SMALL_INT)   return E_DOMAIN_ERROR;

const double max_diff_r = Workspace::get_CT() * fabs(val_r);
const double max_diff_i = Workspace::get_CT() * fabs(val_i);

const APL_Float val_dn_r = floor(val_r);
const APL_Float val_up_r = ceil(val_r);
const APL_Float val_dn_i = floor(val_i);
const APL_Float val_up_i = ceil(val_i);

   // <= and >=: an exact integer part (e.g. the imaginary part 0) is at
   // distance 0 from itself (Bugs38 #54)
   //
double z_r;
   if      (val_r <= (val_dn_r + max_diff_r))   z_r = val_dn_r;
   else if (val_r >= (val_up_r - max_diff_r))   z_r = val_up_r;
   else                                         return E_DOMAIN_ERROR;

double z_i;
   if      (val_i <= (val_dn_i + max_diff_i))   z_i = val_dn_i;
   else if (val_i >= (val_up_i - max_diff_i))   z_i = val_up_i;
   else                                         return E_DOMAIN_ERROR;

   if (z_i == 0.0)   return IntCell::zI(Z, APL_Integer(z_r));
   return ComplexCell::zC(Z, z_r, z_i);
}
//════════════════════════════════════════════════════════════════════════════
//════════════════════════════════════════════════════════════════════════════
//════════════════════════════════════════════════════════════════════════════
ErrorCode
ComplexCell::zV(Cell * Z, APL_Float flt)
{
   if (!Cell::is_near_int64_t(flt))   return FloatCell::zF(Z, flt);

   return flt < 0 ? IntCell::zI(Z, -int64_t(0.5 - flt))
                  : IntCell::zI(Z,  int64_t(0.5 + flt));
}
//──────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::zV(Cell * Z, APL_Complex cpx)
{
   return Cell::is_near_zero(cpx.imag()) ? ComplexCell::zV(Z, cpx.real())
                                   : ComplexCell::zC(Z, cpx);
}
//──────────────────────────────────────────────────────────────────────────
ErrorCode
ComplexCell::zV(Cell * Z, APL_Float real, APL_Float imag)
{
   return Cell::is_near_zero(imag) ? ComplexCell::zV(Z, real)
                             : ComplexCell::zC(Z, real, imag);
}
//════════════════════════════════════════════════════════════════════════════



