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

#ifndef __BIF_OPER2_RANK_HH_DEFINED__
#define __BIF_OPER2_RANK_HH_DEFINED__

#include "PrimitiveOperator.hh"

//────────────────────────────────────────────────────────────────────────────
/** Primitive operator ⍤ (rank)
 */
/// The class implementing ⍤[X] (NARS only) and ⍤.
class Bif_OPER2_RANK : public PrimitiveOperator
{
public:
   /// constructor
   Bif_OPER2_RANK() : PrimitiveOperator(TOK_OPER2_RANK) {}

   /// split strand y_B into vector y and B
   /// @param RO_B  combined strand containing rank vector and right argument
   /// @param RO    output: extracted rank vector
   /// @param B     output: extracted right argument value
   static void unstrand_RO_B(Value_P RO_B, Value_P & RO, Value_P & B);

   static Bif_OPER2_RANK  fun;      ///< Built-in function

protected:
   /// overloaded Function::may_push_SI()
   virtual bool may_push_SI() const
      { return false; }

   /// overloaded Function::eval_ALRB()
   /// @param A   left value argument
   /// @param LO  left operator function argument
   /// @param RO  right operator function argument (rank vector)
   /// @param B   right value argument
   virtual Token eval_ALRB(Value_P A, Token & LO, Token & RO, Value_P B) const;

   /// overloaded Function::eval_ALRXB()
   /// @param A   left value argument
   /// @param LO  left operator function argument
   /// @param RO  right operator function argument (rank vector)
   /// @param X   axis specification
   /// @param B   right value argument
   virtual Token eval_ALRXB(Value_P A, Token & LO, Token & RO,
                            Value_P X, Value_P B) const;

   /// overloaded Function::eval_LRB()
   /// @param LO  left operator function argument
   /// @param RO  right operator function argument (rank vector)
   /// @param B   right value argument
   virtual Token eval_LRB(Token & LO, Token & RO, Value_P B) const;

   /// overloaded Function::eval_LRXB()
   /// @param LO  left operator function argument
   /// @param RO  right operator function argument (rank vector)
   /// @param X   axis specification
   /// @param B   right value argument
   virtual Token eval_LRXB(Token & LO, Token & RO, Value_P X, Value_P B) const;

   /// is the workhorse of dyadic A ⍤ B.
   /// @param A          left value argument
   /// @param rk_chunk_A  chunk rank for left argument
   /// @param LO         left operator function argument
   /// @param X          axis specification value
   /// @param B          right value argument
   /// @param rk_chunk_B  chunk rank for right argument
   static Token do_ALyXB(Value_P A, sRank rk_chunk_A, Token & LO,
                         Value_P X, Value_P B, sRank rk_chunk_B);

   /// the 'normalized' implementation of all eval_Lxxx*( functions. This
   /// is the workhorse of monadic ⍤ B.
   /// @param LO        left operator function argument
   /// @param X         axis specification value
   /// @param B         right value argument
   /// @param rk_chunkB  chunk rank for right argument
   static Token do_LyXB(Token & LO, Value_P X, Value_P B, sRank rk_chunkB);

   /// convert 1- 2- or 3-element vector y123 to chunk-rank of B
   /// @param y123  1-, 2-, or 3-element rank specification vector
   /// @param rk_B  actual rank of the right argument B
   static sRank y123_to_chunk_B_rank(Value_P y123, sRank rk_B);

   /// convert 1- 2- or 3-element vector y123 to chunk-ranks of A and B
   /// @param y123  1-, 2-, or 3-element rank specification vector
   /// @param rk_A  output: chunk rank for left argument A
   /// @param rk_B  output: chunk rank for right argument B
   static void y123_to_AB(Value_P y123, sRank & rk_A, sRank & rk_B);
};
//────────────────────────────────────────────────────────────────────────────

#endif // __BIF_OPER2_RANK_HH_DEFINED__
