/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2016  Dr. Jürgen Sauermann

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

#ifndef __UCS_STRING_HH_DEFINED__
#define __UCS_STRING_HH_DEFINED__

#include <stdint.h>
#include <stdio.h>
#include <string>

#include "Assert.hh"
#include "Common.hh"
#include "Heapsort.hh"
#include "Unicode.hh"
#include "UTF8_string.hh"

using namespace std;

class PrintBuffer;
class PrintContext;
class Shape;
class Value;
class UCS_string_vector;

/// track construction and destruction of UCS_strings
#define UCS_tracking 0

//=============================================================================
/// A string of Unicode characters (32-bit)
class UCS_string : public  basic_string<Unicode>
{
public:
   /// default constructor: empty string
   UCS_string();

   /// constructor: one-element string
   UCS_string(Unicode uni);

   /// constructor: \b len Unicode characters, starting at \b data
   UCS_string(const Unicode * data, size_t len);

   /// constructor: \b len times \b uni
   UCS_string(size_t len, Unicode uni);

   /// constructor: copy of another UCS_string
   UCS_string(const UCS_string & ucs);

   /// constructor: copy of another UCS_string
   UCS_string(const UCS_string & ucs, size_t pos, size_t len);

   /// constructor: UCS_string from UTF8_string
   UCS_string(const UTF8_string & utf);

   /// constructor: UCS_string from 0-terminated C string
   UCS_string(const char * cstring);

   /// constructor: UCS_string from print buffer
   UCS_string(const PrintBuffer & pb, Rank rank, int quad_PW);

   /// constructor: UCS_string from a double with quad_pp valid digits.
   /// (eg. 3.33 has 3 digits), In standard APL format.
   UCS_string(APL_Float value, bool & scaled, const PrintContext & pctx);

   /// constructor: read one line from UTF8-encoded file.
   UCS_string(istream & in);

   /// constructor: UCS_string from simple character vector value.
   UCS_string(const Value & value);

#if UCS_tracking
   /// common part of all constructors
   void create(const char * loc);

   /// destructor
   ~UCS_string();
#else
   /// common part of all constructors
   void create(const char * loc)   { ++total_count; }

   /// destructor
   ~UCS_string()                   { --total_count; }
#endif

   /// cast to an array of items with the same size as Unicode. This is for
   /// interfacing to libraries that have typedef'ed Unicodes differently.
   template<typename T>
   const T * raw() const
      {
        Assert(sizeof(T) == sizeof(Unicode));
        return reinterpret_cast<const T *>(&at(0));
      }

   /// compute the length of an output row
   int compute_chunk_length(int quad_PW, int col) const;

   /// remove trailing pad characters
   void remove_trailing_padchars();

   /// remove trailing blanks, tabs, etc
   void remove_trailing_whitespaces();

   /// remove leading blanks, tabs, etc
   void remove_leading_whitespaces();

   /// remove leading and trailing whitespaces
   void remove_leading_and_trailing_whitespaces()
      {
        remove_trailing_whitespaces();
        remove_leading_whitespaces();
      }

   /// skip leading whitespaces starting at idx, append the following
   /// non-whitespaces (if any) to \b dest, and skip trailing whitespaces
   void copy_black(UCS_string & dest, int & idx) const;

   /// \b this is a command with optional args. Remove leading and trailing
   /// whitespaces, append args to rest, and remove args from this.
   void split_ws(UCS_string & rest);

   /// return the number of LF characters in \b this string
   ShapeItem LF_count() const;

   /// return the start position of \b sub in \b this string or -1 if \b sub
   /// is not contained in \b this string
   ShapeItem substr_pos(const UCS_string & sub) const;

   /// return this string with the first \b drop_count characters removed
   UCS_string drop(int drop_count) const
      {
        if (drop_count <= 0)        return UCS_string(*this, 0, size());
        if (size() <= drop_count)   return UCS_string();
        return UCS_string(*this, drop_count, size() - drop_count);
      }

   /// return the last character in \b this string
   Unicode back() const
    { return size() ? (*this)[size() - 1] : Invalid_Unicode; }

   /// return the last character in \b this string
   Unicode & back()
    { Assert(size());   return at(size() - 1); }

   /// return true if this string contains non-whitespace characters
   bool has_black() const;

   /// return true if \b this starts with prefix (ASCII, case matters).
   bool starts_with(const char * prefix) const;

   /// return true if \b this ends with suffix (ASCII, case matters).
   bool ends_with(const char * suffix) const;

   /// return true if \b this starts with \b prefix (case sensitive).
   bool starts_with(const UCS_string & prefix) const;

   /// return true if \b this starts with \b prefix (ASCII, case insensitive).
   bool starts_iwith(const char * prefix) const;

   /// return true if \b this starts with \b prefix (case insensitive).
   bool starts_iwith(const UCS_string & prefix) const;

   /// return a string like this, but with pad chars mapped to spaces
   UCS_string no_pad() const;

   /// replace pad chars in \b this string by spaces
   void map_pad();

   /// return a string like this, but with pad chars removed
   UCS_string remove_pad() const;

   /// remove the last character in \b this string
   void pop_back()
   { Assert(size() > 0);   resize(size() - 1); }

   /// return this string reversed (i.e. characters from back to front).
   UCS_string reverse() const;

   /// return true if \b this string starts with # or ⍝ or x:
   bool is_comment_or_label() const;

   /// return true if every character in \b this string is the digit '0'
   bool all_zeroes() const
      { loop(s, size())   if ((*this)[s] != UNI_ASCII_0)   return false;
        return true;
      }

   /// return the number of unescaped and un-commented " in this string
   ShapeItem double_quote_count(bool in_quote2) const;

   /// return the position of the first (leftmost) unescaped " in \b this
   /// string (if any), or else -1
   ShapeItem double_quote_first() const;

   /// return the position of the last (rightmost) unescaped " in \b this
   /// string (if any), or else -1
   ShapeItem double_quote_last() const;

   /// return integer value for a string starting with optional whitespaces,
   /// followed by digits.
   int atoi() const;

   /// append UCS_string other to this string
   void append(const UCS_string & other)
      { basic_string::append(other); }

   /// append 0-terminated ASCII string \b str to this string. str is NOT
   /// interpreted as UTF8 string (use append_UTF8() if such interpretation        /// is desired)
   void append_ASCII(const char * ascii)
      { while (*ascii)   *this += Unicode(*ascii++); }


   /// append 0-terminated UTF8 string str to \b this UCS_string.
   // This is different from append_ASCII((const char * str):
   ///
   /// append_ascii() appends one Unicode per byte (i.e. strlen(str) in total),
   /// without checking for UTF8 sequences.
   ///
   /// append_UTF8() appends one Unicode per UTF8 sequence (which is the same
   /// if all characteras are ASCII, but less if not.
   void append_UTF8(const UTF8 * str);

   /// same as app(const UTF8 * str)
   void append_UTF8(const char * str)
      { append_UTF8(utf8P(str)); }

   /// more intuitive insert() function
   void insert(ShapeItem pos, Unicode uni)
      { basic_string::insert(pos, 1, uni); }

   /// prepend character \b uni
   void prepend(Unicode uni)
      { insert(0, uni); }

   /// return \b this string and \b other concatenated
   UCS_string operator +(const UCS_string & other) const
      { UCS_string ret(*this);   ret += other;   return ret; }

   /// append C-string \b str
   UCS_string & operator <<(const char * str)
      { append_UTF8(str);   return *this; }

   /// append number \b num
   UCS_string & operator <<(ShapeItem num)
      { append_number(num);   return *this; }

   /// append character \b uni
   UCS_string & operator <<(Unicode uni)
      { *this += uni;   return *this; }

   /// append UCS_string \b other
   UCS_string & operator <<(const UCS_string & other)
      { basic_string::append(other);   return *this; }

   /// compare \b this with UCS_string \b other
   Comp_result compare(const UCS_string & other) const
      {
        if (*this < other)   return COMP_LT;
        if (*this > other)   return COMP_GT;
        return COMP_EQ;
      }

   /// append \b other in quotes, doubling quoted in \b other
   void append_quoted(const UCS_string & other);

   /// append number (in ASCII encoding like %d) to this string
   void append_number(ShapeItem num);

   /// append number (in ASCII encoding like %X or %x) to this string
   void append_hex(ShapeItem num, bool uppercase);

   /// append shape (in APL encoding tke left arg of ↑) this string
   void append_shape(const Shape & shape);

   /// append number (in ASCII encoding like %lf) to this string
   void append_float(APL_Float num);

   /// split \b this multi-line string into individual lines,
   /// removing the CR and NL chars in \b this string.
   size_t to_vector(UCS_string_vector & result) const;

   /// return \b this string with "escape sequences" replaced by their real
   /// characters ('' → ' if single quoted and \\r, \\n, \\xNNN etc. otherwise.
   UCS_string un_escape(bool double_quoted, bool keep_LF) const;

   /// the inverse of \b un_escape().
   UCS_string do_escape(bool double_quoted) const;

   /// overload basic_string::size() so that it returns a signed length
   ShapeItem size() const
      { return basic_string::size(); }

   /// an iterator for UCS_strings
   class iterator
      {
        public:
           /// constructor: start at position p
           iterator(const UCS_string & ucs, int p)
           : s(ucs),
             pos(p)
           {}

           /// return char at offset off from current position
           Unicode get(int off = 0) const
              { return (pos + off) < s.size() ? s[pos+off] : Invalid_Unicode; }

           /// return next char
           Unicode next()
              { return pos < s.size() ? s[pos++] : Invalid_Unicode; }

           /// return true iff there are more chars in the string
           bool more() const
              { return pos < s.size(); }

        protected:
           /// the string
           const UCS_string & s;

           /// the current position in the string
           int pos;
      };

   /// an iterator set to the start of this string
   UCS_string::iterator begin() const
      { return iterator(*this, 0); }

   /// round last digit and discard it.
   void round_last_digit();

   /// return true if \b this string contains \b uni
   bool contains(Unicode uni);

   /// case-sensitive comparison: return true iff \b this comes before \b other
   bool lexical_before(const UCS_string other) const;

   /// dump \b this string to out (like U+nnn U+mmm ... )
   ostream & dump(ostream & out) const;

   /// sort the characters in this string by their Unicode
   UCS_string sort() const;

   /// return the characters in this string (sorted and duplicates removed)
   UCS_string unique() const;

   /// return this string HTML-escaped, starting at offset, maybe using &nbsp;
   UCS_string to_HTML(int offset, bool preserve_ws) const;

   /// erase 1 (!) character at pos
   void erase(ShapeItem pos)
      { basic_string::erase(pos, 1); }

   /// helper function for Heapsort<Unicode>::sort()
   static bool greater_uni(const Unicode & u1, const Unicode & u2, const void *)
      { return u1 > u2; }

   /// convert a signed integer value to an UCS_string (like sprintf())
   static UCS_string from_int(int64_t value);

   /// convert an unsigned integer value to an UCS_string (like sprintf())
   static UCS_string from_uint(uint64_t value);

   /// convert the integer part of value to an UCS_string and remove it
   /// from value
   static UCS_string from_big(APL_Float & value);

   /// convert double \b value to an UCS_string with \b fract_digits fractional
   /// digits in scaled (exponential) format
   static UCS_string from_double_expo_prec(APL_Float value, int fract_digits);

   /// convert double \b value to an UCS_string with \b fract_digits fractional
   /// digits in fixed point format
   static UCS_string from_double_fixed_prec(APL_Float value, int fract_digits);

   /// convert double \b value to an UCS_string with \b quad_pp significant
   /// digits in scaled (exponential) format
   static UCS_string from_double_expo_pp(APL_Float value, int quad_pp);

   /// convert double \b value to an UCS_string with \b quad_pp significant
   /// digits in fixed point format
   static UCS_string from_double_fixed_pp(APL_Float value, int quad_pp);

   /// return the total number of UCS_strings
   static ShapeItem get_total_count()
      { return total_count; }

   /// return true if n1 < n2
   static bool compare_names(const UCS_string & n1,
                             const UCS_string & n2, const void *)
      { return n2.compare(n1) == COMP_LT; }

protected:
   /// the total number of UCS_strings
   static ShapeItem total_count;

   /// the next unique instance_id
   static ShapeItem total_id;

#if UCS_tracking
   /// a unique number for \b this  UCS_string
   ShapeItem instance_id;
#endif

private:
   /// prevent accidental allocation
   void * operator new[](size_t size);

   /// prevent accidental de-allocation
   void operator delete[](void *);

private:
   /// prevent accidental usage of the rather dangerous default len parameter
   /// in basic_strng::erase(pos, len = npos)
   basic_string & erase(size_type pos, size_type len);
};
//-----------------------------------------------------------------------------
inline void
Hswap(const UCS_string * & u1, const UCS_string * & u2)
{
const UCS_string * tmp = u1;   u1 = u2;   u2 = tmp;
}
//-----------------------------------------------------------------------------
inline void
Hswap(UCS_string & u1, UCS_string & u2)
{
UCS_string  u = u1;
           u1 = u2;
           u2 = u;
}
//-----------------------------------------------------------------------------

#endif // __UCS_STRING_HH_DEFINED__

