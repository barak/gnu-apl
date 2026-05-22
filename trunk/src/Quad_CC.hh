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

#ifndef __Quad_CC_DEFINED__
#define __Quad_CC_DEFINED__

#include <string.h>

#include "QuadFunction.hh"
#include "UCS_string.hh"

//---------------------------------------------------------------------------
/** base class for all character classes */
class CC_base
{
public:
   /// the character classes
   enum CharClass
        {
          QCC_DIGITS    =   1,   ///< 0-9
          QCC_ALPHA     =   2,   ///< A-Z
          QCC_alpha     =   3,   ///< a-x
          QCC_ASCII     =   4,   ///< 0-127
          QCC_SUPER     =   5,   ///< ²³¹ʲᵏᵐᵗ⁰ⁱ⁴⁵⁶⁷⁸⁹⁺⁻⁼⁽⁾ⁿ
          QCC_SUB       =   6,   ///< ᵢ₀₁₂₃₄₅₆₇₈₉₊₋₌₍₎ₖₘₙⱼ
          QCC_BOX       =   7,   ///< ┌─┬─┐╒═╤═╕╓─╥─╖╔═╦═╗...
          QCC_DIGITS_8  =   8,   ///< 0-7
          QCC_MATH      =   9,   ///< 
          QCC_DIGITS_10 =  10,   ///< 0-9
          QCC_DIGITS_AF =  16,   ///< 0-9 A-F
          QCC_DIGITS_af = -16,   ///< 0-9 a-f
          QCC_DIGITS_Af =  17,   ///< 0-9 A-F a-f
          QCC_CHAR_AZ   =  26,   ///< A-Z
          QCC_CHAR_az   = -26,   ///< a-z
          QCC_BASE_32   =  33,   ///< RFC 4648
          QCC_GREEK     =  48,   ///< Α-Ω α-ω
          QCC_CHAR_Az   =  52,   ///< A-Z a-z
          QCC_BASE_64   =  65,   ///< RFC 4648
          QCC_PRINT     =  95,   ///< printable 0x20 ... 0x7E
          QCC_CHAR_128  = 128,   ///< ASCII
        };

   /// check that get_shape() matches get_ravel().
   void check() const;

   /// return \b true if get_ravel() contains uni
   /// @param uni Unicode code point to search for
   bool contains(Unicode uni);

   /// return the shape of the result
   virtual Shape get_shape() const          = 0;

   /// return the ravel of the result
   virtual const char * get_ravel() const   = 0;

   /// return \b true if \b uni belongs to this character class
   virtual bool contains(Unicode uni) const = 0;

   /// @param cls character class identifier
   static const CC_base & get_instance(CharClass cls);
};
//---------------------------------------------------------------------------
/* character class octal digits (0..9) */
class CC_OCTAL : public CC_base
{
public:
   /// constructor
   CC_OCTAL() { check(); }

   static const CC_OCTAL instance;

protected:
   virtual Shape get_shape() const          { return Shape(8); }
   virtual const char * get_ravel() const   { return "01234567"; }
   virtual bool contains(Unicode uni) const
      { return UNI_0 <= uni && uni <= UNI_7; }
};
//---------------------------------------------------------------------------
/* character class decimal digits (0..9) */
class CC_DECIMAL : public CC_base
{
public:
   /// constructor
   CC_DECIMAL() { check(); }

   static const CC_DECIMAL instance;

protected:
   virtual Shape get_shape() const          { return Shape(10); }
   virtual const char * get_ravel() const   { return "0123456789"; }
   virtual bool contains(Unicode uni) const
      { return UNI_0 <= uni && uni <= UNI_9; }
};
//---------------------------------------------------------------------------
/* character class UPPERCASE hexadecimal digits (0..9) */
class CC_HEXA : public CC_base
{
public:
   /// constructor
   CC_HEXA() { check(); }

   static const CC_HEXA instance;

protected:
   virtual Shape get_shape() const          { return Shape(16); }
   virtual const char * get_ravel() const   { return "0123456789ABCDEF"; }
   virtual bool contains(Unicode uni) const
      { return (UNI_0 <= uni && uni <= UNI_9) ||
               (UNI_A <= uni && uni <= UNI_F); }
};
//---------------------------------------------------------------------------
/* character class lowercase hexadecimal digits (0..9) */
class CC_hexa : public CC_base
{
public:
   /// constructor
   CC_hexa() { check(); }

   static const CC_hexa instance;

protected:
   virtual Shape get_shape() const          { return Shape(16); }
   virtual const char * get_ravel() const   { return "0123456789abcdef"; }
   virtual bool contains(Unicode uni) const
      { return (UNI_0 <= uni && uni <= UNI_9) ||
               (UNI_a <= uni && uni <= UNI_f); }
};
//---------------------------------------------------------------------------
/* character class ANYcase hexadecimal digits (0..9) */
class CC_Hexa : public CC_base
{
public:
   /// constructor
   CC_Hexa() { check(); }

   static const CC_Hexa instance;

protected:
   virtual Shape get_shape() const          { return Shape(22); }
   virtual const char * get_ravel() const   { return "0123456789"
                                                     "ABCDEFabcdef"; }
   virtual bool contains(Unicode uni) const
      { return (UNI_0 <= uni && uni <= UNI_9) ||
               (UNI_A <= uni && uni <= UNI_F) ||
               (UNI_a <= uni && uni <= UNI_f); }
};
//---------------------------------------------------------------------------
/* character class ANYcase hexadecimal digits (0..9) */
class CC_SUPERSCRIPT : public CC_base
{
public:
   /// constructor
   CC_SUPERSCRIPT() { check(); }

   static const CC_SUPERSCRIPT instance;

protected:
   virtual Shape get_shape() const          { return Shape(21); }
   virtual const char * get_ravel() const   { return "²³¹ʲᵏᵐᵗ⁰ⁱ⁴⁵⁶⁷⁸⁹⁺⁻⁼⁽⁾ⁿ"; }
   virtual bool contains(Unicode uni) const;
};
//---------------------------------------------------------------------------
/* character class ANYcase hexadecimal digits (0..9) */
class CC_SUBSCRIPT : public CC_base
{
public:
   /// constructor
   CC_SUBSCRIPT() { check(); }

   static const CC_SUBSCRIPT instance;

protected:
   virtual Shape get_shape() const          { return Shape(20); }
   virtual const char * get_ravel() const   { return "ᵢ₀₁₂₃₄₅₆₇₈₉₊₋₌₍₎ₖₘₙⱼ"; }
   virtual bool contains(Unicode uni) const;
};
//---------------------------------------------------------------------------
/* character class UPPERCASE alphabetical chars (A-Z) */
class CC_ALPHA : public CC_base
{
public:
   /// constructor
   CC_ALPHA() { check(); }

   static const CC_ALPHA instance;

protected:
   virtual Shape get_shape() const          { return Shape(26); }
   virtual const char * get_ravel() const   { return "ABCDEFGHIJKLM"
                                                     "NOPQRSTUVWXYZ"; }
   virtual bool contains(Unicode uni) const
      { return UNI_A <= uni && uni <= UNI_Z; }
};
//---------------------------------------------------------------------------
/* character class lowercase alphabetical chars (a-z) */
class CC_alpha : public CC_base
{
public:
   /// constructor
   CC_alpha() { check(); }

   static const CC_alpha instance;

protected:
   virtual Shape get_shape() const          { return Shape(26); }
   virtual const char * get_ravel() const   { return "abcdefghijklm"
                                                     "nopqrstuvwxyz"; }
   virtual bool contains(Unicode uni) const
      { return UNI_a <= uni && uni <= UNI_z; }
};
//---------------------------------------------------------------------------
/* character class ANYcase alphabetical chars (A-Z, a-z) */
class CC_GREEK : public CC_base
{
public:
   /// constructor
   CC_GREEK() { check(); }

   static const CC_GREEK instance;

protected:
   virtual Shape get_shape() const          { return Shape(48); }
   virtual const char * get_ravel() const   { return "ΑΒΓΔΕΖΗΘΙΚΛΜ"
                                                     "ΝΞΟΠΡΣΤΥΦΧΨΩ"
                                                     "αβγδεζηθικλμ"
                                                     "νξοπρστυφχψω"; }
   virtual bool contains(Unicode uni) const
      { return (0x0391 <= uni && uni <= 0x03A9 && uni != 0x03A2) ||
               (0x03B1 <= uni && uni <= 0x03C9 && uni != 0x03C2); }
};
//---------------------------------------------------------------------------
/* character class ANYcase alphabetical chars (A-Z, a-z) */
class CC_ASCII : public CC_base
{
public:
   /// constructor
   CC_ASCII() { check(); }

   static const CC_ASCII instance;

protected:
   virtual Shape get_shape() const          { return Shape(127); }
   virtual const char * get_ravel() const   { return 0; }
   virtual bool contains(Unicode uni) const
      { return (0 <= uni && uni <= 127); }
};
//---------------------------------------------------------------------------
/* character class ANYcase alphabetical chars (A-Z, a-z) */
class CC_Alpha : public CC_base
{
public:
   /// constructor
   CC_Alpha() { check(); }

   static const CC_Alpha instance;

protected:
   virtual Shape get_shape() const          { return Shape(52); }
   virtual const char * get_ravel() const   { return "ABCDEFGHIJKLM"
                                                     "NOPQRSTUVWXYZ"
                                                     "abcdefghijklm"
                                                     "nopqrstuvwxyz"; }
   virtual bool contains(Unicode uni) const
      { return (UNI_A <= uni && uni <= UNI_Z) ||
               (UNI_a <= uni && uni <= UNI_z); }
};
//---------------------------------------------------------------------------
/* character class ANYcase alphabetical chars (A-Z, a-z) */
class CC_LINE_DRAWING: public CC_base
{
public:
   /// constructor
   CC_LINE_DRAWING() { check(); }

   static const CC_LINE_DRAWING instance;

protected:
   virtual Shape get_shape() const          { return Shape(6, 10); }
   virtual const char * get_ravel() const { return "┌─┬─┐╒═╤═╕"
                                                   "├─┼─┤╞═╪═╡"
                                                   "└─┴─┘╘═╧═╛"
                                                   "╓─╥─╖╔═╦═╗"
                                                   "╟─╫─╢╠═╬═╣"
                                                   "╙─╨─╜╚═╩═╝"
; }
   virtual bool contains(Unicode uni) const;
};
//---------------------------------------------------------------------------
/* character class ANYcase alphabetical chars (A-Z, a-z) */
class CC_MATH: public CC_base
{
public:
   /// constructor
   CC_MATH() { check(); }

   static const CC_MATH instance;

protected:
   virtual Shape get_shape() const          { return Shape(4, 7); }
   virtual const char * get_ravel() const   { return "⎲⎛⎞⎡⎤⎧⎫"
                                                     "⎳⎜⎟⎢⎥⎨⎬"
                                                     "↔⎝⎠⎣⎦⎮⎮"
                                                      "ℕℤℚℝℂ⎩⎭"; }
   virtual bool contains(Unicode uni) const;
};
//---------------------------------------------------------------------------
/* character class ANYcase alphabetical chars (A-Z, a-z) */
class CC_BASE_32 : public CC_base
{
public:
   /// constructor
   CC_BASE_32() { check(); }

   static const CC_BASE_32 instance;

protected:
   virtual Shape get_shape() const          { return Shape(33); }
   virtual const char * get_ravel() const   { return "ABCDEFGHIJKLMNOP"
                                                     "QRSTUVWXYZ234567="; }
   virtual bool contains(Unicode uni) const
      { return (UNI_A <= uni && uni <= UNI_Z) ||
               strchr("234567=", int(uni)); }
};
//---------------------------------------------------------------------------
/* character class ANYcase alphabetical chars (A-Z, a-z) */
class CC_BASE_64 : public CC_base
{
public:
   /// constructor
   CC_BASE_64() { check(); }

   static const CC_BASE_64 instance;

protected:
   virtual Shape get_shape() const          { return Shape(65); }
   virtual const char * get_ravel() const   { return "ABCDEFGHIJKLMNOP"
                                                     "QRSTUVWXYZabcdef"
                                                     "ghijklmnopqrstuv"
                                                     "wxyz0123456789+/="; }
   virtual bool contains(Unicode uni) const
      { return (UNI_A <= uni && uni <= UNI_Z) ||
               (UNI_a <= uni && uni <= UNI_z) ||
               (UNI_0 <= uni && uni <= UNI_9) ||
               strchr("+/=", int(uni)); }
};
//---------------------------------------------------------------------------
/* character class ANYcase alphabetical chars (A-Z, a-z) */
class CC_PRINT : public CC_base
{
public:
   /// constructor
   CC_PRINT() { check(); }

   static const CC_PRINT instance;

protected:
   virtual Shape get_shape() const          { return Shape(95); }
   virtual const char * get_ravel() const   { return " !\"#$%&'()*+,-./"
                                                     "0123456789:;<=>?"
                                                     "@ABCDEFGHIJKLMNO"
                                                     "PQRSTUVWXYZ[\\]^_"
                                                     "`abcdefghijklmno"
                                                     "pqrstuvwxyz{|}~"; }

   virtual bool contains(Unicode uni) const
      { return 0x20 <= uni && uni <= 0x7E; }
};
//===========================================================================
class Quad_CC : public QuadFunction
{
public:
   /// Constructor.
   Quad_CC()
      : QuadFunction(TOK_Quad_CC)
   {}

   static Quad_CC  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB()
   /// @param A left argument APL value (character class selector)
   /// @param B right argument APL value (characters to test)
   Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B()
   /// @param B right argument APL value (character class selector)
   Token eval_B(Value_P B) const;

   /// print the classes that were implementee
   /// @param out output stream to write to
   static void print_classes(ostream & out);

   /// return the characters of class \b cls
   /// @param num character class identifier
   static Value_P get_character_class(CC_base::CharClass num);

   /// retur true if \b is contained in character class \b cls
   /// @param uni Unicode code point to test
   /// @param num character class identifier
   static bool contained_in(Unicode uni, CC_base::CharClass num);
};
//---------------------------------------------------------------------------

#endif // __Quad_CC_DEFINED__
// EOF
