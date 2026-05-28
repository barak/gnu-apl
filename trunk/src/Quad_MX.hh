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

#ifndef __Quad_MX_DEFINED__
#define __Quad_MX_DEFINED__

#include "QuadFunction.hh"
#include "Value.hh"

#include <random>


/// The class implementing ⎕MX
class Quad_MX : public QuadFunction
{
public:
   /// Constructor. Sorts \b sub_function_infos by name (as needed for binary
   /// searches).
  Quad_MX();

  typedef std::complex<double> Dcomplex;

  enum MX_ops
     {
       OP_LIST = 0,
#define mx_def(axis, _sub_name, _desc, _valence, enum) OP_ ## enum = axis,
#include "Quad_MX.def"
     };

   static Quad_MX fun;   ///< Built-in function.

protected:
  /// a matrix suitable for libgsl
  class GSL_Matrix
     {
       public:
         /// constructor: un-initialzed r×c matrix
         /// @param r number of rows
         /// @param c number of columns
         GSL_Matrix(int r, int c)
         : krows(r),
           kcols(c),
           vals(r * c)
         {}

         /// return the value at \b row and \b col
         /// @param row zero-based row index
         /// @param col zero-based column index
         Dcomplex val(int row, int col) const
            {
              return vals[col + row * kcols];
            }

         /// set the value at \b row and \b col to \b value
         /// @param row zero-based row index
         /// @param col zero-based column index
         /// @param value complex value to store
         void set_val(int row, int col, Dcomplex value)
            {
              vals[col + row * kcols] = value;
            }

         /// return the number of rows
         int rows() const   { return krows; }

         /// return the number of columns
         int cols() const   { return kcols; }

         /// print \b this matrix to stderr
         void show() const
            {
              loop(r, krows)
                  {
                    loop(c, kcols)
                        {
                          fprintf(stderr, "%gj%g ", val(r, c).real(),
                                                    val(r, c).imag());
                        }
                    fprintf (stderr, "\n");
                  }
            }

         /// return true iff vector (!) has complex items
         /// @param vector vector of complex numbers to inspect
         static bool is_complex(const vector<Dcomplex> & vector)
            {
              loop(i, vector.size())
                  {
                    if (vector[i].imag () != 0.0)   return true;
                  }
              return false;
            }

       private:
         int krows;               ///< the number of rows
         int kcols;               ///< the number of columns
         vector<Dcomplex> vals;   ///< the matrix items
     };

   /// overloaded FunctionGroup::print_fun_syntax()
   /// @param out output stream to write to
   /// @param info descriptor of the subfunction whose syntax is to be printed
   virtual void print_fun_syntax(ostream & out,
                                const function_info & info) const;

   /// overloaded FunctionGroup::print_map_syntax()
   /// @param out output stream to write to
   /// @param info descriptor of the subfunction whose map syntax is to be printed
   virtual void print_map_syntax(ostream & out,
                                 const function_info & info) const;

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis APL value
   /// @param B right argument APL value
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_XB().
   /// @param X axis APL value
   /// @param B right argument APL value
  virtual Token eval_XB(Value_P X, Value_P B) const;

  /// overloaded Function::eval_AB().
  /// @param A left argument APL value
  /// @param B right argument APL value
  virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B().
  /// @param B right argument APL value
  virtual Token eval_B(Value_P B) const;

  /// return the cros product of all rows in \b mtx
  /// @param mtx matrix whose row cross-product is to be computed
  static vector<Dcomplex> getCross(GSL_Matrix * mtx);

  /// return mtx with \b row and \b col removed
  /// @param mtx source matrix
  /// @param row row to exclude
  /// @param col column to exclude
  static GSL_Matrix * genCofactor(GSL_Matrix * mtx, int row, int col);

  /// return the determinant of \b mtx
  /// @param mtx square matrix whose determinant is to be computed
  static Dcomplex getDet(GSL_Matrix * mtx);

  /// return the magnitude of \b v
  /// @param v complex vector whose magnitude (Euclidean norm) is returned
  static Dcomplex magnitude(const vector<Dcomplex> & v);

  /// normalize \b v to length 1
  /// @param v real vector to normalize in-place
  static void normalise(vector<double> & v);

  /// generate a matrix suitable for libgsl
  /// @param B APL matrix value to convert
  /// @param padded true to add an extra column of zeros (for augmented matrices)
  static GSL_Matrix * genMtx(Value_P B, bool padded);

  /// return the eigenvectors of \b B
  /// @param B APL matrix value
  static Value_P eigenvectors(Value_P B);

  /// print value \b B to file \b A
  /// @param filename_A APL character value containing the output filename
  /// @param B APL value to print
  static Value_P printit(Value_P filename_A, Value_P B);

  /// open file indicated by filename.
  /// @param filename APL character value containing the path to open
  /// @param close_file set to true if the caller must close the returned FILE*
  static FILE * open_file(const Value & filename, bool & close_file);

  //// return the eigencalues of \b B
  /// @param B APL matrix value
  static Value_P eigenvalues(Value_P B);

  /// set the random number generators seed
  /// @param B APL integer value containing the desired seed
  static Value_P set_rng_seed(Value_P B);

  //// return the determinant of \b B
  /// @param B APL matrix value
  static Value_P determinant(Value_P B);

  //// return the cross product of all vectors in \b B
  /// @param B APL matrix value whose rows are the input vectors
  static Value_P monadicCrossProduct(Value_P B);

  //// return the cross product of \b A and \b B
  /// @param A left APL vector value
  /// @param B right APL vector value
  static Value_P dyadicCrossProduct(Value_P A, Value_P B);

  //// return the angle between \b A and \b B
  /// @param A left APL vector value
  /// @param B right APL vector value
  static Value_P vectorAngle(const Value_P A, const Value_P B);

  //// return the (complex) identity matrix
  /// @param B APL integer scalar giving the size of the identity matrix
  static Value_P ident(const Value_P B);

  //// return the monadic Covariance
  /// @param B APL matrix value (rows are observations)
  static Value_P monadicCovariance(const Value_P B);

  /// @param A left APL value (group labels or second data set)
  /// @param B right APL value (data vector or matrix)
  static Value_P histogram(const Value_P A, const Value_P B);

  //// return the dyadic Covariance
  /// @param A left APL data vector or matrix
  /// @param B right APL data vector or matrix
  static Value_P dyadicCovariance(const Value_P A, const Value_P B);

  /// return random numbers
  /// @param opt_A optional left argument specifying distribution parameters
  /// @param B right argument giving count or shape of the result
  /// @param dist distribution selector (e.g. uniform, normal)
  static Value_P randoms(Value_P opt_A, Value_P B, int dist);

  /// @param B APL vector or matrix value
  static Value_P norm(const Value_P B);

  /// @param B APL value representing the rotation (angle or quaternion)
  static Value_P monadicRotation(Value_P B);

  /// @param tp rotation type selector
  /// @param A left APL value (rotation parameters)
  /// @param B right APL vector to rotate
  static Value_P dyadicRotation(int tp, Value_P A, Value_P B);

   /// properties of subfunctions
   static const FunctionGroup::function_info subfunction_infos[];

  /// true if the random number generators were initialized
  static bool rng_initialised;

  /// true if the random number generator seed was initialized
  static bool rng_seed_set;

  /// random number generator seed
  static unsigned int rng_seed;

  /// random number generator for real parts
  static mt19937_64 rgen;   // aka. mersenne_twister_engine<...>

  /// random number generator fore imaginary parts
  static mt19937_64 igen;
};

#endif  // __Quad_MX_DEFINED__
