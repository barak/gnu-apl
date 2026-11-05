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

#ifndef __QUAD_FX_HH_DEFINED__
#define __QUAD_FX_HH_DEFINED__

#include "QuadFunction.hh"

//════════════════════════════════════════════════════════════════════════════
/**
   The system function ⎕FX (Fix)
 */
/// The class implementing ⎕FX
class Quad_FX : public QuadFunction
{
public:
   /// Constructor.
   Quad_FX() : QuadFunction(TOK_Quad_FX) {}

   /// overloaded Function::eval_AB().
   /// @param A left APL value argument (execution properties or library name)
   /// @param B right APL value argument (function text)
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return do_eval_AB(A, B); }

   /// overloaded Function::eval_B().
   /// @param B right APL value argument (function text)
   virtual Token eval_B(cValue_R B) const
      { return do_eval_B(B); }

   /// overloaded Function::eval_AXB().
   /// @param A left APL value argument (library name)
   /// @param X axis specification value
   /// @param B right APL value argument (function name)
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// implementation of eval_AB()
   /// @param A raw APL value for execution properties or library name
   /// @param B raw APL value containing the function text
   static Token do_eval_AB(cValue_R A, cValue_R B);

   /// implementation of eval_B()
   /// @param B raw APL value containing the function text
   static Token do_eval_B(cValue_R B);

   /// do ⎕FX with execution properties \b exec_props
   /// @param exec_props array of four execution property integers
   /// @param B APL value containing the function text
   /// @param creator identifier string recorded as the function creator
   /// @param honor_lock false for dyadic ⎕FX (Bugs28 #92): its left
   ///        argument is the documented way to change a function's
   ///        execution properties, including its lock bit
   static Token do_quad_FX(const int * exec_props, cValue_R B,
                           const UTF8_string & creator,
                           bool honor_lock = true);

   static Quad_FX  fun;   ///< Built-in function.

   /// ⎕FX \b text for libapl's fix_function() (do_quad_FX() is protected;
   /// libapl.cc used to call it through a hand-made copy of this class
   /// whose signature got out of date, Bugs34 #2)
   /// @param exec_props array of four execution property integers
   /// @param text UCS function text with lines separated by LF
   /// @param creator identifier string recorded as the function creator
   static Token fix_for_libapl(const int * exec_props,
                               const UCS_string & text,
                               const UTF8_string & creator)
      { return do_quad_FX(exec_props, text, creator); }

protected:
   /// ⎕FX with native function and optional library reference
   /// @param A APL value containing the shared-library path
   /// @param axis axis value selecting the native function variant
   /// @param B APL value containing the APL function name
   static Value_P do_native_FX(cValue_R A, sAxis axis, cValue_R B);

   /// do ⎕FX with execution properties \b exec_props
   /// @param exec_props array of four execution property integers
   /// @param text UCS function text with lines separated by LF
   /// @param creator identifier string recorded as the function creator
   /// @param honor_lock see the cValue_R B overload above
   static Token do_quad_FX(const int * exec_props, const UCS_string & text,
                           const UTF8_string & creator,
                           bool honor_lock = true);
};
//════════════════════════════════════════════════════════════════════════════
#endif //  __QUAD_FX_HH_DEFINED__

