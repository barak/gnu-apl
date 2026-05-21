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
   bool localizes(const Symbol * sym) const
      { return header.localizes(sym); }

   /// pop all
   void pop_local_vars() const
      { header.pop_local_vars(); }

   /// print the local variables for command )SINL
   void print_local_vars(ostream & out) const
      { return header.print_local_vars(out); }

   /// return the number of local variables
   ShapeItem local_var_count() const
      { return header.local_var_count(); }

   /// return the idx'th local variable
   const Symbol * get_local_var(ShapeItem idx) const
      { return header.get_local_var(idx); }

   /// add a label
   void add_label(Symbol * sym, Function_Line line)
      { header.add_label(sym, line); }

   /// return the execution properties (dyadic ⎕FX) of this defined function
   virtual const int * get_exec_properties() const
      { return exec_properties; } 

   /// ovewrloaded Executable::cannot_suspend
   virtual bool cannot_suspend() const
      { return exec_properties[1] != 0; }

   /// set execution properties (as per A ⎕FX)
   void set_exec_properties(const int * props)
      { loop(i, 4)   exec_properties[i] = props[i]; }

   /// overloaded Executable::print()
   virtual ostream & print(ostream & out) const;

   /// return the name of \b this function
   virtual UCS_string get_name() const
      { return header.get_name(); }

   /// return \b true if \b symbol is a label of \b this function
   bool is_label(const Symbol * symbol) const
      {
        loop(l, header.get_label_count())
            {
              if (symbol == header.get_label(l).sym)   return true;
            }

        return false;
      }

   /// return \b true if \b token is for a label of \b this function
   bool is_label(const Token & tok)
      { return tok.get_Class() == TC_SYMBOL && is_label(tok.get_sym_ptr()); }

   /// return \b true if \b token is for a label of \b this function
   bool is_label_or_value(const Token & tok)
      {
        if (tok.get_Class() == TC_VALUE)   return true;
         return is_label(tok);
      }

   /// return the line number for label \b symbol \b this function
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
   UCS_string get_name_and_line(Function_PC pc) const;

   /// Overloaded Function::print_properties()
   virtual void print_properties(ostream & out, int indent) const;

   /// )SAVE this function in the workspace named \b workspace
   /// (in the file system).
   void save(const char * workspace, const char * function);

   /// )LOAD this function into the workspace named \b workspace.
   /// Return a pounter to this newly created function (or 0 on error).
   static UserFunction * load(const char * workspace, const char * function);

   /// Load this function into the workspace named \b workspace.
   static UserFunction * do_load(const char * workspace, const char * function);

   /// overloaded Function::destroy()
   virtual void destroy();

   /// overloaded Executable::pushes_sym() const
   virtual bool pushes_sym(const Symbol * sym) const;

   /// print help for this function on out (for the )HELP command)
   void help(ostream & out) const;

   /// create a user defined function according to \b data of length \b len
   /// in workspace \b w.
   static UserFunction * fix(const UCS_string & text, int & err_line,
                             bool keep_existing, const char * loc,
                             const UTF8_string &  creator);

   /// (re-)create a lambda
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
   virtual Token eval_B(Value_P B) const;

   /// Overloaded Function::eval_XB()
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// Overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// Overloaded Function::eval_AXB()
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// Overloaded Function::eval_LB.
   virtual Token eval_LB(Token & LO, Value_P B) const;

   /// Overloaded Function::eval_LXB()
   virtual Token eval_LXB(Token & LO, Value_P X, Value_P B) const;

   /// Overloaded Function::eval_ALB.
   virtual Token eval_ALB(Value_P A, Token & LO, Value_P B) const;

   /// Overloaded Function::eval_ALXB()
   virtual Token eval_ALXB(Value_P A, Token & LO, Value_P X, Value_P B) const;

   /// Overloaded Function::eval_LRB()
   virtual Token eval_LRB(Token & LO, Token & RO, Value_P B) const;

   /// Overloaded Function::eval_LRXB()
   virtual Token eval_LRXB(Token & LO, Token & RO, Value_P X, Value_P B) const;

   /// Overloaded Function::eval_ALRB()
   virtual Token eval_ALRB(Value_P A, Token & LO, Token & RO, Value_P B) const;

   /// Overloaded Function::eval_ALRXB()
   virtual Token eval_ALRXB(Value_P A, Token & LO, Token & RO, Value_P X,
                            Value_P B) const;

   /// Quad_CR of this function
   virtual UCS_string canonical(bool with_lines) const;

   /// overloaded Executable::line_start()
   virtual Function_PC line_start(Function_Line line) const;

   /// overloaded Executable::remove_TOK_VOID()
   virtual VoidCount remove_TOK_VOID();

   /// compute lines 2 and 3 in \b error
   void set_locked_error_info(Error & error) const;

   /// return the entity that has created the function
   const UTF8_string get_creator() const
      { return creator; }

   /// set trace or stop vector
   void set_trace_stop(const std::vector<Function_Line> & lines, bool stop);

   /// recompile the body
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
   void print_line_PCs(const char * loc) const;

   /// resolve labels in the function body. Return \b true if any
   /// labels were resolved.
   bool optimize_labels();

   bool optimize_labels2();

   /// optimize vectors of labels
   bool optimize_label_vectors();

protected:
   /// constructor for a normal (i.e. non-lambda) user defined function
   UserFunction(const UCS_string txt, const char * loc,
                const UTF8_string &  _creator, bool macro);

   /// overladed Function::may_push_SI()
   virtual bool may_push_SI() const   { return true; }

   /// Overloaded Function::eval_fill_B()
   virtual Token eval_fill_B(Value_P B) const;

   /// Overloaded Function::eval_fill_AB()
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
   static ostream & print_val_or_fun(ostream & out, const Token & tok);

   /// return the "[nn] " prefix
   UCS_string line_prefix(Function_Line nn) const;

   // debug function: print the body tokens line by line.
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
