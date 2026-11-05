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

#include <string.h>
#include <stdio.h>

#include "Avec.hh"
#include "Common.hh"
#include "Error.hh"
#include "Output.hh"
#include "PrintOperator.hh"
#include "UTF8_string.hh"
#include "Workspace.hh"

/* from RFC 3629 / STD 63:

   Char. number range  |        UTF-8 octet sequence
      (hexadecimal)    |              (binary)
   --------------------+---------------------------------------------
   0000 0000-0000 007F | 0xxxxxxx
   0000 0080-0000 07FF | 110xxxxx 10xxxxxx
   0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
   0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

   RFC 2279, the predecessor of RFC 3629, also allowed these:

   0000 0000-0000 007F   0xxxxxxx
   0000 0080-0000 07FF   110xxxxx 10xxxxxx
   0000 0800-0000 FFFF   1110xxxx 10xxxxxx 10xxxxxx
   0001 0000-001F FFFF   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
   0020 0000-03FF FFFF   111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
   0400 0000-7FFF FFFF   1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx

   In order to be more general, we follow RFC 2279 and also allow 5-byte and
   6-byte encodings
*/

//════════════════════════════════════════════════════════════════════════════
UTF8_string::UTF8_string(const UCS_string & ucs)
{
   Log(LOG_char_conversion)
      CERR << "UTF8_string::UTF8_string(ucs = " << ucs << ")" << endl;

   loop(u, ucs.size())
      {
        uint32_t uni = ucs[u];
        if (uni < 0x80)            // 1-byte unicode (ASCII)
           {
             *this += uni;
           }
        else                       // N-byte unicode
           {
             // For uni >= 0x80000000 (every genuinely negative Unicode,
             // e.g. ⎕UCS ¯1, once cast to uint32_t here) the loop below
             // needs a 7th continuation byte -- one byte more than any
             // UTF-8 variant (even the old pre-RFC-3629 6-byte/31-bit
             // scheme) can represent, and one byte more than
             // libapl.h's Unicode_to_UTF8() contract ("provide at least
             // 7 bytes for dest, dest will be 0-terminated") accounts
             // for: that function writes exactly this string's bytes
             // plus a NUL, so an 8-byte need against a 7-byte documented
             // buffer is a 1-byte OOB write for any caller that follows
             // the documented contract. Clamp instead of growing past
             // the 6-byte/31-bit maximum -- 0x7FFFFFFF is not a
             // meaningful Unicode codepoint either way.
             if (uni >= 0x80000000U)   uni = 0x7FFFFFFF;

             // sixbits must hold up to 6 continuation bytes: for
             // uni == 0x7FFFFFFF the loop below writes indices 0..5
             // (confirmed with ASan -- sixbits[5] overflowed by one
             // byte before the clamp above existed).
             //
             char sixbits[6];
             char * s = sixbits;
             while (uni >= 0x40U >> (s - sixbits))  
                   { *s++ = 0x80 | (uni & 0x3F);   uni >>= 6; }
         
             *this += char(uni | 0xFE << (6 + (sixbits - s)));
             while (s > sixbits)   *this += *--s;
           }
      }

   Log(LOG_char_conversion)
      CERR << "UTF8_string::UTF8_string(): utf = " << *this << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
UTF8_string::dump_hex(ostream & out, const UTF8 * utf, int size, int max_bytes)
{
   if (size < 3*max_bytes)   // small string
      {
        loop(b, size)
           {
              if (utf[b] == 0)   break;
             if (b)   out << " ";
             out << HEX2(utf[b]);
           }
      }
   else                        // large string
      {
        const int max1 = 2*max_bytes/3 - 2;
        const int max2 = 1*max_bytes/3 - 2;
        loop(b, max1)
           {
             if (utf[b] == 0)   break;
             if (b)   out << " ";
             out << HEX2(utf[b]);
           }
        out << "  ... ";
        for (int b = size - max2; b < size; ++b)
           {
             if (utf[b] == 0)   break;
             out << " "; out << HEX2(utf[b]);
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
Unicode
UTF8_string::toUni(const UTF8 * string, int & len, bool verbose,
                  const UTF8 * end)
{
const uint32_t b0 = *string++;
   if (b0 < 0x80)                  { len = 1;   return Unicode(b0); }

uint32_t bx = b0;   // the "significant" bits in b0
   if      ((b0 & 0xE0) == 0xC0)   { len = 2;   bx &= 0x1F; }
   else if ((b0 & 0xF0) == 0xE0)   { len = 3;   bx &= 0x0F; }
   else if ((b0 & 0xF8) == 0xF0)   { len = 4;   bx &= 0x07; }
   else if ((b0 & 0xFC) == 0xF8)   { len = 5;   bx &= 0x03; }
   else if ((b0 & 0xFE) == 0xFC)   { len = 6;   bx &= 0x01; }
   else if (verbose)
      {
        CERR << "Bad UTF8 sequence: " << HEX(b0);
        loop(j, 6)
           {
             // most callers pass no end, in which case we must not read
             // any further bytes at all (rather than only bounding the
             // read when end happens to be supplied): confirmed OOB read
             // of up to 6 bytes past string with end == 0.
             if (end == 0 || string + j >= end)   break;
             const uint32_t bx = string[j];
             if (bx & 0x80)   CERR << " " << HEX(bx);
             else              break;
           }

        CERR <<  " at " LOC << endl;

        // Bugs28 #100(x): a BACKTRACE call used to sit here too. The
        // CERR diagnostic and the recovery/fallback below it (see the
        // comment) are both deliberate, but a full stack-trace dump for
        // perfectly ordinary bad input (e.g. )LOAD of a workspace file
        // containing invalid UTF-8) is not -- unlike the genuinely
        // exceptional cases BACKTRACE exists for elsewhere.
        //
        // No Assert() here: at the project's *default* ASSERT_LEVEL (1,
        // not 0 -- see configure.ac), Assert(0 && ...) is not a no-op,
        // it throws via do_Assert(), so it would pre-empt exactly the
        // fallback below that a malformed/truncated workspace file (or
        // any other bad input) needs: without it, len keeps whatever
        // the caller passed in and the continuation-byte code below
        // runs with a garbage/attacker-uninfluenced length -- an
        // infinite loop (some callers pre-set len=0, advancing the
        // cursor by 0 forever) or an OOB read (DiffOut::different()
        // leaves len uninitialized). Fail the same way the non-verbose
        // branch already does.
        len = 0;
        return Invalid_Unicode;
      }
   else
      {
        len = 0;
        return Invalid_Unicode;
      }

   // string already advanced past b0; len-1 continuation bytes remain
   // to be read at string[0]..string[len-2]. Reject a sequence that
   // would read at or past end instead of reading out of bounds.
   //
   if (end && string + (len - 1) > end)
      {
        len = 0;
        return Invalid_Unicode;
      }

uint32_t uni = 0;
   loop(l, len - 1)
       {
         const UTF8 subc = *string++;
         if ((subc & 0xC0) != 0x80)
            {
              CERR << "Bad UTF8 sequence: " << HEX(b0) << "... at " LOC << endl;

              // No Assert() here: at the project's default ASSERT_LEVEL
              // (1), Assert(0 && ...) throws via do_Assert() instead of
              // being a no-op, which would pre-empt exactly this
              // fallback -- without it, a bad continuation byte would
              // fall through and get silently folded into uni as if it
              // were valid instead of failing the decode. len was
              // already fully consumed by the time we get here (this
              // loop doesn't advance len), so just return the failure
              // the same way the lead-byte checks above do.
              return Invalid_Unicode;
            }

         bx  <<= 6;
         uni <<= 6;
         uni |= subc & 0x3F;
       }

   return Unicode(bx | uni);
}
//────────────────────────────────────────────────────────────────────────────
int
UTF8_string::bytes_chars(const void * string)
{
const UTF8 * str = utf8P(string);
int ret = 0;

   while (*str)   { if (0x80 == (0xC0 & *str++))   ++ret; }
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
Unicode
UTF8_string::getc(istream & in)
{
const uint32_t b0 = in.get() & 0xFF;
   if (!in.good())         return Invalid_Unicode;
   if      (b0 < 0x80)   { return Unicode(b0);    }

uint32_t bx = b0;   // the "significant" bits in b0
int len;

   if      ((b0 & 0xE0) == 0xC0)   { len = 1;   bx &= 0x1F; }
   else if ((b0 & 0xF0) == 0xE0)   { len = 2;   bx &= 0x0F; }
   else if ((b0 & 0xF8) == 0xF0)   { len = 3;   bx &= 0x07; }
   else if ((b0 & 0xFC) == 0xF8)   { len = 4;   bx &= 0x03; }
   else if ((b0 & 0xFE) == 0xFC)   { len = 5;   bx &= 0x01; }
   else
      {
        CERR << "Bad UTF8 sequence: " << HEX(b0) << "... at " LOC << endl;
        BACKTRACE
        return Invalid_Unicode;
      }

char cc[8];   // max 6 continuation bytes + NUL, rounded up
   in.get(cc, len + 1);   // read subsequent characters + terminating 0
   if (!in.good())   return Invalid_Unicode;

uint32_t uni = 0;
   loop(l, len)
       {
         const UTF8 subc = cc[l];
         if ((subc & 0xC0) != 0x80)
            {
              CERR << "Bad UTF8 sequence: " << HEX(b0) << "... at " LOC << endl;
              return Invalid_Unicode;
            }

         bx  <<= 6;
         uni <<= 6;
         uni |= subc & 0x3F;
       }

   return Unicode(bx | uni);
}
//────────────────────────────────────────────────────────────────────────────
bool
UTF8_string::starts_with(const char * path) const
{
   if (path == 0)   return false;   // no path provided

const size_t path_len = strlen(path);
   if (path_len > size())   return false;   // path_len longer than this string

   // can't use strncmp() because this string may not be 0-terminated
   //
   loop(p, path_len)   if (at(p) != path[p])   return false;
   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
UTF8_string::ends_with(const char * ext) const
{
   if (ext == 0)   return false;   // no ext provided

const size_t ext_len = strlen(ext);
   if (ext_len > size())   return false;   // ext longer than this string

   // can't use strncmp() because this string may not be 0-terminated
   //
   loop(e, ext_len)   if (at(size() - ext_len + e) != ext[e])   return false;
   return true;
}
//────────────────────────────────────────────────────────────────────────────
int
UTF8_string::un_HTML(int in_HTML)
{
int dest = 0;
bool got_tag = false;
   loop(src, size())
      {
        const char cc = at(src);
        if (in_HTML == 2)   // inside HTML tag, i.e. < seen
           {
             if (cc == '>')   in_HTML = 1;   // in HTML but not in HTML tag
             continue;
           }

        if (cc == '<')   // start of HTML tag
           {
             in_HTML = 2;   // now inside HTML tag
             got_tag = true;
             continue;
           }

        if (cc != '&')   // unless HTML-escaped char
           {
             at(dest++) = cc;
             continue;
           }

        // at this point cc == '&' which is the start of an HTML-escaped
        // character. This can be:
        //
        // &NN;  with decimal digits NN...,        or
        // &#XX;  with hexadecimal digits XX...,   or (incomplete list)
        // &gt;   for >,                           or
        // &lt;   for <
        //
        // There exist more HTML escapes, but the function producing them
        // (i.e. UCS_string::to_HTML()) does not emit them.
        //
        const int rest = size() - src;
        if (rest > 3 && at(src + 1) == '#' &&
            (strchr("0123456789", at(src + 2)) || at(src + 2) == 'x'))
           {
             src += 2;   // skip "&#"
             int val = 0;
             char * end = 0;
             if (at(src) == 'x')   // hex value
                {
                  ++src;   // skip "x"
                  val = strtoll(c_str() + src, &end, 16);
                }
             else                 // decimal
                {
                  val = strtoll(c_str() + src, &end, 10);
                }
             at(dest++) = val;
             src = end - c_str();   // skip hex or decimal digits
             // ";" skipped by loop()
           }
        else if (rest > 3 && at(src + 1) == 'g' &&
                             at(src + 2) == 't' &&
                             at(src + 3) == ';')
           {
             at(dest++) = '>';
             src += 3;   // skip "gt;"
           }
        else if (rest > 3 && at(src + 1) == 'l' &&
                             at(src + 2) == 't' &&
                             at(src + 3) == ';')
           {
             at(dest++) = '<';
             src += 3;   // skip "lt;"
           }
        else if (rest > 5 && at(src + 1) == 'n' &&
                             at(src + 2) == 'b' &&
                             at(src + 3) == 's' &&
                             at(src + 4) == 'p' &&
                             at(src + 5) == ';')
           {
             at(dest++) = ' ';
             src += 5;   // skip "nbsp;"
           }
        else
           {
             at(dest++) = cc;
           }
      }

   if (got_tag)   dest = 0;
   resize(dest);
   return in_HTML;
}
//────────────────────────────────────────────────────────────────────────────
uint64_t
UTF8_string::long_value() const
{
uint64_t ret = 0;
   loop(j, size())   ret = 10*ret + (at(j) - UNI_0);
   return ret;
}
//════════════════════════════════════════════════════════════════════════════
/// declared in PrintOperator.hh
ostream &
operator <<(ostream & os, const UTF8_string & utf)
{
   return os << utf.c_str();
}
//════════════════════════════════════════════════════════════════════════════
/// declared in PrintOperator.hh
ostream &
operator <<(ostream & os, const UTF8 * utf)
{
   return os << charP(utf);
}
//════════════════════════════════════════════════════════════════════════════
UTF8_string_vector::UTF8_string_vector(const char * lines)
{
UTF8_string current;
   current.reserve(80);
   while (*lines)
      {
        const char cc = *lines++;
        if (cc == '\r')   continue;   // ignore \r
        if (cc == '\n')
           {
             push_back(current);
             current.clear();
           }
        else
           {
             current += cc;
           }
      }

   if (current.size())   push_back(current);

}
//════════════════════════════════════════════════════════════════════════════
