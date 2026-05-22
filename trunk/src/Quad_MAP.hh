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

#ifndef __Quad_MAP_DEFINED__
#define __Quad_MAP_DEFINED__

#include "QuadFunction.hh"

//----------------------------------------------------------------------------
/// The implementation of ⎕MAP
class Quad_MAP : public QuadFunction
{
public:
   /// Constructor.
   Quad_MAP()
      : QuadFunction(TOK_Quad_MAP)
   {}

   static Quad_MAP  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB()
   /// @param A left argument APL value (the mapping table)
   /// @param B right argument APL value (the data to map)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// Heapsort helper
   /// @param a first ShapeItem index to compare
   /// @param b second ShapeItem index to compare
   /// @param cells comparison context (pointer to ravel_comp_len)
   static bool greater_map(const ShapeItem & a, const ShapeItem & b,
                           const void * cells);

   /// compute ⎕MAP with (indices of) sorted A
   /// @param A the mapping table value
   /// @param ordered_indices_A sorted indices into A's key column
   /// @param B the data value to map
   /// @param recursive true to apply mapping recursively to nested elements
   static Value_P do_map(const Value & A, const vector<ShapeItem> ordered_indices_A,
                         const Value * B, bool recursive);
};
//----------------------------------------------------------------------------

#endif // __Quad_MAP_DEFINED__

