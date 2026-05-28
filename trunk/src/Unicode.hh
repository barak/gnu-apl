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

#ifndef __UNICODE_HH_DEFINED__
#define __UNICODE_HH_DEFINED__

#ifndef __COMMON_HH_DEFINED__
#  error This file shall NOT be #included directly, but by #including Common.hh
#endif

#include "Common.hh"

/// One Unicode character
enum Unicode
{
#define char_def(name, uni, _tag, _flags, _av_pos) UNI_ ## name = uni,
#define char_uni(name, uni, _tag, _flags)          UNI_ ## name = uni,
#include "Avec.def"

   Unicode_0       = 0,            ///< End of unicode string
   Invalid_Unicode = 0x55AA55AA,   ///< An invalid Unicode.

   // cursor control characters for line editing
   //
   UNI_EOF         = -1,
   UNI_CursorUp    = -2,
   UNI_CursorDown  = -3,
   UNI_CursorRight = -4,
   UNI_CursorLeft  = -5,
   UNI_CursorEnd   = -6,
   UNI_CursorHome  = -7,
   UNI_InsertMode  = -8,

   /// internal pad characters - will be removed or replaced before
   /// the value is printed.
   //
#ifdef cfg_VISIBLE_MARKERS_WANTED
   UNI_iPAD_U0       = UNI_PAD_U0,   // not (yet) a pad char
   UNI_PAD_r_NOTCHAR = UNI_PAD_U1,   // after NOTCHAR column
   UNI_iPAD_U2       = UNI_PAD_U2,   // not (yet) a pad char. Used in ⎕AV!
   UNI_PAD_l_NOTCHAR = UNI_PAD_U3,   // before NOTCHAR column
   UNI_PAD_l_VALUE   = UNI_PAD_U4,   // value separator
   UNI_PAD_r_VALUE   = UNI_PAD_U5,   // value separator
   UNI_PAD_b_ROW     = UNI_PAD_U6,   // pad to max_row_height
   UNI_PAD_r_MAX     = UNI_PAD_U7,   // pad to max_spacing
   UNI_PAD_l_DEPTH   = UNI_PAD_U8,   // depth indicator
   UNI_PAD_r_DEPTH   = UNI_PAD_U9,   // depth indicator

   UNI_PAD_y_AXIS    = UNI_PAD_L0,   // interdimensional spacing (rank > 2)
   UNI_PAD_r_oCol    = UNI_PAD_L1,   // pad new line to column width
   UNI_PAD_r_nCol    = UNI_PAD_L2,   // pad old line to column width
   UNI_PAD_l_INT     = UNI_PAD_L3,   // pad integer part
   UNI_PAD_r_FRACT   = UNI_PAD_L4,   // pad fractional part
   UNI_PAD_l_STRING  = UNI_PAD_L5,   // pad strings
   UNI_iPAD_L9       = UNI_PAD_L9,   // not (yet) a pad char
#else
   UNI_iPAD_U2       = 0xEEEE,
   UNI_PAD_l_NOTCHAR = 0xEEEF,
   UNI_PAD_r_NOTCHAR = 0xEEF0,

   UNI_iPAD_U0       = 0xEEF1,
   UNI_PAD_l_VALUE   = 0xEEF2,
   UNI_PAD_r_VALUE   = 0xEEF3,
   UNI_PAD_b_ROW     = 0xEEF4,
   UNI_PAD_r_MAX     = 0xEEF5,
   UNI_PAD_l_DEPTH   = 0xEEF6,
   UNI_PAD_r_DEPTH   = 0xEEF7,

   UNI_PAD_y_AXIS    = 0xEEF8,
   UNI_PAD_r_oCol    = 0xEEF9,
   UNI_PAD_r_nCol    = 0xEEFA,
   UNI_PAD_l_INT     = 0xEEFB,
   UNI_PAD_r_FRACT   = 0xEEFC,
   UNI_PAD_l_STRING  = 0xEEFD,
   UNI_iPAD_L9       = 0xEEFE,
#endif

   // aliases for Diffout.cc (used in testcase files)
   //
   UNI_DIFF_DIGITS   = UNI_PAD_U0,   ///< ⁰: digits (integer)
   UNI_DIFF_SPACES   = UNI_PAD_U1,   ///< ¹: blanks
   UNI_DIFF_REAL     = UNI_PAD_U2,   ///< ²: real number
   UNI_DIFF_ANY      = UNI_PAD_U3,   ///< ³: anything
   UNI_DIFF_OVERBAR  = UNI_PAD_U4,   ///< ⁴: optional ¯
   UNI_DIFF_SIGN     = UNI_PAD_U5,   ///< ⁵: ASCII sign (+ or -)
   UNI_DIFF_CR28_29  = UNI_PAD_U6,   ///< ⁶: obsolete
   UNI_DIFF_MULT     = UNI_PAD_Un,   ///< ⁿ: unit multiplier (m, n, u, μ)
};

/// value 0-15 of hex digit, or -1 if uni not in "023456789ABCDEFabcdef"
/// @param uni Unicode character to convert
extern int nibble(Unicode uni);

/// value 0-63 of base64 digit, or -1 if uni not base64 (RFC 4648)
/// @param uni Unicode character to convert
extern int sixbit(Unicode uni);

//----------------------------------------------------------------------------
/// return true iff \b uni is a padding character (used internally).
/// @param uni Unicode character to test
inline bool is_iPAD_char(Unicode uni)
{
   return ((uni > UNI_iPAD_U2) && (uni <= UNI_PAD_r_NOTCHAR))    // ³ ¹
       || ((uni >= UNI_iPAD_U0) && (uni <= UNI_iPAD_L9));   // ⁰ ⁴..⁹ ₀..₉
}
//----------------------------------------------------------------------------

#endif // __UNICODE_HH_DEFINED__
