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

#include "PostgresProvider.hh"
#include "PostgresConnection.hh"

//-----------------------------------------------------------------------------
static PostgresConnection *
create_postgres_connection(Value_P B)
{
    if( !B->is_char_string() ) {
        MORE_ERROR() << "Argument must be a single string";
        DOMAIN_ERROR;
    }

     const UTF8_string B_utf(B->get_UCS_ravel());
    string connect_args = B_utf.c_str();

    const char * keywords[] = { "dbname", NULL };
    const char *values[] = { connect_args.c_str(), NULL };
    PGconn *db = PQconnectdbParams( keywords, values, 1 );

    ConnStatusType status = PQstatus( db );
    if( status != CONNECTION_OK ) {
        stringstream out;
        out << "Error connecting to Postgres database: " << PQerrorMessage( db );
        MORE_ERROR() << out.str().c_str();
        PQfinish( db );
        DOMAIN_ERROR;
    }

    int result = PQsetClientEncoding( db, "UTF-8" );
    if( result != 0 ) {
        stringstream out;
        out << "Unable to set encoding to UTF-8: " << PQerrorMessage( db );
        MORE_ERROR() << out.str().c_str();
        PQfinish( db );
        DOMAIN_ERROR;
    }

   return new PostgresConnection(db);
}
//-----------------------------------------------------------------------------
Connection *
PostgresProvider::open_database( Value_P B )
{
    Connection *connection = create_postgres_connection( B );
    return connection;
}
//-----------------------------------------------------------------------------
const char *
PostgresProvider::version_string() const
{
const int version_number = PQlibVersion();
static char version[100];
   snprintf(version, sizeof(version), "PostgreSQL-%u.%u.%u",
            version_number/10000,
            version_number/100 %100,
            version_number%100);
   return version;
}
//-----------------------------------------------------------------------------
int
PostgresProvider::version_number() const
{
   return PQlibVersion();
}
//-----------------------------------------------------------------------------

