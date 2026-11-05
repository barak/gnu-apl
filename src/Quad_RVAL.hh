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

#ifndef __Quad_RVAL_DEFINED__
#define __Quad_RVAL_DEFINED__

using namespace std;

#include "QuadFunction.hh"

//════════════════════════════════════════════════════════════════════════════
/// a random APL value
class Quad_RVAL : public QuadFunction
{
public:
   /// Constructor.
   Quad_RVAL();

   static Quad_RVAL  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB()
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_B()
   /// @param B right argument APL value
   virtual Token eval_B(cValue_R B) const;

   /// overloaded Function::eval_XB().
   /// ⎕RVAL[X] B  ←→  X ⎕RVAL B
   /// @param X axis/subfunction index
   /// @param B right argument APL value
   virtual Token eval_XB(cValue_R X, cValue_R B) const;

   /// overloaded FunctionGroup::print_fun_syntax()
   /// @param out  output stream to print to
   /// @param info function info entry describing the subfunction
   virtual void print_fun_syntax(ostream & out,
                                 const function_info & info) const;

   /// overloaded FunctionGroup::print_fun_syntax()
   /// @param out  output stream to print to
   /// @param info function info entry describing the subfunction
      virtual void print_map_syntax(ostream & out,
                                 const function_info & info) const;

   /// do eval_B(B);
   /// @param B      right argument APL value
   /// @param depth  remaining nesting depth for random values
   /// @param budget max. total (recursive) element count for this
   ///               sub-value; negative means unlimited. Bounds the
   ///               *whole* nested (sub-)tree, unlike desired_max_ecount
   ///               which only bounds a single (top-level) ravel.
   Value_P do_eval_B(const cValue & B, int depth, ShapeItem budget) const;

   /// initialize the next ravel cell of \b Z with a random nested value
   /// @param budget max. total (recursive) element count for the nested
   ///               sub-value; negative means unlimited
   void random_nested(Value & Z, const cValue & B, int depth,
                      ShapeItem budget) const;

   /// generate a value that conforms to \b Bref: either a scalar or a
   /// value with exactly Bref's shape (chosen at random), using the
   /// currently configured type/depth/ecount settings. This is what a
   /// stim of "~B" in rval.def selects, so that A and B (or the axis X)
   /// can be generated to actually fit each other instead of
   /// independently, per SVN r2037 discussion.
   /// @param Bref the value to conform to
   Value_P conform_value(const cValue & Bref) const;

   /// choose an integer value at random according to distribution \b dist
   static int choose_integer(const vector<int> & dist);

   /// do eval_AB(A, B);
   /// @param A integer subfunction selector
   /// @param B right argument APL value
   static Value_P do_eval_AB(int A, const cValue & B);

   /// set or return the state of the random generator
   static Value_P generator_state(const cValue & B);

   /// return a 17-bit random number from random()
   static uint64_t rand17();

   /// initialize the next ravel cell of \b Z with a random character
   static void random_character(Value & Z);

   /// initialize the next ravel cell of \b Z with a random complex number
   static void random_complex(Value & Z);

   /// initialize the next ravel cell of \b Z with a random float
   static void random_float(Value & Z);

   /// a random 64-bit IEEE floating point number
   static double random_ieee();

   /// initialize the next ravel cell of \b Z with a random integer
   static void random_integer(Value & Z);

   /// set or return the desired max. depth of random numbers
   static Value_P result_maxdepth(const cValue & B);

   /// set or return the max. element count (⍴Z) per random value (0 = unlimited)
   static Value_P result_ecount(const cValue & B);

   /// set the purpose (0 or 1) of subsequent random values, return the previous one
   static Value_P result_purpose(const cValue & B);

   /// return the 113×5 nested character matrix of primitive arities/stimuli/constraints
   static Value_P prim_table_value(const cValue & B);

   /// set or return the desired rank of random numbers
   static Value_P result_rank(const cValue & B);

   /// set or return the desired ranks of random numbers
   static Value_P result_shape(const cValue & B);

   /// set or return the desired types of random numbers
   static Value_P result_type(const cValue & B);

   /// a mapping between function names and function numbers
   static const FunctionGroup::function_info subfunction_infos[];

   /// the number of bytes in the state of the random number generator
   static size_t N;

   /// the desired rank of random values
   static vector<int> desired_ranks;

   /// the desired rank of random values
   static Shape desired_shape;

   /// the desired types (or a distribution of types) of random values
   static vector<int> desired_types;

   /// the desired limit on the depths of the random values
   static int desired_maxdepth;

   /// max. element count (⍴Z) per value; 0 = unlimited
   static ShapeItem desired_max_ecount;

   /// the purpose of the random values (8 ⎕RVAL B): 0 (default) or 1
   static int desired_purpose;

   /// the state buffer of the random number generator
   static char state[256];
};
//════════════════════════════════════════════════════════════════════════════

#endif // __Quad_RVAL_DEFINED__

