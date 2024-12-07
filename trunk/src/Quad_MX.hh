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
   /// Constructor.
  Quad_MX() : QuadFunction(TOK_Quad_MX)
  {
  }

   /// overloaded Function::has_subfuns()
   virtual bool has_subfuns() const
      { return true; }

   /// overloaded Function::subfun_to_axis
   virtual sAxis subfun_to_axis(const UCS_string & name) const;

   static Quad_MX  fun;          ///< Built-in function.

protected:
  class Matrix
     {
       public:
         Matrix (int r, int c)
         : krows(r),
           kcols(c)
            {
              vals = new vector<Dcomplex>(r * c);
            }
       
         ~Matrix ()
            {
              delete vals;
            }
    
         Dcomplex val(int r, int c)
            {
              return (*vals)[c + r * kcols];
            }
    
         void val(int r, int c, Dcomplex v)
            {
              (*vals)[c + r * kcols] = v;
            }
    
         int  rows()   { return krows; }
    
         int  cols()   { return kcols; }
    
         void show()
            {
              for (int r = 0; r < krows; r++)
                  {
                    for (int c = 0; c < kcols; c++)
                      {
                        fprintf (stderr, "%gj%g ",
                             this->val (r, c).real (),
                             this->val (r, c).imag ());
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
         int krows;
         int kcols;
         vector<Dcomplex> *vals;
     };

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_XB().
  virtual Token eval_XB(Value_P X, Value_P B) const;

  /// overloaded Function::eval_AB().
  virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B().
  virtual Token eval_B(Value_P B) const;

  static vector<Dcomplex> getCross(Matrix * mtx);

  static Matrix * genCofactor(Matrix *mtx, int r, int c);

  static Dcomplex getDet(Matrix * mtx);

  static Dcomplex magnitude(vector<Dcomplex> &v);

  static void normalise(vector<double> &v);

  static Matrix * genMtx(Value_P B, bool padded);

  static Value_P eigenvectors(Value_P B);

  static Value_P printit(Value_P A, Value_P B);

  static Value_P eigenvalues(Value_P B);

  static Value_P set_rng_seed(Value_P B);

  static Value_P determinant(Value_P B);

  static Value_P monadicCrossProduct(Value_P B);

  static Value_P dyadicCrossProduct(Value_P A, Value_P B);

  static Value_P vectorAngle(const Value_P A, const Value_P B);

  static Value_P ident(const Value_P B);

  static Value_P monadicCovariance(const Value_P B);

  static Value_P histogram(const Value_P A, const Value_P B);

  static Value_P dyadicCovariance(const Value_P A, const Value_P B);

  static Value_P randoms(const Value_P *A, const Value_P B, int modifier);

  static Value_P norm(const Value_P B);

  static Value_P monadicRotation(Value_P B);

  static Value_P dyadicRotation(int tp, Value_P A, Value_P B);

  static void list_functions(bool);
};

#endif  // __Quad_MX_DEFINED__
