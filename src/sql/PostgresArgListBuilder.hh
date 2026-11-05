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

#ifndef POSTGRES_ARG_LIST_BUILDER_HH
#define POSTGRES_ARG_LIST_BUILDER_HH

#include "apl-sqlite.hh"
#include "PostgresConnection.hh"
#include "ArgListBuilder.hh"

//════════════════════════════════════════════════════════════════════════════
/// one bound argument of a Postgres PQexecParams() call, deferred until
/// run_query() so its type/value/length/format can be filled into the
/// parallel arrays PQexecParams() expects
class PostgresArg
{
public:
    /// destructor
    virtual ~PostgresArg() {}

    /// fill this argument's entry (at index \b pos) into the \b types/
    /// \b values/\b lengths/\b formats arrays passed to PQexecParams()
    virtual void update( Oid *types, const char **values, int *lengths, int *formats, int pos ) = 0;
};

/// a PostgresArg holding one bound value of type \b T (string, long, or
/// double)
template<class T>
class PostgresBindArg : public PostgresArg
{
public:
    /// constructor: remember the value to bind
    PostgresBindArg( const T &arg_in ) : arg( arg_in ), string_arg( NULL ) {}

    /// destructor
    virtual ~PostgresBindArg();

    /// see PostgresArg::update()
    virtual void update( Oid *types, const char **values, int *lengths, int *formats, int pos );

private:
    /// the bound value
    const T arg;

    /// the value, rendered as a NUL-terminated string PQexecParams() can
    /// use (owned, freed in the destructor)
    char *string_arg;
};

/// a PostgresArg holding a bound SQL NULL
class PostgresNullArg : public PostgresArg
{
public:
    /// destructor
    virtual ~PostgresNullArg() {}

    /// see PostgresArg::update()
    virtual void update( Oid *types, const char **values, int *lengths, int *formats, int pos );
};

/// ArgListBuilder for a Postgres query run via PQexecParams()
class PostgresArgListBuilder : public ArgListBuilder {
public:
    /// constructor: remember \b sql_in and the connection to run it on
    PostgresArgListBuilder( PostgresConnection *connection_in, const string &sql_in );

    /// destructor
    virtual ~PostgresArgListBuilder();

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
    /// the connection to run the query on
    PostgresConnection *connection;

    /// the SQL text this builder was constructed with
    string sql;

    /// the bound arguments, in position order
    vector<PostgresArg *> args;
};

/// owns a PGresult and PQclear()s it on destruction
class PostgresResultWrapper {
public:
    /// constructor: take ownership of \b result_in
    PostgresResultWrapper( PGresult *result_in ) : result( result_in ) {}

    /// destructor: PQclear()s the wrapped result
    ~PostgresResultWrapper() { PQclear( result ); }

    /// the wrapped result
    PGresult *get_result( void ) { return result; }

private:
    /// the wrapped result
    PGresult *result;
};
//════════════════════════════════════════════════════════════════════════════

#endif
