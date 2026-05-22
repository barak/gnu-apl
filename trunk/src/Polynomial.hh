/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2025  Dr. Jürgen Sauermann

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

#include "APL_types.hh"
#include "Shape.hh"
#include "Value.hh"

class Unicode_source;   // Tokenizer.hh

//----------------------------------------------------------------------------
/** one term aₖXᵐYⁿ... of a polynomial. The indeterminats powers are
    represented by a Shape, and the coefficient by a type T. The rank
    of the shape is the number of indeterminants.

    An exponent 0 is a an omitted indeterminat (since X⁰ = 1).
 **/
class Monomial
{
   typedef complex<double> Complex;
public:
   Monomial()
   : coefficient(0.0)
   {}

   /// constructor: term with indefnitess as per indefs
   /// @param coeff coefficient of this monomial term
   /// @param indefs shape whose items are the exponents of each indeterminate
   Monomial(Complex coeff, const Shape & indefs)
   : coefficient(coeff)
     {
       loop(n, indefs.get_rank())   expos.push_back(indefs.get_shape_item(n));
     }

   /// return true if the imaginary part of the coefficient is significant.
   bool needs_complex() const
      { return !Cell::is_near_zero(coefficient.imag()); }

   /// return the largest exponent of the indeterminants
   int max_expo() const
      {
        int maxi = 0;
        loop(n, expos.size())   if (maxi < expos[n])   maxi = expos[n];
        return maxi;
      }

   /// return the degree of the the term.
   int degree() const
      {
        int sum = 0;
        loop(n, expos.size())   sum += expos[n];
        return sum;
      }

   /// return the number of indeterminats
   int indeterminant_count() const
      { return expos.size(); }

   /// return the exponent of the r'th indeterminant
   /// @param r zero-based indeterminate index
   int get_indeterminant_power(size_t r) const
      {
        Assert(r < expos.size());
        return expos[r];
      }

   /// return the exponents as a shape
   Shape to_shape() const
      {
        Shape shape;
        loop(e, expos.size())   shape.add_shape_item(expos[e]);
        return shape;
      }

   /// comparator (aka. lexical order)
   /// @param other monomial to compare against
   bool operator <(const Monomial & other) const
      {
        Assert(expos.size() == other.expos.size());
        loop(n, expos.size())
            {
              if (expos[n] < other.expos[n])   return true;
              if (expos[n] > other.expos[n])   return false;
            }
        return false;   // equai
      }

   /// return \b true iff \b this polynomial is a multiple of the polynomial
   /// \b factor.
   /// @param factor monomial to test divisibility by
  bool is_multiple_of(const Monomial & factor) const
      {
        Assert(expos.size() == factor.expos.size());
        loop(n, expos.size())
            {
              if (expos[n] < factor.expos[n])   return false;
            }
        return true;
      }

   /// return the coefficient
   Complex get_coefficient() const
      { return coefficient; }

   /// subtract \b coeff from \b coefficient
   /// @param coeff value to subtract from this term's coefficient
   void subtract_coefficient(Complex coeff)
      { coefficient -= coeff; }

   /// negate \b coefficient
   void negate_coefficient()
      { coefficient = - coefficient; }

   /// return \b this divided by \b divisor.
   /// @param divisor monomial to divide by
   Monomial operator /(const Monomial & divisor) const
      {
        Assert(expos.size() == divisor.expos.size());
        Shape quot;
        loop(n, expos.size())
            {
              Assert(expos[n] >= divisor.expos[n]);
              quot.add_shape_item(expos[n] - divisor.expos[n]);
            }
        return Monomial(coefficient / divisor.coefficient, quot);
      }

   /// return \b this times \b factor.
   /// @param factor monomial to multiply by
   Monomial operator *(const Monomial & factor) const
      {
        Assert(expos.size() == factor.expos.size());
        Shape prod;
        loop(n, expos.size())
            {
              prod.add_shape_item(expos[n] + factor.expos[n]);
            }
        return Monomial(coefficient * factor.coefficient, prod);
      }

   /// return \b true if \b this and \b other have the same exponents of
   /// their indeterminants
   /// @param other monomial to compare exponents with
   bool same_expos(const Monomial & other) const
      {
        if (expos.size() != other.expos.size())   return false;
        loop(n, expos.size())   if (expos[n] != other.expos[n])   return false;
        return true;
      }

   double get_real() const
      { return coefficient.real(); }

   double get_imag() const
      { return coefficient.imag(); }

   /// set coefficient (must be called before set_imag())
   /// @param overbar true if the APL overbar negation prefix was present
   /// @param term_sign '+' or '-' sign preceding this term
   /// @param val non-negative real magnitude of the coefficient
   void set_real(bool overbar, char term_sign, double val)
      {
        Assert(val >= 0.0);
        if (term_sign == '-' )
           {
             if (overbar)   coefficient = val;
             else           coefficient = -val;
           }
        else
           {
             if (overbar)   coefficient = -val;
             else           coefficient = val;
           }
      }

   /// return the (user-defined) value to be used for comparing \b rhis
   /// monomial with
   /// other monomials
   /// @param order APL value defining the monomial ordering
   int get_order(const Value & order) const;

   /// scan the coeffient in \b src.
   /// @param src Unicode input stream to read from
   /// @param term_sign '+' or '-' sign preceding this term
   /// @param got_j set to true if an imaginary 'J' separator was found
   /// @param got_overbar true if the overbar negation prefix was seen
   void scan_coefficient(Unicode_source & src, char term_sign,
                         bool & got_j, const bool & got_overbar);

   /// scan the indeterminants in \b src.
   /// @param vars list of known indeterminate variable names
   /// @param src Unicode input stream to read from
   void scan_indeterminants(const UCS_string_vector & vars,
                            Unicode_source & src);

   /// set coefficient (must be called before set_imag())
   /// @param overbar true if the APL overbar negation prefix was present
   /// @param term_sign '+' or '-' sign preceding this term
   /// @param val non-negative imaginary magnitude of the coefficient
   void set_imag(bool overbar, char term_sign, double val)
      {
        Assert(val >= 0.0);
        if (term_sign == '-' )
           {
             if (overbar)   coefficient = Complex(coefficient.real(), val);
             else           coefficient = Complex(coefficient.real(), -val);
           }
        else
           {
             if (overbar)   coefficient = Complex(coefficient.real(), -val);
             else           coefficient = Complex(coefficient.real(), val);
           }
      }

   /// return \b this polynomial as an APL value
   Value_P to_value() const;

   /// print \b this term
   /// @param out output stream to write to
   /// @param firsti true if this is the first term (affects sign printing)
   ostream & print(ostream & out, bool firsti = true) const;

protected:
   Complex coefficient;       ///< the coefficient

public:
   vector<int> expos;   ///< the (powers of) the indeterminants
};
//============================================================================
class Polynomial : public vector<Monomial>
{
public:
   typedef complex<double> Complex;

   /// constructor: 0-polynomial
   Polynomial()
   {}

   /// constructor: from APL value (holding the coefficients).
   /// @param value APL value whose ravel provides the polynomial coefficients
   Polynomial(const Value & value);

   /// return true if any of the terms is complex
   bool needs_complex() const
      {
         loop(t, size())   if (at(t).needs_complex())   return true;
         return false;
      }

   /// return the number of indeterminants (including omitted indeterminants)
   int indeterminant_count() const
       {
         int count = 0;
         loop(t, size())
             {
               const int cnt = at(t).indeterminant_count();
               if (count < cnt)   count = cnt;
             }
         return count;
       }

   /// return (the index of) the term with the largest exponent
   int max_expo() const
      {
        int pos = -1;
        int expo = -1;
        loop(t, size())
            {
              const int mx = at(t).max_expo();
              if (expo < mx)   { pos = t;   expo = mx; }
            }
        return pos;
      }

   /// return the degree of the polynomial ( := the max degree of its terms).
   int degree() const
       {
         int deg = 0;
         loop(t, size())
             {
              const int d = at(t).degree();
              if (deg < d)   deg = d;
             }
         return deg;
       }

   /// return (the index of) the largest term (aka. LT).
   /// @param order APL value defining the monomial ordering (may be null)
   size_t LT_pos(const Value * order) const;

   /// remove the largest term from \b this polynomial and return it.
   /// @param order APL value defining the monomial ordering (may be null)
   Monomial extract_LT(const Value * order)
      {
        const size_t pos = LT_pos(order);
        Monomial result = at(pos);
        erase(begin() + pos);
        return result;
      }

   /// return the largest term from \b this polynomial.
   /// @param order APL value defining the monomial ordering (may be null)
   Monomial get_LT(const Value * order)
      {
        return at(LT_pos(order));
      }

   /// subtract term \b term from polynomial poly
   /// @param other monomial term to subtract
   void subtract_term(const Monomial & other)
      {
        loop(t, size())
            {
              Monomial & term = at(t);
              if (term.same_expos(other))   // compatible term
                 {
                   term.subtract_coefficient(other.get_coefficient());
                   if (Cell::is_near_zero(term.get_coefficient().real()) &&
                       Cell::is_near_zero(term.get_coefficient().imag()))
                      {
                        erase(begin() + t);
                      }
                   return;
                 }
            }

        // at this point poly has no term with the same exponents
        //
        push_back(other);
        back().negate_coefficient();
      }

   /// print \b this polynomial
   /// @param out output stream to write to
   ostream & print(ostream & out) const;

   /// return \b this polynomial as an APL value
   Value_P to_value() const;
};
//============================================================================


