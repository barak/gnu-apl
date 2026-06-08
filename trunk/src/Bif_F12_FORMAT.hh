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

#ifndef __BIF_F12_FORMAT_HH_DEFINED__
#define __BIF_F12_FORMAT_HH_DEFINED__

#include "PrimitiveFunction.hh"

//════════════════════════════════════════════════════════════════════════════
/** A helper struct for Bif_F12_FORMAT
 It represents a sub-field (int part, fract part, or exponent)
 of the format field
 */
/// A helper for Bif_F12_FORMAT (sizes of the fields in a number)
struct Format_sub
{
   Format_sub()
   : out_len(0),
     flt_mask(0),
     min_len(0)
   {}

   /// return true if the decorator is floating floating
   /// @param value_is_negative true if the formatted number is negative
   bool do_float(bool value_is_negative) const
      { if (flt_mask & BIT_1)   return  value_is_negative;
        if (flt_mask & BIT_2)   return !value_is_negative;
        return true;
      }

   /// return true iff format contains a '4' (i.e. counteract the effect of
   /// 1, 2, or 3) or none of 1 (float if negative), 2 (float if positive),
   /// or 3 (always float) was given
   bool no_float() const
        { if (flt_mask & BIT_4)   return true;        // counteract 1, 2, or 3
          return ! (flt_mask & (BIT_1 | BIT_2 | BIT_3));   // no 1, 2, or 3
        }

   /// return the pad character (default: space, otherwise controlled by ⎕FC)
   /// @param qfc current ⎕FC fill character
   Unicode pad_char(Unicode qfc) const
      { return flt_mask & BIT_8 ? qfc : UNI_SPACE; }

   /// return the number of characters in \b format
   size_t size() const
      { return format.size(); }

   /// Fill buf at position x,y with data according to fmt
   /// @param data digit string for the fractional part
   UCS_string insert_fract_commas(const UCS_string & data) const;

   /// Fill buf at position x,y with data according to fmt
   /// @param data digit string for the integer part
   /// @param overflow output flag set if the field is too narrow
   UCS_string insert_int_commas(const UCS_string & data,
                                bool & overflow) const;

   /// set flt_mask according to the digits in member \b format
   /// return the number of digits from '1' to '4' (including)
   /// @param type sub-field type code (integer, fraction, or exponent)
   int map_field(int type);

   /// a debug function for printing \b this Format_sub
   /// @param out output stream to write to
   ostream & print(ostream & out) const;

   UCS_string      format;         ///< the format ('0'...'9' and comma)
   int             out_len;        ///< the length in the output
   uint32_t        flt_mask;       ///< decorator floating mode
   int             min_len;        ///< the minimum length in the output
};
//════════════════════════════════════════════════════════════════════════════
/** System function format
 */
/// The class implementing ⍕
class Bif_F12_FORMAT : public NonscalarFunction
{
public:
   /// constructor
   Bif_F12_FORMAT()
   : NonscalarFunction(TOK_F12_FORMAT)
   {}

   /// An entire format field (LIFER = Left-Int-Fract-Expo-Right
   struct Format_LIFER
      {
        /// empty constructor
        Format_LIFER() {}

        /// constructor
        /// @param fmt format string to parse into sub-fields
        Format_LIFER(UCS_string fmt);

        /// return the size of the format
        size_t format_size() const
           { return left_deco.size() + int_part.size() + fract_part.size()
                  + expo_deco.size() + exponent.size() + right_deco.size(); }

        /// return the size of the output
        size_t out_size() const
           { return left_deco.out_len + int_part.out_len + fract_part.out_len
                  + expo_deco.out_len + exponent.out_len + right_deco.out_len; }

        /// format \b value by example
        /// @param value floating-point number to format
        UCS_string format_example(APL_Float value);

        /// fill data fields from value
        /// @param value floating-point number to decompose
        /// @param data_int output string for the integer digits
        /// @param data_fract output string for the fractional digits
        /// @param data_expo output string for the exponent digits
        /// @param overflow output flag set if the value exceeds field width
        void fill_data_fields(APL_Float value, UCS_string & data_int,
                              UCS_string & data_fract, UCS_string & data_expo,
                              bool & overflow);

        /// format the left decorator and integer part (everything left of the
        /// decimal dot)
        /// @param data_int digit string for the integer part
        /// @param negative true if the value is negative
        /// @param overflow output flag set if the field is too narrow
        UCS_string format_left_side(const UCS_string data_int, bool negative,
                                    bool & overflow);

        /// format the fract part, exponent, and right decorator (everything
        /// left of the decimal dot)
        /// @param data_fract digit string for the fractional part
        /// @param negative true if the value is negative
        /// @param data_expo digit string for the exponent part
        UCS_string format_right_side(const UCS_string data_fract, bool negative,
                                     const UCS_string data_expo);

        Format_sub left_deco;    ///< the left decorator
        Format_sub int_part;     ///< the integer part
        Format_sub fract_part;   ///< the fractional part
        Format_sub expo_deco;    ///< the exponent decorator
        Format_sub exponent;     ///< the exponent
        Format_sub right_deco;   ///< the right decorator

        /// the exponent char to be used
        Unicode exponent_char;

        /// true if the exponent is negative
        bool expo_negative;
      };

   /// A character array with B formatted by specification
   /// @param A format specification APL value (numeric width and precision)
   /// @param B APL value to format
   static Value_P format_by_specification(Value_P A, Value_P B);

   /// Return true iff uni is '0' .. '9', comma, or full-stop
   /// @param uni Unicode character to test
   static bool is_control_char(Unicode uni);

   /// A character array with the display of B
   /// @param B APL value to format
   static Value_P monadic_format(Value_P B);

   static Bif_F12_FORMAT  fun;   ///< Built-in function

protected:
   /// Overloaded Function::eval_B()
   /// @param B right argument APL value
   virtual Token eval_B(Value_P B) const;

   /// Overloaded Function::eval_AB()
   /// @param A left argument APL value (format specification or example)
   /// @param B right argument APL value to format
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// A character array with B formatted by example
   /// @param A format example APL value (character string with picture)
   /// @param B APL value to format
   static Value_P format_by_example(Value_P A, Value_P B);

   /// split entire format string string into \b column format strings
   /// @param format overall format picture string to split
   /// @param col_formats output vector receiving one format string per column
   static void split_example_into_columns(const UCS_string & format,
                                   UCS_string_vector & col_formats);

   /// A character array with one column of B formatted by specification
   /// @param width total output field width in characters
   /// @param precision number of digits after the decimal point
   /// @param cB pointer to the first cell of the column
   /// @param cols total number of columns in the value
   /// @param rows total number of rows in the value
   static PrintBuffer format_one_col_by_spec(int width, int precision,
                                             const Cell * cB, ShapeItem cols,
                                             ShapeItem rows);

   /// add a row (consisting of \b data) to \b PrintBuffer \b ret
   /// @param ret PrintBuffer to append the row to
   /// @param row zero-based index of the row being added
   /// @param has_char true if any cell in the row is a character cell
   /// @param has_num true if any cell in the row is a numeric cell
   /// @param align_char alignment character for mixed char/num columns
   /// @param data formatted string content for this row
   static void add_row(PrintBuffer & ret, int row, bool has_char, bool has_num,
                Unicode align_char, UCS_string & data);

   /// format value with \b precision mantissa digits (floating format)
   /// @param value floating-point number to format
   /// @param precision number of significant mantissa digits
   static UCS_string format_float_by_spec(APL_Float value, int precision);
};
//════════════════════════════════════════════════════════════════════════════

#endif // __BIF_F12_FORMAT_HH_DEFINED__
