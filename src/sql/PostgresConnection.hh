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

#ifndef POSTGRES_CONNECTION_HH
#define POSTGRES_CONNECTION_HH

#include "Connection.hh"

#include <libpq-fe.h>

//════════════════════════════════════════════════════════════════════════════
/// Connection to a Postgres database
class PostgresConnection : public Connection {
public:
    /// constructor: wrap an already-open Postgres handle
    PostgresConnection(PGconn * db_in);

    /// destructor: closes the database
    virtual ~PostgresConnection();

    /// see Connection::make_prepared_query()
    virtual ArgListBuilder * make_prepared_query(const string &sql);

    /// see Connection::make_prepared_update()
    virtual ArgListBuilder * make_prepared_update(const string &sql);

    /// see Connection::transaction_begin()
    virtual void transaction_begin();

    /// see Connection::transaction_commit()
    virtual void transaction_commit();

    /// see Connection::transaction_rollback()
    virtual void transaction_rollback();

    /// see Connection::fill_tables()
    virtual void fill_tables(vector<string> & tables);

    /// see Connection::fill_cols()
    virtual void fill_cols(const string & table,
                           vector<ColumnDescriptor> & cols);

    /// see Connection::get_provider_name()
    virtual const char * get_provider_name() const
       { return "postgreSQL"; }

    /// see Connection::get_provider_type()
    virtual const char * get_provider_type() const
       { return "postgresql"; }

    /// return the name of a positional parameter. In PostgreSQL the positional
    /// parameters are: "$1", "$2", "$3", ...
    virtual const string make_positional_param(int pos)
       { return string("$") + to_string(pos + 1); }

    /// the underlying Postgres handle
    PGconn * get_db() { return db; }

private:
    /// the underlying Postgres handle
    PGconn *db;
};
//════════════════════════════════════════════════════════════════════════════

#endif
