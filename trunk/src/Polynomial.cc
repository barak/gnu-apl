/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2025-2026  Dr. Jürgen Sauermann

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

#include "Common.hh"
#include "Function.hh"
#include "Polynomial.hh"
#include "Tokenizer.hh"

//════════════════════════════════════════════════════════════════════════════
int
Monomial::get_order(const Value & order) const
{
   // find the index of this monomial in order.
   //
Shape index;
   loop(e, expos.size())   index.add_shape_item(expos[e]);

const ShapeItem offset = order.get_shape().ravel_pos(index);
   return order.get_cravel(offset).get_int_value();
}
//────────────────────────────────────────────────────────────────────────────
ostream &
Monomial::print(ostream & out, bool first) const
{
Complex coeff = coefficient;
   if (Cell::is_near_zero(coeff.real()) &&
       Cell::is_near_zero(coeff.imag()))   return out;

   if (first)
      {
        if (coeff.real() < 0)
           {
             out << "¯";
             coeff = - coeff;
           }
      }
   else
      {
        if (coeff.real() < 0)
           {
             out << " - ";
             coeff = - coeff;
           }
        else
           {
             out << " + ";
           }
      }
   if (coeff.imag() || !Cell::is_near_zero(coeff.real() - 1.0))
      {
        Assert(coeff.real() >= 0.0);
        out << coeff.real();
      }

   if (coeff.imag())   out << "J" << coeff.imag();
   loop(r, expos.size())
       {
         if (const int expo = expos[r])   // more than X⁰
            {
              out << Unicode('x' + r);
              if (expo > 1)   out << UCS_string::power(expo);
            }
       }

   return out;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Monomial::to_value() const
{
Value_P Z(expos.size() + 1, LOC);

   if (Cell::is_near_zero(coefficient.imag()))       // real coefficient
      {
        Z->next_ravel_Number(coefficient.real());
      }
   else if (!Cell::is_near_int(coefficient.imag()))   // complex coefficient
      {
        Z->next_ravel_Complex(coefficient.real(), coefficient.imag());
      }

   loop(e, expos.size())   Z->next_ravel_Int(expos[e]);

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Monomial::scan_coefficient(Unicode_source & src, char term_sign, bool & got_j,
                            const bool & got_overbar)
{
const Tokenizer::Int_or_Double id = Tokenizer::tokenize_real(src);
   if (!id.is_valid)   DOMAIN_ERROR;

   if (got_j)   // imag part
      {
        set_imag(got_overbar, term_sign, id.get_double());
        got_j = false;
      }
   else         // real part
      {
        set_real(got_overbar, term_sign, id.get_double());
      }
}
//════════════════════════════════════════════════════════════════════════════
void
Monomial::scan_indeterminants(const UCS_string_vector & vars,
                               Unicode_source & src)
{
   if (!Avec::is_symbol_char(*src))   // expected: indeterminant & power
      {
        MORE_ERROR() << "⌹[11] B: Invalid character '"
                     << *src << "' in polynomial B.";
        DOMAIN_ERROR;
      }

const UTF8_string expo_digits_utf("⁰¹²³⁴⁵⁶⁷⁸⁹");
const UCS_string expo_digits(expo_digits_utf);

   // extract the indeterminant name(s). Could be more
   // than one, e.g. xy². We use the longest in vars.
   //
UCS_string indeterminants;
   loop(r, src.rest_len())
       {
         const Unicode ind = src[r];
         if (Avec::is_symbol_char(ind))
            {
              indeterminants << ind;
            }
         else break;
       }

int found = -1;
size_t found_len = 0;
   loop(v, vars.size())
       {
               const UCS_string & var = vars[v];
               if (indeterminants.starts_with(var) &&
                   found_len < var.size())
                  {
                    found = v;
                    found_len = var.size();
                  }
       }

   if (found == -1)
      {
        MORE_ERROR() << "A ⌹[11] B: no prefix of '"
                     << indeterminants << "' in A.";
        DOMAIN_ERROR;
      }

   // found the indeterminant.
   //
   while (int(expos.size()) < (found + 1))   expos.push_back(0);
   if (expos[found])   // duplicate
      {
              MORE_ERROR() <<
              "⌹[11] B: duplicate indeterminant '"
               << vars[found] << "' in term.";
              DOMAIN_ERROR;
      }
   expos[found] = 1;   // for now

   src.skip(vars[found].size());   // skip name

   // optional indeterminant power
   //
   if (src.has_more() && expo_digits.contains(*src))
      {
        // power present
        //
        int power = 0;
         while (src.has_more() && expo_digits.contains(*src))
               {
                power *= 10;
                loop(e, expo_digits.size())
                    {
                      if (expo_digits[e] == *src)
                         {
                           power += e;
                           break;
                         }
                    }
                 expos[found] = power;
                 ++src;
               }
      }
}
//════════════════════════════════════════════════════════════════════════════
Polynomial::Polynomial(const Value & value)
{
   reserve(40);
   loop(v, value.element_count())
       {
         const Cell & cell = value.get_cravel(v);
         if (cell.is_near_zero())   continue;

         const Complex coeff(cell.get_real_value(),
                             cell.get_imag_value());
         const Shape shape = value.get_shape().offset_to_index(v, /*⎕IO*/ 0);
         const Monomial term(coeff, shape);
         push_back(term);
       }
}
//────────────────────────────────────────────────────────────────────────────
size_t
Polynomial::LT_pos(const Value * order) const
{
size_t pos = 0;

   if (order)   // user-defined order beyween monomials
      {
        int ord = at(0).get_order(*order);
        for (size_t t = 1; t < size(); ++t)
            {
              const int ord_t = at(t).get_order(*order);
              if (ord < ord_t)   { pos = t;   ord = ord_t; }
            }
      }

   /// no user defined order, use lexicographical order
   ///
   for (size_t t = 1; t < size(); ++t)
       {
         if (at(pos) < at(t))   pos = t;
       }
   return pos;

}
//────────────────────────────────────────────────────────────────────────────
ostream &
Polynomial::print(ostream & out) const
{
   if (size() == 0)   return out << "0";

   // find largest
   //
const Monomial * largest = &at(0);
   for (size_t j = 1; j < size(); ++j)
       {
         const Monomial * tj = &at(j);
         if (*largest < *tj)   largest = tj;
       }

   loop(t, size())
       {
         largest->print(out, t == 0);
         const Monomial * next = 0;
         loop(t, size())
             {
               const Monomial * tj = &at(t);
               if (tj == largest)      continue;
               if (*largest < *tj)     continue;
               if (next == 0)          next = tj;
               else if (*next < *tj)   next = tj;
             }
         Assert(next);
         largest = next;
       }
   return out;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Polynomial::to_value() const
{
   // the length of r-th axis of Z is the largest exponen of indeterminant r.
   //
Shape shape_Z;
   loop(r, indeterminant_count())   // loop over indeterminants
      {
        int max_expo = 0;
        loop(t, size())
            {
              const Monomial & term = at(t);
              const ShapeItem len = term.get_indeterminant_power(r);
              if (max_expo < len)   max_expo = len;
            }
        shape_Z.add_shape_item(max_expo + 1);
      }

Value_P Z(shape_Z, LOC);
   loop(z, Z->nz_element_count())   Z->next_ravel_0();

   loop(t, size())
       {
         const Monomial & term = at(t);
         const Shape index = term.to_shape();
         const ShapeItem pos_Z = shape_Z.ravel_pos(index);
         Complex coeff = term.get_coefficient();
         Cell * cell = &Z->get_wravel(pos_Z);

         // there could be multiple terms with the same exponents.
         // Add them up.
         //
         if (!cell->is_near_zero())
            {
              coeff += Complex(cell->get_real_value(),
                               cell->get_imag_value());
            }

         if (!Cell::is_near_zero(coeff.imag()))   // complex coeff
            {
              new (cell) ComplexCell(coeff.real(), coeff.imag());
            }
         else if (!Cell::is_near_int64_t(coeff.real()))
            {
              new (cell) FloatCell(coeff.real());
            }
         else if (!Cell::is_near_zero(coeff.real()))
            {
              new (cell) IntCell(Cell::near_int(coeff.real()));
            }
       }
   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
// EOF
