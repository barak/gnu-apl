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

#ifndef __PRIMITIVE_OPERATOR_HH_DEFINED__
#define __PRIMITIVE_OPERATOR_HH_DEFINED__

#include "PrimitiveFunction.hh"

class BeamIterator;

//════════════════════════════════════════════════════════════════════════════
/// Base class for all primitive APL operators
class PrimitiveOperator : public PrimitiveFunction
{
public:
   /// Constructor.
   /// @param tag token tag identifying this operator
   PrimitiveOperator(TokenTag tag) : PrimitiveFunction(tag) {}

   /// overloaded Function::get_fun_valence()
   virtual int get_fun_valence() const   { return 2; }

   /// overloaded Function::get_oper_valence(). Most primitive operators are
   /// monadic, so we return 1 and overload dyadic operators (i.e. inner/outer
   /// product) to return 2
   virtual int get_oper_valence() const   { return 1; }

   /// overloaded Function::is_operator()
   virtual bool is_operator() const   { return true; }

   /// overloaded Function::has_monadic_form(): a bare, unbound operator
   /// (e.g. ¨, ⍤, ⍣, ∘. used without their operand(s) correctly bound
   /// into a DerivedFunction first -- the "F OPER" fragment reaching
   /// eval_B()/eval_AB() directly) is never itself directly callable;
   /// only a DerivedFunction wrapping it (with LO/RO bound) is. Default
   /// false for both; a subclass that ALSO doubles as a genuine plain
   /// function (e.g. Bif_REDUCE's Replicate role for dyadic /) must
   /// override the applicable one back to true.
   virtual bool has_monadic_form() const   { return false; }

   /// overloaded Function::has_dyadic_form(): see has_monadic_form().
   virtual bool has_dyadic_form() const   { return false; }
};
//════════════════════════════════════════════════════════════════════════════

#endif // __PRIMITIVE_OPERATOR_HH_DEFINED__
