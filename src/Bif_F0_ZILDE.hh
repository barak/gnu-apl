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

#ifndef __BIF_F0_ZILDE_HH_DEFINED__
#define __BIF_F0_ZILDE_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** System function zilde (⍬) */
/// The class implementing ⍬ (the empty numeric vector)
class Bif_F0_ZILDE : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F0_ZILDE()
   : NonscalarFunction(TOK_F0_ZILDE)
   {}

   /// overladed Function::eval_()
   virtual Token eval_() const;

   static Bif_F0_ZILDE  fun;   ///< Built-in function

protected:
   /// overladed Function::may_push_SI()
   virtual bool may_push_SI() const   { return false; }
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F0_ZILDE_HH_DEFINED__
