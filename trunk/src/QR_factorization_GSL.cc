/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2023  Dr. Jürgen Sauermann

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

#include "Common.hh"
#include "Cell.hh"
#include "Value.hh"

#if apl_GSL

#include <gsl/gsl_linalg.h>
#include "QR_factorization_GSL.hh"

//-----------------------------------------------------------------------------------------
int
GSL::invert_matrix(gsl_matrix * A, size_t N)
{
  return 0;
}
//-----------------------------------------------------------------------------------------
void
GSL::factorize_DD_matrix(Value & Z, int M, int N, const Cell * cB)
{
  if (M < N)
     {
       MORE_ERROR() << "⌹[3] (aka. QR-factorization using libgsl) expectes M ≥ N, "
                    << "but got M=" << M << " and N=" << N;
       LENGTH_ERROR;
     }

  // init GSL matricx B from APL ravel * cB
  //
gsl_matrix * B = gsl_matrix_alloc(M, N);
  loop(row, M)
  loop(col, N)   gsl_matrix_set(B, row, col, cB++->get_real_value());

gsl_matrix * T = gsl_matrix_alloc(N, N);
  gsl_linalg_QR_decomp_r(B, T);

gsl_matrix * Q = gsl_matrix_alloc(M, M);
gsl_matrix * R = gsl_matrix_alloc(N, N);
  gsl_linalg_QR_unpack_r(B, T, Q, R);

Value_P Z0(M, M, LOC);   // Q
  loop(row, M)
  loop(col, M)   Z0->next_ravel_Number(gsl_matrix_get(Q, row, col));
  Z0->check_value(LOC);

Value_P Z1(N, N, LOC);   // R
  loop(row, N)
  loop(col, N)   Z1->next_ravel_Number(gsl_matrix_get(R, row, col));
  Z1->check_value(LOC);

  // invert R in place
  //
  gsl_linalg_tri_invert(CblasUpper, CblasNonUnit, R);
Value_P Z2(N, N, LOC);   // inverse of R
  loop(row, N)
  loop(col, N)   Z2->next_ravel_Number(gsl_matrix_get(R, row, col));
  Z2->check_value(LOC);

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);

gsl_matrix_free(B);
gsl_matrix_free(T);
gsl_matrix_free(Q);
gsl_matrix_free(R);
}
//-----------------------------------------------------------------------------------------
void
GSL::factorize_ZZ_matrix(Value & Z, int M, int N, const Cell * cB)
{
}
//-----------------------------------------------------------------------------------------
#endif // apl_GSL
