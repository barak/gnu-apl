/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2017 Elias Mårtenson

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

#ifndef __Regexp_HH__DEFINED__
#define __Regexp_HH__DEFINED__

#include "UCS_string.hh"

#define PCRE2_CODE_UNIT_WIDTH 32
#include <pcre2.h>

/// Regular expression match
class RegexpMatch
{
public:
    RegexpMatch(pcre2_code * code, const UCS_string & B, PCRE2_SIZE start);
    virtual ~RegexpMatch();

    uint32_t get_ovector_count() const
       { return ovector_count; }

    const PCRE2_SIZE * get_ovector() const
       { return ovector; }

    bool is_match() const;
    int num_matches() const;
    UCS_string matched_string() const;

protected:
   PCRE2_SIZE * ovector;
   uint32_t ovector_count;

   pcre2_match_data * match_data;

   int match_result;
   const UCS_string & matched_B;
};

/// Helper class for ⎕RE
class Regexp
{
public:
    Regexp(const UCS_string & pattern, int flags);
    virtual ~Regexp();
    RegexpMatch * match(const UCS_string & match, PCRE2_SIZE size) const;
    int expression_count() const;

    pcre2_code * get_code() const
       { return code; }

   static UCS_string pcre_error(int ec);

protected:
    pcre2_code * code;
};

#endif // __Regexp_HH__DEFINED__
