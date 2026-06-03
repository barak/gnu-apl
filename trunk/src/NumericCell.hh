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

#ifndef __NUMERICCELL_HH_DEFINED__
#define __NUMERICCELL_HH_DEFINED__

#include "Cell.hh"

//════════════════════════════════════════════════════════════════════════════
/*! A cell that is either an integer cell, a floating point cell, or a
    complex number cell. This class contains all cell functions for which
    the detailed type makes no difference.
*/
/// Base class for RealCell and ComplexCell
class NumericCell : public Cell
{
public:
   /// initialize the (un-initialized) Cell *Z to (if possible) an IntCell
   /// or else to a FloatCell with value flt.
   /// @param Z   pointer to uninitialized cell memory
   /// @param flt floating-point value to store
   static ErrorCode zV(Cell * Z, APL_Float flt);

   /// initialize the (un-initialized) Cell *Z to (if possible) an IntCell,
   /// a FloatCell, or a ComplexCell with value cpx.
   /// @param Z   pointer to uninitialized cell memory
   /// @param cpx complex value to store
   static ErrorCode zV(Cell * Z, APL_Complex cpx);

   /// initialize the (un-initialized) Cell *Z to (if possible) an IntCell,
   /// a FloatCell, or a ComplexCell with value cpx.
   /// @param Z    pointer to uninitialized cell memory
   /// @param real real part of the complex value to store
   /// @param imag imaginary part of the complex value to store
   static ErrorCode zV(Cell * Z, APL_Float real, APL_Float imag);

protected:
   /// overloaded Cell::get_classname()
   virtual const char * get_classname() const   { return "NumericCell"; }

   /// overloaded Cell::is_numeric()
   virtual bool is_numeric() const
      { return true; }

   /// overloaded Cell::bif_and()
   /// @param Z result cell pointer
   /// @param A left operand cell
   virtual ErrorCode bif_and(Cell * Z, const Cell * A) const;

   /// overloaded Cell::bif_and_bitwise()
   /// @param Z result cell pointer
   /// @param A left operand cell
   virtual ErrorCode bif_and_bitwise(Cell * Z, const Cell * A) const;

   /// overloaded Cell::bif_binomial()
   /// @param Z result cell pointer
   /// @param A left operand cell (N in N!K)
   virtual ErrorCode bif_binomial(Cell * Z, const Cell * A) const;

   /// N over K for complex A and/or B
   /// @param Z result cell pointer
   /// @param A left operand cell (N in N!K)
   ErrorCode complex_binomial(Cell * Z, const Cell * A) const;

   /// overloaded Cell::bif_equal_bitwise()
   /// @param Z result cell pointer
   /// @param A left operand cell
   virtual ErrorCode bif_equal_bitwise(Cell * Z, const Cell * A) const;

   /// overloaded Cell::bif_nand()
   /// @param Z result cell pointer
   /// @param A left operand cell
   virtual ErrorCode bif_nand(Cell * Z, const Cell * A) const;

   /// overloaded Cell::bif_nand_bitwise()
   /// @param Z result cell pointer
   /// @param A left operand cell
   virtual ErrorCode bif_nand_bitwise(Cell * Z, const Cell * A) const;

   /// overloaded Cell::bif_nor()
   /// @param Z result cell pointer
   /// @param A left operand cell
   virtual ErrorCode bif_nor(Cell * Z, const Cell * A) const;

   /// overloaded Cell::bif_nor_bitwise()
   /// @param Z result cell pointer
   /// @param A left operand cell
   virtual ErrorCode bif_nor_bitwise(Cell * Z, const Cell * A) const;

   /// overloaded Cell::bif_not()
   /// @param Z result cell pointer
   virtual ErrorCode bif_not(Cell * Z) const;

   /// overloaded Cell::bif_not_bitwise()
   /// @param Z result cell pointer
   virtual ErrorCode bif_not_bitwise(Cell * Z) const;

   /// overloaded Cell::bif_not_equal_bitwise()
   /// @param Z result cell pointer
   /// @param A left operand cell
   virtual ErrorCode bif_not_equal_bitwise(Cell * Z, const Cell * A) const;

   /// overloaded Cell::bif_or()
   /// @param Z result cell pointer
   /// @param A left operand cell
   virtual ErrorCode bif_or(Cell * Z, const Cell * A) const;

   /// overloaded Cell::bif_or_bitwise()
   /// @param Z result cell pointer
   /// @param A left operand cell
   virtual ErrorCode bif_or_bitwise(Cell * Z, const Cell * A) const;

   /// N over K for non-integer A and/or B
   /// @param Z result cell pointer
   /// @param A left operand cell (N in N!K)
   ErrorCode real_binomial(Cell * Z, const Cell * A) const;

   /// return the greatest common divisor of complex a and b
   /// @param z   result complex GCD
   /// @param a   first complex operand
   /// @param b   second complex operand
   /// @param qct comparison tolerance ⎕CT
   static ErrorCode cpx_gcd(APL_Complex & z, APL_Complex a, APL_Complex b,
                              double qct);

   /// multiply \b a by 1, -1, i, or -i so that a.real is maximal
   /// @param a complex number to rotate
   static APL_Complex cpx_max_real(APL_Complex a);

   /// return the greatest common divisor of real a and b
   /// @param z   result floating-point GCD
   /// @param a   first floating-point operand
   /// @param b   second floating-point operand
   /// @param qct comparison tolerance ⎕CT
   static ErrorCode flt_gcd(APL_Float & z, APL_Float a, APL_Float b,
                            double qct);

   /// return the greatest common divisor of integers a and b
   /// @param z result integer GCD
   /// @param a first integer operand
   /// @param b second integer operand
   static ErrorCode int_gcd(APL_Integer & z, APL_Integer a, APL_Integer b);

   /// N over K for positive integers N and K.
   /// @param Z      result cell pointer
   /// @param N      total count (top of binomial)
   /// @param K      selection count (bottom of binomial)
   /// @param negate true if result sign should be negated
   static ErrorCode integer_binomial(Cell * Z, APL_Integer N, APL_Integer K,
                                     bool negate);

   /// compute binomial funtion for integers a and b
   /// @param Z      result cell pointer
   /// @param N      total count (top of binomial)
   /// @param K      selection count (bottom of binomial)
   /// @param negate true if result sign should be negated
   static ErrorCode K33_binomial(Cell * Z, APL_Integer N,
                                 APL_Integer K, bool negate);
};
//════════════════════════════════════════════════════════════════════════════

#endif // __NUMERICCELL_HH_DEFINED__
