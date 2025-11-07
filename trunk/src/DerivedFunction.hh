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

#ifndef __DERIVED_FUNCTION__DEFINED__
#define __DERIVED_FUNCTION__DEFINED__

#include "Error.hh"
#include "Function.hh"
#include "Output.hh"

//----------------------------------------------------------------------------
/** base class for all Derived_XXX classes. Used to bind an axis and/or left
    and/or right function or operator arguments to a function or operator.

    DerivedFunction can not be instantiated directly but only via a class
    derived from it.
 **/
/// An APL function operator bound to its function operand(s) and/or axis.
class DerivedFunction : public Function
{
public:
   /// default constructor (for DerivedFunctionCache)
   DerivedFunction() : Function(TOK_FUN0)   {}

   /// deallocate resources held by this DerivedFunction
   void destroy_derived(const char * loc);

   /// overloaded NamedObject::get_name()
   virtual UCS_string get_name() const;

   /// overloaded Function::print();
   virtual ostream & print(ostream & out) const
      { return out << get_name(); }

   /// overloaded Function::is_derived();
   virtual bool is_derived() const
      { return true; }

   /// return the left operand of this derived function
   cFunction_P get_LO() const
      { return left_arg.get_function(); }

   /// return the value (if any) bound to an operator (that allows it)
   Value_P get_bound_LO_value() const
      {
        return left_arg.is_apl_val() ? left_arg.get_apl_val() : Value_P();
      }

   /// return the operator of this derived function
   cFunction_P get_OPER() const
      { return oper; }

   /// return the right operand (or 0) of this derived function
   cFunction_P get_RO() const
      {
        return right_fun.get_tag() == TOK_VOID ? 0 : right_fun.get_function();
      }

   /// return the axis argument (or 0 if none) of this derived function
   const Value * get_AXIS() const
      { return axis.get(); }

   /// Overloaded Function::has_result();
   virtual bool has_result() const;

   /// clear the marked bit in values bound to this derived functions (if any).
   void unmark_all_values() const;   // unmark values bound to this operator

protected:
  /// constructor. Set omitted arguments to 0.
  DerivedFunction(Token * LO, cFunction_P F_or_M_or_D, Token * RO, Value_P X,
                  const char * loc);

   /// Overloaded Function::print_properties()
   virtual void print_properties(ostream & out, int indent) const;

   /// overloaded Function::may_push_SI()
   virtual bool may_push_SI() const
      { return   oper->may_push_SI()
        || (left_arg .is_function() && left_arg .get_function()->may_push_SI())
        || (right_fun.is_function() && right_fun.get_function()->may_push_SI());
      }

   /// overloaded Function::locate_X()
   virtual Value_P * locate_X() const
      { return !axis ? 0 : const_cast<Value_P *>(&axis); }

   /// debug printout when an eval_XXX() function is called.
   void entering(const char * class_name, const char * fun_name) const;

   /// the function (to the left of the operator).
   // Token are ephemeral, therefore we need a copy here.
   Token left_arg;

   /// the monadic operator (to the right of the function)
   cFunction_P oper;

   /// the (normally) function on the right of the (dyadic) operator (if any).
   //  Acording to lrm p. 35, the right operand is a function or array (even
   //  though no primitive dyadic operator takes an array as right operand).
   // Token are ephemeral, therefore we need a copy here.
   Token right_fun;

   /// the axis for \b mon_oper, or 0 if no axis
   Value_P axis;

   /// destructor
   ~DerivedFunction();
};
//============================================================================
/// A dyadic operator axis bound to its left and right function.
class Derived_LO_D_RO: public DerivedFunction
{
public:
   /// constructor
   Derived_LO_D_RO(Token & LO, cDyaOP D, Token & RO, const char * loc)
   : DerivedFunction(&LO, D, &RO, Value_P(), loc)
   {
     Log(LOG_FunOperX)   CERR << "Binding: (LO D RO) at " << loc << endl;
   }

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_AXB()
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_XB()
   virtual Token eval_XB(Value_P X, Value_P B) const;
};
//============================================================================
/// A dyadic operator axis bound to its left and right function and its axis.
class Derived_LO_D_X_RO : public DerivedFunction
{
public:
   /// constructor
   Derived_LO_D_X_RO(Token & LO, cDyaOP D, Value_P X, Token & RO,
                             const char * loc)
   : DerivedFunction(&LO, D, &RO, X, loc)
   {
     Log(LOG_FunOperX)   CERR << "Binding: (LO D X RO) at " << loc << endl;
     Assert(+X);
   }

   /// overloaded Function::eval_AXB()
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_XB()
   virtual Token eval_B(Value_P B) const;
};
//============================================================================
/// A monadic operator axis bound to its left function.
class Derived_LO_M: public DerivedFunction
{
public:
   /// constructor
   Derived_LO_M(Token & LO, cMonOP M, const char * loc)
   : DerivedFunction(&LO, M, 0, Value_P(), loc)
   {
     Log(LOG_FunOperX)   CERR << "Binding: (LO M) at " << loc << endl;
   }

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_AB()
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_AXB()
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_XB()
   virtual Token eval_XB(Value_P X, Value_P B) const;
};
//============================================================================
/// A monadic operator axis bound to its left function and to its axis.
class Derived_LO_M_X: public DerivedFunction
{
public:
   /// constructor
   Derived_LO_M_X(Token & LO, cMonOP M, Value_P X,
                          const char * loc)
   : DerivedFunction(&LO, M, 0, X, loc)
   {}

   /// overloaded Function::eval_AB();
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B();
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_AXB();
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_XB();
   virtual Token eval_XB(Value_P X, Value_P B) const;


};
//============================================================================
/// A function axis bound to its function (not operaator)..
class Derived_F_X : public DerivedFunction
{
public:
   /// constructor
   Derived_F_X(cFunction_P F, Value_P X, const char * loc)
   : DerivedFunction(0, F, 0, X, loc)
   {
     Log(LOG_FunOperX)   CERR << "Binding: (F rXM) at " << loc << endl;
     Assert(+X);
   }

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const;
};
//============================================================================
/// a small cache for storing a few DerivedFunction objects
class DerivedFunctionCache
{
public:
   /// constructor: create empty FunOper cache
   DerivedFunctionCache();

   /// destructor
   ~DerivedFunctionCache();

   /// return the i'th derived function
   const DerivedFunction * at(size_t i) const
      {
        return reinterpret_cast<const DerivedFunction *>
                               (cache + i*sizeof(DerivedFunction));
      }

   /// return the i'th derived function
   const DerivedFunction & operator [](size_t i) const
      {
        Assert(i < idx);
        return *(at(i));
      }

   /// clear the marked bit in values bound to derived functions (if any).
   void unmark_all_values() const
      { loop(d, size())   at(d)->unmark_all_values(); }

   /// return the number of items in the cache
   size_t size() const
      { return idx; }

   /// reset (clear) the cache
   void reset();

   /// return the last cache entry and increment \b idx. To be used with
   /// placement new.
   DerivedFunction * get(const char * loc);

protected:
   /// a cache for derived functions
   uint8_t cache[sizeof(DerivedFunction) * MAX_FUN_OPER];

   /// the number of elements in \b cache
   size_t idx;
};
//----------------------------------------------------------------------------
#endif // __DERIVED_FUNCTION__DEFINED__
