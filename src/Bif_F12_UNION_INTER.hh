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

#ifndef __BIF_F12_UNION_INTER_HH_DEFINED__
#define __BIF_F12_UNION_INTER_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** System function ∪ (unique/union) */
/// The class implementing ∪
class Bif_F12_UNION : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_UNION()
   : NonscalarFunction(TOK_F12_UNION)
   {}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_B()
   virtual Token eval_B(cValue_R B) const;

   /// Built-in function
   static Bif_F12_UNION  fun;
};
//────────────────────────────────────────────────────────────────────────────
/** System function ∩ (intersection) */
/// The class implementing ∩
class Bif_F2_INTER : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F2_INTER()
   : NonscalarFunction(TOK_F2_INTER)
   {}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   static Bif_F2_INTER  fun;   ///< Built-in function

protected:
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F12_UNION_INTER_HH_DEFINED__
