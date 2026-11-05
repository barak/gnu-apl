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

#ifndef __BIF_F1_EXECUTE_HH_DEFINED__
#define __BIF_F1_EXECUTE_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** System function execute */
/// The class implementing ⍎
class Bif_F1_EXECUTE : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F1_EXECUTE()
   : NonscalarFunction(TOK_F1_EXECUTE)
   {}

   /// overladed Function::eval_B()
   /// @param B  the right APL argument value
   virtual Token eval_B(cValue_R B) const;

   /// overloaded Function::eval_fill_B()
   /// @param B  the right APL argument value
   virtual Token eval_fill_B(cValue_R B) const;

   /// overloaded Function::eval_rank_fill_B(): genuinely executing ⍎ on a
   /// synthetic prototype value (its characters are not meaningful APL
   /// source) makes no sense; delegate to eval_fill_B()'s own TOK_VOID
   /// answer instead, the same one already used for Each/Outer Product.
   virtual Token eval_rank_fill_B(cValue_R B) const
      { return eval_fill_B(B); }

   /// execute string containing an APL command
   /// @param command  the APL command text to execute
   static Token execute_command(UCS_string & command);

   /// execute string containing an APL expression or an APL command
   /// @param statement  the APL expression or command text to execute
   static Token execute_statement(UCS_string & statement);

   /// the number of outstanding )COPYs with APL scipts
   static int copy_pending;

   static Bif_F1_EXECUTE  fun;   ///< Built-in function

protected:
   /// overladed Function::may_push_SI()
   virtual bool may_push_SI() const   { return true; }
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F1_EXECUTE_HH_DEFINED__
