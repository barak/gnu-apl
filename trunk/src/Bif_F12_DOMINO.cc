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

#include "Bif_F12_DOMINO.hh"
#include "Bif_F12_FORMAT.hh"
#include "ComplexCell.hh"
#include "Value.hh"
#include "Workspace.hh"

#include "LApack.hh"

Bif_F12_DOMINO Bif_F12_DOMINO   ::fun;    // ⌹

// #define DOMINO_DEBUG

//----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_B(Value_P B) const
{
   if (B->is_scalar())
      {
        Value_P Z(LOC);

        B->get_cscalar().bif_reciprocal(&Z->get_wscalar());
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (B->get_rank() == 1)   // inversion at the unit sphere
      {
        const double qct = Workspace::get_CT();
        const ShapeItem len = B->get_shape_item(0);
        APL_Complex r2(0.0);
        loop(l, len)
            {
              const APL_Complex b = B->get_cravel(l).get_complex_value();
              r2 += b*b;
            }

        if (r2.real() < qct && r2.real() > -qct &&
            r2.imag() < qct && r2.imag() > -qct)
            DOMAIN_ERROR;

        Value_P Z(len, LOC);

        if (r2.imag() < qct && r2.imag() > -qct)   // real result
           {
             loop(l, len)
                 {
                   const APL_Float b = B->get_cravel(l).get_real_value();
                   Z->next_ravel_Float(b / r2.real());
                 }
           }
        else                                       // complex result
           {
             loop(l, len)
                 {
                   const APL_Complex b = B->get_cravel(l).get_complex_value();
                   Z->next_ravel_Complex(b / r2);
                 }
           }

        Z->set_default(*B.get(), LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (B->get_rank() > 2)   RANK_ERROR;

const ShapeItem rows = B->get_shape_item(0);
const ShapeItem cols = B->get_shape_item(1);
   if (cols > rows)
      {
        MORE_ERROR() << "÷B : B is under-specified (has more cols than rows)";
        LENGTH_ERROR;
      }

   // create an identity matrix I and call eval_AB(I, B).
   //
const Shape shape_I(rows, rows);
Value_P I(shape_I, LOC);

   loop(y, rows)
   loop(x, rows)   I->next_ravel_Float(y == x ? 1.0 : 0.0);

Token result = eval_AB(I, B);
   return result;
}
//----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_XB(Value_P X, Value_P B) const
{
   // X: the scalar rcond for B = P∘Q
   //
   if (!X->is_scalar())   RANK_ERROR;

const double EPS = X->get_cfirst().get_real_value();

   if (B->get_rank() != 2)   RANK_ERROR;

   // if rank of A or B is < 2 then treat it as a
   // 1 by n (or 1 by 1) matrix..
   //
const ShapeItem rows_B = B->get_rows();
const ShapeItem cols_B = B->get_cols();
   if (rows_B < cols_B)
      {
        MORE_ERROR() << "A÷B : B is under-specified (has more cols than rows)";
        LENGTH_ERROR;
      }

   if (rows_B*cols_B == 0)   LENGTH_ERROR;   // empty B

const bool need_complex = B->is_complex(true);
Value_P Z(3, LOC);
   QR_factorization(Z, need_complex, rows_B, cols_B, &B->get_cfirst(), EPS);
// LA_pack::factorize_matrix(*Z, need_complex, rows_B, cols_B, &B->get_cfirst(), EPS);

   Z->set_default(*B.get(), LOC);   // not needed
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_AB(Value_P A, Value_P B) const
{
ShapeItem rows_A = 1;
ShapeItem cols_A = 1;

Shape shape_Z;

   // if rank of A or B is < 2 then treat it as a
   // 1 by n (or 1 by 1) matrix..
   //
ShapeItem rows_B = 1;
ShapeItem cols_B = 1;
   switch(B->get_rank())
      {
         case 0:  break;

         case 1:  rows_B = B->get_shape_item(0);
                  break;

         case 2:  cols_B = B->get_shape_item(1);
                  rows_B = B->get_shape_item(0);
                  shape_Z.add_shape_item(cols_B);
                  break;

         default: RANK_ERROR;
      }

   switch(A->get_rank())
      {
         case 0:  break;

         case 1:  rows_A = A->get_shape_item(0);
                  break;

         case 2:  cols_A = A->get_shape_item(1);
                  rows_A = A->get_shape_item(0);
                  shape_Z.add_shape_item(cols_A);
                  break;

         default: RANK_ERROR;
      }

   if (rows_B <  cols_B)
      {
        MORE_ERROR() << "A÷B : B is under-specified (has more cols than rows)";
        LENGTH_ERROR;
       }

   if (rows_A != rows_B)
      {
        MORE_ERROR() << "A÷B : number of rows in A ≠ number of rows in B";
        LENGTH_ERROR;
      }

const bool need_complex = A->is_complex(true) || B->is_complex(true);
Value_P Z(shape_Z, LOC);
   LA_pack::divide_matrix(*Z, need_complex, rows_A, cols_A, &A->get_cfirst(),
                                            cols_B, &B->get_cfirst());

   Z->set_default(*B.get(), LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_fill_B(Value_P B) const
{
   return Bif_F12_TRANSPOSE::do_eval_B(B.get());
}
//----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_fill_AB(Value_P A, Value_P B) const
{
Shape shape_Z;
   loop(r, A->get_rank() - 1)  shape_Z.add_shape_item(A->get_shape_item(r + 1));
   loop(r, B->get_rank() - 1)  shape_Z.add_shape_item(B->get_shape_item(r + 1));

Value_P Z(shape_Z, LOC);
   while (Z->more())   Z->next_ravel_0();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
   TODO;
}
//----------------------------------------------------------------------------
/// print debug infos for \b this real matrix
template<>
void Bif_F12_DOMINO::Matrix<false>::debug(const char * name) const
{
#ifdef DOMINO_DEBUG
const Shape shape_B(M, N);
Value_P B(shape_B, LOC);

   loop(y, M)
   loop(x, N)   B->next_ravel_Float(real(y, x));
   B->check_value(LOC);

Value_P A(2, LOC);   // A←0 4
   A->next_ravel_0();
   A->next_ravel_Int(4);   // number of fractional digits
   A->check_value(LOC);

Value_P Z = Bif_F12_FORMAT::format_by_specification(A, B);
   CERR << name;
   Z->print_boxed(CERR, 0);
#endif // DOMINO_DEBUG
}
//----------------------------------------------------------------------------
/// print debug infos for \b this complex matrix
template<>
void Bif_F12_DOMINO::Matrix<true>::debug(const char * name) const
{
#ifdef DOMINO_DEBUG
const Shape shape_B(M, N);
Value_P B(shape_B, LOC);

   loop(y, M)
   loop(x, N)   B->next_ravel_Complex(real(y, x), imag(y, x));
   B->check_value(LOC);

Value_P A(2, LOC);
   A->next_ravel_0();
   A->next_ravel_Int(4);   // number of fractional digits
   A->check_value(LOC);

Value_P Z = Bif_F12_FORMAT::format_by_specification(A, B);
   CERR << name;
   Z->print_boxed(CERR, 0);
#endif // DOMINO_DEBUG
}
//----------------------------------------------------------------------------
/// invert an upper-triangle matrix. Input is an upper triangle matrix
//  utm (whose items below the diagonal are ignored but pretended to be 0).
//  Result is the inverse M×N matrix aug.
//
template<typename T, bool cplx>
Value_P Bif_F12_DOMINO::invert_UTM(ShapeItem M, ShapeItem N, T * utm, T * aug)
{
matrix_assert(M >= N);

   // ignore cols ≥ N. In a UTM they are 0 and thus rows ≥ N of the result
   // are 0 as well. as well. The 0s of the result will be initialized at
   // the end. Until then we treat UTM as a quadratic N×N matrix.
   //
   invert_QUTM<T>(N, utm, aug);

Matrix<cplx> AUG(reinterpret_cast<double *>(aug), N, N);   // augmented result

   // create the result value
   //
const Shape shape_INV(N, M);   // INV has the transposed shape!
Value_P INV(shape_INV, LOC);
   ALL_ROWS(N)   // for every row
   ALL_COLS(M)   // for every column x
       {
         if (cplx)
            {
              double re = 0.0;
              double im = 0.0;
              if (col < N && col >= row)   // diagonal or above
                 {
                   re = AUG.real(row, col);
                   im = AUG.imag(row, col);
                   if (!(isfinite(re) && isfinite(im)))   DOMAIN_ERROR;
                 }
              INV->next_ravel_Complex(re, im);
            }
         else
            {
               double flt = 0.0;
               if (col < N && col >= row)   // diagonal or above
                  {
                    flt = AUG.real(row, col);
                    if (!isfinite(flt))   DOMAIN_ERROR;
                  }
               INV->next_ravel_Float(flt);
             }
       }

   INV->check_value(LOC);
   return INV;
}
//----------------------------------------------------------------------------
template<typename T>
void Bif_F12_DOMINO::invert_QUTM(ShapeItem N, T * utm, T * aug)
{
#define AT(x, y) at(y,x)   // transposed FORTRAN matrix

LA_pack::Matrix<T>UTM(utm, N, N, N);   // the matrix to be inverted
LA_pack::Matrix<T>AUG(aug, N, N, N);   // the augmented result matrix

   // start with empty aug. 
   //
   ALL_ROWS(N)
   ALL_COLS(N)   AUG.AT(row, col) = T(0.0);

   // divide every row of UTM by its diagonal element, so that the diagonal
   // elements of UTM become 1.0. Also initialize the diagonal of AUG.
   //
   ALL_ROWS(N)
       {
         const T diag = UTM.diag(row);
         if (diag == 0.0)
            {
              MORE_ERROR() << "⌹[X]B: 0 on the main diagonal of R";
              DOMAIN_ERROR;
            }

         // divide all items right of UTM[row;row] as well as the entire
         // AUG[row;] by diag. Conceptually the entire diagonal of UTM is
         // set to 1.0. However, we do not use the diagonal of UTM after
         // this loop and therefore don't bother to set it to 1.0.
         //
         for (Ccol col = row + 1; col < N; ++col)   UTM.AT(row, col) /= diag;

         // divide diagonal items of UTM | AUG by diag_y
         //
         AUG.diag(row) = 1.0 / diag;
       }

   /* at this point the diagonal of UTM is normalized to 1.0 and AUG is
      zero except on its diagonal:

       ┌────UTM────┐         ┌────AUG────┐
       │ 1 u u u u │         │ a         │
       │   1 u u u │         │   a    0  │ aⱼⱼ = 1.0 ÷ uⱼⱼ
       │     1 u u │         │     a     │ uⱼⱼ = 1.0
       │  0    1 u │         │  0    a   │
       │         1 │         │         a │
       └───────────┘         └───────────┘
    */

   // subtract a multiple of the diagonal element from the items above it
   // so that the item becomes 0.0
   //
   REV_COLS(N)      // for every row k
   loop (row1, k)   // for every row1 above row k
       {
         /* subtract (factor1×k) from row1. factor1 is the element of
            row1 that is above the diagonal item of k. This makes
            AT(row1, k) == 0.0 and updates the columns right of it
            in both UTM and AUG:

                   0 ←   ← k ← N-1 ──────────── outer loop (row k=N-1:0)
                           ↓
                   ┌────UTM────┐   0     ┌────UTM────┐
                   │ 1 u u u 0 │   ↓     │ 1 u u u 0 │
                   │   1 u ⍺ 0 │← row1 → │   1 u 0 0 │  ⍺ ← 0
                   │     1 0 0 │   ↓     │     1 0 0 │
                   │  0    1 0 │   k     │  0    1 0 │
                   │         1 │   │     │         1 │
                   └───────────┘   │     └───────────┘
                           ↑       │             ↑
                          col→     │            col→
                                   │
                                   └─────────── inner loop (row1=0:k)

            In this double loop, every item ⍺ above the diagonal is
            visited and set to 0, updating UTM and AUG simultaneously.
          */

         const T alpha = UTM.AT(row1, k);
         if (LA_pack::is_zero(alpha))   continue;   // already 0.0

         for (Ccol col = row1; col < N; ++col)
             {
               // make UTM[k;col] zero by subtracting UTM[y;x] × alpha
               //
               UTM.AT(row1, col) -= UTM.AT(k, col) * alpha;
               AUG.AT(row1, col) -= AUG.AT(k, col) * alpha;
             }
       }
#undef AT
}
//----------------------------------------------------------------------------
void
Bif_F12_DOMINO::QR_factorization(Value_P Z, bool need_complex, ShapeItem rows,
                                 ShapeItem cols, const Cell * cB, double EPS)
{
   /* We want to store all floating point variables (including complex ones)
      in a single double[]. Before and after each variable we leave one double
      for storing numbers 42.0, 43.0, ...51.0. These numbers are used to check
      for overrides of the allocated space.

      Complex numbers are stored as their real part followed by imag part.

      The variables are B, Q, and R with B = Q +.× R, with Q real orthogonal
      and R real or complex upper triangular. Since being R is computed after
      Q, it can be used as a temporary variable in the computation of Q (i,e,
      in function householder()).
   */

   // start with the base addresses of the variables. All variables are
   // allocated as rows * rows so that we can freely rorate them...
   //
const int CPLX = need_complex ? 2 : 1;   // number of doubles per variable item
const ShapeItem len_B   = rows * cols;
const ShapeItem len     = rows * rows;
const ShapeItem base_B  = 1;
const ShapeItem base_Q  = base_B  + CPLX*len + 2;
const ShapeItem base_Qi = base_Q  + CPLX*len + 2;
const ShapeItem base_R  = base_Qi + CPLX*len + 2;
const ShapeItem base_S  = base_R  + CPLX*len + 2;
const ShapeItem end     = base_S  + CPLX*len + 2;

double * data = new double[end*CPLX];   if (data == 0)   WS_FULL;
   memset(data, 0, end*sizeof(double));
   data[base_B - 1]  = 42.0;   data[base_B  + CPLX*len_B] = 43.0;
   data[base_Q - 1]  = 44.0;   data[base_Q  + CPLX*len]   = 45.0;
   data[base_Qi - 1] = 46.0;   data[base_Qi + CPLX*len]   = 47.0;
   data[base_R - 1]  = 48.0;   data[base_R  + CPLX*len]   = 49.0;
   data[base_S - 1]  = 50.0;   data[base_S  + CPLX*len]   = 51.0;

   // compute the QR factorization of B. That is:
   //
   // B = Q∘R where:
   //
   // ⍴B ←→ M N,
   // ⍴Q ←→ N N  (since M≥N) and orthogonal,
   // ⍴R is M N  (since ⍴B ←→ ⍴Q+.×R) and upper triangular
   if (need_complex)   // complex B
      {
        setup_complex_B(cB, data + base_B, len_B);
        double * Q = householder<true>(data + base_B, rows, cols, data + base_Q,
                          data + base_Qi, data + base_R, data + base_S, EPS);

        setup_complex_B(cB, data + base_B, len_B);   // restore B
        const Matrix<true> Bm(data + base_B, rows, cols);
        Matrix<true> Qm(Q, rows, rows);
        Qm.transpose(rows);
        Matrix<true> Rm(data + base_R, rows, cols);
        Rm.init_inner_product(Qm, Bm);
        Qm.debug("final Q");
        Rm.debug("final R");
      }
   else                // real B
      {
   Assert(data[base_B + CPLX*len_B]  == 43.0);
        setup_real_B(cB, data + base_B, len_B);
   Assert(data[base_B + CPLX*len_B]  == 43.0);
        double * Q = householder<false>(data + base_B, rows,cols, data + base_Q,
                           data + base_Qi, data + base_R, data + base_S, EPS);

        setup_real_B(cB, data + base_B, len_B);   // restore B
        const Matrix<false> Bm(data + base_B, rows, cols);
        Matrix<false> Qm(Q, rows, rows);
        Qm.transpose(rows);
        Matrix<false> Rm(data + base_R, rows, cols);
        Rm.init_inner_product(Qm, Bm);
   Assert(data[base_B + CPLX*len_B]  == 43.0);
      }

   // check that the memory areas were not overridden
   Assert(data[base_B - 1]          == 42.0);
   Assert(data[base_B + CPLX*len_B] == 43.0);
   Assert(data[base_Q - 1]          == 44.0);
   Assert(data[base_Q + CPLX*len]   == 45.0);
   Assert(data[base_Qi - 1]         == 46.0);
   Assert(data[base_Qi + CPLX*len]  == 47.0);
   Assert(data[base_R - 1]          == 48.0);
   Assert(data[base_R + CPLX*len]   == 49.0);
   Assert(data[base_S - 1]          == 50.0);
   Assert(data[base_S + CPLX*len]   == 51.0);

   // Z[1] aka. Q
   {
     const Shape Q_shape(rows, rows);
     Value_P Qv(Q_shape, LOC);
     if (need_complex)
        {
          ALL_COLS(rows)   // FORTRAN order
          ALL_ROWS(rows)
             {
               const ShapeItem offset = 2*(col + row*rows);
               const double real = data[base_Q + offset];
               const double imag = data[base_Q + offset + 1];
               if (!(isfinite(real) && isfinite(imag)))   DOMAIN_ERROR;
               Qv->next_ravel_Complex(real, imag);
             }
        }
     else
        {
          ALL_COLS(rows)   // FORTRAN order
          ALL_ROWS(rows)
             {
               const ShapeItem offset = col + row*rows;
               const double real = data[base_Q + offset];
               if (!isfinite(real))   DOMAIN_ERROR;
               Qv->next_ravel_Float(real);
             }
        }
     Qv->check_value(LOC);
     Z->next_ravel_Pointer(Qv.get());
   }

   // Z[2] aka. R
   {
     const Shape shape_R(rows, cols);
     Value_P vR(shape_R, LOC);
     if (need_complex)
        {
          loop(offset, rows*cols)
              {
                const double real = data[base_R + 2*offset];
                const double imag = data[base_R + 2*offset + 1];
                if (!(isfinite(real) && isfinite(imag)))   DOMAIN_ERROR;
                vR->next_ravel_Complex(real, imag);
              }
        }
     else
        {
          loop(offset, rows*cols)
              {
                const double real = data[base_R + offset];
                if (!isfinite(real))   DOMAIN_ERROR;
                vR->next_ravel_Float(real);
              }
        }
     vR->check_value(LOC);
     Z->next_ravel_Pointer(vR.get());
   }

   // Z[3] aka. Rinv
   {

      if (need_complex)
         {
           LA_pack::ZZ * pR = reinterpret_cast<LA_pack::ZZ *>(data + base_R);
           LA_pack::ZZ * pQ = reinterpret_cast<LA_pack::ZZ *>(data + base_Q);
           Value_P INV = invert_UTM<LA_pack::ZZ, true>(rows, cols, pR, pQ);
           Z->next_ravel_Pointer(INV.get());
         }
      else
         {
           LA_pack::DD * pR = reinterpret_cast<LA_pack::DD *>(data + base_R);
           LA_pack::DD * pQ = reinterpret_cast<LA_pack::DD *>(data + base_Q);
           Value_P INV = invert_UTM<LA_pack::DD, false>(rows, cols, pR, pQ);
           Z->next_ravel_Pointer(INV.get());
         }
   }

   delete[] data;
}
//----------------------------------------------------------------------------
void
Bif_F12_DOMINO::setup_complex_B(const Cell * cB, double * D, ShapeItem count)
{
   // initialize the homogeneous complex vector D from the mixed APL ravel cB
   //
   loop(b, count)
      {
        const Cell & cell = *cB++;
        if (cell.is_float_cell())
           { *D++ = cell.get_real_value();   *D++ = 0.0; }
        else if (cell.is_integer_cell())
           { *D++ = cell.get_real_value();   *D++ = 0.0; }
        else if (cell.is_complex_cell())
           { *D++ = cell.get_real_value(); *D++ = cell.get_imag_value(); }
        else   DOMAIN_ERROR;
      }
}
//----------------------------------------------------------------------------
void
Bif_F12_DOMINO::setup_real_B(const Cell * cB, double * D, ShapeItem count)
{
   // initialize the homogeneous real vector D from the mixed APL ravel cB
   //
   loop(b, count)
      {
        const Cell & cell = *cB++;
        if (cell.is_float_cell())          *D++ = cell.get_real_value();
        else if (cell.is_integer_cell())   *D++ = cell.get_real_value();
        else                               DOMAIN_ERROR;
      }
}
//----------------------------------------------------------------------------
template<bool cplx>
double *
Bif_F12_DOMINO::householder(double * pB, ShapeItem rows, ShapeItem cols,
                            double * pQ, double * pQi, double * pT, double * pS,
                            double EPS)
{
   // pB is the matrix to be factorized, pQ, pQi, and pT were initialized to 0
   //
   // the algorithm is essentially the one described in Garry Helzer's paper
   // "THE HOUSEHOLDER ALGORITHM AND APPLICATIONS" but using complex numbers
   // when needed.

const double qct = Workspace::get_CT();
const double qct2 = qct*qct;
double BMAX = 0.0;

Matrix<cplx> mT (pT,  rows, rows);   // temporary storage
Matrix<cplx> mQ (pQ,  rows, rows);   // keeps size
Matrix<cplx> mB (pB,  rows, cols);   // shrinks
Matrix<cplx> mQi(pQi, rows, rows);   // keeps size

   // [0]  Q←HSHLDR2 B;N;BMAX;S;L2;QI;COL1
   // [1]  Q←ID N←↑⍴B

   // mQ was cleared, so setting the diagonal suffices
   loop(x, rows)   mQ.real(x, x) = 1.0;


   // [2]  →(0=(1↓⍴B),BMAX←⌈/∣,B)/0   ⍝ done if no or only near-0 columns

//    Q1(cols)
   if (cols == 0)   return pQ;
   loop(y, rows)
   loop(x, cols)
       {
         const double abs2 = mB.abs2(y, x);
         if (BMAX < abs2)   BMAX = abs2;
       }
   BMAX = sqrt(BMAX);
   if (BMAX < qct)
      {
//         Q1("B is 0")
        return pQ;   // all B[x;y] = 0
      }

   for (;;)
       {

   // [3]  SPRFLCTR: S←ID ↑⍴B ◊ Debug 'B'

        Matrix<cplx> mS (pS,  rows, rows);
mB.debug("[3] B");

   // [4]  L2←NORM2 COL1←B[;1] ◊ Debug 'COL1' ◊ Debug 'L2'

        Matrix<cplx> mCOL1 (pB,  rows, 1, mB.dY);   // COL1←B[;1]
mCOL1.debug("[4] COL1");
        norm_result L;   mB.col1_norm(L);
// Q1(L.norm2_real)
// Q1(L.norm2_imag)
        const bool significant = mCOL1.significant(BMAX, EPS);

   // [5]  IMBED → L2=0
   // [6]  IMBED → ∼0ϵ0=(1↓COL1) CMP_TOL EPS BMAX

        if (significant || (L.norm2_real + L.norm2_imag) > qct2)
           {
// Q1("SIGNIFICANT")
   // [7]  B[1;1]←(↑B) + (L2⋆÷2)×(0≤↑B)-0>↑B

#if 0
             Matrix<cplx>::add_sub(&mB.real(0, 0), &L.norm2_real);
#else
             double * B11_real = &mB.real(0, 0);
             double * B11_imag = B11_real + 1;
             if (*B11_real < 0)
                {
// Q1("SIGN ¯1")
// Q1(*B11_real)
// Q1(*B11_imag)
                  *B11_real -= L.norm_real;
                  *B11_imag -= L.norm_imag;
                }
             else
                {
// Q1("SIGN 1")
// Q1(*B11_real)
// Q1(*B11_imag)
                  *B11_real += L.norm_real;
                  *B11_imag += L.norm_imag;
                }
#endif

   // [8]   COL1←B[;1] ◊ Debug 'COL1'
   // [9]   SCALE←2÷NORM2 COL1 ◊ Debug 'NORM2 COL1' ◊ Debug 'SCALE'·
   // [10]  S←COL1∘.×COL1×SCALE ◊ (1 1⍉S)←1 1⍉S - 1.0


             // COL1←B[;1] changes nothing, so only S←... remains.
             // We have moved the initialization of S to the else
             // clause below. Therefore -S on the right does not work
             // here, but we can simply subtract 1.0 from the diagonal.
             //
mCOL1.debug("[8] COL1");
             norm_result scale;   mCOL1.col1_norm(scale);
             mS.init_outer_product(scale, mCOL1);
// Q1(scale.norm__2_real)
// Q1(scale.norm__2_imag)
             loop(y, rows)   mS.real(y, y) -= 1.0;   // subtract 1.0
           }
        else   // →IMBED but do [3] here
           {
             mS.init_identity(rows);
           }

   // [9] QI←ID N ◊ ((-2/↑⍴S)↑QI)←S ◊ Debug 'QI'

   mQi.imbed(mS);
mQi.debug("[11] QI");
mS .debug("[11] S");

mQ.debug("[12] Q before Q←Q+.×QI");
   // [12]   Q←Q+.×QI ◊ Debug 'Q'

   // use mT as temporary buffer for mB, so that init_inner_product() works
   mT.resize(mQ.M, mQ.N);   mT = mQ;

mT.debug("[12] T←Q before Q←Q+.×QI");
   mQ.init_inner_product(mT, mQi);
mQ.debug("[12] Q after Q←Q+.×QI");

   // since we are only interested in Q we can skip the final B←1 1↓S+.×B
   //
   if (0 == --cols)
      {
        mQ.debug("[end] Q");
        return pQ;
      }

   // [13]   Debug 'B' ◊ Debug 'S' ◊ B←1 1↓S+.×B ◊ Debug 'B'

   // use mT as temporary buffer for mB, so that init_inner_product() works
   mT.resize(mB.M, mB.N);   mT = mB;
mT.debug("[13] B");
mS.debug("[13] S");
   mB.resize(mS.M, mT.N);
   mB.init_inner_product(mS, mT);

   pB = mB.drop_1_1();   // 1 1↓B
mB.debug("[13] B");

   // [14]   →(0≠1↓⍴B)/SPRFLCTR

         --rows;
       }
}
//----------------------------------------------------------------------------

