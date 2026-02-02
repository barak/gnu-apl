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

#include "Quad_CC.hh"
#include "UCS_string.hh"
#include "UTF8_string.hh"

Quad_CC Quad_CC::fun;

// character class instances
//
const CC_OCTAL        CC_OCTAL::       instance;
const CC_DECIMAL      CC_DECIMAL::     instance;
const CC_HEXA         CC_HEXA::        instance;
const CC_hexa         CC_hexa::        instance;
const CC_Hexa         CC_Hexa::        instance;
const CC_SUPERSCRIPT  CC_SUPERSCRIPT:: instance;
const CC_SUBSCRIPT    CC_SUBSCRIPT::   instance;
const CC_ALPHA        CC_ALPHA::       instance;
const CC_alpha        CC_alpha::       instance;
const CC_GREEK        CC_GREEK::       instance;
const CC_Alpha        CC_Alpha::       instance;
const CC_LINE_DRAWING CC_LINE_DRAWING::instance;
const CC_MATH         CC_MATH::        instance;
const CC_BASE_32      CC_BASE_32::     instance;
const CC_BASE_64      CC_BASE_64::     instance;
const CC_PRINT        CC_PRINT::       instance;
const CC_ASCII        CC_ASCII::       instance;

//---------------------------------------------------------------------------
void
CC_base::check() const
{
   if (this == &CC_ASCII::instance)   return;

   // check shape vs. ravel length
   //
const UTF8_string utf(get_ravel());
const UCS_string ucs(utf);
   Assert1(ucs.ssize() == get_shape().get_volume());

   // check that all chars can be found
   //
   loop(u, ucs.size())
       {
         const Unicode uni = ucs[u];
         Assert(contains(uni) == true);
       }

   // check that the char below the smalles and above the largest can not
   // be found. This check is somewhat incomplete because some classes
   // have gaps.
   //
Unicode mini = Unicode(0xFFFF);
Unicode maxi = Unicode(0x0000);
   loop(u, ucs.size())
       {
         const Unicode uni = ucs[u];
         if (mini > uni)   mini = uni;
         if (maxi < uni)   maxi = uni;
       }

   Assert(contains(Unicode(mini - 1)) == false);
   Assert(contains(Unicode(maxi + 1)) == false);
}
//---------------------------------------------------------------------------
bool
CC_base::contains(Unicode uni) const
{
const UCS_string ucs(uni);
   loop(u, ucs.size())   if (uni == ucs[u])   return true;
   return false;   // not found
}
//---------------------------------------------------------------------------
const CC_base &
CC_base::get_instance(CharClass cls)
{
   switch(cls)
      {
        case QCC_DIGITS:    return CC_DECIMAL::instance;
        case QCC_ALPHA:     return CC_ALPHA::instance;
        case QCC_alpha:     return CC_alpha::instance;
        case QCC_ASCII:     return CC_ASCII::instance;
        case QCC_SUPER:     return CC_SUPERSCRIPT::instance;
        case QCC_SUB:       return CC_SUBSCRIPT::instance;
        case QCC_BOX:       return CC_LINE_DRAWING::instance;
        case QCC_DIGITS_8:  return CC_OCTAL::instance;
        case QCC_MATH:      return CC_MATH::instance;
        case QCC_DIGITS_10: return CC_DECIMAL::instance;
        case QCC_DIGITS_AF: return CC_HEXA::instance;
        case QCC_DIGITS_af: return CC_hexa::instance;
        case QCC_DIGITS_Af: return CC_Hexa::instance;
        case QCC_CHAR_AZ:   return CC_ALPHA::instance;
        case QCC_CHAR_az:   return CC_alpha::instance;
        case QCC_GREEK:     return CC_GREEK::instance;
        case QCC_CHAR_Az:   return CC_Alpha::instance;
        case QCC_BASE_32:   return CC_BASE_32::instance;
        case QCC_BASE_64:   return CC_BASE_64::instance;
        case QCC_PRINT:     return CC_PRINT::instance;
        case QCC_CHAR_128:  return CC_ASCII::instance;
      }

   MORE_ERROR() << "⎕CC B: invalid character class B (= " << cls << ")";
   DOMAIN_ERROR;
}
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
               const CC_base::CharClass cc =
                     CC_base::CharClass(B->get_cravel(b).get_int_value());
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
   if (B->get_rank() > 1)         RANK_ERROR;
   if (B->element_count() == 0)
      {
        print_classes(CERR);
        return Token();
      }

   if (B->is_scalar())
      {
        const CC_base::CharClass b =
              CC_base::CharClass(B->get_cfirst().get_int_value());
        Value_P Z = get_character_class(b);
        return Token(TOK_APL_VALUE1, Z);
      }

Value_P Z(B->get_shape(), LOC);
   loop(b, B->element_count())
       {
         const CC_base::CharClass bb =
               CC_base::CharClass(B->get_cravel(b).get_int_value());
        Value_P ZZ = get_character_class(bb);
        Z->next_ravel_Pointer(ZZ.get());
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//---------------------------------------------------------------------------
void
Quad_CC::print_classes(ostream & out)
{
   out << "      ⎕CC   1:  Digits 0-9 (same as ⎕CC 10)"                << endl
       << "      ⎕CC   2:  UPPERCASE CHARACTERS A-Z (same as ⎕CC 26)"  << endl
       << "      ⎕CC   3:  lowercase characters a-z (same as ⎕CC ¯26)" << endl
       << "      ⎕CC   4:  ASCII characters (same as ⎕CC 127)"         << endl
       << "      ⎕CC   5:  Superscripts ( ²³¹ʲᵏᵐᵗ⁰ⁱ⁴⁵⁶⁷⁸⁹⁺⁻⁼⁽⁾ⁿ )"     << endl
       << "      ⎕CC   6:  subscripts ( ᵢ₀₁₂₃₄₅₆₇₈₉₊₋₌₍₎ₖₘₙⱼ )"        << endl
       << "      ⎕CC   7:  Line drawing chatacters"                    << endl
       << "      ⎕CC   8:  Octal digits 0-7"                           << endl
       << "      ⎕CC   9:  Some mathematical symbols"                  << endl
       << "      ⎕CC  10:  Decimal digits 0-9"                         << endl
       << "      ⎕CC  16:  UPPERCASE HEXADECIMAL DIGITS 0-9 A-F"       << endl
       << "      ⎕CC ¯16:  lowercase hexadecimal digits 0-9 a-f"       << endl
       << "      ⎕CC  17:  Hexadecimal Digits 0-9 A-F a-f"             << endl
       << "      ⎕CC  26:  UPPERCASE CHARACTERS A-Z"                   << endl
       << "      ⎕CC ¯26:  lowercase characters a-z"                   << endl
       << "      ⎕CC  33:  base32 encoding (RFC 4648)"                 << endl
       << "      ⎕CC  48:  greek characters A-Ω α-ω"                   << endl
       << "      ⎕CC  52:  characters A-Z a-z"                         << endl
       << "      ⎕CC  65:  base64 encoding (RFC 4648)"                 << endl
       << "      ⎕CC  95:  printable characters 0x20 ... 0x7E"         << endl
       << "      ⎕CC 128:  ASCII characters"                           << endl;
}
//---------------------------------------------------------------------------
Value_P
Quad_CC::get_character_class(CC_base::CharClass cls)
{
   // ASCII is special because it contains 0 as valid character
   //
   if (cls == CC_base::QCC_ASCII || cls == CC_base::QCC_CHAR_128)
      {
        Value_P Z(128, LOC);
        loop(a, 128)   Z->next_ravel_Char(Unicode(a));
        Z->check_value(LOC);
        return Z;
      }

const CC_base & inst = CC_base::get_instance(cls);

const UTF8_string utf(inst.get_ravel());
const UCS_string ucs(utf);
Value_P Z(inst.get_shape(), LOC);
   loop(z, Z->element_count())   Z->next_ravel_Char(ucs[z]);
   Z->check_value(LOC);
   return Z;
}
//---------------------------------------------------------------------------
bool
Quad_CC::contained_in(Unicode uni, CC_base::CharClass cls)
{
const CC_base & inst = CC_base::get_instance(cls);
   return inst.contains(uni);
}
//---------------------------------------------------------------------------
bool
CC_SUPERSCRIPT::contains(Unicode uni) const
{
   if (uni < 178)    return false;
   if (uni > 8319)   return false;
   return CC_base::contains(uni);
}
//---------------------------------------------------------------------------
bool
CC_SUBSCRIPT::contains(Unicode uni) const
{
   if (uni < 7522)    return false;
   if (uni > 11388)   return false;
   return CC_base::contains(uni);
}
//---------------------------------------------------------------------------
bool
CC_LINE_DRAWING::contains(Unicode uni) const
{
   if (uni < 9472)   return false;
   if (uni > 9580)   return false;
   return CC_base::contains(uni);
}
//---------------------------------------------------------------------------
bool
CC_MATH::contains(Unicode uni) const
{
   if (uni < 8450)   return false;
   if (uni > 9139)   return false;
   return CC_base::contains(uni);
}
//---------------------------------------------------------------------------
