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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Common.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "Output.hh"
#include "Value.hh"
#include "Workspace.hh"

using namespace std;

// the implementation of gelsy<T>
#include "LApack.hh"

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

void * free_A = malloc(items_A * FpI * sizeof(APL_Float));
void * free_B = malloc(items_B * FpI * sizeof(APL_Float));
   if (free_A == 0 || free_B == 0)
      {
         free(free_A);
         free(free_B);
         WS_FULL
      }

APL_Float * fpA = reinterpret_cast<APL_Float *>(free_A);
APL_Float * fpB = reinterpret_cast<APL_Float *>(free_B);

#define FREE_AB(ERROR) { free(free_A);   free(free_B);   ERROR; }

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

              {
                const APL_Float rcond = Workspace::get_CT();

                LA_pack::Matrix<LA_pack::DD> B(fpB, rows, cols_B, /*LDB*/ rows);
                LA_pack::Matrix<LA_pack::DD> A(fpA, rows, 1,    /* LDA */ rows);
                const sRank rank = LA_pack::Dgelsy(B, A, rcond);
                if (rank != cols_B)
                   {
                     MORE_ERROR() << "A⌹B : linear dependent (real) B?";
                     FREE_AB(DOMAIN_ERROR)
                   }
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

   // numerical limits
   //
   // DLAMCH determines double precision machine parameters.
   // We (only) #define those that we need.

#define dlamch_E 1.11022e-16
#define dlamch_S 2.22507e-308
#define dlamch_P 2.22045e-16
static const APL_Float small_number = dlamch_S / dlamch_P;    // 1.00208E¯292
static const APL_Float big_number   = 1.0 / small_number;     // 9.97923E291
static const APL_Float safe_min     =  dlamch_S / dlamch_E;   // 2.00417E¯292
static const APL_Float inv_safe_min = 1.0 / safe_min;         // 4.98959E291
static const APL_Float tol3z        = sqrt(dlamch_E);         // 1.05367E¯8

//----------------------------------------------------------------------------
///  DGELSY solves overdetermined or underdetermined systems for GE matrices 
template<typename T>
int LA_pack::gelsy(Matrix<T> & A, Matrix<T> &B, APL_Float rcond)
{
const Crow M    = A.get_row_count();
const Ccol N    = A.get_column_count();
const Ccol NRHS = B.get_column_count();

   // APL is responsible for handling the empty cases
   //
   Assert(M && N && NRHS && N <= M);

   // For a better precision, scale A and B so that their max. element lies
   // between small_number and big_number. Then call scaled_gelsy() and
   // scale the result back by the same factors.
   //
APL_Float scale_A = 1.0;
   {
     const APL_Float norm_A = A.max_norm();

     if (norm_A == 0.0)                return 0;   // A is rank-deficient
     else if (norm_A < small_number)   A.scale(scale_A = big_number);
     else if (norm_A > big_number)     A.scale(scale_A = small_number);
   }

APL_Float scale_B = 1.0;
   {
     const APL_Float norm_B = B.max_norm();

     if (norm_B == 0.0)                /* OK */ ;
     else if (norm_B < small_number)   B.scale(scale_B = big_number);
     else if (norm_B > big_number)     B.scale(scale_B = small_number);
   }

   {
     const int RANK = scaled_gelsy(A, B, rcond);
     if (RANK < N)   return RANK;   // this is an error
   }

   // Undo scaling
   //
   if (scale_A != 1.0)
      {
        Matrix<T> A1 = A.sub_len(N, N);
        Matrix<T> B1 = B.sub_len(N, NRHS);
        B1.scale(APL_Float(1.0/scale_A));
        A1.scale(scale_A);
      }

   if (scale_B != 1.0)
      {
        Matrix<T> B1 = B.sub_len(N, NRHS);
        B1.scale(scale_B);
      }

   return N;   // success
}
//----------------------------------------------------------------------------
///  scaled_gelsy() computes computes gelsy() with A and B scaled nicely
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
   // A * P = Q * R
   //
   // pivot_tau_tmp is a vector of Ccol pivot, real or complex tau, and
   // real or complex tmp in a single memory allcoation.
   //
char * pivot_tau_tmp = new char[N * (  sizeof(Ccol) +   // pivot[N]
                                       sizeof(T)    +   // tau[N]
                                       sizeof(T))];     // tmp[N]

  // N pivot items
  //
Ccol * pivot = reinterpret_cast<Ccol *>(pivot_tau_tmp);   // N pivots

   // N tau items (complex = double[2],
   //
T * tau = reinterpret_cast<T *>(pivot + N);               // N tau items

   // and N tmp items (complex = double[2]).
   //
T * tmp = tau + N;                                        // N tmp1 items

   geqp3<T>(A, pivot, tau);

   // Details of Householder rotations stored in WORK(1:MN).
   //
   // Determine RANK using incremental condition estimation
   //
   {
     const int RANK = estimate_rank(A, rcond);
     if (RANK < N)
        {
          delete[] pivot_tau_tmp;
          return RANK;
        }
   }

   // from here on, RANK == N. We leave RANK in the comments but use N in
   // the code.

   // Logically partition R = [ R11 R12 ]
   //                         [  0  R22 ]
   // where R11 = R(1:RANK,1:RANK)
   // [R11,R12] = [ T11, 0 ] * Y
   //

   // Details of Householder rotations stored in WORK(MN+1:2*MN)
   // 
   // B(1:M, 1:NRHS) := Q* * H * B(1:M,1:NRHS) where
   // Q* is the conjujate transpose (aka. -⍉Q) of Q
   // 
   unm2r<T>(N, A, tau, B);

   // B(1:RANK, 1:NRHS) := reciprocal(T11) * B(1:RANK,1:NRHS)
   //
   {
     Matrix<T> B1 = B.sub_len(B.get_dx(), NRHS);
     trsm<T>(A, B1);
   }

   // workspace: 2*MN+NRHS
   //
   // B(1:N,1:NRHS) := P * B(1:N,1:NRHS)
   //
   ALL_COLS(NRHS)
      {
        ALL_ROWS(N)   tmp[pivot[row]] = B.at(row, col);
        ALL_ROWS(N)   B.at(row, col) = tmp[row];
      }

   delete[] pivot_tau_tmp;
   return N;
}
//----------------------------------------------------------------------------
/// LApack function estimating the rank of \b A
template<typename T>
int LA_pack::estimate_rank(const Matrix<T> & A, APL_Float rcond)
{
   // Determine RANK using incremental condition estimation...

const Ccol N = A.get_column_count();

APL_Float smax = abs(A.diag(0));
APL_Float smin = smax;
   if (smax == 0.0)   return 0;

   // store minima in work_min[ 0 ... N]
   // store maxima in work_max == work_min[N ... 2N]
   //
APL_Float * work_1 = new APL_Float[4*N];
T * work_min = reinterpret_cast<T *>(work_1);
T * work_max = work_min + N;

   work_min[0] = T(1.0);
   work_max[0] = T(1.0);

   for (int RANK = 1; RANK < N; ++RANK)
       {
         T sin_min(0.0);
         T sin_max(0.0);
         T cos_min(0.0);
         T cos_max(0.0);
         const T gamma(A.diag(RANK));

         T alpha_min = 0.0;
         T alpha_max = 0.0;
         loop(r, RANK)
             {
               alpha_min += conjugated(work_min[r]* A.at(r, RANK));
               alpha_max += conjugated(work_max[r]* A.at(r, RANK));
             }

         laic1_MIN<T>(smin, alpha_min, gamma, sin_min, cos_min);
         laic1_MAX<T>(smax, alpha_max, gamma, sin_max, cos_max);

         if (smax*rcond > smin)   // done
            {
              delete[] work_1;
              return RANK;
            }

         loop(i, RANK)
              {
                work_min[i] = work_min[i] * sin_min;
                work_max[i] = work_max[i] * sin_max;
              }

         work_min[RANK] = cos_min;
         work_max[RANK] = cos_max;
       }

   delete[] work_1;
   return N;
}
//----------------------------------------------------------------------------
// LApack function geqp3
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
APL_Float * work = new APL_Float[2*N];
   ALL_COLS(N)
         {
           Vector<T> x(&A.at(0, col), M);
           work[N + col] = work[col] = x.norm();
         }

   laqp2<T>(A, pivot, tau, work);
   delete[] work;
}
//----------------------------------------------------------------------------
template<typename T>
void LA_pack::unm2r(Crow K, Matrix<T> & A, const T * tau, Matrix<T> & C)
{
const Crow M = C.get_row_count();

   // only SIDE == "Left" is implemented, and
   // only TRANS = 'T' or 'C' is implemented.
   // thus NOTRANS is always false.
   //
   ALL_ROWS(K)
       {
         // H(i) or H(i)**H is applied to C(i:m,1:n)
         //
         const int MM = M - row;

        //
         const T tau_i = conjugated(tau[row]);

         const T Aii = A.diag(row);   // remember A(row, row)
           A.diag(row) = APL_Float(1.0);
           Vector<T> v(&A.diag(row), MM);
           Matrix<T> SUB = C.sub_matrix(row, 0);
           larf<T>(v, tau_i, SUB);
         A.diag(row) = Aii;           // restore A(row, row);
       }
}
//----------------------------------------------------------------------------
/** LApack function laic1 (estimate largest singular value).
     apply one step of incremental condition estimation.

     SEST: largest estimated singular value of \b this matrix
 **/
template<typename T>
void LA_pack::laic1_MAX(APL_Float & SEST, T alpha, T GAMMA, T & SIN, T & COS)
{
const APL_Float eps = dlamch_E;
const APL_Float abs_alpha = abs(alpha);
const APL_Float abs_gamma = abs(GAMMA);
const APL_Float abs_estimate = abs(SEST);

   //    Estimating largest singular value ...

   //    special cases
   //
   if (SEST == 0.0)
      {
        const APL_Float s1 = max(abs_gamma, abs_alpha);
        if (s1 == 0.0)
           {
             SIN = T(0.0);
             COS = T(1.0);
             SEST = 0.0;
           }
        else
           {
             SIN = alpha / s1;
             COS = GAMMA / s1;

             const APL_Float tmp = hypotenuse(SIN, COS);
             SIN = SIN / tmp;
             COS = COS / tmp;
             SEST = s1*tmp;
           }
        return;
      }

   if (abs_gamma <= eps*abs_estimate)
      {
        SIN = T(1.0);
        COS = T(0.0);
        APL_Float tmp = max(abs_estimate, abs_alpha);
        const APL_Float s1 = abs_estimate / tmp;
        const APL_Float s2 = abs_alpha / tmp;
        SEST = tmp * hypotenuse(s1, s2);
        return;
      }

   if (abs_alpha <= eps*abs_estimate)
      {
        const APL_Float s1 = abs_gamma;
        const APL_Float s2 = abs_estimate;
        if (s1 <= s2)
           {
             SIN = T(1.0);
             COS = T(0.0);
             SEST = s2;
           }
        else
           {
             SIN = T(0.0);
             COS = T(1.0);
             SEST = s1;
           }
        return;
      }

   if (abs_estimate <= eps*abs_alpha || abs_estimate <= eps*abs_gamma)
      {
        const APL_Float s1 = abs_gamma;
        const APL_Float s2 = abs_alpha;
        if (s1 <= s2)
           {
             const APL_Float tmp = s1 / s2;
             const APL_Float scale = hypotenuse(1.0, tmp);
             SEST = s2*scale;
             SIN = (alpha / s2) / scale;
             COS = (GAMMA / s2) / scale;
           }
        else
           {
             const APL_Float tmp = s2 / s1;
             const APL_Float scale = hypotenuse(1.0, tmp);
             SEST = s1*scale;
             SIN = (alpha / s1) / scale;
             COS = (GAMMA / s1) / scale;
           }
        return;
      }

   // normal case
   //
const APL_Float zeta1 = abs_alpha / abs_estimate;
const APL_Float zeta2 = abs_gamma / abs_estimate;
const APL_Float b = (1.0 - zeta1*zeta1 - zeta2*zeta2)*0.5;
APL_Float t;

   if (b > 0.0)  t = zeta1 * zeta1 / (b + hypotenuse(b, zeta1));
   else          t = hypotenuse(b, zeta1) - b;

const T sine = -(alpha / abs_estimate) / t;
const T cosi = -(GAMMA / abs_estimate) / (1.0 + t);
const APL_Float tmp = hypotenuse(sine, cosi);
   SIN = sine / tmp;
   COS = cosi / tmp;
   SEST = sqrt(t + 1.0) * abs_estimate;
}
//----------------------------------------------------------------------------
/** LApack function laic1 (estimate smallest singular value).
      Results are: SEST, SIN, and COS
     apply one step of incremental condition estimation.

     SEST: smallest estimated singular value of \b this matrix
 **/
template<typename T>
void LA_pack::laic1_MIN(APL_Float & SEST, T ALPHA, T GAMMA, T & SIN, T & COS)
{
const APL_Float eps = dlamch_E;
const APL_Float abs_alpha = abs(ALPHA);
const APL_Float abs_gamma = abs(GAMMA);
const APL_Float abs_estimate = abs(SEST);

   //    Estimating smallest singular value ...
   //

   //    special cases
   //
   if (SEST == 0.0)
      {
        SEST = 0.0;
        T sine(1.0);
        T cosi(0.0);
        if (abs_gamma > 0.0 || abs_alpha > 0.0)
           {
             sine = -conjugated(GAMMA);
             cosi =  conjugated(ALPHA);
           }

        const APL_Float abs_sine = abs(sine);
        const APL_Float abs_cosi = abs(cosi);
        const APL_Float s1 = max(abs_sine, abs_cosi);
        const T sine_s1 = sine / s1;
        const T cosi_s1 = cosi / s1;
        SIN = sine_s1;
        COS = cosi_s1;
        const APL_Float tmp = hypotenuse(sine_s1, cosi_s1);
        SIN = SIN / tmp;
        COS = COS / tmp;
        return;
      }

   if (abs_gamma <= eps*abs_estimate)
      {
        SIN = T(0.0);
        COS = T(1.0);
        SEST = abs_gamma;
        return;
      }

   if (abs_alpha <= eps*abs_estimate)
      {
        const APL_Float s1 = abs_gamma;
        const APL_Float s2 = abs_estimate;

        if (s1 <= s2)
          {
            SIN = T(0.0);
            COS = T(1.0);
            SEST = s1;
          }
        else
          {
            SIN = T(1.0);
            COS = T(0.0);
            SEST = s2;
          }
        return;
      }

   if (abs_estimate <= eps*abs_alpha || abs_estimate <= eps*abs_gamma)
      {
        const APL_Float s1 = abs_gamma;
        const APL_Float s2 = abs_alpha;
        const T conj_gamma = conjugated(GAMMA);
        const T conj_alpha = conjugated(ALPHA);

        if (s1 <= s2)
           {
             const APL_Float tmp = s1 / s2;
             const APL_Float scale = hypotenuse(1.0, tmp);
             const APL_Float tmp_scale = tmp / scale;

             SEST = abs_estimate * tmp_scale;
             SIN = - (conj_gamma / s2) / scale;
             COS =   (conj_alpha / s2) / scale;
           }
        else
           {
             const APL_Float tmp = s2 / s1;
             const APL_Float scale = hypotenuse(1.0, tmp);

             SEST = abs_estimate / scale;
             SIN = - (conj_gamma / s1) / scale;
             COS =   (conj_alpha / s1) / scale;
           }
        return;
      }

   // normal case
   //
const APL_Float zeta1 = abs_alpha / abs_estimate;
const APL_Float zeta2 = abs_gamma / abs_estimate;
const APL_Float zeta12 = zeta1 * zeta2;

const APL_Float norma_1 = 1.0 + square(zeta1) + zeta12;
const APL_Float norma_2 =       square(zeta2) + zeta12;
const APL_Float norma = max(norma_1, norma_2);

const APL_Float test = 1.0 + 2.0 * (zeta1 - zeta2) * (zeta1 + zeta2);
T sine;
T cosi;
   if (test >= 0.0 )
      {
        // root is close to zero, compute directly
        //
        const APL_Float b = (zeta1*zeta1 + zeta2*zeta2 + 1.0)*0.5;
        const APL_Float zeta2_2 = zeta2*zeta2;
        const APL_Float t = zeta2_2 / (b + sqrt(abs(b*b - zeta2_2)));
        sine =   (ALPHA / abs_estimate) / (1.0 - t);
        cosi = - (GAMMA / abs_estimate) / t;
        SEST = sqrt(t + 4.0*eps*eps*norma)*abs_estimate;
      }
   else
      {
        // root is closer to ONE, shift by that amount
        //
        const APL_Float b = (square(zeta2) + square(zeta1) - 1.0)*0.5;
        APL_Float t;
        if (b >= 0.0)   t = -zeta1 * zeta1 / (b + hypotenuse(b, zeta1));
        else            t = b - hypotenuse(b, zeta1);

        sine = - (ALPHA / abs_estimate) / t;
        cosi = - (GAMMA / abs_estimate) / (1.0 + t);
        SEST = sqrt(1.0 + t + 4.0*eps*eps*norma) * abs_estimate;
      }

   // normalize SIN and COS
   //
const APL_Float hypo = hypotenuse(sine, cosi);
   SIN = sine / hypo;
   COS = cosi / hypo;
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
T LA_pack::larfg(Ccol N, Vector<T> & X)
{
   assert(N > 0);

   if (N == 1)   return T(0.0);

T & ALPHA = X.at(-1);

APL_Float alpha_r = get_real(ALPHA);
APL_Float alpha_i = get_imag(ALPHA);

   if (X.norm_2() == 0.0 && alpha_i == 0.0)   return T(0.0);

APL_Float beta_abs = hypotenuse(ALPHA, X);
APL_Float beta = alpha_r < 0.0 ? beta_abs : -beta_abs;

   // scale small beta so it can be used safely
   //
int kcnt = 0;
   if (abs(beta) < safe_min)
       {
         while (abs(beta) < safe_min)
            {
              ++kcnt;
              X.scale(inv_safe_min);     // scale x
              beta *= inv_safe_min;      // scale beta
              alpha_r *= inv_safe_min;   // scale real(ALPHA)
              alpha_i *= inv_safe_min;   // scale imag(ALPHA)
            }

         set_real(ALPHA, alpha_r);   // update ALPHA
         set_imag(ALPHA, alpha_i);   // update ALPHA
         beta_abs = hypotenuse(ALPHA, X);
         beta = alpha_r < 0.0 ? beta : -beta;
       }

T tau;
   set_real(tau, (beta - alpha_r) / beta);
   set_imag(tau, -alpha_i         / beta);

const T factor = reciprocal(ALPHA - beta);
   X.scale(factor);

   loop(k, kcnt)   beta *= safe_min;   // unscale beta

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
Fcol LA_pack::ila_lc(Crow M, const Matrix<T> & C)
{
   /* return the (FORTRAN index of the) rightmost column of C which
      has a non-zero item in its first M rows.

       ├──────── N ────────┤
       ╔═════════════╤═════╗ ┬
       ║             │     ║ │
       ║             │  0  ║ M
       ║      C<T>   │     ║ │
       ║             └─────╢ ┴
       ║                   ║
       ╚═══════════════════╝
                    ↑
                    ila_lc(M, N, C)
    */

const Ccol N = C.get_column_count();

   rev_loop(col, N)
       {
         const Vector<T> column = C.get_column(col);
         if (!column.is_null(M))   return col + 1;

         /* the above is the same as:

         ALL_ROWS(M)
             {
               if (get_real(A[row + col*LDA]) != 0)   return col + 1;
               if (get_imag(A[row + col*LDA]) != 0)   return col + 1;
             }
          */
       }

   return 1;
}
//----------------------------------------------------------------------------
/** LApack function gemv. Compute one of:

    y := alpha × A      × x + beta × y   or
    y := alpha × A* × T × x + beta × y   or
    y := alpha × A* × H × x + beta × y

   constants like 1.0 were removed and only the case needed is implemented
 */
template<typename T>
inline void LA_pack::gemv(int M, int N, const Matrix<T> & A,
                          const Vector<T> & x, Vector<T> & y)
{
   ALL_COLS(N)
       {
         T tmp(0.0);
         ALL_ROWS(M)   tmp += conjugated(A.at(row, col)) * x.at(row);
         y.at(col) = tmp;
       }
}
//----------------------------------------------------------------------------
/// LApack function gerc: C := alpha * x * y* * H + C.
template<typename T>
void LA_pack::gerc(int M, int N, T ALPHA,
                  const Vector<T> &x, const Vector<T> &y, Matrix<T> & C)
{
   if (M && N && is_nonzero(ALPHA))
      {
        ALL_COLS(N)
           {
             const T & Y = y.at(col);
             if (is_nonzero(Y))
                {
                  const T YY = conjugated(Y) * ALPHA;
                  ALL_ROWS(M)   C.at(row, col) += YY * x.at(row);
                }
           }
      }
}
//----------------------------------------------------------------------------
/** LApack function larf: applies an elementary reflector to a general
    rectangular matrix C.

   LARF applies a elementary reflector H to an M-by-N matrix C,
   from either the left or the right. H is represented in the
   form

       H = I - tau * v * v**H

   where tau is a scalar and v is a vector.

   If tau = 0, then H is taken to be the unit matrix.

   To apply H**H, supply conjg(tau) instead.

   C is overwritten by the matrix H * C.
 */
template<typename T>
void LA_pack::larf(Vector<T> & v, T tau, Matrix<T> & C)
{
const Crow M = C.get_row_count();
const Ccol N = C.get_column_count();

   // NOTE only SIDE == "Left" is needed (and implemented here).
   //
Crow lastV = 0;   // index of the last non-zero item in v.
Ccol lastC = 0;   // rightmost column COL with a nonzero item in M↑COL

   if (is_nonzero(tau))   // if H is not the unit matrix
     {
       // Look for the last non-zero item (row) in vector V
       //
       lastV = M;
       while (lastV && is_zero(v.at(lastV - 1)))   --lastV;

       // Scan for the last non-zero column in C(1:lastv,:)
       //
       lastC = ila_lc<T>(lastV, C);
     }

   if (lastV)
      {
        APL_Float * gemv_data = new APL_Float[2*N];   // N double or N complex
        Vector<T> gemv_result(reinterpret_cast<T *>(gemv_data), N);

        gemv<T>(lastV, lastC, C,    v, gemv_result);
        gerc<T>(lastV, lastC, -tau, v, gemv_result, C);
        delete[] gemv_data;
      }
}
//----------------------------------------------------------------------------
/*  LApack function laqp2: computes a QR factorization with column pivoting
    of the matrix block

   work is the concatenation WORK, RWORK in FORTRAN
 */
template<typename T>
void LA_pack::laqp2(Matrix<T> & A, Ccol * pivot, T * tau, APL_Float * work)
{
const Crow M = A.get_row_count();
const Ccol N = A.get_column_count();

#define vn1 work   /* work = vn1, vn2 */
APL_Float * const vn2 = work + N;

   ALL_COLS(N)
      {
        const int col_A1 = col + 1;   // the column right of col
        T & tau_i = tau[col];

        // pvt_0 is the column right of col that has the largesr vn1
        //
        const int pvt_0 = col + max_pos(vn1 + col, N - col);

        if (pvt_0 != col)   // unless col already is the pivot column
           {
             A.exchange_columns(pvt_0, col);
             exchange(pivot[pvt_0], pivot[col]);
             vn1[pvt_0] = vn1[col];
             vn2[pvt_0] = vn2[col];
           }

        // Generate elementary reflector H(i).
        //
        tau_i = 0.0;      // assume col is last (and then col_A1 is invalid)
        if (col_A1 < M)   // no, col_A1 valid
           {
             const int len_X = M - col_A1;   // cols right of col
             Vector<T> X(&A.at(col_A1, col), len_X);
             tau_i = larfg<T>(M - col, X);
           }

        if (col_A1 < N)   // unless last column
           {
             // Apply H(i)**H to A(offset+i:m,i+1:n) from the left.
             //
             const T Aii = A.diag(col);   // save diag
             A.diag(col) = APL_Float(1.0);
                Vector<T> v(&A.diag(col), M - col);
                Matrix<T> C = A.sub_matrix(col, col_A1);
                larf<T>(v, conjugated(tau_i), C);
             A.diag(col) = Aii;           // restore diag
           }

        // Update partial column norms vn1 and vn2.
        //
        for (int c = col_A1; c < N; ++c)
            {
              if (vn1[c] == 0.0)   continue;

              const APL_Float abs_A = abs(A.at(col, c)) / vn1[c];
              const APL_Float temp = max(1.0 - square(abs_A), 0.0);
              const APL_Float temp2 = square(vn1[c] / vn2[c]);
              if (temp * temp2 <= tol3z)   // temp and/or temp2 too small
                 {
                   vn1[c] = 0.0;     // assume invalid col_A1
                   if (col_A1 < M)   // valid col_A1
                      {
                        Vector<T> x(&A.at(col_A1, c), M - col_A1);
                        vn1[c] = x.norm();
                      }
                   vn2[c] = vn1[c];
                 }
              else                                 // "normal" temp2
                 {
                   vn1[c] *= sqrt(temp);
                 }
            }
      }
#undef vn1
}
//============================================================================
