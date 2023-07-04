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
divide_matrix(Value & Z, bool need_complex,
              ShapeItem rows, ShapeItem cols_A, const Cell * cA,
              ShapeItem cols_B, const Cell * cB)
{
   // the following has been checked by the caller:
   //
   // rows_B >= cols_B and
   // rows_A == rows_B  (aka. rows below)
   //
const APL_Float rcond = Workspace::get_CT();
   loop(c, cols_A)
       {
         if (need_complex)
            {
              APL_Float * ad = reinterpret_cast<APL_Float *>(
                               malloc(rows * cols_A * 2*sizeof(APL_Float)));
              if (ad == 0)   WS_FULL
              LA_pack::ZZ * const a = reinterpret_cast<LA_pack::ZZ *>(ad);
              loop(r, rows)
                  {
                    new (a + r) LA_pack::ZZ(cA[r*cols_A + c].get_real_value(),
                                   cA[r*cols_A + c].get_imag_value());
                  }

              APL_Float * bd = reinterpret_cast<APL_Float *>(
                               malloc(rows * cols_B * 2*sizeof(APL_Float)));
              if (bd == 0)   { free(a);   WS_FULL }

              LA_pack::ZZ * const b = reinterpret_cast<LA_pack::ZZ *>(bd);
              LA_pack::ZZ * bb = b;
              loop(rr, cols_B)
              loop(cc, rows)
                 {
                   new (bb++) LA_pack::ZZ(cB[rr + cc*cols_B].get_real_value(),
                                 cB[rr + cc*cols_B].get_imag_value());
                 }

              {
                LA_pack::Matrix<LA_pack::ZZ> B(b, rows, cols_B, /* LDB */ rows);
                LA_pack::Matrix<LA_pack::ZZ> A(a, rows, 1,      /* LDA */ rows);
                const ShapeItem rk = LA_pack::Zgelsy(B, A, rcond);
                free(bd);
                if (rk != cols_B)
                   {
                     free(ad);
                     DOMAIN_ERROR;
                   }
              }

              // cols_A = rows_Z. We have computed the result for col c of A
              // which is row c of Z.
              //
              loop(r, cols_B)
                  Z.set_ravel_Complex(r*cols_A + c, a[r].real(), a[r].imag());
              free(ad);
            }
         else   // real
            {
              APL_Float * a = reinterpret_cast<APL_Float *>(
                              malloc(rows * cols_A * sizeof(APL_Float)));
              if (a == 0)   WS_FULL
              loop(r, rows)
                 {
                   a[r] = cA[r*cols_A + c].get_real_value();
                 }

              APL_Float * b = reinterpret_cast<APL_Float *>(
                              malloc(rows * cols_B * sizeof(APL_Float)));
              if (b == 0)   { free(a);   WS_FULL }

              APL_Float * bb = b;
              loop(rr, cols_B)
              loop(cc, rows)
                 {
                   *bb++ = cB[rr + cc*cols_B].get_real_value();
                 }

              {
                const APL_Float rcond = Workspace::get_CT();

                LA_pack::Matrix<LA_pack::DD> B(b, rows, cols_B, /* LDB */ rows);
                LA_pack::Matrix<LA_pack::DD> A(a, rows, 1,      /* LDA */ rows);
                const ShapeItem rk = LA_pack::Dgelsy(B, A, rcond);
                free(b);
                if (rk != cols_B)
                   {
                     free(a);
                     DOMAIN_ERROR;
                   }
              }

              // cols_A = rows_Z. We have computed the result for col c of A
              // which is row c of Z.
              //

              loop(r, cols_B)   Z.set_ravel_Float(r*cols_A + c, a[r]);
              free(a);
            }
       }
}

   // numerical limits
   //
   // DLAMCH determines double precision machine parameters.
   // We (only) #define those that we need.

#define dlamch_E 1.11022e-16
#define dlamch_S 2.22507e-308
#define dlamch_P 2.22045e-16
static const APL_Float small_number = dlamch_S / dlamch_P;
static const APL_Float big_number   = 1.0 / small_number;
static const APL_Float safe_min     =  dlamch_S / dlamch_E;
static const APL_Float inv_safe_min = 1.0 / safe_min;
static const APL_Float tol3z        = sqrt(dlamch_E);

//----------------------------------------------------------------------------
///  DGELSY solves overdetermined or underdetermined systems for GE matrices 
template<typename T>
int LA_pack::gelsy(Matrix<T> & A, Matrix<T> &B, APL_Float rcond)
{
const ShapeItem M    = A.get_row_count();
const ShapeItem N    = A.get_column_count();
const ShapeItem NRHS = B.get_column_count();

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
const ShapeItem N    = A.get_column_count();
const ShapeItem NRHS = B.get_column_count();

   // Compute QR factorization with column pivoting of A:
   // A * P = Q * R
   //
   // pivot_tau_tmp is a vector of ShapeItem pivot, complex tau, and
   // complex tmp in a single memory allcoation.
   //
char * pivot_tau_tmp = new char[N*(sizeof(ShapeItem) + 4*sizeof(APL_Float))];

  // N pivot items
  //
ShapeItem * pivot = reinterpret_cast<ShapeItem *>(pivot_tau_tmp);

   // N tau items (complex = double[2],
   //
T * tau = reinterpret_cast<T *>(pivot + N);       // N complex tau items

   // and N tmp items (complex = double[2]).
   //
T * tmp = tau + N;                                // N complex tmp1 items

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

   // B(1:RANK, 1:NRHS) := inv(T11) * B(1:RANK,1:NRHS)
   //
   {
     Matrix<T> B1 = B.sub_len(B.get_dx(), NRHS);
     trsm<T>(N, NRHS, A, B1);
   }

   // workspace: 2*MN+NRHS
   //
   // B(1:N,1:NRHS) := P * B(1:N,1:NRHS)
   //
   loop(j, NRHS)
      {
        loop(i, N)   tmp[pivot[i]] = B.at(i, j);
        loop(i, N)   B.at(i, j) = tmp[i];
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

const ShapeItem N = A.get_column_count();

APL_Float smax = ABS(A.diag(0));
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
         T s1(0.0);
         T s2(0.0);
         T c1(0.0);
         T c2(0.0);
         const T gamma(A.diag(RANK));

         T alpha_min = 0.0;
         T alpha_max = 0.0;
         for (int r = 0; r < RANK; ++r)
             {
               alpha_min = alpha_min + DZ::CONJ(work_min[r] * A.at(r, RANK));
               alpha_max = alpha_max + DZ::CONJ(work_max[r] * A.at(r, RANK));
             }

        laic1_min<T>(smin, alpha_min, gamma, s1, c1);
         laic1_max<T>(smax, alpha_max, gamma, s2, c2);

         if (smax*rcond > smin)   // done
            {
              delete[] work_1;
              return RANK;
            }

         loop(i, RANK)
              {
                work_min[i] = work_min[i] * s1;
                work_max[i] = work_max[i] * s2;
              }

         work_min[RANK] = c1;
         work_max[RANK] = c2;
       }

   delete[] work_1;
   return N;
}
//----------------------------------------------------------------------------
// LApack function geqp3
template<typename T>
void LA_pack::geqp3(Matrix<T> & A, ShapeItem * pivot, T * tau)
{
const ShapeItem M = A.get_row_count();
const ShapeItem N = A.get_column_count();

   // init column permutaion for pivoting
   // so we simply create the identical permutation
   //
   loop(j, N)   pivot[j] = j;

   // (no fixed columns)

   // Factorize free columns
   // ======================
   //
   {
     // Initialize partial column norms.
     // the first N elements of WORK store the exact column norms.
     //
     APL_Float * vn12 = new APL_Float[2*N];
     loop(j, N)
         {
           Vector<T> x(&A.at(0, j), M);
           vn12[N + j] = vn12[j] = x.norm();
         }

     // Use unblocked code to factor the last or only block
     //
     laqp2<T>(A, pivot, tau, vn12);
     delete[] vn12;
   }
}
//----------------------------------------------------------------------------
template<typename T>
void LA_pack::unm2r(ShapeItem K, Matrix<T> & A, const T * tau, Matrix<T> & c)
{
const ShapeItem M = c.get_row_count();

   // only SIDE == "Left" is implemented
   // only TRANS = 'T' or 'C' is implemented
   // thus NOTRANS is always false
   //
   loop(i, K)
       {
         // H(i) or H(i)**H is applied to C(i:m,1:n)
         //
         const int MM = M - i;

        //
         const T tau_i = DZ::CONJ(tau[i]);

         const T Aii = A.diag(i);   // remember A(i, i)
         {
           A.diag(i) = APL_Float(1.0);
           Vector<T> v(&A.diag(i), MM);
           Matrix<T> c1 = c.sub_yx(i, 0);
           larf<T>(v, tau_i, c1);
         }
         A.diag(i) = Aii;   // restore A(i, i);
       }
}
//----------------------------------------------------------------------------
/** LApack function laic1 (estimate largest singular value).
     apply one step of incremental condition estimation.

     SEST: largest estimated singular value of \b this matrix
 **/
template<typename T>
void LA_pack::laic1_max(APL_Float & SEST, T alpha, T GAMMA, T & SIN, T & COS)
{
const APL_Float eps = dlamch_E;
const APL_Float abs_alpha = ABS(alpha);
const APL_Float abs_gamma = ABS(GAMMA);
const APL_Float abs_estimate = ABS(SEST);

   //    Estimating largest singular value ...

   //    special cases
   //
   if (SEST == 0.0)
      {
        const APL_Float s1 = MAX(abs_gamma, abs_alpha);
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

             const APL_Float tmp = sqrt(ABS_2(SIN) + ABS_2(COS));
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
        APL_Float tmp = MAX(abs_estimate, abs_alpha);
        const APL_Float s1 = abs_estimate / tmp;
        const APL_Float s2 = abs_alpha / tmp;
        SEST = tmp*sqrt(s1*s1 + s2*s2);
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
             const APL_Float scl = sqrt(1.0 + tmp*tmp);
             SEST = s2*scl;
             SIN = (alpha / s2) / scl;
             COS = (GAMMA / s2) / scl;
           }
        else
           {
             const APL_Float tmp = s2 / s1;
             const APL_Float scl = sqrt(1.0 + tmp*tmp);
             SEST = s1*scl;
             SIN = (alpha / s1) / scl;
             COS = (GAMMA / s1) / scl;
           }
        return;
      }

   // normal case
   //
const APL_Float zeta1 = abs_alpha / abs_estimate;
const APL_Float zeta2 = abs_gamma / abs_estimate;
const APL_Float b = (1.0 - zeta1*zeta1 - zeta2*zeta2)*0.5;
const APL_Float zeta1_2 = zeta1*zeta1;
APL_Float t;

   if (b > 0.0)  t = zeta1_2 / (b + sqrt(b*b + zeta1_2));
   else          t = sqrt(b*b + zeta1_2) - b;

const T sine = -( alpha / abs_estimate ) / t;
const T cosine = -( GAMMA / abs_estimate ) / ( 1.0 + t);
const APL_Float tmp = sqrt(ABS_2(sine) + ABS_2(cosine));
   SIN = sine / tmp;
   COS = cosine / tmp;
   SEST = sqrt(t + 1.0) * abs_estimate;
}
//----------------------------------------------------------------------------
/** LApack function laic1 (estimate smallest singular value).
      Results are: SEST, SIN, and COS
     apply one step of incremental condition estimation.

     SEST: smallest estimated singularvalue of \b this matrix
 **/
template<typename T>
void LA_pack::laic1_min(APL_Float & SEST, T alpha, T GAMMA, T & SIN, T & COS)
{
const APL_Float eps = dlamch_E;
const APL_Float abs_alpha = ABS(alpha);
const APL_Float abs_gamma = ABS(GAMMA);
const APL_Float abs_estimate = ABS(SEST);

   //    Estimating smallest singular value ...
   //

   //    special cases
   //
   if (SEST == 0.0)
      {
        SEST = 0.0;
        T sine(1.0);
        T cosine(0.0);
        if (abs_gamma > 0.0 || abs_alpha > 0.0)
           {
             sine   = -DZ::CONJ(GAMMA);
             cosine =  DZ::CONJ(alpha);
           }

        const APL_Float abs_sine = ABS(sine);
        const APL_Float abs_cosine = ABS(cosine);
        const APL_Float s1 = MAX(abs_sine, abs_cosine);
        const T sine_s1 = sine / s1;
        const T cosine_s1 = cosine / s1;
        SIN = sine_s1;
        COS = cosine_s1;
        const APL_Float tmp = sqrt(ABS_2(sine_s1) + ABS_2(cosine_s1));
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
        const T conj_gamma = DZ::CONJ(GAMMA);
        const T conj_alpha = DZ::CONJ(alpha);

        if (s1 <= s2)
           {
             const APL_Float tmp = s1 / s2;
             const APL_Float scl = sqrt(1.0 + tmp*tmp);
             const APL_Float tmp_scl = tmp / scl;

             SEST = abs_estimate * tmp_scl;
             SIN = -(conj_gamma / s2 ) / scl;
             COS =  (conj_alpha / s2 ) / scl;
           }
        else
           {
             const APL_Float tmp = s2 / s1;
             const APL_Float scl = sqrt(1.0 + tmp*tmp);

             SEST = abs_estimate / scl;
             SIN = -( conj_gamma / s1 ) / scl;
             COS = ( conj_alpha  / s1 ) / scl;
           }
        return;
      }

   // normal case
   //
const APL_Float zeta1 = abs_alpha / abs_estimate;
const APL_Float zeta2 = abs_gamma / abs_estimate;

const APL_Float norma_1 = 1.0 + zeta1*zeta1 + zeta1*zeta2;
const APL_Float norma_2 = zeta1*zeta2 + zeta2*zeta2;
const APL_Float norma = MAX(norma_1, norma_2);

const APL_Float test = 1.0 + 2.0*(zeta1-zeta2)*(zeta1+zeta2);
T sine;
T cosine;
   if (test >= 0.0 )
      {
        // root is close to zero, compute directly
        //
        const APL_Float b = (zeta1*zeta1 + zeta2*zeta2 + 1.0)*0.5;
        const APL_Float zeta2_2 = zeta2*zeta2;
        const APL_Float t = zeta2_2 / (b + sqrt(ABS(b*b - zeta2_2)));
        sine = (alpha / abs_estimate) / (1.0 - t);
        cosine = -( GAMMA / abs_estimate ) / t;
        SEST = sqrt(t + 4.0*eps*eps*norma)*abs_estimate;
      }
   else
      {
        // root is closer to ONE, shift by that amount
        //
        const APL_Float b = (zeta2*zeta2 + zeta1*zeta1 - 1.0)*0.5;
        const APL_Float zeta1_2 = zeta1*zeta1;
        APL_Float t;
        if (b >= 0.0)   t = -zeta1_2 / (b+sqrt(b*b + zeta1_2));
        else            t = b - sqrt(b*b + zeta1_2);

        sine = -( alpha / abs_estimate ) / t;
        cosine = -( GAMMA / abs_estimate ) / (1.0 + t);
        SEST = sqrt(1.0 + t + 4.0*eps*eps*norma)*abs_estimate;
      }

const APL_Float tmp = sqrt(ABS_2(sine) + ABS_2(cosine));
   SIN = sine / tmp;
   COS = cosine / tmp;
}
//----------------------------------------------------------------------------
/** LApack function larfg. It generates an elementary reflector
    (aka. Householder matrix)

   Consider the linear transformation of a point x:

   x → x - 2(x,v)v = x - 2v(v* x)  (v* ←→ complex conjugate of v = -⍉v)

   The Householder matrix P is then

   P = I - 2 v v*   (where I is the identity matrix.
 */
template<typename T>
T LA_pack::larfg(ShapeItem N, T & ALPHA, Vector<T> &x)
{
   assert(N > 0);

   if (N == 1)   return T(0.0);

APL_Float xnorm = x.norm();
APL_Float alpha_r = REAL(ALPHA);
APL_Float alpha_i = IMAG(ALPHA);

   if (xnorm == 0.0 && alpha_i == 0.0)   return T(0.0);

APL_Float beta = -SIGN(sqrt(alpha_r*alpha_r + alpha_i*alpha_i + xnorm*xnorm),
                    alpha_r);

   // scale small beta so it can be used safely
   //
int kcnt = 0;
   if (ABS(beta) < safe_min)
       {
         while (ABS(beta) < safe_min)
            {
              ++kcnt;
              x.scale(inv_safe_min);     // scale x
              beta *= inv_safe_min;      // scale beta
              alpha_r *= inv_safe_min;   // scale real(ALPHA)
              alpha_i *= inv_safe_min;   // scale imag(ALPHA)
            }

         xnorm = x.norm();                  // update xnorm
         Sri<T>(ALPHA, alpha_r, alpha_i);   // update ALPHA
         beta = -SIGN(sqrt(alpha_r*alpha_r + alpha_i*alpha_i + xnorm*xnorm),
                      alpha_r);
       }

T tau;
   Sri<T>(tau, (beta - alpha_r)/beta, -alpha_i/beta);

const T factor = DZ::inv(ALPHA - beta);
   x.scale(factor);

   loop(k, kcnt)   beta = beta * safe_min;   // unscale beta

   Sri<T>(ALPHA, beta);

   return tau;
}
//----------------------------------------------------------------------------
/// LApack function trsm. Solves op(A) * X = alpha * B

template<typename T>
void LA_pack::trsm(int M, int N, const Matrix<T> & A, Matrix<T> & B)
{
   // only: SIDE   = 'Left'              - lside == true
   //       UPLO   = 'Upper'             - upper = true
   //       TRANSA = 'No transpose'      - 
   //       DIAG   = 'Non-unit' and      - nounit = true
   //       ALPHA  = 1.0 is implemented!
   //
   loop(j, N)
       {
         rev_loop(k, M)
             {
               T & B_kj = B.at(k, j);
               if (B_kj != 0.0)
                  {
                    B_kj = B_kj / A.diag(k);
                    loop(i, k)   B.at(i, j) = B.at(i, j) - B_kj * A.at(i, k);
                  }
             }
       }
}
//----------------------------------------------------------------------------
/// LApack function ila_lc
template<typename T>
int LA_pack::ila_lc(ShapeItem M, ShapeItem N, const Matrix<T> & A)
{
   for (ShapeItem col = N - 1; col > 0; --col)
       {
         const Vector<T> column = A.get_column(col);
         if (!column.is_zero(M))   return col + 1;

         /* the above is the same as:

         for (ShapeItem row = 0; row < M; ++row)
             {
               if (REAL(A[row + col*LDA]) != 0)   return col + 1;
               if (IMAG(A[row + col*LDA]) != 0)   return col + 1;
             }

          */
       }

   return 1;
}
//----------------------------------------------------------------------------
/// LApack function gemv. y := alpha*A*x + beta*y or y := alpha*A**T*x + beta*y
template<typename T>
inline void LA_pack::gemv(int M, int N, const Matrix<T> & A, const Vector<T> &x,
                 Vector<T> &y)
{
   loop(j, N)
       {
         T temp(0.0);
         loop(i, M)   temp = temp + DZ::CONJ(A.at(i, j)) * x.at(i);
         y.at(j) = temp;
       }
}
//----------------------------------------------------------------------------
/// LApack function gerc: A := alpha * x * y* * H + A,
template<typename T>
void LA_pack::gerc(int M, int N, T ALPHA, const Vector<T> &x, const Vector<T> &y,
          Matrix<T> & A)
{
   if (M == 0 || N == 0 || ALPHA == 0.0)   return;

   loop(j, N)
      {
        T Y_j = y.at(j);
        if (Y_j != 0.0)
           {
             DZ::conjugate(Y_j);
             Y_j = Y_j * ALPHA;
             loop(i, M)   A.at(i, j) = A.at(i, j) + Y_j * x.at(i);
           }
      }
}
//----------------------------------------------------------------------------
/// LApack function larf: applies an elementary reflector to a general
/// rectangular matrix
template<typename T>
void LA_pack::larf(Vector<T> & v, T tau, Matrix<T> & c)
{
const ShapeItem M = c.get_row_count();
const ShapeItem N = c.get_column_count();

   // only SIDE == "Left" is implemented
   //
ShapeItem  lastv = 0;
ShapeItem  lastc = 0;

   if (tau != 0.0)
     {
       // Look for the last non-zero row in V
       //
       lastv = M;
       while (lastv > 0 && v.at(lastv - 1) == 0.0)   --lastv;

       // Scan for the last non-zero column in C(1:lastv,:)
       //
       lastc = ila_lc<T>(lastv, N, c);
     }

   if (lastv)
      {
        APL_Float * gemv_data = new APL_Float[2*N];   // N double or N complex
        Vector<T> gemv_result(reinterpret_cast<T *>(gemv_data), N);

        gemv<T>(lastv, lastc, c, v, gemv_result);
        gerc<T>(lastv, lastc, -tau, v, gemv_result, c);
        delete[] gemv_data;
      }
}
//----------------------------------------------------------------------------
/// LApack function laqp2: computes a QR factorization with column pivoting
/// of the matrix block
template<typename T>
void LA_pack::laqp2(Matrix<T> & A, ShapeItem * pivot, T * tau, APL_Float * vn1)
{
const ShapeItem M = A.get_row_count();
const ShapeItem N = A.get_column_count();

APL_Float * vn2 = vn1 + N;

   loop(i, N)
      {
        T & tau_i = tau[i];

        // Determine the i'th pivot column and swap if necessary.
        //
        const int pvt_0 = i + max_pos(vn1 + i, N - i);

        if (pvt_0 != i)
           {
             A.exchange_columns(pvt_0, i);
             exchange(pivot[pvt_0], pivot[i]);
             vn1[pvt_0] = vn1[i];
             vn2[pvt_0] = vn2[i];
           }

        // Generate elementary reflector H(i).
        //
        {
          if (i < (M - 1))
             {
               Vector<T> x(&A.at(i + 1, i), M - i - 1);
               T & alpha = *(&x.at(0) - 1);   // one before x
               tau_i = larfg<T>(M - i, alpha, x);
             }
          else
             {
               Vector<T> x(&A.at(M - 1, i), 1);
               T & alpha = x.at(0);           // at x
               tau_i = larfg<T>(1, alpha, x);
             }
        }

        if (i < (N - 1))
           {
             // Apply H(i)**H to A(offset+i:m,i+1:n) from the left.
             //
             const T Aii = A.diag(i);   // save diag
             A.diag(i) = APL_Float(1.0);
                Vector<T> v(&A.diag(i), M - i);
                Matrix<T> c = A.sub_yx(i, i + 1);
                larf<T>(v, DZ::CONJ(tau_i), c);
             A.diag(i) = Aii;           // restore diag
           }

        // Update partial column norms.
        //
        for (ShapeItem j = i + 1; j < N; ++j)
            {
              if (vn1[j] != 0.0)
                 {
                   APL_Float temp = ABS(A.at(i, j)) / vn1[j];
                   temp = 1.0 - temp*temp;
                   temp = MAX(temp, APL_Float(0.0));

                   APL_Float temp2 = vn1[j] / vn2[j];
                   temp2 = temp * temp2 * temp2;
                   if (temp2 <= tol3z)
                      {
                        if (i < (M - 1))
                           {
                             Vector<T> x(&A.at(i + 1, j), M - i - 1);
                             vn1[j] = x.norm();
                             vn2[j] = vn1[j];
                           }
                        else
                           {
                             vn1[j] = 0.0;
                             vn2[j] = 0.0;
                           }
                      }
                   else
                      {
                        vn1[j] = vn1[j] * sqrt(temp);
                      }
                 }
            }
      }
}
//============================================================================
