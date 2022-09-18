/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2022  Dr. Jürgen Sauermann

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

#ifndef __Quad_SQL_HH_DEFINED__
#define __Quad_SQL_HH_DEFINED__

#include "QuadFunction.hh"
#include "Value.hh"

//----------------------------------------------------------------------------
/**
   The system function ⎕SQL
 */
/// The class implementing ⎕SQL
class Quad_SQL : public QuadFunction
{
public:
   /// Constructor.
   Quad_SQL();

   /// Destructor
   ~Quad_SQL();

   static Quad_SQL * fun;          ///< Built-in function.
   static Quad_SQL  _fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_B().
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_XB().
   virtual Token eval_XB(Value_P X, Value_P B) const;

// virtual Token eval_AB(Value_P A, Value_P B) const;

};
//----------------------------------------------------------------------------

#endif // __Quad_SQL_HH_DEFINED__


