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

//════════════════════════════════════════════════════════════════════════════
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
//════════════════════════════════════════════════════════════════════════════
/// A two-dimensional Unicode character buffer
class PrintBuffer
{
public:
   /// maximum nesting depth this class will attempt to print. Printing
   /// a value recurses mutually with PointerCell::character_representation()
   /// once per nesting level, with no depth counter of its own; a
   /// sufficiently deep value (observed at ~11000 on an 8MB stack, far
   /// less on a constrained one) exhausts the C++ call stack with a
   /// hard SIGSEGV instead of a catchable APL error (Blake McBride,
   /// Bugs22 #4). This bound is deliberately conservative -- safely
   /// below that failure threshold even on a small (e.g. 512KB
   /// secondary-thread) stack -- since genuine APL values printed
   /// interactively are essentially never this deep.
   enum { MAX_PRINT_NESTING_DEPTH = 500 };


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
   PrintBuffer(const cValue & value, const PrintContext & pctx, ostream * out);

   /// contructor: a PrintBuffer from an APL value with frames (if any) drawn
   /// with \b frame_chars (see Frame_char) instead of those selected by the
   /// print style in \b pctx. frame_chars plays a similar role for boxed
   /// output as ⎕FC does for numbers.
   /// @param value APL value to render
   /// @param pctx print context controlling formatting
   /// @param frame_chars the lines and decorators of the frames
   /// @param out optional output stream for interruptible printing
   PrintBuffer(const cValue & value, const PrintContext & pctx,
               const UCS_string & frame_chars, ostream * out);

   /// helper for non-trivial PrintBuffer(const cValue & ...) constructor.
   /// return \b true iff the user has hit ^C twice
   /// @param value APL value to render
   /// @param pctx print context controlling formatting
   /// @param out optional output stream for interruptible printing
   /// @param frame_chars the frame characters of value (see Frame_char)
   /// @param item_matrix pre-allocated matrix of per-item PrintBuffers
   bool do_PrintBuffer(const cValue & value,const PrintContext & pctx,
                         ostream * out, const UCS_string & frame_chars,
                         PrintBuffer * item_matrix);

   /// PrintBuffer from an APL value in APL input format (PST_INPUT)
   /// @param value APL value to render
   /// @param frame_chars the frame characters of value (see Frame_char)
   void pb_for_function(const cValue & value, const UCS_string & frame_chars);

   /// append \b value in APL input format to \b ucs: the shortest text
   /// that ⍎ turns into a value identical to \b value (full precision,
   /// independent of ⎕PP), e.g. ⊂1 2, ,'a', or 2 2⍴1 2 3 4.
   static void append_input_form(UCS_string & ucs, const cValue & value);

   /// append the item \b cell of a vector in APL input format to \b ucs,
   /// i.e. as it would be written in a strand, e.g. 5, 'a', or (1 2)
   static void append_input_item(UCS_string & ucs, const Cell & cell);

   /// PrintBuffer from an empty APL value
   /// @param value empty APL value to render
   /// @param pctx print context controlling formatting
   /// @param frame_chars the frame characters of value (see Frame_char)
   void pb_empty(const cValue & value, PrintContext pctx,
                 const UCS_string & frame_chars);

   /// return the number of rows
   int get_row_count() const
      { return buffer.size(); }

   /// return the first (and only) line
   const UCS_string & l1() const
      { Assert(get_row_count() == 1);   return buffer[0]; }

   /// return line y. Returns a reference (not a copy, as it used to):
   /// callers that chunk a long line into ⎕PW-sized pieces call this
   /// once per chunk (PrintBuffer::print_interruptible(),
   /// UCS_string::UCS_string(const PrintBuffer&, ...)), so returning by
   /// value copied the *entire* row on every chunk -- O(row_length ×
   /// chunk_count) copying, and (via repeated huge alloc/free of the
   /// temporaries) the ~100x peak-RSS amplification measured on long
   /// character vectors (Blake McBride, Bugs6 #6).
   /// @param y row index (0-based)
   const UCS_string & get_line(int y) const
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

   /// the positions of the characters in the \b chars argument of
   /// add_frame() and add_outer_frame(): first the 81 line-drawing
   /// characters, then the frame decorators and options.
   ///
   /// A line-drawing character has 4 legs: North, East, South, and West.
   /// In FC_Nn_Ee_Ss_Ww, each of n, e, s, and w is 0 (leg absent), 1 (leg
   /// in line style 1), or 2 (leg in line style 2), and the position is
   /// the base 3 number nesw, i.e. 27n + 9e + 3s + w. Line style 1 draws the
   /// (inner) frames and line style 2 the outer frame.
   ///
   /// A blank character, decorator, or option means: none. Not every
   /// combination of legs exists in Unicode (e.g. a single double leg).
   ///
   /// The comments below are a table of the Unicode characters (U+25xx) at
   /// each position, with one column for each combination of line styles
   /// that Unicode has (· where Unicode has no character):
   ///
   ///     L:  light            (line style 1)
   ///     D:  double           (line style 2)
   ///     LD: light and double (line styles 1 and 2)
   ///     H:  heavy            (line style 2)
   ///     LH: light and heavy  (line styles 1 and 2)
   ///     A:  light arc, i.e. rounded corners (line style 1)
   ///     L2, L3, L4: light double, triple, quadruple dashed (line style 1)
   ///     H2, H3, H4: heavy double, triple, quadruple dashed (line style 2)
   ///
   /// The diagonals ╱╲╳ are the only line-drawing characters of U+25xx
   /// that have no North, East, South, and West legs.

   enum Frame_char
      {                        //   L  D  LD H  LH A  L2 L3 L4 H2 H3 H4
        FC_N0_E0_S0_W0 =  0,   ///<    ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E0_S0_W1 =  1,   ///< ╴  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E0_S0_W2 =  2,   ///< ·  ·  ·  ╸  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E0_S1_W0 =  3,   ///< ╷  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E0_S1_W1 =  4,   ///< ┐  ·  ·  ·  ·  ╮  ·  ·  ·  ·  ·  ·
        FC_N0_E0_S1_W2 =  5,   ///< ·  ·  ╕  ·  ┑  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E0_S2_W0 =  6,   ///< ·  ·  ·  ╻  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E0_S2_W1 =  7,   ///< ·  ·  ╖  ·  ┒  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E0_S2_W2 =  8,   ///< ·  ╗  ·  ┓  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E1_S0_W0 =  9,   ///< ╶  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E1_S0_W1 = 10,   ///< ─  ·  ·  ·  ·  ·  ╌  ┄  ┈  ·  ·  ·
        FC_N0_E1_S0_W2 = 11,   ///< ·  ·  ·  ·  ╾  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E1_S1_W0 = 12,   ///< ┌  ·  ·  ·  ·  ╭  ·  ·  ·  ·  ·  ·
        FC_N0_E1_S1_W1 = 13,   ///< ┬  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E1_S1_W2 = 14,   ///< ·  ·  ·  ·  ┭  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E1_S2_W0 = 15,   ///< ·  ·  ╓  ·  ┎  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E1_S2_W1 = 16,   ///< ·  ·  ╥  ·  ┰  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E1_S2_W2 = 17,   ///< ·  ·  ·  ·  ┱  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E2_S0_W0 = 18,   ///< ·  ·  ·  ╺  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E2_S0_W1 = 19,   ///< ·  ·  ·  ·  ╼  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E2_S0_W2 = 20,   ///< ·  ═  ·  ━  ·  ·  ·  ·  ·  ╍  ┅  ┉
        FC_N0_E2_S1_W0 = 21,   ///< ·  ·  ╒  ·  ┍  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E2_S1_W1 = 22,   ///< ·  ·  ·  ·  ┮  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E2_S1_W2 = 23,   ///< ·  ·  ╤  ·  ┯  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E2_S2_W0 = 24,   ///< ·  ╔  ·  ┏  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E2_S2_W1 = 25,   ///< ·  ·  ·  ·  ┲  ·  ·  ·  ·  ·  ·  ·
        FC_N0_E2_S2_W2 = 26,   ///< ·  ╦  ·  ┳  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E0_S0_W0 = 27,   ///< ╵  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E0_S0_W1 = 28,   ///< ┘  ·  ·  ·  ·  ╯  ·  ·  ·  ·  ·  ·
        FC_N1_E0_S0_W2 = 29,   ///< ·  ·  ╛  ·  ┙  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E0_S1_W0 = 30,   ///< │  ·  ·  ·  ·  ·  ╎  ┆  ┊  ·  ·  ·
        FC_N1_E0_S1_W1 = 31,   ///< ┤  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E0_S1_W2 = 32,   ///< ·  ·  ╡  ·  ┥  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E0_S2_W0 = 33,   ///< ·  ·  ·  ·  ╽  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E0_S2_W1 = 34,   ///< ·  ·  ·  ·  ┧  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E0_S2_W2 = 35,   ///< ·  ·  ·  ·  ┪  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E1_S0_W0 = 36,   ///< └  ·  ·  ·  ·  ╰  ·  ·  ·  ·  ·  ·
        FC_N1_E1_S0_W1 = 37,   ///< ┴  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E1_S0_W2 = 38,   ///< ·  ·  ·  ·  ┵  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E1_S1_W0 = 39,   ///< ├  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E1_S1_W1 = 40,   ///< ┼  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E1_S1_W2 = 41,   ///< ·  ·  ·  ·  ┽  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E1_S2_W0 = 42,   ///< ·  ·  ·  ·  ┟  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E1_S2_W1 = 43,   ///< ·  ·  ·  ·  ╁  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E1_S2_W2 = 44,   ///< ·  ·  ·  ·  ╅  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E2_S0_W0 = 45,   ///< ·  ·  ╘  ·  ┕  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E2_S0_W1 = 46,   ///< ·  ·  ·  ·  ┶  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E2_S0_W2 = 47,   ///< ·  ·  ╧  ·  ┷  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E2_S1_W0 = 48,   ///< ·  ·  ╞  ·  ┝  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E2_S1_W1 = 49,   ///< ·  ·  ·  ·  ┾  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E2_S1_W2 = 50,   ///< ·  ·  ╪  ·  ┿  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E2_S2_W0 = 51,   ///< ·  ·  ·  ·  ┢  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E2_S2_W1 = 52,   ///< ·  ·  ·  ·  ╆  ·  ·  ·  ·  ·  ·  ·
        FC_N1_E2_S2_W2 = 53,   ///< ·  ·  ·  ·  ╈  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E0_S0_W0 = 54,   ///< ·  ·  ·  ╹  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E0_S0_W1 = 55,   ///< ·  ·  ╜  ·  ┚  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E0_S0_W2 = 56,   ///< ·  ╝  ·  ┛  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E0_S1_W0 = 57,   ///< ·  ·  ·  ·  ╿  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E0_S1_W1 = 58,   ///< ·  ·  ·  ·  ┦  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E0_S1_W2 = 59,   ///< ·  ·  ·  ·  ┩  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E0_S2_W0 = 60,   ///< ·  ║  ·  ┃  ·  ·  ·  ·  ·  ╏  ┇  ┋
        FC_N2_E0_S2_W1 = 61,   ///< ·  ·  ╢  ·  ┨  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E0_S2_W2 = 62,   ///< ·  ╣  ·  ┫  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E1_S0_W0 = 63,   ///< ·  ·  ╙  ·  ┖  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E1_S0_W1 = 64,   ///< ·  ·  ╨  ·  ┸  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E1_S0_W2 = 65,   ///< ·  ·  ·  ·  ┹  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E1_S1_W0 = 66,   ///< ·  ·  ·  ·  ┞  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E1_S1_W1 = 67,   ///< ·  ·  ·  ·  ╀  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E1_S1_W2 = 68,   ///< ·  ·  ·  ·  ╃  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E1_S2_W0 = 69,   ///< ·  ·  ╟  ·  ┠  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E1_S2_W1 = 70,   ///< ·  ·  ╫  ·  ╂  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E1_S2_W2 = 71,   ///< ·  ·  ·  ·  ╉  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E2_S0_W0 = 72,   ///< ·  ╚  ·  ┗  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E2_S0_W1 = 73,   ///< ·  ·  ·  ·  ┺  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E2_S0_W2 = 74,   ///< ·  ╩  ·  ┻  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E2_S1_W0 = 75,   ///< ·  ·  ·  ·  ┡  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E2_S1_W1 = 76,   ///< ·  ·  ·  ·  ╄  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E2_S1_W2 = 77,   ///< ·  ·  ·  ·  ╇  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E2_S2_W0 = 78,   ///< ·  ╠  ·  ┣  ·  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E2_S2_W1 = 79,   ///< ·  ·  ·  ·  ╊  ·  ·  ·  ·  ·  ·  ·
        FC_N2_E2_S2_W2 = 80,   ///< ·  ╬  ·  ╋  ·  ·  ·  ·  ·  ·  ·  ·
        // │  │  │  │               │  │  │  │  │  │  │  │  │  │  │  │
        // │  │  │  │     Legs      │  │  │  │  │  │  │  │  │  │  │  │
        // │  │  │  │     ────      │  │  │  │  │  │  │  │  │  │  │  │
        // │  │  │  ╰──── West      │  │  │  │  │  │  │  │  │  │  │  │
        // │  │  ╰─────── South     │  │  │  │  │  │  │  │  │  │  │  │
        // │  ╰────────── East      │  │  │  │  │  │  │  │  │  │  │  │
        // ╰───────────── North     │  │  │  │  │  │  │  │  │  │  │  │
        //                          │  │  │  │  │  │  │  │  │  │  │  │
        //     Styie1 / Style2      │  │  │  │  │  │  │  │  │  │  │  │
        //     ───────────────      │  │  │  │  │  │  │  │  │  │  │  │
        //   ┼ light           ─────┘  │  │  │  │  │  │  │  │  │  │  │
        //   ╬ double          ────────┘  │  │  │  │  │  │  │  │  │  │
        //   ╫ light  / double ───────────┘  │  │  │  │  │  │  │  │  │
        //   ╋ heaavy          ──────────────┘  │  │  │  │  │  │  │  │
        //   ╊ light  / heavy  ─────────────────┘  │  │  │  │  │  │  │
        //     arcs            ────────────────────┘  │  │  │  │  │  │
        //   ┆ light┄          ───────────────────────┘  │  │  │  │  │
        //   ┊ light┈          ──────────────────────────┘  │  │  │  │
        //   ╏ heavy┅          ─────────────────────────────┘  │  │  │
        //   ┇ heavy╍          ────────────────────────────────┘  │  │
        //   ┋  heavy┅         ───────────────────────────────────┘  │
        //     heavy┉          ──────────────────────────────────────┘
        //
        FC_LINES_COUNT  = 81,   ///<the number of line-drawing characters

        // the line-drawing characters of the (inner) frame
        //
        FC_TOP_LEFT     = FC_N0_E1_S1_W0,   ///< top left corner, e.g. ┌
        FC_TOP_RIGHT    = FC_N0_E0_S1_W1,   ///< top right corner, e.g. ┐
        FC_BOTTOM_LEFT  = FC_N1_E1_S0_W0,   ///< bottom left corner, e.g. └
        FC_BOTTOM_RIGHT = FC_N1_E0_S0_W1,   ///< bottom right corner, e.g. ┘
        FC_VERT         = FC_N1_E0_S1_W0,   ///< left and right line, e.g. │
        FC_HORI         = FC_N0_E1_S0_W1,   ///< top and bottom line, e.g. ─

        // the line-drawing characters of the outer frame
        //
        FC_OUTER_TOP_LEFT     = FC_N0_E2_S2_W0,   ///< top left corner, e.g. ╔
                                                  ///< (blank: no outer frame)
        FC_OUTER_TOP_RIGHT    = FC_N0_E0_S2_W2,   ///< top right corner, e.g. ╗
        FC_OUTER_BOTTOM_LEFT  = FC_N2_E2_S0_W0,   ///< bottom left corner, ╚
        FC_OUTER_BOTTOM_RIGHT = FC_N2_E0_S0_W2,   ///< bottom right corner, ╝
        FC_OUTER_VERT         = FC_N2_E0_S2_W0,   ///< left and right line, ║
        FC_OUTER_HORI         = FC_N0_E2_S0_W2,   ///< top and bottom line, ═

        // the frame decorators and options
        //
        FC_RANK_Y       = 81,   ///< rank > 1 (left line): ↓, or # for the
                                ///< axis lengths (NARS)
        FC_RANK_X       = 82,   ///< rank > 0 (top line): →, or # for the
                                ///< axis length (NARS)
        FC_NUMERIC      = 83,   ///< simple numeric (bottom line), e.g. ∼
        FC_MIXED        = 84,   ///< simple mixed (bottom line), e.g. +
        FC_NESTED       = 85,   ///< per depth level (bottom line), e.g. ϵ.
                                ///< Blank: no type decorators at all
        FC_EMPTY_X      = 86,   ///< last axis empty (top line), e.g. ⊖
        FC_EMPTY_Y      = 87,   ///< non-last axis empty (left line), e.g. ⌽
        FC_PARENS       = 88,   ///< ( for parentheses instead of a frame
        FC_DECOR_COUNT  = 8,    ///< the number of decorators and options

        FC_COUNT        = 89    ///< the number of characters
      };

   /// add a decorator frame around this buffer, drawn with the
   /// line-drawing characters that \b style selects (or parentheses)
   /// @param style print style determining frame characters and decorators
   /// @param shape shape of the APL value being framed
   /// @param depth nesting depth for the frame label
   void add_frame(PrintStyle style, const Shape & shape, int depth);

   /// add a decorator frame around this buffer, drawn with the characters
   /// \b chars (see Frame_char)
   /// @param chars the FC_COUNT frame line, decorator, and option characters
   /// @param shape shape of the APL value being framed
   /// @param depth nesting depth for the frame label
   /// @param value_flags the properties of the value that select the
   ///        value-dependent decorators: PST_SIMPLE_NUMER (∼),
   ///        PST_SIMPLE_MIXED (+), PST_EMPTY_LAST (⊖), and PST_EMPTY_NLAST (⌽)
   void add_frame(const UCS_string & chars, const Shape & shape, int depth,
                  int value_flags);

   /// add an outer frame around this buffer
   /// @param style print style determining frame characters
   void add_outer_frame(PrintStyle style);

   /// add an outer frame around this buffer, drawn with the characters
   /// \b chars (see Frame_char)
   /// @param chars the FC_COUNT characters (only FC_OUTER_... are used)
   void add_outer_frame(const UCS_string & chars);

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

   /// return the 16 line-drawing characters of line style \b cs
   /// (PST_CS_NONE, PST_CS_ASCII, ...) as UTF8, indexed by the legs that
   /// are present: 1 (North) + 2 (East) + 4 (South) + 8 (West), e.g.
   /// " ╵╶└╷│┌├╴┘─┴┐┤┬┼"
   /// @param cs the character set
   static const char * frame_lines(int cs);

   /// return the FC_COUNT frame characters (see Frame_char) for the
   /// line styles \b lines1 and \b lines2 (16 characters each, as
   /// returned by frame_lines()) and the decorators and options \b decor
   /// (FC_DECOR_COUNT characters).
   /// @param lines1 the line-drawing characters of line style 1
   /// @param lines2 the line-drawing characters of line style 2
   /// @param decor the decorators and options
   static UCS_string make_frame_chars(const UCS_string & lines1,
                                      const UCS_string & lines2,
                                      const UCS_string & decor);

   /// return the character equivalent of style \b pst for add_frame() and
   /// add_outer_frame(): the lines, decorators, and options of the (inner)
   /// frame, and the lines of the outer frame (see Frame_char)
   /// @param pst print style selecting the frame characters
   static UCS_string get_frame_chars(PrintStyle pst);

   /// return \b true iff \b chars (see Frame_char) has an (inner) frame
   /// @param chars frame characters
   static bool has_frame(const UCS_string & chars)
      { return chars[FC_TOP_LEFT] != UNI_SPACE; }

   /// return \b true iff \b chars (see Frame_char) has an outer frame
   /// @param chars frame characters
   static bool has_outer_frame(const UCS_string & chars)
      { return chars[FC_OUTER_TOP_LEFT] != UNI_SPACE; }

   /// return \b chars (see Frame_char) without its outer frame
   /// @param chars frame characters
   static UCS_string without_outer_frame(const UCS_string & chars);

   /// return the character representation of \b cell, with the frames of
   /// a nested cell drawn with \b frame_chars
   /// @param cell the cell
   /// @param pctx print context controlling formatting
   /// @param frame_chars frame characters (see Frame_char)
   static PrintBuffer item_representation(const Cell & cell,
                                          const PrintContext & pctx,
                                          const UCS_string & frame_chars);

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

   /// format column \b x of a simple numeric \b value with complex items
   /// (lrm p. 17-18) into \b item_matrix
   /// @param value the simple numeric value being printed
   /// @param x the column
   /// @param pctx print context controlling formatting
   /// @param item_matrix the item matrix of \b value (see do_PrintBuffer())
   /// @param strict format exactly like IBM APL2 (STRICT_IBM_APL2_FORMATTING)
   static void format_complex_column(const cValue & value, ShapeItem x,
                                     const PrintContext & pctx,
                                     PrintBuffer * item_matrix, bool strict);

   /// format the items y of \b values with \b used[y] as one numeric
   /// (sub-) column, i.e. in the same format and aligned at their decimal
   /// points and exponents (lrm p. 17)
   /// @param values the (real) values of the column
   /// @param used which items of \b values belong to the column
   /// @param pctx print context controlling formatting
   /// @param result the aligned items (only those with \b used[y])
   /// @param scaled set to true if the sub-column is scaled
   /// @param may_scale the items that may cause the scaling (default: used)
   /// @param int_values the exact values of the items that may not cause
   ///        the scaling (integers), used if the sub-column is not scaled
   /// @return the width of the sub-column (of its aligned items)
   static size_t format_sub_column(const std::vector<APL_Float> & values,
                                   const std::vector<bool> & used,
                                   const PrintContext & pctx,
                                   std::vector<UCS_string> & result,
                                   bool & scaled,
                                   const std::vector<bool> * may_scale = 0,
                              const std::vector<APL_Integer> * int_values = 0);

   /// remove the leading (if \b left) and/or trailing (if \b right) blanks
   /// and internal padding characters of \b ucs
   /// align column \b x of a mixed or nested \b value (non-strict)
   static void align_mixed_column(const cValue & value, ShapeItem x,
                                  PrintBuffer * item_matrix);

   static void trim_padding(UCS_string & ucs, bool left, bool right);

   /// enclose this PrintBuffer in (multi-line) parentheses
   void add_parentheses();

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
   static ShapeItem separator_rows(ShapeItem y, const cValue & value,
                                   bool nested, sRank rk1, sRank rk2);

   /// the character buffer.
   UCS_string_vector buffer;

   /// column properties
   ColInfo col_info;

   /// true if completely constructed (as opposed to interrupted by ^C)
   bool complete;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __PRINT_BUFFER_HH__DEFINED__
