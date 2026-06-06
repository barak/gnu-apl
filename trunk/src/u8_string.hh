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

#ifndef __U8_STRING_HH_DEFINED__
#define __U8_STRING_HH_DEFINED__

#include <cstddef>   // std::nullptr_t
#include <stdarg.h>

#include "UTF8_string.hh"   // charP(), utf8P(), UTF8

/// Thin inline wrappers around \<string.h\>, \<stdlib.h\>, and \<stdio.h\>
/// that accept const UTF8 * (= const uint8_t *) instead of const char *,
/// eliminating charP() casts at every call site.
namespace u8
{

/// convert UTF-8 string \b s to long long (like ::atoll).
/// @param s null-terminated UTF-8 string
/// @return  converted long long value
inline long long
atoll(const UTF8 * s)
{
   return ::atoll(charP(s));
}

/// scan \b s according to \b fmt (like ::sscanf); delegates to vsscanf.
/// @param s   null-terminated UTF-8 input string
/// @param fmt printf-style format string
/// @return    number of items successfully matched and assigned
inline int
sscanf(const UTF8 * s, const char * fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   const int ret = vsscanf(charP(s), fmt, ap);
   va_end(ap);
   return ret;
}

/// compare at most \b n bytes of UTF-8 string \b s1 with ASCII string \b s2.
/// @param s1 UTF-8 string
/// @param s2 ASCII string
/// @param n  maximum number of bytes to compare
/// @return   negative, zero, or positive (like ::strncmp)
inline int
strncmp(const UTF8 * s1, const char * s2, size_t n)
{
   return ::strncmp(charP(s1), s2, n);
}

/// compare at most \b n bytes of ASCII string \b s1 with UTF-8 string \b s2.
/// @param s1 ASCII string
/// @param s2 UTF-8 string
/// @param n  maximum number of bytes to compare
/// @return   negative, zero, or positive (like ::strncmp)
inline int
strncmp(const char * s1, const UTF8 * s2, size_t n)
{
   return ::strncmp(s1, charP(s2), n);
}

/// find the first occurrence of ASCII string \b needle in UTF-8 string \b haystack.
/// @param haystack UTF-8 string to search
/// @param needle   ASCII substring to find
/// @return         pointer to first match, or null if not found
inline const UTF8 *
strstr(const UTF8 * haystack, const char * needle)
{
   return utf8P(::strstr(charP(haystack), needle));
}

/// convert UTF-8 string \b s to double; store end pointer as char *.
/// @param s      null-terminated UTF-8 string
/// @param endptr set to first character after the parsed number (as char *)
/// @return       converted double value (like ::strtod)
inline double
strtod(const UTF8 * s, char ** endptr)
{
   return ::strtod(charP(s), endptr);
}

/// convert UTF-8 string \b s to double; store end pointer as UTF8 *.
/// @param s      null-terminated UTF-8 string
/// @param endptr set to first character after the parsed number (as UTF8 *)
/// @return       converted double value (like ::strtod)
inline double
strtod(const UTF8 * s, UTF8 ** endptr)
{
   char * cp = 0;
   const double ret = ::strtod(charP(s), &cp);
   if (endptr)   *endptr = utf8P(cp);
   return ret;
}

/// convert UTF-8 string \b s to double; discard end pointer.
/// @param s null-terminated UTF-8 string
/// @return  converted double value (like ::strtod)
inline double
strtod(const UTF8 * s, std::nullptr_t)
{
   return ::strtod(charP(s), 0);
}

/// convert UTF-8 string \b s to long long in base \b base; store end pointer as char *.
/// @param s      null-terminated UTF-8 string
/// @param endptr set to first character after the parsed number (as char *)
/// @param base   numeric base (2–36, or 0 for auto-detect)
/// @return       converted long long value (like ::strtoll)
inline long long
strtoll(const UTF8 * s, char ** endptr, int base)
{
   return ::strtoll(charP(s), endptr, base);
}

/// convert UTF-8 string \b s to long long in base \b base; store end pointer as UTF8 *.
/// @param s      null-terminated UTF-8 string
/// @param endptr set to first character after the parsed number (as UTF8 *)
/// @param base   numeric base (2–36, or 0 for auto-detect)
/// @return       converted long long value (like ::strtoll)
inline long long
strtoll(const UTF8 * s, UTF8 ** endptr, int base)
{
   char * cp = 0;
   const long long ret = ::strtoll(charP(s), &cp, base);
   if (endptr)   *endptr = utf8P(cp);
   return ret;
}

/// convert UTF-8 string \b s to long long in base \b base; discard end pointer.
/// @param s    null-terminated UTF-8 string
/// @param base numeric base (2–36, or 0 for auto-detect)
/// @return     converted long long value (like ::strtoll)
inline long long
strtoll(const UTF8 * s, std::nullptr_t, int base)
{
   return ::strtoll(charP(s), 0, base);
}

} // namespace u8

#endif // __U8_STRING_HH_DEFINED__
