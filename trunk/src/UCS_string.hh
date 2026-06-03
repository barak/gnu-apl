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

#ifndef __UCS_STRING_HH_DEFINED__
#define __UCS_STRING_HH_DEFINED__

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "Assert.hh"
#include "Avec.hh"
#include "Common.hh"
#include "Heapsort.hh"
#include "TokenEnums.hh"
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

//════════════════════════════════════════════════════════════════════════════
/// A string of Unicode characters (32-bit)
class UCS_string : public std::vector<Unicode>
{
public:
   /// default constructor: empty string
   UCS_string();

   /// constructor: one-element string
   /// @param uni  single Unicode character to store
   UCS_string(Unicode uni);

   /// constructor: \b len Unicode characters, starting at \b data
   /// @param data  pointer to array of Unicode characters
   /// @param len   number of characters to copy
   UCS_string(const Unicode * data, size_t len);

   /// constructor: \b len times \b uni
   /// @param len  number of copies of uni
   /// @param uni  Unicode character to repeat
   UCS_string(size_t len, Unicode uni);

   /// constructor: copy of another UCS_string
   /// @param ucs  source string to copy
   UCS_string(const UCS_string & ucs);

   /// constructor: copy of another UCS_string \b ucs, starting at \b pos
   /// in \b ucs
   /// @param ucs  source string to copy from
   /// @param pos  starting position within ucs
   UCS_string(const UCS_string & ucs, size_t pos);

   /// constructor: copy of another UCS_string \b ucs, starting at \b pos
   /// in \b ucs and length \b len
   /// @param ucs  source string to copy from
   /// @param pos  starting position within ucs
   /// @param len  number of characters to copy
   UCS_string(const UCS_string & ucs, size_t pos, size_t len);

   /// constructor: UCS_string from UTF8_string
   /// @param utf  UTF-8 encoded source string
   UCS_string(const UTF8_string & utf)
      { create(LOC);   decode(utf8P(utf.c_str()), utf.size()); }

   /// constructor: UCS_string from (0-terminated) U"..." literal
   /// @param literal  null-terminated UTF-32 string literal
   UCS_string(const char32_t * literal)
      { create(LOC);   while (*literal)   push_back(Unicode(*literal++)); }

   /// constructor: UCS_string from print buffer
   UCS_string(const PrintBuffer & pb, sRank rank, int quad_PW);

   /// constructor: UCS_string from a double with quad_pp valid digits.
   /// (eg. 3.33 has 3 digits), In standard APL format.
   UCS_string(APL_Float value, bool & scaled, const PrintContext & pctx);

   /// constructor: read one line from UTF8-encoded file \b in.
   UCS_string(istream & in);

   /// constructor: UCS_string from simple character vector value.
   UCS_string(const Value & value);

   /// constructor: UCS_string from a pointer Cell pointing to a
   /// character vector value (Asserts if not)
   UCS_string(const Cell & cell);

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

   /// an iterator for UCS_strings
   class iterator
      {
        public:
           /// constructor: start at position p
           iterator(const UCS_string & ucs)
           : s(ucs),
             pos(0)
           {}

           /// return the iterator position in the underlying string
           int get_pos() const
              { return pos; }

           /// return true iff there are more chars available
           bool has_more() const
              { return pos < s.ssize(); }

           /// return the next char (without pos increment)
           Unicode lookup() const
              { Assert(has_more());    return s[pos]; }

        /// the rest (starting at \b pos
        UCS_string rest() const
           { return UCS_string(s, pos, s.size() - pos); }

           /// return the next char
           Unicode next()
              { Assert(has_more());   return s[pos++]; }

        /// skip whitespace
        void skip_white()
           { while (has_more() && Avec::is_white(s[pos]))   ++pos; }

        protected:
           /// the string
           const UCS_string & s;

           /// the current position in the string
           int pos;
      };

   /// return true if every character in \b this string is the digit '0'
   bool all_zeroes() const
      { loop(s, size())   if ((*this)[s] != UNI_0)   return false;
        return true;
      }

   /// return the last character in \b this string
   Unicode back() const
    { return size() ? (*this)[size() - 1] : Invalid_Unicode; }

   /// compare \b this with UCS_string \b other
   Comp_result compare(const UCS_string & other) const
      {
        if (*this < other)   return COMP_LT;
        if (*this > other)   return COMP_GT;
        return COMP_EQ;
      }

   /// return this string with the first \b drop_count characters removed
   UCS_string drop(int drop_count) const
      {
        if (drop_count <= 0)        return UCS_string(*this, 0, size());
        if (ssize() <= drop_count)   return UCS_string();
        return UCS_string(*this, drop_count, size() - drop_count);
      }

   /// return the FNV (Fowler-Noll-Vo) hash of \b this_string
   uint32_t FNV_hash() const
      { enum { FNV_Offset_32 = 0x811C9DC5, FNV_Prime_32  = 16777619 };
        uint32_t hash = FNV_Offset_32;
        loop(s, size()) hash = (hash * FNV_Prime_32) ^ at(s);
        return hash;
      }

   /// return false if \b this and \b other are equal (same length and same
   /// characters).
   bool operator !=(const UCS_string & other) const
      { return compare(other) != COMP_EQ; }

   /// return false if \b this and \b UCS_string(other) are equal (same
   /// length and same characters).
   bool operator !=(const UTF8_string & other) const
      { return compare(UCS_string(other)) != COMP_EQ; }

   /// return false if \b this and \b UTF8__string(other) are equal (same
   /// length and same characters).
   bool operator !=(const char * other) const
      { return compare(UTF8_string(other)) != COMP_EQ; }

   /// return \b this string and \b other concatenated
   UCS_string operator +(const UCS_string & other) const
      { UCS_string ret(*this);
        ret << other;
        return ret;
      }

   /// return true if \b this and \b other are equal (same length and same
   /// characters).
   bool operator ==(const UCS_string & other) const
      { return compare(other) == COMP_EQ; }

   /// return true if \b this and \b UCS_string(other) are equal (same
   /// length and same characters).
   bool operator ==(const UTF8_string & other) const
      { return compare(UCS_string(other)) == COMP_EQ; }

   /// return true if \b this and \b UTF8__string(other) are equal (same
   /// length and same characters).
   bool operator ==(const char * other) const
      { return compare(UTF8_string(other)) == COMP_EQ; }

   /// cast to an array of items with the same size as Unicode. This is for
   /// interfacing to libraries that have typedef'ed Unicodes differently.
   template<typename T>
   const T * raw() const
      {
        Assert(sizeof(T) == sizeof(Unicode));
        return reinterpret_cast<const T *>(&front());
      }

   /// overload vector::size() so that it returns a signed length
   ShapeItem ssize() const
      { return  size(); }

   /// return the last character in \b this string
   Unicode & back()
    { Assert(size());   return at(size() - 1); }

   /// erase 1 (!) character at pos
   void erase(ShapeItem pos)
      { vector<Unicode>::erase(begin() + pos); }

   /// more intuitive insert() function
   void insert(ShapeItem pos, Unicode uni)
      {  vector<Unicode>::insert(begin() + pos, uni); }

   /// append UTF8_string \b utf
   UCS_string & operator <<(const UTF8_string & utf)
      { const UCS_string ucs(utf);   return *this << ucs; }

   /// append C-string \b str
   UCS_string & operator <<(const char * str)
      { const UTF8_string utf(str);
        return *this << utf;
      }

   /// append integer \b num
   UCS_string & operator <<(int num)
      { append_int(num);   return *this; }

   /// append integer \b num
   UCS_string & operator <<(unsigned long num)
      { append_int(num);   return *this; }

   /// append integer \b num
   UCS_string & operator <<(unsigned int num)
      { append_int(num);   return *this; }

   /// append number \b num
   UCS_string & operator <<(ShapeItem num)
      { append_int(num);   return *this; }

   /// append character \b uni
   UCS_string & operator <<(Unicode uni)
      { push_back(uni);   return *this; }

   /// append UCS_string \b other
   UCS_string & operator <<(const UCS_string & suffix)
      { loop(s, suffix.size())   push_back(suffix[s]);   return *this; }

   /// remove the last character in \b this string
   void pop_back()
      { Assert(size() > 0); std::vector<Unicode>::pop_back(); }

   /// prepend character \b uni
   void prepend(Unicode uni)
      { insert(0, uni); }

   /// remove leading and trailing whitespaces
   void remove_leading_and_trailing_whitespaces()
      {
        remove_trailing_whitespaces();
        remove_leading_whitespaces();
      }

   /// return true if n1 < n2
   static bool compare_names(const UCS_string & n1,
                             const UCS_string & n2, const void *)
      { return n2.compare(n1) == COMP_LT; }

   /// return the total number of UCS_strings
   static ShapeItem get_total_count()
      { return total_count; }

   /// helper function for Heapsort<Unicode>::sort()
   static bool greater_uni(const Unicode & u1, const Unicode & u2,
                            const void *)
      { return u1 > u2; }

   /// return integer value for a string starting with optional whitespaces,
   /// followed by digits.
   int atoi() const;

   /// compute the length of an output row
   int compute_chunk_length(int quad_PW, int col) const;

   /// return true if \b this string contains \b uni
   bool contains(Unicode uni) const;

   /// skip leading whitespaces starting at idx, append the following
   /// non-whitespaces (if any) to \b dest, and skip trailing whitespaces.
   /// return the idx after that.
   size_t copy_black(UCS_string & dest, int idx) const;

   /// the inverse of \b un_escape().
   UCS_string do_escape(bool double_quoted) const;

   /// return the number of unescaped and un-commented " in this string
   ShapeItem double_quote_count(bool in_quote2) const;

   /// return the position of the first (leftmost) unescaped " in \b this
   /// string (if any), or else -1
   ShapeItem double_quote_first() const;

   /// return the position of the last (rightmost) unescaped " in \b this
   /// string (if any), or else -1
   ShapeItem double_quote_last() const;

   /// dump \b this string to out (like U+nnn U+mmm ... )
   ostream & dump(ostream & out) const;

   /// return true if \b this ends with suffix (ASCII, case sensitive).
   bool ends_with(const char * suffix) const;

   /// return true if \b this string contains any non-whitespace characters
   bool has_black() const;

   /// return true if \b this string starts with # or ⍝ or x:
   bool is_comment_or_label() const;

   /// return \b true iff \b this string is in the range \b from (including)
   /// to 'b to (including).
   bool is_in_range(const UCS_string & from, const UCS_string & to) const;

   /// return the number of LF characters in \b this string
   ShapeItem LF_count() const;

   /// return the start position of """ in \b this string or -1 if \b """
   /// is not contained in \b this string
   ShapeItem multi_pos() const;

   /// return a string like this, but with pad chars mapped to spaces
   UCS_string no_pad() const;

   /// return a string like this, but with pad chars removed
   UCS_string remove_pad() const;

   /// increment pos white at(pos) is a whitespace
   int skip_white(int pos) const;

   /// sort the characters in this string by their Unicode
   UCS_string sort() const;

   /// return true if \b this starts with \b prefix (ASCII, case insensitive).
   bool starts_iwith(const char * prefix) const;

   /// return true if \b this starts with \b prefix (case insensitive).
   bool starts_iwith(const UCS_string & prefix) const;

   /// return true if \b this starts with prefix (ASCII, case sensitive).
   bool starts_with(const char * prefix) const;

   /// return true if \b this starts with \b prefix (case sensitive).
   bool starts_with(const UCS_string & prefix) const;

   /// for a string (single quoted, double quoted, or DAQ (Double Arrow
   /// Quotation Mark)) starting at \b pos, return its end.
   ShapeItem string_end_pos(ShapeItem start_pos, bool throw_error) const;

   /// return the start position of \b sub in \b this string or -1 if \b sub
   /// is not contained in \b this string
   ShapeItem substr_pos(const UCS_string & sub) const;

   /// return \b this string HTML-escaped, starting at offset, and using &nbsp;
   /// for blanks
   UCS_string to_HTML(int offset, bool preserve_ws) const;

   /// split \b this multi-line string into individual lines,
   /// removing the CR and NL chars in \b this string.
   size_t to_vector(UCS_string_vector & result) const;

   /// return \b this string with "escape sequences" replaced by their real
   /// characters ('' → ' if single quoted and \\r, \\n, \\xNNN etc. otherwise.
   UCS_string un_escape(bool double_quoted, bool keep_LF) const;

   /// return the characters in this string (sorted and duplicates removed)
   UCS_string unique() const;

   /// append \b other in (single) quotes, doubling single quotes in \b other
   void append_single_quoted(const UCS_string & other);

   /// append members (like x.y.z) starting at members[m] and going backwards
   /// from the end of \b members to \b this string.
   void append_members(const vector<const UCS_string *> & members, int m);

   /// replace pad chars in \b this string by spaces
   void map_pad();

   /// append \b shape
   UCS_string & operator <<(const Shape & shape);

   /// append a floating point number (in ASCII encoding like %f) to this string
   UCS_string & operator << (double num);

   /// remove comment (if any) from \b this string
   void remove_comment();

   /// remove leading blanks, tabs, etc
   void remove_leading_whitespaces();

   /// remove trailing pad characters
   void remove_trailing_padchars();

   /// remove trailing blanks, tabs, etc
   void remove_trailing_whitespaces();

   /// reverse the characters in \b this string
   void reverse();

   /// round last digit and discard it. Note that \b this is always expected
   /// to be in floating point format and never in exponential format
   void round_last_digit();

   /// \b this is a command with optional args. Remove leading and trailing
   /// whitespaces, append args to rest, and remove args from this.
   void split_ws(UCS_string & rest);

   /// convert the integer part of value to an UCS_string and remove it
   /// from value
   static UCS_string from_big(APL_Float & value);

   /// convert the double \b value to an UCS_string scaled (exponential) format
   /// (with \b fract_digits fractional digits).
   static UCS_string from_double_to_expo(APL_Float value, int fract_digits);

   /// convert the double \b value to an UCS_string in fixed point format
   /// (with \b fract_digits fractional digits).
   static UCS_string from_double_to_fixed(APL_Float value, int fract_digits);

   /// convert a signed integer value to an UCS_string (like snprintf("%d"))
   static UCS_string from_int(int64_t value);

   /// convert an unsigned integer value to an UCS_string (like snprintf("%u"))
   static UCS_string from_uint(uint64_t value);

   /// print (non-negative) n as power (like 2 in N²)
   //
   static UCS_string power(size_t n);

protected:
   // prevernt push_back() outside of this class. Use << instead.
   void push_back(Unicode uni)
      { std::vector<Unicode>::push_back(uni); }

   /// append number (in ASCII encoding like %d) to this string
   void append_int(ShapeItem num);

   /// decode UTF8 decoded \b utf into \b this string
   void decode(const UTF8 * utf, int size);

#if UCS_tracking
   /// a unique number for \b this  UCS_string
   ShapeItem instance_id;
#endif

   /// the total number of UCS_strings
   static ShapeItem total_count;

   /// the next unique instance_id
   static ShapeItem total_id;

private:
   /// constructor: UCS_string from 0-terminated C string.
   /// Made private because it was far too often used incorrectly.
   UCS_string(const char * cstring);

   /// constructor: UCS_string from 0-terminated C string.
   UCS_string(const UTF8 * cstring);

   /// prevent accidental usage of the rather dangerous default len parameter
   /// in basic_strng::erase(pos, len = npos)
    vector<Unicode> & erase(size_type pos, size_type len);

   /// prevent accidental de-allocation
   void operator delete[](void *);

   /// prevent accidental allocation
   void * operator new[](size_t size);
};
//────────────────────────────────────────────────────────────────────────────
/// an UCS_string that contains only ASCII characters,
class UCS_ASCII_string : public UCS_string
{
public:
   /// constructor. The caller MUST have checked that all characters in
   /// cstring are ASCII. Only use it with C literals, not with const char *s.
   UCS_ASCII_string(const char * ascii)
      { *this << ascii; }

   /// return \b tc in a readable form (e.g. TC_ASSIGN, TC_R_ARROW, ...).
   UCS_ASCII_string(TokenClass tc);

   /// return \b tag in a readable form (e.g. TOK_LINE, TOK_SYMBOL, ...).
   UCS_ASCII_string(TokenTag tag);

   /// return \b tvt in a readable form (e.g. TV_NONE, TV_CHAR, ...).
   UCS_ASCII_string(TokenValueType tvt);
};
//════════════════════════════════════════════════════════════════════════════

#endif // __UCS_STRING_HH_DEFINED__

