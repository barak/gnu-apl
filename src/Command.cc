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
    The Command:: core: static data members, the )command dispatch table
    (do_APL_command()), immediate-execution (do_APL_expression()), the
    statement-execution driver (finish_context()), the REPL loop
    (process_line()/process_lines()), and small helpers shared across the
    Cmd_*.cc command-group files (is_lib_ref(), parse_from_to(),
    split_arg(), check_params()).

    The individual )-command handlers (cmd_XXX()) live in the Cmd_*.cc
    files grouped by feature area: Cmd_WS.cc (workspace lifecycle),
    Cmd_LISTING.cc (introspection), Cmd_SI.cc (SI stack), Cmd_DIAG.cc
    (diagnostics), Cmd_SESSION.cc (session/UI settings), Cmd_HOST.cc
    (host/file interaction), Cmd_HELP.cc () HELP), Cmd_USERCMD.cc
    (]USERCMD) — as well as the pre-existing Cmd_IN.cc, Cmd_KEYB.cc, and
    Cmd_LIB.cc. All of them are still, deliberately, part of this one
    Command class, declared once in Command.hh.
*/

#include "config.h"

#if HAVE_SYS_RESOURCE_C
#include <sys/resource.h>
#endif // HAVE_SYS_RESOURCE_C

#include <stdlib.h>
#include <string.h>

#include "Avec.hh"
#include "Cmd_DIAG.hh"
#include "Cmd_HELP.hh"
#include "Cmd_HOST.hh"
#include "Cmd_LISTING.hh"
#include "Cmd_SESSION.hh"
#include "Cmd_SI.hh"
#include "Cmd_USERCMD.hh"
#include "Cmd_WS.hh"
#include "Command.hh"
#include "Common.hh"
#include "Executable.hh"
#include "IndexExpr.hh"
#include "LineInput.hh"
#include "Nabla.hh"
#include "Parser.hh"
#include "Prefix.hh"
#include "QuadFunction.hh"
#include "StateIndicator.hh"
#include "UserPreferences.hh"
#include "Value.hh"
#include "Workspace.hh"

#include "Workspace.icc"

bool Command::auto_MORE = false;
Multiline_status Command::multiline_status = MLS_APL_text;
int  Command::multiline_start = -1;   // read in IO_Files.cc

int Command::boxing_format = 0;
ShapeItem Command::APL_expression_count = 0;
bool Command::expression_failed = false;

UCS_string_vector Command::copy_once_table;

//════════════════════════════════════════════════════════════════════════════
void
Command::clear_copy_once_table()
{
   if (const size_t count = copy_once_table.size())
      {
        copy_once_table.clear();
        CERR << ")COPY_ONCE table cleared (" << count << " entries)" << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Command::cmd_OFF(ostream & out, const UCS_string & arg)
{
   // the exit status has 8 bits: anything else would silently become a
   // different exit status (Bugs37 #76)
   //
bool valid = arg.size() <= 3;
   loop(a, arg.size())   if (!Avec::is_digit(arg[a]))   valid = false;
   if (valid && arg.size() && arg.atoi() > 255)   valid = false;

   if (!valid)
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << ")OFF " << arg << ": the exit code must be an"
                        " integer from 0 to 255";
        if (auto_MORE)   CERR << Workspace::more_error() << endl;
        return;
      }

   cmd_OFF(arg.size() ? arg.atoi() : 0);
}
//────────────────────────────────────────────────────────────────────────────
void
Command::cmd_OFF(int exit_val)
{
   COUT << endl;
   if (UserPreferences::uprefs.silence < NO_BANNER)
      {
        timeval end;
        gettimeofday(&end, 0);
        end.tv_sec -= UserPreferences::uprefs.session_start.tv_sec;
        end.tv_usec -= UserPreferences::uprefs.session_start.tv_usec;
        if (end.tv_usec < 1000000)   { end.tv_usec += 1000000;   --end.tv_sec; }
        COUT << "Goodbye." << endl
             << "Session duration: " << (end.tv_sec + 0.000001*end.tv_usec)
             << " seconds " << endl;
      }

   cleanup(true);

   // restore the initial memory rlimit
   //
#ifndef RLIMIT_AS // BSD does not define RLIMIT_AS
# define RLIMIT_AS RLIMIT_DATA
#endif

#if ! MINGW_SRC
rlimit rl;
   getrlimit(RLIMIT_AS, &rl);
   rl.rlim_cur = Quad_WA::initial_rlimit;
   setrlimit(RLIMIT_AS, &rl);
#endif // ! MINGW_SRC

   exit(exit_val);
}
//────────────────────────────────────────────────────────────────────────────
bool
Command::do_APL_command(ostream & out, UCS_string & line)
{
   // reset the stream to its normal formatting state before processing
   // this command -- a stray manual "left" here (instead of "right") once
   // left the stream left-justified for the rest of the session, silently
   // corrupting the next unrelated setfill('0') << setw(N) elsewhere
   // (confirmed: this caused the workspace-load "SAVED ..." timestamp to
   // print e.g. "70" instead of "07"). reset_format() copies from a single
   // shared default_format_state instead, so there is nothing left to get
   // wrong here.
   //
   out << reset_format;

   if (line.contains(UNI_COMMENT))   // unlikely, but valid
      {
        loop(l, line.size())   // find the leading ⍝
            {
              if (line[l] == UNI_COMMENT)   // found ⍝
                 {
                   line.resize(l);
                   line.remove_trailing_whitespaces();
                   break;
                 }
            }
      }

const UCS_string orig_line(line);   // the original input line

   // split line into command and command arguments
   //
UCS_string cmd;   // the command without arguments
const size_t len = line.copy_black(cmd, 0);   // line w/o leading/trailing ws

UCS_string arg(line, len);
UCS_string_vector args = split_arg(arg);
   line.clear();

   // clear the )MORE info, unless the command itself is )MORE
   //
   if (!is_command(cmd, ")MORE"))   // command is not )MORE.
      {
        // clear )MORE info unless cmd itself is )MORE
        //
        Workspace::more_error().clear();
      }

#define cmd_def(cmd_str, code, garg, _hint)                       \
   if (is_command(cmd, cmd_str))                                          \
      { if (check_params(out, cmd_str, args.size(), garg))   return true; \
        code; return false; }
#include "Command.def"

   // check for user defined commands...
   //
   loop(u, Workspace::get_user_commands().size())
       {
         const UTF8_string ucmd(Workspace::get_user_commands()[u].prefix);
         if (is_command(cmd, ucmd.c_str()))
            {
              Cmd_USERCMD::do_USERCMD(out, line, orig_line, cmd, args, u);
              return true;
            }
       }

     out << "INCORRECT COMMAND" << endl;
     return false;
}
//────────────────────────────────────────────────────────────────────────────
void
Command::do_APL_expression(const UCS_string & line, Value_P literal)
{
   ++APL_expression_count;
   expression_failed = false;

   // see the comment in do_APL_command() above
   COUT << reset_format;
   Workspace::more_error().clear();

Executable * statements = 0;
   try
      {
        statements = StatementList::fix(line, literal, LOC);
      }
   catch (const Error & err)
      {
        expression_failed = true;
        bool plus = Workspace::more_error().size();   // assume + needed
        const char * error_name = Error::error_name(err.get_error_code());
        if (strchr(error_name, UNI_PLUS))   plus = false;   // not needed
        if (Workspace::more_error().size() &&
               Workspace::more_error().back() == UNI_PLUS)
           plus = false; // dito.

        UERR << error_name;
        if (plus)   UERR << UNI_PLUS;
        UERR << endl;
        // line 2 (and thus line 3) already has its margin, like the
        // error lines of every other error (Bugs37 #62)
        //
        if (err.get_error_line_2().size())
           {
             COUT << err.get_error_line_2() << endl
                  << err.get_error_line_3() << endl;
           }

        // err.print() prints nothing further here in a non-verbose
        // build (its body is gated behind Log(LOG_verbose_error),
        // dead code in production) -- its only *reachable* effect was
        // leaking the internal "*** Error printed twice ***"
        // diagnostic to the user whenever err.print_loc happened to
        // already be set (Blake McBride, Bugs24 #2), which the lines
        // above already printed everything this call would have.
        delete statements;
        return;
      }
   catch (std::bad_alloc &)
      {
        CERR << "*** Command::process_line() caught other exception ***"
             << endl;
        delete statements;
        cmd_OFF(0);
      }
   catch (...)
      { FIXME; }

   if (statements == 0)   // StatementList::fix() failed
      {
        expression_failed = true;
        COUT << "main: Parse error";
        if (Workspace::more_error().size())   COUT << "+";
        else                                  COUT << ".";
        COUT << endl;
        return;
      }

   // At this point, the user command was parsed correctly.
   // check for Escape (→)
   //
   {
     const Token_string & body = statements->get_body();
     if (body.size() == 3                &&
         body[0].get_tag() == TOK_ESCAPE &&
         body[1].get_Class() == TC_END   &&
         body[2].get_tag() == TOK_RETURN_STATS)
        {
          // remove all SI entries up to (including) the next immediate
          // execution context
          //
          for (bool goon = true; goon;)
              {
                StateIndicator * si = Workspace::SI_top();
                if (si == 0)   break;   // SI empty

                goon = si->get_parse_mode() != PM_STATEMENT_LIST;
                si->escape();   // pop local vars of user defined functions
                Workspace::pop_SI(LOC);
              }

          delete statements;
          return;
        }
   }

// statements->print(CERR);

   // push a new context for the statements.
   //
   Workspace::push_SI(statements, LOC);
   finish_context();
}
//────────────────────────────────────────────────────────────────────────────
void
Command::finish_context()
{
   for (;;)
       {
         //
         // NOTE that the entire SI may change while executing this loop.
         // We should therefore avoid references to SI entries.
         //
         Token token = Workspace::SI_top()->get_executable()->execute_body();

// Q1(token)

         // start over if execution has pushed a new SI entry
         //
         if (token.get_tag() == TOK_SI_PUSHED)   continue;

check_EOC:
         if (Quad_FIO::benchmark_cycles_from)   // a benchmark is ongoing
            {
              if (StateIndicator * si = Workspace::SI_top()->get_parent())
                 {
                   if (si->is_safe_execution_start())
                      {
                        // at this point token is the result of a defined
                        // function which is being benchmarked. Replace the
                        // result by the number of CPU cycles executed since
                        // the start of the benchmark.
                        //
                        const uint64_t to = cycle_counter();
                        const uint64_t from = Quad_FIO::benchmark_cycles_from;
                        const uint64_t diff = to - from;
                        Value_P val = IntScalar(diff, LOC);
                        token.~Token();   // free the value (if any)
                        new (&token) Token(TOK_APL_VALUE1, val);
                        si->clear_safe_execution();
                      }
                 }
            }
         else if (Workspace::SI_top()->is_safe_execution_start())
            {
              if (Quad_FIO::benchmark_cycles_from)
                 {
                   // ⎕FIO[-1] has started a cycle measurement. Read the CPU
                   // cycle counter, compute the cycle difference, stop the
                   // measurement, and return the the cycle difference as int
                   // scalar...
                   //
                   const uint64_t to = cycle_counter();
                   const uint64_t diff = to - Quad_FIO::benchmark_cycles_from;
                   token.copy(Token(TOK_APL_VALUE1, IntScalar(diff, LOC)), LOC);
                   Quad_FIO::benchmark_cycles_from = 0;
                 }
              else
                 {
                   Quad_EC::eoc(token);
                 }
            }

         // the far most frequent cases are TC_VALUE and TOK_VOID
         // so we handle them first.
         //
         if (token.get_Class() == TC_VALUE || token.get_tag() == TOK_VOID )
            {
              if (Workspace::SI_top()->get_parse_mode() == PM_STATEMENT_LIST)
                 {
                   if (InterruptContext::attention_is_raised())
                      {
                        InterruptContext::clear_attention_raised(LOC);
                        InterruptContext::clear_interrupt_raised(LOC);
                        ATTENTION;
                      }

                   break;   // will return to calling context
                 }

              Workspace::pop_SI(LOC);

              // we are back in the calling SI. There should be a TOK_SI_PUSHED
              // token at the top of stack. Replace it with the result from
              //  the called (just poped) SI.
              //
              {
                Prefix & prefix = Workspace::SI_top()->get_prefix();
                Assert(prefix.at0().get_tag() == TOK_SI_PUSHED);

                prefix.replace_SI_PUSHED(token);
              }
              if (InterruptContext::attention_is_raised())
                 {
                   InterruptContext::clear_attention_raised(LOC);
                   InterruptContext::clear_interrupt_raised(LOC);
                   ATTENTION;
                 }

              continue;
            }

         if (token.get_tag() == TOK_BRANCH_INT ||
             token.get_tag() == TOK_BRANCH_LAB)
            {
              const Function_Line line = Function_Line(token.get_int_val());
              if (line == Function_Retry                                     &&
                  Workspace::SI_top()->get_parse_mode() == PM_STATEMENT_LIST &&
                  Workspace::SI_top()->get_parent())
                 {
                   Workspace::pop_SI(LOC);
                   Workspace::SI_top()->retry(LOC);
                   continue;
                 }

              StateIndicator * si = Workspace::SI_top_fun();

              if (si == 0)
                 {
                   // an orphan branch (nothing suspended to resume into)
                   // is normally invalid (lrm p.357: → in immediate
                   // execution resumes a suspended statement, nothing
                   // else) -- but an isolated one-statement execution
                   // explicitly marked as such (Prefix::execute_EA_
                   // fallback(), lrm p.349 Figure 38: such a branch
                   // means "flow of execution returns to the invoking
                   // expression") should simply complete with no
                   // explicit result instead of erroring.
                   //
                   if (Workspace::SI_top()->get_void_on_orphan_branch())
                      {
                        token.copy(Token(TOK_VOID), LOC);
                        goto check_EOC;
                      }

                    MORE_ERROR() <<
                    "branch back into function (→N) without suspended function";
                    SYNTAX_ERROR;   // →N without function,
                 }

              // a branch returned by ⍎ (e.g. 2+⍎'→3') is only executed if ⍎
              // is the leftmost primitive of its statement (LRM p.121),
              // i.e. if nothing of the calling statement is left
              // (Bugs35 #52)
              //
              if (Workspace::SI_top()->get_parse_mode() == PM_EXECUTE)
                 {
                   StateIndicator * caller = Workspace::SI_top()->get_parent();
                   if (caller && caller == si)
                      {
                        const Prefix & cp = caller->get_prefix();
                        const Token_string & body =
                                             caller->get_executable()->get_body();
                        // the prefix has already read the token left of
                        // ⍎ (the lookahead): the END of the statement if
                        // ⍎ is leftmost. An assignment Z←⍎'→N' is not a
                        // primitive and is OK (the old behaviour).
                        //
                        // Only for ⍎ (body[pc - 2]), not for ⎕EA, ⎕EC, ...
                        //
                        const Prefix_PC pc = cp.get_PC();
                        const TokenClass left = pc > Prefix_PC_0
                                              ? body[pc - 1].get_Class()
                                              : TC_END;
                        const bool is_execute = pc > Prefix_PC_0 + 1 &&
                              body[pc - 2].get_tag() == TOK_F1_EXECUTE;
                        const bool leftmost = cp.ssize() == 1 &&
                                              (left == TC_END ||
                                               left == TC_ASSIGN);
                        if (is_execute && !leftmost)
                           {
                             Workspace::pop_SI(LOC);   // the ⍎ context
                             MORE_ERROR() << "⍎ of a branch is not the"
                                             " leftmost primitive";
                             SYNTAX_ERROR;
                           }
                      }
                 }

              // pop contexts above defined function
              //
              while (si != Workspace::SI_top())   Workspace::pop_SI(LOC);

              si->goon(line, LOC);
              continue;
            }

         if (token.get_tag() == TOK_ESCAPE)
            {
              // remove all SI entries up to (including) the next immediate
              // execution context. If the → came from ⍎ (⍎'→') then the
              // statement containing that ⍎ does not count: ⍎'→' in
              // immediate execution is → (LRM p.121, Bugs35 #52).
              //
              const StateIndicator * execute_caller = 0;
              if (Workspace::SI_top() &&
                  Workspace::SI_top()->get_parse_mode() == PM_EXECUTE)
                 execute_caller = Workspace::SI_top()->get_parent();

              for (bool goon = true; goon;)
                  {
                    StateIndicator * si = Workspace::SI_top();
                    if (si == 0)   break;   // SI empty

                    goon = si->get_parse_mode() != PM_STATEMENT_LIST ||
                           si == execute_caller;
                    si->escape();   // pop local vars of user defined functions
                    Workspace::pop_SI(LOC);
                  }
              return;
            }

         if (token.get_tag() == TOK_ERROR)
            {
              if (token.get_int_val() == E_COMMAND_PUSHED)
                 {
                   Workspace::pop_SI(LOC);
                   UCS_string pushed_command = Workspace::get_pushed_Command();
                   process_line(pushed_command, 0);
                   pushed_command.clear();
                   Workspace::push_Command(pushed_command);   // clear in
                   return;
                 }

              expression_failed = true;

              // clear attention and interrupt flags
              //
              InterruptContext::clear_attention_raised(LOC);
              InterruptContext::clear_interrupt_raised(LOC);

              // check for safe execution mode. Unroll all SI entries that
              // have the same safe_execution_depth, except the last
              // unroll the SI stack.
              //
              if (Workspace::SI_top()->get_safe_execution_depth())
                 {
                   // SI_top() is in save execution mode. Pop it and all
                   // callers with the same get_safe_execution_depth().
                   //
                   // after pop'ing the SI entries only the original SI
                   // (which has set the save execution mode) shall remain
                   // the SI stack.
                   //
                   // Capture the innermost (about-to-be-popped) frame's
                   // error and restore it onto the surviving frame once
                   // unwound, the same way the nonsuspendable-unwind case
                   // below already does -- without this, Quad_EC::eoc()
                   // (called via check_EOC right after) read whatever
                   // stale error happened to already be sitting on the
                   // surviving frame instead of the one that was actually
                   // just thrown, showing the wrong ⎕EM[1;] text and/or
                   // losing ⎕EM[2;]/[3;] whenever evaluating ⎕EC's B
                   // pushed one or more intermediate SI frames (e.g. any
                   // real statement execution, or a called function).
                   // See Bugs27 #57.
                   //
                   StateIndicator * si = Workspace::SI_top();
                   const int sex_level = si->get_safe_execution_depth();
                   Error err = StateIndicator::get_error(si);
                   while (si->get_parent() && sex_level ==
                          si->get_parent()->get_safe_execution_depth())
                      {
                        si = si->get_parent();
                        Workspace::pop_SI(LOC);
                      }
                   StateIndicator::get_error(Workspace::SI_top()) = err;

                    goto check_EOC;
                  }

              // if suspend is not allowed then pop all SI entries that
              // don't allow suspend. Property 1 (nonsuspendable) is
              // inherited from every calling )SI entry too (apl2lrm.txt
              // p.360-361 "or-ing"), not just each entry's own -- so a
              // whole chain of callers under a nonsuspendable one must
              // unwind together, not just the innermost frame.
              //
              if (Workspace::SI_top()->get_inherited_exec_property(1))
                 {
                    Error err = StateIndicator::get_error(Workspace::SI_top());
                    while (Workspace::SI_top()->get_inherited_exec_property(1))
                       {
                         Workspace::pop_SI(LOC);
                       }

                   if (Workspace::SI_top())
                      {
                        StateIndicator::get_error(Workspace::SI_top()) = err;
                      }
                 }

              if (Workspace::get_error()->get_print_loc() == 0)   // not printed
                 {
                   Workspace::get_error()->print(CERR, LOC);
                 }
              else
                 {
                    // CERR << "ERROR printed twice" << endl;
                 }

              if (Workspace::SI_top()->get_level() == 0)
                 {
                   // IndexExpr::erase_stale() is gone: it used to delete
                   // every IndexExpr in the ring unconditionally, live or
                   // not, which is what caused Bugs8 #1 (Blake McBride).
                   // IndexExpr freeing stays where it already correctly
                   // happens (Prefix::clean_up(), the reduce_*() sites).
                   //
                   Value::erase_stale(LOC);

                   // Bugs28 #96 investigated, NOT fixed here -- see
                   // project_blake_bugs28.md: popping this level-0 frame
                   // right after reporting (which does stop )SI from
                   // growing without bound across repeated top-level
                   // errors) was tried and reverted -- ⎕ET/⎕EM (Quad_ET.cc/
                   // SystemVariable.cc's Quad_EM) read the failed
                   // statement's Error directly off this same
                   // StateIndicator object, walking up from Workspace::
                   // SI_top(); popping it away loses that Error entirely,
                   // so ⎕ET/⎕EM after ANY top-level error (not just the
                   // repeated-error growth case) silently went back to
                   // "0 0"/empty instead of the real error info -- a much
                   // bigger regression than the growth bug itself
                   // (confirmed live: 6 testcase files regressed,
                   // Quad_ET/Quad_ES/Lambda/Reshape/SaveLoad/Quad_FX.tc).
                   // A real fix needs the last-error info to survive
                   // independently of the full StateIndicator (FIFO,
                   // local variables, Prefix, ...) that )SI growth is
                   // actually about -- e.g. a small separate "last
                   // immediate-execution error" record ⎕ET/⎕EM can read
                   // after the heavyweight frame is gone -- which is a
                   // real design decision, not a one-line fix.
                   //
                 }
              return;
            }

         // we should not come here.
         //
         Q1(token)  Q1(token.get_Class())  Q1(token.get_tag())  FIXME;
       }

   // pop the context for the statements
   //
   Workspace::pop_SI(LOC);
}
//────────────────────────────────────────────────────────────────────────────
bool
Command::is_lib_ref(const UCS_string & lib)
{
   if (lib.size() == 1)   // single char: lib number
      {
        if (Avec::is_digit(lib.front()))   return true;
      }

   if (lib.front() == UNI_FULLSTOP)   return true;

   loop(l, lib.size())
      {
        const Unicode uni = lib[l];
        if (uni == UNI_SLASH)       return true;
        if (uni == UNI_BACKSLASH)   return true;
      }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
Command::parse_from_to(UCS_string & from, UCS_string & to,
                       const UCS_string & user_arg)
{
   // parse user_arg which is one of the following:
   //
   // 1.   (empty)
   // 2.   FROMTO
   // 3a.  FROM -
   // 3b.       - TO
   // 3c.  FROM - TO
   //
   from.clear();
   to.clear();

int s = 0;
bool got_minus = false;

   // skip spaces before from
   //
   while (s < user_arg.ssize() && user_arg[s] <= ' ') ++s;

   if (s == user_arg.ssize())   return false;   // case 1.: OK

   // copy left of '-' to from
   //
   while (s < user_arg.ssize()  &&
              user_arg[s] > ' ' &&
              user_arg[s] != '-')  from << user_arg[s++];

   // skip spaces after from
   //
   while (s < user_arg.ssize() && user_arg[s] <= ' ') ++s;

   if (s < user_arg.ssize() && user_arg[s] == '-')
      {
        ++s;
        got_minus = true;
      }

   // skip spaces before to
   //
   while (s < user_arg.ssize() && user_arg[s] <= ' ') ++s;

   // copy right of '-' to from
   //
   while (s < user_arg.ssize() && user_arg[s] > ' ')  to << user_arg[s++];

   // skip spaces after to
   //
   while (s < user_arg.ssize() && user_arg[s] <= ' ') ++s;

   if (s < user_arg.ssize())   return true;   // error: non-blank after to

   if (!got_minus)   to = from;   // case 2.

   if (!(from.size() || to.size()))   return true;   // error: single '-'

   return false;   // OK
}
//────────────────────────────────────────────────────────────────────────────
void
Command::process_line(UCS_string & line, ostream * out)
{
   line.remove_leading_whitespaces();
   if (line.size() == 0)           return;   // empty input line

   /* at this point, line is not empty and starts with a non-blank.
      The first character line[0] determines the nature of the line:

      ')':       regular APL command
      ']':       debug command (a GNU APL extension of IBM APL2)
      '∇':       invocaton of the Nabla function editor
      '⍝':       full line APL comment
      '#':       full line APL comment (a GNU APL extension of IBM APL2)
      otherwise: APL expression in immediate execution mode
    */

   switch(line[0])
      {
         case UNI_R_PARENT:      // regular command, e.g. )SI
              if (out == 0)   out = &COUT;
              do_APL_command(*out, line);
              if (line.size())   break;
              return;

         case UNI_R_BRACK:       // debug command, e.g. ]LOG
              if (out == 0)   out = &CERR;
              do_APL_command(*out, line);
              if (line.size())   break;
              return;

         case UNI_NABLA:         // Nabla editor, e.g. ∇FUN
              Nabla::edit_function(line);
              return;

         case UNI_NUMBER_SIGN:         // e.g. # comment
         case UNI_COMMENT:             // e.g. ⍝ comment
              return;

        default: break;
      }

   do_APL_expression(line, Value_P());
}
//────────────────────────────────────────────────────────────────────────────
void
Command::process_lines()
{
UCS_string line;
Multi_line_SM sm;
ShapeItem multi_pos;
UCS_string_vector content;   // for new-style multiline strings

   {
     bool eof = false;
     InputMux::get_line(LIM_ImmediateExecution, Workspace::get_prompt(),
                              line, eof, LineInput::get_history());
     // InputMux::get_line() has removed the trailing \n.

     if (eof)   CERR << "EOF at " << LOC << endl;

     multi_pos = line.multi_pos();
     if (multi_pos == -1)   // OUTSIDE
        {
          process_line(line, 0);
          return;
        }
     content.push_back(UCS_string(line, 0, multi_pos));
     sm.next(line[multi_pos]);
   }

   multiline_status = MLS_Start_of_multi;

const bool multi_literal = sm.in_literal();

const UCS_string prompt = UCS_string(UNI_RIGHT_ARROW) + Workspace::get_prompt();

   for (bool subsequent = false; ; subsequent = true)
       {
         if (subsequent)   // otherwise we use the line received above
            {
              line.clear();

              bool eof = false;
              InputMux::get_line(LIM_ImmediateExecution, prompt,
                                 line, eof, LineInput::get_history());
                // InputMux::get_line() has removed the trailing \n.

              if (eof) CERR << "EOF at " << LOC << endl;

              multi_pos = line.multi_pos();
              if (multi_pos != -1)   // some triple
                 {
                    sm.next(line[multi_pos]);
                 }
              content.push_back(multi_literal ? line : line.do_escape(true));
            }

         if (!sm.inside_multi())   break;
       }

   multiline_status = MLS_APL_text;

const UCS_string suffix(line, multi_pos + 3);

    multiline_start = 0;   // inform IO_Files.cc

   if (multi_literal)   // literal (not string)
      {
        // top-level multiline literals are recursive and therefore more
        // complicated tnan multiline strings
        //
        content[0] << "<<<";

        Lit_DB literals;

        if (Parser::replace_multi_line_strings(content, literals, false))
           SYNTAX_ERROR;
        if (Parser::replace_multi_line_literals(content, literals, false))
           SYNTAX_ERROR;

        // remove trailing empty lines (which would mess up the shape
        // computation)
        while (content.size() && !content.back().size())   content.pop_back();

        if (literals.size() != 1)
           {
             MORE_ERROR() << "Multiline literal has more than one ("
                          << literals.size() << "items";
             SYNTAX_ERROR;
           }

        Value_P literal = literals.pull_last();
        do_APL_expression(content[0], literal);
      }
   else                 // string (not literal)
      {
        // top-level multiline strings are flat and therefore much simpler
        // tnan multiline literals, In particular there is no need for
        // Parser::replace_multi_line_strings().
        //
        if (content.size() == 2)        // special case:  ««« »»»
           {
             const UTF8_string empty(" (0⍴⊂\"\")");   // 0⍴⊂""
             content[0] << UCS_string(empty);
           }
        else if (content.size() == 3)   // special case: ««« string »»»
          {
            const UTF8_string encl("(,⊂\"");  // enclose content...
            content[0] << UCS_string(encl) << content[1] << "\")";
          }
        else                            // general case: ««« string ... »»»
          {
            content[0] << "(";
            for (size_t a = 1; a < content.size() - 1; ++a)
                {
                  if (a > 1)   content[0] << " ";
                  content[0] << "\"" << content[a] << "\"";
                }
            content[0] << ")";
          }

        content[0] << " " << suffix;
        process_line(content[0], 0);
      }
}
//────────────────────────────────────────────────────────────────────────────
bool
Command::check_params(ostream & out, const char * command, int argc,
                      const char * args)
{
   // allow everything for ]USERCMD
   //
   if (!strcmp(command, "]USERCMD"))   return false;

   // analyze args to figure the number of parametes expected.
   //
int mandatory_args = 0;
int opt_args = 0;
int brackets = 0;
bool in_param = false;
bool many = false;

UTF8_string args_utf(args);
UCS_string args_ucs(args_utf);
   loop (a, args_ucs.size())   switch(args_ucs[a])
       {
         case '[': ++brackets;   in_param = false;   continue;
         case ']': --brackets;   in_param = false;   continue;
         case '|':               in_param = false;
              if (brackets)   --opt_args;
              else            --mandatory_args;
              continue;
         case '.':
              if (a < (args_ucs.ssize() - 2) &&
                  args_ucs[a + 1] == '.'    &&
                  args_ucs[a + 2] == '.')   many = true;
              continue;
         case '0' ... '9':
         case '_':
         case 'A' ... 'Z':
         case 'a' ... 'z':
         case UNI_OVERBAR:
         case '-':
              if (!in_param)   // start of a name or range
                 {
                   if (brackets)   ++opt_args;
                   else            ++mandatory_args;
                   in_param = true;
                 }
              continue;

         case ' ': in_param = false;
              continue;

         default: Q1(args_ucs[a])   Q1(int(args_ucs[a]))
       }

   if (argc < mandatory_args)   // too few parameters
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "missing parameter(s) in command " << command
                     << ". Usage:\n"
                     << "      " << command << " " << args;
        if (auto_MORE)   CERR << Workspace::more_error() << endl;
        return true;
      }

   if (many)   return false;

   if (argc > (mandatory_args + opt_args))   // too many parameters
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "too many (" << argc << ") parameter(s) in command "
                     << command << ". Usage:\n"
                     << "      " << command << " " << args;
        if (auto_MORE)   CERR << Workspace::more_error() << endl;
        return true;
      }

   return false;   // OK
}
//────────────────────────────────────────────────────────────────────────────
UCS_string_vector
Command::split_arg(const UCS_string & arg)
{
   // split arg into tokens separated by whitespace

UCS_string_vector result;
   for (size_t idx = 0; ; )
      {
        UCS_string token_ucs;
        idx = arg.copy_black(token_ucs, idx);
        if (token_ucs.size() == 0)   return result;

        result.push_back(token_ucs);
      }
}
//════════════════════════════════════════════════════════════════════════════
