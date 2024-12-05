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

#if apl_GSL
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_eigen.h>

/************** start Matrix class **************/
/****** gsl matrices don't support complex ******/

class Matrix
{
public:
  Matrix(int r, int c)
  {
    krows = r;
    kcols = c;
    vals = new vector<complex<double> >(r * c);
  }
    
  ~Matrix()
  {
    delete vals;
  }

  complex<double> val(int r, int c)
  {
    complex<double>rc(0.0, 0.0);
    rc = (*vals)[c + r * kcols];
    return rc;
  }

  void val(int r, int c, complex<double> v)
  {
    (*vals)[c + r * kcols] = v;
  }

  int  rows()
  {
    return krows;
  }

  int  cols()
  {
    return kcols;
  }

  void show()
  {
    for (int r = 0; r < krows; r++)
      {
    for (int c = 0; c < kcols; c++)
      {
        fprintf(stderr, "%gj%g ",
             this->val(r, c).real(),
             this->val(r, c).imag());
      }
    fprintf(stderr, "\n");
      }
}

  
private:
  int krows;
  int kcols;
  vector<complex<double> > *vals;
};

/************** end Matrix class **************/

/*************** start utility fcns **************/

Quad_MX::Matrix *
Quad_MX::genCofactor(Matrix *mtx, int r, int c)
{
Matrix * cf = new Matrix(mtx->rows()-1, mtx->cols()-1);

  int rx = 0;
  for (int rr = 0; rr < mtx->rows(); rr++)
    {
      if (rr != r)
    {
      int cx = 0;
      for (int cc = 0; cc < mtx->cols (); cc++)
        {
          if (cc != c)
        cf->val(rx, cx++, mtx->val(rr, cc));
        }
      rx++;
    }
    }

  return cf;
}

//----------------------------------------------------------------------------
complex<double>
Quad_MX::getDet(Matrix * mtx)
{
complex<double> det(0.0, 0.0);
  if (mtx->rows() == 2)
     {
       det = (mtx->val(0, 0) * mtx->val(1,1)) -
             (mtx->val(0, 1) * mtx->val(1,0));
     }
  else
     {
       for (int k = 0; k < mtx->rows(); k++) {
         double sign = pow(-1.0, (double)(k));
         Matrix *cf = genCofactor(mtx, 0, k);
         complex<double> id = getDet(cf);
         delete cf;
         det += sign * mtx->val(0, k) * id;
     }
  }

  return det;
}
//----------------------------------------------------------------------------

vector<complex<double> >
Quad_MX::getCross(Matrix *mtx)
{
  complex<double> det(0.0, 0.0);
  vector<complex<double> > rc(mtx->cols());
  if (mtx->rows() == 2) {
    det = (mtx->val(0, 0) * mtx->val(1,1)) -
      (mtx->val(0, 1) * mtx->val(1,0));
  }
  else
    {
      for (int k = 0; k < mtx->rows(); k++)
    {
      double sign = pow(-1.0, (double)(k));
      Matrix *cf = genCofactor(mtx, 0, k);
      complex<double> id = getDet(cf);
      delete cf;
      rc[k] = sign * id;
    }
    }

  return rc;
}
//----------------------------------------------------------------------------
complex<double>
Quad_MX::magnitude(vector<complex<double> > &v)
{
  complex<double> rc(0.0, 0.0);
  loop(i, v.size()) rc += v[i] * v[i];
  rc = sqrt(rc);
  return rc;
}
//----------------------------------------------------------------------------
void
Quad_MX::normalise(vector<double> &v)
{
  double mean = gsl_stats_mean(v.data(), 1, v.size());
  double sdev = gsl_stats_sd_m(v.data(), 2, v.size(), mean);
  loop(c, v.size())
    v[c] = (v[c] - mean) / sdev;
}
//----------------------------------------------------------------------------
Quad_MX::Matrix *
Quad_MX::genMtx(Value_P B, bool padded)
{
  ShapeItem rows = B->get_shape_item(0);
  ShapeItem cols = B->get_shape_item(1);

  if (padded) rows++;

  Matrix *mtx = new Matrix(rows, cols);
  int p = 0;
  loop(r, rows)
    {
      loop(c, cols)
    {
      if (padded && r == 0)
        {
           mtx->val(0, c, complex(1.0, 0.0));
        }
      else {
        const Cell & Bv = B->get_cravel(p++);
        APL_Float xvr = Bv.get_real_value();
        APL_Float xvi = Bv.get_imag_value();
        mtx->val(r, c, complex(xvr, xvi));
      }
    }
    }

  return mtx;
}


/*************** end utility fcns **************/

//----------------------------------------------------------------------------

static bool rng_seed_set = false;
static unsigned int rng_seed;

Value_P
Quad_MX::set_rng_seed(Value_P B)
{
Value_P rc = Str0(LOC);
  const CellType B_celltype = B->deep_cell_types();

  if (B_celltype & CT_INT &&
      B->get_rank() == 0)
    {
      rng_seed_set = true;
      rng_seed = (unsigned int)(B->get_sole_integer());
    }
  else
    {
      MORE_ERROR () << "Requires numeric arguement.";
      DOMAIN_ERROR;
    }
  return rc;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::printit(Value_P A, Value_P B)
{
  Value_P rc = Str0(LOC);
  const CellType A_celltype = A->deep_cell_types();


  if (A->is_char_string() && B->is_char_string())
    {
      const UCS_string ustr = B->get_UCS_ravel();
      UTF8_string fn(ustr);
      char *fns = (char *)(fn.c_str());
      bool append = false;
      if (strlen(fns) > 1)
    {
          if (*fns == '>')
            {
              fns++;
              append = true;
            }
    }
FILE * ofile = fopen(fns, append ? "a" : "w");

const UCS_string vstr = A->get_UCS_ravel();
UTF8_string val(vstr);
char *vs = (char *)(val.c_str());

      fprintf(ofile, "%s\n", vs);
      fclose(ofile);

      return rc;
    }
  if ((A_celltype & CT_NUMERIC) &&
      B->is_char_string())
    {
      const ShapeItem A_count   = A->element_count();
      const uRank     A_rank    = A->get_rank();
      const UCS_string  ustr = B->get_UCS_ravel();
      UTF8_string fn(ustr);
      char *fns = (char *)(fn.c_str());
      bool append = false;
      if (strlen(fns) > 1)
    {
      if (*fns == '>') {
        fns++;
        append = true;
      }
    }
      FILE *ofile = append ?
        fopen(fns, "a") :
        fopen(fns, "w");

      if (!ofile) {
        MORE_ERROR() << "Open failure on " << ustr;
        DOMAIN_ERROR;
      }
      if (A_rank <= 1)
    {
        for (int i = 0; i < A_count; i++)
      {
        const Cell & Av = A->get_cravel(i);
        const APL_Float Avr = Av.get_real_value();
        if (Av.is_complex_cell())
          {
            const APL_Float Avi = Av.get_imag_value();
            fprintf(ofile, "%gj%g ", Avr, Avi);
          }
        else
          fprintf(ofile, "%g ", Avr);
      }
        fprintf(ofile, "\n");
    }
      else
    {
      int end_line = (int)(A->get_shape_item(A_rank-1));
      int end_grid = end_line * (int)(A->get_shape_item(A_rank-2));
#define STR_LEN 256
      char str[STR_LEN];
      bool is_cpx = false;
      int max_len = -1;
      for (int i = 0; i < A_count; i++)
        {
          int len;
          const Cell & Av = A->get_cravel(i);
          const APL_Float Avr = Av.get_real_value();
          if (Av.is_complex_cell())
             {
               const APL_Float Avi = Av.get_imag_value();
               len = snprintf(str, STR_LEN, "%gj%g", Avr, Avi);
               if (Avi != 0.0) is_cpx = true;
             }
          else
             len = snprintf(str, STR_LEN, "%g", Avr);
          if (max_len < len) max_len = len;
        }
      int *rho = (int *)alloca(A_rank * sizeof(int));
      bzero(rho, A_rank * sizeof(int));
      bzero(rho, A_rank * sizeof(int));

      for (int i = 0; i < A_count; i++)
        {
          if (A_rank > 2)
        {
          if (0 == i%end_grid) {
            fprintf(ofile, "\n[");
            for (uRank j = 0; j < A_rank - 2; j++)
              fprintf(ofile, "%d ", rho[j]);
            fprintf(ofile, "* *]:\n");
            bool carry = 1;
            for (int j = A_rank - 3; j >= 0; j--)
              {
            rho[j] += carry;
            if (rho[j] >= A->get_shape_item(j))
              {
                rho[j] = 0;
                carry = 1;
              }
            else
              carry = 0;
              }
          }
        }
          const Cell & Av = A->get_cravel(i);
          const APL_Float Avr = Av.get_real_value();
          char str[STR_LEN];
          if (is_cpx && Av.is_complex_cell())
             {
               const APL_Float Avi = Av.get_imag_value();
               snprintf(str, STR_LEN, "%gj%g", Avr, Avi);
             }
          else
             snprintf(str, STR_LEN, "%g", Avr);
          fprintf(ofile, "%*s ", max_len, str);
          if (0 == (i+1)%end_line) fprintf(ofile, "\n");
        }
    }

      fclose(ofile);
      return rc;
    }
  else
    {
      MORE_ERROR() << "Incompatible arguments.";
      DOMAIN_ERROR;
    }

  return rc;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::eigenvectors(Value_P B)
{
Value_P rc = Str0(LOC);
  
const uRank B_rank = B->get_rank();
  if (B_rank == 2) {
    Matrix *mtx = genMtx(B, false);
    
ShapeItem rows = B->get_shape_item(0);
ShapeItem cols = B->get_shape_item(1);
    
    if (rows == cols)
      {
    double *data =
      (double *)alloca(mtx->rows() * mtx->cols() * sizeof(double));
    int i = 0;
    for (int j = 0; j < mtx->rows(); j++)
      {
        for (int k = 0; k < mtx->cols(); k++, i++)
          {
        data[i] = mtx->val(j, k).real();
          }
      }

    gsl_matrix_view m =
      gsl_matrix_view_array(data, mtx->rows(), mtx->cols());

    gsl_vector_complex *eval = gsl_vector_complex_alloc(mtx->rows());
    gsl_matrix_complex *evec =
          gsl_matrix_complex_alloc(mtx->rows(), mtx->cols());

    gsl_eigen_nonsymmv_workspace *w =
      gsl_eigen_nonsymmv_alloc(mtx->rows());
    int erc = GSL_FAILURE;
    erc = gsl_eigen_nonsymmv(&m.matrix, eval, evec, w);
    if (erc == GSL_SUCCESS)
      {
        gsl_eigen_nonsymmv_free(w);
        erc = gsl_eigen_nonsymmv_sort(eval, evec, GSL_EIGEN_SORT_ABS_DESC);
      }

    if (erc == GSL_SUCCESS)
      {
        Shape shape_Z;
        shape_Z.add_shape_item(rows);
        shape_Z.add_shape_item(cols);
        rc = Value_P(shape_Z, LOC);

        int k = 0;
        for (int c = 0; c < mtx->cols(); c++)
          {
        gsl_vector_complex_view evec_c =
          gsl_matrix_complex_column(evec, c);
        for (int r = 0; r < mtx->rows(); r++)
          {
            gsl_complex z = gsl_vector_complex_get(&evec_c.vector, r);
            (*rc).set_ravel_Complex(k++, GSL_REAL(z), GSL_IMAG (z));
          }
          }
      }
    else
      {
        MORE_ERROR () << "Eigensystem computation error.";
        INTERNAL_ERROR;
      }
      }
    else
      {
    MORE_ERROR () << "Non-square matrix.";
    RANK_ERROR;
      }
    delete mtx;
  }
  else
    {
      MORE_ERROR () << "Invalid rank.";
      RANK_ERROR;
    }

  return rc;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::eigenvalues(Value_P B)
{
Value_P rc = Str0(LOC);
  
const uRank      B_rank    = B->get_rank();
  if (B_rank == 2) {
    Matrix *mtx = genMtx (B, false);
    
ShapeItem rows = B->get_shape_item(0);
ShapeItem cols = B->get_shape_item(1);
    
    if (rows == cols)
      {
    double *data =
      (double *)alloca (mtx->rows () * mtx->cols () * sizeof(double));
    int i = 0;
    for (int j = 0; j < mtx->rows (); j++)
      {
        for (int k = 0; k < mtx->cols (); k++, i++)
          {
        data[i] = mtx->val (j, k).real ();
          }
      }

    gsl_matrix_view m =
      gsl_matrix_view_array (data, mtx->rows (), mtx->cols ());

    gsl_vector_complex *eval = gsl_vector_complex_alloc (mtx->rows ());
    gsl_matrix_complex *evec =
          gsl_matrix_complex_alloc (mtx->rows (), mtx->cols ());

    gsl_eigen_nonsymmv_workspace *w =
      gsl_eigen_nonsymmv_alloc (mtx->rows ());
    int erc = GSL_FAILURE;
    erc = gsl_eigen_nonsymmv (&m.matrix, eval, evec, w);
    if (erc == GSL_SUCCESS)
      {
        gsl_eigen_nonsymmv_free (w);
        erc = gsl_eigen_nonsymmv_sort (eval, evec, GSL_EIGEN_SORT_ABS_DESC);
      }

    if (erc == GSL_SUCCESS)
      {
        Shape shape_Z;
        shape_Z.add_shape_item(cols);
        rc = Value_P (shape_Z, LOC);
    
        for (int i = 0; i < mtx->cols (); i++)
          {
        gsl_complex eval_i = gsl_vector_complex_get (eval, i);

        complex<double> v (GSL_REAL(eval_i), GSL_IMAG(eval_i));
        (*rc).set_ravel_Complex (i, v.real (), v.imag ());
          }
        rc->check_value(LOC);
      }
    else
      {
        MORE_ERROR () << "Eigensystem computation error.";
        INTERNAL_ERROR;
      }
    delete mtx;
      }
    else
      {
    MORE_ERROR () << "Non-square matrix.";
    RANK_ERROR;
      }
  }
  else
    {
      MORE_ERROR () << "Invalid rank.";
      RANK_ERROR;
    }

  return rc;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::determinant(Value_P B)
{
const uRank B_rank = B->get_rank();

  if (B_rank != 2)
    {
      MORE_ERROR () << "Invalid rank (matrix expected)"
                       " in Quad_MX::determinant().";
      RANK_ERROR;
    }

  if (B->get_shape_item(0) != B->get_shape_item(1))
     {
       MORE_ERROR () << "Non-square matrix in Quad_MX::determinant()";
       RANK_ERROR;
     }

Matrix * mtx = genMtx(B, false);
const complex<double>det = getDet(mtx);
  delete mtx;

Value_P Z(det.imag () == 0.0 ? FloatScalar(det.real(), LOC)
                             : ComplexScalar(det.real(), det.imag(), LOC));
  Z->check_value(LOC);
  return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::monadicCrossProduct(Value_P B)
{
Value_P rc = Str0(LOC);
  
  //  const ShapeItem B_count   = B->element_count();
const uRank B_rank = B->get_rank();

  if (B_rank == 2)
    {
      ShapeItem rows = B->get_shape_item(0);
      ShapeItem cols = B->get_shape_item(1);
    if (rows+1 == cols)
      {
    Matrix *mtx = genMtx (B, true);
    vector<complex<double> > cp = getCross (mtx);
    Shape shape_Z;
    shape_Z.add_shape_item(mtx->cols ());
    rc = Value_P (shape_Z, LOC);
    bool is_cpx = false;
    for (int i = 0; i < mtx->cols (); i++)
      {
        if (cp[i].imag () != 0.0)
          {
                is_cpx = true;
                break;
              }
      }
    for (int i = 0; i < mtx->cols (); i++)
      {
        if (is_cpx)
          (*rc).set_ravel_Complex (i, cp[i].real (), cp[i].imag ());
        else
          (*rc).set_ravel_Float (i, cp[i].real ());
      }
    delete mtx;
    rc->check_value(LOC);

      }
    else
      {
    MORE_ERROR () <<
      "For cross product, the shape of the argument must be [n-1 n";
    RANK_ERROR;
      }
    }
  else
    {
      MORE_ERROR () << "Invalid rank in Quad_MX::monadicCrossProduct().";
      RANK_ERROR;
    }
  

  return rc;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::dyadicCrossProduct(Value_P A, Value_P B)
{
const ShapeItem A_count = A->element_count();
const uRank     A_rank  = A->get_rank();
const ShapeItem B_count = B->element_count();
const uRank     B_rank  = B->get_rank();

  if (A_rank != 1 || B_rank != 1 || A_count != B_count || A_count != 3)
    {
      MORE_ERROR () << "Invalid rank in Quad_MX::dyadicCrossProduct().";
      RANK_ERROR;
    }

Matrix *mtx = new Matrix (3, 3);
  loop (c, 3)
       {
         const Cell & Av = A->get_cravel(c);
         const Cell & Bv = B->get_cravel(c);
         APL_Float Avr = Av.get_real_value();
         APL_Float Avi = Av.get_imag_value();
         APL_Float Bvr = Bv.get_real_value ();
         APL_Float Bvi = Bv.get_imag_value();
         mtx->val (0, c, complex (1.0, 0.0));
         mtx->val (1, c, complex (Avr, Avi));
         mtx->val (2, c, complex (Bvr, Bvi));
       }

vector<complex<double> > cp = getCross (mtx);
bool is_cpx = false;
  loop(i, mtx->cols())
      {
        if (cp[i].imag () != 0.0)   { is_cpx = true; break; }
      }

const Shape shape_Z(mtx->cols());
Value_P Z(shape_Z, LOC);
  loop(i, mtx->cols())
      {
        if (is_cpx)   Z->next_ravel_Complex(cp[i].real(), cp[i].imag());
        else          Z->next_ravel_Float(cp[i].real());
      }
  Z->check_value(LOC);
  delete mtx;

  return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::vectorAngle(const Value_P A, const Value_P B)
{
Value_P rc = Str0(LOC);

  const ShapeItem A_count = A->element_count();
  const uRank     A_rank  = A->get_rank();  
  const ShapeItem B_count = B->element_count();
  const uRank     B_rank  = B->get_rank();

  if (A_rank == 1 && B_rank == 1 && A_count == B_count)
    {
      vector<complex<double> > Av (A_count);
      vector<complex<double> > Bv (B_count);
      loop (c, A_count)
    {
      const Cell & Ac = A->get_cravel (c);
      const Cell & Bc = B->get_cravel (c);
      APL_Float Avr = Ac.get_real_value();
      APL_Float Avi = Ac.get_imag_value();
      APL_Float Bvr = Bc.get_real_value();
      APL_Float Bvi = Bc.get_imag_value();
      Av[c] = complex<double> (Avr, Avi);
      Bv[c] = complex<double> (Bvr, Bvi);
    }
      complex<double> Amag = magnitude (Av);
      complex<double> Bmag = magnitude (Bv);
      complex<double> mag = Amag * Bmag;
      if (mag != complex<double>(0.0, 0.0))
    {
      complex<double> dp (0.0, 0.0);
      loop (i, Av.size ()) dp += Av[i] * Bv[i];
      complex<double> an = acos (dp/mag);
      rc = ComplexScalar((APL_Float)an.real (), an.imag (), LOC);
    }
      else
    {
      MORE_ERROR () << "Invalid vector(s).";
      DOMAIN_ERROR;
    }
    }
  else {
    MORE_ERROR () << "Vector angle arguments must be conformable vectors.";
    DOMAIN_ERROR;
  }

  rc->check_value(LOC);
  return rc;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::ident(const Value_P B)
{
  Value_P rc = Str0(LOC);
  
const uRank B_rank = B->get_rank();

  if (B_rank == 0)
    {
      int dim = B->get_sole_integer();
      Shape shape_W;
      shape_W.add_shape_item (dim);
      shape_W.add_shape_item (dim);
      rc = Value_P (shape_W, LOC);
      int p = 0;
      for (int i = 0; i < dim; i++)
    {
      for (int j = 0; j < dim; j++, p++)
        {
          (*rc).set_ravel_Complex (p,
                       ((i == j) ? 1.0 : 0.0),
                       0.0);
            }
    }
    }

  rc->check_value(LOC);
  return rc;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::monadicCovariance(const Value_P B)
{
Value_P rc = Str0(LOC);

const uRank B_rank = B->get_rank();

  if (B_rank == 2)
    {
      ShapeItem rows = B->get_shape_item(0);
      ShapeItem cols = B->get_shape_item(1);

      if (1 || rows == cols)
    {
      vector<vector<double> > Breals (rows);
      vector<vector<double> > Bimags (rows);
      loop (r, rows)
        {
              Breals[r].reserve (cols);
              Bimags[r].reserve (cols);
              int offset = r * cols;
              loop (c, cols) {
                const Cell & Bv = B->get_cravel (offset + c);
                Breals[r][c] = Bv.get_real_value();
                Bimags[r][c] = Bv.get_imag_value();
              }
              normalise (Breals[r]);
              normalise (Bimags[r]);
            }
            Shape shape_Z;
            shape_Z.add_shape_item (rows * rows);
            rc = Value_P (shape_Z, LOC);
            for (int r = 0; r < rows; r++)
          {
        for (int c = r; c < rows; c++)
          {
            double realcov =
              gsl_stats_covariance (Breals[r].data (), 1,
                        Breals[c].data (), 1,
                        cols);
            double imagcov =
              gsl_stats_covariance (Bimags[r].data (), 1,
                        Bimags[c].data (), 1,
                        cols);
            int ix = (r * rows) + c;
            (*rc).set_ravel_Complex (ix,   realcov, imagcov);
            if (r != c)
              {
            int ix = (c * rows) + r;
            (*rc).set_ravel_Complex (ix,   realcov, imagcov);
              }
          }
          }
            rc->check_value(LOC);
            Shape shape_W;
            shape_W.add_shape_item(rows);
            shape_W.add_shape_item(rows);
            (*rc).set_shape (shape_W);
    }
      else {
    MORE_ERROR () << "Not a square matrix.";
    RANK_ERROR;
      }
    }
  else {
    MORE_ERROR () << "Not a matrix.";
    RANK_ERROR;
  }

  rc->check_value(LOC);
  return rc;
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
Value_P rc = Str0(LOC);

const uRank     A_rank  = A->get_rank();
const ShapeItem B_count = B->element_count();
const uRank     B_rank  = B->get_rank();

  int nr_buckets = 0;

  if (A_rank == 0)
    {
      nr_buckets = A->get_sole_integer ();

      if (nr_buckets >= 2)
    {
      if (B_rank == 1)
        {
          double maxv = -MAXFLOAT;
          double minv =  MAXFLOAT;
          loop (c, B_count)
        {   
          const Cell & Bv = B->get_cravel (c);
          double val = Bv.get_real_value ();
          if (maxv < val) maxv = val;
          if (minv > val) minv = val;
        }
          double incr = (maxv - minv) / (double)nr_buckets;
          vector<int> buckets (nr_buckets);
          int nr_bumps = 0;
          loop (c, B_count)
        {   
          const Cell & Bv = B->get_cravel (c);
          double val = Bv.get_real_value ();
          double mark = minv + incr;
          for (int b = 0; b < nr_buckets - 1; b++, mark += incr)
            {
              if (val < mark)
            {
              buckets[b]++;
              nr_bumps++;
              break;
            }
            }
          buckets[nr_buckets - 1] = B_count - nr_bumps;
        }
          Shape shape_W;
          shape_W.add_shape_item (nr_buckets);
          rc = Value_P (shape_W, LOC);
          loop (b, nr_buckets)
        {
          (*rc).set_ravel_Int (b,  buckets[b]);
        }
          rc->check_value(LOC);
        }
      else
        {
          MORE_ERROR () << "Non-vector argument.";
          RANK_ERROR;
        }
    }
      else
    {
      MORE_ERROR () << "Too few buckets.";
      DOMAIN_ERROR;
    }
    }
  else
    {
      MORE_ERROR () << "Non-scalar argument.";
      RANK_ERROR;
    }

  rc->check_value(LOC);
  return rc;
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::dyadicCovariance(const Value_P A, const Value_P B)
{
Value_P Z = Str0(LOC);

const ShapeItem A_count = A->element_count();
const ShapeItem B_count = B->element_count();
  
  if (!(A->is_vector() && B->is_vector()))
    {
      MORE_ERROR () << "Both arguments must be vectors.";
      RANK_ERROR;
    }

  if (A_count != B_count)
    {
      MORE_ERROR () << "Arguments must be of the same length.";
      LENGTH_ERROR;
    }

vector<double> Areals(A_count);
vector<double> Aimags(A_count);
vector<double> Breals(B_count);
vector<double> Bimags(B_count);
  loop (ab, A_count)
    {
      const Cell & Av = A->get_cravel(ab);
      const Cell & Bv = B->get_cravel(ab);
      Areals[ab] = Av.get_real_value();
      Aimags[ab] = Av.get_imag_value();
      Breals[ab] = Bv.get_real_value();
      Bimags[ab] = Bv.get_imag_value();
    }

const double realcov =
    gsl_stats_covariance(Areals.data(), 1, Breals.data(), 1, A_count);
const double imagcov =
    gsl_stats_covariance(Aimags.data(), 1, Bimags.data(), 1, A_count);
  return imagcov == 0.0 ? FloatScalar(realcov, LOC) :
                          ComplexScalar(realcov, imagcov, LOC);
}
//----------------------------------------------------------------------------
Value_P
Quad_MX::randoms(const Value_P *A, const Value_P B, int modifier)
{
Value_P Z = Str0(LOC);
int rcnt = 0;

static mt19937_64 rgen;
static mt19937_64 igen;
static bool rng_initialised = false;
  
   if (!rng_initialised)
     {
       std::seed_seq rseq{1, 2, 3, 4, 5};
       std::seed_seq iseq{6, 7, 8, 9, 10};
       rgen.seed (rseq);
       igen.seed (iseq);
       rng_initialised = true;
     }

   if (rng_seed_set)
     {
       rgen.seed(rng_seed);
       igen.seed(rng_seed);
       rng_seed_set = false;
     }

const uRank B_rank = B->get_rank();
  if (A)
    {
      const uRank A_rank = (*A)->get_rank();
      if (A_rank == 0 && B_rank == 0)
    rcnt = (*A)->get_sole_integer ();
    }
  else
    {
      if (B_rank == 0)   rcnt = 1;
    }

  if (rcnt == 0)   RANK_ERROR;

const Cell & Bv = B->get_cravel (0);
APL_Float Bvr = Bv.get_real_value ();
APL_Float Bvi = Bv.get_imag_value();
   if (rcnt > 1)
      {
        Shape shape_Z;
        shape_Z.add_shape_item (rcnt);
        Z = Value_P(shape_Z, LOC);
      }

   loop(xx, rcnt)
       {
         complex<double> yy;
         switch(modifier)
            {
              // https://en.cppreference.com/w/cpp/numeric/random
              case 1:
                {
                  lognormal_distribution rd{0.0, Bvr};
                  lognormal_distribution id{0.0, Bvi};
                  yy = complex (rd(rgen), id(igen));
                }
                break;
              case 2:
                {
                  chi_squared_distribution rd{Bvr};
                  chi_squared_distribution id{Bvi};
                  yy = complex (rd(rgen), id(igen));
                }
                break;
              case 3:
                {
                  student_t_distribution rd{Bvr};
                  student_t_distribution id{Bvi};
                  yy = complex (rd(rgen), id(igen));
                }
                break;
              default:
                {
                  normal_distribution rd{0.0, Bvr};
                  normal_distribution id{0.0, Bvi};
                  yy = complex(rd(rgen), id(igen));
                }
                break;
            }
         
         if (rcnt > 1)
           Z->next_ravel_Complex(yy.real(), yy.imag());
         else
           Z = ComplexScalar(yy.real(), yy.imag(), LOC);
       }

  return Z;
}
//---------------------------------------------------------------------------
Value_P
Quad_MX::norm(const Value_P B)
{
  if (B->is_scalar())
    {
      MORE_ERROR () << "invalid scalar argument.";
      RANK_ERROR;
    }

Value_P Z(B->get_shape(), LOC);
const ShapeItem B_count = B->element_count();
complex<double> sum(0.0, 0.0);
  loop (c, B_count)
    {
      const Cell & Bv = B->get_cravel (c);
      complex<double> val(Bv.get_real_value(), Bv.get_imag_value());
      sum += val * val;
    }
      
  sum = sqrt(sum);

  loop (b, B_count)
    {
      const Cell & Bv = B->get_cravel(b);
      complex<double> val(Bv.get_real_value(), Bv.get_imag_value());
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
       MORE_ERROR () << "scalar argument expected.";
       RANK_ERROR;
     }

const Shape shape_W(2, 2);
Value_P Z(shape_W, LOC);
const Cell & Bv = B->get_cravel (0);
APL_Float xvr = Bv.get_real_value();
APL_Float xvi = Bv.get_imag_value();
complex<double>theta (xvr, xvi);
complex<double>cosx = cos (theta);
complex<double>sinx = sin (theta);
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
Value_P Z;

const ShapeItem A_count   = A->element_count();
const uRank     A_rank    = A->get_rank();
const ShapeItem B_count   = B->element_count();
const uRank     B_rank    = B->get_rank();
    
  if (A_count == 3 && B_count == 3 && A_rank == 1 && B_rank == 1)
    {
      int sdim = (tp == 9) ? 3 : 4;
      Shape shape_W;
      shape_W.add_shape_item (sdim);
      shape_W.add_shape_item (sdim);
      Z = Value_P (shape_W, LOC);
      vector<complex<double> > angs (3);
      loop(i, 3)
          {
            const Cell & Bv = B->get_cravel (i);
            angs[i] =
              complex<double>(Bv.get_real_value(), Bv.get_imag_value());
          }
      complex<double>cosa = cos(angs[0]);
      complex<double>sina = sin(angs[0]);
      complex<double>cosb = cos(angs[1]);
      complex<double>sinb = sin(angs[1]);
      complex<double>cosg = cos(angs[2]);
      complex<double>sing = sin(angs[2]);

      complex<double>t00 = cosa * cosb;
      complex<double>t01 = cosa * sinb * sing - sina * cosg;;
      complex<double>t02 = cosa * sinb * cosg + sina * sing;;

      complex<double>t10 = sina * cosb;
      complex<double>t11 = sina * sinb * sing + cosa * cosg;;
      complex<double>t12 = sina * sinb * cosg - cosa * sing;;

      complex<double>t20 = -sinb;
      complex<double>t21 =  cosb * sing;
      complex<double>t22 =  cosb * cosg;

      if (tp == 9)
         {
           Z->set_ravel_Complex(0,  t00.real (),  t00.imag ());
           Z->set_ravel_Complex(1,  t01.real (),  t01.imag ());
           Z->set_ravel_Complex(2,  t02.real (),  t02.imag ());
           Z->set_ravel_Complex(3,  t10.real (),  t10.imag ());
           Z->set_ravel_Complex(4,  t11.real (),  t11.imag ());
           Z->set_ravel_Complex(5,  t12.real (),  t12.imag ());
           Z->set_ravel_Complex(6,  t20.real (),  t20.imag ());
           Z->set_ravel_Complex(7,  t21.real (),  t21.imag ());
           Z->set_ravel_Complex(8,  t22.real (),  t22.imag ());
         }
      else
         {
           vector<complex<double> > trans(3);
           loop(i, 3)
               {
                 const Cell & Av = A->get_cravel(i);
                 trans[i] = complex<double>(Av.get_real_value(),
                                            Av.get_imag_value());
               }
           Z->set_ravel_Complex(0,   t00.real (),  t00.imag ());
           Z->set_ravel_Complex(1,   t01.real (),  t01.imag ());
           Z->set_ravel_Complex(2,   t02.real (),  t02.imag ());
           Z->set_ravel_Complex(3,   0.0, 0.0);
        
           Z->set_ravel_Complex(4,   t10.real (),  t10.imag ());
           Z->set_ravel_Complex(5,   t11.real (),  t11.imag ());
           Z->set_ravel_Complex(6,   t12.real (),  t12.imag ());
           Z->set_ravel_Complex(7,   0.0, 0.0);
        
           Z->set_ravel_Complex(8,   t20.real (),  t20.imag ());
           Z->set_ravel_Complex(9,   t21.real (),  t21.imag ());
           Z->set_ravel_Complex(10,  t22.real (),  t22.imag ());
           Z->set_ravel_Complex(11,  0.0, 0.0);
        
           Z->set_ravel_Complex(12,  trans[0].real (), trans[0].imag ());
           Z->set_ravel_Complex(13,  trans[1].real (), trans[1].imag ());
           Z->set_ravel_Complex(14,  trans[2].real (), trans[2].imag ());
           Z->set_ravel_Complex(15,  1.0, 0.0);
         }

      
    }
  else
    {
      MORE_ERROR () << "Both arguments must be vectors of ⍴ = 3.";
      RANK_ERROR;
    }

  Z->check_value(LOC);
  return Z;
}
  
/**************** end operational functions ***************/

enum mx_ops_e
{
#define op_entry(e,d,v) e,
#include "Quad_MX.def"
};

struct op_desc_s
{
  int code;
  const char *valence;
  const char *desc;
} op_desc[] = {
#define op_entry(e,d,v) {e, v, d},
#include "Quad_MX.def"
              };

//----------------------------------------------------------------------------
void
Quad_MX::showHelp()
{
  COUT << "\nValid ⎕MX[*] indices\n\n";
  
  for (int i = OP_DETERMINANT; i < OP_MAX; i++)
      COUT << "\t" << op_desc[i].code
           << "\t" << op_desc[i].valence
           << "\t" << op_desc[i].desc
           << endl;
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_AB(Value_P A, Value_P B) const
{
Value_P Z = Str0(LOC);
  showHelp();
  return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_B(Value_P B) const
{
Value_P Z = Str0(LOC);
  showHelp();
  return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
Value_P Z = Str0(LOC);
  
  mx_ops_e op = OP_UNKNOWN;

  int modifier = 0;

  if (X->is_numeric_scalar())
    {
      op = (mx_ops_e)(X->get_sole_integer ());
    }
  else if (X->is_vector ())
    {
      const ShapeItem X_count   = X->element_count();
      const Cell & Xv = X->get_cravel (0);
      op = (mx_ops_e)(Xv.get_int_value ());
      if (X_count > 1)
    {
      const Cell & Xv = X->get_cravel (1);
      modifier = Xv.get_int_value ();
    }
    }
  else
    {
      MORE_ERROR () << "mtx[*] requires a numeric function specifier.";
      SYNTAX_ERROR;
    }
 
  if (op < OP_UNKNOWN || op >= OP_MAX)
    {
      MORE_ERROR () << "Function specifier out of range.";
      SYNTAX_ERROR;
    }

  switch(op)
    {
    case OP_UNKNOWN:            showHelp(); break;
    case OP_CROSS_PRODUCT:      Z = dyadicCrossProduct (A, B);   break;
    case OP_VECTOR_ANGLE:       Z = vectorAngle (A, B);          break;
    case OP_HOMOGENEOUS_MATRIX: Z = dyadicRotation (16, A, B);   break;
    case OP_COVARIANCE:         Z = dyadicCovariance (A, B);     break;
    case OP_HISTOGRAM:          Z = histogram (A, B);            break;
    case OP_RANDOMS:            Z = randoms (&A, B, modifier);   break;
    case OP_PRINT:              Z = printit (A, B);              break;
    case OP_DETERMINANT:        // fall through
    case OP_EIGENVECTORS:       // fall through
    case OP_EIGENVALUES:        // fall through
    case OP_IDENT:              // fall through
    case OP_ROTATION_MATRIX:    // fall through
    case OP_NORM:               MORE_ERROR() << "Not a dyadic function.";
                                SYNTAX_ERROR;                    break;
    default:                    MORE_ERROR() << "Function not yet implemented.";
                                SYNTAX_ERROR;
    }

  Z->check_value(LOC);
  return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Token
Quad_MX::eval_XB(Value_P X, Value_P B) const
{
Value_P Z = Str0(LOC);
  mx_ops_e op = OP_UNKNOWN;

  int modifier = 0;

  if (X->is_numeric_scalar())
    {
      op = (mx_ops_e)(X->get_sole_integer ());
    }
  else if (X->is_vector ())
    {
      const ShapeItem X_count   = X->element_count();
      const Cell & Xv = X->get_cravel (0);
      op = (mx_ops_e)(Xv.get_int_value ());
      if (X_count > 1)
    {
      const Cell & Xv = X->get_cravel (1);
      modifier = Xv.get_int_value ();
    }
    }
  else
    {
      MORE_ERROR () << "mtx[*] requires a numeric function specifier.";
      SYNTAX_ERROR;
    }

  if (op < OP_UNKNOWN || op >= OP_MAX)
    {
      MORE_ERROR () << "Function specifier out of range.";
      SYNTAX_ERROR;
    }

  switch(op)
    {
      case OP_UNKNOWN:            showHelp ();                          break;
      case OP_CROSS_PRODUCT:      Z = monadicCrossProduct (B);          break;
           //v←v,⍉1 100⍴100 ⎕mx[12] 100000 ⎕mx[10 2] 1
      case OP_RANDOMS:            Z = randoms (nullptr, B, modifier);   break;
      case OP_DETERMINANT:        Z = determinant (B);                  break;
      case OP_COVARIANCE:         Z = monadicCovariance (B);            break;
      case OP_NORM:               Z = norm (B);                         break;
      case OP_EIGENVECTORS:       Z = eigenvectors (B);                 break;
      case OP_EIGENVALUES:        Z = eigenvalues (B);                  break;
      case OP_IDENT:              Z = ident (B);                        break;
      case OP_ROTATION_MATRIX:    Z = monadicRotation (B);              break;
      case OP_SET_RNG_SEED:       Z = set_rng_seed (B);                 break;
      case OP_VECTOR_ANGLE:
      case OP_HOMOGENEOUS_MATRIX: MORE_ERROR () << "Not a monadic function.";
                                  SYNTAX_ERROR;                          break;
      default:
        MORE_ERROR () << "Function not yet implemented.";
        SYNTAX_ERROR;
    }

  Z->check_value(LOC);
  return Token(TOK_APL_VALUE1, Z);
}

#else // not if apl_GSL ======================================================
 
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
//----------------------------------------------------------------------------

#endif

