/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2020-2026  Dr. Jürgen Sauermann

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

#ifndef __Bif_F12_INTERVAL_INDEX_HH_DEFINED__
#define __Bif_F12_INTERVAL_INDEX_HH_DEFINED__

#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** System function interval index (⍸) */
/// The class implementing ⍸
class Bif_F12_INTERVAL_INDEX: public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_INTERVAL_INDEX()
   : NonscalarFunction(TOK_F12_INTERVAL_INDEX)
   {}

   /// overloaded Function::eval_AB()
   /// @param A left argument APL value (sorted interval boundaries)
   /// @param B right argument APL value (values to locate in intervals)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const;

   static Bif_F12_INTERVAL_INDEX  fun;   ///< Built-in function

protected:
   /// find cell in ranges defined by ranges and range_count
   /// @param cell the cell value to locate
   /// @param ranges pointer to the sorted array of interval-boundary cells
   /// @param range_count number of elements in ranges
   static ShapeItem find_range(const Cell & cell, const Cell * ranges,
                               ShapeItem range_count);
};
//────────────────────────────────────────────────────────────────────────────

#endif // __Bif_F12_INTERVAL_INDEX_HH_DEFINED__


