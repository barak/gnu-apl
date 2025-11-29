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

//----------------------------------------------------------------------------
const FunctionGroup::function_info Quad_MX::subfunction_infos[] =
{
#define mx_def(axis, sub_name, comm_2, valence, enum) \
   { axis, #sub_name, "", comm_2, valence },
#include "Quad_MX.def"
};
//----------------------------------------------------------------------------
Quad_MX::Quad_MX() : QuadFunction(TOK_Quad_MX)
{
enum { count = sizeof(subfunction_infos) / sizeof(*subfunction_infos) };
   init_function_group(subfunction_infos, count, "⎕MX");
}
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
Quad_MX::Dcomplex
Quad_MX::getDet(GSL_Matrix * mtx)
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
        GSL_Matrix * cf = genCofactor(mtx, 0, k);
        const Dcomplex id = getDet(cf);
        delete cf;
        if (sign > 0)   det += mtx->val(0, k) * id;
        else            det -= mtx->val(0, k) * id;
      }

  return det;
}
//----------------------------------------------------------------------------
vector<Quad_MX::Dcomplex>
Quad_MX::getCross(GSL_Matrix *mtx)
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
             GSL_Matrix * cf = genCofactor(mtx, 0, k);
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
   loop(c, v.size())   v[c] = (v[c] - mean) / sdev;
}
//----------------------------------------------------------------------------
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
        MORE_ERROR() << "⎕MX.set_rng_seed B: integer scalar B expected.";
        DOMAIN_ERROR;
      }

   rng_seed = B->get_sole_integer();
   rng_seed_set = true;

   return Idx0_0(LOC);
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::printit(Value_P filename_A, Value_P B)
{
const CellType B_celltype = B->deep_cell_types();

bool close_A = false;
   if (B->is_char_string())
      {
        FILE * ofile = open_file(*filename_A, close_A);
        const UCS_string B_ucs = B->get_UCS_ravel();
        const UTF8_string B_utf(B_ucs);
        const char * B_ccp = B_utf.c_str();
 
        fprintf(ofile, "%s\n", B_ccp);
        if (close_A)   fclose(ofile);
        return Idx0_0(LOC);
      }

   if (!(B_celltype & CT_NUMERIC))
      {
        MORE_ERROR() << "A ⎕MX.print B: B is nut numeric.";
        DOMAIN_ERROR;
      }

const ShapeItem B_count = B->element_count();
const uRank     B_rank  = B->get_rank();
FILE * ofile = open_file(*filename_A, close_A);

   if (B_rank <= 1)   // scalar or vector
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
   else               // matrix or higher rank
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
     
       int * rho = ALLOCA(int, B_rank);
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

   if (close_A)   fclose(ofile);
   return Idx0_0(LOC);
}
//----------------------------------------------------------------------------
FILE *
Quad_MX::open_file(const Value & filename, bool & close_file)
{
   if (filename.is_int_scalar())
      {
        close_file = false;   // leave it open
        return Quad_FIO::get_FILE(filename);
      }

   close_file= true;

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
//----------------------------------------------------------------------------
Value_P
Quad_MX::eigenvectors(Value_P B)
{
   if (B->get_rank() != 2)
      {
        MORE_ERROR() << "⎕MX.eigenvector B: matrix B expected.";
        RANK_ERROR;
      }

const ShapeItem rows = B->get_rows();
const ShapeItem cols = B->get_cols();
    
   if (rows != cols)
      {
        MORE_ERROR() << "⎕MX.eigenvector B: square matrix B expected.";
        LENGTH_ERROR;
      }

GSL_Matrix * mtx = genMtx(B, false);
double * data = ALLOCA(double, mtx->rows() * mtx->cols());
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
        MORE_ERROR() << "⎕MX.eigenvalue B: matrix B expected.";
        RANK_ERROR;
      }
    
const ShapeItem rows = B->get_rows();
const ShapeItem cols = B->get_cols();
    
   if (rows != cols)
      {
        MORE_ERROR() << "⎕MX.eigenvalue B: square matrix B expected.";
        LENGTH_ERROR;
      }

GSL_Matrix * mtx = genMtx(B, false);
double * data = ALLOCA(double, mtx->rows() * mtx->cols());
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
        MORE_ERROR() << "⎕MX.determinant B: matrix B expected.";
        RANK_ERROR;
      }
  
   if (B->get_rows() != B->get_cols())
      {
        MORE_ERROR() << "⎕MX.determinant B: square matrix B expected.";
        LENGTH_ERROR;
      }

GSL_Matrix * mtx = genMtx(B, false);
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
        MORE_ERROR() << "⎕MX.cross_product B: Matrix B expected.";
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
//----------------------------------------------------------------------------
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

GSL_Matrix * mtx = new GSL_Matrix(3, 3);
   loop(c, 3)
       {
         const Cell & Ac = A->get_cravel(c);
         const Cell & Bc = B->get_cravel(c);
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
//----------------------------------------------------------------------------
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
        return ComplexScalar(an.real(), an.imag(), LOC);
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
        MORE_ERROR() << "⎕MX.ident B: scalar B expected.";
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
        MORE_ERROR() << "⎕MX.covariance B: maxtrix B expected.";
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
        MORE_ERROR() << "A ⎕MX.histogram B: scalar A expected.";
        RANK_ERROR;
      }

const int nr_buckets = A->get_sole_integer();
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

double maxv = -MAXFLOAT;
double minv =  MAXFLOAT;
   loop(b, B_count)
       {   
         const Cell & Bb = B->get_cravel(b);
         const double val = Bb.get_real_value();
         if (maxv < val) maxv = val;
         if (minv > val) minv = val;
       }

const double incr = double(maxv - minv) / nr_buckets;
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
   if (!A->is_vector())
      {
        MORE_ERROR() << "A ⎕MX.covariance B: vector A expected.";
        RANK_ERROR;
      }

   if (!B->is_vector())
      {
        MORE_ERROR() << "A ⎕MX.covariance B: vector B expected.";
        RANK_ERROR;
      }

const ShapeItem AB_count = A->element_count();
   if (AB_count != B->element_count())
      {
        MORE_ERROR() << "A ⎕MX.covariance B: ⍴A ≠ ⍴B.";
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
Quad_MX::randoms(Value_P opt_A, Value_P B, int dist)
{
int rcnt = 0;   // assume error

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
       rng_seed_set = false;   // re-seed on next call
     }

   if (+opt_A)   // optional A provided
      {
        if (opt_A->is_scalar() && B->is_scalar())
           rcnt = opt_A->get_sole_integer();
      }
   else
      {
        if (B->is_scalar() == 0)   rcnt = 1;
      }

   if (rcnt == 0)   RANK_ERROR;



const Cell & B0 = B->get_cfirst();
const APL_Float B0_real = B0.get_real_value();
const APL_Float B0_imag = B0.get_imag_value();

Shape shape_Z;   // scalar
   if (rcnt > 1)   shape_Z.add_shape_item(rcnt);
Value_P Z(shape_Z, LOC);
   loop(r, rcnt)
       {
         Dcomplex yy;
         switch(dist)
            {
              // https://en.cppreference.com/w/cpp/numeric/random

              case 0:  {
                         normal_distribution real_dist{0.0, B0_real};
                         normal_distribution imag_dist{0.0, B0_imag};
                         yy = complex(real_dist(rgen), imag_dist(igen));
                       }
                        break;

              case 1:   {
                          lognormal_distribution real_dist{0.0, B0_real};
                          lognormal_distribution imag_dist{0.0, B0_imag};
                          yy = complex(real_dist(rgen), imag_dist(igen));
                        }
                        break;

              case 2:   {
                          chi_squared_distribution real_dist{B0_real};
                          chi_squared_distribution imag_dist{B0_imag};
                          yy = complex(real_dist(rgen), imag_dist(igen));
                        }
                        break;

              case 3:   {
                          student_t_distribution real_dist{B0_real};
                          student_t_distribution imag_dist{B0_imag};
                          yy = complex(real_dist(rgen), imag_dist(igen));
                        }
                        break;

              default: MORE_ERROR() << "⎕MX[10, D] B: invalid distribution D";
                       DOMAIN_ERROR;

            }

         Z->next_ravel_Complex(yy.real(), yy.imag());
       }

  return Z;
}
//---------------------------------------------------------------------------
Value_P
Quad_MX::norm(const Value_P B)
{
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
        MORE_ERROR() << "⎕MX.rotation_matrix B: scalar B expected.";
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
    
   if (!A->is_vector())
      {
        MORE_ERROR() << "A ⎕MX.rotation_matrix B: vector A expected.";
        RANK_ERROR;
      }

   if (!B->is_vector())
      {
        MORE_ERROR() << "A ⎕MX.rotation_matrix B: vector B expected.";
        RANK_ERROR;
      }

   if (A_count != 3 || B_count != 3)
      {
        MORE_ERROR() << "A ⎕MX.rotation_matrix B: ⍴A ≠ ⍴B.";
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
//----------------------------------------------------------------------------
void
Quad_MX::print_map_syntax(ostream & out,
                          const FunctionGroup::function_info & info) const
{
const UTF8_literal name = info.function_name;
const int blanks = max_function_name_length - name.get_char_count();

   out << "    ⎕MX[" << setw(3) << info.axis << "]  ←→  ⎕MX['" << name << "']"
       << UCS_string(blanks, UNI_SPACE) << "  ←→  ⎕MX." << name << endl;
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
   if (B->is_str0())    return list_functions(CERR);
   if (B->is_zilde())   return list_mappings(CERR);

   if (!B->is_vector())      RANK_ERROR;
   if (B->element_count())   LENGTH_ERROR;   // not empty
  DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
MX_ops op    = OP_LIST;
int modifier = 0;

   if (X->is_int_scalar() || X->is_char_string())   // op only
      {
        op = MX_ops(value_to_subfun(*X));
        if (op >= 100 && op <= 104)
           {
             op = OP_RANDOMS;
             modifier = op - 100;
           } 
      }
   else if (X->is_vector())                         // op and modifier
      {
        const ShapeItem X_count = X->element_count();
        op = MX_ops(X->get_cravel(0).get_int_value());
        if (X_count > 1)   modifier = X->get_cravel(1).get_int_value();
      }
   else
      {
        MORE_ERROR() << "A ⎕MX[X] B: integer or string X expected.";
        DOMAIN_ERROR;
      }
  
   if (op < OP_LIST || int(op) >= subfun_count)   bad_subfun_number_ERROR(op);

Value_P Z;
  switch(op)
    {
      case OP_LIST:
           if (B->is_str0())    return list_functions(CERR);
           if (B->is_zilde())   return list_mappings(CERR);
           DOMAIN_ERROR;
           break;

      case OP_CROSS_PRODUCT:      Z = dyadicCrossProduct(A, B);   break;
      case OP_VECTOR_ANGLE:       Z = vectorAngle(A, B);          break;
      case OP_HOMOGENEOUS_MATRIX: Z = dyadicRotation(16, A, B);   break;
      case OP_COVARIANCE:         Z = dyadicCovariance(A, B);     break;
      case OP_HISTOGRAM:          Z = histogram(A, B);            break;
      case OP_RANDOMS:            Z = randoms(A, B, modifier);    break;
      case OP_PRINT:              Z = printit(A, B);              break;

      default: MORE_ERROR() << "⎕MX[" << op << "] B is monadic.";
               VALENCE_ERROR;
    }

  Z->check_value(LOC);
  return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_XB(Value_P X, Value_P B) const
{
MX_ops op    = OP_LIST;
int modifier = 0;

  if (X->is_int_scalar() || X->is_char_string())   // op only
     {
       op = MX_ops(value_to_subfun(*X));
     }
  else if (X->is_vector())                         // op and modifier
     {
       const ShapeItem X_count = X->element_count();
       op = MX_ops(X->get_cravel(0).get_int_value());
        if (X_count > 1)   modifier = X->get_cravel(1).get_int_value();
      }
   else
      {
        MORE_ERROR() << "⎕MX[X] B: integer or string X expected.";
        SYNTAX_ERROR;
      }

   if (op < OP_LIST || int(op) >= subfun_count)   bad_subfun_number_ERROR(op);

Value_P opt_A;   // = 0
Value_P Z = Idx0_0(LOC);
   switch(op)
      {
        case OP_LIST:
             if (B->is_str0())   return  list_functions(COUT);
             if (B->is_zilde())   return list_mappings(COUT);
             DOMAIN_ERROR;
       
        case OP_CROSS_PRODUCT:      Z = monadicCrossProduct(B);          break;
             //v←v,⍉1 100⍴100 ⎕mx[12] 100000 ⎕mx[10 2] 1
        case OP_RANDOMS:            Z = randoms(opt_A, B, modifier);     break;
        case OP_DETERMINANT:        Z = determinant(B);                  break;
        case OP_COVARIANCE:         Z = monadicCovariance(B);            break;
        case OP_NORM:               Z = norm(B);                         break;
        case OP_EIGENVECTORS:       Z = eigenvectors(B);                 break;
        case OP_EIGENVALUES:        Z = eigenvalues(B);                  break;
        case OP_IDENT:              Z = ident(B);                        break;
        case OP_ROTATION_MATRIX:    Z = monadicRotation(B);              break;
        case OP_SET_RNG_SEED:       Z = set_rng_seed(B);                 break;
        default:   MORE_ERROR() << "A ⎕MX[" << op << "] B is dyadic.";
                   VALENCE_ERROR;
      }

  Z->check_value(LOC);
  return Token(TOK_APL_VALUE1, Z);
}

#else //====================not apl_GSL =====================================
 
Quad_MX::Quad_MX()
  : QuadFunction(TOK_Quad_MX)
{
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

   return missing_files("⎕MX", libs, hdrs, pkgs);
}
//----------------------------------------------------------------------------
void
Quad_MX::print_fun_syntax(ostream & out,
                          const FunctionGroup::function_info & info) const
{
}
//----------------------------------------------------------------------------
void
Quad_MX::print_map_syntax(ostream & out,
                          const FunctionGroup::function_info & info) const
{
}
//----------------------------------------------------------------------------

#endif   // (not) apl_GSL

