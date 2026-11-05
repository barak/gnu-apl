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

#include "Bif_F12_TRANSPOSE.hh"
#include "Common.hh"
#include "Cell.hh"
#include "Value.hh"
#include "Workspace.hh"

#if apl_GSL

#include <gsl/gsl_complex.h>
#include <gsl/gsl_linalg.h>
#include "QR_factorization_GSL.hh"

//════════════════════════════════════════════════════════════════════════════
void
GSL::LQ_factorize(Value & Z, int M, int N, Value_P B, bool need_complex)
{
  if (M > N)
     {
       MORE_ERROR() << "⌹[4] (LQ-factorization using libgsl) expects M ≤ N, "
                    << "but got M=" << M << " and N=" << N;
       LENGTH_ERROR;
     }

  // If B = L∘Q then
  //   ⍉B = ⍉(L∘Q) = (⍉Q)⍤⍉L. We can therefore QR-transpose ⍉B and return
  //   (⍉Q, ⍉R, and ⊖Ri)
  //
const Shape shape_BT(N, M);
Value_P BT = Bif_F12_TRANSPOSE::transpose(shape_BT, *B);   // BT←⍉B
Value_P ZT(3, LOC);
  if (need_complex)
     QR_factorize_ZZ_matrix(*ZT, N, M, *BT, 0);
  else
     QR_factorize_DD_matrix(*ZT, N, M, *BT, 0);

Value_P QT  = ZT->get_pointer_value(0);
Value_P RT  = ZT->get_pointer_value(1);
Value_P RiT = ZT->get_pointer_value(2);

const Shape shape_Q (N, N);
Value_P Z0 = Bif_F12_TRANSPOSE::transpose(shape_Q, *QT);   // BT←⍉B

const Shape shape_R(M, M);
Value_P Z1 = Bif_F12_TRANSPOSE::transpose(shape_R, *RT);   // BT←⍉B

const Shape shape_Ri(M, M);
Value_P Z2 = Bif_F12_TRANSPOSE::transpose(shape_Ri, *RiT);   // BT←⍉B

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);
}
//════════════════════════════════════════════════════════════════════════════
void
GSL::LU_factorize_DD_matrix(Value & Z, int M, int N, cValue_R B_val, ShapeItem idx)
{
  // gsl_linalg_LU_decomp() below requires a square matrix; without this,
  // calling it on a non-square one (or any other GSL-detected error)
  // reaches GSL's default error handler, which calls abort() and kills
  // the whole process -- every other QR/QL path in this file already
  // installs this handler first (it makes a GSL error a catchable
  // DOMAIN_ERROR instead).
  set_GSL_error_handler();

  // 0. Init GSL matrix B from APL ravel starting at idx
  //
gsl_matrix * B = gsl_matrix_alloc(M, N);   if (B == 0)   WS_FULL;
  loop(row, M)
  loop(col, N)   gsl_matrix_set(B, row, col, B_val.get_real_value(idx++));

gsl_permutation * P = gsl_permutation_alloc(M);   if (P == 0)   WS_FULL;
int signum = 0;
  gsl_linalg_LU_decomp(B, P, &signum);

  // Thats it. construct result vector (P D U).
  //
Value_P Z0(M, LOC);   // Z0 is P
const APL_Integer qio = Workspace::get_IO();
  loop(m, M)   Z0->next_ravel_Int(gsl_permutation_get(P, m) + qio);
  Z0->check_value(LOC);

const ShapeItem min_MN = min(M, N);

Value_P Z1(min_MN, N, LOC);   // Z1 is U
  loop(row, min_MN)
  loop(col, N)
     {
       if (col < row)   Z1->next_ravel_Number(0.0);   // below diag: U is 0
       else             Z1->next_ravel_Number(gsl_matrix_get(B, row, col));
     }
  Z1->check_value(LOC);

Value_P Z2(M, min_MN, LOC);   // Z2 is L
  loop(row, M)
  loop(col, min_MN)
     {
       if (col < row)        Z2->next_ravel_Number(gsl_matrix_get(B, row, col));
       else if (col > row)   Z2->next_ravel_Number(0.0);
       else                  Z2->next_ravel_Number(1.0);
     }
  Z2->check_value(LOC);

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);

  gsl_matrix_free(B);
  gsl_permutation_free(P);
}
//════════════════════════════════════════════════════════════════════════════
void
GSL::LU_factorize_ZZ_matrix(Value & Z, int M, int N, cValue_R B_val, ShapeItem idx)
{
  // see LU_factorize_DD_matrix() above for why this is needed.
  set_GSL_error_handler();

  // 0. Init GSL matrix B from APL ravel starting at idx
  //
gsl_matrix_complex * B = gsl_matrix_complex_alloc(M, N);   if (B == 0)   WS_FULL;
  loop(row, M)
  loop(col, N)
      {
        const ShapeItem idx_c = idx++;
        const gsl_complex tmp = { B_val.get_real_value(idx_c),
                                   B_val.get_imag_value(idx_c) };
        gsl_matrix_complex_set(B, row, col, tmp);
      }

gsl_permutation * P = gsl_permutation_alloc(M);   if (P == 0)   WS_FULL;
int signum = 0;
  gsl_linalg_complex_LU_decomp(B, P, &signum);

  // Thats it. construct result vector (P D U).
  //
Value_P Z0(M, LOC);   // Z0 is P
const APL_Integer qio = Workspace::get_IO();
  loop(m, M)   Z0->next_ravel_Int(gsl_permutation_get(P, m) + qio);
  Z0->check_value(LOC);

const ShapeItem min_MN = min(M, N);

Value_P Z1(min_MN, N, LOC);   // Z1 is U
  loop(row, min_MN)
  loop(col, N)
     {
       if (col < row)   // below diag: U is 0
          {
            Z1->next_ravel_Complex(0.0, 0.0);
          }
       else             // above diag: B is U
          {
            const gsl_complex tmp = gsl_matrix_complex_get(B, row, col);
             Z1->next_ravel_Complex(tmp.dat[0], tmp.dat[1]);
          }
     }
  Z1->check_value(LOC);

Value_P Z2(M, min_MN, LOC);   // Z2 is L
  loop(row, M)
  loop(col, min_MN)
     {
       if (col < row)
          {
            const gsl_complex tmp = gsl_matrix_complex_get(B, row, col);
             Z2->next_ravel_Complex(tmp.dat[0], tmp.dat[1]);
          }
       else if (col > row)
          {
            Z2->next_ravel_Number(0.0);
          }
       else
          {
            Z2->next_ravel_Number(1.0);
          }
     }
  Z2->check_value(LOC);

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);

  gsl_matrix_complex_free(B);
  gsl_permutation_free(P);
}
//════════════════════════════════════════════════════════════════════════════
void
GSL::QL_factorize_DD_matrix(Value & Z, int M, int N, cValue_R B_val, ShapeItem idx)
{
  set_GSL_error_handler();

gsl_matrix * B  = 0;
gsl_vector * TAU = 0;
gsl_matrix * Q  = 0;
gsl_matrix * L  = 0;
gsl_matrix * Li = 0;

  // set_GSL_error_handler() makes a GSL failure (e.g. inside
  // gsl_linalg_tri_invert on a singular matrix) throw DOMAIN_ERROR; free
  // whatever of B/TAU/Q/L/Li has been allocated so far before it
  // unwinds past their normal free() calls below (confirmed leak).
  try
     {
  // 0. Init GSL matrix B from APL ravel starting at idx
  //
  B = gsl_matrix_alloc(M, N);   if (B == 0)   WS_FULL;
  loop(row, M)
  loop(col, N)   gsl_matrix_set(B, row, col, B_val.get_real_value(idx++));

  // 1. Compute Q, L, and TAU. Q and L are packed into B
  //
  TAU = gsl_vector_alloc(N);   if (TAU == 0)   WS_FULL;
  gsl_linalg_QL_decomp(B, TAU);

  // 2. unpack Q and L
  //
  Q = gsl_matrix_alloc(M, M);   if (Q == 0)   WS_FULL;
  L = gsl_matrix_alloc(M, N);   if (L == 0)   WS_FULL;
  gsl_linalg_QL_unpack(B, TAU, Q, L);
  gsl_vector_free(TAU);   TAU = 0;
  gsl_matrix_free(B);     B = 0;

Value_P Z1(M, N, LOC);   // Z[1] lower triangular matrix L
  loop(row, M)
  loop(col, N)
      {
        Z1->next_ravel_Number(gsl_matrix_get(L, row, col));
  //    else              Z1->next_ravel_Number(0.0);
      }
  Z1->check_value(LOC);


Value_P Z0(M, M, LOC);   // Z[0] is the orthogonal M×M matrix Q
  loop(row, M)
  loop(col, M)   Z0->next_ravel_Number(gsl_matrix_get(Q, row, col));
  Z0->check_value(LOC);

  // 2. Invert Z1 to get Z2. L is inverted in place. The non-zero triangle
  //    of L aka. Z1 sits at the bottom of L, i.e. it starts at row (M - N).
  //
  // N×N, not M×M: only the N×N block below is ever filled (and every
  // later use of Li only ever indexes row/col < N), so the extra M-N
  // rows/columns of an M×M allocation were left uninitialized and then
  // read/written by gsl_linalg_tri_invert() over the whole (wrongly
  // M×M-sized) matrix -- latent nondeterministic behavior from
  // uninitialized heap memory.
  Li = gsl_matrix_alloc(N, N);   if (Li == 0)   WS_FULL;
  loop(row, N)
  loop(col, N)   gsl_matrix_set(Li, row, col,
                                gsl_matrix_get(L, row + M - N, col));

  gsl_linalg_tri_invert(CblasLower, CblasNonUnit, Li);
  gsl_matrix_free(L);   L = 0;
  gsl_matrix_free(Q);   Q = 0;

Value_P Z2(N, N, LOC);   // Z[2] is the inverse of L
  loop(row, N)
  loop(col, N)    Z2->next_ravel_Number(gsl_matrix_get(Li, row, col));
  Z2->check_value(LOC);
  gsl_matrix_free(Li);   Li = 0;

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z1->check_value(LOC);
     }
  catch (...)
     {
       if (B)     gsl_matrix_free(B);
       if (TAU)   gsl_vector_free(TAU);
       if (Q)     gsl_matrix_free(Q);
       if (L)     gsl_matrix_free(L);
       if (Li)    gsl_matrix_free(Li);
       throw;
     }
}
//────────────────────────────────────────────────────────────────────────────
void
GSL::QR_factorize_DD_matrix(Value & Z, int M, int N, cValue_R B_val, ShapeItem idx)
{
  set_GSL_error_handler();

  if (M < N)   // otherwise: "GSL error: M must be >= N in rqr.c"
     {
       MORE_ERROR() << "⌹[2] (QR-factorization using libgsl) requires M ≥ N, "
                    << "but got M=" << M << " and N=" << N;
       LENGTH_ERROR;
     }

gsl_matrix * B = 0;
gsl_matrix * T = 0;
gsl_matrix * R = 0;
gsl_matrix * Q = 0;

  // see QL_factorize_DD_matrix() above for why this try/catch is needed.
  try
     {
  // 0. Init GSL matrix B from APL ravel starting at idx
  //
  B = gsl_matrix_alloc(M, N);   if (B == 0)   WS_FULL;
  loop(row, M)
  loop(col, N)   gsl_matrix_set(B, row, col, B_val.get_real_value(idx++));

  // 1. Compute R and T. The diagonal of B and above is R ←→ Z1 = Z[1]
  //
  T = gsl_matrix_alloc(N, N);   if (T == 0)   WS_FULL;
  gsl_linalg_QR_decomp_r(B, T);

Value_P Z1(N, N, LOC);   // Z2 is the upper triangle N×N matrix R
  R = gsl_matrix_alloc(N, N);   if (R == 0)   WS_FULL;
  loop(row, N)
  loop(col, N)
      {
        if (col < row)   // below diag
           {
             gsl_matrix_set(R, row, col, 0);
             Z1->next_ravel_Number(0.0);
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
             Z2->next_ravel_Number(0.0);
           }
        else
           {
             Z2->next_ravel_Number(gsl_matrix_get(R, row, col));
           }
      }
  Z2->check_value(LOC);

  // 3. unpack T into Q and R
  //
  Q = gsl_matrix_alloc(M, M);   if (Q == 0)   WS_FULL;
  gsl_linalg_QR_unpack_r(B, T, Q, R);
  gsl_matrix_free(T);   T = 0;
  gsl_matrix_free(B);   B = 0;
  gsl_matrix_free(R);   R = 0;

Value_P Z0(M, M, LOC);   // Z[0] is the orthogonal M×M matrix Q
  loop(row, M)
  loop(col, M)   Z0->next_ravel_Number(gsl_matrix_get(Q, row, col));
  Z0->check_value(LOC);
  gsl_matrix_free(Q);   Q = 0;

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);
     }
  catch (...)
     {
       if (B)   gsl_matrix_free(B);
       if (T)   gsl_matrix_free(T);
       if (R)   gsl_matrix_free(R);
       if (Q)   gsl_matrix_free(Q);
       throw;
     }
}
//────────────────────────────────────────────────────────────────────────────
void
GSL::QR_factorize_ZZ_matrix(Value & Z, int M, int N, cValue_R B_val, ShapeItem idx)
{
  set_GSL_error_handler();

  if (M < N)
     {
       MORE_ERROR() << "⌹[2] (QR-factorization using libgsl) expects M ≥ N, "
                    << "but got M=" << M << " and N=" << N;
       LENGTH_ERROR;
     }

gsl_matrix_complex * B = 0;
gsl_matrix_complex * T = 0;
gsl_matrix_complex * R = 0;
gsl_matrix_complex * Q = 0;

  // see QL_factorize_DD_matrix() above for why this try/catch is needed.
  try
     {
  // 0. Init GSL matrix B from APL ravel starting at idx
  //
  B = gsl_matrix_complex_alloc(M, N);  if (B == 0)   WS_FULL;
  loop(row, M)
  loop(col, N)
     {
       const ShapeItem idx_c = idx++;
       const gsl_complex tmp = { B_val.get_real_value(idx_c),
                                  B_val.get_imag_value(idx_c) };
       gsl_matrix_complex_set(B, row, col, tmp);
     }

  // 1. Compute R and T. The diagonal of B and above is R ←→ Z1 = Z[1]
  //
  T = gsl_matrix_complex_alloc(N, N);  if (T == 0)   WS_FULL;
  gsl_linalg_complex_QR_decomp_r(B, T);

const gsl_complex zero = { 0, 0 };
Value_P Z1(N, N, LOC);   // Z2 is the upper triangle N×N matrix R
  R = gsl_matrix_complex_alloc(N, N);  if (R == 0)   WS_FULL;
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
  Q = gsl_matrix_complex_alloc(M, M);  if (Q == 0)   WS_FULL;
  gsl_linalg_complex_QR_unpack_r(B, T, Q, R);
  gsl_matrix_complex_free(T);   T = 0;
  gsl_matrix_complex_free(B);   B = 0;
  gsl_matrix_complex_free(R);   R = 0;

Value_P Z0(M, M, LOC);   // Z[0] is the orthogonal M×M matrix Q
  loop(row, M)
  loop(col, M)
     {
       const gsl_complex tmp = gsl_matrix_complex_get(Q, row, col);
       Z0->next_ravel_Complex(tmp.dat[0], tmp.dat[1]);
     }
  Z0->check_value(LOC);
  gsl_matrix_complex_free(Q);   Q = 0;

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);
     }
  catch (...)
     {
       if (B)   gsl_matrix_complex_free(B);
       if (T)   gsl_matrix_complex_free(T);
       if (R)   gsl_matrix_complex_free(R);
       if (Q)   gsl_matrix_complex_free(Q);
       throw;
     }
}
//────────────────────────────────────────────────────────────────────────────
void
GSL::RQ_factorize(Value & Z, int M, int N, Value_P B)
{
  if (M > N)
     {
       MORE_ERROR() << "⌹[3] (RQ-factorization using libgsl) expects M ≤ N, "
                    << "but got M=" << M << " and N=" << N;
       LENGTH_ERROR;
     }

  // If B = L∘Q then
  //   ⍉B = ⍉(L∘Q) = (⍉Q)⍤⍉L. We can therefore QR-transpose ⍉B and return
  //   (⍉Q, ⍉R, and ⊖Ri)
  //
const Shape shape_BT(N, M);
Value_P BT = Bif_F12_TRANSPOSE::transpose(shape_BT, *B);   // BT←⍉B
Value_P ZT(3, LOC);
  QL_factorize_DD_matrix(*ZT, N, M, *BT, 0);

Value_P QT  = ZT->get_pointer_value(0);
Value_P RT  = ZT->get_pointer_value(1);
Value_P RiT = ZT->get_pointer_value(2);

const Shape shape_Q (N, N);
Value_P Z0 = Bif_F12_TRANSPOSE::transpose(shape_Q, *QT);   // BT←⍉B

const Shape shape_R(M, M);
Value_P Z1 = Bif_F12_TRANSPOSE::transpose(shape_R, *RT);   // BT←⍉B

const Shape shape_Ri(M, M);
Value_P Z2 = Bif_F12_TRANSPOSE::transpose(shape_Ri, *RiT);   // BT←⍉B

  Z.next_ravel_Pointer(Z0.get());
  Z.next_ravel_Pointer(Z1.get());
  Z.next_ravel_Pointer(Z2.get());
  Z.check_value(LOC);
}
void
GSL::GSL_error_handler(const char * reason, const char * file, 
                                int line, int gsl_errno)
{
  MORE_ERROR() << "GSL error: " << reason << " in " << file;
  DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
//────────────────────────────────────────────────────────────────────────────
void
GSL::set_GSL_error_handler()
{
static bool GSL_handler_set = false;
  if (!GSL_handler_set)
     {
       gsl_set_error_handler(GSL_error_handler);
       GSL_handler_set = true;
     }
}
//════════════════════════════════════════════════════════════════════════════
#endif // apl_GSL
