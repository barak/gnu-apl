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
  typedef std::complex<double> Dcomplex;

public:
   /// Constructor. Sorts \b sub_function_infos by name (as needed by bsearch()).
  Quad_MX();

  enum MX_ops
     {
       OP_MIN = 0,
#define op_entry(enum, axis, _sub_name, _valence, _desc) OP_ ## enum = axis,
#include "Quad_MX.def"
     };

   /// overloaded Function::has_subfuns()
   virtual bool has_subfuns() const
      { return true; }

   /// overloaded Function::subfun_to_axis
   virtual sAxis subfun_to_axis(const UCS_string & name) const;

   static Quad_MX fun;          ///< Built-in function.

  /// properties of subfunctions
  static sub_function_info sub_function_infos[];   ///< all subfunctions

protected:
  /// a matrix suitable for libgsl
  class Matrix
     {
       public:
         /// constructor: un-initialzed r×c matrix
         Matrix(int r, int c)
         : krows(r),
           kcols(c),
           vals(r * c)
         {}

         /// return the value at \b row and \b col
         Dcomplex val(int row, int col) const
            {
              return vals[col + row * kcols];
            }
    
         /// set the value at \b row and \b col to \b value
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

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_XB().
  virtual Token eval_XB(Value_P X, Value_P B) const;

  /// overloaded Function::eval_AB().
  virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B().
  virtual Token eval_B(Value_P B) const;

  /// return the cros product of all rows in \b mtx
  static vector<Dcomplex> getCross(Matrix * mtx);

  /// return mtx with \b row and \b col removed
  static Matrix * genCofactor(Matrix * mtx, int row, int col);

  /// return the determinant of \b mtx
  static Dcomplex getDet(Matrix * mtx);

  /// return the magnitude of \b v
  static Dcomplex magnitude(vector<Dcomplex> & v);

  /// normalize \b v to length 1
  static void normalise(vector<double> & v);

  /// generate a matrix suitable for libgsl
  static Matrix * genMtx(Value_P B, bool padded);

  /// return the eigenvectors of \b B
  static Value_P eigenvectors(Value_P B);

  /// print value \b B to file \b A
  static Value_P printit(Value_P filename_A, Value_P B);

  /// open file indicated by filename.
  static FILE * open_file(const Value & filename, bool & close_file);

  //// return the eigencalues of \b B
  static Value_P eigenvalues(Value_P B);

  /// set the random number generators seed
  static Value_P set_rng_seed(Value_P B);

  //// return the determinant of \b B
  static Value_P determinant(Value_P B);

  //// return the cross product of all vectors in \b B
  static Value_P monadicCrossProduct(Value_P B);

  //// return the cross product of \b A and \b B
  static Value_P dyadicCrossProduct(Value_P A, Value_P B);

  //// return the angle between \b A and \b B
  static Value_P vectorAngle(const Value_P A, const Value_P B);

  //// return the (complex) identity matrix
  static Value_P ident(const Value_P B);

  //// return the monadic Covariance
  static Value_P monadicCovariance(const Value_P B);

  static Value_P histogram(const Value_P A, const Value_P B);

  //// return the dyadic Covariance
  static Value_P dyadicCovariance(const Value_P A, const Value_P B);

  /// return random numbers
  static Value_P randoms(const Value_P * opt_A, const Value_P B, int modifier);

  static Value_P norm(const Value_P B);

  static Value_P monadicRotation(Value_P B);

  static Value_P dyadicRotation(int tp, Value_P A, Value_P B);

  /// list functions and their syntaces
  static void list_functions(bool mapping);

  /// list one item
  static void list_item(ostream & out, int idx, int valence,
                        const char * description);

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
