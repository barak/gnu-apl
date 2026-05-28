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

#ifndef __ERROR_HH_DEFINED__
#define __ERROR_HH_DEFINED__

#include "Backtrace.hh"
#include "Common.hh"
#include "ErrorCode.hh"
#include "UCS_string.hh"
#include <cstring>

#ifdef __GNUC__
    #define GNUC__noreturn __attribute__ ((noreturn))
#else
    #define GNUC__noreturn
#endif

class StateIndicator;

/**
 ** The exception that is thrown when errors occur. 
 ** The primary item is the error_code; the other items are only valid if
 ** error_code != NO_ERROR
 **/
/// An APL error and information related to it
class Error
{
public:
   /// constructor: error with error code ec
   /// @param ec error code identifying the APL error
   /// @param loc caller location for diagnostics
   Error(ErrorCode ec, const char * loc);

   /// clear error line 1
   void clear_error_line_1()
      { *error_message_1 = 0; }

   /// set the error code
   void clear_error_code()
      { error_code = E_NO_ERROR; }

   /// return the error code of \b this error
   const ErrorCode get_error_code() const
      { return error_code; }

   /// return ⎕EM[1;]. This is the first of 3 error lines. It contains the
   /// error name (like SYNTAX ERROR, DOMAIN ERROR, etc) and is subject
   /// to translation
   UCS_string get_error_line_1() const
      { return UCS_string(UTF8_string(error_message_1)); }

   /// return error_message_2. This is the second of 3 error lines.
   /// It contains the failed statement and is NOT subject to translation.
   UCS_string get_error_line_2() const
      { return UCS_string(UTF8_string(error_message_2)); }

   /// return the left caret (^) position in the third error line
   int get_left_caret() const
      { return left_caret; }

   /// return source file location where this error was printed (0 if not)
   const char * get_print_loc() const
      { return print_loc; }

   /// return the right caret (^) position in the third error line
   int get_right_caret() const
      { return right_caret; }

   /// return the show_locked flag
   bool get_show_locked() const
      { return show_locked; }

   /// return the source code location where the error was thrown
   const char * get_throw_loc() const
      { return throw_loc; }

   /// set the left caret (^) position in the third error line
   /// @param col column index for the left caret
   void set_left_caret(int col)
      { left_caret = col; }

   /// set the source code line where a parse error was thrown
   /// @param loc caller location for diagnostics
   void set_parser_loc(const char * loc)
      { parser_loc = loc; }

   /// set the right caret (^) position in the third error line
   /// @param col column index for the right caret
   void set_right_caret(int col)
      { right_caret = col; }

   /// set the show_locked flag
   /// @param on new value for show_locked
   void set_show_locked(bool on)
      { show_locked = on; }

   /// return the major class (⎕ET) of the error
   /// @param err error code to classify
   static int error_major(ErrorCode err)
      { return err >> 16; }

   /// return the category (⎕ET) of the error
   /// @param err error code to classify
   static int error_minor(ErrorCode err)
      { return err & 0xFFFF; }

   /// return true iff error_code is known (as opposed to an arbitrary
   /// error code constructed by ⎕ES)
   bool is_known() const;

   /// return true iff error_code is some SYNTAX ERROR or VALUE ERROR
   bool is_syntax_or_value_error() const;

   /// add a '+' to error_message_1 unless it hast one already. The caller
   /// has checked that
   /// @param have_more true if there is more context available
   void add_MORE_indicator(bool have_more);

   /// compute the caret line. This is the third of the 3 error lines.
   /// It contains the failure position the statement and is NOT subject
   /// to translation.
   UCS_string get_error_line_3() const;

   /// print the 3 error message lines as per ⎕EM
   /// @param out output stream to print to
   /// @param loc caller location for diagnostics
   void print_em(ostream & out, const char * loc);

   /// print the error and its related information
   /// @param out output stream to print to
   /// @param loc caller location for diagnostics
   void print(ostream & out, const char * loc) const;

   /// set the first error line
   /// @param msg_1 error name text for ⎕EM[1;]
   void set_error_line_1(const char * msg_1);

   /// set the second error line to \b msg_2
   /// @param msg_2 failed-statement text for ⎕EM[2;]
   void set_error_line_2(const char * msg_2);

   /// set error line 2, left caret, and right caret
   /// @param ucs failed-statement text
   /// @param lcaret left caret column position
   /// @param rcaret right caret column position
   void set_error_line_2(const UCS_string & ucs, int lcaret, int rcaret);

   /// update error information in \b this and copy it to si
   /// @param si state indicator entry to update with error info
   void update_error_info(StateIndicator * si);

   /// return a string describing the error
   /// @param err error code to look up
   static const char * error_name(ErrorCode err);

   /// throw a DEFN ERROR for function \b fun
   /// @param fun function name that caused the DEFN ERROR
   /// @param cmd command text associated with the definition attempt
   /// @param loc caller location for diagnostics
   static void throw_define_error(const UCS_string & fun,
                                  const UCS_string & cmd,
                                  const char * loc) GNUC__noreturn;

   /// throw a error with a parser location
   /// @param code error code to throw
   /// @param par_loc parser source location where the error was detected
   /// @param loc caller location for diagnostics
   static void throw_parse_error(ErrorCode code, const char * par_loc,
                                 const char * loc) GNUC__noreturn;

   /// throw an Error related to \b Symbol \b symbol
   /// @param symbol name of the offending symbol
   /// @param loc caller location for diagnostics
   static void throw_symbol_error(const UCS_string & symbol,
                                  const char * loc) GNUC__noreturn;

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
   char error_message_1[200];

   /// second line of error message (only valid if error_code != _NO_ERROR)
   char error_message_2[200];

   /// display user function and line rather than ⎕ES instruction
   bool show_locked;

   /// the left caret position (-1 if none) for the error line 3
   int left_caret;

   /// the right caret position (-1 if none) for the error line 3
   int right_caret;

   /// where this error was printed (0 if not yet printed)
   const char * print_loc;

private:
   /// constructor (not implemented): prevent construction without error code
   Error();
};
#endif // __ERROR_HH_DEFINED__
