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

// Thin inline wrappers around <string.h> / <stdlib.h> / <stdio.h> functions
// that accept const UTF8 * (= const uint8_t *) in place of const char *,
// replacing charP() casts at call sites.

namespace u8
{

//════════════════════════════════════════════════════════════════════════════
// strncmp
//════════════════════════════════════════════════════════════════════════════

inline int
strncmp(const UTF8 * s1, const char * s2, size_t n)
{
   return ::strncmp(charP(s1), s2, n);
}

inline int
strncmp(const char * s1, const UTF8 * s2, size_t n)
{
   return ::strncmp(s1, charP(s2), n);
}

//════════════════════════════════════════════════════════════════════════════
// strtod — char ** endptr variant keeps the existing call-site char * end;
//          UTF8 ** endptr variant avoids the utf8P(end) reconversion.
//════════════════════════════════════════════════════════════════════════════

inline double
strtod(const UTF8 * s, char ** endptr)
{
   return ::strtod(charP(s), endptr);
}

inline double
strtod(const UTF8 * s, UTF8 ** endptr)
{
   char * cp = 0;
   const double ret = ::strtod(charP(s), &cp);
   if (endptr)   *endptr = utf8P(cp);
   return ret;
}

inline double
strtod(const UTF8 * s, std::nullptr_t)
{
   return ::strtod(charP(s), 0);
}

//════════════════════════════════════════════════════════════════════════════
// strtoll — same two endptr variants as strtod
//════════════════════════════════════════════════════════════════════════════

inline long long
strtoll(const UTF8 * s, char ** endptr, int base)
{
   return ::strtoll(charP(s), endptr, base);
}

inline long long
strtoll(const UTF8 * s, UTF8 ** endptr, int base)
{
   char * cp = 0;
   const long long ret = ::strtoll(charP(s), &cp, base);
   if (endptr)   *endptr = utf8P(cp);
   return ret;
}

inline long long
strtoll(const UTF8 * s, std::nullptr_t, int base)
{
   return ::strtoll(charP(s), 0, base);
}

//════════════════════════════════════════════════════════════════════════════
// atoll
//════════════════════════════════════════════════════════════════════════════

inline long long
atoll(const UTF8 * s)
{
   return ::atoll(charP(s));
}

//════════════════════════════════════════════════════════════════════════════
// sscanf — delegates to vsscanf so the variadic args pass through cleanly
//════════════════════════════════════════════════════════════════════════════

inline int
sscanf(const UTF8 * s, const char * fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   const int ret = vsscanf(charP(s), fmt, ap);
   va_end(ap);
   return ret;
}

} // namespace u8

#endif // __U8_STRING_HH_DEFINED__
