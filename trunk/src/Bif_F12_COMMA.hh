/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

#ifndef __BIF_COMMA_HH_DEFINED__
#define __BIF_COMMA_HH_DEFINED__

#include "PrimitiveFunction.hh"

//----------------------------------------------------------------------------
/** Comma related functions (catenate, laminate, and ravel.) */
/// Base class for , and ⍪
class Bif_COMMA : public NonscalarFunction_default_identity
{
public:
   /// Constructor
   /// @param tag token tag identifying this function variant
   Bif_COMMA(TokenTag tag)
   : NonscalarFunction_default_identity(tag)
   {}

   /// ravel along axis, with axis being the first (⍪( or last (,) axis of B
   /// @param X    axis specification value
   /// @param B    APL value to ravel
   /// @param axis default axis to use when X is absent
   static Token ravel_axis(Value_P X, Value_P B, uAxis axis);

   /// Return the ravel of B as APL value
   /// @param new_shape desired shape of the result
   /// @param B         APL value to ravel
   static Token ravel(const Shape & new_shape, Value_P B);

   /// Catenate A and B
   /// @param A    left APL value
   /// @param axis axis along which to catenate
   /// @param B    right APL value
   static Value_P catenate(const Value & A, sAxis axis, const Value & B);

   /// Laminate A and B
   /// @param A    left APL value
   /// @param axis fractional axis between existing axes
   /// @param B    right APL value
   static Value_P laminate(const Value & A, sAxis axis, const Value & B);

   /// either catenate A and B or laminate A and B
   /// @param A left APL value
   /// @param X axis specification (integer → catenate, fractional → laminate)
   /// @param B right APL value
   static Value_P catenate_or_laminate(const Value & A, const Value & X,
                                       const Value & B);

   /// Prepend scalar cell_A to B along axis
   /// @param cell_A scalar cell to prepend
   /// @param axis   axis along which to prepend
   /// @param B      APL value to prepend to
   static Value_P prepend_scalar(const Cell & cell_A, uAxis axis,
                                 const Value & B);

   /// Prepend scalar cell_B to A along axis
   /// @param A      APL value to append to
   /// @param axis   axis along which to append
   /// @param cell_B scalar cell to append
   static Value_P append_scalar(const Value & A, uAxis axis,
                                const Cell & cell_B);
};
//----------------------------------------------------------------------------
/** primitive functions catenate, laminate, and ravel along last axis */
/// The class implementing ,
class Bif_F12_COMMA : public Bif_COMMA
{
public:
   /// Constructor
   Bif_F12_COMMA()
   : Bif_COMMA(TOK_F12_COMMA)
   {}

   /// overloaded Function::eval_B()
   /// @param B right argument APL value
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_AB()
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_XB()
   /// @param X axis specification
   /// @param B right argument APL value
   virtual Token eval_XB(Value_P X, Value_P B) const
      { return ravel_axis(X, B, B->get_rank()); }

   /// overloaded Function::eval_AXB()
   /// @param A left argument APL value
   /// @param X axis specification
   /// @param B right argument APL value
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const
      { return Token(TOK_APL_VALUE1,
               catenate_or_laminate(*A, *X, *B)); }

   static Bif_F12_COMMA  fun;   ///< Built-in function

protected:
};
//----------------------------------------------------------------------------
/** primitive functions catenate and laminate along first axis, table */
/// The class implementing ⍪
class Bif_F12_COMMA1 : public Bif_COMMA
{
public:
   /// Constructor
   Bif_F12_COMMA1()
   : Bif_COMMA(TOK_F12_COMMA1)
   {}

   /// overloaded Function::eval_B()
   /// @param B right argument APL value
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_AB()
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_XB()
   /// @param X axis specification
   /// @param B right argument APL value
   virtual Token eval_XB(Value_P X, Value_P B) const
      { return ravel_axis(X, B, 0); }

  /// overloaded Function::eval_AXB()
   /// @param A left argument APL value
   /// @param X axis specification
   /// @param B right argument APL value
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const
      { return Token(TOK_APL_VALUE1, catenate_or_laminate(*A, *X, *B)); }

   static Bif_F12_COMMA1  fun;   ///< Built-in function

protected:
};
//----------------------------------------------------------------------------

#endif // __BIF_COMMA_HH_DEFINED__

