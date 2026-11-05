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

#include "Bif_OPER1_EACH.hh"
#include "Common.hh"
#include "DerivedFunction.hh"
#include "Id.hh"
#include "Output.hh"
#include "PrintOperator.hh"
#include "StateIndicator.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
Fun_selectivity
DerivedFunction::get_selectivity() const
{
   // Each (¨) forwards a selective-assignment target straight through to
   // LO (Bif_OPER1_EACH.cc's own guards, confirmed empirically this
   // session -- see that file); so LO_¨'s selectivity IS this derived
   // function's selectivity. No other operator is known to do this
   // safely, so every other oper stays SEL_NONE (see the .hh comment).
   //
   if (oper == &Bif_OPER1_EACH::fun)
      {
        cFunction_P LO = get_LO();
        return LO ? LO->get_selectivity() : SEL_NONE;
      }

   // Bugs31 #22 (Blake McBride): a "plain function bound only to an
   // axis" (e.g. ⌽[1], no LO/RO at all -- see this class's own doc
   // comment) stores the function itself in `oper` (the constructor's
   // generic F_or_M_or_D slot), not a real operator -- its OWN
   // selectivity (e.g. Bif_F12_ROTATE.hh's SEL_MON_X/SEL_DYA_X) must
   // still be reported here, not discarded to SEL_NONE. Otherwise this
   // same DerivedFunction, when itself used as the LO of Each just
   // above (⌽[1]¨), always falls through to SEL_NONE via that path,
   // rejecting a legal selective-specification target like
   // (⌽[1]¨V)←B even though both the non-axis Each (⌽¨V)←B and the
   // axis form without Each (⌽[1]V)←B already work.
   //
   if (!get_LO() && !get_RO())   return oper->get_selectivity();

   return SEL_NONE;
}
//────────────────────────────────────────────────────────────────────────────
bool
DerivedFunction::has_monadic_form() const
{
   if (oper == &Bif_OPER1_EACH::fun)
      return get_LO() ? get_LO()->has_monadic_form() : true;
   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
DerivedFunction::has_dyadic_form() const
{
   if (oper == &Bif_OPER1_EACH::fun)
      return get_LO() ? get_LO()->has_dyadic_form() : true;
   return true;
}
//════════════════════════════════════════════════════════════════════════════
UCS_string
DerivedFunction::get_name() const
{
UCS_string name;
   name << "(";

   if (left_arg.is_function())   name << left_arg.get_function()->get_name();
   else                          name << "VAL";
   name << " ";

   name << oper->get_name();
   if (axis)   name << "[]";

   if (right_arg.get_tag() != TOK_VOID)   // dyadic operator
      {
        name << " ";
        if (right_arg.is_function())
           name << right_arg.get_function()->get_name();
        else
           name << "VAL";
      }

   name << ")";
   return name;
}
//────────────────────────────────────────────────────────────────────────────
bool
DerivedFunction::has_result() const
{
   if (!oper->has_result())   return false;   // unlikely
   if (left_arg.is_function())   return left_arg.get_function()->has_result();
   return true;   // operator bound to left value
}
//────────────────────────────────────────────────────────────────────────────
void
DerivedFunction::unmark_all_values() const
{
   if (axis)   axis->unmark();
   if (left_arg.is_apl_val())    left_arg.get_apl_val()->unmark();

   // right_arg, not just left_arg and axis (Bugs30 #26): for a dyadic
   // operator with a value right operand (e.g. f⍤y, f⍣N), that value is
   // held here, exactly like axis/left_arg -- and destroy_derived()
   // just below already clears all three symmetrically. Omitting it
   // left a value genuinely still owned by this suspended frame's
   // derived-function cache permanently marked, so Value::print_stale()
   // reported it as a false-positive stale value until the next )SIC.
   //
   if (right_arg.is_apl_val())   right_arg.get_apl_val()->unmark();
}
//────────────────────────────────────────────────────────────────────────────
void
DerivedFunction::destroy_derived(const char * loc)
{
   Log(LOG_FunOperX)
      {
        CERR << "DerivedFunction::destroy_derived("
             << get_name() << ")" << endl;
      }

   left_arg.clear(loc);
   right_arg.clear(loc);
   axis.clear(loc);
}
//────────────────────────────────────────────────────────────────────────────
DerivedFunction::DerivedFunction(const Token * LO, cFunction_P F_or_M_or_D,
                                 const Token * RO, Value_P X, const char * loc)
   : Function(ID_USER_SYMBOL, TOK_FUN2),
     left_arg(LO ? *LO : Token()),
     oper(F_or_M_or_D),
     right_arg(RO ? *RO : Token() ),
     axis(X)
{
   Assert1(oper);

const char * sepa = "";
   Log(LOG_FunOperX)
      {
        CERR << "DerivedFunction(";
        if (left_arg.get_tag() != TOK_VOID)
           {
             CERR << "LO";
             sepa = ", ";
           }
        CERR << "F";
        sepa = ", ";
        if (right_arg.get_tag() != TOK_VOID)
           {
             CERR << sepa << "RO";
             sepa = ", ";
           }
        if (X)
           {
             CERR << sepa << "X";
             sepa = ", ";
           }
        CERR << ") at " << loc << endl;
     }
}
//────────────────────────────────────────────────────────────────────────────
DerivedFunction::~DerivedFunction()
{
   Log(LOG_FunOperX)
      {
        CERR << "~DerivedFunction()" << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
DerivedFunction::entering(const char * fun_name) const
{
   CERR << "entering DerivedFunction";
   print(CERR);
   CERR << "::" << fun_name << "() , this = " << voidP(this) << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
DerivedFunction::print_properties(ostream & out, int indent) const
{
UCS_string ind(indent, UNI_SPACE);
   out << ind << "Function derived from operator" << endl
       << ind << "Left Function: ";
   if (left_arg.is_function())   left_arg.get_function()->print(out);
   else                          out << "VAL";
   out << endl << ind << "Operator:  ";
   oper->print(out);
   if (axis)   out << "Axis: " << *axis << endl;

   if (right_arg.get_tag() != TOK_VOID)   // dyadic operator
      {
         out << ind << "Right Function:  ";
        if (right_arg.is_function())   right_arg.get_function()->print(out);
        else                           out << "VAL";
      }

   out << endl;
}
//════════════════════════════════════════════════════════════════════════════
// The 4 eval_XXX() below unify what used to be 5 separate subclasses (see
// the class comment in DerivedFunction.hh). Each dispatches purely on
// which of left_arg/right_arg/axis are populated -- exactly reproducing
// each of the 5 old shapes' behavior:
//
//   left_arg   right_arg   axis(member)   old class
//   --------   ---------   ------------   -----------------
//   unset      unset       set            Derived_F_X
//   set        unset       unset          Derived_LO_M
//   set        unset       set            Derived_LO_M_X
//   set        set         unset          Derived_LO_D_RO
//   set        set         set            Derived_LO_D_X_RO
//
// eval_XB()/eval_AXB() (the call-time-axis entry points) fall to
// Function::eval_XB()/eval_AXB()'s phrase_error() default whenever
// no_call_time_axis() is true, i.e. for Derived_F_X (no left operand at
// all) and Derived_LO_D_X_RO (already has LO+RO+a member axis) -- neither
// of those two shapes ever supported a further call-time axis; that
// non-support is preserved exactly, not "completed".
//────────────────────────────────────────────────────────────────────────────
Token
DerivedFunction::eval_AB(cValue_R A, cValue_R B) const
{
   Log(LOG_FunOperX)   entering("eval_AB");

   if (right_arg.get_tag() != TOK_VOID)   // dyadic operator (LO D RO)
      {
        Token & left  = const_cast<Token &>(left_arg);
        Token & right = const_cast<Token &>(right_arg);
        if (!axis)   return oper->eval_ALRB(A, left, right, B);
        else         return oper->eval_ALRXB(A, left, right, *axis, B);
      }

   if (left_arg.get_tag() != TOK_VOID)   // monadic operator (LO M)
      {
        // eval_ALB()/eval_ALXB() already push LO as a function OR a
        // value correctly (UserFunction::eval_ALB()). Bugs27 #50.
        Token & left = const_cast<Token &>(left_arg);
        if (!axis)   return oper->eval_ALB(A, left, B);
        else         return oper->eval_ALXB(A, left, *axis, B);
      }

   // plain function bound to an axis, no operator (F X)
   return oper->eval_AXB(A, *axis, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
DerivedFunction::eval_B(cValue_R B) const
{
   Log(LOG_FunOperX)   entering("eval_B");

   if (right_arg.get_tag() != TOK_VOID)   // dyadic operator (LO D RO)
      {
        Token & left  = const_cast<Token &>(left_arg);
        Token & right = const_cast<Token &>(right_arg);
        if (!axis)   return oper->eval_LRB(left, right, B);
        else         return oper->eval_LRXB(left, right, *axis, B);
      }

   if (left_arg.get_tag() != TOK_VOID)   // monadic operator (LO M)
      {
        // eval_LB()/eval_LXB() already push LO as a function OR a
        // value correctly (UserFunction::eval_LB()). Bugs27 #50.
        Token & left = const_cast<Token &>(left_arg);
        if (!axis)   return oper->eval_LB(left, B);
        else         return oper->eval_LXB(left, *axis, B);
      }

   // plain function bound to an axis, no operator (F X)
   return oper->eval_XB(*axis, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
DerivedFunction::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   Log(LOG_FunOperX)   entering("eval_AXB");

   if (no_call_time_axis())   return Function::eval_AXB(A, X, B);

   if (right_arg.get_tag() != TOK_VOID)   // dyadic operator (LO D RO)
      {
        Token & left  = const_cast<Token &>(left_arg);
        Token & right = const_cast<Token &>(right_arg);
        return oper->eval_ALRXB(A, left, right, X, B);
      }

   // monadic operator (LO M or LO M X); call-time X overrides any
   // member axis
   Token & left = const_cast<Token &>(left_arg);
   return oper->eval_ALXB(A, left, X, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
DerivedFunction::eval_XB(cValue_R X, cValue_R B) const
{
   Log(LOG_FunOperX)   entering("eval_XB");

   if (no_call_time_axis())   return Function::eval_XB(X, B);

   if (right_arg.get_tag() != TOK_VOID)   // dyadic operator (LO D RO)
      {
        Token & left  = const_cast<Token &>(left_arg);
        Token & right = const_cast<Token &>(right_arg);
        return oper->eval_LRXB(left, right, X, B);
      }

   // monadic operator (LO M or LO M X). Use the call-time axis X, not
   // a (possibly unset, or already-baked-in) member axis -- this was
   // the actual bug behind (+/)[1]M silently ignoring [1]. Bugs27 #50.
   //
   // Bugs28 #91: that fix was right when the member axis is UNSET
   // (0, e.g. (+/)[1]M -- no_call_time_axis() above already handles
   // the "left_arg is TOK_VOID" no-LO shape, but not this one), but it
   // silently overrode one that was already baked in at parse time
   // (e.g. "+/[1]" via reduce_F_M_C_) instead of rejecting a second,
   // redundant axis -- so +/[1][2]M silently discarded the [1] and
   // used [2], instead of SYNTAX ERROR the way every OTHER two-axis
   // form on a derived function already is.
   //
   if (axis)   SYNTAX_ERROR;

   Token & left = const_cast<Token &>(left_arg);
   return oper->eval_LXB(left, X, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
DerivedFunction::eval_fill_AB(cValue_R A, cValue_R B) const
{
   // Function's own default (a hard DOMAIN_ERROR) is too conservative
   // for a simple case like +/, +\, or ⌽¨ used as the LO of an empty-
   // frame LO⍤y (Bif_OPER2_RANK.cc's do_ALyXB()/do_LyXB()) or similarly
   // as the RO of an empty A∘.RO B (Bif_OPER2_OUTER.cc) -- those
   // callers only need the SHAPE of one chunk's result (they reshape
   // or take a single prototype cell from it afterward, discarding the
   // actual values), so simply evaluating normally on the placeholder
   // argument(s) they pass in is safe and correct, mirroring
   // PrimitiveFunction::eval_fill_AB()'s identical "just evaluate"
   // approach.
   //
   // But it is NOT safe unconditionally: a derived function whose
   // chain may_push_SI() (e.g. {⍵+1}⍣2 -- LO is a lambda) can, when
   // actually evaluated here, end up invoking a macro-implemented
   // operator (Z__LO_POWER_N_B etc.) from deep inside this same call
   // (do_LyXB() -> eval_fill_B() -> eval_B() -> the macro), a context
   // the macro's own error-reporting does not expect -- confirmed live
   // to hit an internal Assert (Executable.cc:443,
   // body_from_to.low != -1) rather than a clean DOMAIN_ERROR, for
   // ({⍵+1}⍣2⍤1)0 3⍴0. Falling back to the same hard DOMAIN_ERROR as
   // before for that shape is not worse than the pre-fix behavior (a
   // DOMAIN_ERROR either way) -- fixing that macro/nested-SI
   // interaction for real is a separate, deeper piece of work. See
   // Bugs27 #28.
   //
   if (may_push_SI())   return Function::eval_fill_AB(A, B);
   return eval_AB(A, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
DerivedFunction::eval_fill_B(cValue_R B) const
{
   // see eval_fill_AB() above for the may_push_SI() guard's rationale.
   //
   if (may_push_SI())   return Function::eval_fill_B(B);
   return eval_B(B);
}
//════════════════════════════════════════════════════════════════════════════
// DerivedFunctionCache's constructor, destructor, get(), reserve_slot(),
// construct_at(), and reset() are all trivial enough to live inline in
// DerivedFunction.hh; nothing further to define here.
//════════════════════════════════════════════════════════════════════════════
