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

/** @file
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "Common.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "Value.hh"
#include "Workspace.hh"

using namespace std;

#include "LApack.hh"

char * LA_pack::work_gelsy = 0;
char * LA_pack::work_geqp3 = 0;   // geqp3() and estimate_rank()
char * LA_pack::work_larf = 0;

/*
   Notation: in the literature the conjujate transpose of a vector v resp.
   a matrix M is usually v* resp. M*. To avoid confusion with * we use
   v° resp, M° for the conjujate transpose and the APL × for *.

   Call tree:

   divide_matrix(Z, A, B)
      │
      └─── gelsy<T>(A, B, rcond)
               │
               └─── scaled_gelsy(A, B, rcond)
                       │
                       ├─── geqp3<T>(A, pivot, tau)
                       │       │
                       │       └─── laqp2<T>(A, pivot, tau, vn1_vn2)
                       │               │
                       │               ├─── larfg<T>(N, X)
                       │               └─── larf<T>(v, len_v, tau, C)
                       │
                       ├─── estimate_rank(A, rcond)
                       │
                       ├─── unm2r<T>(N, A, tau, B)
                       │       │
                       │       ├─── larf<T>(v, 1, tau, C)
                       │       ├─── ...
                       │       └─── larf<T>(v, N, tau, C)
                       │
                       └─── trsm<T>(A, B1)
                   
 */
//----------------------------------------------------------------------------
/// the implementation of Z←A⌹B.
/// Z[;j] is the solution of A[;j] = B +.× Z[j]
void
divide_matrix(Value & Z, bool need_complex, ShapeItem rows,
              ShapeItem cols_A, const Cell * cA,
              ShapeItem cols_B, const Cell * cB)
{
   // the following has been checked by the caller:
   //
   // rows_B >= cols_B and
   // rows_A == rows_B  (aka. rows below)
   //
const APL_Float rcond = Workspace::get_CT();
const size_t items_A = rows * cols_A;
const size_t items_B = rows * cols_B;
const int FpI = need_complex ? 2 : 1;   // Floats per Item

#define FREE_AB(ERROR) { free(free_A);   free(free_B);   ERROR; }

void * free_A = malloc(items_A * FpI * sizeof(APL_Float));
void * free_B = malloc(items_B * FpI * sizeof(APL_Float));
   if (free_A == 0 || free_B == 0)   FREE_AB(WS_FULL)

APL_Float * fpA = reinterpret_cast<APL_Float *>(free_A);
APL_Float * fpB = reinterpret_cast<APL_Float *>(free_B);

   ALL_COLS(cols_A)
       {
         if (need_complex)
            {
              LA_pack::ZZ * const a = reinterpret_cast<LA_pack::ZZ *>(fpA);
              ALL_ROWS(rows)
                  {
                    const Cell & src_A = cA[row * cols_A + col];
                    new (a + row) LA_pack::ZZ(src_A.get_real_value(),
                                              src_A.get_imag_value());
                  }

              LA_pack::ZZ * const b = reinterpret_cast<LA_pack::ZZ *>(fpB);
              LA_pack::ZZ * bb = b;

              // initialize b[] in FORTRAN (aka. column major) order,
              // which is ⍉ APL (aka. row major) order. I.e. use ⍉B.
              //
              ALL_COLS(cols_B)
              ALL_ROWS(rows)
                 {
                   const Cell & src_B = cB[col + row*cols_B];
                   new (bb++) LA_pack::ZZ(src_B.get_real_value(),
                                          src_B.get_imag_value());
                 }

              LA_pack::Matrix<LA_pack::ZZ> B(b, rows, cols_B, /* LDB */ rows);
              LA_pack::Matrix<LA_pack::ZZ> A(a, rows, 1,      /* LDA */ rows);
              const sRank rank = LA_pack::Zgelsy(B, A, rcond);
              if (rank != cols_B)
                 {
                   MORE_ERROR() << "A⌹B : linear dependent (complex) B?";
                   FREE_AB(DOMAIN_ERROR)
                 }

              // cols_A = rows_Z. We have computed the result for col c of A
              // which is row c of Z.
              //
              ALL_ROWS(cols_B)
                  Z.set_ravel_Complex(row*cols_A + col, a[row].real(),
                                                        a[row].imag());
            }
         else   // real
            {
              ALL_ROWS(rows)
                 {
                    const Cell & src_A = cA[row*cols_A + col];
                    fpA[row] = src_A.get_real_value();
                 }

              APL_Float * bb = fpB;

              // initialize b[] in FORTRAN (aka. column major) order,
              // which is ⍉ APL (aka. row major) order. I.e. use ⍉B.
              //
              ALL_COLS(cols_B)
              ALL_ROWS(rows)
                 {
                   const Cell & src_B = cB[col + row*cols_B];
                   *bb++ = src_B.get_real_value();
                 }

              const APL_Float rcond = Workspace::get_CT();

                LA_pack::Matrix<LA_pack::DD> B(fpB, rows, cols_B, /*LDB*/ rows);
                LA_pack::Matrix<LA_pack::DD> A(fpA, rows, 1,    /* LDA */ rows);
                const sRank rank = LA_pack::Dgelsy(B, A, rcond);
                if (rank != cols_B)
                   {
                     MORE_ERROR() << "A⌹B : linear dependent (real) B?";
                     FREE_AB(DOMAIN_ERROR)
                   }

              // cols_A = rows_Z. We have computed the result for col c of A
              // which is row c of Z.
              //
              ALL_ROWS(cols_B)   Z.set_ravel_Float(row*cols_A + col, fpA[row]);
            }
       }

   free(free_A);
   free(free_B);
}

   /* numerical limits
     
      DLAMCH determines double precision machine parameters.
      We (only) #define those that we need. Precision is not cruical
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
    LApack functions dgelsy and zgelsy:

    GELSY computes the minimum-norm solution to a complex linear least
    squares problem:

    minimize ║ A × X - B ║

    using a complete orthogonal factorization of A.  A is an M-by-N
    matrix which may be rank-deficient.

    Several right hand side vectors b and solution vectors x can be
    handled in a single call; they are stored as the columns of the
    M-by-NRHS right hand side matrix B and the N-by-NRHS solution
    matrix X.

    The routine first computes a QR factorization with column pivoting:

    A × P = Q × ⎡ R₁₁ R₁₂ ⎤
                ⎣  0  R₂₂ ⎦

    with R₁₁ defined as the largest leading submatrix whose estimated
    condition number is less than 1/RCOND. The order of R11, i.e. RANK,
    is the effective rank of A.

    Then, R22 is considered to be negligible, and R12 is annihilated
    by unitary transformations from the right, arriving at the
    complete orthogonal factorization:

    A × P = Q × ⎡ T₁₁ 0 ⎤ × Z
                ⎣  0  0 ⎦

    The minimum-norm solution is then

    X = P × Z° × H [ inv(T11) × Q1 * × H × B ]
                     [        0         ]

    where Q₁ consists of the first RANK columns of Q.

     This routine is basically identical to the original xGELSX except
     three differences:

       • The permutation of matrix B (the right hand side) is faster and
         more simple.

       • The call to the subroutine xGEQPF has been substituted by the
         the call to the subroutine xGEQP3. This subroutine is a Blas-3
         version of the QR factorization with column pivoting.

       • Matrix B (the right hand side) is updated with Blas-3.

 */
template<typename T>
int LA_pack::gelsy(Matrix<T> & A, Matrix<T> & B, APL_Float rcond)
{
const Crow M = A.get_row_count();
const Ccol N = A.get_column_count();

   // APL is responsible for handling the empty cases
   //
   Assert(M && N && B.get_column_count() && N <= M);

   // For a better precision, scale A and B so that their max. element lies
   // between small_number and big_number. Then call scaled_gelsy() and
   // scale the result back by the same factors.
   //
APL_Float un_scale_A = 1.0;
   {
     T * const A00 = &A.diag(0);
     const size_t MN = M * N;
     const APL_Float max_A = max_item(A00, MN);

     if (max_A == 0.0)              return 0;   // A is the 0-matrix
     if (max_A < small_number)      scale(A00, MN, un_scale_A = big_number);
     else if (max_A > big_number)   scale(A00, MN, un_scale_A = small_number);
   }

APL_Float un_scale_B = 1.0;
   {
     T * const B00 = &B.diag(0);
     const size_t MN = B.get_row_count() * B.get_column_count();
     const APL_Float max_B = max_item(B00, MN);

     if (max_B < small_number)      scale(B00, MN, un_scale_B = big_number);
     else if (max_B > big_number)   scale(B00, MN, un_scale_B = small_number);
   }

   // memory allocation
   //
   enum Bytes_per_N
      {
        bytes_gelsy         = sizeof(Ccol)   // pivot
                            + sizeof(T),     // tau
        bytes_geqp3         = sizeof(T)      // vn1
                            + sizeof(T),     // vn2
        bytes_estimate_rank = sizeof(T)      // min
                            + sizeof(T),     // max
        bytes_shared        = bytes_geqp3 > bytes_estimate_rank
                            ? bytes_geqp3 : bytes_estimate_rank,
        bytes_larf          = sizeof(T),     // y
        bytes_per_N         = bytes_gelsy
                            + bytes_shared    // geqp3() or estimate_rank()
                            + bytes_larf
      };

   //  we allocate the total workspace for every function (i.e. gelsy,
   //  geqp3, and larf) at this level; each function then further divides
   //  its worksapace further. For example: we allocate N * bytes_gelsy
   //  here and gelsy() divides it further into pivot, tau, and tmp.
   //
char * const work = new char[N * bytes_per_N];
   if (work == 0)   WS_FULL;

   work_gelsy = work;
   work_geqp3 = work_gelsy + N * bytes_gelsy;
   work_larf  = work_geqp3 + N * bytes_geqp3;

   {
     const int RANK = scaled_gelsy(A, B, rcond);
     if (RANK < N)   // this is an error
        {
          delete[] work;
          return RANK;
        }
   }

   // Undo scaling
   //
   if (un_scale_A != 1.0)
      {
        Matrix<T> A1 = A.take(N, N);
        Matrix<T> B1 = B.take(N, B.get_column_count());
        scale(&B1.diag(0), APL_Float(1.0/un_scale_A), N*B.get_column_count());
        scale(&A1.diag(0), un_scale_A, N*N);
      }

   if (un_scale_B != 1.0)
      {
        Matrix<T> B1 = B.take(N, B.get_column_count());
        scale(&B1.diag(0), un_scale_B, N * B.get_column_count());
      }

   delete[] work;
   return N;   // success
}
//----------------------------------------------------------------------------
///  scaled_gelsy() computes gelsy() with A and B scaled nicely
template<typename T>
int LA_pack::scaled_gelsy(Matrix<T> & A, Matrix<T> & B, double rcond)
{
   // this gelsy is optimized for (and restricted to) the following conditions:
   //
   // 0 < N <= M  →  min_NM = N and max_MN = M
   // 0 < NRHS
   //
const Ccol N    = A.get_column_count();
const Ccol NRHS = B.get_column_count();

   // Compute QR factorization with column pivoting of A:
   // A × P = Q × R
   //

   // split work_gelsy into pivot, tau, and tmp
   //
Ccol * const pivot = reinterpret_cast<Ccol *>(work_gelsy);   // pivot[N]
T    * const tau   = reinterpret_cast<T *>(pivot + N);       // tau[N]

   geqp3<T>(A, pivot, tau);

   // Details of Householder rotations stored in WORK(1:MN)...
   //
   // Determine RANK using incremental condition estimation
   //
   {
     const int RANK = estimate_rank(A, rcond);
     if (RANK < N)   return RANK;
   }

   // from here on, RANK == N. We leave RANK in the comments but use N in
   // the code.

   // Logically partition R = [ R11 R12 ]
   //                         [  0  R22 ]
   // where R11 = R(1:RANK, 1:RANK)
   // [R11, R12] = [ T11, 0 ] × Y
   //

   // Details of Householder rotations stored in WORK(MN+1:2*MN)
   // 
   // B(1:M, 1:NRHS) := Q° × H × B(1:M, 1:NRHS)
   // 
   unm2r<T>(N, A, tau, B);

   // B(1:RANK, 1:NRHS) := reciprocal(T11) * B(1:RANK,1:NRHS)
   //
   {
     Matrix<T> B1 = B.take(B.get_row_count(), NRHS);
     trsm<T>(A, B1);
   }

   // tmp[N]: A column of B, permuted by pivot. FORTRAN uses a separate tmp[N]. 
   // However, tau is no longer needed, so we can reuse it here. The loop below
   // undoes the sorting of columns performed in geqp3() / laqp2().
   //
   ALL_COLS(NRHS)   // for every column of B
      {
        ALL_ROWS(N)   tau[pivot[row]] = B.at(row, col);
        ALL_ROWS(N)   B.at(row, col) = tau[row];
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
   of matrix A:  A × P = Q × R
 */
template<typename T>
void LA_pack::geqp3(Matrix<T> & A, Ccol * pivot, T * tau)
{
const Crow M = A.get_row_count();
const Ccol N = A.get_column_count();

   // init the column permutation for pivoting.
   // start with the identical permutation
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
/* LApack function UNM2R. Overwrite the general complex m-by-n matrix C with:

   Q × C         if SIDE = 'L' and TRANS = 'N', or   (case 1)
   Q° × H × C    if SIDE = 'L' and TRANS = 'C', or   (case 2)
   C × Q         if SIDE = 'R' and TRANS = 'N', or   (case 3)
   C × Q° × ×    if SIDE = 'R' and TRANS = 'C',      (case 4)

   where Q is a unitary matrix defined as the product of k
   elementary reflectors:

   Q = H(1) H(2) ... H(k)
 */

template<typename T>
void LA_pack::unm2r(Crow K, Matrix<T> & A, const T * tau, Matrix<T> & C)
{
const Crow M = C.get_row_count();

   // only case 2 (SIDE == "L" and TRANS = 'T' or 'C' is implemented,
   // thus NOTRAN is false (and then tau[row] is conjugated).
   //
   ALL_ROWS(K)
       {
         // H(i) or H(i)° × H is applied to C(i:m,1:n)
         //
         const int MM = M - row;
         const T tau_row = conjugated(tau[row]);
         const T Aii = A.diag(row);   // remember A(row, row)
             A.diag(row) = APL_Float(1.0);
             Matrix<T> SUB = C.sub_matrix(row, 0);
             larf<T>(&A.diag(row), MM, tau_row, SUB);
         A.diag(row) = Aii;           // restore A(row, row);
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
/** LApack function larfg. It generates an elementary reflector
    (aka. a Householder matrix)

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
 */
template<typename T>
T LA_pack::larfg(Ccol N, T * X, size_t len_X)
{
   assert(N > 0);

   if (N == 1)   return T(0.0);

T & ALPHA = X[-1];

APL_Float alpha_r = get_real(ALPHA);
APL_Float alpha_i = get_imag(ALPHA);
const APL_Float norm2_X = norm_2(X, len_X);

   if (alpha_i == 0.0 && norm2_X == 0.0)   return T(0.0);

APL_Float beta_abs = hypotenuse(ALPHA, sqrt(norm2_X));
APL_Float beta = alpha_r < 0.0 ? beta_abs : -beta_abs;

   // scale small beta (and, with it, X) so that it can be used safely
   //
int kcnt = 0;
   if (abs(beta) < safe_min)
       {
         const APL_Float inv_safe_min = 1.0 / safe_min;   // 4.98959E291
         while (abs(beta) < safe_min)
            {
              ++kcnt;
              scale(X, len_X, inv_safe_min);    // scale x
              beta *= inv_safe_min;      // scale beta
              alpha_r *= inv_safe_min;   // scale real(ALPHA)
              alpha_i *= inv_safe_min;   // scale imag(ALPHA)
            }

         set_real(ALPHA, alpha_r);   // update ALPHA
         set_imag(ALPHA, alpha_i);   // update ALPHA
         beta_abs = hypotenuse(ALPHA, sqrt(norm_2(X, len_X)));
         beta = alpha_r < 0.0 ? beta : -beta;
       }

T tau;
   set_real(tau, (beta - alpha_r) / beta);
   set_imag(tau, -alpha_i         / beta);

   // un-scale X and small beta
   //
   scale(X, len_X, reciprocal(ALPHA - beta));
   loop(k, kcnt)   beta *= safe_min;

   set_real(ALPHA, beta);
   set_imag(ALPHA, 0.0);

   return tau;
}
//----------------------------------------------------------------------------
/// LApack function trsm. Solves op(A) * X = alpha * B

template<typename T>
void LA_pack::trsm(const Matrix<T> & A, Matrix<T> & B)
{
   // only: SIDE   = 'Left'              - lside == true
   //       UPLO   = 'Upper'             - upper = true
   //       TRANSA = 'No transpose'      - 
   //       DIAG   = 'Non-unit' and      - nounit = true
   //       ALPHA  = 1.0 is implemented!
   //
   ALL_COLS(B.get_column_count())
   REV_COLS(A.get_column_count())
      {
        T & B_kj = B.at(k, col);
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
    y := alpha × A° × H × x + beta × y   (case 3)

   Constants alpha=1.0 and beta=0.0 were removed, and only the case 3
   is needed (and implemented). That is:

   y := A° × H × x

 */
template<typename T>
inline void LA_pack::gemv(int M, int N, const Matrix<T> & A,
                          const T * x, size_t len_x, T * y, size_t len_Y)
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
   if (M && N && is_nonzero(ALPHA))
      {
        ALL_COLS(N)
           {
             if (is_zero(y[col]))   continue;

             const T Ay = conjugated(y[col]) * ALPHA;       // alpha * y°
             ALL_ROWS(M)   C.at(row, col) += Ay * x[row];   //
           }
      }
}
//----------------------------------------------------------------------------
/** LApack function larf: applies an elementary reflector to a general
    rectangular matrix C.

   LARF applies a elementary reflector H to an M-by-N matrix C,
   from either the left or the right. H is represented in the
   form

       H = I - tau × v × v° × H

   where tau is a scalar and v is a vector.

   If tau = 0, then H is taken to be the unit matrix.

   To apply H° × H, supply conjg(tau) instead.

   C is overwritten in place by the matrix H * C.
 */
template<typename T>
void LA_pack::larf(const T * v, size_t len_v, T tau, Matrix<T> & C)
{
const Crow M = C.get_row_count();
const Ccol N = C.get_column_count();

   // NOTE only SIDE == "Left" is needed (and implemented here).
   //
Crow lastV = 0;   // index of the last non-zero item in v.
Ccol lastC = 0;   // rightmost column COL with a nonzero item in M↑COL

   if (is_nonzero(tau))   // unless H is the unit matrix
     {
       // Look for the last non-zero item (row) in vector V
       //
       lastV = M;
       while (lastV && is_zero(v[lastV - 1]))   --lastV;

       // Scan for the last non-zero column in C(1:lastv,:)
       //
       lastC = ila_lc<T>(lastV, C);
     }

   if (lastV)
      {
        T * y = reinterpret_cast<T *>(work_larf);

        gemv<T>(lastV, lastC, C,    v, len_v, y, N);      // y := A° × H × x
        gerc<T>(lastV, lastC, -tau, v, len_v, y, N, C);
      }
}
//----------------------------------------------------------------------------
/*  LApack function laqp2. Compute a QR factorization with column pivoting
    of the matrix block

   vn1_vn2 (aka. work) is the concatenation WORK, RWORK in FORTRAN

   Temporarily modifies A (for performance reasons), but does not change it.
 */
template<typename T>
void LA_pack::laqp2(Matrix<T> & A, Ccol * pivot, T * tau, APL_Float * vn1_vn2)
{
const Crow M = A.get_row_count();
const Ccol N = A.get_column_count();

#define vn1 vn1_vn2   /* vn1_vn2 = vn1, vn2 */
APL_Float * const vn2 = vn1 + N;

   ALL_COLS(N)
      {
        const int col_A1 = col + 1;   // the next column (right of col)

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

        // Generate elementary reflector H(i).
        //
        tau[col] = 0.0;    // assume col is the last (then col_A1 is invalid)
        if (col_A1 < M)   // no, col_A1 valid
           {
             const int len_X = M - col_A1;   // cols right of col
             tau[col] = larfg<T>(M - col, &A.at(col_A1, col), len_X);
           }

        if (col_A1 < N)   // unless last column
           {
             // Apply H(i)° × H to A(offset+i:m, i+1:n) from the left.
             //
             const T Aii = A.diag(col);   // save diag
             A.diag(col) = APL_Float(1.0);
                Matrix<T> C = A.sub_matrix(col, col_A1);
                larf<T>(&A.diag(col), M - col, conjugated(tau[col]), C);
             A.diag(col) = Aii;           // restore diag
           }

        // Update partial column norms vn1 and vn2.
        //
        for (int c = col_A1; c < N; ++c)
            {
              if (vn1[c] == 0.0)   continue;

              const APL_Float abs_A = abs(A.at(col, c)) / vn1[c];
              const APL_Float temp = max(1.0 - square(abs_A), 0.0);
              const APL_Float quot = vn1[c] / vn2[c];
              if (temp * quot * quot <= tol3z)   // temp and/or quot too small
                 {
                   vn1[c] = 0.0;   // assume invalid col_A1 (so col_A is last)
                   if (col_A1 < M)   // valid col_A1
                      {
                        vn1[c] = sqrt(norm_2(&A.at(col_A1, c), M - col_A1));
                      }
                   vn2[c] = vn1[c];
                 }
              else                                 // "normal" quot
                 {
                   vn1[c] *= sqrt(temp);
                 }
            }
      }
#undef vn1
}
//============================================================================
