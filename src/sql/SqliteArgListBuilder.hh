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

#ifndef SQLITE_ARG_LIST_BUILDER_HH
#define SQLITE_ARG_LIST_BUILDER_HH

#include "apl-sqlite.hh"
#include "SqliteConnection.hh"
#include "ArgListBuilder.hh"

//════════════════════════════════════════════════════════════════════════════
/// ArgListBuilder for a Sqlite prepared statement
class SqliteArgListBuilder : public ArgListBuilder
{
public:
    /// constructor: prepare \b sql on \b connection_in
    SqliteArgListBuilder( SqliteConnection *connection_in, const string &sql );

    /// destructor
    virtual ~SqliteArgListBuilder();

    /// see ArgListBuilder::append_string()
    virtual void append_string(const string &arg, int pos);

    /// see ArgListBuilder::append_long()
    virtual void append_long(long arg, int pos);

    /// see ArgListBuilder::append_double()
    virtual void append_double(double arg, int pos);

    /// see ArgListBuilder::append_null()
    virtual void append_null(int pos);

    /// see ArgListBuilder::run_query()
    virtual Value_P run_query();

    /// see ArgListBuilder::clear_args()
    virtual void clear_args();

private:
    /// (re-)prepare \b sql into \b statement
    void init_sql();

    /// the SQL text this builder was constructed with
    string sql;

    /// the connection \b statement was prepared on
    SqliteConnection * connection;

    /// the underlying Sqlite prepared statement
    sqlite3_stmt * statement;
};
//════════════════════════════════════════════════════════════════════════════

#endif
