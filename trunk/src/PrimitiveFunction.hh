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

#ifndef __PRIMITIVE_FUNCTION_HH_DEFINED__
#define __PRIMITIVE_FUNCTION_HH_DEFINED__

#include "Common.hh"
#include "Function.hh"
#include "Performance.hh"
#include "Value.hh"
#include "Id.hh"

class ArrayIterator;
class CharCell;
class CollatingCache;
class ConstRavel_P;
class IntCell;

//----------------------------------------------------------------------------
/**
    Base class for the APL system functions (Quad functions and primitives
    like +, -, ...) and operators

    The individual system functions are derived from this class
 */
/// Base class for all internal functions of the interpreter
class PrimitiveFunction : public Function
{
public:
   /// Construct a PrimitiveFunction with \b TokenTag \b tag
   /// @param tag      the token tag identifying this primitive
   /// @param stat_AB  optional dyadic cell-function statistics object
   /// @param stat_B   optional monadic cell-function statistics object
   PrimitiveFunction(TokenTag tag,
                     CellFunctionStatistics * stat_AB = 0,
                     CellFunctionStatistics * stat_B = 0)
   : Function(tag),
       statistics_AB(stat_AB),
       statistics_B(stat_B)
   {}

   /// overloaded Function::has_result()
   virtual bool has_result() const   { return true; }

   /// return the dyadic cell statistics of \b this (scalar) function
   CellFunctionStatistics *
   get_statistics_AB() const   { return statistics_AB; }

   /// return the monadic cell statistics of \b this (scalar) function
   CellFunctionStatistics *
   get_statistics_B() const   { return statistics_B; }

   /// overloaded Function::eval_fill_AB()
   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   virtual Token eval_fill_AB(Value_P A, Value_P B) const;

protected:
   /// overloaded Function::print_properties()
   /// @param out     output stream for property display
   /// @param indent  indentation level for nested output
   virtual void print_properties(ostream & out, int indent) const;

   /// overloaded Function::eval_fill_B()
   /// @param B  the right APL argument value
   virtual Token eval_fill_B(Value_P B) const;

   /// Print the name of \b this PrimitiveFunction to \b out
   /// @param out  output stream to print the function name to
   virtual ostream & print(ostream & out) const;

   /// performance statistics for eval_B()
   CellFunctionStatistics * statistics_AB;

   /// performance statistics for dyadic calls
   CellFunctionStatistics * statistics_B;
};
//----------------------------------------------------------------------------
/// Base class for all internal non-scalar functions of the interpreter
class NonscalarFunction : public PrimitiveFunction
{
public:
   /// Constructor
   /// @param tag  the token tag identifying this non-scalar function
   NonscalarFunction(TokenTag tag)
   : PrimitiveFunction(tag)
   {}
};
//----------------------------------------------------------------------------
/// Base class for all internal non-scalar functions of the interpreter
// that have the default identity function
class NonscalarFunction_default_identity : public NonscalarFunction
{
public:
   /// Constructor
   /// @param tag  the token tag identifying this function
   NonscalarFunction_default_identity(TokenTag tag)
   : NonscalarFunction(tag)
   {}

   /// Overloaded Function::eval_identity_fun()
   /// @param B     the right APL argument value
   /// @param axis  the axis along which to apply the identity
   virtual Token eval_identity_fun(Value_P B, sAxis axis) const;

   /// implementation of eval_identity_fun(), so that non-derived functions
   /// may use it as well.
   /// @param B     the right APL argument value
   /// @param axis  the axis along which to apply the identity
   static Token do_eval_identity_fun(Value_P B, sAxis axis);
};
//----------------------------------------------------------------------------
/** System function zilde (⍬) */
/// The class implementing ⍬ (the empty numeric vector)
class Bif_F0_ZILDE : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F0_ZILDE()
   : NonscalarFunction(TOK_F0_ZILDE)
   {}

   static Bif_F0_ZILDE  fun;   ///< Built-in function

   /// overladed Function::eval_()
   virtual Token eval_() const;

protected:
   /// overladed Function::may_push_SI()
   virtual bool may_push_SI() const   { return false; }
};
//----------------------------------------------------------------------------
/** System function execute */
/// The class implementing ⍎
class Bif_F1_EXECUTE : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F1_EXECUTE()
   : NonscalarFunction(TOK_F1_EXECUTE)
   {}

   static Bif_F1_EXECUTE  fun;   ///< Built-in function

   /// execute string containing an APL expression or an APL command
   /// @param statement  the APL expression or command text to execute
   static Token execute_statement(UCS_string & statement);

   /// execute string containing an APL command
   /// @param command  the APL command text to execute
   static Token execute_command(UCS_string & command);

   /// overladed Function::eval_B()
   /// @param B  the right APL argument value
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_fill_B()
   /// @param B  the right APL argument value
   virtual Token eval_fill_B(Value_P B) const;

   /// the number of outstanding )COPYs with APL scipts
   static int copy_pending;

protected:
   /// overladed Function::may_push_SI()
   virtual bool may_push_SI() const   { return true; }
};
//----------------------------------------------------------------------------
/** System function index (⌷) */
/// The class implementing ⌷
class Bif_F2_INDEX : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F2_INDEX()
   : NonscalarFunction(TOK_F2_INDEX)
   {}

   /// overloaded Function::eval_AB()
   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_AXB()
   /// @param A  the left APL argument value
   /// @param X  the axis specification value
   /// @param B  the right APL argument value
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   static Bif_F2_INDEX  fun;   ///< Built-in function
protected:
};
//----------------------------------------------------------------------------
/** primitive functions member and enlist */
/// The class implementing ϵ
class Bif_F12_ELEMENT : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_ELEMENT()
   : NonscalarFunction(TOK_F12_ELEMENT)
   {}

   /// overloaded Function::eval_B()
   /// @param B  the right APL argument value
   virtual Token eval_B(Value_P B) const
      { return Token(TOK_APL_VALUE1, do_eval_B(B.get())); }

   /// overloaded Function::eval_AB()
   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   virtual Token eval_AB(Value_P A, Value_P B) const;

   static Bif_F12_ELEMENT  fun;   ///< Built-in function

   /// implementation of eval_B()
   /// @param B  the right APL value (raw pointer)
   static Value_P do_eval_B(const Value * B);

protected:
};
//----------------------------------------------------------------------------
/** primitive functions match and depth */
/// The class implementing ≡
class Bif_F12_EQUIV : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_EQUIV()
   : NonscalarFunction(TOK_F12_EQUIV)
   {}

   /// overloaded Function::eval_B() : B
   /// @param B  the right APL argument value
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_AB() : A ≡ B
   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   virtual Token eval_AB(Value_P A, Value_P B) const
      { return Token(TOK_APL_VALUE1,
                     IntScalar((do_eval_AB(A, B) ? 1 : 0), LOC)); }

   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   static bool do_eval_AB(Value_P A, Value_P B);

   static Bif_F12_EQUIV  fun;   ///< Built-in function

protected:
   /// return the depth of B
   /// @param B  the APL value whose depth to compute
   Token depth(Value_P B);
};
//----------------------------------------------------------------------------
/** primitive function natch (≢) */
/// The class implementing ≡
class Bif_F12_NEQUIV : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_NEQUIV()
   : NonscalarFunction(TOK_F12_NEQUIV)
   {}

   /// overloaded Function::eval_B()
   /// @param B  the right APL argument value
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_AB()
   /// @param A  the left APL argument value
   /// @param B  the right APL argument value
   virtual Token eval_AB(Value_P A, Value_P B) const;

   static Bif_F12_NEQUIV  fun;   ///< Built-in function
};
//----------------------------------------------------------------------------
/** System function encode */
/// The class implementing ⊤
class Bif_F12_ENCODE : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_ENCODE()
   : NonscalarFunction(TOK_F12_ENCODE)
   {}

   /// overloaded Function::eval_AB()
   /// @param A  the left APL argument value (number system bases)
   /// @param B  the right APL argument value (numbers to encode)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_AXB()
   /// @param A  the left APL argument value (number system bases)
   /// @param X  the axis specification value
   /// @param B  the right APL argument value (numbers to encode)
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   static Bif_F12_ENCODE  fun;   ///< Built-in function

protected:
   /// return the (minimum) number of digits needed to represent every
   /// item in B in a number system with base A0.
   /// @param A0  the radix (base) of the number system
   /// @param B   the APL value containing the numbers to represent
   static int get_X0(APL_Integer A0, const Value & B);

   /// encode *ib() according to A (integer A and b)
   /// @param Z   the output value to fill with encoded digits
   /// @param aH  high dimension of the A argument (number of digits)
   /// @param aL  low dimension of the A argument (number of elements)
   /// @param iA  ravel iterator over the bases array
   /// @param iB  ravel iterator over the numbers to encode
   static void encode_Int(Value & Z, ShapeItem aH, ShapeItem aL,
                          const ConstRavel_P & iA,
                          const ConstRavel_P & iB);

   /// encode *ib() according to A
   /// @param Z    the output value to fill with encoded digits
   /// @param ah   high dimension of the A argument
   /// @param al   low dimension of the A argument
   /// @param iA   ravel iterator over the bases array
   /// @param iB   ravel iterator over the numbers to encode
   /// @param qct  comparison tolerance for floating-point equality
   static void encode_Flt(Value & Z, ShapeItem ah, ShapeItem al,
                          const ConstRavel_P & iA, const ConstRavel_P & iB,
                         double qct);

   /// encode *ib() according to A
   /// @param Z    the output value to fill with encoded digits
   /// @param aH   high dimension of the A argument
   /// @param aL   low dimension of the A argument
   /// @param iA   ravel iterator over the bases array
   /// @param iB   ravel iterator over the numbers to encode
   /// @param qct  comparison tolerance for floating-point equality
   static void encode_Cpx(Value & Z, ShapeItem aH, ShapeItem aL,
                          const ConstRavel_P & iA, const ConstRavel_P & iB,
                          double qct);
};
//----------------------------------------------------------------------------
/** System function decode */
/// The class implementing ⊥
class Bif_F12_DECODE : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_DECODE()
   : NonscalarFunction(TOK_F12_DECODE)
   {}

   /// overloaded Function::eval_AB()
   /// @param A  the left APL argument value (number system bases)
   /// @param B  the right APL argument value (digit vectors to decode)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   static Bif_F12_DECODE  fun;   ///< Built-in function

protected:
   /// decode B according to len_A and cA (integer A, B and Z)
   /// @param Z      the output value receiving decoded integers
   /// @param len_A  number of elements in the bases array
   /// @param cA     pointer to the first cell of the bases array
   /// @param len_B  number of digit-vectors in B
   /// @param cB     pointer to the first cell of B
   /// @param dB     stride between successive digit-vectors in B
   static bool decode_int(Value & Z, ShapeItem len_A, const Cell * cA,
                          ShapeItem len_B, const Cell * cB, ShapeItem dB);

   /// decode B according to len_A and cA (real A and B)
   /// @param Z      the output value receiving decoded reals
   /// @param len_A  number of elements in the bases array
   /// @param cA     pointer to the first cell of the bases array
   /// @param len_B  number of digit-vectors in B
   /// @param cB     pointer to the first cell of B
   /// @param dB     stride between successive digit-vectors in B
   static void decode_real(Value & Z, ShapeItem len_A, const Cell * cA,
                           ShapeItem len_B, const Cell * cB, ShapeItem dB);

   /// decode B according to len_A and cA (complex A or B)
   /// @param Z      the output value receiving decoded complex numbers
   /// @param len_A  number of elements in the bases array
   /// @param cA     pointer to the first cell of the bases array
   /// @param len_B  number of digit-vectors in B
   /// @param cB     pointer to the first cell of B
   /// @param dB     stride between successive digit-vectors in B
   static void decode_complex(Value & Z, ShapeItem len_A, const Cell * cA,
                              ShapeItem len_B, const Cell * cB, ShapeItem dB);
};
//----------------------------------------------------------------------------
/** primitive functions rotate and reverse */
/// Base class for implementing ⌽ and ⊖
class Bif_ROTATE : public NonscalarFunction_default_identity
{
public:
   /// Constructor.
   /// @param tag  the token tag identifying this rotate/reverse function
   Bif_ROTATE(TokenTag tag)
   : NonscalarFunction_default_identity(tag)
   {}

protected:
   /// Rotate B according to A along axis
   /// @param A     the left APL argument value (rotation amounts)
   /// @param B     the right APL argument value (array to rotate)
   /// @param axis  the axis along which to rotate
   static Token rotate(Value_P A, Value_P B, sAxis axis);

   /// Reverse B along axis
   /// @param B     the right APL argument value (array to reverse)
   /// @param axis  the axis along which to reverse
   static Token reverse(Value_P B, sAxis axis);
};
//----------------------------------------------------------------------------
/** primitive functions rotate and reverse along last axis */
/// The class implementing ⌽
class Bif_F12_ROTATE : public Bif_ROTATE
{
public:
   /// Constructor
   Bif_F12_ROTATE()
   : Bif_ROTATE(TOK_F12_ROTATE)
   {}

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const
      { return reverse(B, B->get_rank() - 1); }

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const
      { return rotate(A, B, B->get_rank() - 1); }

   /// overloaded Function::eval_XB()
   /// @param X  the axis specification value
   /// @param B  the right APL argument value
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// overloaded Function::eval_AXB()
   /// @param A  the left APL argument value (rotation amounts)
   /// @param X  the axis specification value
   /// @param B  the right APL argument value
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   static Bif_F12_ROTATE  fun;   ///< Built-in function
protected:
};
//----------------------------------------------------------------------------
/** primitive functions rotate and reverse along first axis */
/// The class implementing ⊖
class Bif_F12_ROTATE1 : public Bif_ROTATE
{
public:
   /// Constructor
   Bif_F12_ROTATE1()
   : Bif_ROTATE(TOK_F12_ROTATE1)
   {}

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const
      { return reverse(B, 0); }

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const
      { return rotate(A, B, 0); }

   /// overloaded Function::eval_XB()
   /// @param X  the axis specification value
   /// @param B  the right APL argument value
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// overloaded Function::eval_AXB()
   /// @param A  the left APL argument value (rotation amounts)
   /// @param X  the axis specification value
   /// @param B  the right APL argument value
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   static Bif_F12_ROTATE1  fun;   ///< Built-in function
protected:
};
//----------------------------------------------------------------------------
/** System function transpose */
/// The class implementing ⍉
class Bif_F12_TRANSPOSE : public NonscalarFunction_default_identity
{
public:
   /// Constructor
   Bif_F12_TRANSPOSE()
   : NonscalarFunction_default_identity(TOK_F12_TRANSPOSE)
   {}

   /// overloaded Function::eval_B()
   /// @param B  the right APL argument value
   virtual Token eval_B(Value_P B) const
      { return do_eval_B(B.get()); }

   /// overloaded Function::eval_AB()
   /// @param A  the left APL argument value (axis permutation vector)
   /// @param B  the right APL argument value (array to transpose)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   static Bif_F12_TRANSPOSE  fun;   ///< Built-in function

   /// Transpose B according to axes A (without diagonals)
   /// @param A  the permutation shape specifying new axis order
   /// @param B  the APL array to transpose (raw pointer)
   static Value_P transpose(const Shape & A, const Value * B);

   /// implementation of eval_B()
   /// @param B  the APL array to reverse-transpose (raw pointer)
   static Token do_eval_B(const Value * B);

protected:
   /// Transpose B according to axes A (with diagonals)
   /// @param A  the permutation shape (may map multiple axes to one)
   /// @param B  the APL array to transpose (raw pointer)
   static Value_P transpose_diag(const Shape & A, const Value * B);

   /// for \b sh being a permutation of 0, 1, ... rank - 1,
   /// return the inverse permutation sh⁻¹
   /// @param sh  the permutation shape to invert
   static Shape inverse_permutation(const Shape & sh);

   /// return sh permuted according to permutation perm
   /// @param sh    the shape to permute
   /// @param perm  the permutation to apply
   static Shape permute(const Shape & sh, const Shape & perm);
};
//----------------------------------------------------------------------------
/** primitive functions reshape and shape */
/// The class implementing ⍴
class Bif_F12_RHO : public NonscalarFunction_default_identity
{
public:
   /// Constructor
   Bif_F12_RHO()
   : NonscalarFunction_default_identity(TOK_F12_RHO)
   {}

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// Reshape B according to rank and shape
   static Token do_reshape(const Shape & shape, const Value & B);

   static Bif_F12_RHO  fun;   ///< Built-in function
protected:
};
//----------------------------------------------------------------------------
/** System function ∪ (unique/union) */
/// The class implementing ∪
class Bif_F12_UNION : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F12_UNION()
   : NonscalarFunction(TOK_F12_UNION)
   {}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const;

   /// Built-in function
   static Bif_F12_UNION  fun;
};
//----------------------------------------------------------------------------
/** System function ∩ (intersection) */
/// The class implementing ∩
class Bif_F2_INTER : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F2_INTER()
   : NonscalarFunction(TOK_F2_INTER)
   {}

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const;

   static Bif_F2_INTER  fun;   ///< Built-in function

protected:
};
//----------------------------------------------------------------------------
/** System function left (⊣) */
/// The class implementing ⊣
class Bif_F2_LEFT : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F2_LEFT()
   : NonscalarFunction(TOK_F2_LEFT)
   {}

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const
      { return Token(TOK_APL_VALUE2, IntScalar(0, LOC)); }

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const
      { return Token(TOK_APL_VALUE1, A->clone(LOC)); }

   static Bif_F2_LEFT  fun;   ///< Built-in function
protected:
};
//----------------------------------------------------------------------------
/** System function right (⊢) */
/// The class implementing ⊢
class Bif_F2_RIGHT : public NonscalarFunction
{
public:
   /// Constructor
   Bif_F2_RIGHT()
   : NonscalarFunction(TOK_F2_RIGHT)
   {}

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const
      { return Token(TOK_APL_VALUE1, B->clone(LOC)); }

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const
      { return Token(TOK_APL_VALUE1, B->clone(LOC)); }

   /// overloaded Function::eval_AXB()
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   static Bif_F2_RIGHT  fun;   ///< Built-in function
protected:
};
//----------------------------------------------------------------------------

#endif // __PRIMITIVE_FUNCTION_HH_DEFINED__
