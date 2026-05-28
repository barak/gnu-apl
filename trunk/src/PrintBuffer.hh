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

#ifndef __PRINT_BUFFER_HH__DEFINED__
#define __PRINT_BUFFER_HH__DEFINED__

#include "Avec.hh"
#include "Common.hh"
#include "PrintContext.hh"
#include "PrintOperator.hh"
#include "Shape.hh"
#include "UCS_string_vector.hh"

class Value;
class Cell;

//----------------------------------------------------------------------------

/// some information about a column to be printed.
class ColInfo
{
public:
   /// properties of an empty column.
   ColInfo()
   : flags(0),
     int_len(0),
     fract_len(0),
     real_len(0),
     imag_len(0),
     denom_len(0)
   {}

   /// update the properties of \b this ColInfo according to
   /// the properties of \b other
   /// @param other ColInfo whose properties are merged into this
   void consider(const ColInfo & other);

   /// true if \b this ColInfo has an exponent field
   bool have_expo() const
      { return denom_len == 0 && real_len > (int_len + fract_len); }

   /// the total length
   int total_len() const
      { return real_len + imag_len; }

   /// some flags
   int flags;

   /// the length of the integral part (of numbers) or total length
   /// I.e. an optional leading sign and the digits before the '.'
   int int_len;

   /// the length of the fraction part (of numbers)
   /// I.e. the leading dot and the digits left of an 'E'
   int fract_len;

   /// the length of the real part (of complex numbers) or total length
   int real_len;

   /// the length of the imaginary part (of complex numbers)
   int imag_len;

   /// the length of the denominator (for rational numbers)
   int denom_len;
};
//----------------------------------------------------------------------------
/// A two-dimensional Unicode character buffer
class PrintBuffer
{
public:
   /// contructor: empty PrintBuffer
   PrintBuffer();

   /// contructor: a single row PrintBuffer
   /// @param ucs the Unicode string forming the single row
   /// @param ci column properties for this row
   PrintBuffer(const UCS_string & ucs, const ColInfo & ci);

   /// contructor: a PrintBuffer from an APL value
   /// @param value APL value to render
   /// @param pctx print context controlling formatting
   /// @param out optional output stream for interruptible printing
   PrintBuffer(const Value & value, const PrintContext & pctx, ostream * out);

   /// helper for non-trivial PrintBuffer(const Value & ...) constructor.
   /// return \b true iff the user has hit ^C twice
   /// @param value APL value to render
   /// @param pctx print context controlling formatting
   /// @param out optional output stream for interruptible printing
   /// @param outer_style print style of the enclosing context
   /// @param item_matrix pre-allocated matrix of per-item PrintBuffers
   bool do_PrintBuffer(const Value & value,const PrintContext & pctx,
                         ostream * out, PrintStyle outer_style,
                         PrintBuffer * item_matrix);

   /// PrintBuffer from an APL value in function-style
   /// @param value APL value to render
   /// @param pctx print context controlling formatting
   /// @param outer_style print style of the enclosing context
   void pb_for_function(const Value & value, PrintContext pctx,
                        PrintStyle outer_style);

   /// PrintBuffer from an empty APL value
   /// @param value empty APL value to render
   /// @param pctx print context controlling formatting
   /// @param outer_style print style of the enclosing context
   void pb_empty(const Value & value, PrintContext pctx,
                        PrintStyle outer_style);

   /// return the number of rows
   int get_row_count() const
      { return buffer.size(); }

   /// return the first (and only) line
   UCS_string l1() const
      { Assert(get_row_count() == 1);   return buffer[0]; }

   /// return line y
   /// @param y row index (0-based)
   UCS_string get_line(int y) const
      { Assert(y < get_row_count());   return buffer[y]; }

   /// print this buffer, interruptible with ^C
   /// @param out destination output stream
   /// @param rank rank of the value being printed
   /// @param quad_pw current value of ⎕PW (print width)
   void print_interruptible(ostream & out, sRank rank, int quad_pw);

   /// return the number of columns
   int get_column_count() const
      { return get_column_count(0); }

   /// return the number of columns in row y
   /// @param y row index (0-based)
   int get_column_count(int y) const
      { if (get_row_count() == 0)   return 0;   // no rows
        Assert(y < get_row_count());   return buffer[y].size(); }

   /// Set the char in column x and row y to uc.
   /// @param x column index (0-based)
   /// @param y row index (0-based)
   /// @param uc Unicode character to set
   void set_char(int x, int y, Unicode uc);

   /// Return the char in column x and row y.
   /// @param x column index (0-based)
   /// @param y row index (0-based)
   Unicode get_char(int x, int y) const;

   /// prepend buffer with \b count characters \b pad
   /// @param pad Unicode pad character
   /// @param count number of pad characters to prepend
   void pad_l(Unicode pad, ShapeItem count);

   /// append count charactera \b pad  to buffer
   /// @param pad Unicode pad character
   /// @param count number of pad characters to append
   void pad_r(Unicode pad, ShapeItem count);

   /// append lines to reach height
   /// @param pad Unicode pad character for new rows
   /// @param height desired minimum row count
   void pad_height(Unicode pad, ShapeItem height);

   /// prepend lines to reach height
   /// @param pad Unicode pad character for new rows
   /// @param height desired minimum row count
   void pad_height_above(Unicode pad, ShapeItem height);

   /// replace pad chars by spaces
   void pad_to_spaces();

   /// add a decorator frame around this buffer
   /// @param style print style determining frame characters
   /// @param shape shape of the APL value being framed
   /// @param depth nesting depth for the frame label
   void add_frame(PrintStyle style, const Shape & shape, int depth);

   /// add an outer frame around this buffer
   /// @param style print style determining frame characters
   void add_outer_frame(PrintStyle style);

   /// print properties of \b this PrintBuffer
   /// @param out destination output stream
   /// @param title optional label printed before the dump
   ostream & debug(ostream & out, const char * title = 0) const;

   /// append PrintBuffer \b pb1 to \b this PrintBuffer
   /// @param pb1 PrintBuffer to append as a new column
   void append_col(const PrintBuffer & pb1);

   /// append UCS_string \b ucs to \b this PrintBuffer
   /// @param ucs Unicode string to append as a new row
   void append_ucs(const UCS_string & ucs);

   /// append ucs, aligned at char \b align.
   /// @param ucs Unicode string to append
   /// @param align alignment character used to line up columns
   void append_aligned(const UCS_string & ucs, Unicode align);

   /// add padding and a column \b pb to \b this PrintBuffer
   /// @param pad Unicode pad character between columns
   /// @param pad_count number of padding characters to insert
   /// @param pb PrintBuffer column to append
   void add_column(Unicode pad, int32_t pad_count, const PrintBuffer & pb);

   /// add PrintBuffer \b row to \b this PrintBuffer
   /// @param row PrintBuffer to append as the next row(s)
   void add_row(const PrintBuffer & row);

   /// add empty rows to \b this PrintBuffer
   /// @param count number of empty rows to append
   void add_empty_rows(ShapeItem count)
      { loop(c, count)   buffer.push_back(UCS_string()); }

   /// return the ColInfo of \b this PrintBuffer
   const ColInfo & get_info() const   { return col_info; }

   /// update col_info after a change of buffer
   void update_info()/// append UCS_string \b ucs to \b this PrintBuffer
      { if (buffer.size())   col_info.int_len =
                             col_info.real_len = buffer[0].size(); }

   /// return the ColInfo of \b this PrintBuffer
   ColInfo & get_info() { return col_info; }

   /// return true iff all strings have the same size.
   bool is_rectangular() const;

   /// return the characters for style \b pst
   /// @param pst print style selecting the frame character set
   /// @param HORI receives horizontal border character
   /// @param VERT receives vertical border character
   /// @param NW receives north-west corner character
   /// @param NE receives north-east corner character
   /// @param SE receives south-east corner character
   /// @param SW receives south-west corner character
   static void get_frame_chars(PrintStyle pst, Unicode & HORI, Unicode & VERT,
                       Unicode & NW, Unicode & NE, Unicode & SE, Unicode & SW);

protected:
   /// align this PrintBuffer to col
   /// @param col target column properties to align to
   void align(ColInfo & col);

   /// align this char-only PrintBuffer to col
   /// @param col target column properties to align to
   void align_left(const ColInfo & col);

   /// align this real-only PrintBuffer to col
   /// @param col target column properties to align to
   void align_dot(const ColInfo & col);

   /// align this complex PrintBuffer to col
   /// @param col target column properties to align to
   void align_j(const ColInfo & col);

   /// pad the fraction part to \b wanted_fract_len with '0'
   /// @param wanted_fract_len target length of the fractional part
   /// @param want_expo true if exponential notation is used
   void pad_fraction(int wanted_fract_len, bool want_expo);

   /// return the number of separator rows before row \b y in a value with
   /// shape \b shape
   /// @param y row index within the value's last-but-one axis
   /// @param value APL value being rendered
   /// @param nested true if the value contains nested sub-values
   /// @param rk1 rank label for the current row
   /// @param rk2 rank label for the following row
   static ShapeItem separator_rows(ShapeItem y, const Value & value,
                                   bool nested, sRank rk1, sRank rk2);

   /// the character buffer.
   UCS_string_vector buffer;

   /// column properties
   ColInfo col_info;

   /// true if completely constructed (as opposed to interrupted by ^C)
   bool complete;
};

#endif // __PRINT_BUFFER_HH__DEFINED__
