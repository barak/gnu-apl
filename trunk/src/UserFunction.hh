/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

#ifndef __USERFUNCTION_HH_DEFINED__
#define __USERFUNCTION_HH_DEFINED__

#include <sys/types.h>

#include "Error.hh"
#include "Executable.hh"
#include "Function.hh"
#include "Symbol.hh"
#include "UserFunction_header.hh"
#include "UTF8_string.hh"

//----------------------------------------------------------------------------
/// One user-defined function
class UserFunction : public Function, public Executable
{
public:
   /// constructor for a lambda
   /// @param sig function signature (valence and result flags)
   /// @param lambda_num unique lambda identifier
   /// @param text source text of the lambda expression
   /// @param body pre-parsed token string for the lambda body
   /// @param lvars list of symbols localised by the lambda
   UserFunction(Fun_signature sig, Lambda_number lambda_num,
                const UCS_string & text, const Token_string & body,
                const vector<Symbol *> & lvars);

   /// Destructor.
   ~UserFunction();

   /// overloaded Function::is_lambda()
   virtual bool is_lambda() const
      { return header.get_name()[0] == UNI_LAMBDA; }

   /// overloaded Function::is_defined()
   virtual bool is_defined() const
      { return true; }

   /// Overloaded \b Function::is_operator.
   virtual bool is_operator() const
      { return header.is_operator(); }

   /// return the macro number (if this function is one) or -1
   virtual int get_macnum() const
      { return -1; }

   /// overloaded Executable::get_exec_ufun()
   virtual const UserFunction * get_exec_ufun() const
      { return this; }

   /// overloaded Function::get_func_ufun()
   virtual const UserFunction * get_func_ufun() const
   { return this; }

   /// overloaded Executable::get_exec_ufun()
   virtual UserFunction * get_exec_ufun()
   { return this; }

   /// overloaded Function::get_fun_valence()
   virtual int get_fun_valence() const
      { return header.get_fun_valence(); }

   /// overloaded Function::get_oper_valence()
   virtual int get_oper_valence() const
      { return header.get_oper_valence(); }

   /// overloaded Function::has_result()
   virtual bool has_result() const
      { return header.has_result(); }

   /// overloaded Function::has_axis()
   virtual bool has_axis() const
      { return header.has_axis(); }

   /// return \b true if this function localizes \b sym
   /// @param sym symbol to test for localisation
   bool localizes(const Symbol * sym) const
      { return header.localizes(sym); }

   /// pop all
   void pop_local_vars() const
      { header.pop_local_vars(); }

   /// print the local variables for command )SINL
   /// @param out output stream to write the variable list to
   void print_local_vars(ostream & out) const
      { return header.print_local_vars(out); }

   /// return the number of local variables
   ShapeItem local_var_count() const
      { return header.local_var_count(); }

   /// return the idx'th local variable
   /// @param idx zero-based index of the local variable to retrieve
   const Symbol * get_local_var(ShapeItem idx) const
      { return header.get_local_var(idx); }

   /// add a label
   /// @param sym symbol used as the label name
   /// @param line function line number the label refers to
   void add_label(Symbol * sym, Function_Line line)
      { header.add_label(sym, line); }

   /// return the execution properties (dyadic ⎕FX) of this defined function
   virtual const int * get_exec_properties() const
      { return exec_properties; } 

   /// ovewrloaded Executable::cannot_suspend
   virtual bool cannot_suspend() const
      { return exec_properties[1] != 0; }

   /// set execution properties (as per A ⎕FX)
   /// @param props array of four execution property integers
   void set_exec_properties(const int * props)
      { loop(i, 4)   exec_properties[i] = props[i]; }

   /// overloaded Executable::print()
   virtual ostream & print(ostream & out) const;

   /// return the name of \b this function
   virtual UCS_string get_name() const
      { return header.get_name(); }

   /// return \b true if \b symbol is a label of \b this function
   /// @param symbol symbol to test
   bool is_label(const Symbol * symbol) const
      {
        loop(l, header.get_label_count())
            {
              if (symbol == header.get_label(l).sym)   return true;
            }

        return false;
      }

   /// return \b true if \b token is for a label of \b this function
   /// @param tok lexical token to test
   bool is_label(const Token & tok)
      { return tok.get_Class() == TC_SYMBOL && is_label(tok.get_sym_ptr()); }

   /// return \b true if \b token is for a label of \b this function
   /// @param tok lexical token to test
   bool is_label_or_value(const Token & tok)
      {
        if (tok.get_Class() == TC_VALUE)   return true;
         return is_label(tok);
      }

   /// return the line number for label \b symbol \b this function
   /// @param symbol label symbol whose line number is requested
   Function_Line get_label_line(const Symbol * symbol)
      {
        loop(l, header.get_label_count())
            {
              const labVal & label = header.get_label(l);
              if (symbol == label.sym)   return label.line;
            }

        return Function_Invalid;
      }

   /// return e.g. 'FOO[10]' for the given \b pc
   /// @param pc program counter position within the function body
   UCS_string get_name_and_line(Function_PC pc) const;

   /// Overloaded Function::print_properties()
   /// @param out output stream to write properties to
   /// @param indent indentation level for formatting
   virtual void print_properties(ostream & out, int indent) const;

   /// )SAVE this function in the workspace named \b workspace
   /// (in the file system).
   /// @param workspace filesystem name of the workspace to save to
   /// @param function name of the function to save
   void save(const char * workspace, const char * function);

   /// )LOAD this function into the workspace named \b workspace.
   /// Return a pounter to this newly created function (or 0 on error).
   /// @param workspace filesystem name of the workspace to load from
   /// @param function name of the function to load
   static UserFunction * load(const char * workspace, const char * function);

   /// Load this function into the workspace named \b workspace.
   /// @param workspace filesystem name of the workspace to load from
   /// @param function name of the function to load
   static UserFunction * do_load(const char * workspace, const char * function);

   /// overloaded Function::destroy()
   virtual void destroy();

   /// overloaded Executable::pushes_sym() const
   /// @param sym symbol to test for localisation in the function header
   virtual bool pushes_sym(const Symbol * sym) const;

   /// print help for this function on out (for the )HELP command)
   /// @param out output stream to write help text to
   void help(ostream & out) const;

   /// create a user defined function according to \b data of length \b len
   /// in workspace \b w.
   /// @param text full source text of the function definition
   /// @param err_line output: line number of any parse error, or -1
   /// @param keep_existing true to silently keep any existing function of the same name
   /// @param loc caller location for diagnostics
   /// @param creator entity (editor, ⎕FX, filename) creating the function
   static UserFunction * fix(const UCS_string & text, int & err_line,
                             bool keep_existing, const char * loc,
                             const UTF8_string &  creator);

   /// (re-)create a lambda
   /// @param var symbol to which the lambda will be bound
   /// @param text source text of the lambda expression
   static UserFunction * fix_lambda(Symbol & var, const UCS_string & text);

   /// return the pc of the first token in line l (valid line), or
   /// the pc of the last token in the function (invalid line)
   Function_PC pc_for_line(Function_Line line) const;

   /// Overloaded Function::has_alpha()
   virtual bool has_alpha() const   { return true; }

   /// overloaded Executable::get_sym_Z()
   virtual Symbol * get_sym_Z() const   { return header.Z(); }

   /// overloaded Executable::get_line()
   Function_Line get_line(Function_PC pc) const;

   /// Overloaded Function::eval_()
   virtual Token eval_() const;

   /// Overloaded Function::eval_B()
   /// @param B right argument APL value
   virtual Token eval_B(Value_P B) const;

   /// Overloaded Function::eval_XB()
   /// @param X axis specification APL value
   /// @param B right argument APL value
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// Overloaded Function::eval_AB()
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// Overloaded Function::eval_AXB()
   /// @param A left argument APL value
   /// @param X axis specification APL value
   /// @param B right argument APL value
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// Overloaded Function::eval_LB.
   /// @param LO left operand token
   /// @param B right argument APL value
   virtual Token eval_LB(Token & LO, Value_P B) const;

   /// Overloaded Function::eval_LXB()
   /// @param LO left operand token
   /// @param X axis specification APL value
   /// @param B right argument APL value
   virtual Token eval_LXB(Token & LO, Value_P X, Value_P B) const;

   /// Overloaded Function::eval_ALB.
   /// @param A left argument APL value
   /// @param LO left operand token
   /// @param B right argument APL value
   virtual Token eval_ALB(Value_P A, Token & LO, Value_P B) const;

   /// Overloaded Function::eval_ALXB()
   /// @param A left argument APL value
   /// @param LO left operand token
   /// @param X axis specification APL value
   /// @param B right argument APL value
   virtual Token eval_ALXB(Value_P A, Token & LO, Value_P X, Value_P B) const;

   /// Overloaded Function::eval_LRB()
   /// @param LO left operand token
   /// @param RO right operand token
   /// @param B right argument APL value
   virtual Token eval_LRB(Token & LO, Token & RO, Value_P B) const;

   /// Overloaded Function::eval_LRXB()
   /// @param LO left operand token
   /// @param RO right operand token
   /// @param X axis specification APL value
   /// @param B right argument APL value
   virtual Token eval_LRXB(Token & LO, Token & RO, Value_P X, Value_P B) const;

   /// Overloaded Function::eval_ALRB()
   /// @param A left argument APL value
   /// @param LO left operand token
   /// @param RO right operand token
   /// @param B right argument APL value
   virtual Token eval_ALRB(Value_P A, Token & LO, Token & RO, Value_P B) const;

   /// Overloaded Function::eval_ALRXB()
   /// @param A left argument APL value
   /// @param LO left operand token
   /// @param RO right operand token
   /// @param X axis specification APL value
   /// @param B right argument APL value
   virtual Token eval_ALRXB(Value_P A, Token & LO, Token & RO, Value_P X,
                            Value_P B) const;

   /// Quad_CR of this function
   /// @param with_lines true to include line numbers in the output
   virtual UCS_string canonical(bool with_lines) const;

   /// overloaded Executable::line_start()
   virtual Function_PC line_start(Function_Line line) const;

   /// overloaded Executable::remove_TOK_VOID()
   virtual VoidCount remove_TOK_VOID();

   /// compute lines 2 and 3 in \b error
   /// @param error error object to populate with function context
   void set_locked_error_info(Error & error) const;

   /// return the entity that has created the function
   const UTF8_string get_creator() const
      { return creator; }

   /// set trace or stop vector
   /// @param lines function line numbers to set as trace or stop points
   /// @param stop true to set stop lines, false to set trace lines
   void set_trace_stop(const std::vector<Function_Line> & lines, bool stop);

   /// recompile the body
   /// @param loc caller location for diagnostics
   /// @param macro true if re-parsing macro code
   void parse_body(const char * loc, bool macro);

   /// return stop lines (from S∆fun ← lines)
   const std::vector<Function_Line> & get_stop_lines() const
      { return stop_lines; }

   /// return trace lines (from S∆fun ← lines)
   const std::vector<Function_Line> & get_trace_lines() const
      { return trace_lines; }

   /// return the header object (return value name, argument names, local vars,
   /// and function name) for this function
   const UserFunction_header & get_header() const
      { return header; }

   // debug function: print the PC for every linr
   /// @param loc caller location for diagnostics
   void print_line_PCs(const char * loc) const;

   /// resolve labels in the function body. Return \b true if any
   /// labels were resolved.
   bool optimize_labels();

   bool optimize_labels2();

   /// optimize vectors of labels
   bool optimize_label_vectors();

protected:
   /// constructor for a normal (i.e. non-lambda) user defined function
   /// @param txt full source text of the function
   /// @param loc caller location for diagnostics
   /// @param _creator entity (editor, ⎕FX, filename) that created the function
   /// @param macro true if the function is a system macro
   UserFunction(const UCS_string txt, const char * loc,
                const UTF8_string &  _creator, bool macro);

   /// overladed Function::may_push_SI()
   virtual bool may_push_SI() const   { return true; }

   /// Overloaded Function::eval_fill_B()
   /// @param B right argument APL value used to determine fill element
   virtual Token eval_fill_B(Value_P B) const;

   /// Overloaded Function::eval_fill_AB()
   /// @param A left argument APL value used to determine fill element
   /// @param B right argument APL value used to determine fill element
   virtual Token eval_fill_AB(Value_P A, Value_P B) const;

   /// return the line number where an error has occurred (-1 if none)
   int get_error_line() const
      { return error_line; }

   /// return information about an error (if any)
   const char * get_error_info() const
      { return error_info; }

   void bind_to_symbol() const
      {
        if (header.LO())   header.FUN()->set_NC(NC_OPERATOR, this);
        else               header.FUN()->set_NC(NC_FUNCTION, this);
      }

   /// helper function to print token with Function or Value content
   /// @param out output stream to write to
   /// @param tok token whose function or value content is printed
   static ostream & print_val_or_fun(ostream & out, const Token & tok);

   /// return the "[nn] " prefix
   /// @param nn function line number for the prefix
   UCS_string line_prefix(Function_Line nn) const;

   // debug function: print the body tokens line by line.
   /// @param where caller location label for the debug output header
   void print_body_by_line(const char * where) const;

   /// the header (line [0]) of the user-defined function
   UserFunction_header header;

   /** Offsets to the first token in every line (for jumps).
       lines[0] points to the last line, which is automatically
       added and is TOK_RETURN_VOID for void functions
       and TOK_RETURN_VALUE for functions returning a value.

       An N line function:

       [0] R←A FOO B
       [1] L1: xxx
       ...
       [N-1]   yyy

      will, for example, have a N+1 element jump vector:

      [0] goto end of function        ──────────────┐
      [1] L1: xxx                                   │
      ...                                           │
      [N-1]   yyy                                   │
      [N] TOK_RETURN_SYMBOL or TOK_RETURN_VOID   <──┘

   **/
   std::vector<Function_PC> line_starts;

   /// stop lines (from S∆fun ← lines)
   vector<Function_Line> stop_lines;

   /// trace lines (from S∆fun ← lines)
   vector<Function_Line> trace_lines;

   /// execution properties as per 3⎕AT
   int exec_properties[4];

   /// the entity (∇ editor, ⎕FX, or filename) that has created this function
   const UTF8_string creator;

   /// the line number where an error has occurred (-1 if none)
   int error_line;

   /// information about an error (if any) when constructing \b this function
   const char * error_info;
};
//----------------------------------------------------------------------------

#endif // __USERFUNCTION_HH_DEFINED__
