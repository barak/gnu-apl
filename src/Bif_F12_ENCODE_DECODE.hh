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

#ifndef __BIF_F12_ENCODE_DECODE_HH_DEFINED__
#define __BIF_F12_ENCODE_DECODE_HH_DEFINED__

#include "Common.hh"
#include "PrimitiveFunction.hh"

class ConstRavel_P;

//────────────────────────────────────────────────────────────────────────────
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
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_AXB()
   /// @param A  the left APL argument value (number system bases)
   /// @param X  the axis specification value
   /// @param B  the right APL argument value (numbers to encode)
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   static Bif_F12_ENCODE  fun;   ///< Built-in function

protected:
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

   /// encode *ib() according to A (integer A and b)
   /// @param Z   the output value to fill with encoded digits
   /// @param aH  high dimension of the A argument (number of digits)
   /// @param aL  low dimension of the A argument (number of elements)
   /// @param iA  ravel iterator over the bases array
   /// @param iB  ravel iterator over the numbers to encode
   static void encode_Int(Value & Z, ShapeItem aH, ShapeItem aL,
                          const ConstRavel_P & iA,
                          const ConstRavel_P & iB);

   /// return the (minimum) number of digits needed to represent every
   /// item in B in a number system with base A0.
   /// @param A0  the radix (base) of the number system
   /// @param B   the APL value containing the numbers to represent
   static int get_X0(APL_Integer A0, const cValue & B);
};
//────────────────────────────────────────────────────────────────────────────
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
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   static Bif_F12_DECODE  fun;   ///< Built-in function

protected:
   /// decode B according to len_A and cA (complex A or B)
   /// @param Z      the output value receiving decoded complex numbers
   /// @param len_A  number of elements in the bases array
   /// @param cA     pointer to the first cell of the bases array
   /// @param len_B  number of digit-vectors in B
   /// @param cB     pointer to the first cell of B
   /// @param dB     stride between successive digit-vectors in B
   static void decode_complex(Value & Z, ShapeItem len_A,
                              cValue_R VA, ShapeItem idxA,
                              ShapeItem len_B, cValue_R VB, ShapeItem idxB,
                              ShapeItem dB);

   /// decode B according to len_A and cA (integer A, B and Z)
   static bool decode_int(Value & Z, ShapeItem len_A,
                          cValue_R VA, ShapeItem idxA,
                          ShapeItem len_B, cValue_R VB, ShapeItem idxB,
                          ShapeItem dB);

   /// decode B according to len_A and cA (real A and B)
   static void decode_real(Value & Z, ShapeItem len_A,
                           cValue_R VA, ShapeItem idxA,
                           ShapeItem len_B, cValue_R VB, ShapeItem idxB,
                           ShapeItem dB);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __BIF_F12_ENCODE_DECODE_HH_DEFINED__
