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

#include <iomanip>

#include "Bif_F12_TAKE_DROP.hh"
#include "Command.hh"
#include "Executable.hh"
#include "IndexExpr.hh"
#include "Output.hh"
#include "Prefix.hh"
#include "StateIndicator.hh"
#include "SystemLimits.hh"
#include "UserFunction.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
StateIndicator::StateIndicator(const Executable * exec, StateIndicator * _par)
   : executable(exec),
     safe_execution_depth(_par ? _par->safe_execution_depth : 0),
     level(_par ? 1 + _par->get_level() : 0),
     error(E_NO_ERROR, LOC),
     current_stack(*this, exec->get_body()),
     parent(_par)
{
}
//────────────────────────────────────────────────────────────────────────────
StateIndicator::~StateIndicator()
{
   // flush the FIFO. Do that before delete executable so that values
   // copied directly from the body of the executable are not killed.
   //
   current_stack.clean_up();
   fun_oper_cache.reset();

   // if executable is a user defined function then pop its local vars.
   // otherwise delete the body token
   //
   if (get_parse_mode() == PM_FUNCTION)
      {
        const UserFunction * ufun = get_executable()->get_exec_ufun();
        if (ufun)   ufun->pop_local_vars();
      }
   else
      {
         Assert1(executable);
         delete executable;
         executable = 0;
      }
}
//────────────────────────────────────────────────────────────────────────────
bool
StateIndicator::get_inherited_exec_property(int idx) const
{
   for (const StateIndicator * si = this; si; si = si->parent)
       {
         const UserFunction * ufun = si->executable->get_exec_ufun();
         if (ufun && ufun->get_exec_properties()[idx])   return true;
       }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
StateIndicator::function_name() const
{
   Assert(executable);

   switch(get_parse_mode())
      {
        case PM_FUNCTION:
             return executable->get_name();

        case PM_STATEMENT_LIST:
             {
               UCS_string ret;
               ret << UNI_DIAMOND;
               return ret;
             }

        case PM_EXECUTE:
             {
               UCS_string ret;
               ret << UNI_EXECUTE;
               return ret;
             }
      }

   FIXME;
   return UCS_string();
} 
//────────────────────────────────────────────────────────────────────────────
Value_P
StateIndicator::get_L(UCS_string & function) const
{
   if (const Value_P * L = current_stack.locate_L(function))   return *L;
   return Value_P();
}
//────────────────────────────────────────────────────────────────────────────
Function_Line
StateIndicator::get_line() const
{
Prefix_PC pc = get_PC();
   if (pc > Prefix_PC_0)   --pc;
   return executable->get_line(pc);
}
//────────────────────────────────────────────────────────────────────────────
Unicode
StateIndicator::get_parse_mode_name() const
{
   switch(get_parse_mode())
      {
        case PM_FUNCTION:       return UNI_NABLA;
        case PM_STATEMENT_LIST: return UNI_DIAMOND;
        case PM_EXECUTE:        return UNI_EXECUTE;
     }

   CERR << "pmode = " << get_parse_mode() << endl;
   FIXME;
   return Invalid_Unicode;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
StateIndicator::get_R(UCS_string & function) const
{
   if (const Value_P * R = current_stack.locate_R(function))   return *R;
   return Value_P();
}
//────────────────────────────────────────────────────────────────────────────
Value_P
StateIndicator::get_X(UCS_string & function) const
{
   if (const Value_P * X = current_stack.locate_X(function))   return *X;
   return Value_P();
}
//────────────────────────────────────────────────────────────────────────────
ostream &
StateIndicator::indent(ostream & out) const
{
   if (level < 0)
      {
         CERR << "[negative level " << HEX(level) << "]" << endl;
      }
   else if (level > 100)
      {
         CERR << "[huge level " << HEX(level) << "]" << endl;
      }
   else
      {
         loop(d, level)   out << "   ";
      }

   return out;
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::info(ostream & out, const char * loc) const
{
   out << "SI[" << level << ":" << get_PC() << "] "
       << get_parse_mode_name() << " "
       << executable->get_text(0) << " creator: " << executable->get_loc()
       << "   seen at: " << loc << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::list(ostream & out, SI_mode mode) const
{
   if (mode & SIM_debug)   // command ]SI or ]SIS
      {
        print(out);
        return;
      }

   // pmode column
   //
   switch(get_parse_mode())
      {
        case PM_FUNCTION:
             Assert(executable);

             // hide macro-internal frames (e.g. Z__LO_EACH_B) from )SI/
             // )SIS/)SINL -- same principle as SymbolTable::list()'s
             // "hide macros" for )VARS/)FNS/)SYMBOLS: a macro is a real
             // UserFunction, but its name and line numbers are GNU APL
             // implementation detail the user never wrote, not
             // something meaningful to debug against. ]SI/]SIS (the
             // SIM_debug case above) still show everything. See Bugs27
             // #56.
             //
             if (executable->get_exec_ufun()->is_macro())   return;

             if (mode == SIM_SI)   // )SI
                {
                  out << executable->get_exec_ufun()
                                   ->get_name_and_line(get_PC());
                  break;
                }

             if (mode & SIM_statements)   // )SIS
                {
                  if (error.get_error_code())
                     {
                       out << error.get_error_line_2() << endl
                           << error.get_error_line_3();
                     }
                  else
                     {
                       const UCS_string name_and_line =
                            executable->get_exec_ufun()
                                      ->get_name_and_line(get_PC());
                       out << name_and_line
                           << "  " << executable->statement_text(get_PC())
                           << endl
                           << UCS_string(name_and_line.size(), UNI_SPACE)
                           << "  ^";   // ^^^
                     }
                }

             if (mode & SIM_name_list)   // )SINL
                {
                  const UCS_string name_and_line =
                        executable->get_exec_ufun()
                                  ->get_name_and_line(get_PC());
                       out << name_and_line << " ";
                       executable->get_exec_ufun()->print_local_vars(out);
                }
             break;

        case PM_STATEMENT_LIST:
             out << "*";
             if (mode & SIM_statements)   // )SIS
                {
                  if (!executable)      break;

                  // )SIS and we have a statement
                  //
                  out << "  "
                      << executable->statement_text(get_PC())
                      << endl << "   ^";   // ^^^
                }
             break;

        case PM_EXECUTE:
             out << "**  ";
             if (mode & SIM_statements)   // )SIS
                {
                  if (!executable)   break;

                  // )SIS and we have a statement
                  //
                  if (error.get_error_code())
                     // Bugs28 #100(ab): the "**  " prefix above applies
                     // only to line_2 (printed right after it, before
                     // the first endl) -- line_3 (the caret line) used
                     // to follow with only its own embedded six-space
                     // prompt margin and no such prefix, landing four
                     // columns short of line_2's carets.
                     out << error.get_error_line_2() << endl
                         << "**  " << error.get_error_line_3();
                  else
                     out << "  "
                         << executable->statement_text(get_PC());
                }
             break;
      }

   out << endl;
}
//────────────────────────────────────────────────────────────────────────────
int
StateIndicator::nth_push(const Symbol * sym, int from_tos) const
{
   if (from_tos == 0)   return 0;

  // collect SI entries in reverse order...
   //
std::vector<const StateIndicator *> stack;

   for (const StateIndicator * si = Workspace::SI_top();
        si; si = si->get_parent())
      {
        stack.push_back(si);
      }

   loop(d, stack.size())
       {
         const StateIndicator * si = stack[stack.size() - d - 1];
         const Executable * exec = si->get_executable();
         Assert(exec);
         if (!exec->pushes_sym(sym))   continue;
         if (0 == --from_tos)   return si->get_level();
       }

   MORE_ERROR() << "Invalid ⎕STACK index";
   INDEX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::print(ostream & out) const
{
   out << "Depth:      " << level                << endl;
   out << "Exec:       " << executable           << endl;
   out << "Safe exec:  " << safe_execution_depth << endl;

   Assert(executable);

   switch(get_parse_mode())
      {
        case PM_FUNCTION:
             out << "Pmode:      ∇ "
                 << executable->get_exec_ufun()->get_name_and_line(get_PC());
             break;

        case PM_STATEMENT_LIST:
             out << "Pmode:      ◊ " << " " << executable->get_text(0);
             break;

        case PM_EXECUTE:
             out << "Pmode:      ⍎ " << " " << executable->get_text(0);
             break;

        default:
             out << "??? Bad pmode " << get_parse_mode();
      }
   out << endl;

   out << "PC:         " << get_PC() << " (" << executable->get_body().size()
                       << ")";
   out << " " << executable->get_body()[get_PC()] << endl;
   out << "Stat:       " << executable->statement_text(get_PC());
   out << endl;

   out << "err_code:   " << HEX(error.get_error_code()) << endl;
   if (error.get_error_code())
      out << "thrown at:  " << error.get_throw_loc() << endl
       << "e_msg_1:    '" << error.get_error_line_1() << "'" << endl
       << "e_msg_2:    '" << error.get_error_line_2() << "'" << endl
       << "e_msg_3:    '" << error.get_error_line_3() << "'" << endl;

   out << endl;
}
//────────────────────────────────────────────────────────────────────────────
int
StateIndicator::show_owners(ostream & out, const Value & value) const
{
int count = 0;

   Assert(executable);
char cc[100];
   SPRINTF(cc, "    SI[%d] ", level);
   count += executable->show_owners(cc, out, value);

   SPRINTF(cc, "    SI[%d] ", level);
   count += current_stack.show_owners(cc, out, value);

   return count;
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::unmark_all_values() const
{
   Assert(executable);
   executable->unmark_all_values();   // values in function body tokens

   current_stack.unmark_all_values();   // values in the parsers
   fun_oper_cache.unmark_all_values();   // values in the derived function cache
}
//────────────────────────────────────────────────────────────────────────────
bool
StateIndicator::uses_function(const UserFunction * ufun) const
{
const Executable * const uexec = ufun;   // cast ufun to uexec

   for (const StateIndicator * si = this; si; si = si->get_parent())
       {
         const Executable * exec = si->get_executable();
         Assert1(exec);

         // case 1: ufun is the currently executing function
         //
         if (exec == uexec)   return true;

         // case 2: ufun is used on the prefix parser stack
         //
         if (si->current_stack.uses_function(ufun))   return true;

         // case 3: ufun is in the body of si->get_executable(), AND that
         // frame is actually suspended/pendent (has a nonzero recorded
         // error) rather than merely a normal, live caller further up
         // the currently executing chain. Blocking on a live caller
         // that only *mentions* ufun's name (e.g. RUN calling ⎕FX to
         // (re)define GEN, then calling GEN, with RUN's own body
         // naming GEN) broke the common "generate-then-call" pattern --
         // APL2 protects only functions that are themselves pendent/
         // suspended (cases 1/2 above already cover "is ufun itself
         // actually executing/pending re-entry"; this case 3 is only
         // about a stale, no-longer-runnable compiled reference to
         // ufun sitting in a SUSPENDED frame, which really would crash
         // )SAVE since the symbol there is already resolved and points
         // to the old function). See Bugs27 #35 residual.
         //
         if (StateIndicator::get_error(si).get_error_code() == E_NO_ERROR)
            continue;

         const Token_string & body = exec->get_body();
         loop(b, body.size())
             {
               const Token & tok = body[b];
               if (tok.get_tag() != TOK_SYMBOL)   continue;

               if (ufun->get_name() == tok.get_sym_ptr()->get_name())
                  {
                    return true;
                  }
             }
       }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::escape()
{
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::goon(Function_Line new_line, const char * loc)
{
   // jump back into a function, i.e. →N from immediate execution

const UserFunction * ufun = get_executable()->get_exec_ufun();

   // A lambda is a single line (line 1); unlike a ∇-function, its PC for
   // any other line number (pc_for_line()'s out-of-range fallback,
   // body.ssize()-1) is the internal λ←/return token, not a real
   // statement start -- goto_PC() there leaves the prefix parser in an
   // invalid phrase and the next reduction asserts (Executable.cc:173).
   // Any →N other than →1 into a suspended lambda has nowhere valid to
   // resume, so just leave it, matching what →0 already does.
   //
   if (ufun->is_lambda() && new_line != Function_Line(1))
      {
        Workspace::pop_SI(loc);

        // Bugs28 #13: if this lambda was itself called from an
        // expression still awaiting its result (e.g. a plain call, or
        // ⍎'s own TOK_BRANCH_INT bubbling out of a nested ExecuteList
        // and landing here via Command.cc's SI_top_fun() lookup), the
        // caller's Prefix has a TOK_SI_PUSHED placeholder sitting where
        // that result belongs. Left alone, the next reduction there
        // finds no phrase for a bare TOK_SI_PUSHED and asserts/FIXMEs
        // (Prefix::reduce____()). Complete it the same way a normal
        // no-result return does (Command.cc's own TC_VALUE/TOK_VOID
        // handling) -- with TOK_VOID, since the lambda produced nothing.
        //
        if (StateIndicator * caller = Workspace::SI_top())
           {
             Prefix & prefix = caller->get_prefix();
             if (prefix.ssize() && prefix.at0().get_tag() == TOK_SI_PUSHED)
                {
                  prefix.replace_SI_PUSHED(Token(TOK_VOID));
                }
           }
        return;
      }

const Prefix_PC pc = ufun->pc_for_line(new_line);

   Log(LOG_StateIndicator__push_pop)
      CERR << "Continue SI[" << level << "] at line " << new_line
           << " pc=" << pc << " at " << loc << endl;

   if (get_executable()->get_body()[pc].get_tag() == TOK_STOP_LINE)   // S∆
      {
        // pc points to a S∆ token. We are jumping back from immediate
        // execution, that was entered with ⎕STOP or S∆. We then don't
        // want to stop (again) but continue after the stop token.
        //
        current_stack.goto_PC(pc + 2);
      }
   else  // the "normal" case
      {
        current_stack.goto_PC(pc);
      }

   Log(LOG_prefix_parser)   CERR << "GOTO [" << get_line() << "]" << endl;

   current_stack.reset(LOC);
}
//────────────────────────────────────────────────────────────────────────────
Token
StateIndicator::jump(cValue_R B)
{
   // perform a jump. We either remain in the current function (and then
   // return TOK_VOID), or we (want to) jump back into the calling
   // function (and then return TOK_BRANCH_INT.). The jump itself (if any)
   // is executed in Prefix.cc.
   //
   if (B.get_rank() > 1)   RANK_ERROR;

   if (B.element_count() == 0)     // →''
      {
        // →'' in immediate execution means resume (retry) suspended function
        // →'' on ⍎ or defined functions means do nothing
        //
        if (get_parse_mode() == PM_STATEMENT_LIST)
           return Token(TOK_BRANCH_INT, int64_t(Function_Retry));

        return Token(TOK_NOBRANCH);           // stay in context
      }

const Function_Line line = B.get_line_number();
   return jump_to_line(line);
}
//────────────────────────────────────────────────────────────────────────────
Token
StateIndicator::jump_to_line(Function_Line line)
{
   if (const UserFunction * ufun = get_executable()->get_exec_ufun())
      {
        // →N in ∇ context
        //
        current_stack.goto_PC(ufun->pc_for_line(line));   // →N to valid line in user function
        return Token(TOK_VOID);            // stay in context
      }

   // →N in ⍎ or ◊ context (i.e. to start of line 0)
   //
   return Token(TOK_BRANCH_INT, int64_t(line < 0 ? Function_Line_0 : line));
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::retry(const char * loc)
{
   Log(LOG_StateIndicator__push_pop || LOG_prefix_parser)
      CERR << endl << "RETRY " << loc << ")" << endl;
}
//────────────────────────────────────────────────────────────────────────────
Token
StateIndicator::run()
{
Token result = current_stack.reduce_body();

   Log(LOG_prefix_parser)
      CERR << "Prefix::reduce_body(si=" << level << ") returned "
           << result << " in StateIndicator::run()" << endl;
   return result;
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::set_L(Value_P new_value)
{
UCS_string function;
   if (Value_P * L = current_stack.locate_L(function))   *L = new_value;
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::set_R(Value_P new_value)
{
UCS_string function;
   if (Value_P * R = current_stack.locate_R(function))   *R = new_value;
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::set_X(Value_P new_value)
{
UCS_string function;
   if (Value_P * X = current_stack.locate_X(function))   *X = new_value;
}
//────────────────────────────────────────────────────────────────────────────
#ifdef apl_TARGET_LIBAPL
typedef int (*result_callback)(const cValue * result, int committed);
extern "C" result_callback res_callback;
result_callback res_callback = 0;
#endif

#ifdef apl_TARGET_PYTHON
extern bool python_result_callback(const Token & result);
#endif

void
StateIndicator::statement_result(const Token & result, bool trace)
{
   Log(LOG_IfElse)   usleep(200000);

   Log(LOG_StateIndicator__enter_leave)
      CERR << "StateIndicator::statement_result(pmode="
           << get_parse_mode_name() << ", result=" << result << endl;

   if (trace)
      {
        const UserFunction * ufun = executable->get_exec_ufun();
        // property 0 (locked) is inherited from every calling )SI entry
        // too (apl2lrm.txt p.360-361 "or-ing"): "may not be ... traced"
        // applies even if only a caller, not this function itself, is
        // locked.
        //
        if (ufun && !get_inherited_exec_property(0))
           {
             const Function_Line line =
                   executable->get_line(get_PC() - 1);
             result.show_trace(COUT, ufun->get_name(), line);
           }
      }

   fun_oper_cache.reset();

// if (get_parse_mode() == PM_EXECUTE)   return;

   // if result is a value then print it, unless it is a committed value
   // (i.e. TOK_APL_VALUE2)
   //
   if (result.get_ValueType() != TV_VAL)
      {
#ifdef apl_TARGET_PYTHON
         python_result_callback(result);
#endif

         return;
      }

   // a macro (the APL implementation of a primitive) never displays a
   // statement value of its own. In particular ⎕EB's cleanup expression A
   // (⍎μ2 in Macro.def's Z__A_Quad_EB_B) is executed for its side effects
   // only; its value (if any) is discarded. ⊣⍎μ2 would not do since A may
   // legitimately have no value (e.g. a cleanup function without result).
   //
   if (const UserFunction * ufun = executable->get_exec_ufun())
      {
        if (ufun->is_macro())   return;
      }

const TokenTag tag = result.get_tag();
Value_P B(result.get_apl_val());
   Assert(B);

   // print values, but not TOK_APL_VALUE2 (aka. commited value)
   //
bool print_value = result.get_Class() == TC_VALUE && tag != TOK_APL_VALUE2;

#ifdef apl_TARGET_LIBAPL
   if (res_callback)   // callback installed
      {
        // the callback decides whether the value shall be printed (even
        // if it was committed)
        //
        print_value = res_callback(B.get(), !print_value);
      }
#endif

#ifdef apl_TARGET_PYTHON
   print_value = python_result_callback(result);
#endif

   if (!print_value)   return;

   Quad_QUOTE::done(false, LOC);

   display_value(B);
}
//────────────────────────────────────────────────────────────────────────────
void
StateIndicator::display_value(Value_P B, bool boxing)
{
   // the default display of B, also used (without boxing) by ⎕←B (Bugs37
   // #35).
   //
   // the print length limit (⎕SYL) applies to every display of a value,
   // whether boxed or not: if B exceeds it then the longest dimension of B
   // is cut in half until B is below the limit.
   //
Value_P B0 = B;
const bool truncated = Quad_SYL::print_length_limit &&
                       B->element_count() >= Quad_SYL::print_length_limit;
   if (truncated)
      {
        Shape sh(B->get_shape());
        while (sh.get_volume() >= Quad_SYL::print_length_limit)
           {
             sRank longest = 0;
             loop(r, sh.get_rank())
                {
                  if (sh.get_shape_item(r) > sh.get_shape_item(longest))
                     longest = r;
                }

             sh.set_shape_item(longest, sh.get_shape_item(longest) / 2);
           }

        B0 = Bif_F12_TAKE::do_take(sh, *B, false);
      }

const int boxing_format = boxing ? Command::get_boxing_format() : 0;
   if (boxing_format == 0)   // no boxing
      {
        B0->print(COUT);
      }
   else if (boxing_format < 0)   // box B0 only if the boxed B0 fits into ⎕PW
      {
        const PrintContext pctx = Workspace::get_PrintContext(PST_NONE);
        Value_P B1 = Quad_CR::do_CR(-boxing_format, *B0, pctx);
        if (B1->get_cols() >= Workspace::get_PW())   // too large
           B0->print(COUT);   // don't box
        else
           B1->print(COUT);   // do box
      }
   else
      {
        const PrintContext pctx = Workspace::get_PrintContext(PST_NONE);
        Value_P B1 = Quad_CR::do_CR(boxing_format, *B0, pctx);
        B1->print(COUT);
      }

   if (truncated)
      {
        CERR << "      *** display of value was truncated (limit "
                "⎕SYL[⎕IO + " << Quad_SYL::SYL_PRINT_LIMIT
             << "] reached)  ***" << endl;
      }
}
//════════════════════════════════════════════════════════════════════════════

