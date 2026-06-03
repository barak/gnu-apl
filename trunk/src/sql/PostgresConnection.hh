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
class PostgresConnection : public Connection {
public:
    PostgresConnection(PGconn * db_in);
    virtual ~PostgresConnection();
    virtual ArgListBuilder * make_prepared_query(const string &sql);
    virtual ArgListBuilder * make_prepared_update(const string &sql);

    virtual void transaction_begin();
    virtual void transaction_commit();
    virtual void transaction_rollback();

    virtual void fill_tables(vector<string> & tables);
    virtual void fill_cols(const string & table,
                           vector<ColumnDescriptor> & cols);

    virtual const char * get_provider_name() const
       { return "postgreSQL"; }

    virtual const char * get_provider_type() const
       { return "postgresql"; }

    /// return the name of a positional parameter. In PostgreSQL the positional
    /// parameters are: "$1", "$2", "$3", ...
    virtual const string make_positional_param(int pos)
       { return string("$") + to_string(pos + 1); }

    PGconn * get_db() { return db; }

private:
    PGconn *db;
};
//════════════════════════════════════════════════════════════════════════════

#endif
