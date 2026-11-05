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

#ifndef SQLITE_CONNECTION_HH
#define SQLITE_CONNECTION_HH

#include "Connection.hh"

#include <sqlite3.h>

//════════════════════════════════════════════════════════════════════════════
/// Connection to a Sqlite database
class SqliteConnection : public Connection
{
public:
    /// constructor: wrap an already-open Sqlite handle
    SqliteConnection(sqlite3 * db_in);

    /// destructor: closes the database
    virtual ~SqliteConnection();

    /// see Connection::make_prepared_query()
    virtual ArgListBuilder * make_prepared_query( const string & sql);

    /// see Connection::make_prepared_update()
    virtual ArgListBuilder * make_prepared_update( const string & sql);

    /// see Connection::transaction_begin()
    virtual void transaction_begin();

    /// see Connection::transaction_commit()
    virtual void transaction_commit();

    /// see Connection::transaction_rollback()
    virtual void transaction_rollback();

    /// see Connection::fill_tables()
    virtual void fill_tables(vector<string> &tables);

    /// see Connection::fill_cols()
    virtual void fill_cols(const string &table, vector<ColumnDescriptor> &cols);

    /// see Connection::make_positional_param(); Sqlite uses a bare "?"
    /// for every bind position
    virtual const string make_positional_param(int pos)
       { return "?"; }

    /// see Connection::get_provider_name()
    virtual const char * get_provider_name() const
       { return "SQLite"; }

    /// see Connection::get_provider_type()
    virtual const char * get_provider_type() const
       { return "sqlite"; }

    /// raise a DOMAIN ERROR reporting \b message together with Sqlite's
    /// own sqlite3_errmsg() text
    void raise_sqlite_error(const string & message);

    /// the underlying Sqlite handle
    sqlite3 * get_db() { return db; }

    /// run \b sql (e.g. "begin"/"commit"/"rollback") for its side effect,
    /// discarding any result
    void run_simple(const string & sql);

protected:
    /// the underlying Sqlite handle
    sqlite3 * db;
};
//════════════════════════════════════════════════════════════════════════════
/// owns a sqlite3_stmt and finalizes it on destruction
class SqliteStmtWrapper
{
public:
    /// constructor: take ownership of \b statement_in
    SqliteStmtWrapper(sqlite3_stmt * statement_in)
    : statement(statement_in) {}

    /// destructor: finalizes the wrapped statement
    virtual ~SqliteStmtWrapper()
       { sqlite3_finalize(statement); }

    /// the wrapped statement
    sqlite3_stmt * get_statement(void)
       { return statement; }

protected:
    /// the wrapped statement
    sqlite3_stmt * statement;
};
//════════════════════════════════════════════════════════════════════════════

#endif
