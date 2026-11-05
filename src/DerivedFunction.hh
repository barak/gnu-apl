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

#ifndef __DERIVED_FUNCTION__DEFINED__
#define __DERIVED_FUNCTION__DEFINED__

#include <deque>

#include "Error.hh"
#include "Function.hh"
#include "Output.hh"

//════════════════════════════════════════════════════════════════════════════
/** Binds an axis and/or left and/or right function or operator arguments
    to a function or operator.

    This used to be a base class (DerivedFunction) plus 5 subclasses
    (Derived_F_X, Derived_LO_M, Derived_LO_M_X, Derived_LO_D_RO,
    Derived_LO_D_X_RO) -- one per combination of (has a left operand LO?,
    has a right operand RO?, has an axis?) that can occur. All 5 stored
    EXACTLY the same 4 fields (left_arg, oper, right_arg, axis; confirmed
    identical sizeof() for all 5) and differed only in which eval_XXX()
    overrides they provided and how those overrides dispatched to `oper`.
    Merged into one concrete class 2026-09-03 (Bugs27 #13 investigation):
    the 5-way split was pure ceremony around the same 4 fields, and made
    it easy for a fix to one shape's eval_XXX() to be missed in a sibling
    shape's near-identical copy (see Bugs27 #50, where 4 of the 5 had the
    identical wrong-function-called bug in their "value LO" case and the
    fix had to be applied 4 times by hand). One eval_XXX() implementation
    per entry point now dispatches on which of left_arg/right_arg/axis
    are actually populated, exactly reproducing each of the 5 old
    shapes' behavior (including the two shapes -- F bound only to an
    axis, and a dyadic operator bound to LO+RO+axis -- that deliberately
    did NOT support eval_XB()/eval_AXB(), falling instead to
    Function::eval_XB()/eval_AXB()'s phrase_error() default; that
    non-support is preserved exactly, not "completed").
 **/
/// An APL function operator bound to its function operand(s) and/or axis.
class DerivedFunction : public Function
{
public:
   /// default constructor (for DerivedFunctionCache items)
   DerivedFunction() : Function(TOK_FUN0)   {}

   /// constructor: bind LO (if any), the function/operator F_or_M_or_D,
   /// RO (if any), and an axis X (if any) together.
   /// @param LO token for the left operand (function or value), or 0 if
   ///        this is a plain function bound to an axis with no operator
   /// @param F_or_M_or_D the function, monadic operator, or dyadic
   ///        operator being bound
   /// @param RO token for the right operand (function or value), or 0
   ///        if F_or_M_or_D is not a dyadic operator
   /// @param X axis value, or an empty Value_P if no axis is bound
   /// @param loc caller location for diagnostics
   DerivedFunction(const Token * LO, cFunction_P F_or_M_or_D, const Token * RO, Value_P X,
                   const char * loc);

   /// return the axis argument (or 0 if none) of this derived function
   const cValue * get_AXIS() const
      { return axis.get(); }

   /// return the value (if any) bound to an operator (that allows it)
   Value_P get_bound_LO_value() const
      {
        return left_arg.is_apl_val() ? left_arg.get_apl_val() : Value_P();
      }

   /// return the value (if any) bound as the right operand -- e.g. ⍤'s
   /// own axis-like right operand in F⍤y (y a value, not a function).
   /// Mirrors get_bound_LO_value(); see also get_RO()'s own comment.
   Value_P get_bound_RO_value() const
      {
        return right_arg.is_apl_val() ? right_arg.get_apl_val() : Value_P();
      }

   /// return the left operand (or 0) of this derived function -- 0 for
   /// the "plain function bound only to an axis" shape, which has no
   /// left operand at all (Token::get_function() would SYNTAX_ERROR on
   /// its TOK_VOID left_arg otherwise; mirrors get_RO()'s same guard).
   cFunction_P get_LO() const
      {
        return left_arg.get_tag() == TOK_VOID ? 0 : left_arg.get_function();
      }

   /// return the operator of this derived function
   cFunction_P get_OPER() const
      { return oper; }

   /// return the right operand (or 0) of this derived function
   cFunction_P get_RO() const
      {
        return right_arg.get_tag() == TOK_VOID ? 0 : right_arg.get_function();
      }

   /// overloaded Function::is_derived();
   virtual bool is_derived() const
      { return true; }

   /// overloaded Function::print();
   virtual ostream & print(ostream & out) const
      { return out << get_name(); }

   /// overloaded NamedObject::get_name()
   virtual UCS_string get_name() const;

   /// Overloaded Function::has_result();
   virtual bool has_result() const;

   /// clear the marked bit in values bound to this derived functions (if any).
   void unmark_all_values() const;   // unmark values bound to this operator

   /// deallocate resources held by this DerivedFunction
   /// @param loc caller location for diagnostics
   void destroy_derived(const char * loc);

   /// overloaded Function::eval_AB()
   /// @param A left APL argument value
   /// @param B right APL argument value
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_B()
   /// @param B right APL argument value
   virtual Token eval_B(cValue_R B) const;

   /// overloaded Function::eval_AXB(). Falls to Function::eval_AXB()'s
   /// phrase_error() default when this instance has no left operand
   /// (plain F bound to an axis) or has both a left AND a right operand
   /// AND an axis already bound (a dyadic operator bound to LO+RO+axis)
   /// -- neither of those two shapes ever supported a further call-time
   /// axis, matching the original 5-class behavior exactly.
   /// @param A left APL argument value
   /// @param X axis specification value
   /// @param B right APL argument value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// overloaded Function::eval_XB(). See eval_AXB() above for when this
   /// falls to the phrase_error() default instead.
   /// @param X axis specification value
   /// @param B right APL argument value
   virtual Token eval_XB(cValue_R X, cValue_R B) const;

   /// overloaded Function::eval_fill_AB(). Falls to Function's own
   /// default (a hard DOMAIN_ERROR) if may_push_SI() -- see the .cc
   /// file for why. Otherwise just evaluates normally. Used by e.g.
   /// Bif_OPER2_RANK's empty-frame handling when this derived function
   /// (e.g. +/, +\, ⌽¨) is itself the LO of LO⍤y. See Bugs27 #28.
   /// @param A left APL argument value
   /// @param B right APL argument value
   virtual Token eval_fill_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_fill_B(). See eval_fill_AB() above.
   /// @param B right APL argument value
   virtual Token eval_fill_B(cValue_R B) const;

   /// destructor. Public because DerivedFunctionCache now stores
   /// DerivedFunction objects in a std::deque, whose own construction/
   /// destruction machinery must be able to call it directly.
   ~DerivedFunction();

protected:
   /// overloaded Function::locate_X()
   virtual Value_P * locate_X() const
      { return !axis ? 0 : const_cast<Value_P *>(&axis); }

   /// overloaded Function::may_push_SI()
   virtual bool may_push_SI() const
      { return   oper->may_push_SI()
        || (left_arg .is_function() && left_arg .get_function()->may_push_SI())
        || (right_arg.is_function() && right_arg.get_function()->may_push_SI());
      }

   /// overloaded Function::get_selectivity(). Unlike may_push_SI() (a
   /// safe blanket OR: if anything involved may push the SI, so may
   /// this), get_selectivity() is NOT blanket-delegated to every
   /// operator's LO -- getting it wrong would be a security hole again
   /// (a leaking LvalCell reaching a defined function through an
   /// operator whose own internals were never checked for it), not just
   /// a spurious rejection. Only Each (¨) is known to forward its LO's
   /// selectivity correctly (Bif_OPER1_EACH.cc's own guards, confirmed
   /// empirically); every other operator (⍤, ⍣, ∘., commute, ...)
   /// stays SEL_NONE here until it gets the same audit. See the .cc file.
   virtual Fun_selectivity get_selectivity() const;

   /// overloaded Function::has_monadic_form(). For Each (¨), do_eval_LB
   /// calls LO->eval_B() per element, so whether "LO¨" genuinely has a
   /// monadic form is entirely LO's own has_monadic_form() (e.g. ↓¨ has
   /// none, since ↓ has none -- confirmed regression without this:
   /// (↓¨V)←9 would otherwise report SYNTAX ERROR instead of VALENCE
   /// ERROR, the exact same masking bug has_monadic_form() exists to
   /// prevent, just one level up through a bound derived function
   /// instead of a bare primitive). Every other operator defaults to
   /// Function's own true (unaudited; eval_B() is a real override for
   /// all of them, so at worst this costs a SYNTAX ERROR where a more
   /// precise VALENCE ERROR was possible, not a safety issue). See the
   /// .cc file (avoids a Bif_OPER1_EACH.hh include here).
   virtual bool has_monadic_form() const;

   /// overloaded Function::has_dyadic_form(). See has_monadic_form().
   virtual bool has_dyadic_form() const;

   /// debug printout when an eval_XXX() function is called.
   /// @param fun_name name of the eval function being entered
   void entering(const char * fun_name) const;

   /// Overloaded Function::print_properties()
   virtual void print_properties(ostream & out, int indent) const;

   /// true iff a call-time axis (eval_XB()/eval_AXB()) is not supported
   /// by this instance's shape -- see eval_AXB() above.
   bool no_call_time_axis() const
      { return axis && (right_arg.get_tag() != TOK_VOID ||
                        left_arg.get_tag() == TOK_VOID); }

   /// the function (to the left of the operator).
   // Token are ephemeral, therefore we need a copy here.
   Token left_arg;

   /// the monadic operator (to the right of the function)
   cFunction_P oper;

   /// the (normally) function on the right of the (dyadic) operator (if any).
   //  Acording to lrm p. 35, the right operand is a function or array (even
   //  though no primitive dyadic operator takes an array as right operand).
   // Token are ephemeral, therefore we need a copy here.
   Token right_arg;

   /// the axis for \b mon_oper, or 0 if no axis
   Value_P axis;
};
//════════════════════════════════════════════════════════════════════════════
/// A cache owning the DerivedFunction objects created within one )SI
/// frame (see StateIndicator::fun_oper_cache).
///
/// Backed by std::deque rather than std::vector: a handful of call
/// sites (e.g. Prefix.cc's reduce_F_C_M_(), and the )LOAD archive
/// reconstruction path below) capture the raw address of one derived
/// function while constructing ANOTHER one in the same cache -- that
/// address must stay valid no matter how many more entries are added
/// afterwards. std::vector cannot promise that (growing it reallocates
/// and moves every existing element); std::deque can (push_back()/
/// emplace_back() never invalidates references to existing elements).
/// MAX_FUN_OPER (the historical fixed-array size) is therefore no
/// longer a hard cap -- the cache simply grows on demand.
class DerivedFunctionCache
{
public:
   /// constructor: create empty FunOper cache
   DerivedFunctionCache()   {}

   /// destructor
   ~DerivedFunctionCache()   { reset(); }

   /// return the i'th derived function
   /// @param i zero-based index into the cache
   const DerivedFunction & operator [](size_t i) const
      { Assert(i < cache.size());   return cache[i]; }

   /// return the number of items in the cache
   size_t size() const
      { return cache.size(); }

   /// clear the marked bit in values bound to derived functions (if any).
   void unmark_all_values() const
      { loop(d, size())   cache[d].unmark_all_values(); }

   /// construct a new derived function and return a pointer to it.
   /// @param LO token for the left operand (or 0 if absent)
   /// @param F_or_M_or_D the function, monadic, or dyadic operator
   /// @param RO token for the right operand (or 0 if absent)
   /// @param X optional axis value (empty Value_P if absent)
   /// @param loc caller location for diagnostics
   DerivedFunction * get(const Token * LO, cFunction_P F_or_M_or_D, const Token * RO,
                        Value_P X, const char * loc)
      {
        cache.emplace_back(LO, F_or_M_or_D, RO, X, loc);
        return &cache.back();
      }

   /// reserve a placeholder slot (default-constructed) whose real
   /// content is filled in later via construct_at(), once it is known
   /// -- used by the )LOAD archive path, where a derived function's
   /// LO/OPER/RO/axis fids may not all be resolvable until a later
   /// pass. The returned address stays valid across any number of
   /// further get()/reserve_slot() calls on this same cache (see the
   /// class comment above).
   /// @param loc caller location for diagnostics
   DerivedFunction * reserve_slot(const char * loc)
      {
        cache.emplace_back();
        return &cache.back();
      }

   /// give a slot previously handed out by reserve_slot() its real
   /// content, now that LO/OPER/RO/axis are known.
   /// @param slot address previously returned by reserve_slot()
   /// @param LO token for the left operand (or 0 if absent)
   /// @param F_or_M_or_D the function, monadic, or dyadic operator
   /// @param RO token for the right operand (or 0 if absent)
   /// @param X optional axis value (empty Value_P if absent)
   /// @param loc caller location for diagnostics
   static void construct_at(DerivedFunction * slot, const Token * LO,
                            cFunction_P F_or_M_or_D, const Token * RO, Value_P X,
                            const char * loc)
      {
        slot->~DerivedFunction();
        new (slot) DerivedFunction(LO, F_or_M_or_D, RO, X, loc);
      }

   /// reset (clear) the cache
   void reset()
      { cache.clear(); }

protected:
   /// the derived functions created within this )SI frame
   std::deque<DerivedFunction> cache;
};
//════════════════════════════════════════════════════════════════════════════
#endif // __DERIVED_FUNCTION__DEFINED__
