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

#ifndef __SYMBOL_HH_DEFINED__
#define __SYMBOL_HH_DEFINED__

#include <stdint.h>

#include "ErrorCode.hh"
#include "Function.hh"
#include "NamedObject.hh"
#include "Parser.hh"
#include "SystemLimits.hh"
#include "Svar_DB.hh"

class IndexExpr;
class RavelIterator;
class UserFunction;

//----------------------------------------------------------------------------
/** One entry in the value stack of a symbol. The value stack
    is pushed/poped when the symbol is localized on entry/return of
    a user defined function.
 */
/// One entry in the value stack of a Symbol
class ValueStackItem
{
   friend class Symbol;

public:
   /// return the function (caller must have checked NC_FUNOPER).
   cFunction_P get_function() const
      { return sym_val.function; }

   /// return the shared variable key (caller must have checked NC_SYSTEM_VAR).
   SV_key get_key() const
      { return sym_val.sv_key; }

   /// return the line number of a label (caller must have checked NC_LABEL)
   const Function_Line get_label() const
      { return sym_val.label; }

   /// return the name class for \b this ValueStackItem
   NameClass get_NC() const
      { return name_class; }

   /// return the APL value of \b this ValueStackItem, or 0 if it has none.
   /// Only used to iterate over the ValueStack in Doxy.cc and in Quad_RL to
   /// quickly access the current ⎕RL
   const Value * get_val_cptr() const
      { return apl_val.get(); }

   /// delete the function
   void clear_function()
      {
        sym_val.function = 0;
        name_class = NC_UNUSED_USER_NAME;
      }

   /// return the address of function (caller must have checked NC_FUNOPER).
   cFunction_P * get_function_P()
      { return & sym_val.function; }

   /// return the APL value  of \b this ValueStackItem, or 0 if it has none
   /// Only used in ⎕RL for quick in-place update of the current ⎕RL.
   Value * get_val_wptr()
      { return apl_val.get(); }

   /// make \b apl_val the sole owner of apl_val.value
   /// @param loc  caller location for diagnostics
   void isolate(const char * loc)
      { if (+apl_val)   apl_val.isolate(loc); }

   /// isolate \b apl_val and its sub-values
   /// @param loc  caller location for diagnostics
   void isolate_deep(const char * loc)
      { if (+apl_val)   apl_val.isolate_deep(loc); }

   /// reset (clear) the APL value in \b this ValueStackItem
   void reset_apl_value()
      { apl_val.reset(); }

   /// set \b this ValueStackItem to APL value new_value
   /// @param new_value  APL value to store
   void set_apl_value(Value_P new_value)
      {
        name_class = NC_VARIABLE;
        apl_val = new_value;
      }

   /// set function to \b fun
   /// @param fun  function pointer to store
   void set_function(cFunction_P fun)
      {
        name_class = fun->is_operator() ? NC_OPERATOR : NC_FUNCTION;
        sym_val.function = fun;
      }

   /// set the shared variable key
   /// @param key  shared variable key to store
   void set_key(SV_key key)
      { name_class = NC_SYSTEM_VAR;   sym_val.sv_key = key; }

   /// set the name class for \b this ValueStackItem
   /// @param nc  new name class to assign
   void set_NC(NameClass nc)
      { name_class = nc; }

protected:
   /// constructor: ValueStackItem for an unused symbol
   ValueStackItem()
   : name_class(NC_UNUSED_USER_NAME)
      { memset(&sym_val, 0, sizeof(sym_val)); }

   /// constructor: ValueStackItem for a label (function line)
   /// @param lab  function line number for the label
   ValueStackItem(Function_Line lab)
   : name_class(NC_LABEL)
      { sym_val.label = lab; }

   /// constructor: ValueStackItem for a variable
   /// @param val  APL value to hold
   ValueStackItem(Value_P val)
   : apl_val(val),
     name_class(NC_VARIABLE)
   {}

   /// constructor: ValueStackItem for a shared variable
   /// @param key  shared variable key
   ValueStackItem(SV_key key)
   : name_class(NC_SYSTEM_VAR)
      { sym_val.sv_key = key; }

   /// the possible "values" of a symbol
   union _sym_val
      {
        cFunction_P    function;   ///< if \b Symbol is a function
        Function_Line label;       ///< if \b Symbol is a label
        SV_key        sv_key;      ///< if \b Symbol is a shared variable
      };

   /// reset \b this ValueStackItem to being unused
   void clear()
      {
        // sym_val contains only POD variables, so it can be memset()
        memset(&sym_val, 0, sizeof(sym_val));
        if (!!apl_val)   apl_val.reset();
        name_class = NC_UNUSED_USER_NAME;
      }

   /// the (current) value of this symbol (unless variable)
   _sym_val sym_val;

   /// the (current) value of this symbol (if variable)
   Value_P apl_val;

   /// the (current) name class (like ⎕NC, unless shared variable)
   NameClass name_class;
};
//----------------------------------------------------------------------------
/// Base class for variables, defined functions, and distinguished names
class Symbol : public NamedObject
{
   friend class SymbolTable;

public:
   /// create a system symbol with Id \b id
   /// @param id  system symbol identifier
   Symbol(Id id);

   /// create a symbol with name \b ucs
   /// @param ucs  Unicode name of the symbol
   /// @param id   system identifier (or ID_USER_SYMBOL for user names)
   Symbol(const UCS_string & ucs, Id id);

   /// destructor
   virtual ~Symbol()
      { clear_vs(); }

   /// Compare name of \b this value with \b other
   /// @param other  symbol whose name is compared against this symbol's name
   int compare(const Symbol & other) const
       { return name.compare(other.name); }

   /// Compare the name of \b this \b Symbol with \b ucs
   /// @param ucs  Unicode string to compare against this symbol's name
   bool equal(const UCS_string & ucs) const
      { return (name.compare(ucs) == COMP_EQ); }

   /// The name of \b this \b Symbol
   virtual UCS_string get_name() const   { return name; }

   /// overloaded NamedObject::get_name_ptr()
   const UCS_string * get_name_ptr() const   { return &name; }

   /// overloaded NamedObject::get_symbol()
   virtual const Symbol * get_symbol() const
      { return this; }

   /// return a const pointer to the current APL value
   const Value * get_val_cptr() const
      { return value_stack.back().get_val_cptr(); }

   /// return true, iff this Symbol is not used (i.e. erased)
   bool is_erased() const
      { return value_stack_size() == 0 ||
              (value_stack_size() == 1 &&
               value_stack[0].get_NC() == NC_UNUSED_USER_NAME); }

   /// return true iff this variable is read-only
   /// (overloaded by RO_SystemVariable)
   virtual bool is_readonly() const   { return false; }

   /// return the idx'th item on stack (higher index = newer item)
   /// @param idx  stack index (0 = oldest/global entry)
   const ValueStackItem & operator [](int idx) const
      { return value_stack[idx]; }

   /// return the top-most item on the value stack
   const ValueStackItem * top_of_stack() const
      { return value_stack.size() ? &value_stack.back() : 0; }

   /// return the size of the value stack
   const int value_stack_size() const
      { return value_stack.size(); }

   /// call the monitor callback function (if any) with event \b ev
   /// @param ev  symbol event to report
   void call_monitor_callback(Symbol_Event ev)
      { if (monitor_callback)   monitor_callback(*this, ev); }

   /// overloaded NamedObject::get_symbol()
   virtual Symbol * get_symbol()
      { return this; }

   /// return a pointer to the current APL value
   Value * get_val_wptr()
      { return value_stack.back().get_val_wptr(); }

   /// return the idx'th item on stack (higher index = newer item)
   /// @param idx  stack index (0 = oldest/global entry)
   ValueStackItem & operator [](int idx)
      { return value_stack[idx]; }

   /// set a callback function for symbol events
   /// @param callback  function to call on symbol events
   void set_monitor_callback(void (* callback)(const Symbol &, Symbol_Event))
      { monitor_callback = callback; }

   /// return the top-most item on the value stack
   ValueStackItem * top_of_stack()
      { return value_stack.size() ? &value_stack.back() : 0; }

   /// return true, iff this Symbol can be assigned
   bool can_be_assigned() const;

   /// return a reason why this symbol cant become a defined function
   const char * cant_be_defined() const;

   /// dump this symbol to out
   /// @param out  output stream
   void dump(ostream & out) const;

   /// return the current APL value (or throw a VALUE_ERROR)
   virtual Value_P get_apl_value() const;

   /// return the first Cell of this value without creating a value
   const Cell * get_first_cell() const;

   /// Return the current function (or throw a VALUE_ERROR)
   virtual const Function * get_function() const;

   /// Return the function at SI level si, or 0 if none.
   /// @param si  state indicator stack level to query
   cFunction_P get_function(unsigned int si) const;

   /// return the level of fun on the stack of \b this Symbol) on the SI stack
   /// @param fun  function to locate on the SI stack
   int get_SI_level(cFunction_P fun) const;

   /// return the SI stack level of val on the stack of \b this Symbol)
   /// @param val  APL value to locate on the SI stack
   int get_SI_level(const Value & val) const;

   /// return the current SV_key (or throw a VALUE_ERROR)
   SV_key get_SV_key() const;

   /// List \b this \b Symbol ( for )VARS, )FNS )
   ostream & list(ostream & out) const;

   /// helper function for Prefix::MM_is_FM(). \b this is the symbol left
   /// of an operator M. Return \b true for function-like name classes and
   /// \b false for name-like name classes.
   bool M_is_F() const;

   /// return the token class of \b this \b Symbol WITHOUT calling resolve()
   /// @param left  true if resolving for a left-hand-side context
   TokenClass resolve_class(bool left) const;

   /// set \b token according to the current NC/sym_val of \b this shared var
   /// @param token  token to be resolved in place
   void resolve_shared_variable(Token & token) const;

   /// print variables owning value
   /// @param out    output stream
   /// @param value  APL value whose owners are to be printed
   int show_owners(ostream & out, const Value & value) const;

   /// clear the marked flag of all entries
   void unmark_all_values() const;

   /// Assign \b value to \b this \b Symbol
   /// @param value  APL value to assign
   /// @param clone  if true, clone the value before storing
   /// @param loc    caller location for diagnostics
   virtual void assign(Value_P value, bool clone, const char * loc);

   /// Indexed (multi-dimensional) assign \b value to \b this \b Symbol
   /// @param index  multi-dimensional index expression
   /// @param value  APL value to assign at the indexed positions
   virtual void assign_indexed(const IndexExpr & index, Value_P value);

   /// Indexed (one-dimensional) assign \b value to \b this \b Symbol
   /// @param X      one-dimensional index vector
   /// @param value  APL value to assign at the indexed positions
   virtual void assign_indexed(const Value * X, Value_P value);

   /// assign lambda, eg. V←{ ... }
   /// @param lambda  lambda function to assign
   /// @param loc     caller location for diagnostics
   virtual bool assign_named_lambda(cFunction_P lambda, const char * loc);

   /// Assign \b value to \b this \b Symbol (which is a shared variable)
   /// @param value  APL value to assign
   /// @param loc    caller location for diagnostics
   void assign_shared_variable(Value_P value, const char * loc);

   /// clear the value stack of \b this symbol
   void clear_vs();

   /// set current NameClass of this Symbol to NC_UNUSED_USER_NAME and remove
   /// any values associated with this symbol
   virtual int expunge();

   /// store the attributes (as per ⎕AT) of symbol in Z...
   /// @param mode  ⎕AT mode selecting which attributes to retrieve
   /// @param Z     output value receiving the attributes
   virtual void get_attributes(int mode, Value & Z) const;

   /// return the depth (global == 0) of \b ufun on the stack. Use the largest
   /// depth if ufun is pushed multiple times
   /// @param ufun  user-defined function to search for on the stack
   int get_exec_ufun_depth(const UserFunction * ufun);

   /// overloaded NamedObject::get_function(), 0 for non-functions
   virtual cFunction_P get_function();

   /// get the value if a variable, 0 if none
   Value_P get_var_value();

   /// Pop latest entry from the stack of \b this \b Symbol
   virtual void pop();

   /// Print \b this \b Symbol to \b out
   /// @param out  output stream
   virtual ostream & print(ostream & out) const;

   /// Print \b this \b Symbol and its stack to \b out
   /// @param out  output stream
   ostream & print_verbose(ostream & out) const;

   /// Push an undefined entry onto the stack of \b this \b Symbol
   virtual void push();

   /// push a function onto the stack of \b this \b Symbol
   /// @param function  function pointer to push
   virtual void push_function(cFunction_P function);

   /// push a label onto the stack of \b this \b Symbol
   /// @param label  function line number for the label
   virtual void push_label(Function_Line label);

   /// Push an APL value onto the stack of \b this \b Symbol
   /// @param value  APL value to push
   virtual void push_value(Value_P value);

   /// set \b token according to the current NC/sym_val of \b this \b Symbol.
   /// Decrement PC if the name class of \b token is either
   /// NC_UNUSED_USER_NAME or unexpected (supposedly to re-read the
   /// the resolved token).
   /// @param token  token to be resolved in place
   /// @param PC     program counter, decremented on re-read
   virtual void resolve_left(Token & token, Function_PC & PC) const;

   /// resolve a variable name for an assignment (left of ←)
   /// @param loc  caller location for diagnostics
   virtual Token resolve_lv(const char * loc);

   /// set \b token according to the current NC/sym_val of \b this \b Symbol.
   /// Decrement PC if the name class of \b token is either
   /// NC_UNUSED_USER_NAME or unexpected (supposedly to re-read the
   /// the resolved token).
   /// @param token  token to be resolved in place
   /// @param PC     program counter, decremented on re-read
   virtual void resolve_right(Token & token, Function_PC & PC) const;

   /// Set current NameClass of this Symbol to \b nc
   /// @param nc  new name class
   void set_NC(NameClass nc);

   /// Set current NameClass of this Symbol to \b nc and function fun
   /// @param nc   new name class
   /// @param fun  function pointer to associate with this symbol
   void set_NC(NameClass nc, cFunction_P fun);

   /// set the current SV_key
   void set_SV_key(SV_key key);

   /// share variable with \b proc
   /// @param key  shared variable key identifying the offer
   void share_var(SV_key key);

   /// unshare a shared variable
   SV_Coupling unshare_var();

   /// write \b this symbol in )OUT format to file \b out
   /// @param out  destination file
   /// @param seq  running sequence number for )OUT records
   void write_OUT(FILE * out, uint64_t & seq) const;

   /// perform a vector assignment (like (A B C)←1 2 3) for variables in
   /// \b symbols with values \b values
   /// @param symbols  vector of symbols to receive assigned values
   /// @param values   APL value whose items are distributed to symbols
   static void vector_assignment(std::vector<Symbol *> & symbols,
                                 Value_P values);

   /// The next Symbol with the same hash value as \b this \b Symbol
   Symbol * next;

protected:
   /// called on symbol events (if non-0)
   void (*monitor_callback)(const Symbol &, Symbol_Event sev);

   /// the name of \b this \b Symbol
   UCS_string name;

   /// the value stack of \b this \b Symbol
   std::vector<ValueStackItem> value_stack;
};
//----------------------------------------------------------------------------
/// lambda result λ
class LAMBDA : public Symbol
{
public:
   /// constructor
   LAMBDA()
   : Symbol(ID_LAMBDA)
   {}

   /// overloaded Symbol::assign(), suppressing assignment if not localized
   /// @param value  APL value to assign
   /// @param clone  if true, clone the value before storing
   /// @param loc    caller location for diagnostics
   virtual void assign(Value_P value, bool clone, const char * loc)
      { if (value_stack_size() > 1)   Symbol::assign(value, clone, loc); }

   /// destroy variable (don't)
   void destroy_var() {}
};
//----------------------------------------------------------------------------
/// lambda variable ⍺
class ALPHA : public Symbol
{
public:
   /// constructor
   ALPHA()
   : Symbol(ID_ALPHA)
   {}

   /// overloaded Symbol::assign(), suppressing assignment if not localized
   /// @param value  APL value to assign
   /// @param clone  if true, clone the value before storing
   /// @param loc    caller location for diagnostics
   virtual void assign(Value_P value, bool clone, const char * loc)
      { if (value_stack_size() > 1)   Symbol::assign(value, clone, loc); }

   /// destroy variable (don't)
   void destroy_var() {}
};
//----------------------------------------------------------------------------
/// lambda variable ⍶
class ALPHA_U : public Symbol
{
public:
   /// constructor
   ALPHA_U()
   : Symbol(ID_ALPHA_U)
   {}

   /// overloaded Symbol::assign(), suppressing assignment if not localized
   /// @param value  APL value to assign
   /// @param clone  if true, clone the value before storing
   /// @param loc    caller location for diagnostics
   virtual void assign(Value_P value, bool clone, const char * loc)
      { if (value_stack_size() > 1)   Symbol::assign(value, clone, loc); }

   /// destroy variable (don't)
   void destroy_var() {}
};
//----------------------------------------------------------------------------
/// lambda variable χ
class CHI : public Symbol
{
public:
   /// constructor
   CHI()
   : Symbol(ID_CHI)
   {}

   /// overloaded Symbol::assign(), suppressing assignment if not localized
   /// @param value  APL value to assign
   /// @param clone  if true, clone the value before storing
   /// @param loc    caller location for diagnostics
   virtual void assign(Value_P value, bool clone, const char * loc)
      { if (value_stack_size() > 1)   Symbol::assign(value, clone, loc); }

   /// destroy variable (don't)
   void destroy_var() {}
};
//----------------------------------------------------------------------------
/// lambda variable ⍵
class OMEGA : public Symbol
{
public:
   /// constructor
   OMEGA()
   : Symbol(ID_OMEGA)
   {}

   /// overloaded Symbol::assign(), suppressing assignment if not localized
   /// @param value  APL value to assign
   /// @param clone  if true, clone the value before storing
   /// @param loc    caller location for diagnostics
   virtual void assign(Value_P value, bool clone, const char * loc)
      { if (value_stack_size() > 1)   Symbol::assign(value, clone, loc); }

   /// destroy variable (don't)
   void destroy_var() {}
};
//----------------------------------------------------------------------------
/// lambda variable ⍹
class OMEGA_U : public Symbol
{
public:
   /// constructor
   OMEGA_U()
   : Symbol(ID_OMEGA_U)
   {}

   /// overloaded Symbol::assign(), suppressing assignment if not localized
   /// @param value  APL value to assign
   /// @param clone  if true, clone the value before storing
   /// @param loc    caller location for diagnostics
   virtual void assign(Value_P value, bool clone, const char * loc)
      { if (value_stack_size() > 1)   Symbol::assign(value, clone, loc); }

   /// destroy variable (don't)
   void destroy_var() {}
};
//----------------------------------------------------------------------------

#endif // __SYMBOL_HH_DEFINED__
