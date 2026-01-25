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

#ifndef __FUNCTION_HH_DEFINED__
#define __FUNCTION_HH_DEFINED__

#include <sys/types.h>

#include "Error.hh"
#include "NamedObject.hh"
#include "Parser.hh"
#include "PrintBuffer.hh"
#include "Value.hh"

class Workspace;
class UserFunction;
class RavelIterator;

/* Naming conventions for the virtual eval_XXX() functions (in the order of
   appearance in suffix XXX (from left to right).

   A: left value argument (functions and operators)
   L: left function argument (monadic and dyadic operators)
   R: right function argument  (dyadic operators)
   X: axis argument (functions and operators)
   B: right value argument (functions and operators)

   default implementation functions with axis: ignore axis
*/
//----------------------------------------------------------------------------
/** additional functionality for functions with named sub-functions.
    I.e one of ⌹, ⎕CR, ⎕FFT, FIO, ⎕MX, ⎕RVAL, or ⎕SQL.
  */
class FunctionGroup
{
protected:
   /// information pertaining to one subfunction
   struct function_info
      {
        /// the function axis (= function number)
        sAxis axis;

        /// the function name (for e.g. ⎕XXX.function_name) in Prefix.cc
        const char * function_name;

        /// the comment in ⎕XXX '' (mapping == true; currently only used
        /// by  ⎕SQL only).
        /// the syntax table is sorted by function names
        const char * comment_map;

        /// the comment in ⎕XXX ⍬ (mapping = false).
        /// the syntax table is sorted by function number
        const char * comment_fun;

        /// ther function valence, -1 if not specified
        int valence;
      };

public:
   /// default constructor (for functions that are NOT function groups)
   FunctionGroup()
   : group_name(0),
     subfun_count(0),
     max_function_name_length(0)
   {}

   /// initialize \b sorted_by_name and \b sorted_by_axis.
   void init_function_group(const function_info * infos, size_t count,
                            const char * group_name);

   /// return \b true if \b this function has (named) sub-functions.
   /// Most do not.
   bool has_subfuns() const
      { return subfun_count != 0; }

   /// return a function number (pseudo-axis) for subfunction \b name,
   /// or -1 if \b name is not a valid subfunction name
   ///
   sAxis subfun_to_axis(const UCS_string & subfun_name) const;

   /// return a function number (pseudo-axis) for \b value. \b value shall
   /// be an integer scalar, or else a valid subfunction name (APL string).
   sAxis value_to_subfun(const Value & A_or_X) const;
   //
   /// print some help (not for )HELP, but for ⎕XXX ⍬).
   /// The help for )HELP is defined in \b Help.def
   Token list_functions(ostream & out) const;

   /// print the syntax variants for all subfunctions.
   Token list_mappings(ostream & out) const;

   /// complain about an invalid subfunctioon name
   void bad_subfun_name_ERROR(const UCS_string sub_name) const GNUC__noreturn;

   /// complain about an invalid subfunctioon name
   void bad_subfun_number_ERROR(int number) const GNUC__noreturn;

   /// static texts before and after syntax tables
   enum Legend_type
      {
        LET_FUN_PREFIX,   ///< ⎕XXX '' starr
        LET_FUN_SUFFIX,   ///< ⎕XXX '' end
        LET_MAP_PREFIX,   ///< ⎕XXX ⍬ starr
        LET_MAP_SUFFIX,   ///< ⎕XXX ⍬ end
      };

protected:
   /// helper function to print a 1-line function help.
   virtual const char * get_legend(Legend_type lt) const
      { return ""; }   // for non-FunctionGroups

   /// helper function to print the subfunction function syntax
   virtual void print_fun_syntax(ostream & out, const function_info &) const
      { out << "*** missing FunctionGroup::print_fun_syntax()" << endl; }

   /// helper function to print the subfunction syntax mapping
   virtual void print_map_syntax(ostream & out, const function_info &) const
      { out << "*** missing FunctionGroup::print_map_syntax()" << endl; }

   /// compare names. Return \b > 0 if info1 > info2
   static int compare_function_name(const char * const & name,
                                     const function_info * const & info,
                                     const void *);

   /// compare axes. Return \b > 0 if info1 > info2
   static int compare_function_axis(const uAxis & key,
                                    const function_info * const & info,
                                    const void *);

   /// compare names. Return \b true if info1 > info2
   static bool greater_function_name(const function_info * const & info1,
                                     const function_info * const & info2,
                                     const void *)
      { return compare_function_name(info1->function_name, info2, 0) > 0; }

   /// compare axes. Return \b true if info1 > info2
   static bool greater_function_axis(const function_info * const & info1,
                                     const function_info * const & info2,
                                     const void *)
      { return compare_function_axis(info1->axis, info2, 0) > 0; }
 
   /// return the function_info for function name \b name
   const function_info * get_info_by_name(const char * name) const;

   /// return the function_info for function axis \b axis
   const function_info * get_info_by_axis(uAxis axis) const;

   /// return the signature \b sig as string
   UCS_string get_signature_string(Fun_signature sig) const;

   /// the group name (⎕CR, ⎕FFT, ... Only non-zero if this functiuon is a group
   /// (ass opposed to being a function without subfunctions).
   const char * group_name;

   /// the table, sorted by function name
   vector<const function_info *> sorted_by_name;

   /// the table, sorted by function number (= axis)
   vector<const function_info *> sorted_by_axis;

   /// the number of subfunctions
   int subfun_count;

   /// the longest subfunction name (for tabular output)
   size_t max_function_name_length;
};
//----------------------------------------------------------------------------
/** The base class for all functions (user defined or system functions).  */
/// Base class for all APL functions and operators (system or defined)
class Function : public NamedObject, public FunctionGroup
{
public:
   /// constructor for most functions
   Function(TokenTag _tag)
   : NamedObject(Id(_tag >> 16)),
     creation_time(0),
     tag(_tag)
   { parallel_thresholds[0] = parallel_thresholds[1] = -1; }

   /// constructor for functions whose Id does not match their tag
   Function(Id id, TokenTag _tag)
   : NamedObject(id),
     creation_time(0),
     tag(_tag)
   { parallel_thresholds[0] = parallel_thresholds[1] = -1; }

   /// destructor
   virtual ~Function()
   {}

   /// return \b true iff \b this is an operator.
   virtual bool is_operator() const   { return false; }

   /// return \b true iff \b this is a native function
   virtual bool is_native() const   { return false; }
   virtual bool is_scalar_function() const   { return false; }

   /// return \b true iff \b this function is a lambda
   virtual bool is_lambda() const   { return false; }

   /// return \b true iff \b this function is a macro
   virtual bool is_macro() const   { return false; }

   /// return \b true iff \b this function is defined (= has an APL body)
   virtual bool is_defined() const   { return false; }

   /// return \b true iff \b this function is a derived function
   virtual bool is_derived() const
      { return false; }

   /// a monadic Cell function that returns bool
   typedef bool (*bool_f1)(const Cell & B);

   /// a dyadic Cell function that returns bool
   typedef bool (*bool_f2)(const Cell & A, const Cell & B);

   /// a monadic function that returns packed bool for packed A
   typedef uint64_t (*bool_f1_bool)(uint64_t A);

   /// a dyadic function that returns packed bool for packed A and packed B
   typedef uint64_t (*bool_f2_bool)(uint64_t A, uint64_t B);

   /// return \b a bool_f1() (if any), otherwise 0.
   virtual bool_f1 get_bool_f1() const
      { return 0; }

   /// return \b a bool_f2() (if any), otherwise 0.
   virtual bool_f2 get_bool_f2() const
      { return 0; }

   /// return \b a bool_f1() (if any), otherwise 0.
   virtual bool_f1_bool get_bool_f1_bool() const
      { return 0; }

   /// return \b a bool_f2() (if any), otherwise 0.
   virtual bool_f2_bool get_bool_f2_bool() const
      { return 0; }

   /// return \b true iff \b this function returns a (possibly nested)
   /// boolean result and takes (only) boolean arguments.
   /// For example: ∧, ∨, ∼, ...
   virtual bool is_bool_bool() const
      { return 0; }

   /// return \b true if \b eval_XXX may push the SI. True for ⍎, user defined
   /// functions, and operators derived from user defined functions
   virtual bool may_push_SI() const   { return false; }

   /// return the number of value arguments (0, 1, or 2) of a user defined
   /// function, For non-user defined functions, throw DOMAIN_ERROR.
   virtual int get_fun_valence() const
      {
        const TokenClass tc = TokenClass(tag & TC_MASK);
        if (tc == TC_FUN2)   return 2;
        if (tc == TC_FUN1)   return 1;
        return 0;
      }

   /// return the number of function arguments (1 or 2) of an operator,
   /// or 0 for a "normal" function that isn't an operator.
   virtual int get_oper_valence() const   { return 0; }

   /// return 1 if the function returns a result, otherwise 0.
   /// For non-user defined functions, throw DOMAIN_ERROR.
   virtual bool has_result() const = 0;

   /// return true iff the function has and axis (system functions
   /// always have an axis even though most of them ignore it).
   virtual bool has_axis() const    { return true; }

   /// overloaded NamedObject::get_function()
   virtual const Function * get_function() const   { return this; }

   /// the monadic inverse function of \b this function (if any)
   virtual cFunction_P get_monadic_inverse() const   { return 0; }

   /// the dyadic inverse function of \b this function (if any)
   virtual cFunction_P get_dyadic_inverse() const   { return 0; }

   /// GMT when this function was created; 0 for system functions
   APL_time_us get_creation_time() const
      {  return creation_time; }

   /// set the time when the function was created (0 for built-in functions)
   void set_creation_time(APL_time_us t)
      { creation_time = t; }

   /// return the execution properties (3 ⎕AT) for this function
   virtual const int * get_exec_properties() const
      { static int ep[] = { 1, 1, 1, 0 };   return ep; }

   /// print the properties of this function
   virtual void print_properties(ostream & out, int indent) const = 0;

   /// store the attributes (as per ⎕AT) of symbol in Z, ...
   virtual void get_attributes(int mode, Value & Z) const;

   /// return a pointer to \b this UserFunction (if it is one)
   virtual const UserFunction * get_func_ufun() const   { return 0; }

   /// return true if this function has a name with alphabetic chars,
   /// i.e. the function is either a defined function or a ⎕-function
   virtual bool has_alpha() const   { return false; }

   /// Print \b this function.
   virtual ostream & print(ostream & out) const = 0;

   /// return the dyadic scalar primitive if \b this function
   virtual prim_f2 get_scalar_f2() const
      { return 0; }

   /// an associative cell function
   typedef ErrorCode (Cell::*assoc_f2)(Cell *, const Cell *) const;

   /// if this function is an associative scalar function then return
   /// its associative cell function, otherwise 0.
   virtual assoc_f2 get_assoc() const
      { return 0; }

   /// return a \b Token for \b this function.
   Token get_token() const { return Token(tag, this); }

   /// return the \b Token Tag for \b this function.
   TokenTag get_tag() const { return tag; }

   /// plain function, 0 arguments
   virtual Token eval_() const;

   /// plain function, 1 argument
   virtual Token eval_B(Value_P B) const;

   /// plain function, 2 arguments
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// monadic operator, 1 argument
   virtual Token eval_LB(Token & LO, Value_P B) const;

   /// plain function, 1 argument, plus axis
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// monadic operator, 2 arguments
   virtual Token eval_ALB(Value_P A, Token & LO, Value_P B) const;

   /// plain function, 2 arguments, plus axis
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// dyadic operator, 1 arguments
   virtual Token eval_LRB(Token & LO, Token & RO, Value_P B) const;

   /// monadic operator, 1 arguments, plus axis
   virtual Token eval_LXB(Token & LO, Value_P X, Value_P B) const;

   /// dyadic operator, 2 arguments
   virtual Token eval_ALRB(Value_P A, Token & LO, Token & RO, Value_P B) const;

   /// monadic operator, 2 arguments, plus axis
   virtual Token eval_ALXB(Value_P A, Token & LO, Value_P X, Value_P B) const;

   /// dyadic operator, 1 arguments, plus axis
   virtual Token eval_LRXB(Token & LO, Token & RO, Value_P X, Value_P B) const;

   /// dyadic operator, 2 arguments, plus axis
   virtual Token eval_ALRXB(Value_P A, Token & LO, Token & RO,
                            Value_P X, Value_P B) const;

   /// Evaluate \b the fill function.
   virtual Token eval_fill_B(Value_P B) const;

   /// Evaluate \b the fill function.
   virtual Token eval_fill_AB(Value_P A, Value_P B) const;

   /** Evaluate \b the identity function. B is empty and the result is a
   /// value f/B0 with (B0 f ↑B) ≡ B for all B (and f is \b this function).
   /// If such a B0 does not exist then raise a DOMAIN ERROR.

       NOTE that eval_identity_fun() returns f/B0 and not B0 !!!
    **/
   virtual Token eval_identity_fun(Value_P B, sAxis axis) const;

   /// Delete this function (do nothing, overloaded by UserFunction).
   virtual void destroy() {}

   /// Quad_CR of this function.
   virtual UCS_string canonical(bool with_lines) const
      { NeverReach("Function::canonical() called"); }

   /// return axis (non-0 only for derived functions)
   virtual Value_P * locate_X() const
      { return 0; }

   /// return the signature of this function (currently only valid
   /// for user-defined functions)
   Fun_signature get_signature() const;

   /// raise an invalid phrase error
   Token phrase_error(const char * pattern) const;

   /// return break-even point for monadic parallel execution
   ShapeItem get_monadic_threshold() const
      { return parallel_thresholds[0]; }

   /// set the break-even point for monadic parallel execution
   void set_monadic_threshold(ShapeItem new_threshold)
      { parallel_thresholds[0] = new_threshold; }

   /// return break-even point for dyadic parallel execution
   ShapeItem get_dyadic_threshold() const
      { return parallel_thresholds[1]; }

   /// set the break-even point for dyadic parallel execution
   void set_dyadic_threshold(ShapeItem new_threshold)
      { parallel_thresholds[1] = new_threshold; }

   /// when this function was created (0.0 for system functions)
   APL_time_us creation_time;

protected:
   /// the tag for token pointing to \b this function
   TokenTag tag;

   /// the thresholds for parallel execution
   ShapeItem parallel_thresholds[2];
};
//----------------------------------------------------------------------------

#endif // __FUNCTION_HH_DEFINED__
