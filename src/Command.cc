/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2016  Dr. Jürgen Sauermann

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

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/resource.h>

#include "CharCell.hh"
#include "ComplexCell.hh"
#include "Command.hh"
#include "Doxy.hh"
#include "Executable.hh"
#include "FloatCell.hh"
#include "IndexExpr.hh"
#include "IntCell.hh"
#include "IO_Files.hh"
#include "LineInput.hh"
#include "Nabla.hh"
#include "NativeFunction.hh"
#include "Output.hh"
#include "Parser.hh"
#include "Prefix.hh"
#include "Quad_FX.hh"
#include "Quad_TF.hh"
#include "StateIndicator.hh"
#include "Svar_DB.hh"
#include "Symbol.hh"
#include "Tokenizer.hh"
#include "UserFunction.hh"
#include "UserPreferences.hh"
#include "ValueHistory.hh"
#include "Workspace.hh"

#include "Value.hh"

int Command::boxing_format = 0;
ShapeItem Command::APL_expression_count = 0;

//-----------------------------------------------------------------------------
void
Command::process_line()
{
UCS_string accu;   // for new-style multiline strings
UCS_string prompt = Workspace::get_prompt();
   for (;;)
       {
         UCS_string line;
         bool eof = false;
         InputMux::get_line(LIM_ImmediateExecution, prompt,
                      line, eof, LineInput::get_history());

         if (eof) CERR << "EOF at " << LOC << endl;

         if (line.ends_with("\"\"\""))   /// start or end of multi-line
            {
               if (accu.size() == 0)    // start of multi-line
                  {
                    accu = line;
                    prompt.prepend(UNI_RIGHT_ARROW);
                    accu.resize(line.size() - 3);   // discard """
                    accu.append(UNI_ASCII_SPACE);
                  }
               else                     // end of multi-line
                  {
                    accu.pop_back();   // trailing " "
                    process_line(accu);
                    return;
                  }
            }
         else if (accu.size())   // inside multi-line
            {
              accu.append_ASCII("\"");
              accu.append(line.do_escape(true));
              accu.append_ASCII("\" ");
            }
         else                   // normal input line
            {
              process_line(line);
              return;
            }
       }
}
//-----------------------------------------------------------------------------
void
Command::process_line(UCS_string & line)
{
   line.remove_leading_whitespaces();
   if (line.size() == 0)   return;   // empty input line

   switch(line[0])
      {
         case UNI_ASCII_R_PARENT:      // regular command, e.g. )SI
              do_APL_command(COUT, line);
              if (line.size())   break;
              return;

         case UNI_ASCII_R_BRACK:       // debug command, e.g. ]LOG
              do_APL_command(CERR, line);
              if (line.size())   break;
              return;

         case UNI_NABLA:               // e.g. ∇FUN
              Nabla::edit_function(line);
              return;

         case UNI_ASCII_NUMBER_SIGN:   // e.g. # comment
         case UNI_COMMENT:             // e.g. ⍝ comment
              return;

        default: ;
      }

   ++APL_expression_count;
   do_APL_expression(line);
}
//-----------------------------------------------------------------------------
bool
Command::do_APL_command(ostream & out, UCS_string & line)
{
const UCS_string line1(line);   // the original line

   // split line into command and arguments
   //
UCS_string cmd;   // the command without arguments
int len = 0;
   line.copy_black(cmd, len);

UCS_string arg(line, len, line.size() - len);
UCS_string_vector args = split_arg(arg);
   line.clear();
   if (!cmd.starts_iwith(")MORE")) 
      {
        // clear )MORE info unless command is )MORE
        //
        Workspace::more_error().clear();
      }

#define cmd_def(cmd_str, code, garg, _hint)                          \
   if (cmd.starts_iwith(cmd_str))                                    \
      { if (check_params(out, cmd_str, args.size(), garg))   return true; \
        code; return false; }
#include "Command.def"

   // check for user defined commands...
   //
   loop(u, Workspace::get_user_commands().size())
       {
         if (cmd.starts_iwith(Workspace::get_user_commands()[u].prefix))
            {
              do_USERCMD(out, line, line1, cmd, args, u);
              return true;
            }
       }

     out << "BAD COMMAND" << endl;
     return false;
}
//-----------------------------------------------------------------------------
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

UCS_string args_ucs(args);
   loop (a, args_ucs.size())   switch(args_ucs[a])
       {
         case '[': ++brackets;   in_param = false;   continue;
         case ']': --brackets;   in_param = false;   continue;
         case '|':               in_param = false;
              if (brackets)   --opt_args;
              else            --mandatory_args;
              continue;
         case '.':
              if (a < (args_ucs.size() - 2) &&
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

         default: Q(args_ucs[a])   Q(int(args_ucs[a]))
       }

   if (argc < mandatory_args)   // too few parameters
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "missing parameter(s) in command " << command
                     << ". Usage:\n"
                     << "      " << command << " " << args;
        return true;
      }

   if (many)   return false;

   if (argc > (mandatory_args + opt_args))   // too many parameters
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "too many (" << argc<< ") parameter(s) in command "
                     << command << ". Usage:\n"
                     << "      " << command << " " << args;
        return true;
      }

   return false;   // OK
}
//-----------------------------------------------------------------------------
void
Command::do_APL_expression(UCS_string & line)
{
   Workspace::more_error().clear();

Executable * statements = 0;
   try
      {
        statements = StatementList::fix(line, LOC);
      }
   catch (Error err)
      {
        UERR << Error::error_name(err.get_error_code());
        if (Workspace::more_error().size())   UERR << UNI_ASCII_PLUS;
        UERR << endl;
        if (*err.get_error_line_2())
           {
             COUT << "      " << err.get_error_line_2() << endl
                  << "      " << err.get_error_line_3() << endl;
           }

        err.print(UERR, LOC);
        delete statements;
        return;
      }
   catch (...)
      {
        CERR << "*** Command::process_line() caught other exception ***"
             << endl;
        delete statements;
        cmd_OFF(0);
      }

   if (statements == 0)
      {
        COUT << "main: Parse error." << endl;
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
//-----------------------------------------------------------------------------
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

// Q(token)

         // start over if execution has pushed a new SI entry
         //
         if (token.get_tag() == TOK_SI_PUSHED)   continue;

check_EOC:
         if (Workspace::SI_top()->is_safe_execution_start())
            {
              Quad_EC::eoc(token);
            }

         // the far most frequent cases are TC_VALUE and TOK_VOID
         // so we handle them first.
         //
         if (token.get_Class() == TC_VALUE || token.get_tag() == TOK_VOID )
            {
              if (Workspace::SI_top()->get_parse_mode() == PM_STATEMENT_LIST)
                 {
                   if (attention_is_raised())
                      {
                        clear_attention_raised(LOC);
                        clear_interrupt_raised(LOC);
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
                Prefix & prefix =
                         Workspace::SI_top()->get_prefix();
                Assert(prefix.at0().get_tag() == TOK_SI_PUSHED);

                new (&prefix.tos().tok) Token(token);
              }
              if (attention_is_raised())
                 {
                   clear_attention_raised(LOC);
                   clear_interrupt_raised(LOC);
                   ATTENTION;
                 }

              continue;
            }

         if (token.get_tag() == TOK_BRANCH)
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
                    MORE_ERROR() <<
                    "branch back into function (→N) without suspended function";
                    SYNTAX_ERROR;   // →N without function,
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
              return;
            }

         if (token.get_tag() == TOK_ERROR)
            {
              if (token.get_int_val() == E_COMMAND_PUSHED)
                 {
                   Workspace::pop_SI(LOC);
                   UCS_string pushed_command = Workspace::get_pushed_Command();
                   process_line(pushed_command);
                   pushed_command.clear();
                   Workspace::push_Command(pushed_command);   // clear in
                   return;
                 }

              // clear attention and interrupt flags
              //
              clear_attention_raised(LOC);
              clear_interrupt_raised(LOC);

              // check for safe execution mode. Unroll all SI entries that
              // have the same safe_execution_count, except the last
              // unroll the SI stack.
              //
              if (Workspace::SI_top()->get_safe_execution())
                 {
                  StateIndicator * si = Workspace::SI_top();
                   while (si->get_parent() && si->get_safe_execution() ==
                          si->get_parent()->get_safe_execution())
                      {
                        si = si->get_parent();
                        Workspace::pop_SI(LOC);
                      }

                    goto check_EOC;
                  }

              // if suspend is not allowed then pop all SI entries that
              // don't allow suspend
              //
              if (Workspace::SI_top()->get_executable()->cannot_suspend())
                 {
                    Error err = StateIndicator::get_error(Workspace::SI_top());
                    while (Workspace::SI_top()->get_executable()
                                              ->cannot_suspend())
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
                   Value::erase_stale(LOC);
                   IndexExpr::erase_stale(LOC);
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
//-----------------------------------------------------------------------------
void 
Command::cmd_XTERM(ostream & out, const UCS_string & arg)
{
const char * term = getenv("TERM");
   if (!strncmp(term, "dumb", 4) && arg.starts_iwith("ON"))
      {
        out << "impossible on dumb terminal" << endl;
      }
   else if (arg.starts_iwith("OFF") || arg.starts_iwith("ON"))
      {
        Output::toggle_color(arg);
      }
   else if (arg.size() == 0)
      {
        out << "]COLOR/XTERM ";
        if (Output::color_enabled()) out << "ON"; else out << "OFF";
        out << endl;
      }
   else
      {
        out << "BAD COMMAND" << endl;
        return;
      }
}
//-----------------------------------------------------------------------------
UCS_string_vector
Command::split_arg(const UCS_string & arg)
{
UCS_string_vector result;
   for (int idx = 0; ; )
      {
        UCS_string next;
        arg.copy_black(next, idx);
        if (next.size() == 0)   return result;

        result.push_back(next);
      }
}
//-----------------------------------------------------------------------------
void 
Command::cmd_BOXING(ostream & out, const UCS_string & arg)
{
int format = arg.atoi();

   if (arg.size() == 0)
      {
        out << "]BOXING ";
        if (boxing_format == 0) out << "OFF";
        else out << boxing_format;
        out << endl;
        return;
      }

   if (arg.starts_iwith("OFF"))   format = 0;
   switch (format)
      {
        case -29:
        case -25: case -24: case -23:
        case -22: case -21: case -20:
        case -9: case  -8: case  -7:
        case -4: case  -3: case  -2:
        case  0:
        case  2: case   3: case   4:
        case  7: case   8: case   9:
        case 20: case  21: case  22:
        case 23: case  24: case  25:
        case 29:
                 boxing_format = format;
                 return;
      }

   out << "BAD ]BOXING PARAMETER+" << endl;
   MORE_ERROR() << "Parameter " << arg << " is not valid for command ]BOXING.\n"
      "  Valid parameters are OFF, N, and -N with\n"
      "  N ∈ { 2, 3, 4, 7, 8, 9, 20, 21, 22, 23, 24, 25, 29 }";
}
//-----------------------------------------------------------------------------
bool
Command::val_val::compare_val_val(const val_val & A,
                                  const val_val & B, const void *)
{
   return A.child > B.child;
}
//-----------------------------------------------------------------------------
int
Command::val_val::compare_val_val1(const void * key, const void * B)
{
const void * Bv = reinterpret_cast<const val_val *>(B)->child;
   return charP(key) - charP(Bv);
}
//-----------------------------------------------------------------------------
void 
Command::cmd_CHECK(ostream & out)
{
   // erase stale functions from failed ⎕EX
   //
   {
     bool erased = false;
     int stale = Workspace::cleanup_expunged(CERR, erased);
     if (stale)
        {
          out << "WARNING - " << stale << " stale functions ("
               << (erased ? "" : "not ") << "erased)" << endl;
        }
     else out << "OK      - no stale functions" << endl;
   }

   {
     const int stale = Value::print_stale(CERR);
     if (stale)
        {
          out << "ERROR   - " << stale << " stale values" << endl;
          IO_Files::apl_error(LOC);
        }
     else out << "OK      - no stale values" << endl;
   }
   {
     const int stale = IndexExpr::print_stale(CERR);
     if (stale)
        {
          out << "ERROR   - " << stale << " stale indices" << endl;
          IO_Files::apl_error(LOC);
        }
     else out << "OK      - no stale indices" << endl;
   }

   // discover duplicate parents
   //
std::vector<val_val> values;
ShapeItem duplicate_parents = 0;
   for (const DynamicObject * obj = DynamicObject::get_all_values()->get_next();
        obj != DynamicObject::get_all_values(); obj = obj->get_next())
       {
         const Value * val = static_cast<const Value *>(obj);

         val_val vv = { 0, val };   // no parent
         values.push_back(vv);
       }

   Heapsort<val_val>::sort(&values[0], values.size(), 0,
                           &val_val::compare_val_val);
   loop(v, (values.size() - 1))
       Assert(&values[v].child < &values[v + 1].child);

   /// set parents
   loop(v, values.size())   // for every .child (acting as parent here)
      {
        const Value * val = values[v].child;
        const ShapeItem ec = val->nz_element_count();
        loop(e, ec)   // for every ravel cell of the (parent-) value
            {
              const Cell & cP = val->get_ravel(e);
              if (!cP.is_pointer_cell())   continue;   // not a parent

              const Value * sub = cP.get_pointer_value().get();
              Assert1(sub);

              val_val * vvp = reinterpret_cast<val_val *>
                    (bsearch(sub, &values[0], values.size(), sizeof(val_val),
                            val_val::compare_val_val1));
              Assert(vvp);
              if (vvp->parent == 0)   // child has no parent
                 {
                   vvp->parent = val;
                 }
              else
                 {
                   ++duplicate_parents;
                   out << "Value * vvp=" << voidP(vvp) << " already has parent "
                       << voidP(vvp->parent) << " when checking Value * val="
                       << voidP(vvp) << endl;

                   out << "History of the child:" << endl;
                   VH_entry::print_history(out, vvp->child, LOC);
                   out << "History of the first parent:" << endl;
                   VH_entry::print_history(out, vvp->parent, LOC);
                   out << "History of the second parent:" << endl;
                   VH_entry::print_history(out, val, LOC);
                   out << endl;
                 }
           }
      }

   if (duplicate_parents)
        {
          out << "ERROR   - " << duplicate_parents
              << " duplicate parents" << endl;
          IO_Files::apl_error(LOC);
        }
   else out << "OK      - no duplicate parents" << endl;
}
//-----------------------------------------------------------------------------
void 
Command::cmd_CONTINUE(ostream & out)
{
UCS_string wsname("CONTINUE");
   Workspace::wsid(out, wsname, LIB0, false);     // )WSID CONTINUE
   Workspace::save_WS(out, LIB0, wsname, true);   // )SAVE
   cmd_OFF(0);                                    // )OFF
}
//-----------------------------------------------------------------------------
void 
Command::cmd_COPY(ostream & out, UCS_string_vector & args, bool protection)
{
LibRef libref = LIB0;
const Unicode l = args[0][0];
      if (Avec::is_digit(l))
      {
        libref = LibRef(l - '0');
        args.erase(args.begin());
      }

   if (args.size() == 0)   // at least workspace name is required
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "missing workspace name in command )COPY or )PCOPY";
        return;
      }

UCS_string wsname = args[0];
   args.erase(args.begin());
   Workspace::copy_WS(out, libref, wsname, args, protection);
}
//-----------------------------------------------------------------------------
void 
Command::cmd_DOXY(ostream & out, UCS_string_vector & args)
{
UTF8_string root("/tmp");
   if (args.size())   root = UTF8_string(args[0]);

   try
     {
       Doxy doxy(out, root);
       doxy.gen();

       if (doxy.get_errors())
          out << "Command ]DOXY failed (" << doxy.get_errors() << " errors)"
              << endl;
      else
         out << "Command ]DOXY finished successfully." << endl
             << "    The generated documentation was stored in directory "
             << doxy.get_root_dir() << endl
             << "    You may want to browse it from file://"
             << doxy.get_root_dir()
             << "/index.html" << endl;
     }
   catch (...) {}
}
//-----------------------------------------------------------------------------
void 
Command::cmd_DROP(ostream & out, const UCS_string_vector & lib_ws)
{
   // Command is:
   //
   // )DROP wsname
   // )DROP libnum wsname

   // lib_ws.size() is 1 or 2. If 2 then the first is the lib number
   //
LibRef libref = LIB_NONE;
UCS_string wname = lib_ws.back();
   if (lib_ws.size() == 2)   libref = LibRef(lib_ws[0][0] - '0');

UTF8_string filename = LibPaths::get_lib_filename(libref, wname, true,
                                                  ".xml", ".apl");

const int result = unlink(filename.c_str());
   if (result)
      {
        out << wname << " NOT DROPPED: " << strerror(errno) << endl;
        MORE_ERROR() << "could not unlink file " << filename;
      }
   else
      {
        Workspace::get_v_Quad_TZ().print_timestamp(out, now()) << endl;
      }
}
//-----------------------------------------------------------------------------
void 
Command::cmd_DUMP(ostream & out, const UCS_string_vector & args,
                  bool html, bool silent)
{
   // Command is:
   //
   // )DUMP
   // )DUMP workspace
   // )DUMP lib workspace

   if (args.size() > 0)   // workspace or lib workspace
      {
        LibRef lib;
        UCS_string wsname;
        if (resolve_lib_wsname(out, args, lib, wsname))   return;   // error
        Workspace::dump_WS(out, lib, wsname, html, silent);
        return;
      }

   // )DUMP: use )WSID unless CLEAR WS
   //
LibRef wsid_lib = LIB0;
UCS_string wsid_name = Workspace::get_WS_name();
   if (Avec::is_digit(wsid_name[0]))   // wsid contains a libnum
      {
        wsid_lib = LibRef(wsid_name[0] - '0');
        wsid_name.erase(0);
        wsid_name.remove_leading_whitespaces();
      }

   if (wsid_name.compare(UCS_string("CLEAR WS")) == 0)   // don't dump CLEAR WS
      {
        COUT << "NOT DUMPED: THIS WS IS CLEAR WS" << endl;
        MORE_ERROR() <<
        "the workspace was not dumped because 'CLEAR WS' is a special\n"
        "workspace name that cannot be dumped. "
        "First create WS name with )WSID <name>."; 
        return;
      }

   Workspace::dump_WS(out, wsid_lib, wsid_name, html, silent);
}
//-----------------------------------------------------------------------------
void
Command::cmd_ERASE(ostream & out, UCS_string_vector & args)
{
   Workspace::erase_symbols(out, args);
}
//-----------------------------------------------------------------------------
void 
Command::cmd_KEYB(ostream & out)
{
   // maybe print user-supplied keyboard layout file
   //
   if (uprefs.keyboard_layout_file.size())
      {
        FILE * layout = fopen(uprefs.keyboard_layout_file.c_str(), "r");
        if (layout == 0)
           {
             out << "Could not open " << uprefs.keyboard_layout_file
                 << ": " << strerror(errno) << endl
                 << "Showing default layout instead" << endl;
           }
        else
           {
             out << "User-defined Keyboard Layout:\n";
             for (;;)
                 {
                    const int cc = fgetc(layout);
                    if (cc == EOF)   break;
                    out << char(cc);
                 }
             out << endl;
             return;
           }
      }

   out << "US Keyboard Layout:\n"
                              "\n"
"╔════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦═════════╗\n"
"║ ~  ║ !⌶ ║ @⍫ ║ #⍒ ║ $⍋ ║ %⌽ ║ ^⍉ ║ &⊖ ║ *⍟ ║ (⍱ ║ )⍲ ║ _! ║ +⌹ ║         ║\n"
"║ `◊ ║ 1¨ ║ 2¯ ║ 3< ║ 4≤ ║ 5= ║ 6≥ ║ 7> ║ 8≠ ║ 9∨ ║ 0∧ ║ -× ║ =÷ ║ BACKSP  ║\n"
"╠════╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦══════╣\n"
"║       ║ Q  ║ W⍹ ║ E⋸ ║ R  ║ T⍨ ║ Y¥ ║ U  ║ I⍸ ║ O⍥ ║ P⍣ ║ {⍞ ║ }⍬ ║  |⊣  ║\n"
"║  TAB  ║ q? ║ w⍵ ║ e∈ ║ r⍴ ║ t∼ ║ y↑ ║ u↓ ║ i⍳ ║ o○ ║ p⋆ ║ [← ║ ]→ ║  \\⊢  ║\n"
"╠═══════╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩══════╣\n"
"║ (CAPS   ║ A⍶ ║ S  ║ D  ║ F  ║ G  ║ H  ║ J⍤ ║ K  ║ L⌷ ║ :≡ ║ \"≢ ║         ║\n"
"║  LOCK)  ║ a⍺ ║ s⌈ ║ d⌊ ║ f_ ║ g∇ ║ h∆ ║ j∘ ║ k' ║ l⎕ ║ ;⍎ ║ '⍕ ║ RETURN  ║\n"
"╠═════════╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═════════╣\n"
"║             ║ Z  ║ Xχ ║ C¢ ║ V  ║ B£ ║ N  ║ M  ║ <⍪ ║ >⍙ ║ ?⍠ ║          ║\n"
"║  SHIFT      ║ z⊂ ║ x⊃ ║ c∩ ║ v∪ ║ b⊥ ║ n⊤ ║ m| ║ ,⍝ ║ .⍀ ║ /⌿ ║  SHIFT   ║\n"
"╚═════════════╩════╩════╩════╩════╩════╩════╩════╩════╩════╩════╩══════════╝\n"
   << endl;
}
//-----------------------------------------------------------------------------
void 
Command::cmd_PSTAT(ostream & out, const UCS_string & arg)
{
#ifndef PERFORMANCE_COUNTERS_WANTED
   out << "\n"
<< "Command ]PSTAT is not available, since performance counters were not\n"
"configured for this APL interpreter. To enable performance counters (which\n"
"will slightly decrease performance), recompile the interpreter as follows:"

<< "\n\n"
"   ./configure PERFORMANCE_COUNTERS_WANTED=yes (... "
<< "other configure options"
<< ")\n"
"   make\n"
"   make install (or try: src/apl)\n"
"\n"

<< "above the src directory."
<< "\n";

   return;
#endif

   if (arg.starts_iwith("CLEAR"))
      {
        out << "Performance counters cleared" << endl;
        Performance::reset_all();
        return;
      }

   if (arg.starts_iwith("SAVE"))
      {
        const char * filename = "./PerformanceData.def";
        ofstream outf(filename, ofstream::out);
        if (!outf.is_open())
           {
             out << "opening " << filename
                 << " failed: " << strerror(errno) << endl;
             return;
           }

        out << "Writing performance data to file " << filename << endl;
        Performance::save_data(out, outf);
        return;
      }

Pfstat_ID iarg = PFS_ALL;
   if (arg.size() > 0)   iarg = Pfstat_ID(arg.atoi());

   Performance::print(iarg, out);
}
//-----------------------------------------------------------------------------
void
Command::primitive_help(ostream & out, const char * arg, int arity,
                        const char * prim, const char * name,
                        const char * brief, const char * descr)
{
   if (strcmp(arg, prim))   return;

   switch(arity)
      {
        case -5: out << "   quasi-dyadic operator:   Z ← A (∘ "
                     << prim << " G) B";                                  break;
        case -4: out << "   dyadic operator:   Z ← A (F "
                     << prim << " G) B";                                  break;
        case -3: out << "   dyadic operator:   Z ← (F "
                     << prim << " G) B";                                  break;
        case -2: out << "   monadic operator:  Z ← A (F "
                     << prim << ") B";                                    break;
        case -1: out << "   monadic operator:  Z ← (F "
                     << prim << ") B";                                    break;
        case 0:  out << "    niladic function: Z ← " << prim;             break;
        case 1:  out << "    monadic function: Z ← " << prim << " B";     break;
        case 2:  out << "    dyadic function:  Z ← A " << prim << " B";   break;
        default: FIXME;
      }

   out << "  (" << name  <<  ")" << endl
       << "    " << brief << endl;

   if (descr)   out << descr << endl;
}
//-----------------------------------------------------------------------------

/// return the lengt differece between a UCS_string and its UTF8 encoding
static inline int
len_diff(const char * txt)
{
int ret = 0;
   while (const char cc = *txt++)   if ((cc & 0xC0) == 0x80)   ++ret;
   return ret;
}

void 
Command::cmd_HELP(ostream & out, const UCS_string & arg)
{
   if (arg.size() > 0 && Avec::is_first_symbol_char(arg[0]))
      {
        // help for a user defined name
        //
        CERR << "symbol " << arg << " ";
        Symbol * sym = Workspace::lookup_existing_symbol(arg);
        if (sym == 0)
           {
             CERR << "does not exist" << endl;
             return;
           }

        if (sym->is_erased())
           {
             CERR << "is erased." << endl;
             return;
           }

        ValueStackItem * vs = sym->top_of_stack();
        if (vs == 0)
           {
             CERR << " has no stack." << endl;
             return;
           }

        switch(vs->name_class)
           {
             case NC_INVALID:
                  CERR << "has no valid name class" << endl;
                  return;

             case NC_UNUSED_USER_NAME:
                  CERR << "is an unused name" << endl;
                  return;

             case NC_LABEL:
                  CERR << "is a label (line " << vs->sym_val.label
                       << ")" << endl;
                  return;

             case NC_VARIABLE:
                  {
                    CERR << "is a variable:" << endl;
                    Value_P val = sym->get_value();
                    if (!!val)
                       {
                         val->print_properties(CERR, 4, true);
                       }
                  }
                  CERR << endl;
                  return;

             case NC_FUNCTION:
                  {
                    Function * fun = sym->get_function();
                    Assert(fun);
                    if (fun->is_native())
                       {
                         const NativeFunction *nf =
                               reinterpret_cast<const NativeFunction *>(fun);
                         CERR << "is a native function implemented in "
                              << nf->get_so_path() << endl
                              << "    load state: " << (nf->is_valid() ?
                                 "OK (loaded)" : "error") << endl;
                         return;
                       }

                    CERR << "is a ";
                    if      (fun->get_fun_valence() == 2)   CERR << "dyadic";
                    else if (fun->get_fun_valence() == 1)   CERR << "monadic";
                    else                                    CERR << "niladic";
                    CERR << " defined function:" << endl;

                    const UserFunction * ufun = fun->get_ufun1();
                    Assert(ufun);
                    ufun->help(CERR);
                  }
                  return;

             case NC_OPERATOR:
                  {
                    Function * fun = sym->get_function();
                    Assert(fun);
                    CERR << "is a ";
                    if      (fun->get_oper_valence() == 2)   CERR << "dyadic";
                    else                                    CERR << "monadic";
                    CERR << " defined operator:" << endl;

                    const UserFunction * ufun = fun->get_ufun1();
                    Assert(ufun);
                    ufun->help(CERR);
                  }
                  return;

             case NC_SHARED_VAR:
                  CERR << "is a shared variable" << endl;
                  return;

           }

        return;
      }

   if (arg.size() == 1)   // help for an APL primitive
      {
        UTF8_string arg_utf(arg);
        const char * arg_cp = arg_utf.c_str();

#define help_def(ar, prim, name, title, descr)              \
   primitive_help(out, arg_cp, ar, prim, name, title, descr);
#include "Help.def"

         return;
      }

   enum { COL2 = 40 };   ///< where the second column starts

UCS_string_vector commands;
   commands.reserve(60);

   out << left << "APL Commands:" << endl;
#define cmd_def(cmd_str, _cod, arg, _hint) \
   { UCS_string c(cmd_str " " arg);   commands.push_back(c); }
#include "Command.def"

bool left_col = true;
   loop(c, commands.size())
      {
        const UCS_string & cmd = commands[c];
        if (left_col)
           {
              out << "      " << setw(COL2 - 2) << cmd;
              left_col = false;
           }
        else
           {
              out << cmd << endl;
              left_col = true;
           }
      }

  if (Workspace::get_user_commands().size())
     {
       out << endl << "User defined commands:" << endl;
       loop(u, Workspace::get_user_commands().size())
           {
             out << "      " << Workspace::get_user_commands()[u].prefix
                 << " [args]  calls:  ";
             if (Workspace::get_user_commands()[u].mode)
                out << "tokenized-args ";
 
             out << Workspace::get_user_commands()[u].apl_function
                 << " quoted-args" << endl;
           }
     }

   out << endl << "System variables:" << endl
       << "      " << setw(COL2)
       << "⍞       Character Input/Output"
       << "⎕       Evaluated Input/Output" << endl;
   left_col = true;
#define ro_sv_def(x, _str, txt)                                            \
   { const UCS_string & ucs = Workspace::get_v_ ## x().get_name();         \
     if (left_col)   out << "      " << setw(8) << ucs << setw(30) << txt; \
     else            out << setw(8) << ucs << txt << endl;                 \
        left_col = !left_col; }
#define rw_sv_def(x, str, txt) ro_sv_def(x, str, txt)
#include "SystemVariable.def"

   out << endl << "System functions:" << endl;
   left_col = true;
#define ro_sv_def(x, _str, _txt)
#define rw_sv_def(x, _str, _txt)
#define sf_def(_q, str, txt)                                              \
   if (left_col)   out << "      ⎕" << setw(7) << str << setw(30 +        \
                                        len_diff(txt)) << txt;            \
   else            out << "⎕" << setw(7) << str << txt << endl;           \
   left_col = !left_col;
#include "SystemVariable.def"
}
//-----------------------------------------------------------------------------
void 
Command::cmd_HISTORY(ostream & out, const UCS_string & arg)
{
   if (arg.size() == 0)                  LineInput::print_history(out);
   else if (arg.starts_iwith("CLEAR"))   LineInput::clear_history(out);
   else                                  out << "BAD COMMAND" << endl;
}
//-----------------------------------------------------------------------------
void
Command::cmd_HOST(ostream & out, const UCS_string & arg)
{
   if (uprefs.safe_mode)
      {
        out << 
"This interpreter was started in \"safe mode\" (command line option --safe,\n"
"see ⎕ARG). The APL command )HOST is not permitted in safe mode." << endl;
        return;
      }

UTF8_string host_cmd(arg);
FILE * pipe = popen(host_cmd.c_str(), "r");
   if (pipe == 0)   // popen() failed
      {
        out << ")HOST command failed: " << strerror(errno) << endl;
        return;
      }

   for (;;)
       {
         const int cc = fgetc(pipe);
         if (cc == EOF)   break;
         out << char(cc);
       }

int result = pclose(pipe);
   Log(LOG_verbose_error)
      {
        if (result)   CERR << "pclose(" << arg << ") says: "
                           << strerror(errno) << endl;
      }
   out << endl << IntCell(result) << endl;
}
//-----------------------------------------------------------------------------
void
Command::cmd_IN(ostream & out, UCS_string_vector & args, bool protection)
{
   // Command is:
   //
   // IN filename [objects...]

UCS_string fname = args[0];
   args[0] = args.back();
   args.pop_back();

UTF8_string filename = LibPaths::get_lib_filename(LIB_NONE, fname, true,
                                                  ".atf", 0);

FILE * in = fopen(filename.c_str(), "r");
   if (in == 0)   // open failed: try filename.atf unless already .atf
      {
        UTF8_string fname_utf8(fname);
        CERR << ")IN " << fname_utf8.c_str()
             << " failed: " << strerror(errno) << endl;

        char cc[200];
        snprintf(cc, sizeof(cc),
                 "command )IN: could not open file %s for reading: %s",
                 fname_utf8.c_str(), strerror(errno));
        Workspace::more_error() << cc;
        return;
      }

UTF8 buffer[80];
int idx = 0;

transfer_context tctx(protection);

   for (;;)
      {
        const int cc = fgetc(in);
        if (cc == EOF)   break;
        if (idx == 0 && cc == 0x0A)   // optional LF
           {
             // CERR << "CRLF" << endl;
             continue;
           }

        if (idx < 80)
           {
              if (idx < 72)   buffer[idx++] = cc;
              else            buffer[idx++] = 0;
             continue;
           }

        if (cc == 0x0D || cc == 0x15)   // ASCII or EBCDIC
           {
             tctx.is_ebcdic = (cc == 0x15);
             tctx.process_record(buffer, args);

             idx = 0;
             ++tctx.recnum;
             continue;
           }

        CERR << "BAD record charset (neither ASCII nor EBCDIC)" << endl;
        break;
      }

   fclose(in);
}
//-----------------------------------------------------------------------------
void 
Command::cmd_LOAD(ostream & out, UCS_string_vector & args,
                  UCS_string & quad_lx, bool silent)
{
   // Command is:
   //
   // LOAD wsname
   // LOAD libnum wsname

LibRef lib;
UCS_string wsname;
   if (resolve_lib_wsname(out, args, lib, wsname))   return;   // error

   Workspace::load_WS(out, lib, wsname, quad_lx, silent);
}
//-----------------------------------------------------------------------------
void 
Command::cmd_LIBS(ostream & out, const UCS_string_vector & args)
{
   // Command is:
   //
   // )LIB N path         (set libdir N to path)
   // )LIB path           (set libroot to path)
   // )LIB                (display root and path states)
   //
   if (args.size() == 2)   // set individual dir
      {
        const UCS_string & libref_ucs = args[0];
        const int libref = libref_ucs[0] - '0';
        if (libref_ucs.size() != 1 || libref < 0 || libref > 9)
           {
             CERR << "Invalid library reference " << libref_ucs << "'" << endl;
             return;
           }

        UTF8_string path(args[1]);
        LibPaths::set_lib_dir(LibRef(libref), path.c_str(),
                              LibPaths::LibDir::CSRC_CMD);
        out << "LIBRARY REFERENCE " << libref << " SET TO " << path << endl;
        return;
      }

   if (args.size() == 1)   // set root
      {
        UTF8_string utf(args[0]);
        LibPaths::set_APL_lib_root(utf.c_str());
        out << "LIBRARY ROOT SET TO " << args[0] << endl;
        return;
      }

   out << "Library root: " << LibPaths::get_APL_lib_root() << 
"\n"
"\n"
"Library reference number mapping:\n"
"\n"
"---------------------------------------------------------------------------\n"
"Ref Conf  Path                                                State   Err\n"
"---------------------------------------------------------------------------\n";
         

   loop(d, 10)
       {
          UTF8_string path = LibPaths::get_lib_dir(LibRef(d));
          out << " " << d << " ";
          switch(LibPaths::get_cfg_src(LibRef(d)))
             {
                case LibPaths::LibDir::CSRC_NONE:      out << "NONE" << endl;
                                                     continue;
                case LibPaths::LibDir::CSRC_ENV:       out << "ENV   ";   break;
                case LibPaths::LibDir::CSRC_PWD:       out << "PWD   ";   break;
                case LibPaths::LibDir::CSRC_PREF_SYS:  out << "PSYS  ";   break;
                case LibPaths::LibDir::CSRC_PREF_HOME: out << "PUSER ";   break;
                case LibPaths::LibDir::CSRC_CMD:       out << "CMD   ";   break;
             }

        out << left << setw(52) << path.c_str();
        DIR * dir = opendir(path.c_str());
        if (dir)   { out << " present" << endl;   closedir(dir); }
        else       { out << " missing (" << errno << ")" << endl; }
      }

   out <<
"===========================================================================\n" << endl;
}
//-----------------------------------------------------------------------------
DIR *
Command::open_LIB_dir(UTF8_string & path, ostream & out,
                      const UCS_string_vector & args)
{
   // args can be one of:
   //                              example:
   // 1.                           )LIB
   // 2.  N                        )LIB 1
   // 3.  )LIB directory-name      )LIB /usr/lib/...
   //

UCS_string arg("0");
   if (args.size())   arg = args[0];

   if (args.size() == 0)                       // case 1.
      {
        path = LibPaths::get_lib_dir(LIB0);
      }
   else if (arg.size() == 1 &&
            Avec::is_digit(Unicode(arg[0])))   // case 2.
      {
        path = LibPaths::get_lib_dir(LibRef(arg[0] - '0'));
      }
   else                                        // case 3.
      {
        path = UTF8_string(arg);
      }

   // follow symbolic links, but not too often (because symbolic links may
   // form an endless loop)...
   //
   loop(depth, 20)
       {
         char buffer[FILENAME_MAX + 1];
         const ssize_t len = readlink(path.c_str(), buffer, FILENAME_MAX);
         if (len <= 0)   break;   // not a symlink

         buffer[len] = 0;
         if (buffer[0] == '/')   // absolute path
            {
              path = UTF8_string(buffer);
            }
          else                   // relative path
            {
              path += '/';
              path.append_UTF8(UTF8_string(buffer));
            }
       }

DIR * dir = opendir(path.c_str());

   if (dir == 0)
      {
        const char * why = strerror(errno);
        out << "IMPROPER LIBRARY REFERENCE '" << arg << "': " << why << endl;

        MORE_ERROR() <<
        "path '" << path << "' could not be opened as directory: " << why;
        return 0;   // error
      }

   return dir;
}
//-----------------------------------------------------------------------------
bool
Command::is_directory(dirent * entry, const UTF8_string & path)
{
#ifdef _DIRENT_HAVE_D_TYPE
   return entry->d_type == DT_DIR;
#endif

UTF8_string filename = path;
UTF8_string entry_name(entry->d_name);
   filename += '/';
   filename.append_UTF8(entry_name);

DIR * dir = opendir(filename.c_str());
   if (dir) closedir(dir);
   return dir != 0;
}
//-----------------------------------------------------------------------------
void 
Command::lib_common(ostream & out, const UCS_string_vector & args_range,
                    int variant)
{
   // 1. check for (and then extract) an optional range parameter...
   //
UCS_string_vector args;
const UCS_string * range = 0;
   loop(a, args_range.size())
      {
        const UCS_string & arg = args_range[a];
        bool is_range = false;
        bool is_path = false;
        loop(aa, arg.size())
            {
              const Unicode uni = arg[aa];
              if (uni == UNI_ASCII_MINUS)
                 {
                   is_range = true;
                   break;
                 }

              if (!Avec::is_symbol_char(uni))
                 {
                   is_path = true;
                   break;
                 }
            }

         if (is_path)   // normal (non-range) arg
            {
              args.push_back(arg);
            }
         else if (!is_range)   // normal (non-range) arg
            {
              args.push_back(arg);
            }
         else if (range)   // second non-range arg
            {
              MORE_ERROR() <<
              "multiple range parameters in )LIB or ]LIB command";
              return;
            }
         else
            {
              range = &arg;
            }
      }

UCS_string from;
UCS_string to;
   if (range)
      {
        const bool bad_from_to = parse_from_to(from, to, *range);
        if (bad_from_to)
           {
             CERR << "bad range argument" << endl;
             MORE_ERROR() << "bad range argument " << *range
                  << ", expecting from-to";
             return;
           }
      }

   // 2. open directory
   //
UTF8_string path;
DIR * dir = open_LIB_dir(path, out, args);
   if (dir == 0)   return;

   // 3. collect files and directories
   //
UCS_string_vector files;
UCS_string_vector directories;

   for (;;)
       {
         dirent * entry = readdir(dir);
         if (entry == 0)   break;   // directory done

         UTF8_string filename_utf8(entry->d_name);
         UCS_string filename(filename_utf8);

         // check range (if any)...
         //
         if (from.size() && filename.lexical_before(from))   continue;
         if (to.size() && to.lexical_before(filename))       continue;

         if (is_directory(entry, path))
            {
              if (filename_utf8[0] == '.')   continue;
              filename.append(UNI_ASCII_SLASH);
              directories.push_back(filename);
              continue;
            }

         const int dlen = strlen(entry->d_name);
         if (variant == 1)
            {
              if (filename_utf8.ends_with(".apl"))
                 {
                   filename.resize(filename.size() - 4);   // skip extension
                   files.push_back(filename);
                 }
              else if (filename_utf8.ends_with(".xml"))
                 {
                   filename.resize(filename.size() - 4);   // skip extension
                   files.push_back(filename);
                 }
            }
         else
            {
              if (filename[0] == '.')          continue;  // skip dot files
              if (filename[dlen - 1] == '~')   continue;  // and editor backups
              files.push_back(filename);
            }
       }
   closedir(dir);

   // 4. sort directories and filenames alphabetically and append files
   //    to directories
   //
   directories.sort();
   files.sort();
   loop(f, files.size())
      {
        if (directories.size()  && directories.back() == files[f])
           {
             // there were some file.apl and file.xml. Skip the second
             //
             continue;
           }
        directories.push_back(files[f]);
      }

   // 5. list directories first, then files
   //
        
   // figure column widths
   //
   enum { tabsize = 4 };

std::vector<int> col_widths;
   directories.compute_column_width(tabsize, col_widths);

   loop(c, directories.size())
      {
        const size_t col = c % col_widths.size();
        out << directories[c];
        if (col == size_t(col_widths.size() - 1) ||
              c == ShapeItem(directories.size() - 1))
           {
             // last column or last item: print newline
             //
             out << endl;
           }
        else
           {
             // intermediate column: print spaces
             //
             const int len = tabsize*col_widths[col] - directories[c].size();
             Assert(len > 0);
             loop(l, len)   out << " ";
           }
      }
}
//-----------------------------------------------------------------------------
void 
Command::cmd_LIB1(ostream & out, const UCS_string_vector & args)
{
   // Command is:
   //
   // )LIB                (same as )LIB 0)
   // )LIB N              (show workspaces in library N without extensions)
   // )LIB from-to        (show workspaces named from-to in library 0
   // )LIB N from-to      (show workspaces named from-to in library N

   Command::lib_common(out, args, 1);
}
//-----------------------------------------------------------------------------
void 
Command::cmd_LIB2(ostream & out, const UCS_string_vector & args)
{
   // Command is:
   //
   // ]LIB                (same as )LIB 0)
   // ]LIB N              (show workspaces in library N with extensions)
   // ]LIB from-to        (show workspaces named from-to in library 0
   // ]LIB N from-to      (show workspaces named from-to in library N

   Command::lib_common(out, args, 2);
}
//-----------------------------------------------------------------------------
void 
Command::cmd_LOG(ostream & out, const UCS_string & arg)
{
#ifdef DYNAMIC_LOG_WANTED

   log_control(arg);

#else

   out <<
"\n"
"Command ]LOG is not available, since dynamic logging was not\n"
"configured for this APL interpreter. To enable dynamic logging (which\n"
"will slightly decrease performance), recompile the interpreter as follows:\n"
"\n"
"   ./configure DYNAMIC_LOG_WANTED=yes (... other configure options)\n"
"   make\n"
"   make install (or try: src/apl)\n"
"\n"
"above the src directory."
"\n";

#endif
}
//-----------------------------------------------------------------------------
void 
Command::cmd_MORE(ostream & out)
{
   if (Workspace::more_error().size() == 0)
      {
        out << "NO MORE ERROR INFO" << endl;
        return;
      }

   out << Workspace::more_error() << endl;
   return;
}
//-----------------------------------------------------------------------------
void 
Command::cmd_OFF(int exit_val)
{
   cleanup(true);
   COUT << endl;
   if (!uprefs.silent)
      {

        timeval end;
        gettimeofday(&end, 0);
        end.tv_sec -= uprefs.session_start.tv_sec;
        end.tv_usec -= uprefs.session_start.tv_usec;
        if (end.tv_usec < 1000000)   { end.tv_usec += 1000000;   --end.tv_sec; }
        COUT << "Goodbye." << endl
             << "Session duration: " << (end.tv_sec + 0.000001*end.tv_usec)
             << " seconds " << endl;

      }

   // restore the initial memory rlimit
   //
#ifndef RLIMIT_AS // BSD does not define RLIMIT_AS
# define RLIMIT_AS RLIMIT_DATA
#endif

rlimit rl;
   getrlimit(RLIMIT_AS, &rl);
   rl.rlim_cur = Quad_WA::initial_rlimit;
   setrlimit(RLIMIT_AS, &rl);

   exit(exit_val);
}
//-----------------------------------------------------------------------------
void
Command::cmd_OUT(ostream & out, UCS_string_vector & args)
{
UCS_string fname = args[0];
   args.erase(args.begin());

UTF8_string filename = LibPaths::get_lib_filename(LIB_NONE, fname, false,
                                                  ".atf", 0);

FILE * atf = fopen(filename.c_str(), "w");
   if (atf == 0)
      {
        const char * why = strerror(errno);
        out << ")OUT " << fname << " failed: " << why << endl;
        MORE_ERROR() << "command )OUT: could not open file " << fname
                     << " for writing: " << why;
        return;
      }

uint64_t seq = 1;   // sequence number for records written
   Workspace::write_OUT(atf, seq, args);

   fclose(atf);
}
//-----------------------------------------------------------------------------
bool
Command::check_name_conflict(ostream & out, const UCS_string & cnew,
                             const UCS_string cold)
{
int len = cnew.size();
        if (len > cold.size())   len = cold.size();

   loop(l, len)
      {
        int c1 = cnew[l];
        int c2 = cold[l];
        if (c1 >= 'a' && c1 <= 'z')   c1 -= 0x20;   // uppercase
        if (c2 >= 'a' && c2 <= 'z')   c2 -= 0x20;   // uppercase
        if (l && (c1 != c2))   return false;   // OK: different
     }

   out << "BAD COMMAND+" << endl;
   MORE_ERROR() << "conflict with existing command name in command ]USERCMD";

   return true;
}
//-----------------------------------------------------------------------------
bool
Command::check_redefinition(ostream & out, const UCS_string & cnew,
                            const UCS_string fnew, const int mnew)
{
   loop(u, Workspace::get_user_commands().size())
     {
       const UCS_string cold = Workspace::get_user_commands()[u].prefix;
       const UCS_string fold = Workspace::get_user_commands()[u].apl_function;
       const int mold = Workspace::get_user_commands()[u].mode;

       if (cnew != cold)   continue;

       // user command name matches; so must mode and function
       if (mnew != mold || fnew != fold)
         {
           out << "BAD COMMAND" << endl;
           MORE_ERROR() <<
           "conflict with existing user command definition in command ]USERCMD";
         }
       return true;
     }

   return false;
}
//-----------------------------------------------------------------------------
void 
Command::cmd_SAVE(ostream & out, const UCS_string_vector & args)
{
   // )SAVE
   // )SAVE workspace
   // )SAVE lib workspace

   if (args.size() > 0)   // workspace or lib workspace
      {
        LibRef lib;
        UCS_string wsname;
        if (resolve_lib_wsname(out, args, lib, wsname))   return;   // error
        Workspace::save_WS(out, lib, wsname, false);
        return;
      }

   // )SAVE without arguments: use )WSID unless CLEAR WS
   //
LibRef wsid_lib = LIB0;
UCS_string wsid_name = Workspace::get_WS_name();
   if (Avec::is_digit(wsid_name[0]))   // wsid contains a libnum
      {
        wsid_lib = LibRef(wsid_name[0] - '0');
        wsid_name.erase(0);
        wsid_name.remove_leading_whitespaces();
      }

   if (wsid_name.compare(UCS_string("CLEAR WS")) == 0)   // don't save CLEAR WS
      {
        COUT << "NOT SAVED: THIS WS IS CLEAR WS" << endl;
        MORE_ERROR() <<
        "the workspace was not saved because 'CLEAR WS' is a special\n"
        "workspace name that cannot be saved. "
        "First create WS name with )WSID <name>."; 
        return;
      }

   Workspace::save_WS(out, wsid_lib, wsid_name, true);
}
//-----------------------------------------------------------------------------
bool
Command::resolve_lib_wsname(ostream & out, const UCS_string_vector & args,
                            LibRef &lib, UCS_string & wsname)
{
   Assert(args.size() > 0);
   if (args.size() == 1)   // name without libnum
      {
        lib = LIB0;
        wsname = args[0];
        return false;   // OK
      }

   if (!(args[0].size() == 1 && Avec::is_digit(args[0][0])))
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "invalid library reference '" << args[0] << "'";
        return true;   // error
      }

   lib = LibRef(args[0][0] - '0');
   wsname = args[1];
   return false;   // OK
}
//-----------------------------------------------------------------------------
void
Command::cmd_USERCMD(ostream & out, const UCS_string & cmd,
                     UCS_string_vector & args)
{
   // ]USERCMD
   // ]USERCMD REMOVE-ALL
   // ]USERCMD REMOVE        ]existing-command
   // ]USERCMD ]new-command  APL-fun
   // ]USERCMD ]new-command  APL-fun  mode
   // ]USERCMD ]new-command  { ... }
   //
   if (args.size() == 0)
      {
        if (Workspace::get_user_commands().size())
           {
             loop(u, Workspace::get_user_commands().size())
                {
                  out << Workspace::get_user_commands()[u].prefix << " → ";
                  if (Workspace::get_user_commands()[u].mode)   out << "A ";
                  out << Workspace::get_user_commands()[u].apl_function << " B"
                      << " (mode " << Workspace::get_user_commands()[u].mode
                      << ")" << endl;
                }
           }
        return;
      }

  if (args.size() == 1 && args[0].starts_iwith("REMOVE-ALL"))
     {
       Workspace::get_user_commands().clear();
       out << "    All user-defined commands removed." << endl;
       return;
     }

  if (args.size() == 2 && args[0].starts_iwith("REMOVE"))
     {
       loop(u, Workspace::get_user_commands().size())
           {
             if (Workspace::get_user_commands()[u].prefix
                                                  .starts_iwith(args[1]) &&
                 args[1].starts_iwith(Workspace::get_user_commands()[u]
                                                          .prefix))   // same
                {
                  // print first and remove then!
                  //
                  out << "    User-defined command "
                      << Workspace::get_user_commands()[u].prefix
                      << " removed." << endl;
                  Workspace::get_user_commands().
                     erase(Workspace::get_user_commands().begin() + u);
                  return;
                }
           }

       out << "BAD COMMAND+" << endl;
       MORE_ERROR() << "user command in command ]USERCMD REMOVE does not exist";
       return;
     }

  // check if the user command is not followed by the string
  if (args.size() == 1)
     {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "user command syntax in ]USERCMD:"
                        " ]new-command  APL-fun  [mode]";
        return;
     }

   UCS_string command_name = args[0];
   UCS_string apl_fun = args[1];
   int mode = 0;

   // check if lambda
   bool is_lambda = false;
   if (apl_fun[0] == '{')
      {
         // looks like the user command is a lambda function.
         UCS_string result;
         // lambdas could contain spaces, collect all arguments in one string
         for (size_t i = 1; i < args.size(); ++i)
            {
               result << args[i];
            }
         // check if lamda-function closed properly
         if (result.back() == '}')
            {
               is_lambda = true;
               apl_fun = result;
               // determine the mode: if both alpha and omega present then
               // assume dyadic, otherwise monadic usage
               mode = (apl_fun.contains(UNI_OMEGA) &&
                       apl_fun.contains(UNI_ALPHA)) ? 1 : 0;
            }
         else
            {
               out << "BAD COMMAND+" << endl;
               MORE_ERROR() << "closing } in lambda function not found";
               return;
            }
      }

   if (args.size() > 3 && !is_lambda)
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "too many parameters in command ]USERCMD";
        return;
      }

   // check mode
   if (!is_lambda && args.size() == 3)   mode = args[2].atoi();
   if (mode < 0 || mode > 1)
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "unsupported mode " << mode
                     << " in command ]USERCMD (0 or 1 expected)";
        return;
      }

   // check command name
   //
   loop(c, command_name.size())
      {
        bool error = false;
        if (c == 0)   error = error || command_name[c] != ']';
        else          error = error || !Avec::is_symbol_char(command_name[c]);
        if (error)
           {
             out << "BAD COMMAND+" << endl;
             MORE_ERROR() << " bad user command name in command ]USERCMD";
             return;
           }
      }

   // check conflicts with existing commands
   //
#define cmd_def(cmd_str, _cod, _arg, _hint) \
   if (check_name_conflict(out, cmd_str, command_name))   return;
#include "Command.def"
   if (check_redefinition(out, command_name, apl_fun, mode))
     {
       out << "    User-defined command "
           << command_name << " installed." << endl;
       return;
     }

   // check APL function name
   // Only needed when not a lambda function
   if (!is_lambda)
      {
         loop(c, apl_fun.size())
            {
               if (!Avec::is_symbol_char(apl_fun[c]))
                  {
                     out << "BAD COMMAND+" << endl;
                     MORE_ERROR() <<
                          "bad APL function name in command ]USERCMD";
                     return;
                  }
            }
      }

user_command new_user_command = { command_name, apl_fun, mode };
   Workspace::get_user_commands().push_back(new_user_command);

   out << "    User-defined command "
       << new_user_command.prefix << " installed." << endl;
}
//-----------------------------------------------------------------------------
void
Command::do_USERCMD(ostream & out, UCS_string & apl_cmd,
                    const UCS_string & line, const UCS_string & cmd,
                    UCS_string_vector & args, int uidx)
{
  if (Workspace::get_user_commands()[uidx].mode > 0)   // dyadic
     {
        apl_cmd.append_quoted(cmd);
        apl_cmd.append(UNI_ASCII_SPACE);
        loop(a, args.size())
           {
             apl_cmd.append_quoted(args[a]);
             apl_cmd.append(UNI_ASCII_SPACE);
           }
     }

   apl_cmd.append(Workspace::get_user_commands()[uidx].apl_function);
   apl_cmd.append(UNI_ASCII_SPACE);
   apl_cmd.append_quoted(line);
}
//-----------------------------------------------------------------------------
#ifdef DYNAMIC_LOG_WANTED
void
Command::log_control(const UCS_string & arg)
{
UCS_string_vector args = split_arg(arg);

   if (args.size() == 0 || arg[0] == UNI_ASCII_QUESTION)  // no arg or '?'
      {
        for (LogId l = LID_MIN; l < LID_MAX; l = LogId(l + 1))
            {
              const char * info = Log_info(l);
              Assert(info);

              const bool val = Log_status(l);
              CERR << "    " << setw(2) << right << l << ": " 
                   << (val ? "(ON)  " : "(OFF) ") << left << info << endl;
            }

        return;
      }

const LogId val = LogId(args[0].atoi());
int on_off = -1;
   if      (args.size() > 1 && args[1].starts_iwith("ON"))    on_off = 1;
   else if (args.size() > 1 && args[1].starts_iwith("OFf"))   on_off = 0;

   if (val >= LID_MIN && val <= LID_MAX)
      {
        const char * info = Log_info(val);
        Assert(info);
        bool new_status = !Log_status(val);   // toggle
        if (on_off == 0)        new_status = false;
        else if (on_off == 1)   new_status = true;

        Log_control(val, new_status);
        CERR << "    Log facility '" << info << "' is now "
             << (new_status ? "ON " : "OFF") << endl;
      }
}
#endif
//-----------------------------------------------------------------------------
void
Command::transfer_context::process_record(const UTF8 * record,
                                          const UCS_string_vector & objects)
{
const char rec_type = record[0];   // '*', ' ', or 'X'
const char sub_type = record[1];

   if (rec_type == '*')   // comment or similar
      {
        Log(LOG_command_IN)
           {
             const char * stype = " *** bad sub-record of *";
             switch(sub_type)
                {
                  case ' ': stype = " comment";     break;
                  case '(': {
                              stype = " timestamp";
                              YMDhmsu t(now());   // fallback if sscanf() != 7
                              if (7 == sscanf(charP(record + 1),
                                              "(%d %d %d %d %d %d %d)",
                                              &t.year, &t.month, &t.day,
                                              &t.hour, &t.minute, &t.second,
                                              &t.micro))
                                  {
                                    timestamp = t.get();
                                  }
                            }
                            break;
                  case 'I': stype = " imbed";       break;
                }

             CERR << "record #" << setw(3) << recnum << ": '" << rec_type
                  << "'" << stype << endl;
           }
      }
   else if (rec_type == ' ' || rec_type == 'X')   // object
      {
        if (new_record)
           {
             Log(LOG_command_IN)
                {
                  const char * stype = " *** bad sub-record of X";

//                          " -------------------------------------";
                  switch(sub_type)
                     {
                       case 'A': stype = " 2 ⎕TF array ";           break;
                       case 'C': stype = " 1 ⎕TF char array ";      break;
                       case 'F': stype = " 2 ⎕TF function ";        break;
                       case 'N': stype = " 1 ⎕TF numeric array ";   break;
                     }

                  CERR << "record #" << setw(3) << recnum
                       << ": " << stype << endl;
                }

             item_type = sub_type;
           }

        add(record + 1, 71);

        new_record = (rec_type == 'X');   // 'X' marks the final record
        if (new_record)
           {
             if      (item_type == 'A')   array_2TF(objects);
             else if (item_type == 'C')   chars_1TF(objects);
             else if (item_type == 'N')   numeric_1TF(objects);
             else if (item_type == 'F')   function_2TF(objects);
             else                         CERR << "????: " << data << endl;
             data.clear();
           }
      }
   else
      {
        CERR << "record #" << setw(3) << recnum << ": '" << rec_type << "'"
             << "*** bad record type '" << rec_type << endl;
      }
}
//-----------------------------------------------------------------------------
uint32_t
Command::transfer_context::get_nrs(UCS_string & name, Shape & shape) const
{
int idx = 1;

   // data + 1 is: NAME RK SHAPE RAVEL...
   //
   while (idx < data.size() && data[idx] != UNI_ASCII_SPACE)
         name.append(data[idx++]);
   ++idx;   // skip space after the name

int rank = 0;
   while (idx < data.size() &&
          data[idx] >= UNI_ASCII_0 &&
          data[idx] <= UNI_ASCII_9)
      {
        rank *= 10;
        rank += data[idx++] - UNI_ASCII_0;
      }
   ++idx;   // skip space after the rank

   loop (r, rank)
      {
        ShapeItem s = 0;
        while (idx < data.size() &&
               data[idx] >= UNI_ASCII_0 &&
               data[idx] <= UNI_ASCII_9)
           {
             s *= 10;
             s += data[idx++] - UNI_ASCII_0;
           }
        shape.add_shape_item(s);
        ++idx;   // skip space after shape[r]
      }
  
   return idx;
}
//-----------------------------------------------------------------------------
void
Command::transfer_context::numeric_1TF(const UCS_string_vector & objects) const
{
UCS_string var_name;
Shape shape;
int idx = get_nrs(var_name, shape);

   if (objects.size() && !objects.contains(var_name))   return;

Symbol * sym = 0;
   if (Avec::is_quad(var_name[0]))   // system variable.
      {
        int len = 0;
        const Token t = Workspace::get_quad(var_name, len);
        if (t.get_ValueType() == TV_SYM)   sym = t.get_sym_ptr();
        else                               Assert(0 && "Bad system variable");
      }
   else                            // user defined variable
      {
        sym = Workspace::lookup_symbol(var_name);
        Assert(sym);
      }
   
   Log(LOG_command_IN)
      {
        CERR << endl << var_name << " rank " << shape.get_rank() << " IS '";
        loop(j, data.size() - idx)   CERR << data[idx + j];
        CERR << "'" << endl;
      }

Token_string tos;
   {
     UCS_string data1(data, idx, data.size() - idx);
     Tokenizer tokenizer(PM_EXECUTE, LOC, false);
     if (tokenizer.tokenize(data1, tos) != E_NO_ERROR)   return;
   }
 
   if (tos.size() != size_t(shape.get_volume()))   return;

Value_P val(shape, LOC);
   new (&val->get_ravel(0)) IntCell(0);   // prototype

const ShapeItem ec = val->element_count();
   loop(e, ec)
      {
        const TokenTag tag = tos[e].get_tag();
        Cell * C = val->next_ravel();
        if      (tag == TOK_INTEGER)  new (C) IntCell(tos[e].get_int_val());
        else if (tag == TOK_REAL)     new (C) FloatCell(tos[e].get_flt_val());
        else if (tag == TOK_COMPLEX)  new (C)
                                          ComplexCell(tos[e].get_cpx_real(),
                                                      tos[e].get_cpx_imag());
        else FIXME;
      }

   val->check_value(LOC);

   Assert(sym);
   sym->assign(val, false, LOC);
}
//-----------------------------------------------------------------------------
void
Command::transfer_context::chars_1TF(const UCS_string_vector & objects) const
{
UCS_string var_name;
Shape shape;
int idx = get_nrs(var_name, shape);

   if (objects.size() && !objects.contains(var_name))   return;

Symbol * sym = 0;
   if (Avec::is_quad(var_name[0]))   // system variable.
      {
        int len = 0;
        const Token t = Workspace::get_quad(var_name, len);
        if (t.get_ValueType() == TV_SYM)   sym = t.get_sym_ptr();
        else                               Assert(0 && "Bad system variable");
      }
   else                            // user defined variable
      {
        sym = Workspace::lookup_symbol(var_name);
        Assert(sym);
      }
   
   Log(LOG_command_IN)
      {
        CERR << endl << var_name << " shape " << shape << " IS: '";
        loop(j, data.size() - idx)   CERR << data[idx + j];
        CERR << "'" << endl;
      }

Value_P val(shape, LOC);
const ShapeItem ec = val->element_count();
   new (&val->get_ravel(0)) CharCell(UNI_ASCII_SPACE);   // prototype

ShapeItem padded = 0;
   loop(e, ec)
      {
        Unicode uni = UNI_ASCII_SPACE;
        if (e < (data.size() - idx))   uni = data[e + idx];
        else                           ++padded;
         new (&val->get_ravel(e)) CharCell(uni);
      }

   if (padded)
      {
        CERR << "WARNING: ATF Record for " << var_name << " is broken ("
             << padded << " spaces added)" << endl;
      }

   val->check_value(LOC);

   Assert(sym);
   sym->assign(val, false, LOC);
}
//-----------------------------------------------------------------------------
void
Command::transfer_context::array_2TF(const UCS_string_vector & objects) const
{
   // an Array in 2 ⎕TF format
   //
UCS_string data1(&data[1], data.size() - 1);
UCS_string var_or_fun;

   // data1 is: VARNAME←data...
   //
   if (objects.size())
      {
        UCS_string var_name;
        loop(d, data1.size())
           {
             const Unicode uni = data1[d];
             if (uni == UNI_LEFT_ARROW)   break;
             var_name.append(uni);
           }

        if (!objects.contains(var_name))   return;
      }

   var_or_fun = Quad_TF::tf2_inv(data1);

   if (var_or_fun.size() == 0)
      {
        CERR << "ERROR: inverse 2 ⎕TF failed for '" << data1 << ";" << endl;
      }
}
//-----------------------------------------------------------------------------
void
Command::transfer_context::function_2TF(const UCS_string_vector & objects)const
{
int idx = 1;
UCS_string fun_name;

   /// chars 1...' ' are the function name
   while ((idx < data.size()) && (data[idx] != UNI_ASCII_SPACE))
        fun_name.append(data[idx++]);
   ++idx;

   if (objects.size() && !objects.contains(fun_name))   return;

UCS_string statement;
   while (idx < data.size())   statement.append(data[idx++]);
   statement.append(UNI_ASCII_LF);

UCS_string fun_name1 = Quad_TF::tf2_inv(statement);
   if (fun_name1.size() == 0)   // tf2_inv() failed
      {
        CERR << "inverse 2 ⎕TF failed for the following APL statement: "
             << endl << "    " << statement << endl;
        return;
      }

Symbol * sym1 = Workspace::lookup_existing_symbol(fun_name1);
   Assert(sym1);
Function * fun1 = sym1->get_function();
   Assert(fun1);
   fun1->set_creation_time(timestamp);

   Log(LOG_command_IN)
      {
       const YMDhmsu ymdhmsu(timestamp);
       CERR << "FUNCTION '" << fun_name1 <<  "'" << endl
            << "   created: " << ymdhmsu.day << "." << ymdhmsu.month
            << "." << ymdhmsu.year << "  " << ymdhmsu.hour
            << ":" << ymdhmsu.minute << ":" << ymdhmsu.second
            << "." << ymdhmsu.micro << " (" << timestamp << ")" << endl;
      }
}
//-----------------------------------------------------------------------------
void
Command::transfer_context::add(const UTF8 * str, int len)
{

#if 0
   // helper to print the uni_to_cp_map when given a cp_to_uni_map.
   //
   Avec::print_inverse_IBM_quad_AV();
   DOMAIN_ERROR;
#endif

const Unicode * cp_to_uni_map = Avec::IBM_quad_AV();
   loop(l, len)
      {
        const UTF8 utf = str[l];
        switch(utf)
           {
             case '^': data.append(UNI_AND);              break;   // ~ → ∼
             case '*': data.append(UNI_STAR_OPERATOR);    break;   // * → ⋆
             case '~': data.append(UNI_TILDE_OPERATOR);   break;   // ~ → ∼
             
             default:  data.append(Unicode(cp_to_uni_map[utf]));
           }
      }
}
//-----------------------------------------------------------------------------
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
   while (s < user_arg.size() && user_arg[s] <= ' ') ++s;

   if (s == user_arg.size())   return false;   // case 1.: OK

   // copy left of - to from
   //
   while (s < user_arg.size()   &&
              user_arg[s] > ' ' &&
              user_arg[s] != '-')  from.append(user_arg[s++]);

   // skip spaces after from
   //
   while (s < user_arg.size() && user_arg[s] <= ' ') ++s;

   if (s < user_arg.size() && user_arg[s] == '-') { ++s;   got_minus = true; }

   // skip spaces before to
   //
   while (s < user_arg.size() && user_arg[s] <= ' ') ++s;

   // copy right of - to from
   //
   while (s < user_arg.size() && user_arg[s] > ' ')  to.append(user_arg[s++]);

   // skip spaces after to
   //
   while (s < user_arg.size() && user_arg[s] <= ' ') ++s;

   if (s < user_arg.size())   return true;   // error: non-blank after to

   if (!got_minus)   to = from;   // case 2.

   if (from.size() == 0 && to.size() == 0) return true;   // error: single -

   // "increment" TO so that we can compare ITEM < TO
   //
   if (to.size())   to.back() = Unicode(to.back() + 1);

   return false;   // OK
}
//-----------------------------------------------------------------------------
bool
Command::is_lib_ref(const UCS_string & lib)
{
   if (lib.size() == 1)   // single char: lib number
      {
        if (Avec::is_digit(lib[0]))   return true;
      }

   if (lib[0] == UNI_ASCII_FULLSTOP)   return true;

   loop(l, lib.size())
      {
        const Unicode uni = lib[l];
        if (uni == UNI_ASCII_SLASH)       return true;
        if (uni == UNI_ASCII_BACKSLASH)   return true;
      }

   return false;
}
//-----------------------------------------------------------------------------
