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

#ifndef __UTF8_STRING_HH_DEFINED__
#define __UTF8_STRING_HH_DEFINED__

#include <iostream>
#include <stdint.h>
#include <string>
#include <vector>

#include "Common.hh"

using namespace std;

class UCS_string;
class Value;

//════════════════════════════════════════════════════════════════════════════
/// one byte (= 7-bit character !) of an ASCII string
typedef char ASCII;

//════════════════════════════════════════════════════════════════════════════
/// frequently used cast to const UTF8 *
/// @param vp pointer to cast
inline const UTF8 *
utf8P(const void * vp)
{
  return reinterpret_cast<const UTF8 *>(vp);
}
//════════════════════════════════════════════════════════════════════════════
/// frequently used cast to UTF8 *
/// @param cp ASCII pointer to cast
inline UTF8 *
utf8P(ASCII * cp)
{
  return reinterpret_cast<UTF8 *>(cp);
}
//════════════════════════════════════════════════════════════════════════════
/// an UTF8 encoded Unicode (RFC 3629) string
class UTF8_string : public std::string
{
public:
   /// constructor: empty UTF8_string
   UTF8_string()
   {}

   /// constructor: UTF8_string from 0-terminated ASCII string.
   /// @param str null-terminated ASCII source string
   UTF8_string(const ASCII * str)
      { while (const ASCII cc = *str++)   *this += cc; }

   /// constructor: UTF8_string from 0-terminated UTF-8 string.
   /// @param str null-terminated UTF-8 source string
   UTF8_string(const UTF8 * str)
      { while (const UTF8 cc = *str++)   *this += cc; }

   /// constructor: copy of ASCII string, exactly len bytes (Bugs10 #5,
   /// Blake McBride: the caller supplies an explicit length precisely
   /// because the data is not NUL-terminated text; stopping early on a
   /// NUL byte silently truncated it).
   /// @param str source ASCII byte array
   /// @param len number of bytes to copy
   UTF8_string(const ASCII * str, size_t len)
   : std::string(str, len)
      {}

   /// constructor: copy of UTF-8 string, exactly len bytes (see Bugs10 #5
   /// above: U+0000 is a legal Unicode character, so this must not stop
   /// at the first NUL either).
   /// @param str source UTF-8 byte array
   /// @param len number of bytes to copy
   UTF8_string(const UTF8 * str, size_t len)
   : std::string(reinterpret_cast<const char *>(str), len)
      {}

   /// constructor: copy of UCS string. The Unicodes in ucs
   /// will be UTF8-encoded
   /// @param ucs source Unicode string to encode as UTF-8
   UTF8_string(const UCS_string & ucs);

   /// return the last byte in this string
   UTF8 back() const
      { Assert(size());   return at(size() - 1); }

   /// erase one item at \b pos
   /// @param pos byte position of the element to remove
   void erase(size_t pos)
      { std::string::erase(begin() + pos); }

   /// append a (0-terminated) C string
   /// @param ascii null-terminated ASCII string to append
   UTF8_string & operator <<(const char * ascii)
      { while (*ascii)   *this += *ascii++;   return *this; }

   /// append the UTF8_string \b suffix
   /// @param suffix UTF-8 string to append
   UTF8_string & operator <<(const UTF8_string & suffix)
      { loop(s, suffix.size())   *this += suffix[s];   return *this; }

   /// discard the last byte in this string
   void pop_back()
      { Assert(size());   resize(size() - 1); }

   /// return true iff string ends with ext (usually a file name extension)
   /// @param ext expected file extension (including dot, e.g. ".apl")
   bool ends_with(const char * ext) const;

   /// essentially strtol(this, 0, 10)
   uint64_t long_value() const;

   /// return true iff string starts with path (usually a file path)
   /// @param path expected path prefix to check for
   bool starts_with(const char * path) const;

   /// skip over < ... > and expand &lt; and friends
   /// @param in_HTML current HTML nesting depth on entry
   int un_HTML(int in_HTML);

   /// return the (always positive) difference between the number of bytes
   /// and the number of chars in the (UTF8 encoded) \b string.
   /// @param string pointer to a null-terminated UTF-8 encoded string
   static int bytes_chars(const void * string);

   /// display bytes in \b utf
   /// @param out output stream to write to
   /// @param utf pointer to the UTF-8 byte array to display
   /// @param size total number of bytes in utf
   /// @param max_bytes maximum number of bytes to display
   static void dump_hex(ostream & out, const UTF8 * utf, int size,
                        int max_bytes);

   /// return the next UTF8 encoded char from an input file
   /// @param in input stream to read from
   static Unicode getc(istream & in);

   /// convert the first char(s) in UTF8-encoded string to Unicode,
   /// setting len to the number of bytes in the UTF8 encoding of the char
   /// @param string pointer to the start of a UTF-8 encoded byte sequence
   /// @param len output: number of bytes consumed by the first character
   /// @param verbose true to print diagnostics for malformed sequences
   /// @param end optional: one past the last valid byte at \b string; if
   ///        given, the returned char's encoding is guaranteed to not
   ///        read at or past \b end (a truncated sequence is treated as
   ///        malformed instead of reading out of bounds)
   static Unicode toUni(const UTF8 * string, int & len, bool verbose,
                       const UTF8 * end = 0);

};
//════════════════════════════════════════════════════════════════════════════
class UTF8_string_vector : public std::vector<UTF8_string>
{
public:
   /// constructor: from string with lines separated by \n. The lines
   /// will be the items of the vector (with any CR or LF characters removed).
   /// @param lines newline-separated C string to split into individual lines
   UTF8_string_vector(const char * lines);
};
//════════════════════════════════════════════════════════════════════════════

#endif // __UTF8_STRING_HH_DEFINED__
