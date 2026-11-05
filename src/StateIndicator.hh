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

#ifndef __STATE_INDICATOR_HH_DEFINED__
#define __STATE_INDICATOR_HH_DEFINED__

#include "Common.hh"
#include "DerivedFunction.hh"
#include "Executable.hh"
#include "Error.hh"
#include "Function.hh"
#include "Parser.hh"
#include "Prefix.hh"
#include "PrintOperator.hh"

//════════════════════════════════════════════════════════════════════════════
/**
    One entry of the state indicator (SI) of the APL interpreter.
    Compared to e.g.  C++, the state indicator is (one element of) the
    function call stack of the interpreter.
 */
/// One entry of the state indicator (SI) of the APL interpreter
class StateIndicator
{
   friend class XML_Loading_Archive;
   friend class XML_Saving_Archive;

public:
   /// display \b B like the result of a statement (the print length limit
   /// in ⎕SYL and, if \b boxing, ]BOXING apply)
   /// @param B the value to display
   /// @param boxing true if ]BOXING applies
   static void display_value(Value_P B, bool boxing = true);

   /// constructor
   /// @param exec executable (function or statements) being entered
   /// @param _par parent SI entry (caller), or null for the outermost level
   StateIndicator(const Executable * exec, StateIndicator * _par);

   /// destructor
   ~StateIndicator();

   /// return the depth of this )SI entry
   int get_depth() const
       {
         int ret = 0;
         for (const StateIndicator * p = this; p->parent; p = p->parent)  ++ret;
         return ret;
       }

   /// return pointer to the current user function, statements, or execute
   const Executable * get_executable() const
      { return executable; }

   /// return the nesting level (oldest SI has level 0, next has level 1, ...)
   SI_level get_level() const   { return level; };

   /// return the mode of this entry
   ParseMode get_parse_mode() const
      { return executable->get_parse_mode(); }

   /// return the SI entry that has called \b this one
   StateIndicator * get_parent() const
      { return parent; }

   /// return execution property \b idx (0=locked, 1=nonsuspendable,
   /// 2=ignore-attention, 3=convert-errors-to-DOMAIN-ERROR), OR-ed with
   /// the same property of every calling (enclosing) )SI entry -- per
   /// apl2lrm.txt p.360-361: "The execution properties of a called
   /// function or operator during an execution sequence are determined
   /// by 'or-ing' its properties with those of the calling function or
   /// operator ... if a locked function calls an unlocked function, the
   /// unlocked function behaves as though it were locked." Entries with
   /// no user function (e.g. the top-level immediate-execution context)
   /// contribute nothing and are simply skipped.
   bool get_inherited_exec_property(int idx) const;

   /// return the current PC
   Prefix_PC get_PC() const
      { return current_stack.get_PC(); }

   /// get the current prefix parser
   const Prefix & get_prefix() const
      { return current_stack; }

   /// return the number of pending ⎕ECs (or other safe execution contexts)
   int get_safe_execution_depth() const
      { return safe_execution_depth; }

   /// return true if \b this )SI entry has entered safe execution mode.
   /// That is, \b this )SI entry has initiated  ⎕EC)
   bool is_safe_execution_start() const
      { if (!parent)   return safe_execution_depth > 0;
        return safe_execution_depth > parent->safe_execution_depth;
      }

   /// remember (for the )SI entry of the macro implementing A ⎕EB B) that
   /// B has failed, see Error::update_error_info()
   void set_EB_B_failure(const UCS_string & text)
      { EB_B_failure = text; }

   /// return the text set by set_EB_B_failure() (empty if B has not failed)
   const UCS_string & get_EB_B_failure() const
      { return EB_B_failure; }

   /// clear safe_execution mode
   void clear_safe_execution()
      {
        if (parent)  safe_execution_depth = parent->safe_execution_depth;
        else         safe_execution_depth = 0;
      }

   /// restore safe_execution_depth to a value saved earlier (e.g. via
   /// get_safe_execution_depth()) -- unlike clear_safe_execution(),
   /// which unconditionally resets to the *parent's* depth, this is
   /// safe to use after a temporary set_safe_execution_depth() on a
   /// frame that may itself already have been a safe-execution frame
   /// (e.g. a nested ⎕EC's B is itself the B of an enclosing ⎕EC):
   /// clear_safe_execution() there would incorrectly strip the
   /// enclosing ⎕EC's own protection instead of merely undoing the
   /// temporary bump (Bugs28 #27).
   void restore_safe_execution_depth(int saved_depth)
      { safe_execution_depth = saved_depth; }

   /// construct a new derived function and return a pointer to it.
   /// @param LO token for the left operand (or 0 if absent)
   /// @param F_or_M_or_D the function, monadic, or dyadic operator
   /// @param RO token for the right operand (or 0 if absent)
   /// @param X optional axis value (empty Value_P if absent)
   /// @param loc caller location for diagnostics
   DerivedFunction * get_fun_oper_slot(const Token * LO, cFunction_P F_or_M_or_D,
                                       const Token * RO, Value_P X,
                                       const char * loc)
      { return fun_oper_cache.get(LO, F_or_M_or_D, RO, X, loc); }

   /// get the current prefix parser
   Prefix & get_prefix()
      { return current_stack; }

   /// set safe_execution mode
   void set_safe_execution_depth()
      {
        if (parent)  safe_execution_depth = parent->safe_execution_depth + 1;
        else         safe_execution_depth = 1;
      }

   /// mark this )SI entry as an isolated one-statement execution (e.g.
   /// ⎕EA's fallback argument, run via Bif_F1_EXECUTE::execute_statement())
   /// whose own →N/→ has nowhere real to go: apl2lrm.txt p.349 Figure 38
   /// says such a branch/escape means "flow of execution returns to the
   /// invoking expression", i.e. this )SI entry simply completes with no
   /// explicit result, rather than Command.cc's normal (and normally
   /// correct) "→N without function" SYNTAX ERROR for an orphan branch.
   void set_void_on_orphan_branch()
      { void_on_orphan_branch = true; }

   /// see set_void_on_orphan_branch()
   bool get_void_on_orphan_branch() const
      { return void_on_orphan_branch; }

   /// return the error related info in this context
   static const Error & get_error(const StateIndicator * si)
       { return si ? si->error : top_level_error; }

   /// return the error related info in this context
   static Error & get_error(StateIndicator * si)
       { return si ? si->error : top_level_error; }

   /// Return the function name, or "*" for an immediate execution context
   UCS_string function_name() const;

   /// return left arg (and set \b function)
   Value_P get_L(UCS_string & function) const;

   /// return the current line number
   Function_Line get_line() const;

   /// return the name of the parse mode
   Unicode get_parse_mode_name() const;

   /// return the right arg (and set \b function)
   Value_P get_R(UCS_string & function) const;

   /// return axis arg (and set \b function)
   Value_P get_X(UCS_string & function) const;

   /// print spaces according to level
   /// @param out destination output stream
   ostream & indent(ostream & out) const;

   /// print a short debug info
   void info(ostream & out, const char * loc) const;

   /// list the stack entry (for commands ]SI, )SI, and )SIS)
   /// @param out destination output stream
   /// @param mode which fields to include in the listing
   void list(ostream & out, SI_mode mode) const;

   /// return the level at which sym is pushed for the nth. time
   SI_level nth_push(const Symbol * sym, int from_tos) const;

   /// list the stack entry (for command ]SI)
   /// @param out destination output stream
   void print(ostream & out) const;

   /// print all owners of \b value
   /// @param out destination output stream
   /// @param value APL value whose owners are to be listed
   int show_owners(ostream & out, const Value & value) const;

   /// clear the marked bit in the executable, in the fun_oper_cache, and
   /// in all parsers
   void unmark_all_values() const;

   /// return true iff
   ///  (1) this SI entry is executing \b funname, or
   ///  (2) has resolved \b funname on its prefix parser stack
   /// @param ufun user function to check for
   bool uses_function(const UserFunction * ufun) const;

   /// Escape from \b user function (exit from each invocation until
   /// immediate execution is reached)
   void escape();

   /// continue this StateIndicator (to line N after →N back into it)
   /// @param N function line to continue execution at
   /// @param loc caller location for diagnostics
   void goon(Function_Line N, const char * loc);

   /// evaluate a →B statement. Update PC if needed, maybe do nothing (→'')
   /// @param B right argument giving the target line number or empty vector
   Token jump(cValue_R B);

   /// do a jump to function line \b line
   /// @param line target function line number
   Token jump_to_line(Function_Line line);

   /// retry this StateIndicator (after →'')
   /// @param loc caller location for diagnostics
   void retry(const char * loc);

   /// execute token in body...
   Token run();

   /// change the left argument (if any) of a failed primitive
   void set_L(Value_P value);

   /// change right argument of a failed primitive
   void set_R(Value_P value);

   /// change the axis left argument (if any) of a failed primitive
   void set_X(Value_P value);

   /// Maybe print B (according to tag) and erase B
   /// @param result token holding the statement result value
   /// @param trace true if tracing is active for this function
   void statement_result(const Token & result, bool trace);

   /// a small storage for DerivedFunction objects.
   DerivedFunctionCache fun_oper_cache;

   /// error when )SI is empty
   static Error top_level_error;

protected:

   /// the user function that is being executed
   const Executable * executable;

   /** track ⎕EC calls. The first )SI entry that calls ⎕ES sets
       \b safe_execution_depth to 1 and every child )SI increments it.
       Therefore:

       1. safe_execution_depth != 0 tells if ⎕ES is in effect, and the
       2. the parent with safe_execution_depth == 1 is the one that has
          initiated ⎕ES (and to which the )SI stack shall be popped on error.
    */
   int safe_execution_depth;

   /// for the macro implementing A ⎕EB B: the failure of B (if any), for
   /// )MORE when A fails too. Kept here (rather than in the global )MORE
   /// info) so that A cannot lose it and that it ends with the macro.
   UCS_string EB_B_failure;

   /// see set_void_on_orphan_branch()
   bool void_on_orphan_branch = false;

   /// The nesting level (of sub-executions)
   const SI_level level;

   /// details of the last error in this context.
   Error error;

   /// the current-stack of this context.
   Prefix current_stack;

   /// the StateIndicator that has called this one
   StateIndicator * const parent;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __STATE_INDICATOR_HH_DEFINED__
