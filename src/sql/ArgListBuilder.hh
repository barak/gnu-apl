/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2014  Elias Mårtenson

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

#ifndef ARG_LIST_BUILDER_HH
#define ARG_LIST_BUILDER_HH

#include "apl-sqlite.hh"

//════════════════════════════════════════════════════════════════════════════
/// abstract base for binding a prepared SQL statement's arguments (by
/// 0-based position) and running it
class ArgListBuilder
{
public:
    /// destructor
    virtual ~ArgListBuilder() {}

    /// bind a string argument at 0-based position \b pos
    virtual void append_string(const string &arg, int pos) = 0;

    /// bind an integer argument at 0-based position \b pos
    virtual void append_long(long arg, int pos)            = 0;

    /// bind a floating-point argument at 0-based position \b pos
    virtual void append_double(double arg, int pos)        = 0;

    /// bind a SQL NULL at 0-based position \b pos
    virtual void append_null(int pos)                      = 0;

    /// execute the prepared statement and return its result as an APL value
    virtual Value_P run_query()                            = 0;

    /// discard all bound arguments and reset the statement for reuse
    virtual void clear_args()                              = 0;
};
//════════════════════════════════════════════════════════════════════════════

#endif
