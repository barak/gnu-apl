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

   /// constructor: UTF8_string from 0-terminated C string.
   /// @param str null-terminated ASCII source string
   UTF8_string(const ASCII * str)
      { while (const ASCII cc = *str++)   *this += cc; }

   /// constructor: copy of C string, at most len bytes, possibly less if
   /// \b str is 0-terminated.
   /// @param str source UTF-8 byte array
   /// @param len maximum number of bytes to copy
   UTF8_string(const UTF8 * str, size_t len)
      {
        loop(l, len)
            if (const UTF8 cc = *str++)   *this += cc;
            else        break;
      }

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

   /// round a digit string is the fractional part of a number between
   /// 0.0... and 0.9... up or down according to its last digit, return true
   /// if the exponent shall be increased (because 1.0 -> 0.1)
   bool round_0_1();

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
   static Unicode toUni(const UTF8 * string, int & len, bool verbose);

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
/// A UTF8 string to be used as filebuf in UTF8_ostream
class UTF8_filebuf : public filebuf
{
public:
   /// return the data in this filebuf
   const UTF8_string & get_data()
      { return data; }

protected:
   /// insert \b c into this filebuf
   /// @param c character value to append (EOF signals flush)
   virtual int overflow(int c);

   /// the data in this filebuf
   UTF8_string data;
};
//════════════════════════════════════════════════════════════════════════════
/// a UTF8 string that can be used as ostream
class UTF8_ostream : public ostream
{
public:
   /// An UTF8_string that can be used like an ostream to format data
   UTF8_ostream()
   : ostream(&utf8_filebuf)
   {}

   /// return the data in this UTF8_string
   const UTF8_string & get_data()
      { return utf8_filebuf.get_data(); }

protected:
   /// the filebuf of this ostream
   UTF8_filebuf utf8_filebuf;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __UTF8_STRING_HH_DEFINED__
