/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2019  Dr. Jürgen Sauermann

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

#ifndef __BIF_F12_TAKE_DROP_HH_DEFINED__
#define __BIF_F12_TAKE_DROP_HH_DEFINED__

#include "PrimitiveFunction.hh"

//-----------------------------------------------------------------------------

/** primitive functions Take and First */
/// The class implementing ↑
class Bif_F12_TAKE : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_TAKE()
   : NonscalarFunction(TOK_F12_TAKE)
   {}

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B)
      { return Token(TOK_APL_VALUE1, first(B));}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B);

   /// overloaded Function::eval_AXB()
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B);

   /// Take from B according to ravel_A
   static Token do_take(const Shape shape_Zi, Value_P B);

   /// Fill Z with B, pad as necessary
   static void fill(const Shape & shape_Zi, Cell * cZ, Value & Z_owner,
                    Value_P B);

   static Bif_F12_TAKE * fun;   ///< Built-in function
   static Bif_F12_TAKE  _fun;   ///< Built-in function

   /// ↑B
   static Value_P first(Value_P B);

protected:
   /// Take A from B
   Token take(Value_P A, Value_P B);
};
//-----------------------------------------------------------------------------
/** System function drop */
/// The class implementing ↓
class Bif_F12_DROP : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_DROP()
   : NonscalarFunction(TOK_F12_DROP)
   {}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B);

   /// overloaded Function::eval_AXB()
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B);

   static Bif_F12_DROP * fun;   ///< Built-in function
   static Bif_F12_DROP  _fun;   ///< Built-in function

protected:

};
//-----------------------------------------------------------------------------
#endif // __BIF_F12_TAKE_DROP_HH_DEFINED__
