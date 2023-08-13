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

    See also file LApack.hh for additional copyright notices.
*/

/*
   The mathematical background for the below is nicely described in:

   "The Householder transformation in numerical linear algebra"
   by: John Kerl February 3, 2008
 */

/** @file
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "Bif_F12_DOMINO.hh"
#include "Common.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "Value.hh"
#include "Workspace.hh"

using namespace std;

/// enable (1) / disable (0) debug printouts
#define LA_DEBUG 0

#include "LApack.hh"

inline void next_Cell(Value_P & value, const LA_pack::DD & dd)
   { value->next_ravel_Float(real(dd)); }

inline void next_Cell(Value_P & value, const LA_pack::ZZ & zz)
   { value->next_ravel_Complex(real(zz), imag(zz)); }

char * LA_pack::work_gelsy = 0;
char * LA_pack::work_geqp3 = 0;   // geqp3() and estimate_rank()
char * LA_pack::work_larf = 0;

/*
   Notation: in the literature the conjujate transpose of a vector v resp.
   a matrix M is usually (and also in LApack) denoted by v* resp. M*.

   Since Unicode has no superscript-* and to avoid confusion as to
   what * means, we normally use:

     v° resp. M° to denote the conjugate transposes (v* resp. M* in FORTRAN),
     A∘B for the inner product of A and B (aka. matrix multiplication), and
     A×B for the scalar (component-wise) multiplication of A and B

   Call tree:

   divide_matrix(Z, A, B)
   │
   └─── gelsy<T>(A, B, rcond)
        │
        └─── scaled_gelsy(A, B, rcond)
             │
             ├─── geqp3<T>(A, pivot, tau)
             │    │
             │    └─── laqp2<T>(A, pivot, tau, vn1_vn2)
             │            │
             │            ├─── larfg<T>('Left', N, X)
             │            └─── larf<T>(v, len_v, tau, C)
             │
             ├─── estimate_rank(A, rcond)
             │
             ├─── trsm<T>('Left', 'Upper', 'No transp.' N, A, tau, B)
             │
             ├─── unmqr<T>('Left', 'Conj. transp.', N, A, tau, B)
             │    │
             │    ├─── unm2r<T>('Left', 'Conj transp', N, A, tau, B)
             │    │    │
             │    │    ├─── larft('Forward', 'Columnwise')
             │    │    └─── larfb('Forward', 'Columnwise')
             │    │
             │    ├─── larf<T>('Left', v, 1, tau, C)
             │    ├─── ...
             │    └─── larf<T>('Left', v, N, tau, C)
             │
             └─── trsm<T>('Left', 'Upper', 'No trans', A, B1)

   in the complex A/B case.  The FORTRAN naming in the real case is:

           Complex A/B │ Real A/B │ file names
           ────────────┼──────────┼───────────────────
            ZUNMQR     │ DORMQR   │ zunmqr.f, dormqr.f
            ZUNM2R     │ DORM2R   │ zunm2r.f, dorm2r.f
           ────────────┴──────────┴───────────────────
  
   Other notes/conventions:

   * The FORTRAN function names are UPPERCASE, their filename lowercase.f,
   * The C/C++ functions are lowercase (static members of class LApack),
   * All complex FORTRAN functions start with Z, all real FORTRAN functions
     with D (omitted above and in C/C++).

 */
//----------------------------------------------------------------------------

#if LA_DEBUG
# include "LAdebug.icc"
#else

template<class T> inline void print_vector(const char * title,
                                           const T * data,
                                           int N) {}

template<class T> inline void print_matrix(const char * title,
                                           const LA_pack::Matrix<T> & A) {}

template<class T>inline  void print_product(const char * title,
                                            const LA_pack::Matrix<T> & A,
                                            const LA_pack::Matrix<T> & B,
                                            const Ccol * pivot) {}

template<class T> inline void print_QR(const char * title,
                                       const LA_pack::Matrix<T> & B,
                                       const LA_pack::Matrix<T> & QR,
                                       const Ccol * pivot, T * tau) {}
#endif

//----------------------------------------------------------------------------
/// the implementation of Z←A⌹B, where
/// Z[;j] is the solution of A[;j] = B +.× Z[j]   (if rows == cols_B),
/// or else: A[;j] - B +.× Z[j] is minimal         (if rows > cols_B)
void
LA_pack::divide_matrix(Value & Z, bool need_complex, ShapeItem rows,
                       ShapeItem cols_A, const Cell * cA,
                       ShapeItem cols_B, const Cell * cB)
{
   // the following has been checked by the caller:
   //
   // rows_B >= cols_B and
   // rows_A == rows_B  (aka. rows)
   //
const APL_Float rcond = Workspace::get_CT();
const size_t items_A = rows * cols_A;
const size_t items_B = rows * cols_B;
const int FpI = need_complex ? 2 : 1;   // Floats per Item

const size_t bytes_A = items_A * FpI * sizeof(APL_Float);
const size_t bytes_B = items_B * FpI * sizeof(APL_Float);

   // allocate storage for A and B
   //
char * work_AB = new char[bytes_A + bytes_B];
   if (work_AB == 0)   WS_FULL;

char * work_A = work_AB;
char * work_B = work_A + bytes_A;

APL_Float * fpA = reinterpret_cast<APL_Float *>(work_A);
APL_Float * fpB = reinterpret_cast<APL_Float *>(work_B);

   ALL_COLS(cols_A)
       {
         if (need_complex)   // complex A or B
            {
              ZZ * const a = reinterpret_cast<ZZ *>(fpA);
              ALL_ROWS(rows)
                  {
                    const Cell & src_A = cA[row * cols_A + col];
                    new (a + row) ZZ(src_A.get_real_value(),
                                     src_A.get_imag_value());
                  }

              // initialize complex b[] in FORTRAN (aka. column major) order,
              // which is ⍉ APL (aka. row major) order. I.e. use ⍉B.
              //
              ZZ * const b = reinterpret_cast<ZZ *>(fpB);
              ZZ * bb = b;

              ALL_COLS(cols_B)
              ALL_ROWS(rows)
                 {
                   const Cell & src_B = cB[col + row*cols_B];
                   new (bb++) ZZ(src_B.get_real_value(),
                                 src_B.get_imag_value());
                 }

              // create complex Matrix<ZZ> views A and B of a and b
              //
              Matrix<ZZ> A(a, rows, 1,      /* LDA */ rows);
              Matrix<ZZ> B(b, rows, cols_B, /* LDB */ rows);

              const int N = cols_A > cols_B ? cols_A : cols_B;
              char * work = allocate_workspace<ZZ>(N);
              const sRank rank = gelsy<ZZ>(A, B, rcond, work);
              delete work;

              if (rank != cols_B)
                 {
                   MORE_ERROR() << "A⌹B : linear dependent (complex) B?";
                   delete work_AB;
                   DOMAIN_ERROR;
                 }

              // cols_A = rows_Z. We have computed the result for col c of A
              // which is row c of Z.
              //
              ALL_ROWS(cols_B)
                  Z.set_ravel_Complex(row*cols_A + col, a[row].real(),
                                                        a[row].imag());
            }
         else   // real A and B
            {
              ALL_ROWS(rows)
                 {
                    const Cell & src_A = cA[row*cols_A + col];
                    fpA[row] = src_A.get_real_value();
                 }

              // initialize real b[] in FORTRAN (aka. column major) order,
              // which is ⍉ APL (aka. row major) order. I.e. use ⍉B.
              //
              DD * const b = reinterpret_cast<DD *>(fpB);
              APL_Float * bb = b;
              ALL_COLS(cols_B)
              ALL_ROWS(rows)
                 {
                   const Cell & src_B = cB[col + row*cols_B];
                   *bb++ = src_B.get_real_value();
                 }

              const APL_Float rcond = Workspace::get_CT();

              // create real Matrix<DD> views A and B of a and b
              //
              Matrix<DD> A(fpA, rows, 1,      /*LDA*/ rows);
              Matrix<DD> B(fpB, rows, cols_B, /*LDB*/ rows);
              const int N = cols_A > cols_B ? cols_A : cols_B;
              char * work = allocate_workspace<DD>(N);
              const sRank rank = gelsy<DD>(A, B, rcond, work);
              delete work;
              if (rank != cols_B)
                 {
                   MORE_ERROR() << "A⌹B : linear dependent (real) B?";
                   delete work_AB;
                     DOMAIN_ERROR;
                 }

              // cols_A = rows_Z. We have computed the result for col c of A
              // which is row c of Z.
              //
              ALL_ROWS(cols_B)   Z.set_ravel_Float(row*cols_A + col, fpA[row]);
            }
       }

   delete work_AB;
}
//----------------------------------------------------------------------------
/// the implementation of Z←⌹[X]B, where Z is (Q T T⁻¹) and B = T∘Q.
//
void
LA_pack::factorize_matrix(Value & Z, bool need_complex, ShapeItem rows,
                          ShapeItem cols, const Cell * cB, APL_Float rcond)
{
const size_t items_B = rows * cols;
const int FpI = need_complex ? 2 : 1;   // Floats per Item
const size_t bytes_B = items_B * FpI * sizeof(APL_Float);

char * work_B = new char[bytes_B];
   if (work_B == 0)   WS_FULL;

APL_Float * fpB = reinterpret_cast<APL_Float *>(work_B);

   if (need_complex)
      {
        // initialize complex b[] in FORTRAN (aka. column major) order,
        // which is ⍉ APL (aka. row major) order. I.e. use ⍉B.
        //
        ZZ * const b = reinterpret_cast<ZZ *>(fpB);
        ZZ * bb = b;

        ALL_COLS(cols)
        ALL_ROWS(rows)
           {
             const Cell & src_B = cB[col + row*cols];
             new (bb++) ZZ(src_B.get_real_value(),
                           src_B.get_imag_value());
           }

        // create a complex Matrix<ZZ> view B of b. The 0-pointer
        // in A tells gelsy() to return after the QR-factorization.
        //
        Matrix<ZZ> A(0, 0, 0, 0);
        Matrix<ZZ> B(b, rows, cols, /* LDB */ rows);

        char * work = allocate_workspace<ZZ>(cols);
        const sRank rank = gelsy<ZZ>(A, B, rcond, work);
        delete work;
        if (rank != cols)
           {
             MORE_ERROR() << "⌹[X]B : linear dependent (complex) B?";
             delete work_B;
             DOMAIN_ERROR;
           }
      }
   else   // real B
      {
        // initialize real b[] in FORTRAN (aka. column major) order,
        // which is ⍉ APL (aka. row major) order. I.e. use ⍉B.
        //
        DD * const b = reinterpret_cast<DD *>(fpB);
        APL_Float * bb = b;
        ALL_COLS(cols)
        ALL_ROWS(rows)
           {
             const Cell & src_B = cB[col + row*cols];
             *bb++ = src_B.get_real_value();
           }

        // create a real Matrix<ZZ> view B of b. The 0-pointer
        // in A tells gelsy() to return after the QR-factorization.
        //
        Matrix<DD> A(0, 0, 0, 0);
        Matrix<DD> B(b, rows, cols, /* LDB */ rows);

        char * work = allocate_workspace<DD>(cols);
           {
             const sRank rank = gelsy<DD>(A, B, rcond, work);
       
             if (rank != cols)
                {
                  MORE_ERROR() << "⌹[X]B : linear dependent (real) B?";
                  delete work_B;
                  DOMAIN_ERROR;
                }
       
             Ccol * const pivot = reinterpret_cast<Ccol *>(work_gelsy);
             DD   * const tau   = reinterpret_cast<DD *>(pivot + cols);
             split_HR(Z, B, pivot, tau);
                }
             delete work;
      }

   delete work_B;
}
//----------------------------------------------------------------------------
template<typename T>
void LA_pack::split_HR(Value & Z, const Matrix<T> & HR,
                       const Ccol * pivot, const T * tau)
{
const Crow M = HR.get_row_count();
const Ccol N = HR.get_column_count();

   // init the top-level Z. The sub-values Z[1], Z[2], and Z[3] will
   // be initialized after that.
   //
   //
const Shape shape_Z1(M, M);   // Symmetric Orthogonal factor Q
const Shape shape_Z2(M, N);   // upper triangle matrix R

Value_P Z1(shape_Z1, LOC);   // Q
Value_P Z2(shape_Z2, LOC);   // R
 
print_matrix("split_HR(): initial HR from geqp3()", HR);
   /* start with Z[2], the symmetric orthogonal M×M matrix. B aka. QR:

             ┌─────HR────┐ ┬
             │ d         │ │
             │   d    R  │ │
             │     d     │ M rows
             │  H    d   │ │
             │         d │ │
             └───────────┘ ┴
             ├─────N─────┤
                  cols

         The diagonal belongs to R, the diagonal of R is tau, and
         pivol is the permutation of columns
    */

   // 1. generate matrices TMP_C, TMP_R, and (INV_R, AUG) for Z1, Z2, and Z3
   //

   /* 1a. allocate space:
 
      tmp = [tmp_C], [tmp_R], [inv_R]
   */
T * tmp = new T[4*M*M];   // large enough for 4 M×M, M×N, or N×M matrices
   if (tmp == 0)   WS_FULL;
T * tmp_C = tmp;
T * tmp_R = tmp_C + M*M;
T * inv_R = tmp_R + M*M;
T * aug_R = inv_R + M*M;

   // 1b. create matrices
   //
Matrix<T> TMP_C(tmp_C, M, N, M);   // a mutable (by ung2r()) copy C of HR
Matrix<T> TMP_R(tmp_R, M, N, M);   // the upper triangle of HR
Matrix<T> INV_R(inv_R, M, N, M);   // the inverse of TMP_R

   // 1c. initialize matrices
   //

   // TMP_C
   //
   ALL_ROWS(M)
   ALL_COLS(N)
      TMP_C.at(row, col) = HR.at(row, col);
print_matrix("split_HR(): matrix C before ung2r(C, tau)", TMP_C);
     ung2r(TMP_C, tau);
print_matrix("split_HR(): matrix C after ung2r() before un_pivot()", TMP_C);
   ALL_ROWS(M)
   ALL_COLS(N)
      {
        const Ccol pcol = un_pivot(pivot, N, col);
        const T item = TMP_C.at(row, pcol);
        next_Cell(Z1, item);
      }
   Z1->check_value(LOC);
   Z.next_ravel_Pointer(Z1.get());

   // final TMP_R, initial INV_R. NOTE that TMP_R is a matrix in APL (row
   // major) order while INV_R is in FORTRAN column major) order.
   //
   ALL_ROWS(M)
   ALL_COLS(N)
      {
        const T item = (row > col) ? T(0.0) : HR.at(row, col);
        TMP_R.at(row, col) = INV_R.at(col, row) = item;
        next_Cell(Z2, item);
      }
   Z2->check_value(LOC);
   Z.next_ravel_Pointer(Z2.get());

print_matrix("final TMP_R",   TMP_R);
print_product("Q∘R", TMP_C, TMP_R, pivot);

print_matrix("initial INV_R", INV_R);

   // INV_R
   //
     if (is_complex(*tau))
        {
          ZZ * inv = reinterpret_cast<ZZ *>(inv_R);
          ZZ * aug = reinterpret_cast<ZZ *>(aug_R);
          Value_P Z3 = Bif_F12_DOMINO::invert_UTM<ZZ, true> (M, N, inv, aug);
          Z.next_ravel_Pointer(Z3.get());   // Z[2]
        }
     else
        {
          DD * inv = reinterpret_cast<DD *>(inv_R);
          DD * aug = reinterpret_cast<DD *>(aug_R);
          Value_P Z3 = Bif_F12_DOMINO::invert_UTM<DD, false>(M, N, inv, aug);
          Z.next_ravel_Pointer(Z3.get());   // Z[2]
        }

print_matrix("split_HR(): orthogonal factor Q",     TMP_C);
print_matrix("split_HR(): upper triangle factor R", TMP_R);
print_matrix("split_HR(): inverse of R",            INV_R);

   Z.check_value(LOC);
   return;

/***
   {
     ALL_ROWS(M) ALL_COLS(N)   TMP_C.at(row, col) = HR.at(row, col);
print_matrix("split_HR(): matrix C before ung2r(C, tau)", TMP_C);
     ung2r(TMP_C, tau);
print_matrix("split_HR(): matrix C after ung2r() before un_pivot()", TMP_C);

     ALL_ROWS(M)   ALL_COLS(N)
        {
          const Ccol pcol = un_pivot(pivot, N, col);
          next_Cell(Z1, tmp_C.at(row, pcol));
        }
     Z1->check_value(LOC);
   }

   // continue with Z2 ←→ R (the uppeR tRiangular) factor of Q∘R)
   //
   // Z2 ←→ R
   //
   {
     Matrix<T> TMP_R(tmp_R, M, N, N);   // a mutable (by ung2r()) copy C of HR
     ALL_ROWS(M)   ALL_COLS(N)
        {
          if ((row <= col)   
        }
        {
          T t(0.0);                                // fallback if below diag
          if (row <= col)   t = HR.at(row, col);   // above diag
          next_Cell(Z2, t);
        }
     Z2->check_value(LOC);
   }

   // then compute and store Z3 ←→ R⁻¹.
   //
   {
     Matrix<T>TMP(tmp, M, N, 1);   // a mutable (by ung2r()) copy C of HR
     ALL_ROWS(M)
     ALL_COLS(N)   TMP.at(row, col) = HR.at(row, col);

     ALL_ROWS(M)
     ALL_COLS(row)   TMP.at(row, col) = T(0);   // TMP←0 below diag

     if (is_complex(*tau))
        {
          ZZ * tmp_ZZ = reinterpret_cast<ZZ *>(tmp);
          ZZ * aug_ZZ = reinterpret_cast<ZZ *>(aug);
          Bif_F12_DOMINO::invert_UTM<ZZ, true>(M, N, tmp_ZZ, aug_ZZ);
        }
     else
        {
          DD * tmp_DD = reinterpret_cast<DD *>(tmp);
          DD * aug_DD = reinterpret_cast<DD *>(aug);
          Bif_F12_DOMINO::invert_UTM<DD, false>(M, N, tmp_DD, aug_DD);
        }

     Matrix<T> AUG(aug, N, M, 1);
     ALL_ROWS(N)
     ALL_COLS(M)
        {
          const T Null(0.0);
          if (row <= col)   next_Cell(Z3, AUG.at(row, col));   // above diag
          else              next_Cell(Z3, Null);               // below diag
        }
     Z3->check_value(LOC);
   }

   delete tmp;

   Z.next_ravel_Pointer(Z1.get());
   Z.next_ravel_Pointer(Z2.get());
   Z.next_ravel_Pointer(Z3.get());
***/
}
//----------------------------------------------------------------------------
Ccol
LA_pack::un_pivot(const Ccol * pivot, int len, Ccol col)
{
   loop(j, len)   if (pivot[j] == col)   return j;

   // explain why col was nor found.
   //
   CERR << "*** could not find column " << col << " in pivot[" << len << "]:";
   loop(j, len) CERR << " " << pivot[j];
   CERR << endl;
   return -1;
}
//----------------------------------------------------------------------------


/* numerical limits
     
   DLAMCH determines double precision machine parameters.
   We (only) #define those that we need. Precision is not crucial
   as long as rounding is done in the right direction.
 */

/// aka. \b eps: relative machine precision. About 1 LSB of 1.0
#define dlamch_E 1.11022e-16

/// base (=2 for binary) * dlamch_E
#define dlamch_P 2.22045e-16

/// aka. \b sfmin: smallest (positive) number with a valid reciprocal
#define dlamch_S 2.22507e-308

static const APL_Float small_number = dlamch_S / dlamch_P;    // 1.00208E¯292
static const APL_Float big_number   = 1.0 / small_number;     // 9.97923E291
static const APL_Float safe_min     =  dlamch_S / dlamch_E;   // 2.00417E¯292
static const APL_Float tol3z        = sqrt(dlamch_E);         // 1.05367E¯8
#undef dlamch_S
#undef dlamch_P

//----------------------------------------------------------------------------
/**
    LApack functions DGELSY and ZGELSY aka. xGELSY:

    xGELSY computes the minimum-norm solution to a complex linear least
    squares problem:

    minimize ║ B × X - A ║

    using a complete orthogonal factorization of B.
    B is an M-by-N matrix which may be rank-deficient.

    * if M < N (or if too many lines of A are linearly dependent) then
         A is under-determined and a DOMAIN ERRO is returned.
    * if M > N then A is overdetermined and the nearest X is returnd.
    * otherwise M = N and an exact solution X is returned if it exists.

    Several right hand side vectors b and solution vectors x can be
    handled in a single call; they are stored as the columns of the
    M-by-NRHS right hand side matrix A and the N-by-NRHS solution
    matrix X.

    The routine first computes a QR factorization with column pivoting:

    B × P = Q × ⎡ R₁₁ R₁₂ ⎤
                ⎣  0  R₂₂ ⎦

    with R₁₁ defined as the largest leading submatrix whose estimated
    condition number is less than 1/RCOND. The order of R11, i.e. RANK,
    is the effective rank of B.

    Then, R22 is considered to be negligible, and R12 is annihilated
    by unitary transformations from the right, arriving at the
    complete orthogonal factorization:

    B ∘ P = Q ∘ ⎡ T₁₁ 0 ⎤ ∘ Z
                ⎣  0  0 ⎦

    The minimum-norm solution is then

    X = P × Z° × H [ inv(T11) × Q1 * × H × A ]
                     [        0         ]

    where q₁ consists of the first rank columns of q.

     This routine is basically identical to the original xGELSX except
     three differences:

       • The permutation of matrix A (the right hand side) is faster and
         more simple.

       • The call to the subroutine xGEQPF has been substituted by the
         the call to the subroutine xGEQP3. This subroutine is a Blas-3
         version of the QR factorization with column pivoting.

       • Matrix A (the right hand side) is updated with Blas-3.

    If A.get_dx() is 0 then only the QR factorization of B shall be
    computed and returned.
 */
template<typename T>
int LA_pack::gelsy(Matrix<T> & A, Matrix<T> & B, APL_Float rcond, char * work)
{
const Crow M = B.get_row_count();
const Ccol N = B.get_column_count();
const bool solve = A.get_dx() != 0;   // otherwise factorize

   // APL is responsible for handling the empty cases
   //
   if (solve)   Assert(M && N && A.get_column_count() && N <= M);

   // For a better precision, scale B and A so that their max. norm lies
   // between small_number and big_number. Then call scaled_gelsy() and
   // scale the result back by the same factors.
   //
APL_Float un_scale_B = 1.0;
   {
     T * const B00 = &B.diag(0);
     const size_t MN = M * N;
     const APL_Float norm_B = max_item(B00, MN);

     if (norm_B == 0.0)              return 0;   // B is the 0-matrix
     if (norm_B < small_number)      scale(B00, MN, un_scale_B = big_number);
     else if (norm_B > big_number)   scale(B00, MN, un_scale_B = small_number);
   }

APL_Float un_scale_A = 1.0;
   if (solve)   // unless factorize
      {
        T * const A00 = &A.diag(0);
        const size_t MN = A.get_row_count() * A.get_column_count();
        const APL_Float norm_A = max_item(A00, MN);

        if (norm_A < small_number)   scale(A00, MN, un_scale_A = big_number);
        if (norm_A > big_number)     scale(A00, MN, un_scale_A = small_number);
      }

   // do the work.
   //
   {
     const int RANK = scaled_gelsy(B, A, rcond, work);
     if (RANK < N)   return RANK;   // this is an error
   }

   // Undo scaling. A needs only be scaled back if B∘X = A is solved,
   //  but not if B was only factorized.
   //
   if (solve)
      {
        Matrix<T> A1 = A.take(N, A.get_column_count());
        if (un_scale_B != 1.0)
           {
             scale(&A1.diag(0), 1.0/un_scale_B, N*A.get_column_count());
           }
      
        if (un_scale_A != 1.0)
           {
             scale(&A1.diag(0), un_scale_A, N * A.get_column_count());
           }
      }
 
   if (un_scale_B != 1.0)
      {
        Matrix<T> B1 = B.take(N, N);
        scale(&B1.diag(0), un_scale_B, N*N);
      }
 
   return N;   // success
}
//----------------------------------------------------------------------------
/// scaled_gelsy() computes gelsy() with B and A scaled nicely.
/*
   B is the matrix to be inverted, and
   A is one or more column vectors for which we want to solve B ∘ X[;i] = A[;i]

   On return: result X stored in B and items of A overwritten.
 */ 
template<typename T>
int LA_pack::scaled_gelsy(Matrix<T> & B, Matrix<T> & A, double rcond,
                          char * work)
{
   // print_matrix<T>("B", B);
   // print_matrix<T>("A", A);

   // this gelsy is optimized for (and restricted to) the following conditions:
   //
   // 0 < N <= M  →  min_MN = N and max_MN = M
   // 0 < NRHS
   //
const Ccol NRHS = A.get_column_count();   // right hand side (of B ∘ X = A)
const Ccol N    = B.get_column_count();

/* Compute QR factorization of B with column pivoting: B ∘ P = Q ∘ R

   The routine first computes a QR factorization with column pivoting:

                                 ⎡ R11 R12 ⎤
       B ∘ P = Q ∘ R    with R = ⎢         ⎥, and
                                 ⎣  0  R22 ⎦

   R11 defined as the largest leading submatrix whose estimated
   condition number is less than 1/RCOND. The order of R11, i.e. RANK,
   is the effective rank of B.

   Then, R22 is considered to be negligible, and R12 is annihilated
   by orthogonal transformations from the right, arriving at the
   complete orthogonal factorization:

                                     ⎡ T11 0 ⎤
       B ∘ P = Q ∘ T ∘ Z    with T = ⎢       ⎥
                                     ⎣  0  0 ⎦

   The minimum-norm solution is then

       X = P ∘ Z° ∘ T [ inv(T11) ∘ Q1° ∘ T ∘ A ]
                   [        0         ]
   where Q1 consists of the first RANK columns of Q.
*/

   // split work_gelsy into pivot, tau, (and tmp, but see below)
   //
Ccol * const pivot = reinterpret_cast<Ccol *>(work_gelsy);   // pivot[N]
T    * const tau   = reinterpret_cast<T *>(pivot + N);       // tau[N]

#if LA_DEBUG
DebugMatrix B_before("B_before geqp3()", B);
   print_matrix("before geqp3() at " LOC, B_before);
   geqp3<T>(B, pivot, tau);
   print_QR("Q∘R after geqp3() at " LOC, B_before, B, pivot, tau);
#else
   geqp3<T>(B, pivot, tau);
#endif

   // Determine RANK (of B) using incremental condition estimation
   //
   {
     const int RANK_B = estimate_rank(B, rcond);
     if (RANK_B < N)   return RANK_B;
   }

   if (A.get_dx() == 0)   return N;   // QR factorization only

   // from here on, RANK == N (B has N linear independent rows)
   // We leave RANK in the comments but use N in the code.

   //                         ⎡ R11 R12 ⎤
   // Logically partition R = ⎢         ⎥,
   //                         ⎣  0  R22 ⎦
   //
   // where R11 = R[1:RANK, 1:RANK]
   // [R11, R12] = [ T11, 0 ] × A
   //

   /* according to e.g. ZUNM2R:

      B is COMPLEX*16 array, dimension (LDA,K)
      The i-th column must contain the vector which defines the
      elementary reflector H(i), for i = 1,2,...,k, as returned by
      ZGEQRF in the first k columns of its array argument B.
      B is modified by the routine but restored on exit.
      A[1:M, 1:NRHS] := Q° ∘ H ∘ A[1:M, 1:NRHS]
    */
   unm2r<T>(N, B, tau, A);   // called with SIDE = 'Left'

   // A(1:RANK, 1:NRHS) := reciprocal(T11) * A(1:RANK,1:NRHS)
   //
   {
     // solve B ∘ X[; 1:NRHS] = A[; 1:NRHS]; store the result X in A[; 1:NRHS].
     //
     trsm<T>(B, A, NRHS);
   }

   // tmp[N]: B column of A, permuted by pivot. FORTRAN uses a separate tmp[N]. 
   // However, tau[N] is no longer needed, so we can reuse it here. The loop
   // below undoes the pivoting of columns performed in geqp3() / laqp2().
   //
   ALL_COLS(NRHS)   // for every column col of A
      {
        ALL_ROWS(N)   tau[pivot[row]] = A.at(row, col);   // tau ← A[;col]
        ALL_ROWS(N)   A.at(row, col) = tau[row];          // A[;col]←tau
      }

   return N;
}
//----------------------------------------------------------------------------
/// function estimating the rank of \b A. (simplified part of GELSY)
template<typename T>
int LA_pack::estimate_rank(const Matrix<T> & A, APL_Float rcond)
{
   /* Determine RANK using incremental condition estimation...
     
      most likely this is pretty much an inlined GESVD with not used cases
      removed. As to GESVD:

      GESVD computes the singular value decomposition (SVD) of an
      M-by-N matrix A, optionally computing the left and/or right singular
      vectors. The SVD is written

      A = U × SIGMA × conjugate-transpose(V)  (i.e. A = U × SIGMA × V°)

      where SIGMA is an M-by-N matrix which is zero except for its
      min(m,n) diagonal elements, U is an M-by-M unitary matrix, and
      V is an N-by-N unitary matrix.  The diagonal elements of SIGMA
      are the singular values of A; they are real and non-negative, and
      are returned in descending order.  The first min(m,n) columns of
      U and V are the left and right singular vectors of A.

      Note that the routine returns V°*H, not V.
    */
const Ccol N = A.get_column_count();

APL_Float smax = abs(A.diag(0));
APL_Float smin = smax;
   if (smax == 0.0)   return 0;

   // store minima in work_min[ 0 ... N]
   // store maxima in work_max == work_min[N ... 2N]
   //
T * const work_min = reinterpret_cast<T *>(work_estimate_rank);
T * const work_max = work_min + N;

   // work_min and work_max will grow in the RANK loop, so only work_min[0]
   // and work_max[0] need to be initialized. The last item in work_min/max is
   // always cos_min/max (from laic1_MIN/MAX()) while the items before are the
   // the products of the prior sin_min/max (also from laic1_MIN/MAX()).
   //
   work_min[0] = T(1.0);   // cos 90°
   work_max[0] = T(1.0);   // cos 90°

   for (int RANK = 1; RANK < N; ++RANK)   // loop over columns of A
       {
         T sin_min(0.0);
         T sin_max(0.0);
         T cos_min(0.0);
         T cos_max(0.0);

         T alpha_min = 0.0;
         T alpha_max = 0.0;
         const T gamma(A.diag(RANK));

         ALL_ROWS(RANK)
             {
               const T Ar = A.at(row, RANK);
               alpha_min += conjugated(work_min[row] * Ar);
               alpha_max += conjugated(work_max[row] * Ar);
             }

         laic1_MIN<T>(smin, alpha_min, gamma, sin_min, cos_min);
         laic1_MAX<T>(smax, alpha_max, gamma, sin_max, cos_max);

         if (smax*rcond > smin)   // done (rank of A is < N).
            {
              return RANK;
            }

         ALL_ROWS(RANK)
              {
                work_min[row] *= sin_min;
                work_max[row] *= sin_max;
              }

         work_min[RANK] = cos_min;   // for the next iteration
         work_max[RANK] = cos_max;   // for the next iteration
       }

   return N;
}
//----------------------------------------------------------------------------
/* LApack function geqp3. Computes a QR factorization with column pivoting
   of matrix A:  A ∘ P = Q ∘ R
 */
template<typename T>
void LA_pack::geqp3(Matrix<T> & A, Ccol * pivot, T * tau)
{
const Crow M = A.get_row_count();
const Ccol N = A.get_column_count();

   // init the column permutation for pivoting.
   // start with the identical permutation. laqp2() will change pivot.
   //
   ALL_COLS(N)   pivot[col] = col;

   // (no fixed columns)

   // Factorize free columns...
   // =========================
   //

   // Initialize partial column norms.
   // the first N elements of WORK store the exact column norms,
   // and the next N elements are a copy of the first N elements
   //
APL_Float * const vn1_vn2 = reinterpret_cast<APL_Float *>(work_geqp3);

   // initialize vn1 and vn2 with column norms
   //
   ALL_COLS(N)
         {
           vn1_vn2[N + col] = vn1_vn2[col] = sqrt(norm_2(&A.at(0, col), M));
         }

   laqp2<T>(A, pivot, tau, vn1_vn2);
}
//----------------------------------------------------------------------------
/* LApack function UNM2R. Overwrite the general complex m-by-n matrix C:

   C ← Q ∘ C         if SIDE = 'L' and TRANS = 'N', or   (case 1)
   C ← Q° ∘ H ∘ C    if SIDE = 'L' and TRANS = 'C', or   (case 2)   <---
   C ← C ∘ Q         if SIDE = 'R' and TRANS = 'N', or   (case 3)
   C ← C ∘ Q° ∘ ×    if SIDE = 'R' and TRANS = 'C',      (case 4)

   where Q is a unitary matrix defined as the product of k
   elementary reflectors:

   Q = H(1) ∘ H(2) ∘...∘ H(k)

   The reflectors are in the lower triangle of A, the diagonal in tau

   NOTE: in LApack, the real version is DORM2R, while
                    the complex version is ZUNM2R.

         We use the name unm2r<T> for both.
 */
template<typename T>
void LA_pack::unm2r(Crow K, const Matrix<T> & A, const T * tau, Matrix<T> & C)
{
const Crow M = C.get_row_count();

   // only case 2 (SIDE == "L" and TRANS = 'C') is needed and implemented.
   // thus NOTRAN is false (and then tau[row] (Q°) is conjugated).
   //
   ALL_ROWS(K)
       {
         /* for row i of A; apply H(i) to SUB = C(i:m, 1:n)

          ┬        ╔════════════════╗       ╔═════════╗
          │        ║ \              ║   0   ║         ║
          │        ║  \    A        ║   ↓   ║    C    ║
          │        ║   \            ║   ↓   ║         ║ 
          M   ┬    ╟─── ○  v ... v ─╫─ row ─╫─────────╢ ○: Arr = := 1.0
          │   │    ║                ║   ↓   ║         ║ 
          │ M-row  ║       C        ║   K   ║   SUB   ║ 
          │   │    ║                ║       ║         ║ 
          ┴   ┴    ╚════════════════╝       ╚═════════╝
          */
         const T tau_row = conjugated(tau[row]);   // must copy tau[row] !

         // strictly speaking is A not const. However, we modify it
         // but then restore again. Therefore a cobnst_cast<> is OK.
         //
         T & tmp_diag = const_cast<T &>(A.diag(row));
         const T Arr = tmp_diag;   // remember A(row, row)
             tmp_diag = T(1.0);
             Matrix<T> SUB = C.sub_matrix(row, 0);
             larf<T>(&A.diag(row), M - row, tau_row, SUB);
         tmp_diag = Arr;           // restore A(row, row);
       }
}
//----------------------------------------------------------------------------
/** LApack function UNG2R.

   A is a matrix whose upper triangle matrix (including its diagonal) is
   not used (the result of geqp3()). The lower triangle of A contains
   elementary reflecors, and tau[] the scales (as returned by larfg()).

   Only K=N is needed and therefore derived locally from A.
   On exit, A is Q = H(1)∘H(2)∘...∘H(K).

   In FORTRAN: real DORG2R and complex ZUNG2R

   ung2r() is essentially unm2r() applied to the unit matrix
 **/
template<typename T>
void LA_pack::ung2r(Matrix<T> & A, const T * tau)
{
const Ccol N = A.get_column_count();
const Crow M = A.get_row_count();
const Ccol K = N;   // number of reflectors (all)

   REV_COLS(K)   // i in FORTRAN comments is k in C++.
      {
        // Apply reflector H(i) to A(i:m,i:n) (aka. (-i, i)↑A) from the left

        if (k < N)   // valid column (valid reflector)
           {
             T & Akk = A.diag(k);   // the diagonal item of column k
             Akk = T(1.0);          // set it to 1.
             Matrix<T> SUB = A.sub_matrix(k, k + 1);
             larf<T>(&Akk, M - k, conjugated(tau[k]), SUB);   // apply H(i)
           }

         if (k < M)   // valid row
            {
              /*
                  Update the entire column k of A as follows:

                  (a) items above the diagonal: set to 0.0,
                  (b) item on the diagonal:     set to 1.0 - tau[k];
                  (c) items below the diagonal: multiply by -tau[k]
               */

              ALL_ROWS(M)
                 {
                    T & Arow = A.at(row, k);
                    if      (row < k)   Arow = T(0.0);     // (a) above diag
                    else if (row > k)   Arow *= -tau[k];   // (c) below diag
                    else                Arow = T(1.0) - tau[k];   // (b on diag)
                 }
           }
      }
}
//----------------------------------------------------------------------------
/** LApack function laic1 (estimate largest singular value).
     apply one step of incremental condition estimation.

     SEST: largest estimated singular value of \b this matrix

     See laic1_MIN() below. laic1_MAX is laic1 with JOB=1,
                      while laic1_MIN is laic1 with JOB = 2.
 **/
template<typename T>
void LA_pack::laic1_MAX(APL_Float & SEST, T ALPHA, T GAMMA, T & SIN, T & COS)
{
const APL_Float abs_ALPHA = abs(ALPHA);
const APL_Float abs_GAMMA = abs(GAMMA);
const APL_Float abs_SEST  = abs(SEST);

   //    Estimating largest singular value ...

   //    special cases
   //
   if (SEST == 0.0)
      {
        const APL_Float smax = max(abs_GAMMA, abs_ALPHA);
        if (smax == 0.0)
           {
             SIN = T(0.0);
             COS = T(1.0);
             SEST = 0.0;
           }
        else
           {
             SIN = ALPHA / smax;
             COS = GAMMA / smax;
             SEST = smax * normalize(SIN, COS);
           }
        return;
      }

   if (abs_GAMMA <= dlamch_E * abs_SEST)   // if /tmp overflow
      {
        SIN = T(1.0);   // 90⁰
        COS = T(0.0);   // 90⁰
        const APL_Float abs_max = max(abs_SEST, abs_ALPHA);
        const APL_Float s1 = abs_SEST / abs_max;
        const APL_Float s2 = abs_ALPHA    / abs_max;
        SEST = abs_max * hypotenuse(s1, s2);
        return;
      }

   if (abs_ALPHA <= dlamch_E * abs_SEST)   // small abs_ALPHA
      {
        if (abs_GAMMA <= abs_SEST)
           {
             SIN = T(1.0);   // 90⁰
             COS = T(0.0);   // 90⁰
             SEST = abs_SEST;
           }
        else
           {
             SIN = T(0.0);   // 0⁰
             COS = T(1.0);   // 0⁰
             SEST = abs_GAMMA;
           }
        return;
      }

   if (abs_SEST <= dlamch_E * abs_ALPHA ||   // small abs_SEST
       abs_SEST <= dlamch_E * abs_GAMMA)     // small abs_SEST
      {
        if (abs_GAMMA <= abs_ALPHA)
           {
             const APL_Float quot = abs_GAMMA / abs_ALPHA;
             const APL_Float scale = hypotenuse(1.0, quot);

             SEST = abs_ALPHA * scale;
             SIN = (ALPHA / abs_ALPHA) / scale;
             COS = (GAMMA / abs_ALPHA) / scale;
           }
        else
           {
             const APL_Float quot = abs_ALPHA / abs_GAMMA;
             const APL_Float scale = hypotenuse(1.0, quot);
             SEST = abs_GAMMA * scale;
             SIN = (ALPHA / abs_GAMMA) / scale;
             COS = (GAMMA / abs_GAMMA) / scale;
           }
        return;
      }

   // the normal case
   //
const APL_Float zeta1 = abs_ALPHA / abs_SEST;
const APL_Float zeta2 = abs_GAMMA / abs_SEST;
const APL_Float b = 0.5*(1.0 - square(zeta1) - square(zeta2));
const APL_Float t = b > 0.0 ? square(zeta1) / (b + hypotenuse(b, zeta1))
                            : hypotenuse(b, zeta1) - b;

   SIN = -(ALPHA / abs_SEST) / t;
   COS = -(GAMMA / abs_SEST) / (1.0 + t);
   normalize(SIN, COS);
   SEST = sqrt(t + 1.0) * abs_SEST;
}
//----------------------------------------------------------------------------
/** LApack function laic1 (estimate smallest singular value).
      Results are: SEST, SIN, and COS
     apply one step of incremental condition estimation.

     SEST: smallest estimated singular value of \b this matrix

     LAIC1 applies one step of incremental condition estimation in
     its simplest version:

     Let x, twonorm(x) = 1, be an approximate singular vector of an j-by-j
     lower triangular matrix L, such that

          twonorm(L × x) = sest

     Then LAIC1 computes sestpr, s, c such that the vector

                 [ s × x ]


     x^ = [  c  ]

     is an approximate singular vector of

     [ L 0 ]

     L^ = [ w° × H gamma ]


     in the sense that twonorm(L^ × x^) = sestpr.

     Depending on JOB (_MIN resp, _MAX), an estimate for the largest or
     smallest singular value is computed.


    Note that [s c]° × H and sestpr° × 2 is an eigenpair of the system

    diag(sest*sest, 0) + [alpha  gamma] * [ conjg(alpha) ]
                                          [ conjg(gamma) ]

    where  alpha =  x° × H × w.

 **/
template<typename T>
void LA_pack::laic1_MIN(APL_Float & SEST, T ALPHA, T GAMMA, T & SIN, T & COS)
{
   // Estimating the smallest singular value...
   //
const APL_Float abs_ALPHA = abs(ALPHA);
const APL_Float abs_GAMMA = abs(GAMMA);
const APL_Float abs_SEST  = abs(SEST);

   // special cases
   //
   if (SEST == 0.0)
      {
        SIN = 1.0;   // 90°
        COS = 0.0;   // 90°
        if (abs_GAMMA > 0.0 || abs_ALPHA > 0.0)
           {
             SIN = -conjugated(GAMMA);
             COS =  conjugated(ALPHA);
           }

        const APL_Float abs_max = max(abs(SIN), abs(COS));
        SIN /= abs_max;
        COS /= abs_max;
        normalize(SIN, COS);
        return;
      }

   if (abs_GAMMA <= dlamch_E * abs_SEST)
      {
        SIN = T(0.0);   // 0°
        COS = T(1.0);   // 0°
        SEST = abs_GAMMA;
        return;
      }

   if (abs_ALPHA <= dlamch_E * abs_SEST)
      {
        if (abs_GAMMA <= abs_SEST)
          {
            SIN = T(0.0);   // 0°
            COS = T(1.0);   // 0°
            SEST = abs_GAMMA;
          }
        else
          {
            SIN = T(1.0);
            COS = T(0.0);
            SEST = abs_SEST;
          }
        return;
      }

   if (abs_SEST <= dlamch_E * abs_ALPHA ||   // small abs_SEST
       abs_SEST <= dlamch_E * abs_GAMMA)     // small abs_SEST
      {
        const T conj_gamma = conjugated(GAMMA);
        const T conj_alpha = conjugated(ALPHA);

        if (abs_GAMMA <= abs_ALPHA)
           {
             const APL_Float quot = abs_GAMMA / abs_ALPHA;
             const APL_Float scale = hypotenuse(1.0, quot);
             const APL_Float tmp_scale = quot / scale;

             SIN = - (conj_gamma / abs_ALPHA) / scale;
             COS =   (conj_alpha / abs_ALPHA) / scale;
             SEST = abs_SEST * tmp_scale;
           }
        else
           {
             const APL_Float tmp = abs_ALPHA / abs_GAMMA;
             const APL_Float scale = hypotenuse(1.0, tmp);

             SIN = - (conj_gamma / abs_GAMMA) / scale;
             COS =   (conj_alpha / abs_GAMMA) / scale;
             SEST = abs_SEST / scale;
           }
        return;
      }

   // normal case
   //
const APL_Float zeta1 = abs_ALPHA / abs_SEST;
const APL_Float zeta2 = abs_GAMMA / abs_SEST;
const APL_Float zeta = zeta1 + zeta2;

const APL_Float norma_1 = 1.0 + zeta1 * zeta;
const APL_Float norma_2 =       zeta2 * zeta;
const APL_Float norma = max(norma_1, norma_2);

const APL_Float test = 1.0 + 2.0*(zeta1 - zeta2)*(zeta1 + zeta2);
   if (test >= 0.0 )
      {
        // root is close to zero, compute directly
        //
        const APL_Float zeta2_2 = square(zeta2);
        const APL_Float b = 0.5*(square(zeta1) + zeta2_2 + 1.0);
        const APL_Float t = zeta2_2 / (b + sqrt(abs(b*b - zeta2_2)));
        SIN =   (ALPHA / abs_SEST) / (1.0 - t);
        COS = - (GAMMA / abs_SEST) / t;
        SEST *= sqrt(t + square(2.0*dlamch_E) * norma);
      }
   else
      {
        // root is closer to ONE, shift by that amount
        //
        const APL_Float b = 0.5 * (square(zeta2) + square(zeta1) - 1.0);
        APL_Float t;
        if (b >= 0.0)   t = -square(zeta1) / (b + hypotenuse(b, zeta1));
        else            t = b - hypotenuse(b, zeta1);

        SIN = - (ALPHA / abs_SEST) / t;
        COS = - (GAMMA / abs_SEST) / (1.0 + t);
        SEST *= sqrt(1.0 + t + square(2.0*dlamch_E) * norma);
      }

   normalize(SIN, COS);
}
//----------------------------------------------------------------------------
/** LApack function larfg. It generates one elementary reflector
    (aka. a Householder matrix)

   BACKGROUND:

   Let v be a K-by-1 vector (a column vector), and 
   let v° ←→ -⍉v ←→ -⍪v (a 1-by-K a row vector) aka, the conjugate
   transpose of v. NOTE: We use ° instead of * to avoid confusion with ×.

   Consider the linear transformation of a point x:

   x → x - 2 × (x,v) × v = x - 2 × v × (v° x)

   The Householder matrix H (aka. elementary reflector) is then:

   H = I - 2 × v × v° , where I is the identity matrix

   Since H is completely determined by v, LA_pack uses vector v instead of H.

   H has the following properties:

       H is hermetian,   i.e. H  = H°
       H is unitary,     i.e. H° = H⁻¹
       H is involutory,  i.e. H  = H⁻¹

       Therefore, for every (column vector) x:

       y = H x   ←→   H y = x

   END OF BACKGROUND

   LARFG generates a elementary reflector H of order n, such that

               ⎛alpha⎞   ⎛beta⎞
      H° × H × ⎜     ⎟ = ⎜    ⎟ ,   H° × H × H = I.
               ⎝  x  ⎠   ⎝  0 ⎠

   where alpha and beta are scalars, with beta real, and x is an (n-1)-element
   vector.

   Reflector H is represented in the form
                    ⎛ 1 ⎞
      H = I - tau × ⎜   ⎟ × ( 1 v° ) × H ,
                    ⎝ v ⎠

   where tau is a scalar of type T and v is a (n-1)-element vector of type T.
   Note that H is not hermitian (not H⁻¹ = H°). If the elements of x are
   all zero and alpha is real, then tau = 0 (and therefore H is the unit
   matrix I). Otherwise 1 <= real(tau) <= 2 and abs(tau-1) <= 1 . 

   Note also: The caller of larfg() i.e. laqp2() sets ALPHA = X[-1]. We
   set ALPHA inside larfg() rather than setting it before calling larfg()
   and passing it as parameter.
 */
template<typename T>
T LA_pack::larfg(Ccol N, T * X, size_t len_X)
{
   Assert(N > 0);
   if (N == 1)   return T(0.0);

   // ALPHA: geometrical length of vector X (NOT len_X!)
   // BETA:  geometrical length of vector ALPHA,X
   //
T & ALPHA = X[-1];   // ⍺ is the diagonal element of A above X

APL_Float ALPHA_r = get_real(ALPHA);
APL_Float ALPHA_i = get_imag(ALPHA);
const APL_Float norm2_X = norm_2(X, len_X);   // ║X║²

   if (ALPHA_i == 0.0 &&                  // ALPHA is real, and
       norm2_X == 0.0)   return T(0.0);   // all x are 0, then H = I.

APL_Float BETA_abs = sqrt(square(ALPHA) + norm2_X);   // length of ║ALPHA, X║
APL_Float BETA = ALPHA_r < 0.0 ? BETA_abs : -BETA_abs;

   // scale small BETA (and, with it, X) so that it can be used safely
   //
int kcnt = 0;
   if (abs(BETA) < safe_min)
      {
        const APL_Float inv_safe_min = 1.0 / safe_min;   // 4.98959E291
        while (abs(BETA) < safe_min)
           {
             ++kcnt;
             scale(X, len_X, inv_safe_min);    // scale x
             BETA *= inv_safe_min;      // scale BETA
             ALPHA_r *= inv_safe_min;   // scale real(ALPHA)
             ALPHA_i *= inv_safe_min;   // scale imag(ALPHA)
           }

        set_real(ALPHA, ALPHA_r);   // update ALPHA
        set_imag(ALPHA, ALPHA_i);   // update ALPHA
        BETA_abs = sqrt(square(ALPHA) + norm_2(X, len_X));
        BETA = ALPHA_r < 0.0 ? BETA : -BETA;
      }

T           tau( (  BETA   - ALPHA_r) / BETA);
   set_imag(tau, ( /*0.0*/ - ALPHA_i) / BETA);   // imag(real BETA) = 0.0

   scale(X, len_X, T(1.0) / (ALPHA - BETA));   // scale X
   loop(k, kcnt)   BETA *= safe_min;           // un-scale small beta

   ALPHA = T(BETA);

   // print_vector("reflector V returned by larfg()", X, len_X);
   //
   return tau;
}
//----------------------------------------------------------------------------
/// LApack function trsm. Solves op(A) * X = alpha * B
//  The result is stored in the first NRHS columns of B
//  Only the special case needed for A⌹B is implemented.

template<typename T>
void LA_pack::trsm(const Matrix<T> & A, Matrix<T> & B, Ccol NRHS)
{
   /* only: SIDE   = 'Left'              → lside == true
            UPLO   = 'Upper'             → upper = true
            TRANSA = 'No transpose'      → A is not conjugate transposed
            DIAG   = 'Non-unit' and      → nounit = true
            ALPHA  = 1.0                 → no scaling
            op     = A                   → neither A°∘T nor A°∘H
     
      is implemented! is upper triangular, therefore the computation
      of B starts at the last equation xₙ = bₙ  and moves upwards by
      repeatedly subtracting the xₙ column on the right side from the
      b column on the left side. In every step:

       a₁₁ x₁ + a₁₂ x₂ + ... + a₁ₙ xₙ₋₁ + a₁ₙ xₙ = b₁
          0   + a₂₂ x₂ + ... + a₂ₙ xₙ₋₁ + a₂ₙ xₙ = b₂
                        ...         
          0   +    0   + ... + aₙₙ xₙ₋₁ + aₙₙ xₙ = bₙ

       becomes:

       a₁₁ x₁ + a₁₂ x₂ + ... + a₁ₙ xₙ₋₁       = b₁   - a₁ₙ     (bₙ ÷ aₙₙ)
          0   + a₂₂ x₂ + ... + a₂ₙ xₙ₋₁       = b₂   - a₂ₙ     (bₙ ÷ aₙₙ)
                                             ...
          0   +    0   + ... + a₍ₙ₋₁₎ₙ xₙ₋₁   = bₙ₋₁ - a₍ₙ₋₁₎ₙ (bₙ ÷ aₙₙ)

       and the scalar result xₙ = bₙ ÷ aₙₙ in each step is stored in B[col;n].
    */
   ALL_COLS(NRHS)
   REV_COLS(A.get_column_count())
      {
        T & B_kj = B.at(k, col);   // result bₙ
        if (is_nonzero(B_kj))
           {
             B_kj /= A.diag(k);
             ALL_ROWS(k)   B.at(row, col) -= B_kj * A.at(row, k);
           }
      }
}
//----------------------------------------------------------------------------
/// LApack function ila_lc
template<typename T>
Ccol LA_pack::ila_lc(Crow M, const Matrix<T> & C)
{
   /* return the smallest col so that C(1:M, col:N) is the null matrix:
 

       ├──────── N ────────┤
       ╔═════════════╤═════╗ ┬
       ║             │     ║ │
       ║           ≠0│  0  ║ M
       ║      C<T>   │     ║ │
       ║             └─────╢ ┴
       ║            ↑ ↑    ║
       ╚════════════│═│════╝
                    │ │
                    │ └── ila_lc(M, N, C)
                    └──── !column.is_null(M)   (last non-0 column
    */

const Ccol N = C.get_column_count();

   REV_COLS(N)
   ALL_ROWS(M)
      if (is_nonzero(C.at(row, k)))   return k + 1;

   // all columns are 0
   //
   return 0;
}
//----------------------------------------------------------------------------
/** LApack function gemv. Normally computes one of:

    y := alpha × A      × x + beta × y   (case 1) or
    y := alpha × A° × T × x + beta × y   (case 2) or
    y := alpha × A° × H × x + beta × y   (case 3)   <---

   Constants alpha=1.0 and beta=0.0 were removed, and only the case 3
   above is needed (and implemented). That is:

   y := A° × H × x

 */
template<typename T>
inline void LA_pack::gemv(int M, int N, const Matrix<T> & A,   // A°
                          const T * x, size_t len_x,           // H 
                          T * y, size_t len_Y)                 // resilt
{
   ALL_COLS(N)
       {
         T tmp(0.0);
         ALL_ROWS(M)   tmp += conjugated(A.at(row, col)) * x[row];
         y[col] = tmp;
       }
}
//----------------------------------------------------------------------------
/// LApack function gerc: C := alpha × x × y° × H + C.
template<typename T>
void LA_pack::gerc(Crow M, Ccol N, T ALPHA, const T * x, size_t len_X,
                   const T * y, size_t len_Y, Matrix<T> & C)
{
   if (!(M && N && is_nonzero(ALPHA)))   return;

   ALL_COLS(N)
      {
        if (is_zero(y[col]))   continue;

        const T Ay = conjugated(y[col]) * ALPHA;       // alpha * y°
        ALL_ROWS(M)   C.at(row, col) += Ay * x[row];   //
      }
}
//----------------------------------------------------------------------------
/** LApack function larf: applies the elementary reflector H to the
    rectangular matrix C.

   LARF applies a elementary reflector H to an M-by-N matrix C,
   from either the left or the right. H is represented in the
   form

       H = I - tau × v × v° × H

   where tau is a scalar and v is a vector.

   NOTE: If tau = 0, then H is the unit matrix.

   To apply H° × H, supply conjg(tau) instead.

   C is overwritten in place by the matrix H * C.
 */
template<typename T>
void LA_pack::larf(const T * v, size_t len_v, T tau, Matrix<T> & C)
{
   if (is_zero(tau))   return;   // H is I.

const Crow M = C.get_row_count();
const Ccol N = C.get_column_count();

   // NOTE only SIDE == "Left" is needed (and implemented here).

   /*
                  ├──────N──────┤
          ┌─┐     ╔══════C═╤════╗ ┬
          │v│     ║        │0 0 ║ │
          │v│     ║       c│0 0 ║ │
          │v│     ║        │0 0 ║ M   v≠0, c≠0
    lastV→│v│ → → ╟────────┘0 0 ║ │
      ↑   │0│     ║       │     ║ │
      ↑   │0│     ║       │     ║ │
      M   └─┘     ╚═══════│═════╝ ┴
                        lastC ← N
    */

Crow lastV = M;   // index of the last non-zero item in v.

   // find the last non-zero item (row) in vector v
   //
   while (lastV && is_zero(v[lastV - 1]))   --lastV;
   if (lastV == 0)   return;   // if v is the null vector

   // Scan for the last non-zero column in C(1:lastV,1:lastV)
   //
const Ccol lastC = ila_lc<T>(lastV, C);

T * y = reinterpret_cast<T *>(work_larf);

   gemv<T>(lastV, lastC, C,    v, len_v, y, N);      // y := A° × H × x
   gerc<T>(lastV, lastC, -tau, v, len_v, y, N, C);
}
//----------------------------------------------------------------------------
/*  LApack function laqp2. Compute a QR factorization with column pivoting
    of the matrix block

   vn1_vn2 (aka. work) is the concatenation WORK, RWORK in FORTRAN
 */
template<typename T>
void LA_pack::laqp2(Matrix<T> & A, Ccol * pivot, T * tau, APL_Float * vn1_vn2)
{
const Crow M = A.get_row_count();
const Ccol N = A.get_column_count();
   Assert(M >= N);

#define vn1 vn1_vn2   /* since vn1_vn2 = vn1, vn2 */
APL_Float * const vn2 = vn1 + N;

   ALL_COLS(N)
      {
        const int rc_A1 = col + 1;   // the next column (right of col)

        // pvt_0 is the column at or right of col that has the largest vn1.
        // If col already has the largest norm: OK, otherwise exchange
        // columns col and pvt_0 of a and their column norms
        //
        const int pvt_0 = col + max_pos(vn1 + col, N - col);
        if (pvt_0 != col)   // unless col already is the pivot column
           {
             A.exchange_columns(pvt_0, col);
             exchange(pivot[pvt_0], pivot[col]);   // remember the exchange
             vn1[pvt_0] = vn1[col];
             vn2[pvt_0] = vn2[col];
           }

        /* Generate elementary reflector H(col). The reflector
           is stored in row rc_A1 of A, starting at column col:
           
                0→ → → col → → N
                ↓  ╔════ A═══╗
                ↓  ║ \       ║
                ↓  ║  \      ║
             rc_A1 ║   ○▒▒X▒▒║   ○: ALPHA aka. X[-1] of reflector X
                ↓  ║    │    ║      len_X is the reflector length
                ↓  ║    │ \  ║      (=number fo items right of ALPHA)
                N  ║    │  \ ║ 
                M  ╚════╪════╝
                      col+1     
         */
        tau[col] = 0.0;   // assume row rc_A1 is invalid
        if (const int len_X = M - rc_A1)   // reflector length
           {
             tau[col] = larfg<T>(M - col, &A.at(rc_A1, col), len_X);
           }

        if (rc_A1 < N)   // if (col + 1) is a valid column of A
           {
             /* Apply H(i)° × H to A(offset+i:m, i+1:n) from the left.

                0→ → → col → → N
                ↓  ╔════╪═══════╗
                ↓  ║ \          ║
                ↓  ║  \    A    ║
                ↓  ║   \        ║ 
               col ║    ○┌──────╢   ○: Acc = A.diag(col) := 1.0
                ↓  ║     │      ║ 
                ↓  ║     │ SUB  ║ 
                N  ║     │      ║ 
                M  ╚═════╪══════╝
                       col+1     
             */
             const T Acc = A.diag(col);   // save diag (ALPHA)
             A.diag(col) = APL_Float(1.0);
                Matrix<T> SUB = A.sub_matrix(col, rc_A1);
                larf<T>(&A.diag(col), M - col, conjugated(tau[col]), SUB);
             A.diag(col) = Acc;           // restore diag
           }

        // Update the partial column norms vn1 and vn2.
        //
        for (Ccol c = rc_A1; c < N; ++c)
            {
              if (vn1[c] == 0.0)   continue;

              const APL_Float abs_A = abs(A.at(col, c)) / vn1[c];
              const APL_Float temp = max(1.0 - square(abs_A), 0.0);
              const APL_Float quot = vn1[c] / vn2[c];
              if (temp * quot * quot <= tol3z)   // temp and/or quot too small
                 {
                   vn1[c] = 0.0;    // assume invalid row rc_A1
                   if (rc_A1 < M)   // valid row rc_A1
                      {
                        // vn1[c] = length of the row vector in A that
                        // starts at row rc_A1 and columns c.
                        //
                        vn1[c] = sqrt(norm_2(&A.at(rc_A1, c), M - rc_A1));
                      }
                   vn2[c] = vn1[c];
                 }
              else                                 // "normal" quot
                 {
                   vn1[c] *= sqrt(temp);
                 }
            }
      }
#undef vn1   // alias for vn1_vn2
}
//----------------------------------------------------------------------------
template<typename T>
char * LA_pack::allocate_workspace(Ccol N)
{
enum Bytes_per_N
   {
     bytes_gelsy         = sizeof(Ccol)   // pivot
                         + sizeof(T),     // tau
     bytes_geqp3         = sizeof(T)      // vn1
                         + sizeof(T),     // vn2
     bytes_estimate_rank = sizeof(T)      // min
                         + sizeof(T),     // max
     bytes_shared        = bytes_geqp3      > bytes_estimate_rank
                         ? bytes_geqp3      : bytes_estimate_rank,
     bytes_larf          = sizeof(T),     // y
     bytes_per_N         = bytes_gelsy
                         + bytes_shared    // geqp3() or estimate_rank()
                         + bytes_larf
   };

char * work = new char[N *  bytes_per_N];
  work_gelsy = work;
  work_geqp3 = work_gelsy + N * bytes_gelsy;
  work_larf  = work_geqp3 + N * bytes_geqp3;
   return work;
}
//============================================================================
