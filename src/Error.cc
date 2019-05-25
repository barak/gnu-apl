/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2015  Dr. Jürgen Sauermann

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

#include "Common.hh"
#include "Error.hh"
#include "Output.hh"
#include "PrintOperator.hh"
#include "Symbol.hh"
#include "StateIndicator.hh"
#include "UserFunction.hh"
#include "Workspace.hh"

//-----------------------------------------------------------------------------
Error::Error(ErrorCode ec, const char * loc)
   : error_code(ec),
     throw_loc(loc),
     parser_loc(0),
     show_locked(false),
     left_caret(-1),
     right_caret(-1),
     print_loc(0)
{
const char more = Workspace::more_error().size() ? '+' : 0;
   snprintf(error_message_1, sizeof(error_message_1), "%s%c",
            error_name(error_code), more);

   *symbol_name = 0;
   *error_message_2 = 0;
}
//-----------------------------------------------------------------------------
void
Error::print(ostream & out, const char * loc) const
{
   if (print_loc)
      {
        CERR << endl << "*** Error printed twice; first printed at "
             << print_loc << endl
             << "now printed at " << loc << endl;

        return;
      }

   Log(LOG_verbose_error)
      {
        out << endl
            << "--------------------------" << endl;

        if (error_code == E_NO_ERROR)
           {
             out << error_name(E_NO_ERROR) << endl
                 << "--------------------------" << endl;

             return;
           }

        if (*error_message_1)
           {
             out << error_message_1 << endl;
           }
        else
           {
             out << error_name(error_code);
             if (Workspace::more_error().size())   out << UNI_ASCII_PLUS;
             out << endl;
           }

        if (parser_loc)   out << "   Parser LOC: " << parser_loc  << endl;
        if (print_loc)    out << "   Print LOC:  " << print_loc   << endl;
        out                   << "   loc:        " << loc         << endl;
        loc = print_loc;

        if (*symbol_name)
           out                << "   Symbol:     " << symbol_name << endl;

        out <<                   "   Thrown at:  " << throw_loc   << endl
            <<                   "--------------------------"     << endl
                                                                  << endl;
      }
}
//-----------------------------------------------------------------------------
const char *
Error::error_name(ErrorCode err)
{
   switch(err)
      {
   /// the cases
#define err_def(c, txt, _maj, _min) \
   case E_ ## c:   return txt;

#include "Error.def"
      }

   return "Unknown Error";
}
//-----------------------------------------------------------------------------
bool
Error::is_known() const
{
   switch(error_code)
      {
   /// the cases
#define err_def(c, _txt, _maj, _min) case  E_ ## c:   return true;
#include "Error.def"
      }

   return false;
}
//-----------------------------------------------------------------------------
void
Error::set_error_line_2(const UCS_string & ucs, int lcaret, int rcaret)
{
UTF8_string utf(ucs);
   strncpy(error_message_2, utf.c_str(), sizeof(error_message_2));
   error_message_2[sizeof(error_message_2) - 1] = 0;
   left_caret = lcaret;
   right_caret = rcaret;
}
//-----------------------------------------------------------------------------
bool
Error::is_syntax_or_value_error() const
{
   if (error_major(error_code) == 2)   return true;   // some SYNTAX ERROR
   if (error_major(error_code) == 3)   return true;   // VALUE ERROR
   return false;
}
//-----------------------------------------------------------------------------
UCS_string
Error::get_error_line_3() const
{
   if (error_code == E_NO_ERROR)   return UCS_string();

   if (left_caret < 0)   return UCS_string();

UCS_string ret;
   if (left_caret > 0)   ret.append(UCS_string(left_caret, UNI_ASCII_SPACE));
   ret.append(UNI_ASCII_CIRCUMFLEX);

const int diff = right_caret - left_caret;
   if (diff <= 0)   return ret;
   if (diff > 1)   ret.append(UCS_string(diff - 1, UNI_ASCII_SPACE));
   ret.append(UNI_ASCII_CIRCUMFLEX);

   return ret;
}
//-----------------------------------------------------------------------------
void
Error::print_em(ostream & out, const char * loc)
{
   if (print_loc)
      {
        CERR << endl << "*** Error printed twice; first printed at "
             << print_loc << endl
             << "now printed from " << loc << endl;

        return;
      }

   print_loc = loc;
   if (*get_error_line_1())   out << get_error_line_1() << endl;

   out << get_error_line_2() << endl
       << get_error_line_3() << endl;
}
//-----------------------------------------------------------------------------
void
throw_apl_error(ErrorCode code, const char * loc)
{
   ADD_EVENT(0, VHE_Error, code, loc);

StateIndicator * si = Workspace::SI_top();

   Log(LOG_error_throw)
      {
        CERR << endl
             << "throwing " << Error::error_name(code)
             << " at " << loc << endl;
      }

   Log(LOG_verbose_error)
      {
        if (!(si && si->get_safe_execution()))
           Backtrace::show(__FILE__, __LINE__);
      }

   // maybe map error to DOMAIN ERROR.
   //
   if (si)
      {
        const UserFunction * ufun = si->get_executable()->get_ufun();
        if (ufun && ufun->get_exec_properties()[3])   code = E_DOMAIN_ERROR;
      }

Error error(code, loc);
   if (si)   error.update_error_info(si);

Error & eref = error;
   throw eref;
}
//-----------------------------------------------------------------------------
void
Error::throw_parse_error(ErrorCode code, const char * par_loc, const char *loc)
{
   Log(LOG_error_throw)
      CERR << endl
           << "throwing " << Error::error_name(code) << " at " << loc << endl;

   Log(LOG_verbose_error)   Backtrace::show(__FILE__, __LINE__);

   MORE_ERROR() << Error::error_name(code);

Error error(code, loc);
   error.parser_loc = par_loc;

// StateIndicator * si = Workspace::SI_top();
// if (si)   error.update_error_info(si);

Error & eref = error;
   throw eref;
}
//-----------------------------------------------------------------------------
void
Error::throw_symbol_error(const UCS_string & sym_name, const char * loc)
{
   Log(LOG_error_throw)   
      {   
        CERR << "throwing VALUE ERROR at " << loc;
        if (sym_name.size())   CERR << " (symbol is " << sym_name << ")"; 
        CERR << endl;
      }

   Log(LOG_verbose_error)     Backtrace::show(__FILE__, __LINE__);

Error error(E_VALUE_ERROR, loc);
UTF8_string sym_name_utf(sym_name);
   snprintf(error.symbol_name, sizeof(error.symbol_name), "%s",
            sym_name_utf.c_str());
   if (StateIndicator * si = Workspace::SI_top())   error.update_error_info(si);

Error & eref = error;
   throw eref;
}
//-----------------------------------------------------------------------------
void
Error::throw_define_error(const UCS_string & fun_name, const UCS_string & cmd,
                          const char * loc)
{
   Log(LOG_error_throw)   
      {   
        CERR << "throwing DEFN ERROR at " << loc
             << " (function is " << fun_name << ")" << endl;
      }

   Log(LOG_verbose_error)     Backtrace::show(__FILE__, __LINE__);

Error error(E_DEFN_ERROR, loc);
Error & eref = error;
UTF8_string fun_name_utf(fun_name);
   snprintf(error.symbol_name, sizeof(error.symbol_name), "%s",
            fun_name_utf.c_str());

UTF8_string cmd_utf(cmd);   // cmd is something like ∇FUN[⎕]∇
   snprintf(error.error_message_2, sizeof(error.error_message_2),
            " aaa  %s", cmd_utf.c_str());
   eref.left_caret = 5 + cmd.size();
   if (Workspace::SI_top())   *Workspace::get_error() = eref;
   throw eref;
}
//-----------------------------------------------------------------------------
void
Error::update_error_info(StateIndicator * si)
{
bool locked = false;
const UserFunction * ufun = si->get_executable()->get_ufun();

   // prepare second error line (failed statement)
   //
   if (ufun)
      {
        if (get_show_locked() || ufun->get_exec_properties()[1])
           {
             locked = true;
             set_error_line_2("      ");
             set_left_caret(6);
           }
        else
           {
             UCS_string ucs(ufun->get_name_and_line(si->get_PC()));
             ucs.append(UNI_ASCII_SPACE);
             ucs.append(UNI_ASCII_SPACE);
             UTF8_string utf(ucs);
             set_error_line_2(utf.c_str());
             set_left_caret(ucs.size());
           }
      }
   else
      {
        set_error_line_2("      ");
        set_left_caret(6);
      }

   // prepare third line (carets)
   //
   set_right_caret(-1);

   if (locked)
      {
        si->get_executable()->get_ufun()->set_locked_error_info(*this);
      }
   else
      {
        const Function_PC from = si->get_prefix().get_range_low();
        Function_PC to   = si->get_prefix().get_range_high();
        const Function_PC2 error_range(from, to);
        si->get_executable()->set_error_info(*this, error_range);
      }

   // print error, unless we are in safe execution mode.
   //
   {
     bool print_error = true;
     for (const StateIndicator * si1 = si; si1; si1 = si1->get_parent())
         {
           if (si1->get_safe_execution())
              {
                print_error = false;
                break;
              }
         }
     if (print_error)   print_em(UERR, LOC);
   }

   StateIndicator::get_error(si) = *this;
}
//-----------------------------------------------------------------------------

