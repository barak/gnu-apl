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

#ifndef CONNECTION_HH
#define CONNECTION_HH

#include "apl-sqlite.hh"
#include "ArgListBuilder.hh"

#include <stdlib.h>

#include <vector>

//════════════════════════════════════════════════════════════════════════════
/// the name and SQL type of one column, as reported by fill_cols()
class ColumnDescriptor
{
public:
    /// constructor
    ColumnDescriptor(const string &name_in, const string &type_in)
    : name( name_in ),
      type( type_in )
    {}

    // name/type are const, so a real assignment operator can't assign
    // *this -- the old body returned a fresh temporary and silently left
    // *this untouched, a trap for any vector op that assigns (erase,
    // insert, resize, sort, ...) rather than just push_back (Blake
    // McBride, Bugs14 #13). Delete it rather than pretend it works.
    ColumnDescriptor & operator=(const ColumnDescriptor &) = delete;

    /// the column name
    const string & get_name()
        { return name; }

    /// the column's SQL type name
    const string & get_type()
        { return type; }

private:
   /// name of the colunmn
    const string name;

   /// data type of the column
    const string type;
};
//════════════════════════════════════════════════════════════════════════════
/// abstract base for a database connection (one concrete subclass per
/// ⎕SQL provider, e.g. Sqlite/Postgres)
class Connection
{
public:
    /// destructor
    virtual ~Connection() {}

    /// prepare \b sql (a SELECT) and return an ArgListBuilder for binding
    /// its arguments and running it
    virtual ArgListBuilder *make_prepared_query(const string & sql)  = 0;

    /// prepare \b sql (an INSERT/UPDATE/DELETE) and return an
    /// ArgListBuilder for binding its arguments and running it
    virtual ArgListBuilder *make_prepared_update(const string & sql) = 0;

    /// begin a transaction
    virtual void transaction_begin()                                 = 0;

    /// commit the current transaction
    virtual void transaction_commit()                                = 0;

    /// roll back the current transaction
    virtual void transaction_rollback()                              = 0;

    /// append the names of all tables in the database to \b tables
    virtual void fill_tables(vector<string> & tables)                = 0;

    /// append the column descriptors of \b table to \b cols
    virtual void fill_cols(const string & table,
                           vector<ColumnDescriptor> & cols)          = 0;

    /// the provider-specific placeholder syntax for bind position \b pos
    /// (0-based), e.g. "?" for Sqlite or "$1", "$2", ... for Postgres
    virtual const string make_positional_param(int pos)              = 0;

    /// human-readable provider name, e.g. "Sqlite" or "Postgres"
    virtual const char * get_provider_name() const                   = 0;

    /// provider type tag used by ⎕SQL to select this Connection
    virtual const char * get_provider_type() const                   = 0;

    /// return \b sql with every '?' placeholder rewritten to this
    /// provider's own positional-parameter syntax via make_positional_param()
    virtual const string replace_bind_args(const string & sql);
};
//════════════════════════════════════════════════════════════════════════════

#endif
