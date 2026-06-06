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

#include <math.h>
#include <string.h>

#include "Backtrace.hh"
#include "Common.hh"
#include "FloatCell.hh"
#include "Heapsort.hh"
#include "Output.hh"
#include "PrintBuffer.hh"
#include "PrintOperator.hh"
#include "Symbol.hh"
#include "UCS_string.hh"
#include "UTF8_string.hh"
#include "Value.hh"

/// uncomment to NOT replace iPAD chars with blanks
//#define KEEP_iPAD_characters

ShapeItem UCS_string::total_count = 0;
ShapeItem UCS_string::total_id = 0;

//════════════════════════════════════════════════════════════════════════════
UCS_string::UCS_string()
{
   create(LOC);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string::UCS_string(Unicode uni)
{
  push_back(uni);
  create(LOC);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string::UCS_string(const Unicode * data, size_t len)
{
   reserve(2*len);
   loop(l, len)   push_back(data[l]);
   create(LOC);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string::UCS_string(size_t len, Unicode uni)
{
   reserve(2*len);
   loop(l, len)   push_back(uni);
   create(LOC);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string::UCS_string(const UCS_string & ucs)
   : vector<Unicode>(ucs)
{
   create(LOC);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string::UCS_string(const UCS_string & ucs, size_t pos)
{
   reserve(ucs.size());
   while (pos < ucs.size())    push_back(ucs[pos++]);
   create(LOC);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string::UCS_string(const UCS_string & ucs, size_t pos, size_t len)
{
   if (len > (ucs.size() - pos))   len = ucs.ssize() - pos;
   reserve(2*len);
   loop(l, len)   push_back(ucs[pos + l]);
   create(LOC);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string::UCS_string(const PrintBuffer & pb, sRank rank, int quad_PW)
{
   create(LOC);

   if (pb.get_row_count() == 0)   return;      // empty PrintBuffer

const int total_width = pb.get_column_count();

   // initialize chunk_lengths, based on the first row of the PrintBuffer.
   // All subsequent rows are aligned to the first row, therefore the first
   // row can be taken as a prototype for all rows.
size_t chunk_len = 0;
vector<int> chunk_lengths;
   chunk_lengths.reserve(2*total_width/quad_PW);
   for (int col = 0; col < total_width; col += chunk_len)
       {
         chunk_len = pb.get_line(0).compute_chunk_length(quad_PW,col);
         chunk_lengths.push_back(chunk_len);
       }

   // print rows, breaking at chunk_lengths
   //
   loop(row, pb.get_row_count())
       {
         if (row)   *this << UNI_LF;   // end previous row
         int brk_idx = 0;

         for (int col = 0; col < total_width; col += chunk_len)
               {
                 chunk_len = chunk_lengths[brk_idx++];

                 UCS_string trow(pb.get_line(row), col, chunk_len);
                 if (col && trow.size() && trow.back() == UNI_PAD_r_DEPTH)
                    trow.pop_back();   // depth marker blocks remove_trailing_padchars
                 trow.remove_trailing_padchars();
                 if (col && trow.size() == 0)   continue;   // skip empty continuation
                 if (col)   *this << "\n      ";
                 *this << trow;
               }
       }

   // replace pad chars with blanks.
   //
#ifndef KEEP_iPAD_characters
   loop(u, size())
       {
         if (is_iPAD_char(at(u)))   at(u) = UNI_SPACE;
       }
#endif // KEEP_iPAD_characters
}
//────────────────────────────────────────────────────────────────────────────
UCS_string::UCS_string(APL_Float value, bool & scaled,
                       const PrintContext & pctx)
{
   create(LOC);

int quad_pp = pctx.get_PP();
   if (quad_pp > MAX_Quad_PP)   quad_pp = MAX_Quad_PP;
   if (quad_pp < MIN_Quad_PP)   quad_pp = MIN_Quad_PP;

const bool negative = (value < 0.0);
   if (negative)   value = -value;

int expo = 0;

   if (value >= 10.0)   // large number, positive exponent
      {
        if (value > BIG_FLOAT || !isnormal(value))   // something odd
           {
            if (isnormal(value) || isinf(value))   // rather large
               {
                 if (negative)   *this << u8"¯∞";
                 else            *this << u8"∞";
                 FloatCell::map_FC(*this);
               }
           else
               {
                 *this << "-nan-";
               }
             return;
           }

       while (value >= 1e16)   { value *= 1e-16;   expo += 16; }
       while (value >= 1e4)    { value *= 1e-4;    expo +=  4; }
       while (value >= 1e1)    { value *= 1e-1;    ++expo;     }
      }
   else if (value < 1.0)   // small number, negative exponent
      {
       if (value < 1e-305)   // very small number: make it 0
          {
            *this << UNI_0;
            return;
          }

       while (value < 1e-16)   { value *= 1e16;   expo -= 16; }
       while (value < 1e-4)    { value *= 1e4;    expo -=  4; }
       while (value < 1.0)     { value *= 10.0;   --expo;     }
      }

   // In theory, at this point, 1.0 ≤ value < 10.0. In reality value can
   // be outside, though, due to rounding errors.

   // create a string with quad_pp + 1 significant digits.
   // The last digit is used for rounding and then discarded.
   //
UCS_string digits;
   loop(d, (quad_pp + 2))
      {
        if (value >= 10.0)
           {
             // 10.0 or more is a rounding error from 9,999...
             digits << Unicode(10 + '0');
             while (digits.ssize() < (quad_pp + 2))   digits << UNI_0;
             break;
           }
        else if (value < 0.0)
           {
             // less than 0.0 is a rounding error from 0.000...
             while (digits.ssize() < (quad_pp + 2))   digits << UNI_0;
             break;
             digits << UNI_0;
             value = 0.0;
           }
        else
           {
             const int dig = int(value);
             value -= dig;
             value *= 10.0;
             digits << Unicode(dig + '0');
           }
      }

   if (digits[0] != '0')   digits.pop_back();

   // round last digit
   //
const Unicode last = digits.back();
   digits.pop_back();

   if (last >= '5')   digits.back() = Unicode(digits.back() + 1);
 
   // adjust carries of 2nd to last digit
   //
   for (int d = digits.size() - 1; d > 0; --d)   // all but first
       {
        if (digits[d] > '9')
           {
             digits[d] =     Unicode(digits[d]     - 10);
             digits[d - 1] = Unicode(digits[d - 1] +  1);
           }
       }

   // adjust carry of 1st digit
   //
   if (digits[0] > '9')
      {
        digits[0] = Unicode(digits[0] - 10);
        digits.insert(0, UNI_1);
        ++expo;
        digits.pop_back();
      }

   // remove trailing zeros
   //
   while (digits.size() > 1 && digits.back() == UNI_0)  digits.pop_back();

   // force scaled format if:
   //
   // value < .00001     ( value has ≥ 5 leading zeroes)
   // value > 10⋆quad_pp ( integer larger than ⎕PP)
   //
   if ((expo < -6) || (expo > quad_pp))   scaled = true;

   if (negative)   *this << UNI_OVERBAR;

   if (scaled)
      {
        *this << digits[0];       // integer part
        if (digits.size() > 1)   // fractional part
           {
             *this << UNI_FULLSTOP;
             loop(d, (digits.size() - 1))   *this << digits[d + 1];
           }
        if (expo < 0)
           {
             *this << UNI_E << UNI_OVERBAR << (-expo);
           }
        else if (expo > 0)
           {
             *this << UNI_E << expo;
           }
        else if (!(pctx.get_style() & PST_NO_EXPO_0)) // expo == 0
           {
             *this << UNI_E << UNI_0;
           }
      }
   else
      {
        if (expo < 0)   // 0.000...
           {
             *this << UNI_0 << UNI_FULLSTOP;
             loop(e, (-(expo + 1)))   *this << UNI_0;
             *this << digits;
           }
        else   // expo >= 0
           {
             loop(e, expo + 1)
                {
                  if (e < digits.ssize())   *this << digits[e];
                  else                      *this << UNI_0;
                }

             if ((expo + 1) < digits.ssize())   // there are fractional digits
                {
                  *this << UNI_FULLSTOP;
                  for (int e = expo + 1; e < digits.ssize(); ++e)
                     {
                       if (e < digits.ssize())   *this << digits[e];
                       else                      break;
                     }
                }
           }
      }

   FloatCell::map_FC(*this);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string::UCS_string(istream & in)
{
   create(LOC);

   for (;;)
      {
        const Unicode uni = UTF8_string::getc(in);
        if (uni == Invalid_Unicode)   return;
        if (uni == UNI_LF)      return;
        *this << uni;
      }
}
//────────────────────────────────────────────────────────────────────────────
/// constructor
UCS_string::UCS_string(const Value & value)
{
   create(LOC);

   if (value.get_rank() > 1) RANK_ERROR;

const ShapeItem ec = value.element_count();
   reserve(ec);

   loop(e, ec)   *this << value.get_cravel(e).get_char_value();
}
//────────────────────────────────────────────────────────────────────────────
/// constructor
UCS_string::UCS_string(const Cell & cell)
{
   create(LOC);

   if (cell.is_character_cell())
      {
        *this << cell.get_char_value();
        return;
      }

   if (cell.is_integer_cell())
      {
        append_int(cell.get_int_value());
        return;
      }

   if (cell.is_float_cell())
      {
        *this << cell.get_real_value();
        return;
      }

   if (cell.is_complex_cell())
      {
        *this << cell.get_real_value() << UNI_J << cell.get_imag_value();
        return;
      }

   Assert(cell.is_pointer_cell());
const Value & value = *cell.get_pointer_value().get();

   if (value.get_rank() > 1)
      {
        MORE_ERROR() << "Bad rank " << value.get_rank()
                     << " when expecting a character string";
        RANK_ERROR;
      }

const ShapeItem ec = value.element_count();
   reserve(ec);

   loop(e, ec)   *this << value.get_cravel(e).get_char_value();
}
//────────────────────────────────────────────────────────────────────────────
#if UCS_tracking
void UCS_string::create(const char * loc)
{
   ++total_count;
   instance_id = ++total_id;
   cerr << setfill('0') << endl << "@@ " << setw(5) << instance_id
        << " NEW ##" << total_count << " " << loc
        << " c= " << Backtrace::caller(3) << setfill(' ') << endl;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string::~UCS_string()
{
   --total_count;
   cerr << setfill('0') << endl << "@@ " << setw(5) << instance_id
        << " DEL ##" << total_count
        << " c= " << Backtrace::caller(3) << setfill(' ') << endl;
}

#endif
//────────────────────────────────────────────────────────────────────────────
int
UCS_string::atoi() const
{
int ret = 0;
bool negative = false;

   loop(s, size())
      {
        const Unicode uni = at(s);

        if (!ret && Avec::is_white(uni))   continue;   // leading whitespace

        if (uni == UNI_MINUS || uni == UNI_OVERBAR)
           {
             negative = true;
             continue;
           }

        if (uni < UNI_0)                break;      // non-digit
        if (uni > UNI_9)                break;      // non-digit

        ret *= 10;
        ret += uni - UNI_0;
      }

   return negative ? -ret : ret;
}
//────────────────────────────────────────────────────────────────────────────
int
UCS_string::compute_chunk_length(int quad_PW, int col) const
{
   // the space available is ⎕PW for the first line or ⎕PW less the usual
   // 6 space indentation for subsequent lines.
   //
const int chunk_len = col ? quad_PW - 6 : quad_PW;

   // if the rest of the line (from col to the end of the line) is small,
   //  then return the length of the rest.
   //
int pos = col + chunk_len;
   if (pos >= ssize())   return ssize() - col;

   // otherwise the rest does not fit into ⎕PW. Find a whitespace
   // at which the line should be broken.
   //
   while (--pos > col)
      {
         const Unicode uni = at(pos);
         if (uni == UNI_SPACE || is_iPAD_char(uni))
            {
               return pos - col + 1;
            }
      }

   // no whitespace found (the entire string may not have any).
   // Break at chunk_len.
   //
   return chunk_len;
}
//────────────────────────────────────────────────────────────────────────────
bool
UCS_string::contains(Unicode uni) const
{
   loop(u, size())   if (at(u) == uni)   return true;
   return false;
}
//────────────────────────────────────────────────────────────────────────────
size_t
UCS_string::copy_black(UCS_string & dest, int idx) const
{
   // skip leading whitespace
   //
   while (idx < ssize() && at(idx) <= ' ')   ++idx;

bool single_quoted = false;
bool double_quoted = false;
   while (idx < ssize() && (at(idx) >  ' ' || single_quoted || double_quoted))
         {
           const Unicode uni = at(idx++);
           dest << uni;
           if (uni == UNI_SINGLE_QUOTE)   single_quoted = !single_quoted;
           if (uni == UNI_DOUBLE_QUOTE)   double_quoted = !double_quoted;
         }

   // skip trailing whitespace
   //
   while (idx < ssize() && at(idx) <= ' ')   ++idx;
   return idx;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::do_escape(bool double_quoted) const
{
const char * hex = "0123456789abcdef";
UCS_string ret;
   ret.reserve(size());

   if (double_quoted)
      {
        loop(s, size())
           {
             const Unicode uni = at(s);
             switch(uni)
                {
                  case UNI_BEL:            ret << "\\a";    continue;
                  case UNI_BS:             ret << "\\b";    continue;
                  case UNI_HT:             ret << "\\t";    continue;
                  case UNI_LF:             ret << "\\n";    continue;
                  case UNI_VT:             ret << "\\v";    continue;
                  case UNI_FF:             ret << "\\f";    continue;
                  case UNI_CR:             ret << "\\r";    continue;
                  case UNI_DOUBLE_QUOTE:   ret << "\\\"";   continue;
                  case UNI_BACKSLASH:      ret << "\\\\";   continue;
                  default:                       break;
                }

             // none of the above
             //
             if (uni >= UNI_SPACE && uni < UNI_DELETE)
                {
                  ret << uni;
                  continue;
                }

             if (uni <= 0x0F)   // small ASCII
                {
                  ret << "\\x0";
                  ret << Unicode(hex[uni]);
                }
             else
                {
                  ret << uni;
                }
           }
      }
   else   // single-quoted
      {
        loop(s, size())
           {
             const Unicode uni = at(s);
             ret << uni;
             if (uni == UNI_SINGLE_QUOTE)   ret << uni;   // ' → ''
           }
      }

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
UCS_string::double_quote_count(bool in_quote2) const
{
ShapeItem count = 0;
bool in_quote1 = false;
   loop(s, size())
       {
        const Unicode uni = at(s);
        switch(uni)
           {
             case UNI_SINGLE_QUOTE:
                  if (!in_quote2)   in_quote1 = ! in_quote1;
                  break;

             case UNI_DOUBLE_QUOTE:
                  if (!in_quote1)
                     {
                       ++count;
                       in_quote2 = ! in_quote2;
                     }
                  break;

             case UNI_BACKSLASH:
                  if (in_quote2)    ++s;   // ignore next char inside ""
                  break;

             case UNI_NUMBER_SIGN:
             case UNI_COMMENT:
                  if (!(in_quote1 || in_quote2))   return count;

             default:                            ;
           }
       }

   return count;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
UCS_string::double_quote_first() const
{
bool in_quote1 = false;
bool in_quote2 = true;
   loop(s, size())
       {
        const Unicode uni = at(s);
        switch(uni)
           {
             case UNI_SINGLE_QUOTE:
                  if (!in_quote2)   in_quote1 = ! in_quote1;
                  break;

             case UNI_DOUBLE_QUOTE:
                  if (!in_quote1)   return s;
                  break;

             case UNI_BACKSLASH:
                  if (in_quote2)    ++s;   // ignore next char inside ""
                  break;

             case UNI_NUMBER_SIGN:
             case UNI_COMMENT:
                  if (in_quote1 || in_quote2)   ; // ignore # and ⍝ in atrings
                  else                          s = size();
                  break;

             default:                            ;
           }
       }


   return -1;   // no un-commented and un-escaped " found
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
UCS_string::double_quote_last() const
{
ShapeItem ret = -1;
bool in_quote1 = false;
bool in_quote2 = false;
   loop(s, size())
       {
        const Unicode uni = at(s);
        switch(uni)
           {
             case UNI_SINGLE_QUOTE:
                  if (!in_quote2)   in_quote1 = ! in_quote1;
                  break;

             case UNI_DOUBLE_QUOTE:
                  if (!in_quote1)   ret = s;
                  break;

             case UNI_BACKSLASH:
                  if (in_quote2)    ++s;   // ignotr next char inside ""
                  break;

             case UNI_NUMBER_SIGN:
             case UNI_COMMENT:
                  if (in_quote1 || in_quote2)   ; // ignore # and ⍝ in atrings
                  else                          s = size();
                  break;

             default:                            ;
           }
       }

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
UCS_string::dump(ostream & out) const
{
   out << right << hex << uppercase << setfill('0');
   loop(s, size())
      {
        out << " U+" << setw(4) << int(at(s));
      }

   return out << left << dec << nouppercase << setfill(' ');
}
//────────────────────────────────────────────────────────────────────────────
bool 
UCS_string::ends_with(const char * suffix) const
{
const ShapeItem s_len = strlen(suffix);
   if (ssize() < s_len)   return false;

   suffix += s_len;    // goto end of suffix
   loop(s, s_len)   if (at(ssize() - s - 1) != *--suffix)   return false;
   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
UCS_string::has_black() const
{
   loop(s, ssize())   if (!Avec::is_white(at(s)))   return true;
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
UCS_string::is_comment_or_label() const
{
   if (size() == 0)                  return false;
   if (front() == UNI_NUMBER_SIGN)   return true;   // comment
   if (front() == UNI_COMMENT)       return true;   // comment
   loop(t, size())
       {
         if (at(t) == UNI_COLON)             return true;   // label
         if (!Avec::is_symbol_char(at(t)))   return false;
       }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
UCS_string::is_in_range(const UCS_string & from, const UCS_string & to) const
{
   if (from.size())   // from provided
      {
         if (this->starts_with(from))          return true;
         if (this->compare(from) == COMP_LT)   return false;
      }

   if (to.size())   // to provided
      {
         if (this->starts_with(to))          return true;
         if (this->compare(to) == COMP_GT)   return false;
      }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
UCS_string::LF_count() const
{
ShapeItem count = 0;
   loop(u, ssize())   if (at(u) == UNI_LF)   ++count;
   return count;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
UCS_string::multi_pos() const
{
   /* This functions returns the start or the end of a multiline
      string or literal.
 
     There are two basic multiline styles:

      Old-style strings: start with one quote (" or »), but have no matching
                         end quote on the same line. This function returns -1
                         in this case.

      New-style strings: start with three identical quotes and have a matching
                         triple quote on some other line.

      This function only cares for New-style strings or literals.

      The quote pairs considered are:
          '...'   standard APL strings (but only to skip them), or
          "..."   GNU strings, or
          «...»   Double Arrow Quote Marks

      but not standard APL strings (because ''' is a prefix of '''' (which
      is a normal APL string consisting of a single '.
    */
ShapeItem pos;
   for (pos = 0; pos < ssize(); ++pos)
       {
         const Unicode u0 = at(pos);
         const bool triple = pos + 2 < ssize() &&
                             at(pos + 1) == u0 &&
                             at(pos + 2) == u0;
         switch(u0)
            {
              default:   continue;

              case UNI_DOUBLE_QUOTE:   // """ or "..."
                   if (triple)   return pos;
                   pos = string_end_pos(pos, false);   // skip string
                   if (pos == -1)   return -1;         // old style
                   continue;

              case UNI_SINGLE_QUOTE:   // """ or "..."
                   pos = string_end_pos(pos, false);   // skip string
                   if (pos == -1)   return -1;
                   continue;

              case UNI_NUMBER_SIGN:   // #
              case UNI_COMMENT:       // ⍝
                   return -1;

              case UNI_LEFT_DAQ:   // ««« : start of multiline
                   if (triple)   goto start_found;
                   break;

              case UNI_RIGHT_DAQ:   // »»» : end of multiline
                   if (triple)   return pos;
                   break;

              case UNI_LESS:   // <<< : start of multiline
                   if (triple)   goto start_found;
                   break;

              case UNI_GREATER:   // >>> : end of multiline
                   if (triple)   return pos;
                   break;
            }
       }   // loop(pos, end)

   return -1;   // not found

start_found:
const ShapeItem end = skip_white(pos + 3);
   if (end < ssize() && !Avec::is_comment(at(end)))
      {
        CERR << "*** WARNING: non-blank and non-comment '"
             << UCS_string(*this, end) << "' after "
             << at(pos) << at(pos) << at(pos) << " (ignored)" << endl;
      }
   return pos;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::no_pad() const
{
UCS_string ret;
   loop(s, size())
      {
        Unicode uni = at(s);
        if (is_iPAD_char(uni))   uni = UNI_SPACE;
        ret << uni;
      }

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::remove_pad() const
{
UCS_string ret;
   loop(s, size())
      {
        Unicode uni = at(s);
        if (!is_iPAD_char(uni))   ret << uni;
      }

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
int
UCS_string::skip_white(int pos) const
{
   while (pos < ssize() && Avec::is_white(at(pos)))   ++pos;
   return pos;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::sort() const
{
UCS_string ret(*this);
   Heapsort<Unicode>::sort(ret, greater_uni, 0);
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
bool 
UCS_string::starts_iwith(const char * prefix) const
{
   loop(s, size())
      {
        char pc = *prefix++;
        if (pc == 0)   return true;   // prefix matches this string.
        if (pc >= 'a' && pc <= 'z')   pc -= 'a' - 'A';

        int uni = at(s);
        if (uni >= 'a' && uni <= 'z')   uni -= 'a' - 'A';

        if (uni != Unicode(pc))   return false;
      }

   return *prefix == 0;   
}
//────────────────────────────────────────────────────────────────────────────
bool 
UCS_string::starts_iwith(const UCS_string & prefix) const
{
   if (prefix.size() > size())   return false;

   loop(p, prefix.size())
      {
        int c1 = at(p);
        int c2 = prefix[p];
        if (c1 >= 'a' && c1 <= 'z')   c1 -= 'a' - 'A';
        if (c2 >= 'a' && c2 <= 'z')   c2 -= 'a' - 'A';
        if (c1 != c2)   return false;
      }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool 
UCS_string::starts_with(const char * prefix) const
{
   loop(s, ssize())
      {
        const char pc = *prefix++;
        if (pc == 0)   return true;   // prefix matches this string.

        const Unicode uni = at(s);
        if (uni != Unicode(pc))   return false;
      }

   // match, unless prefix is longer than this string
   return *prefix == 0;
}
//────────────────────────────────────────────────────────────────────────────
bool 
UCS_string::starts_with(const UCS_string & prefix) const
{
   if (prefix.ssize() > ssize())   return false;

   loop(p, prefix.size())   if (at(p) != prefix[p])   return false;

   return true;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
UCS_string::string_end_pos(ShapeItem pos, bool throw_error) const
{
   Assert1(size());   // since at(start_pos) exists

const Unicode first = at(pos++);
   if (first == UNI_SINGLE_QUOTE)   // standard APL string
      {
        // standard APL string
        //
        for (; pos < ssize(); ++pos)
            {
              if (at(pos) != UNI_SINGLE_QUOTE)   continue;
              if (pos + 1 < ssize() && at(pos + 1) == UNI_SINGLE_QUOTE)
                 {
                   // this is '' iniside the APL string, which is an escaped '
                   // (and not the end of the string).
                   ++pos;
                 }
              else
                 {
                   // this is the true end of the APL string
                   return pos;
                 }
            }
      }
   else if (first == UNI_DOUBLE_QUOTE)   // GNU APL string
      {
        // GNU APL string
        //
        for (; pos < ssize(); ++pos)
            {
              if (at(pos) == UNI_DOUBLE_QUOTE)   return pos;
              if (at(pos) == UNI_BACKSLASH)      ++pos;
              // otherwise at(pos) belongs to this string.
            }
      }
   else if (first == UNI_LEFT_DAQ)   // Double Arrow Quote Mark
      {
        for (; pos < ssize(); ++pos)
            {
              if (at(pos) == UNI_RIGHT_DAQ)   return pos;
              // otherwise at(pos) belongs to this string.
            }
      }
   else
      {
        // start_pos is not the start of a string (and then calling
        // string_end_pos(start_pos) is an error).
        Q1(*this)
        Q1(first)
        FIXME;
      }

   // at this point, no end of string was found.
   //
   if (!throw_error)   return -1;
   MORE_ERROR() << "No end of string found. The offending string was: "
                << *this;
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
UCS_string::substr_pos(const UCS_string & sub) const
{
const ShapeItem start_positions = 1 + ssize() - sub.ssize();
   loop(start, start_positions)
      {
        bool mismatch = false;
        loop(u, sub.ssize())
           {
             if (at(start + u) != sub[u])
                {
                  mismatch = true;
                  break;
                }
           }

        if (!mismatch)   return start;   // found sub at start
      }

   return -1;   // not found
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::to_HTML(int offset, bool preserve_ws) const
{
UCS_string ret;
   for (;offset < ssize(); ++offset)
      {
        const Unicode uni = at(offset);
        switch(uni)
           {
             case ' ':  if (preserve_ws)   ret << "&nbsp;";
                        else               ret << uni;
                        break;
             case '#':  ret << "&#35;";   break;
             case '%':  ret << "&#37;";   break;
             case '&':  ret << "&#38;";   break;
             case '<':  ret << "&lt;";    break;
             case '>':  ret << "&gt;";    break;
             default:   ret << uni;
           }
      }

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
size_t
UCS_string::to_vector(UCS_string_vector & result) const
{
size_t max_len = 0;

   result.clear();
   if (size() == 0)   return max_len;

   result.push_back(UCS_string());
   loop(s, size())
      {
        const Unicode uni = at(s);
        if (uni == UNI_LF)    // line done
           {
             const size_t len = result.back().size();
             if (max_len < len)   max_len = len;

             if (s < ssize() - 1)   // more coming
                result.push_back(UCS_string());
           }
        else
           {
             if (uni != UNI_CR)         // ignore \r.
                result.back() << uni;
           }
      }

   // if the last line lacked a \n we check max_len here again.
const size_t len = result.back().size();
   if (max_len < len)   max_len = len;

   return max_len;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::un_escape(bool double_quoted, bool keep_LF) const
{
const char * hex = "0123456789abcdef";
UCS_string ret;
   ret.reserve(size());

   if (double_quoted)
      {
        loop(s, size())
            {
             const Unicode uni = at(s);
             if (uni != UNI_BACKSLASH)   // normal char
                {
                  ret << uni;
                  continue;
                }

             if (s >= ssize() - 1)   // \ at end of string
                {
                  ret << UNI_BACKSLASH;
                  break;
                }

             const Unicode uni1 = at(++s);
             switch(uni1)
                 {
                  case UNI_a:            ret << UNI_BEL;   continue;
                  case UNI_b:            ret << UNI_BS;    continue;
                  case UNI_f:            ret << UNI_FF;    continue;
                  case UNI_n:            if (keep_LF)   break;
                                               ret << UNI_LF;    continue;
                  case UNI_r:            ret << UNI_CR;    continue;
                  case UNI_t:            ret << UNI_BS;    continue;
                  case UNI_v:            ret << UNI_VT;    continue;
                  case UNI_DOUBLE_QUOTE:
                  case UNI_BACKSLASH:
                                               ret << uni1;            continue;
                  default:                     break;
                 }

             int max_len = 0;
             if (uni1 == UNI_u)
                {
                  max_len = 4;
                }
             else if (uni1 == UNI_x)
                {
                  max_len = 2;
                }
             else   // \n or \": keep them escaped
                {
                  ret << uni << uni1;
                  continue;
                }

               // \x or \u
               //
               int value = 0;
               loop(m, max_len)
                   {
                     if (s >= ssize() - 1)   break;
                     const int dig = at(s+1);
                     const char * pos = strchr(hex, dig);
                     if (pos == 0)   break;   // non-hex character

                     value = value << 4 | (pos - hex);
                     ++s;
                   }
               ret << Unicode(value);
            }
      }
   else
      {
        bool got_quote = false;
        loop(s, size())
           {
             const Unicode uni = at(s);
             if (uni == UNI_SINGLE_QUOTE)
                {
                  if (got_quote)
                     {
                        ret << UNI_SINGLE_QUOTE;
                        got_quote = false;
                     }
                  else
                     {
                        ret << UNI_SINGLE_QUOTE;
                        got_quote = true;
                     }
                }
             else
                {
                  if (got_quote)   ret << UNI_SINGLE_QUOTE;   // mal-formed
                  ret << uni;
                  got_quote = false;
                }
           }
      }

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::unique() const
{
   if (size() <= 1)   return UCS_string(*this);

UCS_string sorted = sort();
UCS_string ret;
   ret.reserve(sorted.size());

   ret << sorted[0];
   for (ShapeItem j = 1; j < ssize(); ++j)
       {
         if (sorted[j] != ret.back())   ret << sorted[j];
       }

   Heapsort<Unicode>::sort(ret, greater_uni, 0);
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::append_single_quoted(const UCS_string & other)
{
   *this << UNI_DOUBLE_QUOTE;
   loop(s, other.size())
       {
          const Unicode uni = other[s];
          if (uni == UNI_DOUBLE_QUOTE)   *this << UNI_BACKSLASH;
          *this << uni;
       }
   *this << UNI_DOUBLE_QUOTE;
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::append_members(const vector<const UCS_string *> & members, int m)
{
   for (int mm = members.size() - 1; mm >= m; --mm)
       {
         if (mm < int(members.size() - 1))   push_back(UNI_FULLSTOP);
         *this << *members[mm];
       }
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::map_pad()
{
   loop(s, size())
      {
        if (is_iPAD_char(at(s)))   at(s) = UNI_SPACE;
      }
}
//────────────────────────────────────────────────────────────────────────────
UCS_string &
UCS_string::operator <<(const Shape & shape)
{
   loop(r, shape.get_rank())
       {
         if (r)   *this << UNI_SPACE;
         *this << shape.get_shape_item(r);
       }

   return *this;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string &
UCS_string::operator <<(double num)
{
char cc[40];
   if (Cell::is_near_int(num))   { SPRINTF(cc, "%.2g", num); }
   else                          { SPRINTF(cc, "%.16g", num);   }

   loop(c, sizeof(cc))
      {
        if (const char digit = cc[c])
           {
             if (digit == 'e')        push_back(UNI_E);
             else if (digit == '-')   push_back(UNI_OVERBAR);
             else                     push_back(Unicode(digit));
           }
        else                            break;
      }

   return *this;
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::remove_comment()
{
   loop(j, size())
      {
        if (at(j) == UNI_COMMENT || at(j) == UNI_NUMBER_SIGN)
           {
             resize(j);
             remove_trailing_whitespaces();
             return;
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::remove_leading_whitespaces()
{
ShapeItem count = 0;
   loop(s, size())
      {
        if (at(s) <= UNI_SPACE)   ++count;
        else                      break;
      }

   if (count == 0)        return;      // no leading whitspaces
   if (count == ssize())   clear();     // only whitespaces
   else                    vector<Unicode>::erase(begin(), begin() + count);
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::remove_trailing_padchars()
{
   // remove trailing pad chars from align() and append_string(),
   // but leave other pad chars intact.
   // But only if the line has no frame (vert).
   //
again:
   if (size() > 2 && contains(UNI_PAD_y_AXIS))
      {
        Unicode first = Unicode_0;
        if      (back() == UNI_PAD_r_DEPTH)   first = UNI_PAD_l_DEPTH;
        else if (back() == UNI_PAD_r_VALUE)   first = UNI_PAD_l_VALUE;

        if (first && at(size() - 2) == UNI_PAD_y_AXIS)
           {
             // last column is a nested dimension separator row
             //
             ShapeItem len = size() - 2;
             while (len && (at(len - 1) == UNI_PAD_y_AXIS     ||
                            at(len - 1) == UNI_iPAD_U0        ||
                            at(len - 1) == UNI_PAD_r_NOTCHAR  ||
                            at(len - 1) == UNI_PAD_b_ROW      ||
                            at(len - 1) == UNI_PAD_r_MAX))   --len;
             if (len && at(len - 1) == first)
                {
                  resize(len - 1);
                  goto again;
                }
           }
      }

   // discard all trailing pad chars (unless the line is framed)..
   //
   while (size())
      {
        const Unicode last = back();
        if (last == UNI_PAD_y_AXIS      // ₀: interdimensional separator
         || last == UNI_PAD_r_oCol      // ₁: pad new line to old PrintBuffer
         || last == UNI_PAD_r_nCol      // ₂: pad old PrintBuffer to new line
         || last == UNI_PAD_l_INT       // ₃: pad integer part
         || last == UNI_PAD_r_FRACT     // ₄: pad fractional part
         || last == UNI_PAD_r_NOTCHAR   // ¹: column separator (after NOTCHAR)
         || last == UNI_PAD_l_NOTCHAR   // ³: column separator (before NOTCHAR)
         || last == UNI_PAD_r_MAX)      // ⁷  pad final to the right
           {
             pop_back();
           }
        else
           {
             break;
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::remove_trailing_whitespaces()
{
   while (size() && back() <= UNI_SPACE)   pop_back();
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::reverse()
{
   loop(s, size() / 2)
      {
        const Unicode tmp = at(s);
        at(s) = at(size() - s - 1);
        at(size() - s - 1) = tmp;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::round_last_digit()
{
   Assert(size() > 1);

   // if the number ends with digits 0..4 then we simply discard
   // the trailing digit and return
   //
const bool round_down = back() <= UNI_4;   // remember direction
   pop_back();   // discard the digit that is rounded up or down
   if (size() && back() == UNI_FULLSTOP)   pop_back();   // trailing .

   Assert(size() > 0);
   if (round_down)   // round down (= discard last digit)
      {
        return;
      }

bool carry = true;   // from the rounded up digit

   rev_loop (q, size())
       {
         const Unicode uni = at(q);
         if (uni < UNI_0)   continue;   // not a digit, e.g. ¯ or .
         if (uni > UNI_9)   continue;   // not a digit, e.g. ¯ or .
         if (uni == UNI_9)
            {
              at(q) = UNI_0;   // 9 → 0 and keep carry
            }
         else
            {
              at(q) = Unicode(uni + 1);   // round cc up and clear carry
              carry = false;
              break;
            }
       }

   if (size() && back() == UNI_FULLSTOP)   pop_back();
   if (carry)
      {
        insert(front() == UNI_OVERBAR || front() == UNI_MINUS ? 1 : 0, UNI_1);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::split_ws(UCS_string & rest)
{
   remove_leading_and_trailing_whitespaces();

   loop(clen, size())
       {
         if (Avec::is_white(at(clen)))   // first whilespace: end of command
            {
              ShapeItem arg = clen;
              while (arg < ssize() && Avec::is_white(at(arg)))   ++arg;
              while (arg < ssize())   rest << at(arg++);
              resize(clen);
              return;
            }
       }
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::from_big(APL_Float & val)
{
   Assert(val >= 0.0);

long double value = val;
int digits[320];   // DBL_MAX is 1.79769313486231470E+308
int * d = digits;

const long double initial_fract = modf(value, &value);
long double fract;
   for (; value >= 1.0; ++d)
      {
         fract = modf(value / 10.0, &value);   // U.x -> .U
         *d = int((fract + .02) * 10.0);
         fract -= 0.1 * *d;
      }

   val = initial_fract;

UCS_string ret;
   if (d == digits)   ret << UNI_0;   // 0.xxx

   while (d > digits)   ret << Unicode(UNI_0 + *--d);
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::from_double_to_expo(APL_Float v, int fract_digits)
{
UCS_string ret;

   if (v == 0.0)
      {
        ret << UNI_0;
        if (fract_digits)   // unless integer only
           {
             ret << UNI_FULLSTOP;
             loop(f, fract_digits)   ret << UNI_0;
           }
        ret << UNI_E << UNI_0;
        return ret;
      }

   if (v < 0.0)   { ret << UNI_OVERBAR;   v = -v; }

int expo = 0;
   while (v >= 1.0E1)   // scale large v ≥ 10 down to [1:10)
      {
        if (v >= 1.0E9)
           if (v >= 1.0E13)
              if (v >= 1.0E15)
                 if     (v >= 1.0E16)      { v = v * 1.0E-16;   expo += 16; }
                 else /* v >= 1.0E15 */    { v = v * 1.0E-15;   expo += 15; }
              else
                 if     (v >= 1.0E14)      { v = v * 1.0E-14;   expo += 14; }
                 else /* v >= 1.0E13 */    { v = v * 1.0E-13;   expo += 13; }
           else
              if (v >= 1.0E11)
                 if     (v >= 1.0E12)      { v = v * 1.0E-12;   expo += 12; }
                 else /* v >= 1.0E11 */    { v = v * 1.0E-11;   expo += 11; }
              else
                 if     (v >= 1.0E10)      { v = v * 1.0E-10;   expo += 10; }
                 else /* v >= 1.0E9 */     { v = v * 1.0E-9;    expo += 9;  }
        else
           if (v >= 1.0E5)
              if (v >= 1.0E7)
                 if     (v >= 1.0E8)       { v = v * 1.0E-8;    expo += 8;  }
                 else /* v >= 1.0E7 */     { v = v * 1.0E-7;    expo += 7;  }
              else
                 if     (v >= 1.0E6)       { v = v * 1.0E-6;    expo += 6;  }
                 else /* v >= 1.0E5 */     { v = v * 1.0E-5;    expo += 5;  }
           else
              if (v >= 1.0E3)
                 if     (v >= 1.0E4)       { v = v * 1.0E-4;    expo += 4;  }
                 else /* v >= 1.0E3 */     { v = v * 1.0E-3;    expo += 3;  }
              else
                 if     (v >= 1.0E2)       { v = v * 1.0E-2;    expo += 2;  }
                 else /* v >= 1.0E1 */     { v = v * 1.0E-1;    expo += 1;  }
      }

   while (v < 1.0E0)   // scale small v < 1 down to [1:10)
      {
        if (v < 1.0E-8)
           if (v < 1.0E-12)
              if (v < 1.0E-14)
                 if     (v < 1.0E-15)      { v = v * 1.0E16;   expo += -16; }
                 else /* v < 1.0E-14 */    { v = v * 1.0E15;   expo += -15; }
              else
                 if     (v < 1.0E-13)      { v = v * 1.0E14;   expo += -14; }
                 else /* v < 1.0E-12 */    { v = v * 1.0E13;   expo += -13; }
           else
              if (v < 1.0E-10)
                 if     (v < 1.0E-11)      { v = v * 1.0E12;   expo += -12; }
                 else /* v < 1.0E-10 */    { v = v * 1.0E11;   expo += -11; }
              else
                 if     (v < 1.0E-9 )      { v = v * 1.0E10;   expo += -10; }
                 else /* v < 1.0E-8 */     { v = v * 1.0E9;    expo += -9;  }
        else
           if (v < 1.0E-4)
              if (v < 1.0E-6)
                 if     (v < 1.0E-7)       { v = v * 1.0E8;    expo += -8;  }
                 else /* v < 1.0E-6 */     { v = v * 1.0E7;    expo += -7;  }
              else
                 if     (v < 1.0E-5)       { v = v * 1.0E6;    expo += -6;  }
                 else /* v < 1.0E-4 */     { v = v * 1.0E5;    expo += -5;  }
           else
              if (v < 1.0E-2)
                 if     (v < 1.0E-3)       { v = v * 1.0E4;    expo += -4;  }
                 else /* v < 1.0E-2 */     { v = v * 1.0E3;    expo += -3;  }
              else
                 if     (v < 1.0E-1)       { v = v * 1.0E2;    expo += -2;  }
                 else /* v < 1.0E0  */     { v = v * 1.0E1;    expo += -1;  }
      }

   Assert(v >= 1.0);
   Assert(v < 10.0);

   // print the mantissa in fixed format. Normally the mantissa is ≥ 1.0 and
   // < 10.0, i.e. excluding 10.0. However, rounding up to fract_digits could
   // make it 10 (including)
   //
UCS_string mantissa = from_double_to_fixed(v, fract_digits);
   if (mantissa.size() >= 2 && mantissa[0] == UNI_1 && mantissa[1] == UNI_0)
      {
        // mantissa starts with 10
        //
        if (mantissa.size() == 2)   // 10Ee → 1.0yyyEe+1
           {
             mantissa[0] = UNI_1;
             mantissa.resize(1);
             if (fract_digits)
                {
                  mantissa.push_back(UNI_FULLSTOP);
                  loop(z, fract_digits)   mantissa.push_back(UNI_0);
                }
             ++expo;
           }
        else if (mantissa[2] == UNI_FULLSTOP)   // 10.yyyEe →  1.0yyyEe+1
           {
             mantissa[1] = UNI_FULLSTOP;
             mantissa[2] = UNI_0;
             mantissa.pop_back();
             ++expo;
           }
      }

   return ret << mantissa << UNI_E << from_int(expo);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::from_double_to_fixed(APL_Float v, int fract_digits)
{
   // fract_digits shall be the number of digitsd AFTER the decimal point

UCS_string ret;
   if (v < 0.0)   { ret << UNI_OVERBAR;   v = - v; }

   Assert(v >= 0.0);

   // store the integer part of v in ret, leaving the fract part in v.
   //
   ret << from_big(v) << UNI_FULLSTOP;   // from_big() leaves fractional part of v in v

   // append one more fractional digit than needed (the last one will be
   // rounded
   loop(f, fract_digits + 1)
       {
         v = v * 10.0;
         const int vv = v;
         ret << Unicode(UNI_0 + vv);
         v -= vv;
       }

   // round to last digit may increase the length
const int ret_len = ret.size();
   ret.round_last_digit();
   while (ret.ssize() > ret_len)   ret.pop_back();
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::from_int(int64_t value)
{
   if (value >= 0)   return from_uint(value);

UCS_string ret(UNI_OVERBAR);
   return ret + from_uint(- value);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::from_uint(uint64_t value)
{
   if (value == 0)   return UCS_string(UNI_0);

int digits[40];
int * d = digits;

   while (value)
      {
        const uint64_t v_10 = value / 10;
        *d++ = value - 10*v_10;
        value = v_10;
      }

UCS_string ret;
   while (d > digits)   ret << Unicode(UNI_0 + *--d);
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UCS_string::power(size_t n)
{
const Unicode expos[] = { UNI_PAD_U0, UNI_PAD_U1, UNI_PAD_U2, UNI_PAD_U3,
                          UNI_PAD_U4, UNI_PAD_U5, UNI_PAD_U6, UNI_PAD_U7,
                          UNI_PAD_U8, UNI_PAD_U9 };

UCS_string ret;
   do { ret << expos[n % 10];   n /= 10; }   while (n);
   ret.reverse();
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::append_int(ShapeItem num)
{
char cc[40];
   SPRINTF(cc, "%lld", long_long(num));
   loop(c, sizeof(cc))
      {
        if (const char digit = cc[c])
           {
             if (digit == '-')   push_back(UNI_OVERBAR);
             else                push_back(Unicode(digit));
           }
        else                      break;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
UCS_string::decode(const UTF8 * utf, int size)
{

   Log(LOG_char_conversion)
      CERR << "UCS_string::UCS_string(): utf = " << utf << endl;

int from = 0;

   for (int i = 0; i < size;)
      {
start_of_sequence:

        const uint32_t b0 = utf[i++];
        uint32_t bx = b0;
        uint32_t more;
        if      ((b0 & 0x80) == 0x00)   { more = 0;             }
        else if ((b0 & 0xE0) == 0xC0)   { more = 1; bx &= 0x1F; }
        else if ((b0 & 0xF0) == 0xE0)   { more = 2; bx &= 0x0F; }
        else if ((b0 & 0xF8) == 0xF0)   { more = 3; bx &= 0x0E; }
        else if ((b0 & 0xFC) == 0xF8)   { more = 4; bx &= 0x07; }
        else if ((b0 & 0xFE) == 0xFC)   { more = 5; bx &= 0x03; }
        else   // invalid UTF start byte
           {
             Log(LOG_char_conversion)
                {
                 CERR << "Bad UTF8 string: ";
                 UTF8_string::dump_hex(CERR, utf, size, 40);
                 CERR << " at " << LOC <<  endl;
                 BACKTRACE
                }

             // map this string to the "supplementary private use area B" so
             // that its hex code becomes (sort of) visible
             //
             *this << Unicode(utf[from] | 0x100000);
             i = ++from;   // retry, starting at next char
             goto start_of_sequence;
           }

        uint32_t uni = 0;
        for (; more; --more)
            {
              if (i >= size)
                 {
                   Log(LOG_char_conversion)
                      {
                        CERR << "Truncated UTF8 string: ";
                        UTF8_string::dump_hex(CERR, utf, size, 40);
                        CERR << " len " << size << " at " << LOC << endl;
                      }

                   *this << Unicode(utf[from] | 0x100000);
                   i = ++from;   // retry, starting at next char
                   goto start_of_sequence;
                 }

              const UTF8 subc = utf[i++];
              if ((subc & 0xC0) != 0x80)   // invalid UTF continuation byte
                 {
                   Log(LOG_char_conversion)
                      {
                        CERR << "Bad UTF8 string: ";
                        UTF8_string::dump_hex(CERR, utf, size, 40);
                        CERR << " len " << size << " at " << LOC <<  endl;
                        BACKTRACE
                      }

                   *this << Unicode(utf[from] | 0x100000);
                   i = ++from;   // retry, starting at next char
                   goto start_of_sequence;
                 }

              bx  <<= 6;
              uni <<= 6;
              uni |= subc & 0x3F;
            }

         *this << Unicode(bx | uni);
         from = i;
      }

   Log(LOG_char_conversion)
      CERR << "UCS_string::UCS_string(): ucs = " << *this << endl;
}
//════════════════════════════════════════════════════════════════════════════
ostream &
operator <<(ostream & os, Unicode uni)
{
   if (uni < 0x80)      return os << char(uni);
        
   if (uni < 0x800)     return os << char(0xC0 | (uni >> 6))
                                  << char(0x80 | (uni & 0x3F));

   if (uni < 0x10000)    return os << char(0xE0 | (uni >> 12))
                                   << char(0x80 | (uni >>  6 & 0x3F))
                                   << char(0x80 | (uni       & 0x3F));

   if (uni < 0x200000)   return os << char(0xF0 | (uni >> 18))
                                   << char(0x80 | (uni >> 12 & 0x3F))
                                   << char(0x80 | (uni >>  6 & 0x3F))
                                   << char(0x80 | (uni       & 0x3F));

   if (uni < 0x4000000)  return os << char(0xF8 | (uni >> 24))
                                   << char(0x80 | (uni >> 18 & 0x3F))
                                   << char(0x80 | (uni >> 12 & 0x3F))
                                   << char(0x80 | (uni >>  6 & 0x3F))
                                   << char(0x80 | (uni       & 0x3F));

   return os << char(0xFC | (uni >> 30))
             << char(0x80 | (uni >> 24 & 0x3F))
             << char(0x80 | (uni >> 18 & 0x3F))
             << char(0x80 | (uni >> 12 & 0x3F))
             << char(0x80 | (uni >>  6 & 0x3F))
             << char(0x80 | (uni       & 0x3F));
}
//════════════════════════════════════════════════════════════════════════════
ostream &
operator <<(ostream & os, const UCS_string & ucs)
{
   /*
      os.width() is the minimum output width that may have been set
      before <<, for example:

      out << setw(10) << "abcde"

       os.fill() is the character to be used for filling the output as tp
       reach the minimum output width.
    */
const int fill_len = os.width() - ucs.size();

   if (fill_len > 0)   // unlikely
      {
        os.width(0);
        const char fill = os.fill();
        if (os.flags() & ios::right)   // right fill
           {
             loop(f, fill_len)     os << fill;
             loop(u, ucs.size())   os << ucs[u];
           }
        else                           // left fill
           {
             loop(u, ucs.size())   os << ucs[u];
             loop(f, fill_len)     os << fill;
           }
      }
   else
      {
        loop(u, ucs.size())   os << ucs[u];
      }

   return os;
}
//════════════════════════════════════════════════════════════════════════════
UCS_ASCII_string::UCS_ASCII_string(TokenClass tc)
{
   switch(tc)
      {
        case TC_ASSIGN:    *this << "TC_ASSIGN";     return;
        case TC_R_ARROW:   *this << "TC_R_ARROW";    return;
        case TC_L_BRACK:   *this << "TC_L_BRACK";    return;
        case TC_R_BRACK:   *this << "TC_R_BRACK";    return;
        case TC_END:       *this << "TC_END";        return;
        case TC_FUN0:      *this << "TC_FUN0";       return;
        case TC_FUN12:     *this << "TC_FUN12";      return;
        case TC_INDEX:     *this << "TC_INDEX";      return;
        case TC_OPER1:     *this << "TC_OPER1";      return;
        case TC_OPER2:     *this << "TC_OPER2";      return;
        case TC_L_PARENT:  *this << "TC_L_PARENT";   return;
        case TC_R_PARENT:  *this << "TC_R_PARENT";   return;
        case TC_RETURN:    *this << "TC_RETURN";     return;
        case TC_SYMBOL:    *this << "TC_SYMBOL";     return;
        case TC_VALUE:     *this << "TC_VALUE";      return;
        case TC_PINDEX:    *this << "TC_PINDEX";     return;
        case TC_VOID:      *this << "TC_VOID";       return;
        case TC_OFF:       *this << "TC_OFF";        return;
        case TC_SI_CHANGE: *this << "TC_SI_CHANGE"; return;
        case TC_LINE:      *this << "TC_LINE";       return;
        case TC_NUMERIC:   *this << "TC_NUMERIC";    return;
        case TC_SPACE:     *this << "TC_SPACE";      return;
        case TC_NEWLINE:   *this << "TC_NEWLINE";    return;
        case TC_COLON:     *this << "TC_COLON";      return;
        case TC_QUOTE:     *this << "TC_QUOTE";      return;
        case TC_L_CURLY:   *this << "TC_L_CURLY";    return;
        case TC_R_CURLY:   *this << "TC_R_CURLY";    return;
        case TC_LIT_ITEM:  *this << "TC_LIT_ITEM";   return;
        case TC_LIT_GRP:   *this << "TC_LIT_GRP";    return;

         default: ;
      }

   // tc was not defined
   //
char cc[40];
   SPRINTF(cc, "TC_%4.4X", tc);
   *this << cc;
}
//────────────────────────────────────────────────────────────────────────────
UCS_ASCII_string::UCS_ASCII_string(TokenTag tag)
{
   switch(tag)
      {
        case TOK_NONE:   *this << "TOK_NONE";             return;
#define TD(tag, _tc, _tv, _id) case tag: *this << #tag;   return;
#include "Token.def"
      }

   // tag was not defined
   //
char cc[40];
   SPRINTF(cc, "TOK_%4.4X", tag);
   *this << cc;
}
//────────────────────────────────────────────────────────────────────────────
UCS_ASCII_string::UCS_ASCII_string(TokenValueType tvt)
{
   switch(tvt)
      {
        case TV_MASK:  *this << "TV_MASK";    return;
        case TV_NONE:  *this << "TV_NONE";    return;
        case TV_CHAR:  *this << "TV_CHAR";    return;
        case TV_INT:   *this << "TV_INT";     return;
        case TV_FLT:   *this << "TV_FLT";     return;
        case TV_CPX:   *this << "TV_CPX";     return;
        case TV_SYM:   *this << "TV_SYM";     return;
        case TV_LIN:   *this << "TV_LIN";     return;
        case TV_VAL:   *this << "TV_VAL";     return;
        case TV_INDEX: *this << "TV_INDEX";   return;
        case TV_FUN:   *this << "TV_FUN";     return;

         default: ;
      }

   // tvt was not defined
   //
char cc[40];
   SPRINTF(cc, "TV_%4.4X", tvt);
   *this << cc;
}
//════════════════════════════════════════════════════════════════════════════

