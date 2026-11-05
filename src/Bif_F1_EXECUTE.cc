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

#include <sstream>

#include "ArgCheck.hh"
#include "Bif_F1_EXECUTE.hh"
#include "Command.hh"
#include "Executable.hh"
#include "StateIndicator.hh"
#include "UTF8_string.hh"
#include "Workspace.hh"

// primitive function instance
//
Bif_F1_EXECUTE Bif_F1_EXECUTE::fun;    // ⍎

int Bif_F1_EXECUTE::copy_pending = 0;

//════════════════════════════════════════════════════════════════════════════
Token
Bif_F1_EXECUTE::eval_B(cValue_R B) const
{
   ArgCheck::require_scalar_or_vector("⍎B", "B", B);

UCS_string statement(B);

   if (statement.size() == 0)   return Token(TOK_NO_VALUE);

   return execute_statement(statement);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F1_EXECUTE::eval_fill_B(cValue_R B) const
{
   return Token(TOK_VOID);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F1_EXECUTE::execute_command(UCS_string & command)
{
   if (copy_pending &&
       (
//      command.starts_iwith(")COPY")    ||
        command.starts_iwith(")ERASE")   ||
        command.starts_iwith(")FNS")     ||
        command.starts_iwith(")NMS")     ||
        command.starts_iwith(")QLOAD")   ||
        command.starts_iwith(")SYMBOLS") ||
        command.starts_iwith(")VARS")))
      {
        throw_apl_error(E_COPY_PENDING, LOC);
      }

   if (command.starts_iwith(")LOAD")  ||
       command.starts_iwith(")QLOAD") ||
       command.starts_iwith(")CLEAR") ||
       command.starts_iwith(")RESET") ||
       command.starts_iwith(")SIC")   ||
       command.starts_iwith(")OFF")   ||  // Bugs30 #24: )OFF terminates
                                           // the whole session -- at
                                           // least as disruptive as
                                           // )CLEAR/)RESET, and ⎕EC/⎕EA
                                           // exist precisely so their
                                           // caller can catch that kind
                                           // of thing rather than have
                                           // it happen out from under
                                           // them.
       command.starts_iwith("]NEXTFILE") ||  // ends the current input
                                              // file (and, in a testcase,
                                              // checks that the SI is
                                              // clear), so it must run
                                              // after ⍎ has returned
       command.starts_iwith(")CONTINUE") ||  // Bugs37 #22: )SAVE + )OFF
       command.starts_iwith(")SAVE"))  // Bugs30 #25: )SAVE from within
                                        // ⎕EC/⎕EA persists the transient
                                        // ⎕EC/⎕EA SI B-frame itself into
                                        // the saved workspace, so a later
                                        // )LOAD resurrects a phantom SI
                                        // frame that )CHECK then reports
                                        // as "SI not cleared".
      {
        // the command modifies the SI stack. We throw E_COMMAND_PUSHED
        // but without displaying it. That should bring us back to
        // Command::do_APL_expression() with token.get_tag() == TOK_ERROR
        //
        Workspace::push_Command(command);
        throw_apl_error(E_COMMAND_PUSHED, LOC);
      }

ostringstream out;   // the APL output (like stdout) of the command

   // check for user-defined commands (they are defined APL functions)
   //
const bool user_cmd = Command::do_APL_command(out, command);   // writes to out
   if (user_cmd)   return execute_statement(command);

   // system command. Append linefeed if needed.
   // To accommodate line_starts below.
   //
const string & out_str = out.str();
UTF8_string result_utf8(out_str.data(), out_str.size());
   if (result_utf8.size() == 0 ||
       result_utf8.back() != UNI_LF)
      result_utf8 += '\n';

   // result_utf8 may have multiple lines. Remember where the lines start.
   //
std::vector<ShapeItem> line_starts;
   line_starts.push_back(0);   // the first line
   loop(r, result_utf8.size())
      {
        if (result_utf8[r] == UNI_LF)   line_starts.push_back(r + 1);
      }

Value_P Z(ShapeItem(line_starts.size() - 1), LOC);
   loop(l, line_starts.size() - 1)
      {
        // loop(l, line_starts.size() - 1) never reaches the last index of
        // line_starts, so line_starts[l + 1] is always valid here.
        const ShapeItem len = line_starts[l + 1] - line_starts[l] - 1;

        const UTF8_string line_utf8(&result_utf8[line_starts[l]], len);
        const UCS_string line_ucs(line_utf8);
        Value_P ZZ(line_ucs, LOC);
        Z->next_ravel_Pointer(ZZ.get());
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F1_EXECUTE::execute_statement(UCS_string & statement)
{
   statement.remove_leading_and_trailing_whitespaces();

   // check for commands
   //
   if (statement.size() &&
       (statement[0] == UNI_R_PARENT ||
        statement[0] == UNI_R_BRACK))   return execute_command(statement);

ExecuteList * fun = ExecuteList::fix(statement.no_pad(), LOC);
   if (fun == 0)   SYNTAX_ERROR;

   Log(LOG_UserFunction__execute)   fun->print(CERR);

   // important special case: ⍎ of an APL literal value
   //
   if (fun->get_body().size() == 2 &&
       fun->get_body()[0].get_Class() == TC_VALUE)
      {
        Value_P Z = fun->get_body()[0].get_apl_val();
        delete fun;
        return Token(TOK_APL_VALUE1, Z);
      }

   Workspace::push_SI(fun, LOC);

   Log(LOG_StateIndicator__push_pop)
      {
        Workspace::SI_top()->info(CERR, LOC);
      }

   return Token(TOK_SI_PUSHED);
}
//════════════════════════════════════════════════════════════════════════════
