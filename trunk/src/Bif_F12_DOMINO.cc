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

#include "Bif_F12_DOMINO.hh"
#include "Bif_F12_FORMAT.hh"
#include "ComplexCell.hh"
#include "LibPaths.hh"
#include "Polynomial.hh"
#include "PythonPipe.hh"
#include "Sys.hh"
#include "Tokenizer.hh"
#include "Value.hh"
#include "Workspace.hh"

#include "LApack.hh"

#if apl_GSL
# include "QR_factorization_GSL.hh"
#endif

# include "LAdebug.icc"   // print_matrix() etc.

Bif_F12_DOMINO Bif_F12_DOMINO::fun;    // ⌹

const FunctionGroup::function_info Bif_F12_DOMINO::subfunction_infos[] =
{
#define domino_def(N, name, comm_2) { N, #name, "", comm_2, -1 },
  domino_def( 1, qr_fact_helzer , "QR factorization of B (Helzer algorithm)"  )
  domino_def( 2, qr_fact_gsl    , "QR factorization of B (libgsl algorithm)"  )
  domino_def( 3, rq_fact        , "RQ factorization of B (libgsl algorithm)"  )
  domino_def( 4, lq_fact        , "LQ factorization of B (libgsl algorithm)"  )
  domino_def( 5, ql_fact        , "QL factorization of B (libgsl algorithm)"  )
  domino_def( 6, lu_fact        , "LU factorization of B (libgsl algorithm)"  )
  domino_def( 7, poly_print     , "print polynomial B"                        )
  domino_def( 8, poly_multiply  , "polynomial A × polynomial B"               )
  domino_def( 9, poly_divide    , "polynomial A ÷ polynomial B"               )
  domino_def(10, poly_divideN   , "polynomial A ÷ polynomial B (multivariate)")
  domino_def(11, poly_scan      , "string B to polynomial"                    )
  domino_def(12, poly_divideNO  , "polynomial A ÷ polynomial B (with order)"  )
  domino_def(20, integral       , "compute integral for B"                    )
#undef domino_def
};
//----------------------------------------------------------------------------
Bif_F12_DOMINO::Bif_F12_DOMINO()
   : NonscalarFunction_default_identity(TOK_F12_DOMINO)
{
enum { count = sizeof(subfunction_infos) / sizeof(*subfunction_infos) };
   init_function_group(subfunction_infos, count, "⌹");
}
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

   if (B->get_rank() == 1)   // help or inversion at the unit sphere
      {
        if (B->element_count() == 0)   // '' or ⍬: help
           {
             if (B->get_cfirst().is_character_cell())      list_functions(CERR);
             else if (B->get_cfirst().is_integer_cell())   list_mappings(CERR);
             else                                          DOMAIN_ERROR;
             return Token(TOK_APL_VALUE1, Idx0_0(LOC));
           }

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
        MORE_ERROR() <<
        "⌹B : B is under-specified (B has more columns than rows)";
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
   // X shall be a scalar with:
   //
   // a. real EPS < 1 (aka. rcond) for backward compatibility (not used), or
   // b. integer 0 for the QR factorization with the Helzer algorithm, or
   // c. integer 1 for the QR factorization with LApack
   //
   if (!X->is_scalar())   RANK_ERROR;

enum { ALGO_BAD,         ///< bad algorithm number
       ALGO_QR_HELZER,   ///< QR factorization with Gary Helzer's algorithm
       ALGO_QR_LAPACK,   ///< QR " with a LApack based algorithm (obsolete)
       ALGO_QR_GSL,      ///< QR factorization with a libgsl based algorithm
       ALGO_RQ_GSL,      ///< RQ factorization (libgsl based, ⍉(QR ⍉B)
       ALGO_LQ_GSL,      ///< LQ factorization (libgsl based, ⍉(QL ⍉B)
       ALGO_QL_GSL,      ///< QL factorization (libgsl based)
       ALGO_LU_GSL,      ///< LU factorization (libgsl based)
     } algo = ALGO_BAD;

const Cell & X0 = X->get_cscalar();
double EPS = Workspace::get_CT();

   if (X0.is_float_cell())   // a.
      {
        EPS = X0.get_real_value();
        if (EPS < 0.1)   algo = ALGO_QR_HELZER;
      }
   else if (X0.is_integer_cell())   // b. or c.
      {
        APL_Integer x0 = X0.get_int_value();
        if      (x0 <= 1)   algo = ALGO_QR_HELZER;
#if apl_GSL
        else if (x0 ==  2)   algo = ALGO_QR_GSL;
        else if (x0 ==  3)   algo = ALGO_RQ_GSL;
        else if (x0 ==  4)   algo = ALGO_LQ_GSL;
        else if (x0 ==  5)   algo = ALGO_QL_GSL;
        else if (x0 ==  6)   algo = ALGO_LU_GSL;
#endif
        else if (x0 ==  7)   return Token(TOK_APL_VALUE1, print_polynomial(*B));
        else if (x0 == 11)   return Token(TOK_APL_VALUE1, scan_polynomial(*B));
        else if (x0 == 20)   return Token(TOK_APL_VALUE1, integral(0, *B));
      }

   if (algo == ALGO_BAD)   // none of the above
      {
        MORE_ERROR() << "⌹[X]B: Invalid algorithm X";
        DOMAIN_ERROR;
      }

   if (B->get_rank() != 2)   RANK_ERROR;

   // if rank of A or B is < 2 then treat it as a
   // 1 by n (or 1 by 1) matrix..
   //
const ShapeItem M = B->get_rows();
const ShapeItem N = B->get_cols();
   if (M*N == 0)   LENGTH_ERROR;   // empty B

const bool need_complex = B->is_complex(true);
Value_P Z(3, LOC);

   if (algo == ALGO_QR_HELZER)
      {
        LA_DEBUG && CERR <<
                    "QR factorization with Gary Helzer's algorithm...\n";
        QR_Helzer(Z, need_complex, M, N, &B->get_cfirst(), EPS);
      }
#if apl_GSL
   else if (algo == ALGO_QR_GSL)
      {
        LA_DEBUG && CERR << "QR factorization with libgsl algorithm...\n";
        if (need_complex)
           {
             GSL::QR_factorize_ZZ_matrix(*Z, M, N, &B->get_cfirst());
           }
        else   // real
           {
             GSL::QR_factorize_DD_matrix(*Z, M, N, &B->get_cfirst());
           }
      }
   else if (algo == ALGO_RQ_GSL)
      {
        if (need_complex)
           {
             MORE_ERROR() << "RQ factorization is only available "
                             "for real matrices.";
             DOMAIN_ERROR;
           }

        LA_DEBUG && CERR << "RQ factorization with libgsl algorithm...\n";
        GSL::RQ_factorize(*Z, M, N, B);
      }
   else if (algo == ALGO_LQ_GSL)
      {
        LA_DEBUG && CERR << "LQ factorization with libgsl algorithm...\n";
        GSL::LQ_factorize(*Z, M, N, B, need_complex);
      }
   else if (algo == ALGO_QL_GSL)
      {
        LA_DEBUG && CERR << "QL factorization with libgsl algorithm...\n";
        if (need_complex)
           {
             MORE_ERROR() << "LQ factorization is only available "
                             "for real matrices.";
             DOMAIN_ERROR;
           }
        else   // real
           {
             GSL::QL_factorize_DD_matrix(*Z, M, N, &B->get_cfirst());
           }
      }
   else if (algo == ALGO_LU_GSL)
      {
        LA_DEBUG && CERR << "LU factorization with libgsl algorithm...\n";
        if (need_complex)
           {
             GSL::LU_factorize_ZZ_matrix(*Z, M, N, &B->get_cfirst());
           }
        else
           {
             GSL::LU_factorize_DD_matrix(*Z, M, N, &B->get_cfirst());
           }
      }
#endif

   Z->set_proto_Int();   // never since M*cols__B ≠ 0. Just for clarity
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
const APL_Integer X0 = X->get_sole_integer();
   if (X0 < 7)     VALENCE_ERROR;
   switch(X0)
      {
        case  7: return Token(TOK_APL_VALUE1, print_polynomial(*A, *B));
        case  8: return Token(TOK_APL_VALUE1, polynomial_product(*A, *B));
        case  9: return Token(TOK_APL_VALUE1, poly_quotient(*A, *B));
        case 10: return Token(TOK_APL_VALUE1, poly_quotient_NO(*A, *B, 0, 0));
        case 11: return Token(TOK_APL_VALUE1, scan_polynomial(*A, *B));
        case 12: return Token(TOK_APL_VALUE1, poly_quotient_N(*A, *B));
        case 20: return Token(TOK_APL_VALUE1, integral(A.get(), *B));
      }

   MORE_ERROR() << "A ⌹[X] B: invalid function number X (=" << X0 << ").";
   DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Token
Bif_F12_DOMINO::eval_AB(Value_P A, Value_P B) const
{
ShapeItem rows_A = 1;
ShapeItem cols_A = 1;
ShapeItem rows_B = 1;
ShapeItem cols_B = 1;

   // if rank of A or B is < 2 then treat it as a
   // 1 by n (or 1 by 1) matrix..
   //
Shape shape_Z;   // ⍴Z ←→ (¯1↓⍴A), (1↓⍴B)
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

   if (rows_B < cols_B)
      {
        MORE_ERROR() <<
        "A⌹B : B is under-specified (B has more columns than rows)";
        LENGTH_ERROR;
       }

   if (rows_A != rows_B)
      {
        MORE_ERROR() << "A÷B : number of rows in A ≠ number of rows in B";
        LENGTH_ERROR;
      }

const bool need_complex = A->is_complex(true) || B->is_complex(true);
Value_P Z(shape_Z, LOC);
const sRank rank = need_complex ?  LA_pack::divide_ZZ_matrix(*Z, rows_A,
                                          cols_A, &A->get_cfirst(),
                                          cols_B, &B->get_cfirst())
                                :  LA_pack::divide_DD_matrix(*Z, rows_A,
                                          cols_A, &A->get_cfirst(),
                                          cols_B, &B->get_cfirst());

   if (rank < cols_B)
      {
        const char * type = need_complex ? "complex" : "real";
        MORE_ERROR() << "A⌹B : linearly dependent (" << type << ") B?"
                        " ⍴B is " << rows_A << " " << cols_B
                     << ", but the estimated rank is " << rank << ".\n"
                     << "      NOTE that the estimated rank "
                        "is controlled by ⎕CT.";
        DOMAIN_ERROR;
      }

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
void
Bif_F12_DOMINO::print_fun_syntax(ostream & out,
                                const function_info & info) const
{
const uAxis axis = info.axis;

const char * Z = "";   // result
   switch(axis)
      {
        default: Z = "    (Q R Ri)";   break;   // QR factorizations
        case  6: Z = "    (P U Li)";   break;   // LU factorization
        case  7: Z = "          Zs";   break;   // print polynomial
        case  8:
        case  9:
        case 10:
        case 11:
        case 12: Z = "       Zpoly";   break;   // 
      }

const char * A = "←   ";   // left arg

   if (axis >= 8 && axis <= 10)   A = "← A ";   // dyadic

   out << Z << "   " << A << "⌹[" << setw(2) << axis << "] B   ⍝ "
       << info.comment_fun << endl;
   if (axis == 10)
   out << Z << A << "LO ⌹[" << setw(2) << axis << "] B   ⍝ "
       << "dito with optional monomial order LO" << endl;
}
//----------------------------------------------------------------------------
void
Bif_F12_DOMINO::print_map_syntax(ostream & out,
                                 const function_info & info) const
{
const char * name = info.function_name;
const UCS_string blanks(max_function_name_length - strlen(name), UNI_SPACE);
   out << "    ⌹[" << setw(2) << info.axis << "]  ←→  ⌹['" << name << "']"
       << blanks << "  ←→  ⌹." << name << endl;
}
//----------------------------------------------------------------------------
/// print debug infos for \b this real matrix
template<>
void Bif_F12_DOMINO::Matrix<false>::debug(const char * name) const
{
#if DOMINO_DEBUG
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
#if DOMINO_DEBUG
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
void
Bif_F12_DOMINO::QR_Helzer(Value_P Z, bool need_complex, ShapeItem M,
                          ShapeItem N, const Cell * cB, double EPS)
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
   // allocated as M * M so that we can freely rorate them...
   //
const int CPLX = need_complex ? 2 : 1;   // number of doubles per variable item
const ShapeItem max_MN  = M > N ? M : N;
const ShapeItem len_B   = M * N;
const ShapeItem len_QR  = max_MN * max_MN;
const ShapeItem base_B  = 1;
const ShapeItem base_Q  = 1 + base_B  + CPLX*len_QR + 1;
const ShapeItem base_Qi = 1 + base_Q  + CPLX*len_QR + 1;
const ShapeItem base_R  = 1 + base_Qi + CPLX*len_QR + 1;
const ShapeItem base_S  = 1 + base_R  + CPLX*len_QR + 1;
const ShapeItem end     = 1 + base_S  + CPLX*len_QR + 1;
#define base_AUG  base_Q   /* reuse Q */

if ((size_t)end > SIZE_MAX / CPLX)   WS_FULL;
double * data = new double[end*CPLX];
   memset(data, 0, end*sizeof(double));
   data[base_B - 1]  = 42.0;   data[base_B  + CPLX*len_B] = 43.0;
   data[base_Q - 1]  = 44.0;   data[base_Q  + CPLX*len_QR]   = 45.0;
   data[base_Qi - 1] = 46.0;   data[base_Qi + CPLX*len_QR]   = 47.0;
   data[base_R - 1]  = 48.0;   data[base_R  + CPLX*len_QR]   = 49.0;
   data[base_S - 1]  = 50.0;   data[base_S  + CPLX*len_QR]   = 51.0;

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
        double * Q = householder<true>(data + base_B, M, N, data + base_Q,
                          data + base_Qi, data + base_R, data + base_S, EPS);

        setup_complex_B(cB, data + base_B, len_B);   // restore B
        const Matrix<true> Bm(data + base_B, M, N);
        Matrix<true> Qm(Q, M, M);
        Qm.transpose(M);
        Matrix<true> Rm(data + base_R, M, N);
        Rm.init_inner_product(Qm, Bm);
        Qm.debug("final Q");
        Rm.debug("final R");
      }
   else                // real B
      {
   Assert(data[base_B + CPLX*len_B]  == 43.0);
        setup_real_B(cB, data + base_B, len_B);
   Assert(data[base_B + CPLX*len_B]  == 43.0);
        double * Q = householder<false>(data + base_B, M,N, data + base_Q,
                           data + base_Qi, data + base_R, data + base_S, EPS);

        setup_real_B(cB, data + base_B, len_B);   // restore B
        const Matrix<false> Bm(data + base_B, M, N);
        Matrix<false> Qm(Q, M, M);
        Qm.transpose(M);
        Matrix<false> Rm(data + base_R, M, N);
        Rm.init_inner_product(Qm, Bm);
   Assert(data[base_B + CPLX*len_B]  == 43.0);
      }

   // check that the memory areas were not overridden
   //
   Assert(data[base_B - 1]            == 42.0);
   Assert(data[base_B + CPLX*len_B]   == 43.0);
   Assert(data[base_Q - 1]            == 44.0);
   Assert(data[base_Q + CPLX*len_QR]  == 45.0);
   Assert(data[base_Qi - 1]           == 46.0);
   Assert(data[base_Qi + CPLX*len_QR] == 47.0);
   Assert(data[base_R - 1]            == 48.0);
   Assert(data[base_R + CPLX*len_QR]  == 49.0);
   Assert(data[base_S - 1]            == 50.0);
   Assert(data[base_S + CPLX*len_QR]  == 51.0);

   // Z[1] aka. Q
   {
     const Shape shape_Z1(M, M);
     Value_P Z1(shape_Z1, LOC);
     if (need_complex)
        {
          ALL_COLS(M)   // FORTRAN order
          ALL_ROWS(M)
             {
               const ShapeItem offset = 2*(col + row*M);
               const double real = data[base_Q + offset];
               const double imag = data[base_Q + offset + 1];
               if (!(isfinite(real) && isfinite(imag)))   DOMAIN_ERROR;
               Z1->next_ravel_Complex(real, imag);
             }
        }
     else
        {
          ALL_COLS(M)   // FORTRAN order
          ALL_ROWS(M)
             {
               const ShapeItem offset = col + row*M;
               const double real = data[base_Q + offset];
               if (!isfinite(real))   DOMAIN_ERROR;
               Z1->next_ravel_Float(real);
             }
        }
     Z1->check_value(LOC);
     Z->next_ravel_Pointer(Z1.get());
   }

   // Z[2] aka. R
   {
     const Shape shape_Z2(N, N);
     Value_P Z2(shape_Z2, LOC);
     if (need_complex)
        {
          ALL_ROWS(N)   // APL order
          ALL_COLS(N)   // APL order
             {
               const ShapeItem offset = col + row*N;   // APL order
               if (row > col)   // below diagonal: force 0.0
                  {
                     data[base_R + 2*offset]     = 0;
                     data[base_R + 2*offset + 1] = 0;
                  }
               const double real = data[base_R + 2*offset];
               const double imag = data[base_R + 2*offset + 1];
               if (!(isfinite(real) && isfinite(imag)))   DOMAIN_ERROR;
               Z2->next_ravel_Complex(real, imag);
              }
        }
     else
        {
          ALL_ROWS(N)   // APL order
          ALL_COLS(N)   // APL order
             {
               const ShapeItem offset = col + row*N;   // APL order
               if (row > col)   // below diagonal: force 0.0
                  {
                     data[base_R + offset] = 0;
                  }
               const double real = data[base_R + offset];
               if (!isfinite(real))   DOMAIN_ERROR;
               Z2->next_ravel_Float(real);
              }
        }
     Z2->check_value(LOC);
     Z->next_ravel_Pointer(Z2.get());
   }


const ShapeItem D = M < N ? M : N;   // length of the diagonal

   // function householder above has computed R aka. UTM in APL order.
   // Function LA_pack::invert_T_UTM() wants it in FORTRAN order.
   // We therefore need to ⍉ R and possibly fix UTM
   //
   if (M < N)   // fix UTM
      {
        /* R is under-specified. We want to invert only M M↑R,
           which requires some re-ordering of R:

              ├──────── N ────────┤        ├────── M ──────┤
           ┬  ╔═════════C═════════╗     ┬  ╔═════════C═════╗
           │  ║               │   ║     │  ║               ║
           │  ║               │   ║     │  ║               ║
           M  ║               │   ║  →  M  ║               ║
           │  ║               │   ║     │  ║               ║
           │  ║               │   ║     │  ║               ║
           ┴  ╚═══════════════════╝     ┴  ╚═══════════════╝
              ├────── M ──────┤
         */
          ALL_ROWS(M)   // APL order
          ALL_COLS(M)   // APL order
             {
               const ShapeItem offset_from = col + N*row;
               const ShapeItem offset_to   = col + M*row;
               if (need_complex)
                  {
                    data[base_R + 2*offset_to] = data[base_R + 2*offset_from];
                    data[base_R + 2*offset_to + 1] =
                                             data[base_R + 2*offset_from + 1];
                  }
               else
                  {
                    data[base_R + offset_to] = data[base_R + offset_from];
                  }
             }
      }

   if (need_complex)
      {
        LA_pack::fMatrix<LA_pack::ZZ >UTM(data + base_R,   D, D, D);
        LA_pack::fMatrix<LA_pack::ZZ >AUG(data + base_AUG, D, D, D);

        UTM.set_transpose();   // ⍉R

        Value_P Z3 = LA_pack::invert_ZZ_UTM(M, N, UTM, AUG);
        Z->next_ravel_Pointer(Z3.get());
      }
   else
      {
        LA_pack::fMatrix<LA_pack::DD>UTM(data + base_R,   D, D, D);
        LA_pack::fMatrix<LA_pack::DD>AUG(data + base_AUG, D, D, D);

        UTM.set_transpose();   // ⍉R

        Value_P Z3 = LA_pack::invert_DD_UTM(D, D, UTM, AUG);
        Z->next_ravel_Pointer(Z3.get());
      }

   delete[] data;
#undef base_AUG
}
//----------------------------------------------------------------------------
template<bool cplx>
double *
Bif_F12_DOMINO::householder(double * pB, ShapeItem rows, ShapeItem cols,
                            double * pQ, double * pQi, double * pT, double * pS,
                            double EPS)
{
   // pB is the matrix to be factorized.
   // the caller has initialized pQ, pQi, and pT to 0.0
   //
   // the algorithm is essentially the one described in Garry Helzer's paper
   // "THE HOUSEHOLDER ALGORITHM AND APPLICATIONS" but using complex numbers
   // when needed.

ShapeItem dias = rows < cols ? rows : cols;   // number of diagonals
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
   if (0 == --dias)
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
Value_P
Bif_F12_DOMINO::print_polynomial(const Value & A, const Value & B)
{
   /* A is a vector of names for the indeterminants (typically
      'x', 'x'y, 'x'z, ...) and B is the coefficients of the powers of
      the indeterminants.

      We support the following variants of A and B:

      1. 1-dimensional B and single string A (varname = indeterminant)
      2. N-dimensional B and single string A (N 1-character indeterminants)
      3. N-dimensional B and N strings A (N variable-length indeterminants)
    */
const ShapeItem ec_A = A.element_count();

   if (A.get_rank() > 1)   RANK_ERROR;   // neither name nor vector of names

UCS_string_vector vars;
   if (A.is_char_array())   // case 1. or 2.
      {
        if (B.get_rank() <= 1)   // case 1. (single indeterminant)
           {
             const UCS_string x(A);
             vars.push_back(x);
           }
        else                      // case 2: N 1-character indeterminant
           {
             if (ec_A != B.get_rank())   LENGTH_ERROR;
             loop(a, ec_A)
                 {
                   const UCS_string x(A.get_cravel(a).get_char_value());
                   vars.push_back(x);
                 }
           }
      }
   else                     // case 3.
      {
        // every axis of B is one indeterminant of the polynomial
        //
        if (B.get_rank() > ec_A)   LENGTH_ERROR;
        loop(a, ec_A)
            {
              const Cell & cell = A.get_cravel(a);
              const Value & x = *cell.get_pointer_value();
              if (!x.is_char_array())
                 {
                    MORE_ERROR() << "A ⌹.print_poly B: Bad variable name A["
                                 << a << "]";
                    DOMAIN_ERROR;
                 }
              const UCS_string var(x);
              vars.push_back(var);
            }
      }

const UCS_string Z_ucs = print_polynomial(vars, B);
   return Value_P(Z_ucs, LOC);
}
//----------------------------------------------------------------------------
UCS_string
Bif_F12_DOMINO::print_polynomial(const UCS_string_vector & vars,
                                 const Value & B)
{
const UTF8_string expo_digits_utf("⁰¹²³⁴⁵⁶⁷⁸⁹");
const UCS_string expo_digits(expo_digits_utf);

const sRank rank_B = B.get_rank();
const ShapeItem ec_B = B.element_count();
UCS_string poly;

int term = 0;
   loop(n, ec_B)   // loop over terms
      {
        const size_t b = ec_B - n - 1;
        const Cell & coeff = B.get_cravel(b);

        if (coeff.is_near_zero())   continue;   // hide zero terms entirely

        const bool subsequent_term = term++;

        // hide coefficient 1 (unless the term is constant)
        if (coeff.is_near_one())
           {
             if (subsequent_term)   poly << " + ";
             if (b == 0)            poly << "1";
           }
        else if (coeff.is_near_int() && coeff.get_checked_near_int() == -1)
           {
             if (subsequent_term && b)   poly << " - ";
             else if (subsequent_term)   poly << " - 1";
             else if (b == 0)            poly << "¨1";
             else                        poly << "-";
           }
        else if (coeff.is_integer_cell())   // integer coefficient
           {
             const int value = coeff.get_int_value();
             if (value < 0)
                {
                  if (subsequent_term)   poly << " - " << - value;
                  else                   poly << "¯"   << - value;
                }
             else
                {
                  if (subsequent_term)   poly << " + " << value;
                  else                   poly << value;
                }
           }
        else if (coeff.is_float_cell())   // real coefficient
           {
             const double value = coeff.get_real_value();
             if (value < 0)
                {
                   if (subsequent_term)   poly << " - " << - value;
                   else                   poly << "¯"   << - value;
                }
             else
                {
                  if (subsequent_term)   poly << " + " << value;
                  else                   poly << value;
                }
           }
        else if (coeff.is_complex_cell())   // complex coefficient
           {
             double c_real = coeff.get_real_value();
             double c_imag = coeff.get_imag_value();
             if (c_real < 0 && c_imag < 0)
                {
                  if (subsequent_term)
                     poly << " - " << (-c_real) << (-c_imag);
                  else
                     poly << "¯" << (-c_real) << "J¯" << (-c_imag);
                }
             else if (c_real < 0)   // and c_imag > 0
                {
                  if (subsequent_term)
                     poly << " - " << (-c_real) << "J" << c_imag;
                  else
                     poly << "¯" << (-c_real) << "J¯" << (-c_imag);
                }
             else if (c_imag < 0)   // and c_real > 0
               {
                  if (subsequent_term)   poly << " + ";
                  poly << c_real << "J¯" << (-c_imag);
               }
             else   // both positive
                {
                  if (subsequent_term)   poly << " + ";
                  poly << c_real << "J" << c_imag;
                }
           }
        else
           {
             MORE_ERROR() << "A ⌹.poly_print B: non-numeric coefficient in B";
             DOMAIN_ERROR;
           }

        // show the indeterminant()
        //
        const Shape powers = B.get_shape().offset_to_index(b, /* ⎕IO */0);

       // show non-zero indeterminants
       //
       loop(p, rank_B)
           {
             const size_t power = powers.get_shape_item(p);
             if (power == 0)   continue;   // hide indeterminants I⁰

             // show the indeterminant, e.g. Xⁿ
             //
             if (power == 0)   continue;   // hide factor X⁰ (== 1) completely

             poly << vars[p];   // name of the indeterminant
             if (power == 1)
                {
                  // hide exponent 1 (since X¹ == X).
                }
             else if (power < 10)   // single digit exponent
                {
                  poly << expo_digits[power];
                }
             else              // multi digit power
                {
                  UCS_string pow;

                  // power in reverse order
                  //
                  for (size_t p = power; p; p /= 10)
                      {
                        pow << expo_digits[p % 10];
                      }

                  loop(p, pow.size())   poly << pow[pow.size() - p - 1];
                }
           }
      }

   // if poly is empty then set c₀ = 0;
   //
   if (poly.size() == 0)   poly << UNI_0;
   return poly;
}
//----------------------------------------------------------------------------
Value_P
Bif_F12_DOMINO::print_polynomial(const Value & B)
{
const uRank rank_B = B.get_rank();
   if (rank_B > 8)   RANK_ERROR;

const char * xyz = "xyzuwvst";
UCS_string_vector vars;   vars.reserve(8);
   loop(v, rank_B)
       {
         const Unicode var = Unicode(xyz[v]);
         const UCS_string var_ucs(var);
         vars.push_back(var_ucs);
       }

const UCS_string Z = print_polynomial(vars, B);
   return Value_P(Z, LOC);
}
//----------------------------------------------------------------------------
Value_P
Bif_F12_DOMINO::polynomial_product(const Value & A, const Value & B)
{
   // the rank is the number of indeterminants (which should be the same for
   // A and B).
   //
Shape shape_A = A.get_shape();
Shape shape_B = B.get_shape();
   while (shape_A.get_rank() < shape_B.get_rank())   shape_A.add_shape_item(1);
   while (shape_B.get_rank() < shape_A.get_rank())   shape_B.add_shape_item(1);
const sRank rank = shape_A.get_rank();
   Assert(shape_A.get_rank() == shape_B.get_rank());

const ShapeItem ec_A = A.element_count();
const ShapeItem ec_B = B.element_count();

   if (ec_A == 0)   LENGTH_ERROR;   // at least a₀ is required
   if (ec_B == 0)   LENGTH_ERROR;   // at least b₀ is required

   loop(a, ec_A)   if (!A.get_cravel(a).is_numeric())   DOMAIN_ERROR;
   loop(b, ec_B)   if (!B.get_cravel(b).is_numeric())   DOMAIN_ERROR;

Shape shape_Z;
   loop(r, rank)
       {
         shape_Z.add_shape_item(shape_A.get_shape_item(r) +
                                shape_B.get_shape_item(r) - 1);
       }

   typedef complex<double> Complex;
vector<Complex> vZ(shape_Z.get_volume(), Complex(0));

   loop(a, ec_A)
       {
        const Cell & cell_A = A.get_cravel(a);
        if (cell_A.is_near_zero())   continue;
        const Shape idx_A = shape_A.offset_to_index(a, /* ⎕IO */ 0);
        loop(b, ec_B)
           {
             const Cell & cell_B = B.get_cravel(b);
             if (cell_B.is_near_zero())   continue;
             const Shape idx_B = shape_B.offset_to_index(b, /* ⎕IO */ 0);

             // at this point A and B are non-zero coefficients, and
             // idx_A and idx_B are the powers of their indeterminants
             //
             // Compute the product of the powers
             //
             Shape idx_AB;
             loop(r, rank)   // loop over the indeterminants
                 {
                   // the product of powers is the sum of the exponents
                   //
                   idx_AB.add_shape_item(idx_A.get_shape_item(r) +
                                         idx_B.get_shape_item(r));
                 }

             const ShapeItem pos = shape_Z.ravel_pos(idx_AB);
             const double real_a = cell_A.get_real_value();
             const double real_b = cell_B.get_real_value();
             if (cell_A.is_near_real() && cell_B.is_near_real())
                {
                  vZ[pos] = Complex(vZ[pos].real() + real_a * real_b, 0);
                }
             else
                {
                  const double imag_a = cell_A.get_imag_value();
                  const double imag_b = cell_B.get_imag_value();
                  vZ[pos] = Complex(vZ[pos].real() + real_a * real_b
                                                   - imag_a * imag_b,
                                    vZ[pos].imag() + real_a * imag_b
                                                   + imag_a * real_b);

                }
           }
       }

   // construct the result
   //
Value_P Z(shape_Z, LOC);
   loop(z, shape_Z.get_volume())
       {
         const double real_z = vZ[z].real();
         const double imag_z = vZ[z].imag();
         if (!Cell::is_near_zero(imag_z))   // complex coefficient
             {
               Z->next_ravel_Complex(real_z, imag_z);
             }
         else if (Cell::is_near_zero(real_z))   // 0J0
             {
               Z->next_ravel_0();
             }
          else
             {
               Z->next_ravel_Number(real_z);
             }
       }

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Bif_F12_DOMINO::poly_quotient(const Value & A, const Value & B)
{
   if (A.get_rank() > 1)   RANK_ERROR;
   if (B.get_rank() > 1)   RANK_ERROR;

const ShapeItem ec_A = A.element_count();
const ShapeItem ec_B = B.element_count();

   if (ec_A == 0 )   LENGTH_ERROR;
   if (ec_B == 0 )   LENGTH_ERROR;

   // if the degree of A is less than the degree of B, then the quotient
   // Q←A÷B is 0 and the remainder A∣B is B.
   //
   if (ec_A < ec_B)
      {
        Value_P Z(2, LOC);   // Z is quotient Z1 and remainder Z2
        {
          Value_P Z1(1, LOC);   // q₀ of the polynomial)
          Z1->next_ravel_0();   // scalar 0
          Z1->check_value(LOC);
          Z->next_ravel_Pointer(Z1.get());   // quotient 0
        }

        {
          Value_P Z2 = CLONE(&B, LOC);
          Z->next_ravel_Pointer(Z2.get());         // and remainder B
        }
        Z->check_value(LOC);
        return Z;
      }

   // check if complex is needed and remember the powers of B with
   // non-0 coefficients.
   //
vector<size_t> powers_B;   powers_B.reserve(ec_B);
bool need_complex = false;
   loop(a, ec_A)
       {
          if (A.get_cravel(a).is_complex_cell())
             {
               need_complex = true;
               break;
             }
       }

   loop(b, ec_B)
       {
         const Cell & cell = B.get_cravel(b);
         if (cell.is_near_zero())   continue;
         powers_B.push_back(b);   // remember b
         if (cell.is_complex_cell())   need_complex = true;
       }

const ShapeItem ec_Q = ec_A - ec_B + 1;

Value_P Z(2, LOC);   // Z is quotient Z1 and remainder Z2
   if (need_complex)
      {
        typedef complex<double> Complex;
        vector<Complex> D;   D.reserve(ec_B);              // divisor
        vector<Complex> Q;   Q.reserve(ec_Q);              // quotient
        vector<Complex> R;   R.reserve(ec_A + ec_B - 1);   // remainder
        loop(a, ec_A)
            {
              const Cell & cell = A.get_cravel(ec_A - a - 1);
               R.push_back(Complex(cell.get_real_value(),
                                   cell.get_imag_value()));
            }
        loop(b, ec_B - 1)   R.push_back(0);

        loop(b, ec_B)
            {
              const Cell & cell = B.get_cravel(ec_B - b - 1);
               D.push_back(Complex(cell.get_real_value(),
                                   cell.get_imag_value()));
            }

        loop(q, ec_Q)
            {
              /* 1. divide the leading coefficients and store it in PQ

                    R = B × (R ÷ B) = R × factor.
               */
              const Complex & Rq = R[q];   // dividend
              if (Cell::is_near_zero(Rq.real()) &&
                  Cell::is_near_zero(Rq.imag()))
                 {
                   Q.push_back(0);
                   continue;
                 }

              const Complex factor = Rq / D[0];
              Q.push_back(factor);

              // 2. subtract scaled B from R
              //
              loop(n, powers_B.size())
                  {
                    const size_t b = powers_B[n];
                    R[q + b] -= factor * D[b];
                  }
            }
        Assert(size_t(ec_Q) == Q.size());

        // construct the result
        //
        Value_P Z1(ec_Q, LOC);   // A÷B
        loop(q, ec_Q)
           {
              const Complex z = Q[ec_Q - q - 1];
              const double r = z.real();
              const double i = z.imag();
              if      (!Cell::is_near_zero(i))     Z1->next_ravel_Complex(r, i);
              else if (!Cell::is_near_int64_t(r))  Z1->next_ravel_Number(r);
              else                     Z1->next_ravel_Int(Cell::near_int(r));
           }
        Z1->check_value(LOC);

        Value_P Z2(ec_B - 1, LOC);   // remainder A∣B
        loop(b, ec_B - 1)
            {
              const Complex z = R[ec_Q + b];
              const double re = z.real();
              const double im = z.imag();
              if (!Cell::is_near_zero(im))      Z2->next_ravel_Complex(re, im);
              else if (!Cell::is_near_int64_t(re))   Z2->next_ravel_Number(re);
              else                      Z2->next_ravel_Int(Cell::near_int(re));
            }
        Z2->check_value(LOC);

        Z->next_ravel_Value(Z1.get());
         Z->next_ravel_Value(Z2.get());
      }
   else   // real coefficients
      {
        vector<double> D;   D.reserve(ec_B);   // divisor
        vector<double> Q;   Q.reserve(ec_Q);   // quotient
        vector<double> R;   R.reserve(ec_A);   // remainder
        loop(a, ec_A)
            {
              const Cell & cell = A.get_cravel(ec_A - a - 1);
               R.push_back(cell.get_real_value());
            }

        loop(b, ec_B)
            {
              const Cell & cell = B.get_cravel(ec_B - b - 1);
               D.push_back(cell.get_real_value());
            }

        loop(q, ec_Q)
            {
              /* 1. divide the leading coefficients and store it in PQ

                    R = B × (R ÷ B) = R × factor.
               */
              const double Rq = R[q];   // dividend
              if (Cell::is_near_zero(Rq))
                 {
                   Q.push_back(0);
                   continue;
                 }

              const double factor = Rq / D[0];
              Q.push_back(factor);

              // 2. subtract scaled B from R
              //
              loop(n, powers_B.size())
                  {
                    const size_t b = powers_B[n];
                    R[q + b] -= factor * D[b];
                  }
            }
        Assert(size_t(ec_Q) == Q.size());

        // construct the result
        //
        Value_P Z1(ec_Q, LOC);   // A÷B
        loop(q, ec_Q)
           {
              const double r = Q[ec_Q - q - 1];
              if (!Cell::is_near_int64_t(r))   Z1->next_ravel_Number(r);
              else                     Z1->next_ravel_Int(Cell::near_int(r));
           }
        Z1->check_value(LOC);

        Value_P Z2(ec_B - 1, LOC);   // remainder A∣B

        loop(b, ec_B - 1)
            {
              const double re = R[ec_Q + b];
              if (!Cell::is_near_int64_t(re))   Z2->next_ravel_Number(re);
              else                 Z2->next_ravel_Int(Cell::near_int(re));
            }
        Z2->check_value(LOC);

        Z->next_ravel_Value(Z1.get());
         Z->next_ravel_Value(Z2.get());
      }

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Bif_F12_DOMINO::poly_quotient_NO(const Value & A, const Value & B,
                              const Value * order_A, const Value * order_B)
{
   // divide multivariate polynomials.
   //
   // the rank of a polynomial is the number of indeterminants.
   // They must be the same for A and B
   //
const int rank = A.get_rank();
   if (rank != sRank(B.get_rank()))
      {
        const char * sub_fun = order_A ? "poly_divideNO" : "poly_divideN";
        MORE_ERROR() << "A ⌹." << sub_fun << " B: ⍴⍴A="
                     << rank << " (= number of indeterminants in A; ⍴⍴B="
                     << B.get_rank() << " (= number of\n"
                        "    indeterminants in B. Omitted indeterminants "
                        "count.";
        RANK_ERROR;
      }

   if (A.element_count() == 0 )   LENGTH_ERROR;
   if (B.element_count() == 0 )   LENGTH_ERROR;

Polynomial R(A);   // remainder
Polynomial D(B);   // divisor
Polynomial Q;      // quotient
Polynomial T;      // temp remainder

   if (R.needs_complex())   TODO;
   if (D.needs_complex())   TODO;

const Monomial term_R_max = R.get_LT(order_A);
const Monomial term_D_max = D.extract_LT(order_B);
   if (order_A)
      {
        Assert(order_B);
        if (term_R_max.get_order(*order_A) < term_D_max.get_order(*order_B))
           {
             // R is already smaller than D: return it.
             //
             return A.clone(LOC);
           }
      }
   else   // default (lexicographic) order
      {
        Assert(!order_B);
        if (term_R_max < term_D_max)
           {
             // R is already smaller than D: return it.
             //
             return A.clone(LOC);
           }
      }

   while (R.size())
         {
           // 1. pick a large coefficient in R
           //
           const Monomial term_R = R.extract_LT(order_A);
           if (!term_R.is_multiple_of(term_D_max))
              {
                // if term_R is not a multiple of term_D then we move it
                // from the current remainder to the final remainder.
                //
                T.push_back(term_R);
                continue;
              }

           const Monomial factor = term_R / term_D_max;
           Q.push_back(factor);

           // remove multiples of B from R
           //
           loop(d, D.size())
               {
                 const Monomial scaled_D = factor * D[d];
                 R.subtract_term(scaled_D);
               }
            }

        // construct the result
        //
Value_P Z1 = Q.to_value();
Value_P Z2 = T.to_value();
Value_P Z(2, LOC);
   Z->next_ravel_Value(Z1.get());
   Z->next_ravel_Value(Z2.get());
   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Bif_F12_DOMINO::poly_quotient_N(const Value & A, const Value & B)
{
   // A contains 2 "planes": the real polynomial A[1;...] and the item
   // order A[2;...] for the corresponding coefficient in A[1[...].
   //
   if (A.get_rank() <= 2)   RANK_ERROR;
   if (B.get_rank() <= 2)   RANK_ERROR;

   if (A.get_shape_item(0) != 2)
      {
        MORE_ERROR() << "A ⌹.poly_divideNO B: ↑⍴A must be 2 (polynomial "
                        "A[1;...] and\nmonomial order A[2;...])";
        LENGTH_ERROR;
      }

const Shape shape_A = A.get_shape().without_axis(0);
Value_P poly_A(shape_A, LOC);   // A[1;...]
Value_P order_A(shape_A, LOC);  // A[2;...]
   {
     const ShapeItem len = shape_A.get_volume();

     const Cell & order0 = A.get_cravel(len);
     if (!order0.is_near_zero())
        {
          MORE_ERROR() << "A ⌹.poly_divideNO B: the order value ("
                       << order0 << ") of the constant term is ≠ 0.";
          DOMAIN_ERROR;
        }

     const Cell * cell = &A.get_cfirst();

     loop(l, len)   poly_A->next_ravel_Cell(*cell++);
     loop(l, len)   order_A->next_ravel_Cell(*cell++);

     poly_A->check_value(LOC);
     order_A->check_value(LOC);
   }

const Shape shape_B = B.get_shape().without_axis(0);
Value_P poly_B(shape_B, LOC);   // B[1;...]
Value_P order_B(shape_B, LOC);  // B[2;...]
   {
     const ShapeItem len = shape_B.get_volume();

     const Cell & order0 = B.get_cravel(len);
     if (!order0.is_near_zero())
        {
          MORE_ERROR() << "B ⌹.poly_divideNO B: the order value ("
                       << order0 << ") of the constant term is ≠ 0.";
          DOMAIN_ERROR;
        }

     const Cell * cell = &B.get_cfirst();

     loop(l, len)   poly_B->next_ravel_Cell(*cell++);
     loop(l, len)   order_B->next_ravel_Cell(*cell++);

     poly_B->check_value(LOC);
     order_B->check_value(LOC);
   }

   return poly_quotient_NO(*poly_A, *poly_B, order_A.get(), order_B.get());
}
//----------------------------------------------------------------------------
Value_P
Bif_F12_DOMINO::scan_polynomial(const Value & A, const Value & B)
{
   /* A is a vector of names for the indeterminants (typically
      'x', 'x'y, 'x'z, ...) and B is the coefficients of the powers of
      the indeterminants.

      We support the following variants of A and B:

      1. 1-dimensional B and single string A (varname = indeterminant)
      2. N-dimensional B and single string A (N 1-character indeterminants)
      3. N-dimensional B and N strings A (N variable-length indeterminants)
    */
const ShapeItem ec_A = A.element_count();

   if (A.get_rank() > 1)   RANK_ERROR;   // neither name nor vector of names

UCS_string_vector vars;
   if (A.is_char_array())   // case 1. or 2.
      {
        if (B.get_rank() <= 1)   // case 1. (single indeterminant)
           {
             const UCS_string x(A);
             vars.push_back(x);
           }
        else                      // case 2: N 1-character indeterminant
           {
             if (ec_A != B.get_rank())   LENGTH_ERROR;
             loop(a, ec_A)
                 {
                   const UCS_string x(A.get_cravel(a).get_char_value());
                   vars.push_back(x);
                 }
           }
      }
   else                     // case 3.
      {
        // every axis of B is one indeterminant of the polynomial
        //
        if (B.get_rank() > ec_A)   LENGTH_ERROR;
        loop(a, ec_A)
            {
              const Cell & cell = A.get_cravel(a);
              const Value & x = *cell.get_pointer_value();
              if (!x.is_char_array())
                 {
                    MORE_ERROR() << "A ⌹.print_poly B: Bad variable name A["
                                 << a << "]";
                    DOMAIN_ERROR;
                 }
              const UCS_string var(x);
              vars.push_back(var);
            }
      }

   return scan_polynomial(vars, B);
}
//----------------------------------------------------------------------------
Value_P
Bif_F12_DOMINO::scan_polynomial(const Value & B)
{
const uRank rank_B = B.get_rank();
   if (rank_B > 8)   RANK_ERROR;

const char * xyz = "xyzuwvst";
UCS_string_vector vars;   vars.reserve(8);
   loop(v, 8)
       {
         const Unicode var = Unicode(xyz[v]);
         const UCS_string var_ucs(var);
         vars.push_back(var_ucs);
       }

   return scan_polynomial(vars, B);
}
//----------------------------------------------------------------------------
Value_P
Bif_F12_DOMINO::scan_polynomial(const UCS_string_vector & vars, const Value & B)
{
   if (B.get_rank() > 1)     RANK_ERROR;
   if (!B.is_char_array())   DOMAIN_ERROR;

UCS_string text(B);
   text.remove_leading_and_trailing_whitespaces();
Unicode_source src(text);

   // scanner state
   //
char term_sign = '+';
bool got_j = false;
bool got_overbar = false;

vector<Monomial> terms;
Monomial T;
   if (src.has_more() && *src == '-')   { term_sign = '-';   ++src; }
   T.set_real(got_overbar, term_sign, 1.0);

   while (src.has_more())
     {
       switch(const Unicode uni = *src)
          {
             case UNI_SPACE:
                  ++src;
                  continue;

             case '+': 
             case '-': terms.push_back(T);
                       new (&T) Monomial;
                       term_sign = uni;
                       T.set_real(got_overbar, term_sign, 1.0);
                        ++src;
                       continue;

             case 'J': 
             case 'j': got_j = true;
                       continue;

             case UNI_OVERBAR: got_overbar = true;
                  ++src;
                  continue;

             case '0'...'9':
                       T.scan_coefficient(src, term_sign, got_j, got_overbar);
                       continue;

             default: T.scan_indeterminants(vars, src);
          }
     }
   terms.push_back(T); 

   // check for duplicate terms
   //
   loop(t1, terms.size() - 1)
   for (size_t t2 = t1 + 1; t2 < terms.size(); ++t2)
       {
         if (!(terms[t1].same_expos(terms[t2])))   continue;   // OK

         const Monomial & term = terms[t1];

         UCS_string & more = MORE_ERROR();
         more << "⌹[11] B: duplicate term: ";
         loop(var, term.expos.size())
             {
               if (const int expo = term.expos[var])
                  {
                    more << vars[var];
                    if (expo > 1) more << UCS_string::power(expo);
                  }
             }

         DOMAIN_ERROR;
       }

   // construct the result.
   //

   // A. figure the largestindefinite
   //
vector<int> max_powers(8, -1);
   loop(t, terms.size())
       {
         const Monomial & term = terms[t];
         loop (i, term.expos.size())
             {
               const int expo = term.expos[i];
               max_powers[i] = max(max_powers[i], expo);
             }
       }

   // figure the largest indeterminant present in the terms.
   // This largest indeterminant determines the rabk of Z.
   //
int max_var = -1;
   loop(r, 8)   if (max_powers[r] != -1)   max_var = r;

   // construct the shape of Z. Each max_powers[r] determines the corresponding
   // axis length of Z (+1 for the constant term).
   //
Shape shape_Z;
   loop(r, max_var + 1)   shape_Z.add_shape_item(max_powers[r] + 1);

Value_P Z(shape_Z, LOC);
   loop(e, Z->element_count())   Z->next_ravel_0();

   // fill in the terms
   //
   loop(t, terms.size())   // loop over terms
       {
         const Monomial & term = terms[t];
         Cell * dest = &Z->get_wfirst();
         // CERR << "\nTERM " << t << ":" << endl;

         // compute the position of the term in Z
         //
         if (term.expos.size())   // no, not constant
            {
              Shape index;
              loop(r, shape_Z.get_rank())   index.add_shape_item(0);

              loop (v, term.expos.size())
                  {
                    const int expo = term.expos[v];
                    index.set_shape_item(v, expo);
                  }

              dest += shape_Z.ravel_pos(index);
            }

         if (term.get_imag() != 0)                      // complex coefficient
            {
              new (dest) ComplexCell(term.get_real(), term.get_imag());
            }
         else if (Cell::is_near_int(term.get_real()))   // integer coefficient
            {
              new (dest) IntCell(Cell::near_int(term.get_real()));
            }
         else                                         // real coefficient
            {
              new (&dest) FloatCell(term.get_real());
            }
       }

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
#if 0 // not used
void
Bif_F12_DOMINO::run_script(const UTF8_string & script,
                           const UTF8_string & args,
                           UCS_string_vector & result)
{
   // bin_path is where the GNU APL binary gets installed. We expect script
   // to live in the same directory (or in ./ during development). If both
   // exist then we use ./script (so we can develop it without running
   // make install after every change of the script..
   //
UTF8_string script_path("./");
   script_path << script;
   if (access(script_path.c_str(), R_OK))   // no ./script
      {
          const char * bin_path = LibPaths::get_APL_bin_path();
          script_path = UTF8_string(bin_path);
          script_path += '/';
          script_path << script;
          if (access(script_path.c_str(), R_OK))   // no bin_path/script
             {
               MORE_ERROR() << "Installation problem: No readable file '"
                            << script << "' (in directory . or "
                            << bin_path << ")";
               DOMAIN_ERROR;
             }
      }

   // construct command
   //
const char * pythons[] = { "python3", "python" };
PipeReader reader;
   loop(p, sizeof(pythons) / sizeof(*pythons))
       {
         UTF8_string cmd(pythons[p]);
         cmd << " " << script_path << " " << args;
         new (&reader) PipeReader(cmd.c_str());
         if (+reader)   break;   // success opening cmd
      }

   if (!reader)
      {
        MORE_ERROR() << "A ⌹.poly_quotient2 B: Could not start python.";
        DOMAIN_ERROR;
      }

enum { BUFSIZE = 80 };
char buffer[BUFSIZE + 1];
   for (;;)
       {
           const char * s = reader.fgets(buffer, BUFSIZE);
           if (s == 0)   break;
           buffer[BUFSIZE] = 0;   // to secure strlen()
           size_t len = strlen(buffer);
           if (len && buffer[len - 1] == '\n')   // discard trailing \n
              buffer[--len] = 0;

           if (len && buffer[len - 1] == '\r')   // discard trailing \r
              buffer[--len] = 0;

           if (len == 0)   continue;   // if nothing left

           // COUT << "──── " << buffer << " ────" << endl;
           try   // proposed by Claude Code
              {
                const UTF8_string buffer_utf(buffer);
                const UCS_string buffer_ucs(buffer_utf);
                result.push_back(buffer_ucs);
              }
           catch (std::bad_alloc &) { WS_FULL; }
           catch (...)              { FIXME; }
       }
}
#endif
//----------------------------------------------------------------------------
Value_P
Bif_F12_DOMINO::integral(const Value * A, const Value & B)
{
int printer = 1;
   if (A)   // optional attributes
      {
        if (A->get_rank() > 1)        RANK_ERROR;
        if (A->element_count() > 1)   LENGTH_ERROR;
        printer = A->get_cravel(0).get_int_value();
      }

   // B shall be a character string with the expression to be integrated
   //
   if (!B.is_char_string())
      {
        if (B.get_rank() > 1)        RANK_ERROR;
        DOMAIN_ERROR;
      }

const char * script = "integral.py";
PythonPipe p(script);
   {
     // a successfully started python script should print the following
     // messages when started.
     //
     const char * start_up[] = { "<→WRAPPER-STARTED→>.",
                                 "<→MODULES-IMPORTED→>."
                               };
     loop(m, sizeof(start_up) / sizeof(const char *))
         {
           const char * expected = start_up[m];
           const UCS_string message = p.read();
           if (message != expected)
              {
                MORE_ERROR() << "Error starting python script '" << script<< "'.\n"
                                "Expected '" << expected <<
                                "' but got '" << message << "'.";
                DOMAIN_ERROR;
              }
         }
   }

   {
     char cc[10];   SPRINTF(cc, "%d", printer);
     p.write(cc);
   }

   UCS_string BB(B);
   p.write(UCS_string(B));
   const UTF8_string DONE_utf("<→DONE→>.");
   const UCS_string DONE(DONE_utf);

UCS_string_vector result;
   for (;;)
       {
         const UTF8_string response_utf(p.read());
         const UCS_string response(response_utf);

         // the "normal" end of the python script is <!DONE!>. followed by
         // the script name.
         if (response.starts_with(DONE))   break;

         result.push_back(response);

         // the "abnormal" end of the python script, e.g. when a stack traces id
         // printed, is <!END-OF-FILE!>. This message comes from PythonPipe::read()
         // and NOT from the oython script.
         //
         if (response == "<!END-OF-FILE!>.")   break;
       }

   // remove a trailing empty line (if any)
   //
   if (result.size() > 1 && result.back().size() == 0)
      {
         result.pop_back();
      }

const ShapeItem rows = result.size();
ShapeItem cols = 0;
   loop(z, rows)
      {
        cols = max(size_t(cols), result[z].size());
      }

Shape shape_Z;
   if (rows > 1)   shape_Z.add_shape_item(rows);
   shape_Z.add_shape_item(cols);

Value_P Z(shape_Z, LOC);
   loop(r, rows)
      {
        const ShapeItem len = result[r].size();
        loop(c, len)         Z->next_ravel_Char(result[r][c]);
        loop(c, cols - len)   Z->next_ravel_Char(UNI_SPACE);
      }

   Z->check_value(LOC);
   return Z;
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

