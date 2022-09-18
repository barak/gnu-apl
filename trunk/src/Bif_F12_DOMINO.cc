/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2022  Dr. Jürgen Sauermann

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

/// a macro to enable debug printouts
// #define DOMINO_DEBUG

#include "Bif_F12_DOMINO.hh"
#include "Bif_F12_FORMAT.hh"
#include "ComplexCell.hh"
#include "Value.hh"
#include "Workspace.hh"
extern void divide_matrix(Value & Z, bool need_complex,
                          ShapeItem rows, ShapeItem cols_A, const Cell * cA,
                          ShapeItem cols_B, const Cell * cB);


Bif_F12_DOMINO   Bif_F12_DOMINO   ::_fun;    // ⌹
Bif_F12_DOMINO * Bif_F12_DOMINO::fun = &Bif_F12_DOMINO::_fun;

#ifndef DOMINO_DEBUG
# undef Q1
# define Q1(x)
#endif

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
   if (cols > rows)   LENGTH_ERROR;

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
   if (!X->is_scalar())   RANK_ERROR;
const double EPS = X->get_cfirst().get_real_value();

   if (B->get_rank() != 2)        RANK_ERROR;

   // if rank of A or B is < 2 then treat it as a
   // 1 by n (or 1 by 1) matrix..
   //
const ShapeItem rows_B = B->get_rows();
const ShapeItem cols_B = B->get_cols();
   if (rows_B <  cols_B)   LENGTH_ERROR;
   if (rows_B*cols_B == 0)   LENGTH_ERROR;


const bool need_complex = B->is_complex(true);
Value_P Z(3, LOC);
   QR_factorization(Z, need_complex, rows_B, cols_B, &B->get_cfirst(), EPS);

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

   if (rows_B <  cols_B)   LENGTH_ERROR;
   if (rows_A != rows_B)   LENGTH_ERROR;

const bool need_complex = A->is_complex(true) || B->is_complex(true);
Value_P Z(shape_Z, LOC);
   divide_matrix(*Z, need_complex, rows_A, cols_A, &A->get_cfirst(),
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
/// inver a real upper-triangle matrix
template<>
Value_P Bif_F12_DOMINO::invert_upper_triangle_matrix<false>(const ShapeItem M,
                                                            const ShapeItem N,
                                                            double * utm,
                                                            double * tmp)
{
   // ignore rows ≥ N; they are 0 and then cols ≥ N of the result as well.
   // the 0s will be filled in at the end.

matrix_assert(M >= N);
Matrix<false> UTM(utm, N, N);   // the matrix to be inverted
Matrix<false> AUG(tmp, N, N);   // the augmented matrix for the result

   AUG.init_identity(N);   // start with the identity matrix

UTM.debug("UTM orig");
AUG.debug("AUG orig");

   // divide every row by its diagonal element, so that the diagonal
   // elements of UTM become 1.0.
   //
   loop(y, N)   // for every row y
       {
         const double diag_y = UTM.real(y, y);
         if (diag_y == 0.0)   DOMAIN_ERROR;

         // divide off-diagonal UTM | AUG[y;] by diag_y
         for (ShapeItem x = y + 1; x < N; ++x)   UTM.real(y, x) /= diag_y;

         // divide diagonal of UTM | AUG by diag_y
         UTM.real(y, y) = 1.0;
         AUG.real(y, y) /= diag_y;
       }

UTM.debug("UTM unity " LOC);
AUG.debug("AUG unity " LOC);

   // subtract a multiple of the diagonal element from the items above it
   // so that the item becomes 0.0
   //
   loop(y, N)       // for every row y
   loop (y1, y)     // for every row y1 above y
       {
Q1(y) Q1(y1)
         const double factor = UTM.real(y1, y);
         if (factor == 0)   continue;   // already 0.0
Q1(factor)
         for (ShapeItem x1 = y1; x1 < N; ++x1)
             {
               // make UTM[y1;x1] zero by subtracting UTM[y;x] × UTM[y;]
               UTM.real(y1, x1) -= UTM.real(y, x1) * factor;
               AUG.real(y1, x1) -= AUG.real(y, x1) * factor;
             }
UTM.debug("UTM at " LOC);
AUG.debug("AUG at " LOC);
       }

AUG.debug("AUG at " LOC);

   // create the result value
const Shape shape_INV(N, M);
Value_P INV(shape_INV, LOC);
   loop(y, N)   // for every row y
       {
         loop(x, M)   // for every column y
             {
               const double flt = (x < N) ? AUG.real(y, x) : 0.0;
               if (!isfinite(flt))   DOMAIN_ERROR;
               INV->next_ravel_Float(flt);
             }
       }

   INV->check_value(LOC);
   return INV;
}
//----------------------------------------------------------------------------
/// inver a complex upper-triangle matrix
template<>
Value_P Bif_F12_DOMINO::invert_upper_triangle_matrix<true>(ShapeItem M,
                                                           ShapeItem N,
                                                           double * utm,
                                                           double * tmp)
{
   // ignore rows ≥ N; they are 0 and then cols ≥ N of the result as well.
   // the 0s will be filled in at the end.

matrix_assert(M >= N);
Matrix<true> UTM(utm, N, N);   // the matrix to be inverted
Matrix<true> AUG(tmp, N, N);   // the augmented matrix for the result

   AUG.init_identity(N);   // start with the identity matrix

UTM.debug("UTM orig");
AUG.debug("AUG orig");

   // divide every row by its diagonal element, so that the diagonal
   // elements of UTM become 1.0.
   //
   loop(y, N)   // for every row y
       {
         const complex<double> diag_y = UTM.get_Z(y, y);
         if (diag_y.real() == 0.0 && diag_y.imag())   DOMAIN_ERROR;

         // divide off-diagonal UTM | AUG[y;] by diag_y
         for (ShapeItem x = y + 1; x < N; ++x)
             UTM.div_Z(y, x, diag_y);

         // divide diagonal of UTM | AUG by diag_y
         UTM.set_Z(y, y, complex<double>(1.0, 0.0));
         AUG.div_Z(y, y, diag_y);
       }

UTM.debug("UTM unity " LOC);
AUG.debug("AUG unity " LOC);

   // subtract a multiple of the diagonal element from the items above it
   // so that the item becomes 0.0
   //
   loop(y, N)       // for every row y
   loop (y1, y)     // for every row y1 above y
       {
Q1(y) Q1(y1)
         const complex<double> factor = UTM.get_Z(y1, y);
         if (factor.real() == 0 && factor.imag() == 0)   continue;
Q1(factor)
         for (ShapeItem x1 = y1; x1 < N; ++x1)
             {
               // make UTM[y1;x1] zero by subtracting UTM[y;x] × UTM[y;]
               UTM.sub_Z(y1, x1, UTM.get_Z(y, x1) * factor);
               AUG.sub_Z(y1, x1, AUG.get_Z(y, x1) * factor);
             }
UTM.debug("UTM at " LOC);
AUG.debug("AUG at " LOC);
       }

AUG.debug("AUG at " LOC);

   // create the result value
const Shape shape_INV(N, M);
Value_P INV(shape_INV, LOC);
   loop(y, N)   // for every row y
       {
         loop(x, M)   // for every column y
             {
               if (x < N)   // diagonal or above
                  {
                    const double re = AUG.real(y, x);
                    const double im = AUG.imag(y, x);
                    if (!(isfinite(re) && isfinite(im)))   DOMAIN_ERROR;
                    INV->next_ravel_Complex(re, im);
                  }
               else   // below diagonal
                  {
                    INV->next_ravel_Complex(0.0, 0.0);
                  }
             }
       }

   INV->check_value(LOC);
   return INV;
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

      Complex numbers are stored as real followed by imag part.

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

   {
     const Shape Q_shape(rows, rows);
     Value_P Qv(Q_shape, LOC);
     if (need_complex)
        {
          loop(q, len)
              {
                const double re = data[base_Q + 2*q];
                const double im = data[base_Q + 2*q + 1];
                if (!(isfinite(re) && isfinite(im)))   DOMAIN_ERROR;
                Qv->next_ravel_Complex(re, im);
              }
        }
     else
        {
          loop(q, len)
              {
                const double re = data[base_Q + q];
                if (!isfinite(re))   DOMAIN_ERROR;
                Qv->next_ravel_Float(re);
              }
        }
     Qv->check_value(LOC);
     Z->next_ravel_Pointer(Qv.get());
   }
   {
     const Shape shape_R(rows, cols);
     Value_P vR(shape_R, LOC);
     if (need_complex)
        {
          loop(r, rows*cols)
              {
                const double re = data[base_R + 2*r];
                const double im = data[base_R + 2*r + 1];
                if (!(isfinite(re) && isfinite(im)))   DOMAIN_ERROR;
                vR->next_ravel_Complex(re, im);
              }
        }
     else
        {
          loop(r, rows*cols)
              {
                const double re = data[base_R + r];
                if (!isfinite(re))   DOMAIN_ERROR;
                vR->next_ravel_Float(re);
              }
        }
     vR->check_value(LOC);
     Z->next_ravel_Pointer(vR.get());

      if (need_complex)
         {
           Value_P INV = invert_upper_triangle_matrix<true>(rows, cols,
                                                            data + base_R,
                                                            data + base_Q);
           Z->next_ravel_Pointer(INV.get());
         }
      else
         {
           Value_P INV = invert_upper_triangle_matrix<false>(rows, cols,
                                                             data + base_R,
                                                             data + base_Q);
           Z->next_ravel_Pointer(INV.get());
         }
   }

   delete[] data;
}
//----------------------------------------------------------------------------
void
Bif_F12_DOMINO::setup_complex_B(const Cell * cB, double * D, ShapeItem count)
{
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

   Q1(cols)
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
        Q1("B is 0")
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
Q1(L.norm2_real)
Q1(L.norm2_imag)
        const bool significant = mCOL1.significant(BMAX, EPS);

   // [5]  IMBED → L2=0
   // [6]  IMBED → ∼0ϵ0=(1↓COL1) CMP_TOL EPS BMAX

        if (significant || (L.norm2_real + L.norm2_imag) > qct2)
           {
Q1("SIGNIFICANT")
   // [7]  B[1;1]←(↑B) + (L2⋆÷2)×(0≤↑B)-0>↑B

#if 0
             Matrix<cplx>::add_sub(&mB.real(0, 0), &L.norm2_real);
#else
             double * B11_real = &mB.real(0, 0);
             double * B11_imag = B11_real + 1;
             if (*B11_real < 0)
                {
Q1("SIGN ¯1")
Q1(*B11_real)
Q1(*B11_imag)
                  *B11_real -= L.norm_real;
                  *B11_imag -= L.norm_imag;
                }
             else
                {
Q1("SIGN 1")
Q1(*B11_real)
Q1(*B11_imag)
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
Q1(scale.norm__2_real)
Q1(scale.norm__2_imag)
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

