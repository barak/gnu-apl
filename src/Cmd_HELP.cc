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
    Cmd_HELP:: implements the )HELP command (+ its primitive_help() helper
    and the file-local len_diff() formatting helper).
*/

#include <string.h>

#include "Avec.hh"
#include "Cmd_HELP.hh"
#include "Command.hh"
#include "Common.hh"
#include "NativeFunction.hh"
#include "Symbol.hh"
#include "UserFunction.hh"
#include "Value.hh"
#include "Workspace.hh"

//────────────────────────────────────────────────────────────────────────────
/// return the length differece between a UCS_string and its UTF8 encoding
static inline int
len_diff(const char * txt)
{
int ret = 0;
   while (const char cc = *txt++)   if ((cc & 0xC0) == 0x80)   ++ret;
   return ret;
}
//════════════════════════════════════════════════════════════════════════════
void
Cmd_HELP::cmd_HELP(ostream & out, const UCS_string & _arg)
{
   // map alternate APL characters to standard ones
   //
UCS_string arg;
   loop(a, _arg.size())   arg << Avec::make_standard(_arg[a]);

   if (arg.size() > 0 && Avec::is_first_symbol_char(arg.front()))
      {
        // help for a user defined name
        //
        CERR << "symbol '" << arg << "' ";
        Symbol * sym = Workspace::lookup_existing_symbol(arg);
        if (sym == 0)
           {
             CERR << "does not exist in the current workspace" << endl;
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

        switch(vs->get_NC())
           {
             case NC_INVALID:
                  CERR << "has no valid name class" << endl;
                  return;

             case NC_UNUSED_USER_NAME:
                  CERR << "is an unused name" << endl;
                  return;

             case NC_LABEL:
                  CERR << "is a label (line " << vs->get_label()
                       << ")" << endl;
                  return;

             case NC_VARIABLE:
                  {
                    CERR << "is a variable:" << endl;
                    if (Value_P val = sym->get_apl_value())
                       val->print_properties(CERR, 4, true);
                  }
                  CERR << endl;
                  return;

             case NC_FUNCTION:
                  {
                    cFunction_P fun = sym->get_function();
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

                    const UserFunction * ufun = fun->get_func_ufun();
                    Assert(ufun);
                    ufun->help(CERR);
                  }
                  return;

             case NC_OPERATOR:
                  {
                    cFunction_P fun = sym->get_function();
                    Assert(fun);
                    CERR << "is a ";
                    if (fun->get_oper_valence() == 2)   CERR << "dyadic";
                    else                                CERR << "monadic";
                    CERR << " defined operator:" << endl;

                    const UserFunction * ufun = fun->get_func_ufun();
                    Assert(ufun);
                    ufun->help(CERR);
                  }
                  return;

             case NC_SYSTEM_VAR:
                  CERR << "is a shared variable" << endl;
                  return;

             default:
                  FIXME;
           }

        return;
      }

   {
     bool prim = arg.size() == 1;   // standard (1-character) APL primitive
     if (arg.size() == 2)
        prim = arg.front() == UNI_DOWN_TACK ||
              (arg.front() == UNI_COMMENT && arg[1] == UNI_COMMENT);

     if (prim)
        {
          UTF8_string arg_utf(arg);
          const char * arg_cp = arg_utf.c_str();
          bool first = true;

#define help_def(ar, prim, name, title, descr)              \
   primitive_help(out, arg_cp, ar, prim, name, title, descr, first);
#include "Help.def"

         return;
        }
   }

   enum { COL2 = 40 };   ///< where the second column starts

UCS_string_vector commands;
   commands.reserve(60);

   out << left << "APL Commands:" << endl;
#define cmd_def(cmd_str, _code, arg, _hint) \
   { UCS_string c(UTF8_string(cmd_str " " arg));   commands.push_back(c); }
#include "Command.def"

bool left_col = true;
   loop(c, commands.size())
      {
        const UCS_string & cmd = commands[c];
        if (left_col && cmd.size() >= size_t(COL2 - 2))
           {
              // too wide for the left column (it was glued to the next
              // command, Bugs34 #46): on a line of its own
              out << "      " << cmd << endl;
           }
        else if (left_col)
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
       out << endl << endl << "User defined commands:" << endl;
       for (size_t u = Workspace::get_user_commands().size(); u; )
           {
             const Command::user_command & ucmd = Workspace::get_user_commands()[--u];
             out << "      " << ucmd.prefix << " [args]  calls:  ";
             if (ucmd.mode)   out << "tokenized-args ";

             out << ucmd.apl_function << " (quoted-args)" << endl;
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
#define rw_sv_def(x, _str, txt)                                            \
   { const UCS_string & ucs = Workspace::get_v_ ## x().get_name();         \
     if (left_col)   out << "      " << setw(8) << ucs << setw(30) << txt; \
     else            out << setw(8) << ucs << txt << endl;                 \
        left_col = !left_col; }
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
   if (!left_col)   out << endl;   // last function was in left column: close line

   out << reset_format;   // undo the left set above; out is caller-owned
}
//────────────────────────────────────────────────────────────────────────────
/// print help text \b text via UCS_string::wrapped_lines(), i.e. filled/
/// word-wrapped to the current ⎕PW (not a fixed column count -- terminals
/// vary in exactly where they wrap) and indented by \b indent spaces on
/// every line. See wrapped_lines() for how an author-provided '\n' (as
/// Help.def routinely uses to keep a distinct clause or sentence on its
/// own line) is handled. Previously, Help.def entries with an embedded
/// '\n' rendered with only the first line indented (every subsequent
/// line flush left) and relied on hand-picked break points sized for a
/// fixed assumed width rather than ⎕PW.
static void
print_wrapped(ostream & out, const char * text, int indent)
{
const UCS_string_vector lines =
      UCS_string(UTF8_string(text)).wrapped_lines(indent, Workspace::get_PW());

   loop(l, lines.size())   out << lines[l] << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_HELP::primitive_help(ostream & out, const char * arg, int arity,
                        const char * prim,  const char * name,
                        const char * brief, const char * descr,
                        bool & first)
{
   if (strcmp(arg, prim))   return;

   // a primitive with several forms (e.g. ⍳'s monadic "Index generator"
   // and dyadic "Index of B in A") has one Help.def entry per form, all
   // matching the same arg; separate them with a blank line so that
   // )HELP/]HELP output doesn't run different forms together.
   //
   if (!first)   out << endl;
   first = false;

   if (arity == -6)
      {
        out << "   " << name << ":   " << brief << endl;
        print_wrapped(out, descr, 4);
        return;
      }

   out << "   " << name << " is a ";
   switch(arity)
      {
        case -5: out << "quasi-dyadic operator:\n"
                        "   Z ← A (∘ . G) B";               break;
        case -4: out << "dyadic primitive operator:\n"
                        "   Z ← A (F " << prim << " G) B";  break;
        case -3: out << "dyadic primitive operator:\n"
                        "   Z ← (F " << prim << " G) B";    break;
        case -2: out << "monadic primitive operator:\n"
                        "   Z ← A (F " << prim << ") B";     break;
        case -1: out << "monadic primitive operator:\n"
                        "   Z ← (F " << prim << ") B";       break;
        case  0: out << "niladic primitive function:\n"
                        "   Z ← " << prim;                    break;
        case  1: out << "monadic primitive function:\n"
                        "   Z ← " << prim << " B";            break;
        case  2: out << "dyadic primitive function:\n"
                        "   Z ← A " << prim << " B";         break;
        case  3: out << "monadic primitive function (with axis):\n"
                        "   Z ← " << prim << "[X] B";      break;
        case  4: out << "dyadic primitive function (with axis):\n"
                        "   Z ← A " << prim << "[X] B";      break;

        default: FIXME;   // error in Help.def
      }

   if (*name)   out << "  ("  << name  <<  ")";
   out << endl;
   if (*brief)   print_wrapped(out, brief, 4);

   if (*descr)   print_wrapped(out, descr, 4);
}
//════════════════════════════════════════════════════════════════════════════
