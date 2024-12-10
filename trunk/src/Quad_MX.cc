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
#include "Quad_MX.hh"
#include "Workspace.hh"
#include "APL_types.hh"
#include "Shape.hh"
#include "Value.hh"

Quad_MX Quad_MX::fun;

#include<cmath>
#include<math.h>
#include<complex>
#include<vector>

struct Quad_MX::fun_info Quad_MX::op_desc[] = {
#define op_entry(e, desc, v, sub) { Quad_MX::OP_ ## e, v, desc, sub},
#include "Quad_MX.def"
                                              };
Quad_MX::Quad_MX() : QuadFunction(TOK_Quad_MX)
{
  // sort op_desc alphabetically. It is small, so a simple O(n²) algo suffices
  //
const int count = sizeof(op_desc) / sizeof(*op_desc);

  loop(i, count)
  for (ShapeItem j = i + 1; j < count; ++j)
      {
        if (strcmp(op_desc[i].sub_name, op_desc[j].sub_name) > 0)
           {;
             const fun_info tmp = op_desc[i];
             op_desc[i] = op_desc[j];
             op_desc[j] = tmp;
           }
      }
}

#if apl_GSL

#include <gsl/gsl_statistics.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_eigen.h>

/************** start Matrix class **************/
/****** NOTE: gsl matrices don't support complex ******/

//----------------------------------------------------------------------------
// ============= start utility fcns ===============

//----------------------------------------------------------------------------
Quad_MX::Matrix *
Quad_MX::genCofactor(Matrix *mtx, int r, int c)
{
Matrix * cf = new Matrix(mtx->rows() - 1, mtx->cols() - 1);

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
//----------------------------------------------------------------------------
Quad_MX::Dcomplex
Quad_MX::getDet(Matrix * mtx)
{
  if (mtx->rows() == 2)
     {
       return (mtx->val(0, 0) * mtx->val(1, 1)) -   // diagonal
              (mtx->val(0, 1) * mtx->val(1, 0));    // off-diagonal
     }

Dcomplex det(0.0, 0.0);
  for (int k = 0; k < mtx->rows(); k++)
      {
        const int sign = k & 1 ? -1 : 1;
        Matrix * cf = genCofactor(mtx, 0, k);
        const Dcomplex id = getDet(cf);
        delete cf;
        if (sign > 0)   det += mtx->val(0, k) * id;
        else            det -= mtx->val(0, k) * id;
      }

  return det;
}
//----------------------------------------------------------------------------
vector<Quad_MX::Dcomplex>
Quad_MX::getCross(Matrix *mtx)
{
Dcomplex det(0.0, 0.0);
vector<Dcomplex> rc(mtx->cols());
  if (mtx->rows() == 2)
     {
       det = (mtx->val(0, 0) * mtx->val(1,1)) 
           - (mtx->val(0, 1) * mtx->val(1,0));
     }
  else
    {
      loop(k, mtx->rows())
          {
            const int sign = k & 1 ? -1 : 1;
            Matrix * cf = genCofactor(mtx, 0, k);
            const Dcomplex id = getDet(cf);
            delete cf;
            rc[k] = sign > 0 ? id : -id;
          }
    }

  return rc;
}
//----------------------------------------------------------------------------
Quad_MX::Dcomplex
Quad_MX::magnitude(vector<Dcomplex> &v)
{
Dcomplex rc(0.0, 0.0);
  loop(i, v.size())   rc += v[i] * v[i];
  return sqrt(rc);
}
//----------------------------------------------------------------------------
void
Quad_MX::normalise(vector<double> &v)
{
const double mean = gsl_stats_mean(v.data(), 1, v.size());
const double sdev = gsl_stats_sd_m(v.data(), 2, v.size(), mean);
  loop(c, v.size())
    v[c] = (v[c] - mean) / sdev;
}
//----------------------------------------------------------------------------
Quad_MX::Matrix *
Quad_MX::genMtx(Value_P B, bool padded)
{
const ShapeItem cols = B->get_cols();
const ShapeItem rows = B->get_rows() + (padded ? 1 : 0);
Matrix * mtx = new Matrix(rows, cols);
ShapeItem b = 0;
  loop(r, rows)
  loop(c, cols)
      {
        if (padded && r == 0)
           {
             mtx->set_val(0, c, complex(1.0, 0.0));
           }
        else
           {
             const Cell & Bb = B->get_cravel(b++);
             const APL_Float Bbr = Bb.get_real_value();
             const APL_Float Bbi = Bb.get_imag_value();
             mtx->set_val(r, c, complex(Bbr, Bbi));
           }
      }

  return mtx;
}
// ============= end of utility fcns ===============

//----------------------------------------------------------------------------

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
       MORE_ERROR() << "numeric scalar argument expected.";
       DOMAIN_ERROR;
     }

  rng_seed = B->get_sole_integer();
  rng_seed_set = true;

  return Idx0_0(LOC);
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::printit(Value_P filename, Value_P B)
{
const CellType B_celltype = B->deep_cell_types();


  // A is either function_name or >function_name,
  // where > indicates append (as opposed to overwrite) and shall be skipped
  //
  if (filename->is_char_string() && B->is_char_string())
     {
       const UCS_string ustr = filename->get_UCS_ravel();
       UTF8_string fun(ustr);
       const char * fun_ccp = fun.c_str();
       const char * mode = "w";
       if (strlen(fun_ccp) > 1 && *fun_ccp == '>')
          {
            fun_ccp++;
            mode = "a";   // append
          }

       FILE * ofile = fopen(fun_ccp, mode);
       const UCS_string B_ucs = B->get_UCS_ravel();
       const UTF8_string val(B_ucs);
       const char * vs = val.c_str();

       fprintf(ofile, "%s\n", vs);
       fclose(ofile);

       return Idx0_0(LOC);
     }

  if (!(filename->is_char_string() && (B_celltype & CT_NUMERIC)))
     {
       MORE_ERROR() << "Incompatible arguments in ⎕MX.print.";
       DOMAIN_ERROR;
     }

const ShapeItem B_count = B->element_count();
const uRank     B_rank  = B->get_rank();
const UCS_string ustr   = filename->get_UCS_ravel();
UTF8_string fn(ustr);
const char * fun_ccp = fn.c_str();
const char * mode = "w";   // write (as opposed to append)
  if (strlen(fun_ccp) > 1 && * fun_ccp == '>')
     {
       fun_ccp++;
       mode = "a";   // append
     }
FILE * ofile = fopen(fun_ccp, mode);

  if (!ofile)
     {
       MORE_ERROR() << "Open failure on " << ustr;
       DOMAIN_ERROR;
     }
  if (B_rank <= 1)
     {
       loop(b, B_count)
           {
             const Cell & Bb = B->get_cravel(b);
             const APL_Float Bbr = Bb.get_real_value();
             if (Bb.is_complex_cell())
               {
                 const APL_Float Bbi = Bb.get_imag_value();
                 fprintf(ofile, "%gj%g ", Bbr, Bbi);
               }
             else
               {
                 fprintf(ofile, "%g ", Bbr);
               }
           }
       fprintf(ofile, "\n");
     }
  else
     {
       const int end_line = B->get_shape_item(B_rank - 1);
       const int end_grid = end_line * B->get_shape_item(B_rank - 2);
       enum { STR_LEN = 256 };
      char str[STR_LEN];
      bool is_cpx = false;
      int max_len = -1;
      loop(a, B_count)
          {
            int len;
            const Cell & cell_A = B->get_cravel(a);
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

      int * rho = reinterpret_cast<int *>(alloca(B_rank * sizeof(int)));
      memset(rho, 0, B_rank * sizeof(int));

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

            const Cell & Av = B->get_cravel(b);
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

  fclose(ofile);
  return Idx0_0(LOC);
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::eigenvectors(Value_P B)
{
  if (B->get_rank() != 2)
     {
       MORE_ERROR() << "Matrix expected.";
       RANK_ERROR;
     }

const ShapeItem rows = B->get_rows();
const ShapeItem cols = B->get_cols();
    
    if (rows != cols)
       {
         MORE_ERROR() << "Non-square matrix.";
         RANK_ERROR;
       }

Matrix * mtx = genMtx(B, false);
const size_t mtx_bytes = mtx->rows() * mtx->cols() * sizeof(double);
double * data = reinterpret_cast<double *>(alloca(mtx_bytes));
int i = 0;
  loop(j, mtx->rows())
  for (int k = 0; k < mtx->cols(); k++, i++)
      {
        data[i] = mtx->val(j, k).real();
      }

gsl_matrix_view m = gsl_matrix_view_array(data, mtx->rows(), mtx->cols());

gsl_vector_complex * eval = gsl_vector_complex_alloc(mtx->rows());
gsl_matrix_complex * evec = gsl_matrix_complex_alloc(mtx->rows(), mtx->cols());

gsl_eigen_nonsymmv_workspace * w = gsl_eigen_nonsymmv_alloc(mtx->rows());
int erc = gsl_eigen_nonsymmv(&m.matrix, eval, evec, w);
    if (erc == GSL_SUCCESS)
       {
         gsl_eigen_nonsymmv_free(w);
         erc = gsl_eigen_nonsymmv_sort(eval, evec, GSL_EIGEN_SORT_ABS_DESC);
       }

    if (erc != GSL_SUCCESS)
      {
        delete mtx;
        MORE_ERROR() << "Eigensystem computation error.";
        INTERNAL_ERROR;
      }

Value_P Z(rows, cols, LOC);
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

  return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::eigenvalues(Value_P B)
{
  if (B->get_rank() != 2) 
     {
       MORE_ERROR() << "Matrix expected.";
       RANK_ERROR;
     }
    
const ShapeItem rows = B->get_rows();
const ShapeItem cols = B->get_cols();
    
  if (rows != cols)
     {
       MORE_ERROR() << "square matrix expected.";
       LENGTH_ERROR;
     }

Matrix * mtx = genMtx(B, false);
const size_t mtx_bytes = mtx->rows() * mtx->cols() * sizeof(double);
double * data = reinterpret_cast<double *> (alloca(mtx_bytes));
int i = 0;
  loop(j, mtx->rows())
  loop(k, mtx->cols())
      {
        data[i++] = mtx->val(j, k).real();
      }

gsl_matrix_view m = gsl_matrix_view_array(data, mtx->rows(), mtx->cols());

gsl_vector_complex * eval = gsl_vector_complex_alloc(mtx->rows());
gsl_matrix_complex * evec = gsl_matrix_complex_alloc(mtx->rows(), mtx->cols());

gsl_eigen_nonsymmv_workspace * w = gsl_eigen_nonsymmv_alloc(mtx->rows());
int erc = gsl_eigen_nonsymmv(&m.matrix, eval, evec, w);
  if (erc == GSL_SUCCESS)
     {
       gsl_eigen_nonsymmv_free(w);
       erc = gsl_eigen_nonsymmv_sort(eval, evec, GSL_EIGEN_SORT_ABS_DESC);
     }

  if (erc != GSL_SUCCESS)
     {
       delete mtx;
       MORE_ERROR() << "Eigensystem computation error.";
       INTERNAL_ERROR;
     }

Value_P Z(cols, LOC);
  loop(i, mtx->cols())
      {
        gsl_complex eval_i = gsl_vector_complex_get(eval, i);

        Dcomplex v(GSL_REAL(eval_i), GSL_IMAG(eval_i));
        Z->next_ravel_Complex(v.real(), v.imag());
      }

  delete mtx;
  Z->check_value(LOC);

  return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::determinant(Value_P B)
{
  if (B->get_rank() != 2)
    {
      MORE_ERROR() << "Matrix expected in ⎕MX.determinant.";
      RANK_ERROR;
    }

  if (B->get_rows() != B->get_cols())
     {
       MORE_ERROR() << "Square matrix expected in ⎕MX.determinant.";
       RANK_ERROR;
     }

Matrix * mtx = genMtx(B, false);
const Dcomplex det = getDet(mtx);
  delete mtx;

  return det.imag() == 0.0 ? FloatScalar(det.real(), LOC)
                           : ComplexScalar(det.real(), det.imag(), LOC);
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::monadicCrossProduct(Value_P B)
{
  if (B->get_rank() != 2)
    {
      MORE_ERROR() << "matrix expected in Quad_MX::monadicCrossProduct().";
      RANK_ERROR;
    }

const ShapeItem rows = B->get_rows();
const ShapeItem cols = B->get_cols();
  if (rows >= cols)
     {
       MORE_ERROR() << "too many vectors in cross product. "
                        "Max. is n-1 vectors in ℝⁿ resp. ℂⁿ ";
       LENGTH_ERROR;
     }
     
Matrix * mtx = genMtx(B, true);
vector<Dcomplex> cp = getCross(mtx);
Value_P Z(mtx->cols(), LOC);
const bool cp_is_complex = Matrix::is_complex(cp);
  loop(i, mtx->cols())
      {
        if (cp_is_complex)   Z->next_ravel_Complex(cp[i].real(), cp[i].imag());
        else                 Z->next_ravel_Float(cp[i].real());
      }
  delete mtx;
  Z->check_value(LOC);
  return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::dyadicCrossProduct(Value_P A, Value_P B)
{
  // A and B shall be vectors
  //
  if (!(A->is_vector() && B->is_vector()))
    {
      MORE_ERROR() << "Vectors expected in dyadic ⎕MX.cross_product().";
      RANK_ERROR;
    }

const ShapeItem A_count = A->element_count();
  if (A_count != B->element_count())
    {
      MORE_ERROR() << "Length mismatch in dyadic ⎕MX.cross_product().";
      LENGTH_ERROR;
    }

  // JSA: handle len > 3 cases in monadicCrossProduct()
  //
  if (A_count != 3)
     {
       Value_P BB(2, A_count, LOC);
       Cell * cell_BB = &BB->get_wfirst();

       const Cell * cell_A = &A->get_cfirst();
       loop(a, A_count)   cell_A++->init_other(cell_BB++, *BB, LOC);

       const Cell * cell_B = &B->get_cfirst();
       loop(b, A_count)   cell_B++->init_other(cell_BB++, *BB, LOC);

       BB->check_value(LOC);
       return monadicCrossProduct(BB);
     }

Matrix * mtx = new Matrix(3, 3);
  loop(c, 3)
      {
        const Cell & Ac = A->get_cravel(c);
        const Cell & Bc = B->get_cravel(c);
        mtx->set_val(0, c, Dcomplex(1.0, 0.0));
        mtx->set_val(1, c, Dcomplex(Ac.get_real_value(), Ac.get_imag_value()));
        mtx->set_val(2, c, Dcomplex(Bc.get_real_value(), Bc.get_imag_value()));
      }

vector<Dcomplex> cp = getCross(mtx);
const bool cp_is_complex = Matrix::is_complex(cp);

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
//----------------------------------------------------------------------------
Value_P
Quad_MX::vectorAngle(const Value_P A, const Value_P B)
{
const ShapeItem A_count = A->element_count();

  if (!(A->is_vector() && B->is_vector()))
     {
       MORE_ERROR() << "Vectors expected in ⎕MX.vector_angle.";
       DOMAIN_ERROR;
     }

  if (A_count != B->element_count())
     {
       MORE_ERROR() << "Length mismatch in ⎕MX.vector_angle.";
       LENGTH_ERROR;
     }

vector<Dcomplex> Av(A_count);
vector<Dcomplex> Bv(A_count);
  loop(a, A_count)
      {
        const Cell & Aa = A->get_cravel(a);
        const Cell & Ba = B->get_cravel(a);
        Av[a] = Dcomplex(Aa.get_real_value(), Aa.get_imag_value());
        Bv[a] = Dcomplex(Ba.get_real_value(), Ba.get_imag_value());
      }

const Dcomplex Amag = magnitude(Av);
const Dcomplex Bmag = magnitude(Bv);
const Dcomplex mag = Amag * Bmag;
  if (mag != Dcomplex(0.0, 0.0))
     {
       Dcomplex dp(0.0, 0.0);
       loop(i, Av.size())   dp += Av[i] * Bv[i];
       const Dcomplex an = acos(dp/mag);
       return ComplexScalar((APL_Float)an.real(), an.imag(), LOC);
     }

  MORE_ERROR() << "Invalid vector(s) in ⎕MX.vector_angle.";
  DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::ident(const Value_P B)
{
  if (!B->is_scalar())
     {
       MORE_ERROR() << "skalar expected.";
       RANK_ERROR;
     }

const ShapeItem dim = B->get_sole_integer();
  if (dim == 0)   return Idx0_0(LOC);

Value_P Z(dim, dim, LOC);
  loop(i, dim)
  loop(j, dim)   Z->next_ravel_Complex( (i == j ? 1.0 : 0.0), 0.0);

  Z->check_value(LOC);
  return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::monadicCovariance(const Value_P B)
{
  if (B->get_rank() != 2)
     {
       MORE_ERROR() << "Matrix argument expected.";
       RANK_ERROR;
     }

const ShapeItem rows = B->get_rows();
const ShapeItem cols = B->get_cols();

  if (rows == 0)   return Idx0_0(LOC);   // empty result

vector<vector<double> > Breals(rows);
vector<vector<double> > Bimags(rows);
  loop(r, rows)
      {
        Breals[r].reserve(cols);
        Bimags[r].reserve(cols);
        const int offset = r * cols;
        loop(c, cols)
            {
              const Cell & Bv = B->get_cravel(offset + c);
               Breals[r][c] = Bv.get_real_value();
               Bimags[r][c] = Bv.get_imag_value();
            }
        normalise(Breals[r]);
        normalise(Bimags[r]);
      }

Value_P Z(rows, rows, LOC);
  // Z is symmetric, therefore only the upper half is computed
  // and copied to the lower half in the process
  //
  loop(r, rows)
  loop(c, rows)
      {
       if (c >= r)   // on or above diagonal: compute value
          {
            const double realcov =
                         gsl_stats_covariance(Breals[r].data(), 1,
                                              Breals[c].data(), 1, cols);
            const double imagcov =
                         gsl_stats_covariance(Bimags[r].data(), 1,
                                              Bimags[c].data(), 1, cols);
            Z->next_ravel_Complex(realcov, imagcov);
          }
       else   // below diagonal: copy from corresponding item above diagonal
          {
            Z->next_ravel_Cell(Z->get_cravel(c * rows + r));
          }
      }

  Z->check_value(LOC);
  return Z;
}
//----------------------------------------------------------------------------
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
       MORE_ERROR() << "Scalar left argument expected in ⎕MX.histogram.";
       RANK_ERROR;
     }

const int nr_buckets = A->get_sole_integer();
  if (nr_buckets < 2)
     {
       MORE_ERROR() << "Too few buckets in ⎕MX.histogram.";
       DOMAIN_ERROR;
     }

  if (B->get_rank() != 1)
     {
       MORE_ERROR() << "Non-vector right argument.";
       RANK_ERROR;
     }

double maxv = -MAXFLOAT;
double minv =  MAXFLOAT;
  loop(b, B_count)
      {   
        const Cell & Bb = B->get_cravel(b);
        const double val = Bb.get_real_value();
        if (maxv < val) maxv = val;
        if (minv > val) minv = val;
      }

const double incr = (maxv - minv) / (double)nr_buckets;
vector<int> buckets(nr_buckets);
int nr_bumps = 0;
  loop(b, B_count)
      {   
        const Cell & Bb = B->get_cravel(b);
        double val = Bb.get_real_value();
        double mark = minv + incr;
        loop(bucket, nr_buckets - 1)
            {
              if (val < mark)
                 {
                   buckets[bucket]++;
                   nr_bumps++;
                   break;
                 }
            mark += incr;
          }
        buckets[nr_buckets - 1] = B_count - nr_bumps;
      }

Value_P Z(nr_buckets, LOC);
  loop(b, nr_buckets)
      {
        Z->next_ravel_Int(buckets[b]);
      }
  Z->check_value(LOC);
  return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::dyadicCovariance(const Value_P A, const Value_P B)
{
  if (!(A->is_vector() && B->is_vector()))
    {
      MORE_ERROR() << "Both arguments must be vectors.";
      RANK_ERROR;
    }

const ShapeItem AB_count = A->element_count();
  if (AB_count != B->element_count())
    {
      MORE_ERROR() << "Arguments must be of the same length.";
      LENGTH_ERROR;
    }

vector<double> Areals(AB_count);
vector<double> Aimags(AB_count);
vector<double> Breals(AB_count);
vector<double> Bimags(AB_count);
  loop(ab, AB_count)
      {
        const Cell & Av = A->get_cravel(ab);
        const Cell & Bv = B->get_cravel(ab);
        Areals[ab] = Av.get_real_value();
        Aimags[ab] = Av.get_imag_value();
        Breals[ab] = Bv.get_real_value();
        Bimags[ab] = Bv.get_imag_value();
      }

const double realcov =
    gsl_stats_covariance(Areals.data(), 1, Breals.data(), 1, AB_count);
const double imagcov =
    gsl_stats_covariance(Aimags.data(), 1, Bimags.data(), 1, AB_count);
  return imagcov == 0.0 ? FloatScalar(realcov, LOC) :
                          ComplexScalar(realcov, imagcov, LOC);
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::randoms(const Value_P * opt_A, const Value_P B, int modifier)
{
int rcnt = 0;

   if (!rng_initialised)
     {
       std::seed_seq rseq{1, 2, 3, 4,  5};
       std::seed_seq iseq{6, 7, 8, 9, 10};
       rgen.seed(rseq);
       igen.seed(iseq);
       rng_initialised = true;
     }

   if (rng_seed_set)
     {
       rgen.seed(rng_seed);
       igen.seed(rng_seed);
       rng_seed_set = false;
     }

  if (opt_A)
     {
       if ((*opt_A)->is_scalar() && B->is_scalar())
          rcnt = (*opt_A)->get_sole_integer();
     }
  else
     {
       if (B->is_scalar() == 0)   rcnt = 1;
     }

  if (rcnt == 0)   RANK_ERROR;

const Cell & Bv = B->get_cfirst();
const APL_Float Bvr = Bv.get_real_value();
const APL_Float Bvi = Bv.get_imag_value();
Value_P Z(rcnt, LOC);
   loop(r, rcnt)
       {
         Dcomplex yy;
         switch(modifier)
            {
              // https://en.cppreference.com/w/cpp/numeric/random
              case 1:   {
                          lognormal_distribution rd{0.0, Bvr};
                          lognormal_distribution id{0.0, Bvi};
                          yy = complex(rd(rgen), id(igen));
                        }
                        break;
                       
              case 2:   {
                          chi_squared_distribution rd{Bvr};
                          chi_squared_distribution id{Bvi};
                          yy = complex(rd(rgen), id(igen));
                        }
                        break;
                       
              case 3:   {
                          student_t_distribution rd{Bvr};
                          student_t_distribution id{Bvi};
                          yy = complex(rd(rgen), id(igen));
                        }
                        break;

              default: {
                         normal_distribution rd{0.0, Bvr};
                         normal_distribution id{0.0, Bvi};
                         yy = complex(rd(rgen), id(igen));
                       }
            }
         
         if (rcnt == 1)   return ComplexScalar(yy.real(), yy.imag(), LOC);

         Z->next_ravel_Complex(yy.real(), yy.imag());
       }

  return Z;
}
//---------------------------------------------------------------------------
Value_P
Quad_MX::norm(const Value_P B)
{
  if (B->is_scalar())
    {
      MORE_ERROR() << "Unexpected scalar argument in ⎕MX.norm.";
      RANK_ERROR;
    }

Value_P Z(B->get_shape(), LOC);
const ShapeItem B_count = B->element_count();
Dcomplex sum(0.0, 0.0);
  loop(c, B_count)
      {
        const Cell & Bv = B->get_cravel(c);
        Dcomplex val(Bv.get_real_value(), Bv.get_imag_value());
        sum += val * val;
      }
      
  sum = sqrt(sum);

  loop(b, B_count)
      {
        const Cell & Bb = B->get_cravel(b);
        Dcomplex val(Bb.get_real_value(), Bb.get_imag_value());
        val /= sum;

        Z->next_ravel_Complex(val.real(), val.imag());
      }

  return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::monadicRotation(Value_P B)
{
  if (!B->is_scalar())
     {
       MORE_ERROR() << "scalar argument expected in ⎕MX.rotation_matrix.";
       RANK_ERROR;
     }

const Cell & B0 = B->get_cravel(0);
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
//----------------------------------------------------------------------------
Value_P
Quad_MX::dyadicRotation(int tp, Value_P A, Value_P B)
{
const ShapeItem A_count   = A->element_count();
const ShapeItem B_count   = B->element_count();
    
  if (!(A->is_vector() && B->is_vector()))
    {
      MORE_ERROR() << "Both arguments must be vectors in ⎕MX.rotation_matrix.";
      RANK_ERROR;
    }

  if (A_count != 3 || B_count != 3)
    {
      MORE_ERROR() << "Both arguments must be vectors "
                      "of ⍴ = 3 in ⎕MX.rotation_matrix.";
      LENGTH_ERROR;
    }

const int sdim = (tp == 9) ? 3 : 4;
Value_P Z(sdim, sdim, LOC);
Dcomplex angles[3];
  loop(i, 3)
      {
        const Cell & Bv = B->get_cravel(i);
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

           Z->next_ravel_Cell(A->get_cravel(0));
           Z->next_ravel_Cell(A->get_cravel(1));
           Z->next_ravel_Cell(A->get_cravel(2));
           Z->next_ravel_Complex(1.0, 0.0);
         }

  Z->check_value(LOC);
  return Z;
}
//----------------------------------------------------------------------------
void
Quad_MX::list_functions(bool mapping)
{
ostream & out = CERR;
  if (mapping)
     {
       // ⎕MX "": print the number to name mappings like:
       //
       // ⎕MX[1]  ←→  ⎕MX['determinant']        ←→  ⎕MX.determinant
       //
       out << "      With a small performance penalty, ⎕MX also accepts the "
                     "following strings\n"
              "      instead of function numbers as axis argument:\n\n";

       loop(c, sizeof(op_desc) / sizeof(*op_desc))
           {
             const fun_info & info = op_desc[c];
             if (*info.desc == 0)   continue;

             char NN[10];   SPRINTF(NN, "%2d", info.code);
             out << "      ⎕MX[" << NN
                 << "]  ←→  ⎕MX['" << info.sub_name << "']"
                 << UCS_string(20 - strlen(info.sub_name), UNI_SPACE)
                 << "←→  ⎕MX." << info.sub_name << endl;
           }
       out << "\n      For a more detailed description of all functions:\n\n"
              "      ⎕MX ⍬" << endl;
     }
  else
     {
       out << "\nValid ⎕MX[*] indices are:\n\n";
       
       loop(c, sizeof(op_desc) / sizeof(*op_desc))
           {
             const fun_info & info = op_desc[c];
             if (*info.desc == 0)   continue;

             char descr[40];
             snprintf(descr, sizeof(descr), "⎕MX[%2u] B", info.code);
      
             out << "    ";
             switch(info.valence)
                {
                  case 1:  out << "    ";   break;
                  case 2:  out << "  A ";   break;
                  case 3:  out << "{A} ";   break;
                  default: FIXME;
                }
             out << descr << "    ";
             switch(info.valence)
                {
                  case 1:  out << "(monadic) ";   break;
                  case 2:  out << "(dyadic)  ";   break;
                  case 3:  out << "(nomadic) ";   break;
                  default: FIXME;
                }
      
             out << info.desc << endl;
           }
       out << "\n    where {A} shall mean that A is optional" << endl;
     }
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_AB(Value_P A, Value_P B) const
{
  return eval_B(B);   // show help
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_B(Value_P B) const
{
  // the only non-axis function is to show help (for now)
  //
  if (B->get_rank() != 1)        RANK_ERROR;
  if (B->element_count() != 0)   LENGTH_ERROR;

  if (B->get_cfirst().is_character_cell())      list_functions(true);
  else if (B->get_cfirst().is_integer_cell())   list_functions(false);
  else                                          DOMAIN_ERROR;

  return Token(TOK_APL_VALUE1, Idx0_0(LOC));
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
MX_ops op = OP_UNKNOWN;
int modifier = 0;

  if (X->is_numeric_scalar())
     {
       op = static_cast<MX_ops>(X->get_sole_integer());
     }
  else if (X->is_vector())
     {
       const ShapeItem X_count = X->element_count();
       op = static_cast<MX_ops>(X->get_cravel(0).get_int_value());
       if (X_count > 1)   modifier = X->get_cravel(1).get_int_value();
     }
  else
    {
      MORE_ERROR() << "Axis X of ⎕MX[X] expects an integer function specifier.";
      SYNTAX_ERROR;
    }
 
  if (op < OP_UNKNOWN || op >= OP_MAX)
     {
       MORE_ERROR() << "Function specifier " << op << " is out of range.";
       AXIS_ERROR;
     }

Value_P Z;
  switch(op)
    {
      case OP_UNKNOWN:            Z = Idx0_0(LOC);
                                  list_functions(false);          break;
      case OP_CROSS_PRODUCT:      Z = dyadicCrossProduct(A, B);   break;
      case OP_VECTOR_ANGLE:       Z = vectorAngle(A, B);          break;
      case OP_HOMOGENEOUS_MATRIX: Z = dyadicRotation(16, A, B);   break;
      case OP_COVARIANCE:         Z = dyadicCovariance(A, B);     break;
      case OP_HISTOGRAM:          Z = histogram(A, B);            break;
      case OP_RANDOMS:            Z = randoms(&A, B, modifier);   break;
      case OP_PRINT:              Z = printit(A, B);              break;
      case OP_DETERMINANT:        // fall through
      case OP_EIGENVECTORS:       // fall through
      case OP_EIGENVALUES:        // fall through
      case OP_IDENT:              // fall through
      case OP_ROTATION_MATRIX:    // fall through
      case OP_NORM:               MORE_ERROR() << "Not a dyadic function.";
                                  SYNTAX_ERROR;                    break;
      default:   MORE_ERROR() << "⎕MX[" << op << "] is not dyadic.";
                 VALENCE_ERROR;
    }

  Z->check_value(LOC);
  return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_XB(Value_P X, Value_P B) const
{
MX_ops op = OP_UNKNOWN;
int modifier = 0;

  if (X->is_numeric_scalar())   // op only
    {
      op = static_cast<MX_ops>(X->get_sole_integer());
    }
  else if (X->is_vector())   // op and modifier
    {
      const ShapeItem X_count = X->element_count();
      op = static_cast<MX_ops>(X->get_cravel(0).get_int_value());
      if (X_count > 1)   modifier = X->get_cravel(1).get_int_value();
    }
  else
    {
      MORE_ERROR() << "Axis X of ⎕MX[X] expects an integer function specifier.";
      SYNTAX_ERROR;
    }

  if (op < OP_UNKNOWN || op >= OP_MAX)
     {
       MORE_ERROR() << "Function specifier " << op << " is out of range.";
       AXIS_ERROR;
    }

Value_P Z = Idx0_0(LOC);
  switch(op)
    {
      case OP_UNKNOWN:            list_functions(false);               break;
      case OP_CROSS_PRODUCT:      Z = monadicCrossProduct(B);          break;
           //v←v,⍉1 100⍴100 ⎕mx[12] 100000 ⎕mx[10 2] 1
      case OP_RANDOMS:            Z = randoms(nullptr, B, modifier);   break;
      case OP_DETERMINANT:        Z = determinant(B);                  break;
      case OP_COVARIANCE:         Z = monadicCovariance(B);            break;
      case OP_NORM:               Z = norm(B);                         break;
      case OP_EIGENVECTORS:       Z = eigenvectors(B);                 break;
      case OP_EIGENVALUES:        Z = eigenvalues(B);                  break;
      case OP_IDENT:              Z = ident(B);                        break;
      case OP_ROTATION_MATRIX:    Z = monadicRotation(B);              break;
      case OP_SET_RNG_SEED:       Z = set_rng_seed(B);                 break;
      default:   MORE_ERROR() << "⎕MX[" << op << "] is not monadic.";
                 VALENCE_ERROR;
    }

  Z->check_value(LOC);
  return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------

#else //====================not apl_GSL =====================================
 
extern Token missing_files(const char * qfun,  const char ** libs,
                           const char ** hdrs, const char ** pkgs);

//----------------------------------------------------------------------------
Token
Quad_MX::eval_AB(Value_P A, Value_P B) const
{
  return eval_B(B);
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_B(Value_P B) const
{
const char * libs[] = { "libgsl.so", "libgslcblas.so", 0 };
const char * hdrs[] = { "gsl_statistics.h", "gsl_math.h", "gsl_eigen.h", 0 };
const char * pkgs[] = { "libgsl-dev", 0 };

   return missing_files("⎕ME", libs, hdrs, pkgs);
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
  return eval_B(B);
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_XB(Value_P X, Value_P B) const
{
  return eval_B(B);
}

#endif   // (not) apl_GSL

//----------------------------------------------------------------------------
int
Quad_MX::axis_compare(const void * key, const void * info)
{
   return strcasecmp(reinterpret_cast<const char *>(key),
                     reinterpret_cast<const fun_info *>(info)->sub_name);
}
//----------------------------------------------------------------------------
sAxis
Quad_MX::subfun_to_axis(const UCS_string & name) const
{
const UTF8_string function_name_utf8(name);
const char * function_name_str = function_name_utf8.c_str();

  // Note: cannot use FUN_INFO_COUNT = FUN_INFO_SIZE / sizeof(op_desc)
  //       since Apple complains with a bogus error.
  enum { FUN_INFO_SIZE  = sizeof(fun_info),
         FUN_INFO_COUNT = OP_MAX
       };

  if (const void * vp = bsearch(function_name_str, op_desc,
                                FUN_INFO_COUNT, FUN_INFO_SIZE, axis_compare))
     {
       // found: vp is a fun_info *
       const fun_info * info = reinterpret_cast<const fun_info *>(vp);
       if (info->valence)   return info->code;
     }

  return -1;    // not found
}
//----------------------------------------------------------------------------
