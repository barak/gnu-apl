/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2019  Dr. Jürgen Sauermann

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

#ifndef __ERROR_HH_DEFINED__
#define __ERROR_HH_DEFINED__

#include "Backtrace.hh"
#include "Common.hh"
#include "ErrorCode.hh"
#include "UCS_string.hh"

/// throw a error with a parser location
void throw_parse_error(ErrorCode code, const char * par_loc,
                       const char * loc)
#ifdef __GNUC__
    __attribute__ ((noreturn))
#endif
;

/// throw an Error related to \b Symbol \b symbol
void throw_symbol_error(const UCS_string & symb, const char * loc)
#ifdef __GNUC__
    __attribute__ ((noreturn))
#endif
;

/// throw a define error for function \b fun
void throw_define_error(const UCS_string & fun, const UCS_string & cmd,
                        const char * loc)
#ifdef __GNUC__
    __attribute__ ((noreturn))
#endif
;

/**
 ** The exception that is thrown when errors occur. 
 ** The primary item is the error_code; the other items are only valid if
 ** error_code != NO_ERROR
 **/
/// An APL error and information related to it
class Error
{
   // TODO: the friends below exist for historical reasons and should use
   // access functions instead of accessing data members of class Error.
   // The throw_xxx() friends could be made static members of class Error.
   // That will be quite some work, though.
   //
   friend class Command;
   friend class Doxy;
   friend class Executable;
   friend class ExecuteList;
   friend class Prefix;
   friend class Quad_EM;
   friend class Quad_ES;
   friend class Quad_ET;
   friend class Quad_SI;
   friend class StateIndicator;
   friend class StatementList;
   friend class Tokenizer;
   friend class UserFunction;
   friend class Workspace;

   friend void  throw_apl_error(ErrorCode, const char *);
   friend void  throw_parse_error(ErrorCode, const char *, const char *);
   friend void  throw_symbol_error(const UCS_string &, const char *);

public:
   /// constructor: error with error code ec
   Error(ErrorCode ec, const char * loc);

   /// return true iff error_code is known (as opposed to an arbitrary
   /// error code constructed by ⎕ES)
   bool is_known() const;

   /// return true iff error_code is some SYNTAX ERROR or VALUE ERROR
   bool is_syntax_or_value_error() const;

   /// set error line 2 to ucs
   void set_error_line_2(const UCS_string & ucs, int lcaret, int rcaret);

   /// return ⎕EM[1;]. This is the first of 3 error lines. It contains the
   /// error name (like SYNTAX ERROR, DOMAIN ERROR, etc) and is subject
   /// to translation
   const char * get_error_line_1() const
      { return error_message_1; }

   /// clear error line 1
   void clear_error_line_1()
      { *error_message_1 = 0; }

   /// return error_message_2. This is the second of 3 error lines.
   /// It contains the failed statement and is NOT subject to translation.
   const char * get_error_line_2() const
      { return error_message_2; }

   /// return the major class (⎕ET) of the error
   static int error_major(ErrorCode err)
      { return err >> 16; }

   /// return the category (⎕ET) of the error
   static int error_minor(ErrorCode err)
      { return err & 0x00FF; }

   /// return source file location where this error was printed (0 if not)
   const char * get_print_loc() const
      { return print_loc; }

   /// compute the caret line. This is the third of the 3 error lines.
   /// It contains the failure position the statement and is NOT subject
   /// to translation.
   UCS_string get_error_line_3() const;

   /// print the error and its related information
   void print(ostream & out, const char * loc) const;

   /// return a string describing the error
   static const char * error_name(ErrorCode err);

   /// print the 3 error message lines as per ⎕EM
   void print_em(ostream & out, const char * loc);

   /// throw a DEFN ERROR
   static void throw_define_error(const UCS_string & fun,
                                  const UCS_string & cmd, const char * loc);

protected:
   /// the error code
   ErrorCode error_code;

   /// the mandatory source file location where the error was thrown
   const char * throw_loc;

   /// an optional symbol related to (i.e. triggering) the error
   char symbol_name[60];

   /// an optional source file location (for parse errors)
   const char * parser_loc;

   /// an optional error text. this text is provided for non-APL errors,
   // for example in ⎕ES
   char error_message_1[40];

   /// second line of error message (only valid if error_code != _NO_ERROR)
   char error_message_2[60];

   /// display user function and line rather than ⎕ES instruction
   bool show_locked;

   /// the left caret position (-1 if none) for the error display
   int left_caret;

   /// the right caret position (-1 if none) for the error display
   int right_caret;

   /// where this error was printed (0 if not)
   const char * print_loc;

private:
   /// constructor (not implemented): prevent construction without error code
   Error();
};
#endif // __ERROR_HH_DEFINED__
