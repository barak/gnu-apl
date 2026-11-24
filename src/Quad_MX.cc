/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2024 Henrik Moller

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


/***

NOTES

check ident for real scalar
other sorts of rotation
https://en.wikipedia.org/wiki/Rotation_matrix

modded files:
Quad_MX.cc
Quad_MX.hh
Quad_MX.def
doc/apl.texi

 ***/

#include "PointerCell.hh"
#include "Quad_FIO.hh"
#include "Quad_MX.hh"
#include "Workspace.hh"
#include "APL_types.hh"
#include "Shape.hh"
#include "Sys.hh"
#include "Value.hh"

Quad_MX Quad_MX::fun;

#include<cmath>
#include<math.h>
#include<complex>
#include<vector>

#if apl_GSL

#include <gsl/gsl_statistics.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_eigen.h>
#include <gsl/gsl_errno.h>

//════════════════════════════════════════════════════════════════════════════
const FunctionGroup::function_info Quad_MX::subfunction_infos[] =
{
#define mx_def(axis, sub_name, comm_2, valence, enum) \
   { axis, #sub_name, "", comm_2, valence },
#include "Quad_MX.def"
};
//════════════════════════════════════════════════════════════════════════════
Quad_MX::Quad_MX() : QuadFunction(TOK_Quad_MX)
{
enum { count = sizeof(subfunction_infos) / sizeof(*subfunction_infos) };
   init_function_group(subfunction_infos, count, "⎕MX");
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_MX::print_fun_syntax(ostream & out,
                          const FunctionGroup::function_info & info) const
{
  out << "    ";
  switch(info.valence)
     {
       case 1:  out << "    ";   break;
       case 2:  out << "  A ";   break;
       case 3:  out << "{A} ";   break;
       default: FIXME;
     }
  out << "⎕MX[" << setw(3) << info.axis << "]    ⍝ ";
  switch(info.valence)
     {
       case 1:  out << "(monadic) ";   break;
       case 2:  out << "(dyadic)  ";   break;
       case 3:  out << "(nomadic) ";   break;
       default: FIXME;
     }
      
  out << info.comment_fun << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_MX::print_map_syntax(ostream & out,
                          const FunctionGroup::function_info & info) const
{
const char * name = info.function_name;
const UCS_string blanks(max_function_name_length - strlen(name), UNI_SPACE);

   out << "    ⎕MX[" << setw(3) << info.axis << "]  ←→  ⎕MX['" << name << "']"
       << blanks << "  ←→  ⎕MX." << name << endl;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_MX::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
MX_ops op    = OP_LIST;
int modifier = 0;

   if (X.is_int_scalar() || X.is_char_string())   // op only
      {
        op = to_MX_op(value_to_subfun(X));
      }
   else if (X.is_vector())                         // op and modifier
      {
        // at most op and modifier, both (near-) integers (Bugs38 #127)
        const ShapeItem X_count = X.element_count();
        if (X_count > 2)   LENGTH_ERROR;
        // no narrowing to int (Bugs39 #68)
        const APL_Integer x0 = X.get_near_int(0);
        const APL_Integer x1 = X_count > 1 ? X.get_near_int(1) : 0;
        if (x0 != int(x0))   bad_subfun_number_ERROR(x0);   // Bugs40 #63
        if (x1 != int(x1))
           {
             MORE_ERROR() << "⎕MX[X] B: bad modifier X[2] (= " << x1 << ")";
             DOMAIN_ERROR;
           }
        op = to_MX_op(x0);
        modifier = x1;
      }
   else
      {
        MORE_ERROR() << "A ⎕MX[X] B: integer or string X expected.";
        DOMAIN_ERROR;
      }
   remap_randoms_subfun(op, modifier);

   // not subfun_count, which is not the largest subfunction (Bugs38 #127)
   if (op < OP_LIST || !get_info_by_axis(op))   bad_subfun_number_ERROR(op);

Value_P A_vp(static_cast<Value *>(const_cast<cValue *>(&A)), LOC);
Value_P B_vp(static_cast<Value *>(const_cast<cValue *>(&B)), LOC);
Value_P Z;
  switch(op)
    {
      case OP_LIST:
           if (B.is_str0())    return list_functions(CERR);
           if (B.is_zilde())   return list_mappings(CERR);
           DOMAIN_ERROR;
           break;

      case OP_CROSS_PRODUCT:      Z = dyadicCrossProduct(A_vp, B_vp);   break;
      case OP_VECTOR_ANGLE:       Z = vectorAngle(A_vp, B_vp);          break;
      case OP_HOMOGENEOUS_MATRIX: Z = dyadicRotation(16, A_vp, B_vp);   break;
      case OP_COVARIANCE:         Z = dyadicCovariance(A_vp, B_vp, false);
                                  break;
      case OP_CORRELATION:        Z = dyadicCovariance(A_vp, B_vp, true);
                                  break;
      case OP_HISTOGRAM:          Z = histogram(A_vp, B_vp);            break;
      case OP_RANDOMS:            Z = randoms(A_vp, B_vp, modifier);    break;
      case OP_PRINT:              Z = printit(A_vp, B_vp);              break;

      default: MORE_ERROR() << "⎕MX[" << op << "] B is monadic.";
               VALENCE_ERROR;
    }

  Z->check_value(LOC);
  Z->check_finite("A ⎕MX[X] B");   // Bugs39 #31
  real_cells(*Z);          // Bugs39 #100
  return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_MX::eval_XB(cValue_R X, cValue_R B) const
{
MX_ops op    = OP_LIST;
int modifier = 0;

  if (X.is_int_scalar() || X.is_char_string())   // op only
     {
       op = to_MX_op(value_to_subfun(X));
     }
  else if (X.is_vector())                         // op and modifier
     {
       // at most op and modifier, both (near-) integers (Bugs38 #127)
       const ShapeItem X_count = X.element_count();
        if (X_count > 2)   LENGTH_ERROR;
        // no narrowing to int (Bugs39 #68)
        const APL_Integer x0 = X.get_near_int(0);
        const APL_Integer x1 = X_count > 1 ? X.get_near_int(1) : 0;
        if (x0 != int(x0))   bad_subfun_number_ERROR(x0);   // Bugs40 #63
        if (x1 != int(x1))
           {
             MORE_ERROR() << "⎕MX[X] B: bad modifier X[2] (= " << x1 << ")";
             DOMAIN_ERROR;
           }
        op = to_MX_op(x0);
        modifier = x1;
      }
   else
      {
        MORE_ERROR() << "⎕MX[X] B: integer or string X expected.";
        DOMAIN_ERROR;   // like A ⎕MX[X] B (Bugs38 #127)
      }
   remap_randoms_subfun(op, modifier);

   // not subfun_count, which is not the largest subfunction (Bugs38 #127)
   if (op < OP_LIST || !get_info_by_axis(op))   bad_subfun_number_ERROR(op);

Value_P opt_A;   // = 0
Value_P B_vp(static_cast<Value *>(const_cast<cValue *>(&B)), LOC);
Value_P Z = Idx0_0(LOC);
   switch(op)
      {
        case OP_LIST:
             if (B.is_str0())   return  list_functions(COUT);
             if (B.is_zilde())   return list_mappings(COUT);
             DOMAIN_ERROR;
       
        case OP_CROSS_PRODUCT:      Z = monadicCrossProduct(B_vp);          break;
             //v←v,⍉1 100⍴100 ⎕mx[12] 100000 ⎕mx[10 2] 1
        case OP_RANDOMS:            Z = randoms(opt_A, B_vp, modifier);     break;
        case OP_DETERMINANT:        Z = determinant(B_vp);                  break;
        case OP_COVARIANCE:         Z = monadicCovariance(B_vp, false);     break;
        case OP_CORRELATION:        Z = monadicCovariance(B_vp, true);      break;
        case OP_NORM:               Z = norm(B_vp);                         break;
        case OP_EIGENVECTORS:       Z = eigenvectors(B_vp);                 break;
        case OP_EIGENVALUES:        Z = eigenvalues(B_vp);                  break;
        case OP_IDENT:              Z = ident(B_vp);                        break;
        case OP_ROTATION_MATRIX:    Z = monadicRotation(B_vp);              break;
        case OP_SET_RNG_SEED:       Z = set_rng_seed(B_vp);                 break;
        default:   MORE_ERROR() << "A ⎕MX[" << op << "] B is dyadic.";
                   VALENCE_ERROR;
      }

  Z->check_value(LOC);
  Z->check_finite("⎕MX[X] B");   // Bugs39 #31
  real_cells(*Z);          // Bugs39 #100
  return Token(TOK_APL_VALUE1, Z);
}

//────────────────────────────────────────────────────────────────────────────
Token
Quad_MX::eval_AB(cValue_R A, cValue_R B) const
{
  return eval_B(B);   // show help
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_MX::eval_B(cValue_R B) const
{
   if (B.is_str0())    return list_functions(CERR);
   if (B.is_zilde())   return list_mappings(CERR);

   if (!B.is_vector())      RANK_ERROR;
   if (B.element_count())   LENGTH_ERROR;   // not empty
  DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
vector<Quad_MX::Dcomplex>
Quad_MX::getCross(GSL_Matrix *mtx)
{
   // The generalised cross product of n-1 vectors in R^n expands a
   // formal n×n determinant along its first (formal-basis) row. genMtx()
   // pads its caller's n-1 given vectors with exactly one extra row of
   // 1s, so mtx here is square (n×n) *provided* the caller actually
   // supplied exactly n-1 vectors -- which callers now enforce (see
   // monadicCrossProduct()'s tightened precondition), fixing Bugs18 #10/
   // the earlier M7 attempt at its root instead of trying to recover a
   // square matrix here after the fact (that approach -- an extra
   // column-drop in this function -- double-shrinks an already-correctly
   // -sized mtx and corrupts genCofactor()'s cofactor buffer; tried and
   // reverted while fixing this).
   //
   // rc is sized by columns, not rows, so the loop below must be too:
   // genCofactor(mtx,0,k) removes row 0 and column k, i.e. k indexes a
   // *column*, and for square input mtx->rows()==mtx->cols() anyway.
   //
vector<Dcomplex> rc(mtx->cols());
   loop(k, mtx->cols())
       {
         const int sign = k & 1 ? -1 : 1;
         GSL_Matrix * cf = genCofactor(mtx, 0, k);
         const Dcomplex id = getDet(cf);
         delete cf;
         rc[k] = sign > 0 ? id : -id;
       }

  return rc;
}
//────────────────────────────────────────────────────────────────────────────
Quad_MX::GSL_Matrix *
Quad_MX::genCofactor(GSL_Matrix *mtx, int r, int c)
{
GSL_Matrix * cf = new GSL_Matrix(mtx->rows() - 1, mtx->cols() - 1);

int rx = 0;
   loop(rr, mtx->rows())
       {
         if (rr != r)
            {
              int cx = 0;
              loop(cc, mtx->cols())
                {
                  if (cc != c)   cf->set_val(rx, cx++, mtx->val(rr, cc));
                }
              rx++;
            }
       }

  return cf;
}
//────────────────────────────────────────────────────────────────────────────
/// return z × 2⋆-e and add e to expo, where e is the exponent of the larger
/// component of z (so that it cannot overflow like |z| can), Bugs40 #18
static Quad_MX::Dcomplex
scale_down(const Quad_MX::Dcomplex & z, long & expo)
{
const double m = max(fabs(z.real()), fabs(z.imag()));
   if (m == 0.0 || !isfinite(m))   return z;

int e = 0;
   frexp(m, &e);
   expo += e;
   return Quad_MX::Dcomplex(ldexp(z.real(), -e), ldexp(z.imag(), -e));
}
//────────────────────────────────────────────────────────────────────────────
Quad_MX::Dcomplex
Quad_MX::getDet(GSL_Matrix * mtx)
{
const int n = mtx->rows();

   // the determinant of a 0×0 matrix is the empty product 1. It occurs as
   // the cofactor in ⎕MX[2] 0 1⍴0 (Bugs39 #1).
   //
   if (n == 0)   return Dcomplex(1.0, 0.0);
   if (n == 1)   return mtx->val(0, 0);

   // If all items are (Gaussian) integers, then use Bareiss's fraction-free
   // elimination: every minor that it computes is an integer, so the
   // result is exact (Bugs38 #15) as long as the minors (bounded by
   // Hadamard's bound, the product of the row norms) are below 2⋆53. If
   // an intermediate result overflows, then fall back to the Gaussian
   // elimination below.
   //
   // Otherwise use Gaussian elimination with partial pivoting and multiply
   // the pivots with separate exponents, so that no intermediate product
   // overflows or underflows unless the determinant itself does (Bareiss
   // computed products like 1E160×1E160 and returned ∞J-nan- for a
   // determinant of 1E20, Bugs39 #25).
   //
std::vector<Dcomplex> a(size_t(n) * n);
bool integral = true;
double log2_bound = 0.0;
   loop(r, n)
      {
        double row_max = 0.0;   // scaled row norm (avoid overflow)
        loop(c, n)
           {
             const Dcomplex v = mtx->val(r, c);
             a[r*n + c] = v;
             if (integral && (v.real() != nearbyint(v.real()) ||
                              v.imag() != nearbyint(v.imag())))
                integral = false;
             const double av = abs(v);
             if (row_max < av)   row_max = av;
           }
        if (row_max == 0.0)   return Dcomplex(0.0, 0.0);   // zero row

        double sum = 0.0;
        loop(c, n)   { const double q = abs(a[r*n + c]) / row_max;
                       sum += q*q; }
        log2_bound += log2(row_max) + 0.5*log2(sum);
      }

bool negate = false;
   if (integral && log2_bound < 1000.0)   // Bareiss (exact)
      {
        const std::vector<Dcomplex> a0 = a;   // for the fallback
        Dcomplex prev(1.0, 0.0);
        for (int k = 0; k < n - 1; ++k)
            {
              int piv = k;
              while (piv < n && a[piv*n + k] == Dcomplex(0.0, 0.0))   ++piv;
              if (piv == n)   return Dcomplex(0.0, 0.0);
              if (piv != k)
                 {
                   loop(c, n)   std::swap(a[k*n + c], a[piv*n + c]);
                   negate = !negate;
                 }

              const Dcomplex akk = a[k*n + k];
              for (int r = k + 1; r < n; ++r)
                  {
                    const Dcomplex ark = a[r*n + k];
                    for (int c = k + 1; c < n; ++c)
                        a[r*n + c] = (a[r*n + c] * akk - ark * a[k*n + c])
                                   / prev;
                  }
              prev = akk;
            }

        const Dcomplex det = a[(n - 1)*n + (n - 1)];
        if (isfinite(det.real()) && isfinite(det.imag()))
           return negate ? -det : det;

        a = a0;   // overflow: fall back to Gaussian elimination
        negate = false;
      }

   // Gaussian elimination: det = ± (mant × 2⋆expo)
   //
Dcomplex mant(1.0, 0.0);
long expo = 0;

   // scale every row by a power of 2 so that its largest entry is near 1
   // (exactly, adding the exponent to expo): an elimination update of
   // unscaled rows could overflow although the determinant does not, e.g.
   // ⎕MX[1] 3 3⍴1 0 1E308 ¯1 1 1E308 0 1E¯300 0 (Bugs41 #83)
   //
   loop(r, n)
      {
        double row_max = 0.0;
        loop(c, n)
           {
             const Dcomplex & x = a[r*n + c];
             row_max = max(row_max, max(fabs(x.real()), fabs(x.imag())));
           }
        if (row_max == 0.0 || !isfinite(row_max))   continue;
        int e;
        frexp(row_max, &e);
        loop(c, n)
           {
             Dcomplex & x = a[r*n + c];
             x = Dcomplex(ldexp(x.real(), -e), ldexp(x.imag(), -e));
           }
        expo += e;
      }

   for (int k = 0; k < n; ++k)
       {
         // pivot: the largest |a[r;k]| in column k, rows k...
         int piv = k;
         for (int r = k + 1; r < n; ++r)
             if (abs(a[r*n + k]) > abs(a[piv*n + k]))   piv = r;
         if (a[piv*n + k] == Dcomplex(0.0, 0.0))   return Dcomplex(0.0, 0.0);
         if (piv != k)
            {
              loop(c, n)   std::swap(a[k*n + c], a[piv*n + c]);
              negate = !negate;
            }

         const Dcomplex akk = a[k*n + k];
         for (int r = k + 1; r < n; ++r)
             {
               const Dcomplex f = a[r*n + k] / akk;
               if (f == Dcomplex(0.0, 0.0))   continue;
               for (int c = k + 1; c < n; ++c)   a[r*n + c] -= f * a[k*n + c];
             }

         // mant ← mant × akk, keeping |mant| near 1. The exponent is taken
         // from the larger component (|akk| itself can overflow), and the
         // components are scaled with ldexp(): for |akk| ≥ 2⋆1023,
         // ldexp(1.0, 1024) is ∞, so akk ÷ ldexp(1.0, e) was 0 (Bugs40 #18)
         //
         mant *= scale_down(akk, expo);
         mant  = scale_down(mant, expo);
       }

   if (expo >  2000)   expo =  2000;   // ∞ (ldexp() would overflow anyway)
   if (expo < -2000)   expo = -2000;   // 0
const Dcomplex det(ldexp(mant.real(), int(expo)), ldexp(mant.imag(), int(expo)));
   return negate ? -det : det;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_MX::unit_vector(vector<Dcomplex> & v)
{
double max_abs = 0.0;   // max. of |Re v[i]| and |Im v[i]|
   loop(i, v.size())
       {
         if (max_abs < fabs(v[i].real()))   max_abs = fabs(v[i].real());
         if (max_abs < fabs(v[i].imag()))   max_abs = fabs(v[i].imag());
       }
   if (max_abs == 0.0 || !isfinite(max_abs))   return false;

double len = 0.0;   // the length of v ÷ max_abs (1 ≤ len ≤ √(2×⍴v))
   loop(i, v.size())   { v[i] /= max_abs;   len += std::norm(v[i]); }
   len = sqrt(len);
   loop(i, v.size())   v[i] /= len;
   return true;
}
//────────────────────────────────────────────────────────────────────────────
Quad_MX::Dcomplex
Quad_MX::magnitude(const vector<Dcomplex> &v)
{
Dcomplex rc(0.0, 0.0);
   // v[i]*v[i] is the complex SQUARE, not |v[i]|^2 -- for a complex
   // vector that's mathematically wrong (can be negative-real or fully
   // complex) and can even sum to exactly 0 for a non-zero vector
   // (e.g. 1J1 and 1J¯1 square to 2i and -2i). Use v[i]*conj(v[i])
   // instead, which is always the correct non-negative real |v[i]|^2.
   loop(i, v.size())   rc += v[i] * conj(v[i]);
   return sqrt(rc);
}
//────────────────────────────────────────────────────────────────────────────
Quad_MX::GSL_Matrix *
Quad_MX::genMtx(Value_P B, bool padded)
{
const ShapeItem cols = B->get_cols();
const ShapeItem rows = B->get_rows() + (padded ? 1 : 0);
GSL_Matrix * mtx = new GSL_Matrix(rows, cols);
ShapeItem b = 0;
   loop(r, rows)
   loop(c, cols)
       {
         if (padded && r == 0)
            {
              mtx->set_val(0, c, complex<double>(1.0, 0.0));
            }
         else
            {
              Cell cache;
              const Cell & Bb = B->get_cravel(b++, cache);
              const APL_Float Bbr = Bb.get_real_value();
              const APL_Float Bbi = Bb.get_imag_value();
              mtx->set_val(r, c, complex<double>(Bbr, Bbi));
            }
       }

  return mtx;
}
// ============= end of utility fcns ===============

//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::eigenvectors(Value_P B)
{
   if (B->get_rank() != 2)
      {
        MORE_ERROR() << "⎕MX.eigenvector B: matrix B expected.";
        RANK_ERROR;
      }

   // GSL's gsl_eigen_nonsymmv() is real-only: a complex B lost its imaginary
   // part (Bugs38 #61)
   //
   loop(b, B->element_count())
      {
        Cell cache;
        const Cell & cell = B->get_cravel(b, cache);
        if (cell.is_numeric() && cell.get_imag_value() != 0.0)
           {
             MORE_ERROR() << "⎕MX.eigenvector B: real matrix B expected.";
             DOMAIN_ERROR;
           }
      }

const ShapeItem rows = B->get_rows();
const ShapeItem cols = B->get_cols();
    
   if (rows != cols)
      {
        MORE_ERROR() << "⎕MX.eigenvector B: square matrix B expected.";
        LENGTH_ERROR;
      }

   if (rows == 0)
      {
        MORE_ERROR() << "⎕MX.eigenvector B: non-empty matrix B expected.";
        LENGTH_ERROR;
      }

   // constructed before any of the GSL allocations below so that a
   // WS_FULL thrown here cannot leak mtx/eval/evec (filled in below,
   // once erc == GSL_SUCCESS is confirmed).
   //
Value_P Z(rows, cols, LOC);

GSL_Matrix * mtx = genMtx(B, false);
std::vector<double> data_v((size_t)mtx->rows() * mtx->cols());
double * data = data_v.data();
int i = 0;
   loop(j, mtx->rows())
   for (int k = 0; k < mtx->cols(); k++, i++)
       {
         data[i] = mtx->val(j, k).real();
       }

gsl_matrix_view m = gsl_matrix_view_array(data, mtx->rows(), mtx->cols());

gsl_vector_complex * eval = gsl_vector_complex_alloc(mtx->rows());
gsl_matrix_complex * evec = gsl_matrix_complex_alloc(mtx->rows(), mtx->cols());

   // GSL's default error handler calls abort(), and a throwing handler
   // (see GSL::set_GSL_error_handler()) would leak eval/evec/w: switch the
   // handler off and check erc instead (Bugs38 #14).
   //
gsl_error_handler_t * old_handler = gsl_set_error_handler_off();
gsl_eigen_nonsymmv_workspace * w = gsl_eigen_nonsymmv_alloc(mtx->rows());
int erc = gsl_eigen_nonsymmv(&m.matrix, eval, evec, w);
   gsl_eigen_nonsymmv_free(w);
   if (erc == GSL_SUCCESS)
      {
        erc = gsl_eigen_nonsymmv_sort(eval, evec, GSL_EIGEN_SORT_ABS_DESC);
      }
   gsl_set_error_handler(old_handler);

   if (erc != GSL_SUCCESS)
     {
       delete mtx;
       gsl_vector_complex_free(eval);
       gsl_matrix_complex_free(evec);
       MORE_ERROR() << "⎕MX[4] (eigenvectors): GSL error: "
                    << gsl_strerror(erc);
       DOMAIN_ERROR;
     }

   loop(c, mtx->cols())
       {
         gsl_vector_complex_view evec_c = gsl_matrix_complex_column(evec, c);
         loop(r, mtx->rows())
             {
                gsl_complex z = gsl_vector_complex_get(&evec_c.vector, r);
                Z->next_ravel_Complex(GSL_REAL(z), GSL_IMAG (z));
             }
       }
  delete mtx;
  gsl_vector_complex_free(eval);
  gsl_matrix_complex_free(evec);

  return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Quad_MX::printit(Value_P filename_A, Value_P B)
{
const CellType B_celltype = B->deep_cell_types();

   if (B->is_char_string())
      {
        FileWriter writer;
        bool close_A = false;
        FILE * ofile = open_file(*filename_A, close_A);
        if (close_A)   new (&writer) FileWriter(ofile);
        const UCS_string B_ucs = B->get_UCS_ravel();
        const UTF8_string B_utf(B_ucs);
        const char * B_ccp = B_utf.c_str();
 
        fprintf(ofile, "%s\n", B_ccp);
        return Idx0_0(LOC);
      }

   // all items numeric, before anything is written (Bugs38 #127)
   if (!(B_celltype & CT_NUMERIC) || (B_celltype & ~CT_NUMERIC))
      {
        MORE_ERROR() << "A ⎕MX.print B: B is not numeric.";
        DOMAIN_ERROR;
      }

const ShapeItem B_count = B->element_count();
const uRank     B_rank  = B->get_rank();

bool close_A = false;
FILE * ofile = open_file(*filename_A, close_A);
FileWriter writer;
   if (close_A)   new (&writer) FileWriter(ofile);

const RavelType rt = B->get_ravel_type();
   if (B_rank <= 1)   // scalar or vector
      {
        if (rt == RPT_CELLS)
           { loop(b, B_count)
                 {
                   Cell cache;
                   const Cell & Bb = B->get_cravel(b, cache);
                   const APL_Float Bbr = Bb.get_real_value();
                   if (Bb.is_complex_cell())
                      { const APL_Float Bbi = Bb.get_imag_value();
                        fprintf(ofile, "%gj%g ", Bbr, Bbi); }
                   else
                      fprintf(ofile, "%g ", Bbr);
                 }
           }
        else if (rt == RPT_COMPLEX)
           { loop(b, B_count)   fprintf(ofile, "%gj%g ",
                                         B->get_real_value(b),
                                         B->get_imag_value(b)); }
        else   // RPT_integer or RPT_FLOAT64
           { loop(b, B_count)   fprintf(ofile, "%g ", B->get_real_value(b)); }
        fprintf(ofile, "\n");
      }
   else               // matrix or higher rank
      {
        const int end_line = B->get_shape_item(B_rank - 1);
        const int end_grid = end_line * B->get_shape_item(B_rank - 2);
        enum { STR_LEN = 256 };
       char str[STR_LEN];
       bool is_cpx = false;
       int max_len = -1;
        if (rt == RPT_CELLS)
           { loop(a, B_count)
                 {
                   int len;
                   Cell cache;
                   const Cell & cell_A = B->get_cravel(a, cache);
                   const APL_Float Aa_real = cell_A.get_real_value();
                   if (cell_A.is_complex_cell())
                      {
                        const APL_Float Aa_imag = cell_A.get_imag_value();
                        len = snprintf(str, STR_LEN, "%gj%g", Aa_real, Aa_imag);
                        if (Aa_imag != 0.0) is_cpx = true;
                      }
                   else
                      len = snprintf(str, STR_LEN, "%g", Aa_real);
                   if (max_len < len) max_len = len;
                 }
           }
        else if (rt == RPT_COMPLEX)
           { loop(a, B_count)
                 {
                   const APL_Float re = B->get_real_value(a);
                   const APL_Float im = B->get_imag_value(a);
                   if (im != 0.0) is_cpx = true;
                   const int len = snprintf(str, STR_LEN, "%gj%g", re, im);
                   if (max_len < len) max_len = len;
                 }
           }
        else   // RPT_integer or RPT_FLOAT64
           { loop(a, B_count)
                 {
                   const int len = snprintf(str, STR_LEN, "%g", B->get_real_value(a));
                   if (max_len < len) max_len = len;
                 }
           }
     
       std::vector<int> rho_v(B_rank, 0);
       int * rho = rho_v.data();
     
       loop(b, B_count)
           {
             if (B_rank > 2 && 0 == b % end_grid)
                {
                  fprintf(ofile, "\n[");
                  loop(j, B_rank - 2)   fprintf(ofile, "%d ", rho[j]);
                  fprintf(ofile, "* *]:\n");
                  bool carry = 1;
                  for (int j = B_rank - 3; j >= 0; j--)
                      {
                        rho[j] += carry;
                        if (rho[j] >= B->get_shape_item(j))
                           {
                             rho[j] = 0;
                             carry = 1;
                           }
                        else
                           {
                             carry = 0;
                           }
                      }
                }
     
             Cell cache;
             const Cell & Av = B->get_cravel(b, cache);
             const APL_Float Avr = Av.get_real_value();
             char str[STR_LEN];
             if (is_cpx && Av.is_complex_cell())
                {
                  const APL_Float Avi = Av.get_imag_value();
                  snprintf(str, STR_LEN, "%gj%g", Avr, Avi);
                }
             else
                {
                  snprintf(str, STR_LEN, "%g", Avr);
                }
             fprintf(ofile, "%*s ", max_len, str);
             if (0 == (b + 1) % end_line)   fprintf(ofile, "\n");
           }
      }

   // ofile is closed by writer's destructor (if close_A); calling
   // fclose(ofile) here as well would double-close the same FILE*.
   return Idx0_0(LOC);
}
//────────────────────────────────────────────────────────────────────────────
FILE *
Quad_MX::open_file(const Value & filename, bool & close_file)
{
   if (filename.is_int_scalar())
      {
        close_file = false;   // leave it open
        return Quad_FIO::get_FILE(filename);
      }

   close_file = true;

   // filename is either "file_name" or ">file_name:", where the leading
   // '>' indicates append (as opposed to overwrite) and shall be skipped.
   //
   if (!filename.is_char_string())
      {
        MORE_ERROR() << "A ⎕MX.print B: bad filename A (string expected)";
        DOMAIN_ERROR;
      }

const UCS_string filename_ucs = filename.get_UCS_ravel();
const UTF8_string filename_utf(filename_ucs);
const char * filename_ccp = filename_utf.c_str();
const char * mode = "w";
   if (strlen(filename_ccp) > 1 && *filename_ccp == '>')
      {
        ++filename_ccp;
        mode = "a";   // append
      }

   if (FILE * fp = fopen(filename_ccp, mode))   return fp;

   MORE_ERROR() << "open " << filename_ucs << " failed: " << strerror(errno);
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::eigenvalues(Value_P B)
{
   if (B->get_rank() != 2) 
      {
        MORE_ERROR() << "⎕MX.eigenvalue B: matrix B expected.";
        RANK_ERROR;
      }

   // GSL's gsl_eigen_nonsymmv() is real-only: a complex B lost its imaginary
   // part (Bugs38 #61)
   //
   loop(b, B->element_count())
      {
        Cell cache;
        const Cell & cell = B->get_cravel(b, cache);
        if (cell.is_numeric() && cell.get_imag_value() != 0.0)
           {
             MORE_ERROR() << "⎕MX.eigenvalue B: real matrix B expected.";
             DOMAIN_ERROR;
           }
      }
    
const ShapeItem rows = B->get_rows();
const ShapeItem cols = B->get_cols();
    
   if (rows != cols)
      {
        MORE_ERROR() << "⎕MX.eigenvalue B: square matrix B expected.";
        LENGTH_ERROR;
      }

   if (rows == 0)
      {
        MORE_ERROR() << "⎕MX.eigenvalue B: non-empty matrix B expected.";
        LENGTH_ERROR;
      }

   // constructed before any of the GSL allocations below so that a
   // WS_FULL thrown here cannot leak mtx/eval/evec (filled in below,
   // once erc == GSL_SUCCESS is confirmed).
   //
Value_P Z(cols, LOC);

GSL_Matrix * mtx = genMtx(B, false);
std::vector<double> data_v((size_t)mtx->rows() * mtx->cols());
double * data = data_v.data();
int i = 0;
   loop(j, mtx->rows())
   loop(k, mtx->cols())
       {
         data[i++] = mtx->val(j, k).real();
       }

gsl_matrix_view m = gsl_matrix_view_array(data, mtx->rows(), mtx->cols());

gsl_vector_complex * eval = gsl_vector_complex_alloc(mtx->rows());
gsl_matrix_complex * evec = gsl_matrix_complex_alloc(mtx->rows(), mtx->cols());

   // GSL's default error handler calls abort(), and a throwing handler
   // (see GSL::set_GSL_error_handler()) would leak eval/evec/w: switch the
   // handler off and check erc instead (Bugs38 #14).
   //
gsl_error_handler_t * old_handler = gsl_set_error_handler_off();
gsl_eigen_nonsymmv_workspace * w = gsl_eigen_nonsymmv_alloc(mtx->rows());
int erc = gsl_eigen_nonsymmv(&m.matrix, eval, evec, w);
   gsl_eigen_nonsymmv_free(w);
   if (erc == GSL_SUCCESS)
      {
        erc = gsl_eigen_nonsymmv_sort(eval, evec, GSL_EIGEN_SORT_ABS_DESC);
      }
   gsl_set_error_handler(old_handler);

   if (erc != GSL_SUCCESS)
      {
        delete mtx;
        gsl_vector_complex_free(eval);
        gsl_matrix_complex_free(evec);
        MORE_ERROR() << "⎕MX[5] (eigenvalues): GSL error: "
                     << gsl_strerror(erc);
        DOMAIN_ERROR;
      }

   loop(i, mtx->cols())
       {
         gsl_complex eval_i = gsl_vector_complex_get(eval, i);

         Dcomplex v(GSL_REAL(eval_i), GSL_IMAG(eval_i));
         Z->next_ravel_Complex(v.real(), v.imag());
       }

  delete mtx;
  gsl_vector_complex_free(eval);
  gsl_matrix_complex_free(evec);
  Z->check_value(LOC);

  return Z;
}
//════════════════════════════════════════════════════════════════════════════

bool Quad_MX::rng_initialised = false;
bool Quad_MX::rng_seed_set = false;
unsigned int Quad_MX::rng_seed = 0;
mt19937_64 Quad_MX::rgen;   // aka. mersenne_twister_engine<...>
mt19937_64 Quad_MX::igen;

Value_P
Quad_MX::set_rng_seed(Value_P B)
{
   if (!B->is_int_scalar())
      {
        MORE_ERROR() << "⎕MX.set_rng_seed B: integer scalar B expected.";
        DOMAIN_ERROR;
      }

   rng_seed = B->get_sole_integer();
   rng_seed_set = true;

   return Idx0_0(LOC);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::determinant(Value_P B)
{
   if (B->get_rank() != 2)
      {
        MORE_ERROR() << "⎕MX.determinant B: matrix B expected.";
        RANK_ERROR;
      }
  
   if (B->get_rows() != B->get_cols())
      {
        MORE_ERROR() << "⎕MX.determinant B: square matrix B expected.";
        LENGTH_ERROR;
      }

   // the determinant of the empty matrix is the empty product 1 (Bugs38 #127)
   if (B->get_rows() == 0)   return FloatScalar(1.0, LOC);

GSL_Matrix * mtx = genMtx(B, false);
const Dcomplex det = getDet(mtx);
  delete mtx;

   if (!(isfinite(det.real()) && isfinite(det.imag())))   // Bugs39 #25
      {
        MORE_ERROR() << "⎕MX.determinant B: the determinant of B exceeds "
                        "the range of floating point numbers.";
        DOMAIN_ERROR;
      }

  return det.imag() == 0.0 ? FloatScalar(det.real(), LOC)
                           : ComplexScalar(det.real(), det.imag(), LOC);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::monadicCrossProduct(Value_P B)
{
   if (B->get_rank() != 2)
      {
        MORE_ERROR() << "⎕MX.cross_product B: Matrix B expected.";
        RANK_ERROR;
      }

const ShapeItem rows = B->get_rows();
const ShapeItem cols = B->get_cols();
   // the generalised cross product needs *exactly* n-1 vectors in ℝⁿ
   // (rows == cols-1): genMtx(B,true) pads B with one extra row of 1s,
   // so this makes the resulting mtx square by construction. The
   // earlier rows >= cols check silently accepted fewer, leaving
   // getCross() to feed a non-square matrix into genCofactor()'s
   // square-only cofactor recursion -- for a rank<cols-1 B the trailing
   // rc[] components stayed at their vector<Dcomplex> zero-init instead
   // of being computed at all (Bugs18 #10; the crash-prone "shrink to
   // square inside getCross()" alternative was tried and reverted while
   // fixing this).
   //
   if (rows != cols - 1)
      {
        MORE_ERROR() << "⎕MX.cross_product B: expecting exactly n-1 "
                         "vectors in ℝⁿ resp. ℂⁿ (⍴B is " << B->get_shape()
                      << ", so n-1 must be " << (cols - 1) << ", not "
                      << rows << ")";
        LENGTH_ERROR;
      }

GSL_Matrix * mtx = genMtx(B, true);
vector<Dcomplex> cp = getCross(mtx);
Value_P Z(mtx->cols(), LOC);
const bool cp_is_complex = GSL_Matrix::is_complex(cp);
   loop(i, mtx->cols())
       {
         if (cp_is_complex)   Z->next_ravel_Complex(cp[i].real(), cp[i].imag());
         else                 Z->next_ravel_Float(cp[i].real());
       }
  delete mtx;
  Z->check_value(LOC);
  return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Quad_MX::dyadicCrossProduct(Value_P A, Value_P B)
{
   // A and B shall be vectors
   //
   if (!A->is_vector())
      {
        MORE_ERROR() << "A ⎕MX.cross_product B: vector A expected.";
        RANK_ERROR;
      }

   if (!B->is_vector())
      {
        MORE_ERROR() << "A ⎕MX.cross_product B: vector B expected.";
        RANK_ERROR;
      }

const ShapeItem A_count = A->element_count();
   if (A_count != B->element_count())
      {
        MORE_ERROR() << "A ⎕MX.cross_product B: ⍴A ≠ ⍴B.";
        LENGTH_ERROR;
      }

   // The dyadic form only ever builds a 2×A_count matrix BB and hands it
   // to monadicCrossProduct(), whose own precondition (rows == cols - 1)
   // reduces to A_count == 3 for that shape -- exactly the case this
   // branch used to exclude, so the old A_count != 3 fallback could never
   // reach anything but LENGTH_ERROR there anyway. Raise it here directly
   // instead, with a message that actually names what the caller passed
   // (⍴A, not the monadic form's ⍴B).
   if (A_count != 3)
      {
        MORE_ERROR() << "A ⎕MX.cross_product B: the dyadic cross product is "
                        "defined only for 3-element vectors (⍴A is "
                     << A_count << "); use the monadic form "
                        "⎕MX.cross_product B with an (n-1)×n matrix B for "
                        "the generalised cross product in ℝⁿ";
        LENGTH_ERROR;
      }

GSL_Matrix * mtx = new GSL_Matrix(3, 3);
   loop(c, 3)
       {
         Cell cache_A, cache_B;
         const Cell & Ac = A->get_cravel(c, cache_A);
         const Cell & Bc = B->get_cravel(c, cache_B);
         mtx->set_val(0, c, Dcomplex(1.0, 0.0));
         mtx->set_val(1, c, Dcomplex(Ac.get_real_value(), Ac.get_imag_value()));
         mtx->set_val(2, c, Dcomplex(Bc.get_real_value(), Bc.get_imag_value()));
       }

vector<Dcomplex> cp = getCross(mtx);
const bool cp_is_complex = GSL_Matrix::is_complex(cp);

Value_P Z(mtx->cols(), LOC);
   loop(i, mtx->cols())
       {
         if (cp_is_complex)   Z->next_ravel_Complex(cp[i].real(), cp[i].imag());
         else                 Z->next_ravel_Float(cp[i].real());
       }
   Z->check_value(LOC);
   delete mtx;

   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Quad_MX::vectorAngle(const Value_P A, const Value_P B)
{
const ShapeItem A_count = A->element_count();

   if (!A->is_vector())
      {
        MORE_ERROR() << "A ⎕MX.vector_angle B: vector A expected.";
        RANK_ERROR;
      }

   if (!B->is_vector())
      {
        MORE_ERROR() << "A ⎕MX.vector_angle B: vector B expected.";
        RANK_ERROR;
      }


   if (A_count != B->element_count())
      {
        MORE_ERROR() << "A ⎕MX.vector_angle B: ⍴A ≠ ⍴B.";
        LENGTH_ERROR;
      }

vector<Dcomplex> Av(A_count);
vector<Dcomplex> Bv(A_count);
   loop(a, A_count)
       {
         Cell cache_A, cache_B;
         const Cell & Aa = A->get_cravel(a, cache_A);
         const Cell & Ba = B->get_cravel(a, cache_B);
         Av[a] = Dcomplex(Aa.get_real_value(), Aa.get_imag_value());
         Bv[a] = Dcomplex(Ba.get_real_value(), Ba.get_imag_value());
       }

   // the (real) angle acos(Re⟨A,B⟩÷|A||B|) of A and B as real vectors,
   // i.e. with the Hermitian inner product like magnitude(), computed
   // with Kahan's stable formula 2×atan2(‖a-b‖, ‖a+b‖) for the unit vectors
   // a and b of A and B: acos() of a rounded cosine near 1 is noise, e.g.
   // 2.1E¯8 for a vector and itself (Bugs38 #68).
   //
   // A and B are first scaled by their largest component, so that the
   // result does not depend on their lengths (no overflow or underflow of
   // the squares, Bugs39 #26).
   //
   if (unit_vector(Av) && unit_vector(Bv))
      {
        double diff = 0.0, sum = 0.0;
        loop(i, Av.size())
           {
             diff += std::norm(Av[i] - Bv[i]);
             sum  += std::norm(Av[i] + Bv[i]);
           }
        return FloatScalar(2.0 * atan2(sqrt(diff), sqrt(sum)), LOC);
     }

   MORE_ERROR() << "Invalid vector(s) in ⎕MX.vector_angle.";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::ident(const Value_P B)
{
   if (!B->is_scalar())
      {
        MORE_ERROR() << "⎕MX.ident B: scalar B expected.";
        RANK_ERROR;
      }

const ShapeItem dim = B->get_sole_integer();
   if (dim <  0)
      {
        MORE_ERROR() << "⎕MX.ident B: non-negative B expected.";
        DOMAIN_ERROR;
      }
   if (dim == 0)   return Idx0_0(LOC);

Value_P Z(dim, dim, LOC);
   loop(i, dim)
   loop(j, dim)   Z->next_ravel_Complex( (i == j ? 1.0 : 0.0), 0.0);

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::monadicCovariance(const Value_P B, bool corr)
{
const char * name = corr ? "⎕MX.correlation" : "⎕MX.covariance";
   if (B->get_rank() != 2)
      {
        MORE_ERROR() << name << " B: maxtrix B expected.";
        RANK_ERROR;
      }

const ShapeItem rows = B->get_rows();
const ShapeItem cols = B->get_cols();

   if (rows == 0)   return Idx0_0(LOC);   // empty result
   if (cols < 2)   // the sample covariance divides by cols - 1 (Bugs38 #62)
      {
        MORE_ERROR() << name << " B: at least 2 samples (columns of"
                        " B) expected.";
        DOMAIN_ERROR;
      }

vector<vector<double> > Breals(rows);
vector<vector<double> > Bimags(rows);
bool real_used = false;   // some real part is non-zero
bool imag_used = false;   // some imaginary part is non-zero
   loop(r, rows)
       {
         Breals[r].resize(cols);
         Bimags[r].resize(cols);
         const int offset = r * cols;
         loop(c, cols)
             {
               Cell cache;
               const Cell & Bv = B->get_cravel(offset + c, cache);
                Breals[r][c] = Bv.get_real_value();
                Bimags[r][c] = Bv.get_imag_value();
                if (Breals[r][c] != 0.0)   real_used = true;
                if (Bimags[r][c] != 0.0)   imag_used = true;
             }
       }

   // the correlation does not change if a sample is scaled by a positive
   // factor. Scale every sample by its largest magnitude, so that neither
   // the (co-)variances nor their products overflow or underflow (Bugs39
   // #30).
   //
   if (corr)   loop(r, rows)   { scale_samples(Breals[r]);
                                 scale_samples(Bimags[r]); }

   // the (co-)variances of the real and imaginary parts (Bugs38 #63:
   // the covariance, not the correlation that the former normalization
   // of each row produced). Z is symmetric, so only the upper half is
   // computed.
   //
vector<double> real_cov(rows * rows);
vector<double> imag_cov(rows * rows);
   loop(r, rows)
   for (ShapeItem c = r; c < rows; ++c)
       {
         real_cov[r*rows + c] = real_cov[c*rows + r] =
             gsl_stats_covariance(Breals[r].data(), 1,
                                  Breals[c].data(), 1, cols);
         imag_cov[r*rows + c] = imag_cov[c*rows + r] =
             gsl_stats_covariance(Bimags[r].data(), 1,
                                  Bimags[c].data(), 1, cols);
       }

   if (corr)   // check that every used part of every row has a variance
      {
        // a part that is 0 everywhere (e.g. the imaginary part of real B)
        // is ignored, unless B is 0 everywhere.
        if (!imag_used)   real_used = true;
        loop(r, rows)
            {
              const double rvar = real_cov[r*rows + r];
              const double ivar = imag_cov[r*rows + r];
              if ((real_used && !(rvar > 0.0)) ||
                  (imag_used && !(ivar > 0.0)))
                 {
                   MORE_ERROR() << name << " B: the samples in row "
                                << (r + Workspace::get_IO()) << " of B"
                                   " have no variance (all equal).";
                   DOMAIN_ERROR;
                 }
            }
      }

Value_P Z(rows, rows, LOC);
   loop(r, rows)
   loop(c, rows)
       {
         double real = real_cov[r*rows + c];
         double imag = imag_cov[r*rows + c];
         if (corr)
            {
              const ShapeItem rr = r*rows + r;
              const ShapeItem cc = c*rows + c;
              real = !real_used ? 0.0 : r == c ? 1.0
                   : pearson(real, real_cov[rr], real_cov[cc]);
              imag = !imag_used ? 0.0 : r == c ? 1.0
                   : pearson(imag, imag_cov[rr], imag_cov[cc]);
            }
         // not next_ravel_Number(), which would round e.g. a covariance
         // of 1E¯12 to the integer 0 (Bugs39 #33)
         if (imag == 0.0)   Z->next_ravel_Float(real);
         else               Z->next_ravel_Complex(real, imag);
       }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_MX::real_cells(Value & Z)
{
   // store a complex result item with imaginary part 0 as a real number,
   // like ⎕MX[1] and the covariance do, e.g. the identity matrix ⎕MX[6] 2
   // (Bugs39 #100)
   //
   if (Z.is_packed())   return;   // no ComplexCells

const ShapeItem count = Z.nz_element_count();
   loop(z, count)
       {
         Cell & cell = Z.get_wravel(z);
         if (cell.is_pointer_cell())
            {
              real_cells(*cell.get_pointer_value());
            }
         else if (cell.is_complex_cell() && cell.get_imag_value() == 0.0)
            {
              Z.set_ravel_Float(z, cell.get_real_value());
            }
       }
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_MX::scale_samples(vector<double> & samples)
{
double max_abs = 0.0;
   for (size_t s = 0; s < samples.size(); ++s)
       if (max_abs < fabs(samples[s]))   max_abs = fabs(samples[s]);

   if (max_abs == 0.0 || !isfinite(max_abs))   return;
   for (size_t s = 0; s < samples.size(); ++s)   samples[s] /= max_abs;
}
//────────────────────────────────────────────────────────────────────────────
double
Quad_MX::pearson(double cov, double var1, double var2)
{
   // not sqrt(var1 * var2), which may overflow or underflow (Bugs39 #30)
const double rho = cov / (sqrt(var1) * sqrt(var2));
   // rounding may push rho slightly beyond ±1
   if (rho >  1.0)   return  1.0;
   if (rho < -1.0)   return -1.0;
   return rho;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::histogram(const Value_P A, const Value_P B)
{
  /****
         1         2       3       4
       |       |       |       |      |
       0      .25     .50     .75    1.0
       
   ****/

const ShapeItem B_count = B->element_count();

   if (!A->is_scalar())
      {
        MORE_ERROR() << "A ⎕MX.histogram B: scalar A expected.";
        RANK_ERROR;
      }

const APL_Integer nr_buckets = A->get_sole_integer();   // not int: Bugs38 #94
   if (nr_buckets < 2)
      {
        MORE_ERROR() << "A ⎕MX.histogram B: too few buckets.";
        DOMAIN_ERROR;
      }

   if (B->get_rank() != 1)
      {
        MORE_ERROR() << "A ⎕MX.histogram B: vector B expected.";
        RANK_ERROR;
      }

double maxv = -DBL_MAX;
double minv =  DBL_MAX;
   loop(b, B_count)
       {   
         Cell cache;
         const Cell & Bb = B->get_cravel(b, cache);
         const double val = Bb.get_real_value();
         if (maxv < val) maxv = val;
         if (minv > val) minv = val;
       }

Value_P Z(nr_buckets, LOC);   // WS FULL for a huge A (before buckets)
vector<ShapeItem> buckets(nr_buckets);

   // halve the operands only if maxv - minv exceeds the double range
   // (Bugs39 #29): halving loses the last bit of subnormal values, which
   // moved them into the wrong bucket (Bugs40 #85)
   //
const bool halve = !isfinite(maxv - minv);
   loop(b, B_count)
       {   
         Cell cache;
         const Cell & Bb = B->get_cravel(b, cache);
         const double val = Bb.get_real_value();

         // compute the bucket of val directly instead of accumulating the
         // bucket edges (which accumulated rounding errors), and snap
         // values that lie on an edge (up to rounding) onto that edge,
         // so that the histogram is invariant under scaling (Bugs38 #94).
         //
         // The halves (see above) keep the differences finite also when
         // maxv - minv exceeds the double range (Bugs39 #29). The snap tolerance is a
         // few hundred ulps of q, i.e. rounding errors only, so that
         // values clearly below an edge are not moved up (Bugs39 #27).
         //
         ShapeItem bucket = nr_buckets - 1;   // also if maxv == minv
         if (maxv > minv)
            {
              double q = halve
                       ? (val/2 - minv/2) / (maxv/2 - minv/2) * nr_buckets
                       : (val - minv) / (maxv - minv) * nr_buckets;
              const double edge = nearbyint(q);
              if (fabs(q - edge) < 1E-13 * (1.0 + fabs(edge)))   q = edge;
              if (q < double(nr_buckets - 1))   bucket = ShapeItem(q);
            }
         buckets[bucket]++;
       }

   loop(b, nr_buckets)
       {
         Z->next_ravel_Int(buckets[b]);
       }
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::dyadicCovariance(const Value_P A, const Value_P B, bool corr)
{
const char * name = corr ? "A ⎕MX.correlation B" : "A ⎕MX.covariance B";
   if (!A->is_vector())
      {
        MORE_ERROR() << name << ": vector A expected.";
        RANK_ERROR;
      }

   if (!B->is_vector())
      {
        MORE_ERROR() << name << ": vector B expected.";
        RANK_ERROR;
      }

const ShapeItem AB_count = A->element_count();
   if (AB_count < 2)   // the sample covariance divides by AB_count - 1
      {
        MORE_ERROR() << name << ": at least 2 samples expected.";
        DOMAIN_ERROR;   // Bugs38 #62
      }
   if (AB_count != B->element_count())
      {
        MORE_ERROR() << name << ": ⍴A ≠ ⍴B.";
        LENGTH_ERROR;
      }

vector<double> Areals(AB_count);
vector<double> Aimags(AB_count);
vector<double> Breals(AB_count);
vector<double> Bimags(AB_count);
bool real_used = false;   // some real part is non-zero
bool imag_used = false;   // some imaginary part is non-zero
   loop(ab, AB_count)
       {
         Cell cache_A, cache_B;
         const Cell & Av = A->get_cravel(ab, cache_A);
         const Cell & Bv = B->get_cravel(ab, cache_B);
         Areals[ab] = Av.get_real_value();
         Aimags[ab] = Av.get_imag_value();
         Breals[ab] = Bv.get_real_value();
         Bimags[ab] = Bv.get_imag_value();
         if (Areals[ab] != 0.0 || Breals[ab] != 0.0)   real_used = true;
         if (Aimags[ab] != 0.0 || Bimags[ab] != 0.0)   imag_used = true;
       }

   if (corr)   // see monadicCovariance() (Bugs39 #30)
      {
        scale_samples(Areals);   scale_samples(Aimags);
        scale_samples(Breals);   scale_samples(Bimags);
      }

double real =
    gsl_stats_covariance(Areals.data(), 1, Breals.data(), 1, AB_count);
double imag =
    gsl_stats_covariance(Aimags.data(), 1, Bimags.data(), 1, AB_count);

   if (corr)   // Bugs38 #63
      {
        // a part that is 0 everywhere (e.g. the imaginary part of real
        // A and B) is ignored, unless A and B are 0 everywhere.
        if (!imag_used)   real_used = true;
        const double A_rvar = gsl_stats_variance(Areals.data(), 1, AB_count);
        const double B_rvar = gsl_stats_variance(Breals.data(), 1, AB_count);
        const double A_ivar = gsl_stats_variance(Aimags.data(), 1, AB_count);
        const double B_ivar = gsl_stats_variance(Bimags.data(), 1, AB_count);
        const char * novar = 0;
        if      (real_used && !(A_rvar > 0.0))   novar = "A";
        else if (real_used && !(B_rvar > 0.0))   novar = "B";
        else if (imag_used && !(A_ivar > 0.0))   novar = "A";
        else if (imag_used && !(B_ivar > 0.0))   novar = "B";
        if (novar)
           {
             MORE_ERROR() << name << ": the samples in " << novar
                          << " have no variance (all equal).";
             DOMAIN_ERROR;
           }

        real = real_used ? pearson(real, A_rvar, B_rvar) : 0.0;
        imag = imag_used ? pearson(imag, A_ivar, B_ivar) : 0.0;
      }

  return imag == 0.0 ? FloatScalar(real, LOC) : ComplexScalar(real, imag, LOC);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::randoms(Value_P opt_A, Value_P B, int dist)
{
APL_Integer rcnt = 0;   // assume error (not int: Bugs39 #68)

   if (!rng_initialised)
     {
       std::seed_seq rseq{1, 2, 3, 4,  5};   // real
       std::seed_seq iseq{6, 7, 8, 9, 10};   // imag
       rgen.seed(rseq);
       igen.seed(iseq);
       rng_initialised = true;
     }

   if (rng_seed_set)
     {
       rgen.seed(rng_seed);

       // a different seed for the imaginary parts, otherwise they were
       // equal to the real parts (Bugs38 #64)
       std::seed_seq iseq{rng_seed, 6u, 7u, 8u, 9u, 10u};
       igen.seed(iseq);
       rng_seed_set = false;   // re-seed on next call
     }

   // A ⎕MX[10] B returns a vector of A items, ⎕MX[10] B a scalar (apl.texi
   // "Randoms", Bugs38 #66). Monadic: B is a scalar (or, as before, a
   // non-scalar whose first item is used).
   //
   if (opt_A)   // optional A provided
      {
        if (!opt_A->is_scalar() || !B->is_scalar())
           {
             MORE_ERROR() << "A ⎕MX[10] B: scalar A and B expected.";
             RANK_ERROR;
           }
        rcnt = opt_A->get_sole_integer();
        if (rcnt < 0)
           {
             MORE_ERROR() << "A ⎕MX[10] B: non-negative A expected.";
             DOMAIN_ERROR;
           }
      }
   else
      {
        if (B->element_count() == 0)
           {
             MORE_ERROR() << "⎕MX[10] B: non-empty B expected.";
             LENGTH_ERROR;
           }
        rcnt = 1;
      }

   if (rcnt == 0)   return Idx0(LOC);   // 0 ⎕MX[10] B

Cell cache;
const Cell & B0 = B->get_cfirst(cache);
const APL_Float B0_real = B0.get_real_value();
const APL_Float B0_imag = B0.get_imag_value();

Shape shape_Z;   // scalar
   if (opt_A)   shape_Z.add_shape_item(rcnt);
Value_P Z(shape_Z, LOC);
   loop(r, rcnt)
       {
         Dcomplex yy;
         switch(dist)
            {
              // https://en.cppreference.com/w/cpp/numeric/random

              case 0:  {
                         normal_distribution<double> real_dist{0.0, B0_real};
                         normal_distribution<double> imag_dist{0.0, B0_imag};
                         yy = complex<double>(real_dist(rgen), imag_dist(igen));
                       }
                        break;

              case 1:   {
                          // like the chi-squared and student-t cases below:
                          // σ > 0, and no imaginary part for a real B (it
                          // was exp(N(0,0)) = 1, Bugs38 #65)
                          if (B0_real <= 0.0)
                             {
                               MORE_ERROR() << "⎕MX[10, 1] B: B (log-normal "
                                               "σ) must be > 0.";
                               DOMAIN_ERROR;
                             }
                          lognormal_distribution<double> real_dist{0.0, B0_real};
                          const double yim = (B0_imag > 0.0)
                             ? lognormal_distribution<double>{0.0, B0_imag}(igen)
                             : 0.0;
                          yy = complex<double>(real_dist(rgen), yim);
                        }
                        break;

              case 2:   {
                          // unlike normal/lognormal's stddev, chi-squared's
                          // degrees-of-freedom parameter has no meaningful
                          // value at 0 -- constructing chi_squared_distribution
                          // with dof<=0 is undefined behaviour (observed as
                          // outright corrupt output, not just a wrong number).
                          // B0_real is the parameter the caller actually
                          // asked for, so reject it; B0_imag is 0.0 whenever
                          // B was a plain real number, so treat that as "no
                          // imaginary part requested" (matching how case 0/1
                          // already tolerate a real-only B).
                          if (B0_real <= 0.0)
                             {
                               MORE_ERROR() << "⎕MX[10, 2] B: B (chi-squared "
                                               "degrees of freedom) must be "
                                               "> 0.";
                               DOMAIN_ERROR;
                             }
                          chi_squared_distribution<double> real_dist{B0_real};
                          const double yim = (B0_imag > 0.0)
                             ? chi_squared_distribution<double>{B0_imag}(igen)
                             : 0.0;
                          yy = complex<double>(real_dist(rgen), yim);
                        }
                        break;

              case 3:   {
                          // see case 2: student-t's degrees-of-freedom
                          // parameter is likewise undefined at 0.
                          if (B0_real <= 0.0)
                             {
                               MORE_ERROR() << "⎕MX[10, 3] B: B (student-t "
                                               "degrees of freedom) must be "
                                               "> 0.";
                               DOMAIN_ERROR;
                             }
                          student_t_distribution<double> real_dist{B0_real};
                          const double yim = (B0_imag > 0.0)
                             ? student_t_distribution<double>{B0_imag}(igen)
                             : 0.0;
                          yy = complex<double>(real_dist(rgen), yim);
                        }
                        break;

              default: MORE_ERROR() << "⎕MX[10, D] B: invalid distribution D";
                       DOMAIN_ERROR;

            }

         Z->next_ravel_Complex(yy.real(), yy.imag());
       }

  return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::norm(const Value_P B)
{
Value_P Z(B->get_shape(), LOC);
const ShapeItem B_count = B->element_count();
vector<Dcomplex> v;
   v.reserve(B_count);
   loop(c, B_count)
       {
         Cell cache;
         const Cell & Bv = B->get_cravel(c, cache);
         v.push_back(Dcomplex(Bv.get_real_value(), Bv.get_imag_value()));
       }

   // scaled by the largest component first (like ⎕MX[3]), so that Σ|b|²
   // neither overflows (giving 0 0) nor underflows (giving the DOMAIN
   // ERROR below) for large or tiny B (Bugs40 #46)
   //
   if (!unit_vector(v))
      {
        MORE_ERROR() << "⎕MX.norm B: B is the zero vector.";
        DOMAIN_ERROR;
      }

   loop(b, B_count)   Z->next_ravel_Complex(v[b].real(), v[b].imag());
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::monadicRotation(Value_P B)
{
   if (!B->is_scalar())
      {
        MORE_ERROR() << "⎕MX.rotation_matrix B: scalar B expected.";
        RANK_ERROR;
      }

Cell cache;
const Cell & B0 = B->get_cscalar(cache);
const APL_Float xr = B0.get_real_value();
const APL_Float xi = B0.get_imag_value();
const Dcomplex theta(xr, xi);
const Dcomplex cosx = cos(theta);
const Dcomplex sinx = sin(theta);

Value_P Z(2, 2, LOC);
  Z->next_ravel_Complex( cosx.real(),  cosx.imag());
  Z->next_ravel_Complex(-sinx.real(), -sinx.imag());
  Z->next_ravel_Complex( sinx.real(),  sinx.imag());
  Z->next_ravel_Complex( cosx.real(),  cosx.imag());
  Z->check_value(LOC);
  return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_MX::dyadicRotation(int tp, Value_P A, Value_P B)
{
const ShapeItem A_count   = A->element_count();
const ShapeItem B_count   = B->element_count();
    
   if (!A->is_vector())
      {
        MORE_ERROR() << "A ⎕MX.homogeneous_matrix B: vector A expected.";
        RANK_ERROR;
      }

   if (!B->is_vector())
      {
        MORE_ERROR() << "A ⎕MX.homogeneous_matrix B: vector B expected.";
        RANK_ERROR;
      }

   if (A_count != 3 || B_count != 3)
      {
        MORE_ERROR() << "A ⎕MX."
                     << (tp == 9 ? "rotation_matrix" : "homogeneous_matrix")
                     << " B: 3-element vectors A and B expected.";
        LENGTH_ERROR;
      }

const int sdim = (tp == 9) ? 3 : 4;
Value_P Z(sdim, sdim, LOC);
Dcomplex angles[3];
   loop(i, 3)
       {
         Cell cache;
         const Cell & Bv = B->get_cravel(i, cache);
         angles[i] = Dcomplex(Bv.get_real_value(), Bv.get_imag_value());
       }

const Dcomplex cosa = cos(angles[0]);
const Dcomplex sina = sin(angles[0]);
const Dcomplex cosb = cos(angles[1]);
const Dcomplex sinb = sin(angles[1]);
const Dcomplex cosg = cos(angles[2]);
const Dcomplex sing = sin(angles[2]);

const Dcomplex t00 = cosa * cosb;
const Dcomplex t01 = cosa * sinb * sing - sina * cosg;
const Dcomplex t02 = cosa * sinb * cosg + sina * sing;

const Dcomplex t10 = sina * cosb;
const Dcomplex t11 = sina * sinb * sing + cosa * cosg;
const Dcomplex t12 = sina * sinb * cosg - cosa * sing;

const Dcomplex t20 = -sinb;
const Dcomplex t21 =  cosb * sing;
const Dcomplex t22 =  cosb * cosg;

   if (tp == 9)
      {
        Z->next_ravel_Complex(t00.real(), t00.imag());
        Z->next_ravel_Complex(t01.real(), t01.imag());
        Z->next_ravel_Complex(t02.real(), t02.imag());
        Z->next_ravel_Complex(t10.real(), t10.imag());
        Z->next_ravel_Complex(t11.real(), t11.imag());
        Z->next_ravel_Complex(t12.real(), t12.imag());
        Z->next_ravel_Complex(t20.real(), t20.imag());
        Z->next_ravel_Complex(t21.real(), t21.imag());
        Z->next_ravel_Complex(t22.real(), t22.imag());
      }
   else
      {
        Z->next_ravel_Complex(t00.real(), t00.imag());
        Z->next_ravel_Complex(t01.real(), t01.imag());
        Z->next_ravel_Complex(t02.real(), t02.imag());
        Z->next_ravel_Complex(0.0,        0.0);

        Z->next_ravel_Complex(t10.real(), t10.imag());
        Z->next_ravel_Complex(t11.real(), t11.imag());
        Z->next_ravel_Complex(t12.real(), t12.imag());
        Z->next_ravel_Complex(0.0,        0.0);

        Z->next_ravel_Complex(t20.real(), t20.imag());
        Z->next_ravel_Complex(t21.real(), t21.imag());
        Z->next_ravel_Complex(t22.real(), t22.imag());
        Z->next_ravel_Complex(0.0,        0.0);

        // A is a displacement vector: numbers (not e.g. characters,
        // Bugs38 #67), like every other ⎕MX argument
        //
        loop(a, 3)
           {
             Cell cache;
             const Cell & cell = A->get_cravel(a, cache);
             if (!cell.is_numeric())
                {
                  MORE_ERROR() << "A ⎕MX.homogeneous_matrix B: numeric A"
                                  " expected.";
                  DOMAIN_ERROR;
                }
             Z->next_ravel_Complex(cell.get_real_value(),
                                   cell.get_imag_value());
           }
        Z->next_ravel_Complex(1.0, 0.0);
      }

   Z->check_value(LOC);
   return Z;
}
#else //====================not apl_GSL =====================================
 
Quad_MX::Quad_MX()
  : QuadFunction(TOK_Quad_MX)
{
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_MX::print_fun_syntax(ostream & out,
                          const FunctionGroup::function_info & info) const
{
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_MX::print_map_syntax(ostream & out,
                          const FunctionGroup::function_info & info) const
{
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_MX::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
  return eval_B(B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_MX::eval_XB(cValue_R X, cValue_R B) const
{
  return eval_B(B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_MX::eval_AB(cValue_R A, cValue_R B) const
{
  return eval_B(B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_MX::eval_B(cValue_R B) const
{
const char * libs[] = { "libgsl.so", "libgslcblas.so", 0 };
const char * hdrs[] = { "gsl_statistics.h", "gsl_math.h", "gsl_eigen.h", 0 };
const char * pkgs[] = { "libgsl-dev", 0 };

   return missing_files("⎕MX", libs, hdrs, pkgs);
}
//════════════════════════════════════════════════════════════════════════════

#endif   // (not) apl_GSL

