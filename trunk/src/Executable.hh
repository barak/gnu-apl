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

#ifndef __EXECUTABLE_HH_DEFINED__
#define __EXECUTABLE_HH_DEFINED__

#include "Avec.hh"
#include "Token.hh"
#include "Token_string.hh"
#include "UCS_string_vector.hh"

class Error;
class UCS_string;
class UserFunction;

//────────────────────────────────────────────────────────────────────────────
/**
     A sequence of APL token. An executable is created for one of 3 purposes:
     - an APL expression for execute (⍎), or
     - the statements of one line in immediate execution, or
     - all lines of a user defined function.
 **/
/// Base class for ExecuteList, StatementList, and UserFunction
class Executable
{
public:
   /// constructor
   /// @param ucs source text of the executable
   /// @param multi_line true if the source spans multiple lines
   /// @param pm parse mode (execute, statement list, or function)
   /// @param loc caller location for diagnostics
   Executable(const UCS_string & ucs, bool multi_line,
              ParseMode pm, const char * loc);

   /// constructor for lambdas
   /// @param sig function signature of the lambda
   /// @param lambda_num sequential number identifying this lambda
   /// @param lambda_text source text of the lambda body
   /// @param loc caller location for diagnostics
   Executable(Fun_signature sig, int lambda_num,
              const UCS_string & lambda_text, const char * loc);

   /// destructor: release values held by the body
   virtual ~Executable();

   /// return true iff this Executable cannot be suspended
   virtual bool cannot_suspend() const
      { return false; }

   /// return the body of this executable
   const Token_string & get_body() const
      { return body; }

   /// return a UserFunction * (if \b this is one) or else 0.
   virtual const UserFunction * get_exec_ufun() const
      { return 0; }

   /// get the line number for pc
   /// @param pc program counter value to map to a line number
   virtual Function_Line get_line(Function_PC pc) const
      { return Function_Line_0; }

   /// say where this SI entry was allocated
   const char * get_loc() const
      { return alloc_loc; }

   /// the parse mode of \b this Executable
   ParseMode get_parse_mode() const
      { return pmode; }

   /// return the refcount
   int get_refcount() const
      { return refcount; }

   /// return the result symbol iff this is a user defined function or
   /// operator that returns a value
   virtual Symbol * get_sym_Z() const
      { return 0; }

   /// return function line n (0 = header line)
   /// @param l line number (0 = header)
   const UCS_string & get_text(int l) const
      { return text[l]; }

   /// return number of function lines (including header line 0)
   const int get_text_size() const
      { return text.size(); }

   /// return the first PC of the \b line
   /// @param line function line number whose start PC is requested
   virtual Function_PC line_start(Function_Line line) const
      { return Function_PC_0; }

   /// print this user defined executable to \b out
   /// @param out output stream to write to
   virtual ostream & print(ostream & out) const
      { print_token(out, false);   return out; }

   /// return true if this Executable localizes Symbol \b sym
   /// @param sym symbol to check for localisation
   virtual bool pushes_sym(const Symbol * sym) const
      { return false; }

   /// clear the text of line n (0 = header line)
   /// @param l line number to clear (0 = header)
   void clear_text(int l)
      { text[l].clear(); }

   /// return a UserFunction * (if \b this is one) or else 0.
   virtual UserFunction * get_exec_ufun()
   { return 0; }

   /// set the text of line n (0 = header line)
   /// @param l line number to replace (0 = header)
   /// @param new_line replacement text for line \b l
   void set_text(int l, const UCS_string & new_line)
      { text[l] = new_line; }

   /// execute the body of this executable
   Token execute_body() const;

   /// return the name of this function, or ◊ or ⍎
   virtual UCS_string get_name() const = 0;

   /// return the end of the statement (excluding) to which pc belongs
   /// @param pc program counter within the statement
   Function_PC get_statement_end(Function_PC pc) const;

   /// return the start of the statement to which pc belongs
   /// @param pc program counter within the statement
   Function_PC get_statement_start(Function_PC pc) const;

   /// print a body range (in APL order)
   /// @param out output stream to write to
   /// @param from_to PC range (start and end) to print
   ostream & print_range(ostream & out, Function_PC2 from_to) const;

   /// print the text of this user defined executable to \b out
   /// @param out output stream to write to
   void print_text(ostream & out) const;

   /// print this user defined executable to \b out
   /// @param out output stream to write to
   /// @param details non-zero to include additional token detail
   void print_token(ostream & out, int details) const;

   /// restore the original (un-optimized) tokens for a failed statement
   /// @param original token string to restore into
   /// @param low_PC first PC of the statement to restore
   void reparse(Token_string & original, Function_PC low_PC) const;

   /// compute error lines 2 and 3 in \b error
   /// @param error error object to fill with location information
   /// @param range PC range (start/end) of the failing statement
   void set_error_info(Error & error, Function_PC2 range) const;

   /// helper for set_error_info(Error & error, Function_PC2 range).
   /// @param error error object to fill with location information
   /// @param failed_statement token string of the failing statement
   /// @param range PC range (start/end) of the failing statement
   void set_error_info(Error & error, const Token_string & failed_statement,
                       Function_PC2 range) const;

   /// print all owners of \b value
   /// @param prefix string prepended to each output line
   /// @param out output stream to write to
   /// @param value APL value whose owners are to be listed
   int show_owners(const char * prefix, ostream & out,
                   const Value & value) const;

   /// return the text of statement at \b pc
   /// @param pc program counter identifying the statement
   UCS_string statement_text(Function_PC pc) const;

   /// clear marked flag in all body token
   void unmark_all_values() const;

   /// delete values in body and (maybe) remove lambdas
   void clear_body();

   /// compute the targets for if/else aka →→ ←→ and ←←.
   /// Maybe set MORE() info and return true on error.
   bool compute_if_else_targets();

   /// decrement the reference counter
   /// @param loc caller location for diagnostics
   void decrement_refcount(const char * loc);

   /// increment the reference counter
   /// @param loc caller location for diagnostics
   /// @param creator description of the entity acquiring the reference
   void increment_refcount(const char * loc, const char * creator);

   /// remove all TOK_VOID token from the body. \b UserFunction needs to
   /// overload this function, e.g. to update its jump table.
   /// Return the number of tokens removed
   virtual VoidCount remove_TOK_VOID();

protected:
   /// the body positions of the ←← and ←→ tokens of a conditional
   struct conditional
      {
        conditional(Function_PC pc)
        : if_THEN(pc),
          if_ELSE(Function_PC(-2))   // -2 means no ELSE clause
        {}

        /// print \b this conditional
        /// @param out output stream to write to
        /// @param level indentation level for nested output
        /// @param body token string of the enclosing executable
        void print(ostream & out, int level, const Token_string & body) const;

        Function_PC if_THEN;   ///< position of the ←← token (end of condition)
        Function_PC if_ELSE;   ///< position of the ←→ token (before else)
      };


   /// return true if the body contains { ... }. The parser has already
   /// complained if the curly brackets do not match, therefore we only check
   /// for } (which comes  last in APL, but first in the (reversed) body.
   inline bool body_has_curly() const
      {
        loop(b, body.size())
           {
             if (body[b].get_tag() == TOK_R_CURLY)   return true;
           }
        return false;
      }

   /// extract the skip'th { ... } from the function text
   /// @param signature function signature of the lambda
   /// @param skip zero-based index of which lambda to extract
   UCS_string extract_lambda_text(Fun_signature signature, int skip) const;

   /// body[b ... bend] is a lambda. Move these token from this body to the body
   /// of the lambda and clear them in \b this body.
   /// @param rev_lambda_body token string receiving the reversed lambda body
   /// @param b index of the opening '{' in the body
   /// @param bend index of the matching '}' in the body
   Fun_signature compute_lambda_body(Token_string & rev_lambda_body,
                                     ShapeItem b, ShapeItem bend);

   /// parse the body line number \b line of \b this function
   /// @param line function line number being parsed
   /// @param ucs source text of the line
   /// @param trace true to enable trace output during parsing
   /// @param loc caller location for diagnostics
   /// @param macro true if this line originates from a macro expansion
   ErrorCode parse_body_line(Function_Line line, const UCS_string & ucs,
                             bool trace, const char * loc, bool macro);

   /// parse the body line number \b line of \b this function
   /// @param line function line number being parsed
   /// @param tos pre-tokenised source for the line
   /// @param trace true to enable trace output during parsing
   /// @param loc caller location for diagnostics
   ErrorCode parse_body_line(Function_Line line, const Token_string & tos,
                             bool trace, const char * loc);

   /// recursively extract all lambda expressions from body and store
   /// them in lambdas
   void setup_lambdas();

   /// extract one lambda expressions from body and store it in lambdas
   /// body[b] is { and body[bend] is }.
   /// @param b index of the opening '{' in the body
   /// @param nend index of the matching '}' in the body
   /// @param lambda_num sequential number to assign to this lambda
   ShapeItem setup_one_lambda(ShapeItem b, ShapeItem nend,
                              Lambda_number lambda_num);

   /// reverse the token order of the entire Token_string (i.e. not statement
   /// by statement)
   /// @param tos token string to reverse in place
  static void reverse_all_token(Token_string & tos);

   /// reverse the token order in each statement of tos (statement by
   /// statement reversal). The order pf statements is not changed).
   /// @param tos token string whose statements are reversed in place
  static void reverse_each_statement(Token_string & tos);

   /// where this SI entry was allocated
   const char * alloc_loc;

   /// The token to be executed. They are organized line by line and
   /// statement by statement, but the token within a statement reversed
   /// due to the right-to-left execution of APL.
   Token_string body;

   /// the mode \b this Executable
   const ParseMode pmode;

   /// reference counter (for lambdas)
   int refcount;

   /// the program text from which \b body was created
   UCS_string_vector text;
};
//────────────────────────────────────────────────────────────────────────────
/**
   The token of an execute expression (⍎'...')
 **/
/// The token of an execute expression ⍎'...'
class ExecuteList : public Executable
{
   friend class XML_Loading_Archive;

public:
   /// constructor
   /// @param txt source text of the execute expression
   /// @param loc caller location for diagnostics
   ExecuteList(const UCS_string & txt, const char * loc)
   : Executable(txt, false, PM_EXECUTE, loc)
   {}

   /// compute body token from text \b data
   /// @param data APL source text of the execute expression
   /// @param loc caller location for diagnostics
   static ExecuteList * fix(const UCS_string & data, const char * loc);

protected:
   /// overloaded Executable::get_name()
   virtual UCS_string get_name() const
      { return UCS_string(UNI_EXECUTE); }
};
//────────────────────────────────────────────────────────────────────────────
/**
   The token of an statement list (cmd ◊ cmd ... ◊ cmd)
 **/
/// The token of an statement list cmd ◊ cmd ... ◊ cmd
class StatementList : public Executable
{
   friend class XML_Loading_Archive;

public:
   /// compute body token from text \b data
   /// @param data APL source text of the statement list
   /// @param suffix optional APL value appended after execution
   /// @param loc caller location for diagnostics
   static StatementList * fix(const UCS_string & data, Value_P suffix,
                              const char * loc);

protected:
   /// constructor
   /// @param txt source text of the statement list
   /// @param loc caller location for diagnostics
   StatementList(const UCS_string txt, const char * loc)
   : Executable(txt, false, PM_STATEMENT_LIST, loc)
   {}

   /// overloaded Executable::get_name()
   virtual UCS_string get_name() const
      { return UCS_string(UNI_DIAMOND); }
};
//────────────────────────────────────────────────────────────────────────────

#endif // __EXECUTABLE_HH_DEFINED__
