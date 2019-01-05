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

#include <math.h>

#include "Value.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "RealCell.hh"
#include "Workspace.hh"

//-----------------------------------------------------------------------------
ErrorCode
RealCell::bif_logarithm(Cell * Z, const Cell * A) const
{
   // ISO p. 88
   //
   if (!A->is_numeric())   return E_DOMAIN_ERROR;

   if (get_real_value() == A->get_real_value() &&
       A->get_imag_value() == 0.0)   return IntCell::z1(Z);

   if (get_real_value() == 0.0)   return E_DOMAIN_ERROR;
   if (A->is_near_one())          return E_DOMAIN_ERROR;

   if (A->is_real_cell()          &&
       A->get_real_value() >= 0.0 &&
          get_real_value() >= 0.0)
      {
        const APL_Float z = log(get_real_value()) / log(A->get_real_value());
        if (!isfinite(z))   return E_DOMAIN_ERROR;
        return FloatCell::zv(Z, z);
      }

   // complex result (complex B or negative A)
   //
const APL_Complex z = log(get_complex_value()) / log(A->get_complex_value());
   if (!isfinite(z.real()))   return E_DOMAIN_ERROR;
   if (!isfinite(z.imag()))   return E_DOMAIN_ERROR;
   return ComplexCell::zv(Z, z);
}
//-----------------------------------------------------------------------------
ErrorCode
RealCell::bif_circle_fun(Cell * Z, const Cell * A) const
{
   if (!A->is_near_int())   return E_DOMAIN_ERROR;
const APL_Integer fun = A->get_checked_near_int();

   FloatCell::zv(Z, 0);   // prepare for DOMAIN ERROR

const ErrorCode ret = do_bif_circle_fun(Z, fun);
   if (!Z->is_finite())   return E_DOMAIN_ERROR;
   return ret;
}
//-----------------------------------------------------------------------------
ErrorCode
RealCell::bif_circle_fun_inverse(Cell * Z, const Cell * A) const
{
   if (!A->is_near_int())   return E_DOMAIN_ERROR;
const APL_Integer fun = A->get_checked_near_int();

   FloatCell::zv(Z, 0);   // prepare for DOMAIN ERROR

ErrorCode ret = E_DOMAIN_ERROR;
   switch(fun)
      {
        case 1: case -1:
        case 2: case -2:
        case 3: case -3:
        case 4: case -4:
        case 5: case -5:
        case 6: case -6:
        case 7: case -7:
                ret = do_bif_circle_fun(Z, -fun);
                if (!Z->is_finite())   return E_DOMAIN_ERROR;
                return ret;

        case -10:  // +A (conjugate) is self-inverse
                 ret = do_bif_circle_fun(Z, fun);
                if (!Z->is_finite())   return E_DOMAIN_ERROR;
                return ret;

        default: return E_DOMAIN_ERROR;
      }

   // not reached
   return E_DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
ErrorCode
RealCell::do_bif_circle_fun(Cell * Z, int fun) const
{
const APL_Float b = get_real_value();
const APL_Complex cb(b);

   switch(fun)
      {
        case -12: ComplexCell(0, b).bif_exponential(Z);   return E_NO_ERROR;
        case -11: return ComplexCell::zv(Z, 0.0, b );
        case -10: return FloatCell::zv(Z,       b );
        case  -9: return FloatCell::zv(Z,       b );
        case  -8: { const APL_Float par = -(b*b + 1.0);       // (¯1 + R⋆2)
                    if (par < 0.0)   // complex square root
                       {
                         const APL_Float sq = sqrt(-par);     // (¯1 + R⋆2)⋆0.5
                         if (b < 0.0)   return ComplexCell::zv(Z, 0.0, -sq);
                         else           return ComplexCell::zv(Z, 0.0, sq);
                       }
                    else           // real square root
                       {
                         const APL_Float sq = sqrt(par);      // (¯1 + R⋆2)⋆0.5
                         if (b < 0.0)   return FloatCell::zv(Z, -sq);
                         else           return FloatCell::zv(Z, sq);
                       }
                  }

        case  -7: if (b > -1.0 && b < 1.0)   return FloatCell::zv(Z, atanh(b));
                  if (b == -1.0 || b == 1.0)   return E_DOMAIN_ERROR;
                  return ComplexCell::do_bif_circle_fun(Z, -7, cb);

        case  -6: if (b > 1.0)   return FloatCell::zv(Z, acosh(b));
                  return ComplexCell::do_bif_circle_fun(Z, -6, cb);

        case  -5: return FloatCell::zv(Z, asinh(b));
        case  -4: if (b >= -1.0)   return FloatCell::zv(Z, sqrt(b*b - 1.0));
                  return ComplexCell::do_bif_circle_fun(Z, -4, cb);
        case  -3: return FloatCell::zv(Z, atan (b));
        case  -2: if (b >= -1.0 && b <= 1.0)  return FloatCell::zv(Z, acos (b));
                  return ComplexCell::do_bif_circle_fun(Z, -2, cb);
        case  -1: if (b >= -1.0 && b <= 1.0)  return FloatCell::zv(Z, asin (b));
                  return ComplexCell::do_bif_circle_fun(Z, -1, cb);
        case   0: if (b*b < 1.0)   return FloatCell::zv(Z, sqrt (1 - b*b));
                  return ComplexCell::do_bif_circle_fun(Z, 0, cb);
        case   1: return FloatCell::zv(Z,  sin (b));
        case   2: return FloatCell::zv(Z,  cos (b));
        case   3: return FloatCell::zv(Z,  tan (b));
        case   4: return FloatCell::zv(Z, sqrt (1 + b*b));
        case   5: return FloatCell::zv(Z,  sinh(b));
        case   6: return FloatCell::zv(Z,  cosh(b));
        case   7: return FloatCell::zv(Z,  tanh(b));
        case   8: { const APL_Float par = -(b*b + 1.0);       // (¯1 - R⋆2)
                    if (par < 0.0)   // complex square root
                       {
                         const APL_Float sq = sqrt(-par);     // (¯1 + R⋆2)⋆0.5
                         if (b < 0.0)   return ComplexCell::zv(Z, 0.0, sq);
                         else           return ComplexCell::zv(Z, 0.0, -sq);
                       }
                    else           // real square root
                       {
                         const APL_Float sq = sqrt(par);      // (¯1 + R⋆2)⋆0.5
                         if (b < 0.0)   return FloatCell::zv(Z, sq);
                         else           return FloatCell::zv(Z, -sq);
                       }
                  }
        case   9: return FloatCell::zv(Z, b );
        case  10: if (b < 0.0)   return FloatCell::zv(Z, -b);
                  else           return FloatCell::zv(Z,  b);
        case  11: return FloatCell::zv(Z, 0.0 );
        case  12: return FloatCell::zv(Z, (b < 0.0) ? M_PI : 0.0);
      }

   // invalid fun
   //
   return E_DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
