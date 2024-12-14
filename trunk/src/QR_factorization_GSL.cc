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

#include <gsl/gsl_complex.h>
#include <gsl/gsl_linalg.h>
#include "QR_factorization_GSL.hh"

//=============================================================================
void
GSL::LQ_factorize_DD_matrix(Value & Z, int M, int N, const Cell * cB)
{
  // the LQ factorization of B is the QR factorization of B , but with
  // with B, R, and Ri // transposed
  if (M < N)
     {
       MORE_ERROR() << "⌹[2] (LQ-factorization using libgsl) expects M ≥ N, "
                    << "but got M=" << M << " and N=" << N;
       LENGTH_ERROR;
     }

  // 0. Init GSL matrix B from APL ravel * cB
  //
gsl_matrix * B = gsl_matrix_alloc(M, N);   if (B == 0)   WS_FULL;
  loop(row, M)
  loop(col, N)   gsl_matrix_set(B, col, row, cB++->get_real_value());

  // 1. Compute R and T. The diagonal of B and above is R ←→ Z1 = Z[1]
  //
gsl_matrix * T = gsl_matrix_alloc(N, N);   if (T == 0)   WS_FULL;
  gsl_linalg_QR_decomp_r(B, T);

Value_P Z1(N, N, LOC);   // Z2 is the upper triangle N×N matrix R
gsl_matrix * R = gsl_matrix_alloc(N, N);   if (R == 0)   WS_FULL;
  loop(col, N)
  loop(row, N)
      {
        if (col < row)   // below diag
           {
             gsl_matrix_set(R, row, col, 0);
             Z1->next_ravel_Number(0,0);
           }
        else
           {
             const double tmp = gsl_matrix_get(B, row, col);
             gsl_matrix_set(R, row, col, tmp);
             Z1->next_ravel_Number(tmp);
           }
      }
  Z1->check_value(LOC);

  // 2. Invert Z1 to get Z2. R is inverted in place.
  //
  gsl_linalg_tri_invert(CblasUpper, CblasNonUnit, R);
Value_P Z2(N, N, LOC);   // Z[2] is the N×N matrix Ri
  loop(col, N)
  loop(row, N)
      {
        if (col < row)   // below diag
           {
             Z2->next_ravel_Number(0,0);
           }
        else
           {
             Z2->next_ravel_Number(gsl_matrix_get(R, row, col));
           }
      }
  Z2->check_value(LOC);

  // 3. unpack T into Q and R
  //
gsl_matrix * Q = gsl_matrix_alloc(M, M);   if (Q == 0)   WS_FULL;
  gsl_linalg_QR_unpack_r(B, T, Q, R);
  gsl_matrix_free(T);
  gsl_matrix_free(B);
  gsl_matrix_free(R);

Value_P Z0(M, M, LOC);   // Z[0] is the orthogonal M×M matrix Q
  loop(col, M)
  loop(row, M)   Z0->next_ravel_Number(gsl_matrix_get(Q, row, col));
  Z0->check_value(LOC);
  gsl_matrix_free(Q);

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);
}
//-----------------------------------------------------------------------------
void
GSL::LQ_factorize_ZZ_matrix(Value & Z, int M, int N, const Cell * cB)
{
  if (M < N)
     {
       MORE_ERROR() << "⌹[2] (QR-factorization using libgsl) expects M ≥ N, "
                    << "but got M=" << M << " and N=" << N;
       LENGTH_ERROR;
     }

  // 0. Init GSL matric B from APL ravel * cB
  //
gsl_matrix_complex * B = gsl_matrix_complex_alloc(M, N);  if (B == 0)   WS_FULL;
  loop(row, M)
  loop(col, N)
     {
       const gsl_complex tmp = { cB->get_real_value(), cB->get_imag_value() };
       gsl_matrix_complex_set(B, col, row, tmp);
     }

  // 1. Compute R and T. The diagonal of B and above is R ←→ Z1 = Z[1]
  //
gsl_matrix_complex * T = gsl_matrix_complex_alloc(N, N);  if (T == 0)   WS_FULL;
  gsl_linalg_complex_QR_decomp_r(B, T);

const gsl_complex zero = { 0, 0 };
Value_P Z1(N, N, LOC);   // Z2 is the upper triangle N×N matrix R
gsl_matrix_complex * R = gsl_matrix_complex_alloc(N, N);  if (R == 0)   WS_FULL;
  loop(col, N)
  loop(row, N)
      {
        if (col < row)   // below diag
           {
             gsl_matrix_complex_set(R, row, col, zero);
             Z1->next_ravel_Complex(0.0, 0.0);
           }
        else
           {
             const gsl_complex tmp = gsl_matrix_complex_get(B, row, col);
             gsl_matrix_complex_set(R, row, col, tmp);
             Z1->next_ravel_Complex(tmp.dat[0], tmp.dat[1]);
           }
      }
  Z1->check_value(LOC);

  // 2. Invert Z1 to get Z2. R is inverted in place.
  //
  gsl_linalg_complex_tri_invert(CblasUpper, CblasNonUnit, R);
Value_P Z2(N, N, LOC);   // Z[2] is the N×N matrix Ri
  loop(col, N)
  loop(row, N)
      {
        if (col < row)   // below diag
           {
             Z2->next_ravel_Complex(0.0, 0.0);
           }
        else
           {
             const gsl_complex tmp = gsl_matrix_complex_get(R, row, col);
             Z2->next_ravel_Complex(tmp.dat[0], tmp.dat[1]);
           }
      }
  Z2->check_value(LOC);

  // 3. unpack T into Q and R
  //
gsl_matrix_complex * Q = gsl_matrix_complex_alloc(M, M);  if (Q == 0)   WS_FULL;
  gsl_linalg_complex_QR_unpack_r(B, T, Q, R);
  gsl_matrix_complex_free(T);
  gsl_matrix_complex_free(B);
  gsl_matrix_complex_free(R);

Value_P Z0(M, M, LOC);   // Z[0] is the orthogonal M×M matrix Q
  loop(col, M)
  loop(row, M)
     {
       const gsl_complex tmp = gsl_matrix_complex_get(Q, row, col);
       Z0->next_ravel_Complex(tmp.dat[0], tmp.dat[1]);
     }
  Z0->check_value(LOC);
  gsl_matrix_complex_free(Q);

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);
}
//=============================================================================
void
GSL::QR_factorize_DD_matrix(Value & Z, int M, int N, const Cell * cB)
{
  if (M < N)
     {
       MORE_ERROR() << "⌹[2] (QR-factorization using libgsl) expects M ≥ N, "
                    << "but got M=" << M << " and N=" << N;
       LENGTH_ERROR;
     }

  // 0. Init GSL matrix B from APL ravel * cB
  //
gsl_matrix * B = gsl_matrix_alloc(M, N);   if (B == 0)   WS_FULL;
  loop(row, M)
  loop(col, N)   gsl_matrix_set(B, row, col, cB++->get_real_value());

  // 1. Compute R and T. The diagonal of B and above is R ←→ Z1 = Z[1]
  //
gsl_matrix * T = gsl_matrix_alloc(N, N);   if (T == 0)   WS_FULL;
  gsl_linalg_QR_decomp_r(B, T);

Value_P Z1(N, N, LOC);   // Z2 is the upper triangle N×N matrix R
gsl_matrix * R = gsl_matrix_alloc(N, N);   if (R == 0)   WS_FULL;
  loop(row, N)
  loop(col, N)
      {
        if (col < row)   // below diag
           {
             gsl_matrix_set(R, row, col, 0);
             Z1->next_ravel_Number(0,0);
           }
        else
           {
             const double tmp = gsl_matrix_get(B, row, col);
             gsl_matrix_set(R, row, col, tmp);
             Z1->next_ravel_Number(tmp);
           }
      }
  Z1->check_value(LOC);

  // 2. Invert Z1 to get Z2. R is inverted in place.
  //
  gsl_linalg_tri_invert(CblasUpper, CblasNonUnit, R);
Value_P Z2(N, N, LOC);   // Z[2] is the N×N matrix Ri
  loop(row, N)
  loop(col, N)
      {
        if (col < row)   // below diag
           {
             Z2->next_ravel_Number(0,0);
           }
        else
           {
             Z2->next_ravel_Number(gsl_matrix_get(R, row, col));
           }
      }
  Z2->check_value(LOC);

  // 3. unpack T into Q and R
  //
gsl_matrix * Q = gsl_matrix_alloc(M, M);   if (Q == 0)   WS_FULL;
  gsl_linalg_QR_unpack_r(B, T, Q, R);
  gsl_matrix_free(T);
  gsl_matrix_free(B);
  gsl_matrix_free(R);

Value_P Z0(M, M, LOC);   // Z[0] is the orthogonal M×M matrix Q
  loop(row, M)
  loop(col, M)   Z0->next_ravel_Number(gsl_matrix_get(Q, row, col));
  Z0->check_value(LOC);
  gsl_matrix_free(Q);

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);
}
//-----------------------------------------------------------------------------
void
GSL::QR_factorize_ZZ_matrix(Value & Z, int M, int N, const Cell * cB)
{
  if (M < N)
     {
       MORE_ERROR() << "⌹[2] (QR-factorization using libgsl) expects M ≥ N, "
                    << "but got M=" << M << " and N=" << N;
       LENGTH_ERROR;
     }

  // 0. Init GSL matric B from APL ravel * cB
  //
gsl_matrix_complex * B = gsl_matrix_complex_alloc(M, N);  if (B == 0)   WS_FULL;
  loop(row, M)
  loop(col, N)
     {
       const gsl_complex tmp = { cB->get_real_value(), cB->get_imag_value() };
       gsl_matrix_complex_set(B, row, col, tmp);
     }

  // 1. Compute R and T. The diagonal of B and above is R ←→ Z1 = Z[1]
  //
gsl_matrix_complex * T = gsl_matrix_complex_alloc(N, N);  if (T == 0)   WS_FULL;
  gsl_linalg_complex_QR_decomp_r(B, T);

const gsl_complex zero = { 0, 0 };
Value_P Z1(N, N, LOC);   // Z2 is the upper triangle N×N matrix R
gsl_matrix_complex * R = gsl_matrix_complex_alloc(N, N);  if (R == 0)   WS_FULL;
  loop(row, N)
  loop(col, N)
      {
        if (col < row)   // below diag
           {
             gsl_matrix_complex_set(R, row, col, zero);
             Z1->next_ravel_Complex(0.0, 0.0);
           }
        else
           {
             const gsl_complex tmp = gsl_matrix_complex_get(B, row, col);
             gsl_matrix_complex_set(R, row, col, tmp);
             Z1->next_ravel_Complex(tmp.dat[0], tmp.dat[1]);
           }
      }
  Z1->check_value(LOC);

  // 2. Invert Z1 to get Z2. R is inverted in place.
  //
  gsl_linalg_complex_tri_invert(CblasUpper, CblasNonUnit, R);
Value_P Z2(N, N, LOC);   // Z[2] is the N×N matrix Ri
  loop(row, N)
  loop(col, N)
      {
        if (col < row)   // below diag
           {
             Z2->next_ravel_Complex(0.0, 0.0);
           }
        else
           {
             const gsl_complex tmp = gsl_matrix_complex_get(R, row, col);
             Z2->next_ravel_Complex(tmp.dat[0], tmp.dat[1]);
           }
      }
  Z2->check_value(LOC);

  // 3. unpack T into Q and R
  //
gsl_matrix_complex * Q = gsl_matrix_complex_alloc(M, M);  if (Q == 0)   WS_FULL;
  gsl_linalg_complex_QR_unpack_r(B, T, Q, R);
  gsl_matrix_complex_free(T);
  gsl_matrix_complex_free(B);
  gsl_matrix_complex_free(R);

Value_P Z0(M, M, LOC);   // Z[0] is the orthogonal M×M matrix Q
  loop(row, M)
  loop(col, M)
     {
       const gsl_complex tmp = gsl_matrix_complex_get(Q, row, col);
       Z0->next_ravel_Complex(tmp.dat[0], tmp.dat[1]);
     }
  Z0->check_value(LOC);
  gsl_matrix_complex_free(Q);

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);
}
//-----------------------------------------------------------------------------
#endif // apl_GSL
