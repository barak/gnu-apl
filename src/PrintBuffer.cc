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

#include <stdio.h>
#include <string.h>

#include "Function.hh"
#include "Output.hh"
#include "Performance.hh"
#include "PointerCell.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "PrintBuffer.hh"
#include "PrintOperator.hh"
#include "Value.hh"
#include "UserPreferences.hh"
#include "Workspace.hh"

/// max sizes for arrays on the stack. Larger values are allocated with new()
enum
{
   PB_MAX_COLS   = 200,
   PB_MAX_ROWS   = 100,
   PB_MAX_ITEMS  = PB_MAX_COLS * PB_MAX_ROWS,
   PB_MAX_CHUNKS = 200,
};

//════════════════════════════════════════════════════════════════════════════
void
ColInfo::consider(const ColInfo & item)
{
   // this is the collective ColInfo of an entire column, and item
   // is a new, not yet considered item in the columns.
   //
   flags |= item.flags;
   if (item.imag_len)   flags |= has_j;

   if (int_len < item.int_len)
      {
        real_len += item.int_len - int_len;
        int_len = item.int_len;
      }

   if (item.denom_len)
      {
        if (denom_len < item.denom_len)   denom_len = item.denom_len;
        if (real_len < int_len + denom_len)   real_len = int_len + denom_len;
      }
   else
      {
        const int EXPO_LEN = real_len - fract_len - int_len;
        const int expo_len = item.real_len - item.fract_len - item.int_len;

        if (fract_len < item.fract_len)
           {
             fract_len = item.fract_len;
             if (fract_len + expo_len > denom_len)
             real_len = int_len + fract_len + EXPO_LEN;
           }

        if (EXPO_LEN < expo_len)    real_len += expo_len - EXPO_LEN;
      }

   if (imag_len  < item.imag_len)    imag_len  = item.imag_len;
}
//════════════════════════════════════════════════════════════════════════════
PrintBuffer::PrintBuffer()
   : complete(true)
{
}
//────────────────────────────────────────────────────────────────────────────
PrintBuffer::PrintBuffer(const UCS_string & ucs, const ColInfo & ci)
   : col_info(ci),
     complete(true)
{
   buffer.push_back(ucs);
}
//────────────────────────────────────────────────────────────────────────────
PrintBuffer::PrintBuffer(const cValue & value, const PrintContext & pctx,
                         ostream * out)
   : PrintBuffer(value, pctx, get_frame_chars(pctx.get_style()), out)
{
}
//────────────────────────────────────────────────────────────────────────────
PrintBuffer::PrintBuffer(const cValue & value, const PrintContext & _pctx,
                         const UCS_string & frame_chars, ostream * out)
   : complete(false)
{
PERFORMANCE_START(start_0)

   // bounded, catchable error instead of the mutual recursion with
   // PointerCell::character_representation() below eventually
   // exhausting the C++ call stack (Blake McBride, Bugs22 #4). Checked
   // once per level (this constructor is the only place either side of
   // that recursion calls back into), so a value within the limit
   // never pays for more than one compute_depth() call per level.
   if (value.compute_depth() > PrintBuffer::MAX_PRINT_NESTING_DEPTH)
      LIMIT_ERROR_NESTING;

   // Note: if ostream is non-0 then this value may be incomplete
   // (as indicated by member complete if it is huge). This is to speed
   // up printing if the value is discarded after having been printed

   // the outer frame (if any) is drawn around this value only, but not
   // around its items.
   //
const bool framed = has_frame(frame_chars) || has_outer_frame(frame_chars);
const UCS_string item_chars = without_outer_frame(frame_chars);
PrintContext pctx(_pctx);
   pctx.set_style(PrintStyle(_pctx.get_style() &~ PST_CS_OUTER));

const ShapeItem ec = value.element_count();

   // APL input format (e.g. 1 ⎕CR or error line 2). Test the flag rather
   // than the style, so that other flags in the style do not silently
   // select the output format.
   //
   if (pctx.get_style() & PST_INPUT)
      {
        pb_for_function(value, frame_chars);
        if (out)
           {
             UCS_string ucs(*this, value.get_rank(), _pctx.get_PW());
             if (ucs.size())   *out << ucs << endl;
            }
        update_info();
        complete = true;
        return;
      }

   if (value.is_scalar())
      {
        PERFORMANCE_START(start_1)
        Cell cache;
        const Cell & cell = value.get_cfirst(cache);
        PrintContext pctx1(pctx);
        if (cell.need_scaling(pctx))   pctx1.set_scaled();

        *this = item_representation(cell, pctx1, item_chars);

        // pad the value unless it is framed
        if (value.compute_depth() > 1 && !framed)
           {
             pad_l(UNI_PAD_l_VALUE, 1);
             pad_r(UNI_PAD_r_VALUE, 1);
           }

        add_outer_frame(frame_chars);

        if (out)
           {
             UCS_string ucs(*this, value.get_rank(), _pctx.get_PW());
             if (ucs.size())   *out << ucs << endl;
            }
        complete = true;
        PERFORMANCE_END(fs_PrintBuffer1_B, start_1, ec)
        return;
      }

   if (pctx.get_style() & PST_QUOTE_CHARS)
      {
        if (value.is_char_vector())
           {
             UCS_string ucs;
             ucs << UNI_DOUBLE_QUOTE;
             loop(v, ec)   ucs << value.get_char_value(v);
             ucs << UNI_DOUBLE_QUOTE;
             append_ucs(ucs);
             update_info();
             complete = true;
             return;
           }

        if (value.is_char_array())
           {
             pctx.set_style(PR_BOXED_DOUBLE);
             new (this)   PrintBuffer(value, pctx, out);
             return;
           }
      }

   if (ec == 0)   // empty value of any dimension
      {
        pb_empty(value, pctx, frame_chars);
        if (out)
           {
             UCS_string ucs(*this, value.get_rank(), _pctx.get_PW());
             if (ucs.size())   *out << ucs << endl;
            }
        update_info();
        complete = true;
        return;
      }

   if (value.is_char_vector())
      {
        // fast path: a simple (non-nested) character vector's per-cell
        // representation (CharCell::character_representation() for the
        // default, non-quoted style handled above) is always exactly
        // the one character itself -- no scaling, no column alignment
        // is ever needed. The general path below builds one heap-
        // allocated PrintBuffer/UCS_string *per character* via an
        // ec-element item_matrix; for a long line that cost ~100x the
        // payload size in peak RSS and was clearly superlinear in time
        // (Blake McBride, Bugs6 #6). Assemble the single row directly
        // instead, matching the same shortcut already taken above for
        // PST_QUOTE_CHARS and in pb_for_function().
        //
        UCS_string ucs;
        ucs.reserve(ec);
        const bool pretty = pctx.get_style() & PST_PRETTY;
        loop(e, ec)
           {
             Unicode uni = value.get_char_value(e);
             if (pretty && uni < UNI_SPACE)   uni = Unicode(uni + 0x2400);
             ucs << uni;
           }
        append_ucs(ucs);
        add_outer_frame(frame_chars);

        if (ec > 10000 && out)
           print_interruptible(*out, value.get_rank(), pctx.get_PW());
        else if (out)
           {
             UCS_string out_ucs(*this, value.get_rank(), pctx.get_PW());
             if (out_ucs.size())   *out << out_ucs << endl;
           }

        update_info();
        complete = true;
        return;
      }

   // non-trivial PrintBuffer
   //
const ShapeItem cols = value.get_last_shape_item();

PrintBuffer * item_matrix = 0;
   try { item_matrix = new PrintBuffer[ec]; }
   catch (std::bad_alloc &)
      {
        MORE_ERROR() << "value too large to print ("
                     << cols << " columns, " << ec << " items)";
        WS_FULL;
      }
   catch (...)
      { FIXME; }

   // do_PrintBuffer() can itself throw (e.g. WS_FULL from a nested
   // PrintBuffer of a PointerCell under memory pressure); without this
   // guard, item_matrix (and every UCS_string row already built inside
   // it) would leak, worsening the very WS_FULL that caused it.
   //
bool interrupted = false;
   try            { interrupted = do_PrintBuffer(value, pctx, out,
                                                 frame_chars, item_matrix); }
   catch (...)   { delete [] item_matrix;   throw; }

   if (interrupted)   // ^C hit
      {
        // the user has interrupted the construction
        //
        InterruptContext::clear_attention_raised(LOC);
        InterruptContext::clear_interrupt_raised(LOC);
        if (out)   *out << endl << "INTERRUPT" << endl;
      }

   delete [] item_matrix;

   PERFORMANCE_END(fs_PrintBuffer_B, start_0, ec)
}
//────────────────────────────────────────────────────────────────────────────
bool
PrintBuffer::do_PrintBuffer(const cValue & value, const PrintContext & pctx,
                            ostream * out, const UCS_string & frame_chars,
                            PrintBuffer * item_matrix)
{
const bool framed = has_frame(frame_chars) || has_outer_frame(frame_chars);
const UCS_string item_chars = without_outer_frame(frame_chars);
const ShapeItem ec = value.element_count();
const uint64_t ii_count = InterruptContext::get_interrupt_count();
const bool huge = out && ec > 10000;
const bool nested = !value.is_simple();
const ShapeItem cols = value.get_last_shape_item();
const ShapeItem rows = ec/cols;
vector<bool> scaling;         scaling.reserve(cols);
vector<PrintBuffer> pcols;    pcols.reserve(cols);
   loop(c, cols)
       {
         scaling.push_back(false);
         pcols.push_back(PrintBuffer());
       }

   // 1. init scaling, a vector with a bool per column that tells if the
   //    column needs scaling (i.e. exponential format) or not, as IBM APL2
   //    does it (lrm p.12-13, and verified with IBM APL2 itself):
   //
   //    a. in a simple numeric array, one item that needs scaling makes the
   //       entire column scaled. IBM APL2 stores such an array as a float
   //       (or complex) array as soon as it contains one non-integer item,
   //       and then its integers are scaled like floats (e.g. 163710 with
   //       ⎕PP←5 in 163710 1.8E¯11 is 1.6371E5). In an integer array, ⎕PP
   //       is ignored and nothing is scaled (lrm p.12).
   //       Without STRICT_IBM_APL2_FORMATTING an integer never causes the
   //       scaling (GNU APL's traditional output, e.g. 163710 1.8E¯11).
   //
   //    b. in a mixed (character and number) or nested array, every item
   //       keeps its own type and is formatted on its own: a float is scaled
   //       if it needs it, an integer never; there is no column rule (e.g.
   //       ¯84 stays ¯84 below 1.22E¯7 in a column of a mixed array).
   //
#define huge_interrupted \
   (huge && (ii_count != InterruptContext::get_interrupt_count()))

const bool strict = UserPreferences::uprefs.strict_IBM_APL2_formatting;
bool simple_numeric  = !nested;   // a. (else b.)
bool float_storage   = false;     // a. with a non-integer item
bool complex_storage = false;     // a. with a complex item
   loop(e, ec)
      {
        if (!simple_numeric)   break;
        Cell cache;
        const Cell & cell = value.get_cravel(e, cache);
        if (!cell.is_numeric())             simple_numeric = false;
        else if (!cell.is_integer_cell())   float_storage = true;
        // a complex number with imaginary part 0 is real (IBM APL2 demotes
        // it, e.g. the literal 1J0 is 1)
        if (cell.is_complex_cell() && cell.get_imag_value() != 0.0)
           complex_storage = true;
      }
   if (!simple_numeric)   float_storage = complex_storage = false;

   if (simple_numeric)   // case a.
      {
        loop(x, cols)
        loop(y, rows)
            {
              if (huge_interrupted)   return true;
              Cell cache;
              const Cell & cell = value.get_cravel(x + y*cols, cache);
              // GNU APL (unlike IBM APL2) never lets an integer cause the
              // scaling of its column, since that loses precision without
              // making the output shorter. An integer in a column
              // that is scaled anyway is scaled like the floats in it.
              //
              const bool need = cell.is_integer_cell()
                 ? strict && float_storage &&
                   FloatCell::need_scaling(APL_Float(cell.get_int_value()),
                                           pctx.get_PP())
                 : cell.need_scaling(pctx);
              if (need)
                 {
                   scaling[x] = true;
                   break;
                 }
            }
      }

   /* 2. create a matrix of items.

         An item of the matrix is a PrintBuffer for a (possibly nested)
         top-level cell. The item matrix therefore has (⍴,value) == rows×cols
         items. Every items is a PrintBuffer and therefore rectangular.

              value          =>              item matrix
         ──────────────────      ───────────────────────────────────────
         Cell Cell ... Cell      PrintBuffer PrintBuffer ... PrintBuffer
         Cell Cell ... Cell      PrintBuffer PrintBuffer ... PrintBuffer
         ...                     ...
         Cell Cell ... Cell      PrintBuffer PrintBuffer ... PrintBuffer
    */
   PERFORMANCE_START(start_2)
   vector<sRank> max_row_ranks;
   max_row_ranks.reserve(rows);
   loop(y, rows)
      {
        ShapeItem max_row_height = 0;
        max_row_ranks.push_back(0);

        loop(x, cols)
            {
              if (huge_interrupted)   return true;

              PrintBuffer & item = item_matrix[y*cols + x];
              PrintContext pctx1 = pctx;
              Cell cache;
              const Cell & cell = value.get_cravel(x + y*cols, cache);
              if (simple_numeric)   // case a.: per column
                 {
                   if (scaling[x])   pctx1.set_scaled();
                 }
              else if (cell.need_scaling(pctx))   // case b.: per item
                 {
                   pctx1.set_scaled();
                 }
              item = item_representation(cell, pctx1, item_chars);
              if (!item.get_row_count())
                 {
                   UCS_string empty;
                   item.append_ucs(empty);
                 }

              // GNU APL (unless STRICT_IBM_APL2_FORMATTING): an empty item
              // is displayed as a single blank, whatever its shape and
              // prototype. For the blanks and lines between items it counts
              // as a vector.
              //
              const Value * empty_sub = 0;
              if (Value_P sub = cell.try_pointer_value())
                 {
                   if (sub->element_count() == 0)   empty_sub = sub.get();
                 }
              if (empty_sub && !strict && !framed)
                 {
                   ColInfo ci = item.get_info();
                   const UCS_string ucs(1, UNI_SPACE);
                   item = PrintBuffer(ucs, ci);
                   item.get_info().int_len  = ucs.size();
                   item.get_info().real_len = ucs.size();
                 }

              if (Value_P sub = cell.try_pointer_value())
                 {
                   sRank sub_rank = sub->get_rank();
                   if (empty_sub && !strict && sub_rank > 1)   sub_rank = 1;
                   if (max_row_ranks.back() < sub_rank)
                      max_row_ranks.back() = sub_rank;
                 }

              if (max_row_height < item.get_row_count())
                 max_row_height = item.get_row_count();

              Assert1(item.is_rectangular());
            }

// loop(y, rows) loop(x, cols) CERR << item_matrix[y*cols + x] << endl;

        // pad all items to the same height
        //
        loop(x, cols)
           {
              PrintBuffer & item = item_matrix[y*cols + x];
              if (huge_interrupted)   return true;

             item.pad_height(UNI_PAD_b_ROW, max_row_height);
             Assert1(item.is_rectangular());
           }
      }
   PERFORMANCE_END(fs_PrintBuffer2_B, start_2, ec)

   // 3. align all columns (which pads them to the same width).
   //
   PERFORMANCE_START(start_3)
   loop(x, cols)
      {
        ColInfo col_info_x;
        loop(y, rows)
            {
              if (huge_interrupted)   return true;

              PrintBuffer * item_row = item_matrix + y*cols;
              col_info_x.consider(item_row[x].get_info());
            }
         if (col_info_x.real_len<(col_info_x.int_len + col_info_x.denom_len))
            col_info_x.real_len = col_info_x.int_len + col_info_x.denom_len;

        // A column of a simple numeric array with complex numbers is
        // formatted as two sub-columns, see format_complex_column(). With
        // STRICT_IBM_APL2_FORMATTING (as in IBM APL2) that is every column
        // of an array with a complex number somewhere (also columns with
        // only real numbers); otherwise (GNU APL's traditional output) only
        // the columns that contain a complex number. Both include columns
        // where no J is displayed (because all imaginary parts are too
        // small, lrm p. 13).
        //
        bool complex_column = strict && complex_storage;
        if (!strict && simple_numeric)
           {
             loop(y, rows)
                {
                  Cell cache;
                  if (value.get_cravel(x + y*cols, cache).is_complex_cell())
                     {
                       complex_column = true;
                       break;
                     }
                }
           }

        if (complex_column)
           {
             format_complex_column(value, x, pctx, item_matrix, strict);
             continue;
           }

        // In a mixed (character and number) or nested array, IBM APL2 does
        // not align the numbers of a column to their decimal points, E's,
        // or J's (nor pad them to a common format, see step 1 b.): every
        // simple item is simply right-aligned to the width of the widest
        // item of the column, e.g. 0.0059 below 7294149 takes 7 columns,
        // not 12, and 600 below (⊂1 2) stays 600 (not 600.0E0) above 4.2E¯9.
        // Nested items (incl. character vectors) are left-aligned (see
        // below). GNU APL's traditional output: see align_mixed_column().
        //
        if (!simple_numeric && (col_info_x.flags & CT_NUMERIC) && !strict)
           {
             // GNU APL's traditional output: the simple scalar numbers of
             // the column are aligned at their decimal points (and other
             // items are right-aligned), see align_mixed_column().
             //
             align_mixed_column(value, x, item_matrix);
             continue;
           }

        if (!simple_numeric && (col_info_x.flags & CT_NUMERIC))
           {
             int width = 0;
             loop(y, rows)
                {
                  const int w = item_matrix[y*cols + x].get_column_count();
                  if (width < w)   width = w;
                }

             loop(y, rows)
                {
                  if (huge_interrupted)   return true;

                  PrintBuffer & item = item_matrix[y*cols + x];
                  const int diff = width - item.get_column_count();
                  // IBM APL2 left-aligns a nested item (also a character
                  // vector, contrary to lrm p. 19) in a column that
                  // contains numbers, and right-aligns the other items.
                  Cell cache;
                  if (diff > 0)
                     {
                       if (value.get_cravel(x + y*cols, cache).is_pointer_cell())
                          item.pad_r(UNI_PAD_r_FRACT, diff);
                       else
                          item.pad_l(UNI_PAD_l_INT, diff);
                     }
                  ColInfo & ci = item.get_info();
                  ci.int_len  = width;   // as for a character item
                  ci.fract_len = 0;
                  ci.real_len = width;
                }
             continue;
           }

        loop(y, rows)
            {
              if (huge_interrupted)   return true;

              PrintBuffer * item_row = item_matrix + y*cols;
              item_row[x].align(col_info_x);
            }
      }
   PERFORMANCE_END(fs_PrintBuffer3_B, start_3, ec)

// loop(y, rows) loop(x, cols) CERR << item_matrix[y*cols + x] << endl;

   // 4. collect columm items. That is, merge all PrintBuffers of each
   //    column into a single PrintBuffer for that column.
   //
   PERFORMANCE_START(start_4)

   // the blank lines between rows with nested items of rank > 1 (lrm p.
   // 138) are not needed when the items are enclosed in (multi-line)
   // parentheses, which show the structure (STRICT_IBM_APL2_FORMATTING
   // parentheses)
   //
const bool row_gaps = nested && frame_chars[FC_PARENS] != UNI_L_PARENT;

   int last_col_spacing = 0;    // the col_spacing of the previous column
   bool last_non_char_col = false;   // the previous column has a non-char

   loop(x, cols)
      {
        // merge column x of the item_matrix into one
        // PrintBuffer pcol. Insert separator rows as needed.
        //
        PrintBuffer & dest = pcols[x];
        pcols[x] = PrintBuffer();

        // compute the final height of dest and reserve enough rows as to
        // avoid unnecessary copies
        //
        {
          ShapeItem dest_height = 0;
          loop(y, rows)
             {
               const sRank Rk_y_1 = y ? max_row_ranks[y - 1] : 0;
               const ShapeItem sepa_rows =
                     separator_rows(y, value, row_gaps, max_row_ranks[y], Rk_y_1);
               const ShapeItem item_rows =
                               item_matrix[y*cols + x].get_row_count();
               Assert(item_rows);
               dest_height += sepa_rows + item_rows;
             }
          dest.buffer.reserve(dest_height);
        }

        loop(y, rows)
            {
              if (huge_interrupted)   return true;

              // insert separator row(s)
              //
              if (const ShapeItem sepa_rows =
                        separator_rows(y, value, row_gaps, max_row_ranks[y],
                                       y ? max_row_ranks[y - 1] : 0))
                 {
                  const UCS_string sepa_row(dest.get_column_count(),
                                            UNI_PAD_y_AXIS);
                  loop(r, sepa_rows)   dest.append_ucs(sepa_row);
                 }

              const PrintBuffer & src = item_matrix[y*cols + x];
              dest.add_row(src);
            }

        // non_char_col: column x contains a number or a nested item. Not to
        // be confused with the NOTCHAR function of lrm p. 138 (which is a
        // property of an item, see cValue::NOTCHAR()). It only decides on
        // which side of the column boundary the first blank goes, while
        // col_spacing (computed from NOTCHAR of the items) decides how many
        // blanks there are.
        //
        bool non_char_col = false;   // set by Value::get_col_spacing()
        const int col_spacing = value.get_col_spacing(non_char_col, x,
                                                      framed);

        const int max_spacing = (col_spacing > last_col_spacing) 
                                  ?  col_spacing : last_col_spacing;
        int non_char_spaces = 0;   // the spaces added for non_char_col

        if (huge_interrupted)  return true;

        if (x)   // subsequent column
           {
             if (last_non_char_col)
                {
                  // the previous column has a non-char, therefore we append
                  // one pad char to the previous column.
                  //
                  pcols[x - 1].pad_r(UNI_PAD_r_NOTCHAR, 1);
                  ++non_char_spaces;
                }
             else if (non_char_col)
                {
                  // the current column has a non-char, therefore we prepend
                  // one pad to the current column.
                  //
                  dest.pad_l(UNI_PAD_l_NOTCHAR, 1);
                  ++non_char_spaces;
                }

             // we want a total spacing of 'max_spacing', but we
             // do not count the 'non_char_spaces' chars ² and ³
             // that were appended above.
             //
             if (const int u7_pad_len = max_spacing - non_char_spaces)
                {
                   pcols[x - 1].pad_r(UNI_PAD_r_MAX, u7_pad_len);
                }
           }

        if (huge_interrupted)   return true;

        last_col_spacing = col_spacing;
        last_non_char_col = non_char_col;
      }

#undef huge_interrupted

   // 5. combine pcols. That is, take the first PrintBuffer pcols[0] and append
   //    all subsequenct PrintBuffers pcol[N], N ≥ 1, to it. 
   //    the f
   //
   for (int dx = 1; dx < cols; dx += dx)
   for (int xx = dx; xx < cols; xx += dx)
       {
         pcols[xx - dx].append_col(pcols[xx]);
       }
   *this = pcols[0];

   if (value.compute_depth() > 1 && !framed)
      {
        pad_l(UNI_PAD_l_DEPTH,  1);
        pad_r(UNI_PAD_r_DEPTH, 1);
      }

   if (!is_rectangular())   // should not happen
      {
        Q1(get_row_count())
        loop(h, get_row_count())   CERR << "w=" << get_column_count(h)
                                        << "*" << endl;
        loop(h, get_row_count())   CERR << "*"  << get_line(h)
                                        << "*" << endl;
      }

   Assert(is_rectangular());
   add_outer_frame(frame_chars);

   PERFORMANCE_END(fs_PrintBuffer4_B, start_4, ec)
   
   PERFORMANCE_START(start_5)

   if (huge)   // ergo: out
      {
        print_interruptible(*out, value.get_rank(), pctx.get_PW());
      }
   else if (out)
      {
        UCS_string ucs(*this, value.get_rank(), pctx.get_PW());
        if (ucs.size())   *out << ucs << endl;
      }

   PERFORMANCE_END(fs_PrintBuffer5_B, start_5, ec)
   
   complete = true;
   update_info();
   return false;   // OK
}
//════════════════════════════════════════════════════════════════════════════
void
PrintBuffer::pb_for_function(const cValue & value,
                             const UCS_string & frame_chars)
{
UCS_string ucs;
   append_input_form(ucs, value);

ColInfo ci;
   *this = PrintBuffer(ucs, ci);
   add_outer_frame(frame_chars);
}
//────────────────────────────────────────────────────────────────────────────
/// append the shortest text of \b value that reads back exactly
static void
append_input_float(UCS_string & ucs, APL_Float value)
{
char buf[40];
   for (int prec = 15; prec <= 17; ++prec)
       {
         snprintf(buf, sizeof(buf), "%.*g", prec, value);
         if (strtod(buf, 0) == value)   break;
       }

   // C notation → APL notation, e.g. -1.5e-05 → ¯1.5E¯5
   //
   for (const char * cp = buf; *cp; ++cp)
       {
         if      (*cp == '-')   ucs << UNI_OVERBAR;
         else if (*cp == '+')   ;
         else if (*cp == 'e')
            {
              ucs << UNI_E;
              if (cp[1] == '-')   { ucs << UNI_OVERBAR;   ++cp; }
              else if (cp[1] == '+')   ++cp;
              while (cp[1] == '0' && cp[2])   ++cp;   // leading 0s
            }
         else   ucs << Unicode(*cp);
       }
}
//────────────────────────────────────────────────────────────────────────────
/// append a simple scalar \b cell in APL input format (without quotes for
/// a character)
static bool
append_input_scalar(UCS_string & ucs, const Cell & cell)
{
   if (cell.is_character_cell())
      {
        const Unicode uni = cell.get_char_value();
        ucs << uni;
        if (uni == UNI_SINGLE_QUOTE)   ucs << uni;   // ' → ''
      }
   else if (cell.is_integer_cell())
      {
        char buf[40];
        snprintf(buf, sizeof(buf), "%lld",
                 static_cast<long long>(cell.get_int_value()));
        for (const char * cp = buf; *cp; ++cp)
            ucs << (*cp == '-' ? UNI_OVERBAR : Unicode(*cp));
      }
   else if (cell.is_complex_cell())
      {
        append_input_float(ucs, cell.get_real_value());
        ucs << UNI_J;
        append_input_float(ucs, cell.get_imag_value());
      }
   else if (cell.is_float_cell())
      {
        append_input_float(ucs, cell.get_real_value());
      }
   else
      {
        return false;
      }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::append_input_item(UCS_string & ucs, const Cell & cell)
{
   if (cell.is_character_cell())   // 'c'
      {
        ucs << UNI_SINGLE_QUOTE;
        append_input_scalar(ucs, cell);
        ucs << UNI_SINGLE_QUOTE;
        return;
      }

   if (cell.is_pointer_cell())
      {
        // a strand encloses every item that is not a simple scalar (also a
        // nested scalar): P → (P), except for a character vector of length
        // ≠ 1 ('ab' or '') and ⍬, which need no parentheses.
        //
        const Value_P P = cell.get_pointer_value();
        Cell cache;
        if ((P->is_char_vector() && P->element_count() != 1) ||
            (P->is_vector() && P->element_count() == 0 &&
             P->get_cproto(cache).is_numeric()))
           {
             append_input_form(ucs, *P);
             return;
           }

        ucs << UNI_L_PARENT;
        append_input_form(ucs, *P);
        ucs << UNI_R_PARENT;
        return;
      }

   if (append_input_scalar(ucs, cell))   return;

   // some other cell (e.g. an LvalCell): its normal display
   //
const PrintContext pctx(PR_APL_MIN);
   ucs << UCS_string(cell.character_representation(pctx), 0, MAX_Quad_PW);
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::append_input_form(UCS_string & ucs, const cValue & value)
{
const ShapeItem ec = value.element_count();
Cell cache;

   if (value.is_scalar())
      {
        const Cell & cell = value.get_cfirst(cache);
        if (cell.is_pointer_cell())   // ⊂P
           {
             ucs << UNI_SUBSET;
             append_input_form(ucs, *cell.get_pointer_value());
           }
        else
           {
             append_input_item(ucs, cell);
           }
        return;
      }

   if (value.get_rank() == 1 && ec == 0)   // '', ⍬, or 0⍴⊂P
      {
        const Cell & proto = value.get_cfirst(cache);
        if      (proto.is_character_cell())   ucs << UNI_SINGLE_QUOTE
                                                  << UNI_SINGLE_QUOTE;
        else if (proto.is_pointer_cell())
           {
             ucs << UNI_0 << UNI_RHO << UNI_SUBSET;
             append_input_form(ucs, *proto.get_pointer_value());
           }
        else                                  ucs << UNI_ZILDE;
        return;
      }

   if (value.get_rank() != 1)   // the shape, then ⍴ and the ravel
      {
        loop(r, value.get_rank())
           {
             if (r)   ucs << UNI_SPACE;
             append_input_scalar(ucs, IntCell(value.get_shape_item(r)));
           }
        ucs << UNI_RHO;

        if (ec == 0)   // the prototype
           {
             const Cell & proto = value.get_cfirst(cache);
             if      (proto.is_character_cell())   ucs << UNI_SINGLE_QUOTE
                                                       << UNI_SINGLE_QUOTE;
             else if (proto.is_pointer_cell())
                {
                  ucs << UNI_SUBSET;
                  append_input_form(ucs, *proto.get_pointer_value());
                }
             else                                  ucs << UNI_ZILDE;
             return;
           }

        if (ec == 1)   // a scalar reshaped, e.g. 1 1⍴5
           {
             const Cell & cell = value.get_cfirst(cache);
             if (cell.is_pointer_cell())
                {
                  ucs << UNI_SUBSET;
                  append_input_form(ucs, *cell.get_pointer_value());
                }
             else
                {
                  append_input_item(ucs, cell);
                }
             return;
           }
      }
   else if (ec == 1)   // a 1-element vector: ,X
      {
        ucs << UNI_COMMA;
        const Cell & cell = value.get_cfirst(cache);
        if (cell.is_pointer_cell())
           {
             ucs << UNI_SUBSET;
             append_input_form(ucs, *cell.get_pointer_value());
           }
        else
           {
             append_input_item(ucs, cell);
           }
        return;
      }

   // the ravel (with at least 2 elements) as a string or a strand
   //
   if (value.is_char_array())
      {
        ucs << UNI_SINGLE_QUOTE;
        loop(e, ec)   append_input_scalar(ucs, value.get_cravel(e, cache));
        ucs << UNI_SINGLE_QUOTE;
        return;
      }

   loop(e, ec)
      {
        if (e)   ucs << UNI_SPACE;
        append_input_item(ucs, value.get_cravel(e, cache));
      }
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::pb_empty(const cValue & value, PrintContext pctx, 
                      const UCS_string & frame_chars)
{
   if (value.get_rank() == 1)   // vector: 1 line
      {
        UCS_string ucs;   // empty
        append_ucs(ucs);
        add_outer_frame(frame_chars);
        return;   // 1 row
      }

const Shape sh = value.get_shape().without_last_axis();

   // Test the VALUE's own element count, not sh's volume: sh is the
   // shape with the LAST axis dropped, so for e.g. 1E9 1E9 0⍴0 (a huge
   // leading-axes product, but 0 elements overall because the trailing
   // axis is 0) sh.get_volume() is 1E18, not <=1 -- the empty-array
   // fast path below was skipped, and `lines = sh.get_volume()` a few
   // lines down then throws std::length_error out of buffer.resize(),
   // an exception Executable.cc's outermost catch(...) turns into a
   // silent exit(0) instead of ever displaying the (empty!) array.
   //
   if (value.element_count() == 0)
      {
        // With STRICT_IBM_APL2_FORMATTING, an empty value of rank ≥ 2 has
        // rows of width 0 if its last axis is 0, or otherwise (some other
        // axis is 0) only the blank lines between its planes, as IBM APL2
        // displays it -- see cValue::empty_lines(). Every line is empty
        // (width 0), so the buffer stays rectangular. Otherwise (GNU APL's
        // traditional output) it has no lines at all.
        //
        if (UserPreferences::uprefs.strict_IBM_APL2_formatting)
           buffer.resize(value.empty_lines());
        add_outer_frame(frame_chars);
        return;
      }

   // value has > 0 rows. Compute how many lines we need.
   //
ShapeItem lines = sh.get_volume();
   loop(s, sh.get_rank())
      lines += s * (sh.get_shape_item(sh.get_rank() - s - 1));

   buffer.resize(lines);
   add_outer_frame(frame_chars);
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::print_interruptible(ostream & out, sRank rank, int quad_PW)
{
   if (get_row_count() == 0)   return;      // empty PrintBuffer

   // lines may be (very) long (compared to ⎕PW) and if they are then they
   // need to be broken into 2 or more chunks. A chunk is smaller than ⎕PW;
   // the first chunk is printed un-indented, while subsequent chunk are
   // indented by 6 blanks.
   //
const int total_width = get_column_count();

vector<ShapeItem> chunk_lengths;
   if (quad_PW)   // APL folding of lines near ⎕PW
      {
        const int max_breaks = 2 + total_width/quad_PW;   // a first guess

        chunk_lengths.reserve(max_breaks + 1);

        // initialize chunk_lengths based on the first row of the PrintBuffer.
        // All subsequent rows are aligned to the first row, therefore the
        // first row can be taken as a prototype for all rows.
        //
        for (int col = 0; col < total_width;)
            {
              const ShapeItem chunk_len =
                      get_line(0).compute_chunk_length(quad_PW, col);
              chunk_lengths.push_back(chunk_len);
              col += chunk_len;
            }
       }
    else          // no APL wrap around
       {
         chunk_lengths.push_back(total_width);
       }

   // print rows, breaking each row at chunk_lengths
   //
   loop(row, get_row_count())
       {
         int brk_idx = 0;   // chunk_lengths index

         for (int col = 0; col < total_width;)
            {
              if (col)   out << endl << "      ";

              const size_t chunk_len = chunk_lengths[brk_idx++];
              UCS_string trow(get_line(row), col, chunk_len);
              trow.remove_trailing_padchars();

              loop(t, trow.size())
                  {
                     const Unicode uni = trow[t];
                     if (is_iPAD_char(uni))   out << " ";
                     else                     out << uni;
                  }
              col += chunk_len;

              if (InterruptContext::interrupt_is_raised())
                 {
                   out << endl << "INTERRUPT" << endl;
                   InterruptContext::clear_attention_raised(LOC);
                   InterruptContext::clear_interrupt_raised(LOC);
                   return;
                 }
            }

         out << endl;   // end of row
       }
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::set_char(int x, int y, Unicode uc)
{
   Assert(y < int(buffer.size()));
   Assert(x < int(buffer[y].size()));
   buffer[y][x] = uc;
}
//────────────────────────────────────────────────────────────────────────────
Unicode
PrintBuffer::get_char(int x, int y) const
{ 
   Assert(y < int(buffer.size()));
   Assert(x < int(buffer[y].size()));
   return buffer[y][x];
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::pad_l(Unicode pad, ShapeItem count)
{
   if (count == 1)
      {
        loop(y, get_row_count())   buffer[y].prepend(pad);
      }
   else
      {
        UCS_string pads(count, pad);
        loop(y, get_row_count())   buffer[y] = pads + buffer[y];
      }
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::pad_r(Unicode pad, ShapeItem count)
{
UCS_string ucs(count, pad);
   loop(y, get_row_count())   buffer[y] << ucs;
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::pad_height(Unicode pad, ShapeItem height)
{
   if (height > get_row_count())
      {
        UCS_string ucs(get_column_count(), pad);
        while (height > get_row_count())   buffer.push_back(ucs);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::pad_height_above(Unicode pad, ShapeItem height)
{
   if (height > get_row_count())
      {
        UCS_string ucs(get_column_count(), pad);
        while (height > get_row_count())   buffer.insert(0, ucs);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::pad_to_spaces()
{
   loop(y, get_row_count())
   loop(x, get_column_count(y))
      if (is_iPAD_char(get_char(x, y)))   set_char(x, y, UNI_SPACE);
}
//────────────────────────────────────────────────────────────────────────────
// The →/↓/∼/+/∊/¯ frame decorators below follow IBM's DISPLAY workspace
// convention, documented in devel_doc/apl2lrm.txt, "Picture of an Array's
// Structure" (Chapter 2, p.9): →/↓ indicate rank, and exactly one of
// ∼ (numeric) / + (mixed) / no symbol (character) / ∊ (nested) / ¯ (scalar
// blank) indicates a box's own data type on its bottom border. GNU APL
// uses ϵ (U+03F5) rather than the LRM's actual ∊ (U+220A) to avoid
// colliding with the real Enlist/Member primitive glyph, and extends the
// single flag into a repeated depth counter (ϵ, ϵϵ, ϵϵϵ, ...) -- the LRM's
// own convention has no such counter; it says depth is read by counting
// box borders crossed while tracing inward from the outside.
//
void
PrintBuffer::add_frame(PrintStyle style, const Shape & shape, int depth)
{
   add_frame(get_frame_chars(style), shape, depth, style);
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::add_frame(const UCS_string & chars, const Shape & shape,
                       int depth, int value_flags)
{
   Assert(is_rectangular());
   Assert(chars.size() == FC_COUNT);

   if (chars[FC_PARENS] == UNI_L_PARENT)
      {
        add_parentheses();
        return;
      }

   if (get_row_count() == 0)   // empty
      {
        UCS_string upper;
        upper << chars[FC_TOP_LEFT] << chars[FC_TOP_RIGHT];
        buffer.push_back(upper);

        UCS_string lower;
        lower << chars[FC_BOTTOM_LEFT] << chars[FC_BOTTOM_RIGHT];
        buffer.push_back(lower);

        Assert(is_rectangular());
        return;
      }

   // draw │ on the left and on the right
   //
   loop(y, get_row_count())
      {
        buffer[y].prepend(chars[FC_VERT]);
        buffer[y] << chars[FC_VERT];

        // change internal pad characters to SPACE so that they will
        // not be removed later and the frame is printed correctly
        //
        buffer[y].map_pad();

      }

   // draw ─ above the top and below the bottom.
   //
UCS_string hori(get_column_count(), chars[FC_HORI]);

   buffer.insert(0, hori);
   buffer.push_back(hori);

   // draw the corners ┌, └, ┐, and ┘
   //
   {
     const int XX = get_column_count() - 1;
     const int YY = get_row_count() - 1;
     set_char(0,  0,  chars[FC_TOP_LEFT]);       // e.g. ╔
     set_char(0,  YY, chars[FC_BOTTOM_LEFT]);    // e.g. ╚
     set_char(XX, 0,  chars[FC_TOP_RIGHT]);      // e.g. ╗
     set_char(XX, YY, chars[FC_BOTTOM_RIGHT]);   // e.g. ╝
   }

   // draw the frame decorators. A blank in chars means: no such decorator.
   // value_flags tells which of the value-dependent decorators apply.
   //
   if (shape.get_rank() > 0)               // → on top frame line
      {
        if (chars[FC_RANK_X] == UNI_NUMBER_SIGN)   // axis length (NARS)
           {
             UCS_string ucs;
             ucs << shape.get_last_shape_item();
             // fits into the interior of the frame line (Bugs37 #59)
             if (ucs.ssize() <= (get_column_count() - 2))
                {
                  loop(u, ucs.ssize())   set_char(u + 1, 0, ucs[u]);
                }
           }
        else if (chars[FC_RANK_X] != UNI_SPACE)   // e.g. →
           {
             set_char(1, 0, chars[FC_RANK_X]);
           }
      }
   
   if (shape.get_rank() > 1)               // ↓ on left frame line
      {
        if (chars[FC_RANK_Y] == UNI_NUMBER_SIGN)   // axis lengths (NARS)
           {
             UCS_string ucs;
             loop(r, shape.get_rank() - 1)
                {
                  if (r)   ucs << chars[FC_VERT];
                  ucs << shape.get_shape_item(r);
                }
             if (ucs.ssize() <= get_row_count() - 2)   // Bugs37 #59
                {
                  loop(u, ucs.size())   set_char(0, u + 1, ucs[u]);
                }
           }
        else if (chars[FC_RANK_Y] != UNI_SPACE)   // e.g. ↓
           {
             set_char(0, 1, chars[FC_RANK_Y]);
           }
      }

   if (chars[FC_NESTED] == UNI_SPACE)   // no type decorators
      {
      }
   else if (depth > 1)                 // one or more ϵ on bottom frame line
      {
        loop(d, depth - 1)
            if (d + 1 < get_column_count() - 1)
               set_char(1 + d, get_row_count() - 1, chars[FC_NESTED]);
      }
   else if (value_flags & PST_SIMPLE_NUMER)   // simple numeric
     {
        if (chars[FC_NUMERIC] != UNI_SPACE)
           set_char(1, get_row_count() - 1, chars[FC_NUMERIC]);   // e.g. ∼
     }
   else if (value_flags & PST_SIMPLE_MIXED)   // simple mixed
     {
        if (chars[FC_MIXED] != UNI_SPACE)
           set_char(1, get_row_count() - 1, chars[FC_MIXED]);   // e.g. +
     }
   
   if ((value_flags & PST_EMPTY_LAST) &&   // last (X-) dimension is empty
       chars[FC_EMPTY_X] != UNI_SPACE)
      {
        set_char(1, 0, chars[FC_EMPTY_X]);   // e.g. ⊖
      }
   
   if ((value_flags & PST_EMPTY_NLAST) &&   // a non-last (Y-) axis is empty
       chars[FC_EMPTY_Y] != UNI_SPACE)
      {
        set_char(0, 1, chars[FC_EMPTY_Y]);   // e.g. ⌽
      }
   

   Assert(is_rectangular());
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::add_parentheses()
{
   // enclose this (nested) item in parentheses: ( and ) for an item of one
   // line, or ⎛ ⎜ ... ⎝ and ⎞ ⎟ ... ⎠ for an item of several lines (but
   // no lines above or below the item, unlike add_frame()).
   //
   if (get_row_count() == 0)   // empty
      {
        UCS_string ucs;
        ucs << UNI_L_PARENT << UNI_R_PARENT;
        buffer.push_back(ucs);
        return;
      }

const ShapeItem rows = get_row_count();
   loop(y, rows)
      {
        Unicode left  = UNI_L_PARENT;
        Unicode right = UNI_R_PARENT;
        if (rows > 1)
           {
             if (y == 0)              { left = Unicode(0x239B);     // ⎛
                                        right = Unicode(0x239E); }  // ⎞
             else if (y == rows - 1)  { left = Unicode(0x239D);     // ⎝
                                        right = Unicode(0x23A0); }  // ⎠
             else                     { left = Unicode(0x239C);     // ⎜
                                        right = Unicode(0x239F); }  // ⎟
           }
        buffer[y].prepend(left);
        buffer[y] << right;

        // change internal pad characters to SPACE so that they will
        // not be removed later (as in add_frame())
        //
        buffer[y].map_pad();
      }

   Assert(is_rectangular());
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::add_outer_frame(PrintStyle style)
{
   add_outer_frame(get_frame_chars(style));
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::add_outer_frame(const UCS_string & chars)
{
   Assert(chars.size() == FC_COUNT);

   if (chars[FC_OUTER_TOP_LEFT] == UNI_SPACE)   return;   // no outer frame

   if (get_row_count() == 0)   // empty
      {
        UCS_string upper;
        upper << chars[FC_OUTER_TOP_LEFT] << chars[FC_OUTER_TOP_RIGHT];
        buffer.push_back(upper);

        UCS_string lower;
        lower << chars[FC_OUTER_BOTTOM_LEFT] << chars[FC_OUTER_BOTTOM_RIGHT];
        buffer.push_back(lower);

        Assert(is_rectangular());
        return;
      }

   // draw a bar left and right
   //
   loop(y, get_row_count())
      {
        buffer[y].prepend(chars[FC_OUTER_VERT]);
        buffer[y] << chars[FC_OUTER_VERT];

        // change internal pad characters to SPACE so that they will
        // not be removed later and the frame is printed correctly
        //
        buffer[y].map_pad();
      }

   // draw a bar on top and bottom.
   //
UCS_string hori(get_column_count(), chars[FC_OUTER_HORI]);

   buffer.insert(0, hori);
   buffer.push_back(hori);

   // draw the corners
   //
   {
     const int XX = get_column_count() - 1;
     const int YY = get_row_count() - 1;
     set_char(0,  0,  chars[FC_OUTER_TOP_LEFT]);
     set_char(0,  YY, chars[FC_OUTER_BOTTOM_LEFT]);
     set_char(XX, 0,  chars[FC_OUTER_TOP_RIGHT]);
     set_char(XX, YY, chars[FC_OUTER_BOTTOM_RIGHT]);
   }

   Assert(is_rectangular());
}
//────────────────────────────────────────────────────────────────────────────
ostream &
PrintBuffer::debug(ostream & out, const char * title) const
{
   if (title)   out << title << endl;

   if (get_row_count() == 0)
      {
        out << UNI_LINE_DOWN_RIGHT << UNI_LINE_DOWN_LEFT << endl
            << UNI_LINE_UP_RIGHT   << UNI_LINE_UP_LEFT
            << "  flags=" << HEX(col_info.flags)
            << "  len="  << col_info.int_len
            << "."   << col_info.fract_len
            << endl << endl;
        return out;
      }

   out << UNI_LINE_DOWN_RIGHT;
   loop(w, get_column_count())   out << UNI_LINE_HORI;
   out << UNI_LINE_DOWN_LEFT << endl;

   loop(y, get_row_count())
   out << UNI_LINE_VERT << buffer[y] << UNI_LINE_VERT << endl;

   out << UNI_LINE_UP_RIGHT;
   loop(w, get_column_count())   out << UNI_LINE_HORI;
   out << UNI_LINE_UP_LEFT
       << " flg=" << HEX(col_info.flags)
       << " il="  << col_info.int_len
       << " fl="   << col_info.fract_len
       << " rl="   << col_info.real_len
       << " ÷l="   << col_info.denom_len
       << endl << endl;

   return out;
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::append_col(const PrintBuffer & pb1)
{
   Assert(get_row_count() == pb1.get_row_count());

   loop(h, get_row_count())   buffer[h] << pb1.buffer[h];
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::append_ucs(const UCS_string & ucs)
{
   if (buffer.size() == 0)   // empty buffer (no lines yet) : add ucs
      {
        buffer.push_back(ucs);
        return;
      }

const int size = ucs.size();
   if (size < get_column_count())  // new line is shorter: pad it)
      {
        UCS_string ucs1(ucs);
        UCS_string pad(get_column_count() - size, UNI_PAD_r_oCol);
        ucs1 << pad;
        buffer.push_back(ucs1);
        return;
      }

   if (size > get_column_count())   // new line is longer: pad PrintBufer
      {
        UCS_string pad(ucs.size() - get_column_count(), UNI_PAD_r_nCol);
        loop(h, get_row_count())   buffer[h] << pad;
        buffer.push_back(ucs);
        return;
      }

   buffer.push_back(ucs);
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::append_aligned(const UCS_string & ucs, Unicode align)
{
   Assert(is_rectangular());

int ucs_pos = -1;
   loop(u, ucs.size())
      {
        if (ucs[u] == align)
           {
             ucs_pos = u;
             break;
           }
      }

int this_pos = -1;
   loop(y, buffer.size())
      {
        const UCS_string & row = buffer[y];
        loop(u, row.size())
          {
            if (row[u] == align)
               {
                 this_pos = u;
                 break;
               }
          }
        if (this_pos != -1)   break;
      }

const int ucs_w = ucs.size();
const int this_w = get_column_count();

int ucs_l = 0;    // padding left of ucs
int ucs_r = 0;    // padding right of ucs
int this_l = 0;   // padding left of this
int this_r = 0;   // padding right of this

   if (ucs_pos == -1)       // no align char in ucs
      if (this_pos == -1)   // no align char in this pb
         {
           // no align char at all: align at right border
           //
           //      TTTTttt
           //      UUUUuuu
           //
           if (this_w > ucs_w)   ucs_l = this_w - ucs_w;
           else                  this_l = ucs_w - this_w;
         }
      else                  // align char only in this pb
         {
           //
           //      TTTTttt.ttt
           //      UUUUuuu
           //
           ucs_r = this_w - this_pos;
           if (this_pos > ucs_w)   ucs_l = this_pos - ucs_w;
           else                    this_l = ucs_w - this_pos;
         }
   else                     // align char in ucs
      if (this_pos == -1)   // no align char in this pb
         {
           //
           //      TTTTttt
           //      UUUUuuu.uuu
           //
           this_r = ucs_w - ucs_pos;
           if (ucs_pos > this_w)   this_l = ucs_pos - this_w;
           else                    ucs_l = this_w - ucs_pos;
         }
      else                  // align char in this pb
         {
           //
           //      TTTTttt.tttTTTT
           //      UUUUuuu.uuuUUUU
           //
           if (ucs_pos > this_pos)   this_l = ucs_pos - this_pos;
           else                      ucs_l = this_pos - ucs_pos;
           const int uu = ucs_w - ucs_pos;
           const int tt = this_w - this_pos;
           if (uu > tt)   this_r = uu - tt;
           else           ucs_r = tt - uu;
         }

   Assert(ucs_l >= 0);
   Assert(ucs_r >= 0);
   Assert(this_l >= 0);
   Assert(this_r >= 0);

UCS_string ucs1;

   if (ucs_l > 0)   ucs1 << UCS_string(ucs_l, UNI_SPACE);
   ucs1 << ucs;
   if (ucs_r > 0)   ucs1 << UCS_string(ucs_r, UNI_SPACE);

   if (this_l > 0)   pad_l(UNI_SPACE, this_l);
   if (this_r > 0)   pad_r(UNI_SPACE, this_r);

   buffer.push_back(ucs1);

   Assert(is_rectangular());
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::add_column(Unicode pad, int32_t pad_count, const PrintBuffer & pb)
{
   if (get_row_count() != pb.get_row_count())
      {
         debug(CERR, "this");
         pb.debug(CERR, "pb");
      }

   Assert(get_row_count() == pb.get_row_count());

   if (pad_count)
      {
        UCS_string ucs(pad_count, pad);
        loop(y, get_row_count())   buffer[y] << ucs;
      }

   loop(y, get_row_count())   buffer[y] << pb.buffer[y];
}
//────────────────────────────────────────────────────────────────────────────
void PrintBuffer::add_row(const PrintBuffer & pb)
{
   buffer.reserve(buffer.size() + pb.get_row_count());
   loop(h, pb.get_row_count())   buffer.push_back(pb.buffer[h]);
}
//────────────────────────────────────────────────────────────────────────────
bool
PrintBuffer::is_rectangular() const
{
   if (get_row_count())
      {
        const ShapeItem w = get_column_count();
        loop(h, get_row_count())
           {
             if (get_column_count(h) != w)    return false;
           }
      }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
const char *
PrintBuffer::frame_lines(int cs)
{
   // index: the legs present: 1 (N) + 2 (E) + 4 (S) + 8 (W)
   //
   //                     0123456789012345
   switch(cs)
      {
        case PST_CS_NONE:   return "                ";   // no frame
        case PST_CS_ASCII:  return " |-'||.+-'-+.+++";
        case PST_CS_THIN:   return " ╵╶└╷│┌├╴┘─┴┐┤┬┼";
        case PST_CS_THICK:  return " ╹╺┗╻┃┏┣╸┛━┻┓┫┳╋";
        case PST_CS_DOUBLE: return "   ╚ ║╔╠ ╝═╩╗╣╦╬";   // no single legs
        case PST_CS_PARENS: return "   (  (  )  )   ";   // add_parentheses()
      }

   FIXME;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
PrintBuffer::make_frame_chars(const UCS_string & lines1,
                              const UCS_string & lines2,
                              const UCS_string & decor)
{
   Assert(lines1.size() == 16);
   Assert(lines2.size() == 16);
   Assert(decor.size() == FC_DECOR_COUNT);

   // the 81 line-drawing characters. A character whose legs are all in
   // the same line style (or absent) is taken from that line style. The
   // characters with legs in both line styles are not (yet) needed and
   // remain blank.
   //
UCS_string chars(FC_LINES_COUNT, UNI_SPACE);
   loop(fc, FC_LINES_COUNT)
      {
        const int f = fc;
        const int legs[4] = { f / 27, f / 9 % 3, f / 3 % 3, f % 3 };
        int mask1 = 0;   // legs in line style 1
        int mask2 = 0;   // legs in line style 2
        loop(l, 4)
           {
             if (legs[l] == 1)   mask1 |= 1 << l;
             if (legs[l] == 2)   mask2 |= 1 << l;
           }

        if      (mask2 == 0)   chars[fc] = lines1[mask1];
        else if (mask1 == 0)   chars[fc] = lines2[mask2];
      }

   chars << decor;
   Assert(chars.size() == FC_COUNT);
   return chars;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
PrintBuffer::get_frame_chars(PrintStyle pst)
{
   // the frame decorators and options, blank if not wanted. Whether a
   // decorator for a property of the value (∼ + ⊖ ⌽) applies is decided by
   // add_frame().
   //
const bool plain = pst & PST_PLAIN;
const bool nars  = pst & PST_NARS;
UCS_string decor;
   decor << (plain ? UNI_SPACE : nars ? UNI_NUMBER_SIGN : UNI_DOWN_ARROW)
         << (plain ? UNI_SPACE : nars ? UNI_NUMBER_SIGN : UNI_RIGHT_ARROW)
         << (plain ? UNI_SPACE : UNI_TILDE_OPERATOR)
         << (plain ? UNI_SPACE : UNI_PLUS)
         << (plain ? UNI_SPACE : UNI_ELEMENT)
         << (plain ? UNI_SPACE : UNI_CIRCLE_BAR)
         << (plain ? UNI_SPACE : UNI_CIRCLE_STILE)
         << ((pst & PST_CS_MASK) == PST_CS_PARENS ? UNI_L_PARENT : UNI_SPACE);

   // line style 1: the (inner) frame, line style 2: the outer frame
   //
   return make_frame_chars(UCS_string(UTF8_string(frame_lines(pst & PST_CS_MASK))),
                           UCS_string(UTF8_string(frame_lines(pst >> 4 & PST_CS_MASK))),
                           decor);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
PrintBuffer::without_outer_frame(const UCS_string & chars)
{
   // blank all line-drawing characters with a leg in line style 2
   //
UCS_string ret(chars);
   loop(fc, FC_LINES_COUNT)
      {
        if (fc / 27 == 2 || fc / 9 % 3 == 2 || fc / 3 % 3 == 2 || fc % 3 == 2)
           ret[fc] = UNI_SPACE;
      }
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
PrintBuffer
PrintBuffer::item_representation(const Cell & cell, const PrintContext & pctx,
                                 const UCS_string & frame_chars)
{
   if (cell.is_pointer_cell())
      return static_cast<const PointerCell &>(cell)
                        .character_representation(pctx, frame_chars);

   return cell.character_representation(pctx);
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::align(ColInfo & cols)
{
   // this PrintBuffer is one (possibly nested) APL value.
   // Align the buffer:
   //
   // to the J (in a column containing complex numbers), or
   // to the decimal point (in a column containing non-complex numbers), or
   // to the left (in a column containing text or nested values).
   //
   // make sure that the item is (and remains) rectangular).
   //
   Assert(is_rectangular());

   if      (cols.flags & has_j)          align_j(cols);
   else if ((cols.flags & CT_NUMERIC))   align_dot(cols);
   else                                  align_left(cols);

   Assert(is_rectangular());
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::align_left(const ColInfo & COL_INFO)
{
   Log(LOG_printbuf_align)
      {
        CERR << "before align_left(), COL_INFO = " << COL_INFO.int_len
             << ":" << COL_INFO.fract_len
             << ":" << COL_INFO.real_len
             << ", this col = " << col_info.int_len
             << ":" << col_info.fract_len
             << ":" << col_info.real_len << endl;
        debug(CERR, 0);
      }

   if (col_info.int_len == COL_INFO.int_len)   return;   // no padding needed.

   Assert(col_info.int_len < COL_INFO.int_len);

const size_t diff = COL_INFO.int_len - col_info.int_len;

   if (buffer.size())   pad_r(UNI_PAD_l_INT, diff);
   else                 buffer.push_back(UCS_string(diff, UNI_PAD_l_INT));

   col_info.int_len = COL_INFO.int_len;

   Log(LOG_printbuf_align)   debug(CERR, "after align_left()");
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::align_dot(const ColInfo & COL_INFO)
{
   // align this PrintBuffer (one value) at the decimal dot of COL_INFO.
   //
   // COL_INFO is the desired ColInfo of the entire column while
   // col_info (a member of this PrintBuffer) is the (smaller) item
   // being aligned
   //
   Log(LOG_printbuf_align)
      {
        CERR << "before align_dot():" << endl
             << "desired COL_INFO = "
                "i-"  << COL_INFO.int_len
             << " f-" << COL_INFO.fract_len
             << " r-" << COL_INFO.real_len
             << " ÷"  << COL_INFO.denom_len << endl
             << "this row         = "
                "i-"  << col_info.int_len
             << " f-" << col_info.fract_len
             << " r-" << col_info.real_len
             << " ÷"  << col_info.denom_len << endl;
        debug(CERR, 0);
      }

   Assert(buffer.size() > 0);

   // make sure that consider() has worked. real_len is always a genuine
   // upper bound (the widest element in the column, numeric or not, per
   // do_PrintBuffer()'s reconciliation). int_len/fract_len/denom_len are
   // only guaranteed to bound a NUMERIC item's own int_len/fract_len/
   // denom_len -- for a non-numeric item, its own int_len (ab)used to
   // hold that item's total width (see PrintBuffer.hh's documented dual
   // meaning), a different quantity from COL_INFO.int_len's "digits
   // before the decimal point" once the column is mixed, so the two are
   // not comparable there.
   //
   Assert(COL_INFO.real_len   >= col_info.real_len);

   if (col_info.flags & CT_NUMERIC)
      {
        Assert(COL_INFO.int_len    >= col_info.int_len);
        Assert(COL_INFO.fract_len  >= col_info.fract_len);
        Assert(COL_INFO.denom_len  >= col_info.denom_len);
        // numeric items are aligned at the decimal dot. First pad the
        // integer part with spaces to the left
        //
        if (COL_INFO.int_len > col_info.int_len)
           {
             const size_t diff = COL_INFO.int_len - col_info.int_len;
             pad_l(UNI_PAD_l_INT, diff);
             col_info.real_len += diff;
             col_info.int_len  += diff;
           }

        if (col_info.denom_len)   // quotient: maybe pad right with spaces
           {
             const size_t diff = COL_INFO.real_len - col_info.real_len;
             if (diff)
                {
                  pad_r(UNI_PAD_r_FRACT, diff);
                  col_info.real_len += diff;
                }
           }
        else
           {
             if (COL_INFO.fract_len > col_info.fract_len)
                {
                  pad_fraction(COL_INFO.fract_len, COL_INFO.have_expo());
                }

             if (COL_INFO.real_len > col_info.real_len)
                {
                  const size_t diff = COL_INFO.real_len - col_info.real_len;
                  if (!(COL_INFO.flags & (real_has_E | imag_has_E))
                   || col_info.have_expo())       // or has one already 
                     {
                       pad_r(UNI_PAD_r_FRACT, diff);
                     }
                  else                        // no expo yet: create one
                     {
                       Assert1(diff >= 2);
                       pad_r(UNI_E, 1);
                       pad_r(UNI_0, 1);
                       pad_r(UNI_PAD_r_FRACT, diff - 2);
                     }
                  col_info.real_len = COL_INFO.real_len;
                }
           }
      }
   else
      {
        // char items are right aligned
        //
        size_t LEN = COL_INFO.total_len();
        size_t len = col_info.total_len();
        if (LEN > len)
           {
             const size_t diff = LEN - len;
             pad_l(UNI_PAD_l_STRING, diff);
             col_info.int_len   = COL_INFO.int_len;
             col_info.fract_len = COL_INFO.fract_len;
             col_info.real_len = COL_INFO.real_len;
           }
      }

   Log(LOG_printbuf_align)   debug(CERR, "after align_dot()");
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::align_j(const ColInfo & COL_INFO)
{
   // align all items in this PrintBuffer (= one APL output column)
   // at the complex J.
   //
   Log(LOG_printbuf_align)
      {
        CERR << "before align_j(), COL_INFO = " << COL_INFO.int_len
             << ":" << COL_INFO.fract_len
             << ":" << COL_INFO.real_len
             << ", this col = " << col_info.int_len
             << ":" << col_info.fract_len
             << ":" << col_info.real_len << endl;
        debug(CERR, 0);
      }

   Assert(buffer.size() > 0);

   Assert(COL_INFO.real_len >= col_info.real_len);
   Assert(COL_INFO.imag_len >= col_info.imag_len);

   if (col_info.flags & CT_NUMERIC)
      {
        // J-align numeric items
        if (COL_INFO.real_len > col_info.real_len)
           {
             const size_t diff = COL_INFO.real_len - col_info.real_len;
             pad_l(UNI_PAD_l_INT, diff);
             col_info.real_len = COL_INFO.real_len;
           }

        if (COL_INFO.imag_len > col_info.imag_len)
           {
             const size_t diff = COL_INFO.imag_len - col_info.imag_len;
             pad_r(UNI_PAD_r_FRACT, diff);
             col_info.imag_len = COL_INFO.imag_len;
           }
      }
   else
      {
        // right-align char items
        size_t LEN = COL_INFO.real_len + COL_INFO.imag_len;
        size_t len = col_info.real_len + col_info.imag_len;
        if (LEN > len)
           {
             const size_t diff = LEN - len;
             pad_l(UNI_PAD_l_STRING, diff);
             col_info.real_len = COL_INFO.real_len;
             col_info.imag_len = COL_INFO.imag_len;
           }
      }

   Log(LOG_printbuf_align)   debug(CERR, "after align_j()");
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::format_complex_column(const cValue & value, ShapeItem x,
                                   const PrintContext & pctx,
                                   PrintBuffer * item_matrix, bool strict)
{
   // A column of a simple numeric array that contains (somewhere) a complex
   // number, which IBM APL2 then stores as a complex array as a whole. With
   // strict (STRICT_IBM_APL2_FORMATTING), the layout follows IBM APL2
   // (verified with APL2 itself, see FORMAT_CPLX.tc and FORMAT_COLS.tc):
   //
   // * the real parts of a column form one sub-column and the imaginary
   //   parts another one. Each sub-column is scaled as a whole if one of its
   //   items needs it, and has the same number of fractional digits (lrm
   //   p. 17: "Each column ... is formatted in the same way").
   //
   // * the width of a sub-column is that of its items when aligned at their
   //   decimal points (and E's), e.g. 7 for the imaginary parts 1.473 and
   //   222.43 (integer part 3 + fraction 4), even though the parts are not
   //   placed like that:
   //
   // * an unscaled real part is right-aligned (i.e. directly left of the J
   //   if there is one); a scaled real part is aligned at its decimal point.
   //   The J follows the real part directly, and the imaginary part follows
   //   the J directly, so that a complex number never contains blanks.
   //   The item is padded on the right to the width of the column, e.g.
   //   (⎕PP←5):
   //
   //       2.50E¯7                    1J1.473
   //       3.33E2J22.25               2J222.43
   //
   // * this also applies to a column without any complex number (e.g.
   //   ⎕←2 2⍴1.5 1J1 22.25 1), whose numbers are therefore right-aligned
   //   rather than aligned at their decimal points (unless scaled).
   //
   // Without strict (GNU APL's traditional output), the real sub-column is
   // as wide as above, the imaginary sub-column only as wide as its longest
   // part, every real part is right-aligned
   // (so that the J's are aligned), and a column without any J is aligned
   // at the decimal points like a column of real numbers.
   //
   // A complex number whose imaginary part is not displayed (lrm p. 13) is
   // shown as its real part only; one whose real part is not displayed has
   // the real part 0.
   //
const ShapeItem cols = value.get_last_shape_item();
const ShapeItem rows = value.element_count()/cols;
std::vector<APL_Float> real(rows, 0.0);
std::vector<APL_Float> imag(rows, 0.0);
std::vector<bool> all_used(rows, true);
std::vector<bool> has_imag(rows, false);

// the real parts that can cause the scaling of the real sub-column: not an
// integer in GNU APL's traditional output (GNU APL rule 1, as in a column
// of real numbers; 2 1⍴1J1 123456 scaled 123456 to 1.2346E5, Bugs34 #23)
std::vector<bool> real_may_scale(rows, true);
std::vector<APL_Integer> real_int(rows, 0);

   loop(y, rows)
      {
        Cell cache;
        const Cell & cell = value.get_cravel(x + y*cols, cache);
        if (cell.is_integer_cell())
           {
             real[y] = APL_Float(cell.get_int_value());
             real_int[y] = cell.get_int_value();
             if (!strict)   real_may_scale[y] = false;
             continue;
           }

        real[y] = cell.get_real_value();
        if (!cell.is_complex_cell())   continue;

        imag[y] = cell.get_imag_value();
        switch(ComplexCell::exponent_rule(real[y], imag[y], pctx.get_PP()))
           {
             case ComplexCell::DP_REAL: break;
             case ComplexCell::DP_IMAG: real[y] = 0.0;
                                        has_imag[y] = true;
                                        break;
             default: has_imag[y] = imag[y] != 0.0;   // lrm p. 13
           }
      }

bool any_imag = false;
   loop(y, rows)   if (has_imag[y])   any_imag = true;

std::vector<UCS_string> real_ucs(rows);
std::vector<UCS_string> imag_ucs(rows);
bool real_scaled = false;
bool imag_scaled = false;
size_t real_len = format_sub_column(real, all_used, pctx,
                                    real_ucs, real_scaled, &real_may_scale,
                                    &real_int);
size_t imag_len = any_imag ? format_sub_column(imag, has_imag, pctx,
                                               imag_ucs, imag_scaled)
                           : 0;

   if (!strict && any_imag)   // the width of the longest imaginary part
      {
        // the real parts keep the width as if aligned at their decimal
        // points (as in IBM APL2), although they are right-aligned
        imag_len = 0;
        loop(y, rows)
           {
             if (!has_imag[y])   continue;
             UCS_string i = imag_ucs[y];
             trim_padding(i, true, true);
             if (imag_len < i.size())   imag_len = i.size();
           }
      }

const size_t width = real_len + (any_imag ? 1 + imag_len : 0);

   loop(y, rows)
      {
        UCS_string ucs;
        if (!strict && !any_imag)   // like a column of real numbers
           {
             ucs = real_ucs[y];
           }
        else if (strict && real_scaled)   // aligned at the decimal point
           {
             ucs = real_ucs[y];
             trim_padding(ucs, false, true);
           }
        else               // right-aligned
           {
             UCS_string r = real_ucs[y];
             trim_padding(r, true, true);
             ucs = UCS_string(real_len - r.size(), UNI_PAD_l_INT);
             ucs << r;
           }

        if (has_imag[y])
           {
             UCS_string i = imag_ucs[y];
             trim_padding(i, true, true);
             ucs << UNI_J << i;
           }

        if (ucs.size() < width)
           ucs << UCS_string(width - ucs.size(), UNI_PAD_r_FRACT);

        ColInfo info;
        info.flags = int(CT_COMPLEX) | has_j;
        info.int_len = width;
        info.real_len = width;
        info.imag_len = 0;
        item_matrix[y*cols + x] = PrintBuffer(ucs, info);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::align_mixed_column(const cValue & value, ShapeItem x,
                                PrintBuffer * item_matrix)
{
   // align column x of a mixed (character and number) or nested value
   // without STRICT_IBM_APL2_FORMATTING: the simple scalar real numbers
   // that are not scaled are aligned at their decimal points (an integer as
   // if it had a decimal point after its last digit). The block of aligned
   // numbers and all other items (incl. scaled and complex numbers) are
   // then right-aligned to the width of the widest of them.
   //
const ShapeItem cols = value.get_last_shape_item();
const ShapeItem rows = value.element_count()/cols;
std::vector<int> dot(rows, -1);   // position of the "decimal point", or -1
int int_max = 0;     // max. characters before the decimal point
int fract_max = 0;   // max. characters from the decimal point on
int other_max = 0;   // max. width of the other items
   loop(y, rows)
      {
        const PrintBuffer & item = item_matrix[y*cols + x];
        Cell cache;
        const Cell & cell = value.get_cravel(x + y*cols, cache);
        const int w = item.get_column_count();
        if (!cell.is_numeric() || item.get_row_count() == 0)
           {
             if (other_max < w)   other_max = w;
             continue;
           }

        const UCS_string & line = item.buffer[0];
        int pos = line.size();
        bool other = false;   // scaled or complex
        loop(c, line.size())
           {
             if (line[c] == UNI_FULLSTOP)                  pos = c;
             else if (line[c] == UNI_E || line[c] == UNI_J)   other = true;
           }
        if (other)
           {
             if (other_max < w)   other_max = w;
             continue;
           }

        dot[y] = pos;
        if (int_max < pos)                 int_max = pos;
        if (fract_max < w - pos)           fract_max = w - pos;
      }

const int numbers_w = int_max + fract_max;
const int width = numbers_w < other_max ? other_max : numbers_w;
   loop(y, rows)
      {
        PrintBuffer & item = item_matrix[y*cols + x];
        const int w = item.get_column_count();
        if (dot[y] == -1)   // not a simple real number: right-aligned
           {
             if (width > w)   item.pad_l(UNI_PAD_l_INT, width - w);
           }
        else                // number: aligned at its decimal point
           {
             const int left = width - numbers_w + int_max - dot[y];
             const int right = fract_max - (w - dot[y]);
             if (left > 0)    item.pad_l(UNI_PAD_l_INT, left);
             if (right > 0)   item.pad_r(UNI_PAD_r_FRACT, right);
           }

        ColInfo & ci = item.get_info();
        ci.int_len  = width;   // as for a character item
        ci.fract_len = 0;
        ci.real_len = width;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::trim_padding(UCS_string & ucs, bool left, bool right)
{
size_t from = 0;
size_t to = ucs.size();
   if (left)
      while (from < to && (ucs[from] == UNI_SPACE ||
                           is_iPAD_char(ucs[from])))   ++from;
   if (right)
      while (to > from && (ucs[to - 1] == UNI_SPACE ||
                           is_iPAD_char(ucs[to - 1])))   --to;
   ucs = UCS_string(ucs, from, to - from);
}
//────────────────────────────────────────────────────────────────────────────
size_t
PrintBuffer::format_sub_column(const std::vector<APL_Float> & values,
                               const std::vector<bool> & used,
                               const PrintContext & pctx,
                               std::vector<UCS_string> & result,
                               bool & scaled,
                               const std::vector<bool> * may_scale,
                               const std::vector<APL_Integer> * int_values)
{
   // may_scale (if given): the items that can cause the scaling of the
   // sub-column (others are only formatted like the rest)
   //
   if (may_scale == 0)   may_scale = &used;

   scaled = false;
   loop(y, values.size())
      {
        if ((*may_scale)[y] &&
            FloatCell::need_scaling(values[y], pctx.get_PP()))
           {
             scaled = true;
             break;
           }
      }

PrintContext pctx1(pctx);
   pctx1.set_style(scaled ? PrintStyle(pctx.get_style() |  PST_SCALED)
                          : PrintStyle(pctx.get_style() & ~PST_SCALED));

   // format the items like the items of a real column (steps 2. and 3. of
   // do_PrintBuffer()), so that they get the same format (scaling and
   // number of fractional digits) and are aligned at their decimal points.
   //
std::vector<PrintBuffer> items(values.size());
ColInfo col_info;
   loop(y, values.size())
      {
        if (!used[y])   continue;
        if (!scaled && int_values && !(*may_scale)[y])   // an exact integer
           items[y] = IntCell((*int_values)[y]).character_representation(pctx1);
        else
           items[y] = FloatCell(values[y]).character_representation(pctx1);
        col_info.consider(items[y].get_info());
      }
   if (col_info.real_len < (col_info.int_len + col_info.denom_len))
      col_info.real_len = col_info.int_len + col_info.denom_len;

size_t width = 0;
   loop(y, values.size())
      {
        if (!used[y])   continue;
        items[y].align(col_info);
        result[y] = items[y].get_line(0);
        if (width < result[y].size())   width = result[y].size();
      }

   return width;
}
//────────────────────────────────────────────────────────────────────────────
void
PrintBuffer::pad_fraction(int wanted_fract_len, bool want_expo)
{
const int diff = wanted_fract_len - col_info.fract_len;
   Assert1(diff > 0);

      // copy integer part of this PrintBuffer to to new_buf
      //
UCS_string new_buf(buffer[0], 0, col_info.int_len);

      // copy fractional part to new_buf. If the number has no exponent part,
      // then we fill with spaces. Otherwise fill with '0', possibly inserting
      // a decimal point.
      //
      loop(f, col_info.fract_len)
          new_buf << buffer[0][col_info.int_len + f];
      if (!want_expo)                     // no exponent, e.g. 1,0
         {
           loop(d, diff)   new_buf << UNI_PAD_r_FRACT;
         }
      else if (col_info.fract_len == 0)   // no fractional part (yet), e.g. 1E2
         {
           new_buf << UNI_FULLSTOP;
           loop(d, diff - 1)   new_buf << UNI_0;
         }
      else
         {
           loop(d, diff)   new_buf << UNI_0;
         }

   // copy exponent part
   //
   for (int ex = col_info.int_len + col_info.fract_len;
        ex < buffer[0].ssize(); ++ex)
       new_buf << buffer[0][ex];

   col_info.fract_len = wanted_fract_len;
   col_info.real_len += diff;

   buffer[0] = new_buf;

   // if buffer is multi-len then pad remaining line to new length
   //
   for (ShapeItem h = 1; h < get_row_count(); ++h)
      {
        const int diff = new_buf.size() - get_column_count(h);
        if (diff > 0)   buffer[h] << UCS_string(diff, UNI_PAD_r_FRACT);
      }
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
PrintBuffer::separator_rows(ShapeItem y, const cValue & value, bool nested,
                            sRank rk1, sRank rk2)
{
   if (y == 0)   return 0;

const Shape & shape = value.get_shape();

ShapeItem prod = 1;
ShapeItem ret = 0;
   loop(r, shape.get_rank() - 2)
       {
         prod *= shape.get_shape_item(shape.get_rank() - r - 2);
         if (y % prod == 0)   ++ret;
         else                 break;
       }

   if (nested)   // see lrm p. 138
      {
        /* lrm p. 138. Indeed, examples with IBM APL2 show that:

        N←⊂ 3 3⍴'abcdefghi'   ⍝ nested
        Q←3 3⍴⍳9 ◊ Q[2;2] ← N ◊ Q

        gives:

 1    2  3 

 4  abc  6 
    def    
    ghi    

 7    8  9 

        i.e. with one blank line before and after the nested N.

         */
        const int max_rk = max(rk1, rk2);
        if (max_rk > 1)   ret += max_rk - 1;
      }

   return ret;
}
//════════════════════════════════════════════════════════════════════════════
ostream &
operator << (ostream & out, const PrintBuffer & pb)
{
   out << endl;   return pb.debug(out);
}
//════════════════════════════════════════════════════════════════════════════
