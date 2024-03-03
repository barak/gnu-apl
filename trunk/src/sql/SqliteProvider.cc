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

#include "SqliteProvider.hh"
#include "SqliteConnection.hh"

//-----------------------------------------------------------------------------
SqliteProvider::~SqliteProvider()
{
}
//-----------------------------------------------------------------------------
static SqliteConnection *
create_sqlite_connection(Value_P B)
{
    if (!B->is_char_string())
       {
         MORE_ERROR() << "SQLite database connect argument "
                         "must be a simple string (filename)";
         DOMAIN_ERROR;
       }

const UTF8_string filename(B->get_UCS_ravel());

sqlite3 * db;
    if (sqlite3_open(filename.c_str(), &db) == SQLITE_OK)
       {
         SqliteConnection * conn = new SqliteConnection(db);
//       char ATTACH[200];
//       snprintf(ATTACH, sizeof(ATTACH), "ATTACH DATABASE '%s' AS '%s';",
//                filename.c_str(), "DUMMY");
//       conn->run_simple(ATTACH);
         return conn;
       }

   MORE_ERROR() << "Error opening database " << filename << ": "
                << sqlite3_errmsg(db);
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
Connection *
SqliteProvider::open_database(Value_P B)
{
Connection * conn = create_sqlite_connection(B);
    return conn;
}
//-----------------------------------------------------------------------------
const char *
SqliteProvider::version_string() const
{
   return "SQLite-" SQLITE_VERSION;
}
//-----------------------------------------------------------------------------
int
SqliteProvider::version_number() const
{
   return SQLITE_VERSION_NUMBER;
}
//-----------------------------------------------------------------------------
