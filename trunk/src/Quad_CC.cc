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

#include "Quad_CC.hh"

Quad_CC Quad_CC::fun;

//---------------------------------------------------------------------------
Token
Quad_CC::eval_AB(Value_P A, Value_P B) const
{
Value_P Z(A->get_shape(), LOC);
   new (&Z->get_wproto())   IntCell(0);   // prototype
 
   loop(a, A->element_count())
       {
         const Unicode uni_a = A->get_cravel(a).get_char_value();
         bool found = false;
         loop(b, B->element_count())
             {
               const CharClass cc = CharClass(B->get_cravel(b).get_int_value());
               if (contained_in(uni_a, cc))
                  {
                    found = true;
                    break;
                  }
             }
         if (found)   Z->next_ravel_1();
         else         Z->next_ravel_0();
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//---------------------------------------------------------------------------
Token
Quad_CC::eval_B(Value_P B) const
{
   if (B->get_rank() > 1)        RANK_ERROR;
   if (B->element_count() == 0)   LENGTH_ERROR;

   if (B->is_scalar())
      {
        const CharClass b = CharClass(B->get_cfirst().get_int_value());
        Value_P Z = get_character_class(b);
        return Token(TOK_APL_VALUE1, Z);
      }

Value_P Z(B->get_shape(), LOC);
   loop(b, B->element_count())
       {
         const CharClass bb = CharClass(B->get_cravel(b).get_int_value());
        Value_P ZZ = get_character_class(bb);
        Z->next_ravel_Pointer(ZZ.get());
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//---------------------------------------------------------------------------
Value_P
Quad_CC::get_character_class(CharClass cls)
{
   switch(cls)
      {
         case QCC_DIGITS:
              {
                Value_P Z(10, LOC);
                loop(z, Z->element_count())  Z->next_ravel_Char(Unicode('0'+z));
                Z->check_value(LOC);
                return Z;
              }

         case QCC_ALPHA:
              {
                Value_P Z(26, LOC);
                loop(z, Z->element_count())  Z->next_ravel_Char(Unicode('A'+z));
                Z->check_value(LOC);
                return Z;
              }

         case QCC_alpha:
              {
                Value_P Z(26, LOC);
                loop(z, Z->element_count())  Z->next_ravel_Char(Unicode('a'+z));
                Z->check_value(LOC);
                return Z;
              }

         case QCC_ASCII:
              {
                Value_P Z(128, LOC);
                loop(z, Z->element_count())   Z->next_ravel_Char(Unicode(z));
                Z->check_value(LOC);
                return Z;
              }

         case QCC_SUPER:
              {
                const UTF8_string utf("²³¹ʲᵏᵐᵗ⁰ⁱ⁴⁵⁶⁷⁸⁹⁺⁻⁼⁽⁾ⁿ");
                const UCS_string ucs(utf);
                return Value_P(ucs, LOC);
              }

         case QCC_SUB:
              {
                const UTF8_string utf("ᵢ₀₁₂₃₄₅₆₇₈₉₊₋₌₍₎ₖₘₙⱼ");
                const UCS_string ucs(utf);
                return Value_P(ucs, LOC);
              }

         case QCC_BOX:
              {
                const UTF8_string utf("┌─┬─┐╒═╤═╕╓─╥─╖╔═╦═╗"
                                      "├─┼─┤╞═╪═╡╟─╫─╢╠═╬═╣"
                                      "└─┴─┘╘═╧═╛╙─╨─╜╚═╩═╝");
                const UCS_string ucs(utf);
                Value_P Z(3, 20, LOC);
                loop(z, Z->element_count())   Z->next_ravel_Char(ucs[z]);
                Z->check_value(LOC);
                return Z;
              }

         case QCC_MATH:
              {
                const UTF8_string utf("⎲⎛⎞⎡⎤⎧⎫"
                                      "⎳⎜⎟⎢⎥⎨⎬"
                                      "↔⎝⎠⎣⎦⎮⎮"
                                      "ℕℤℚℝℂ⎩⎭");
                const UCS_string ucs(utf);
                Value_P Z(4, 7, LOC);
                loop(z, Z->element_count())   Z->next_ravel_Char(ucs[z]);
                Z->check_value(LOC);
                return Z;
              }
      }

   MORE_ERROR() << "⎕CC B: invalid character class B (= " << cls << ")";
   DOMAIN_ERROR;
}
//---------------------------------------------------------------------------
bool
Quad_CC::contained_in(Unicode uni, CharClass cls)
{
   switch(cls)
      {
         case QCC_DIGITS:  return uni >= UNI_0 && uni <= UNI_9;
         case QCC_ALPHA:   return uni >= UNI_A && uni <= UNI_Z;
         case QCC_alpha:   return uni >= UNI_a && uni <= UNI_z;
         case QCC_ASCII:   return uni >= 0     && uni <= 0x7F;
         case QCC_SUPER:
              if (uni < 178 || uni > 8319)   return false;
              {
                const UTF8_string utf("²³¹ʲᵏᵐᵗ⁰ⁱ⁴⁵⁶⁷⁸⁹⁺⁻⁼⁽⁾ⁿ");
                const UCS_string ucs(utf);
                loop(u, ucs.size())   if (ucs[u] == uni)   return true;
                return false;
              }

         case QCC_SUB:
              if (uni < 7522 || uni > 11388)   return false;
              {
                const UTF8_string utf("ᵢ₀₁₂₃₄₅₆₇₈₉₊₋₌₍₎ₖₘₙⱼ");
                const UCS_string ucs(utf);
                loop(u, ucs.size())   if (ucs[u] == uni)   return true;
                return false;
              }

         case QCC_BOX:
              if (uni < 9472 || uni > 9580)   return false;
              {
                const UTF8_string utf("┌─┬─┐╒═╤═╕╓─╥─╖╔═╦═╗"
                                      "├─┼─┤╞═╪═╡╟─╫─╢╠═╬═╣"
                                      "└─┴─┘╘═╧═╛╙─╨─╜╚═╩═╝");
                const UCS_string ucs(utf);
                loop(u, ucs.size())   if (ucs[u] == uni)   return true;
                return false;
              }

         case QCC_MATH:
              if (uni < 8450 || uni > 9139)   return false;
              {
                const UTF8_string utf("⎲⎛⎞⎡⎤⎧⎫"
                                      "⎳⎜⎟⎢⎥⎨⎬"
                                      "↔⎝⎠⎣⎦⎮⎮"
                                      "ℕℤℚℝℂ⎩⎭");
                const UCS_string ucs(utf);
                loop(u, ucs.size())   if (ucs[u] == uni)   return true;
                return false;
              }
      }

   MORE_ERROR() << "A ⎕CC B: invalid character class B (= " << cls << ")";
   DOMAIN_ERROR;
}
//---------------------------------------------------------------------------
