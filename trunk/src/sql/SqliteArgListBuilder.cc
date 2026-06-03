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

#include "SqliteArgListBuilder.hh"

#include <string.h>
#include "SqliteResultValue.hh"

//────────────────────────────────────────────────────────────────────────────
void
SqliteArgListBuilder::init_sql()
{
const char * sql_charptr = sql.c_str();
   if (sqlite3_prepare_v2(connection->get_db(),
                          sql_charptr, strlen(sql_charptr) + 1,
                          &statement, NULL) == SQLITE_OK)   return;

   connection->raise_sqlite_error( "Error preparing query" );
}
//────────────────────────────────────────────────────────────────────────────
SqliteArgListBuilder::SqliteArgListBuilder(SqliteConnection * connection_in,
                                           const string &sql_in)
   : sql(sql_in), connection(connection_in)
{
    init_sql();
}
//────────────────────────────────────────────────────────────────────────────
SqliteArgListBuilder::~SqliteArgListBuilder()
{
    sqlite3_finalize( statement );
}
//────────────────────────────────────────────────────────────────────────────
void SqliteArgListBuilder::clear_args( void )
{
    sqlite3_finalize( statement );
    init_sql();
}
//────────────────────────────────────────────────────────────────────────────
static void
free_text_arg( void *arg )
{
    free(arg);
}
//────────────────────────────────────────────────────────────────────────────
void
SqliteArgListBuilder::append_string( const string &arg, int pos )
{
    char *text = strdup( arg.c_str() );
    if( text == NULL ) {
        CERR << "Failed to allocate memory for bind arg" << endl;
        abort();
    }
    sqlite3_bind_text( statement, pos + 1, text, -1, free_text_arg );
}
//────────────────────────────────────────────────────────────────────────────
void
SqliteArgListBuilder::append_long( long arg, int pos )
{
    sqlite3_bind_int64( statement, pos + 1, arg );
}
//────────────────────────────────────────────────────────────────────────────
void
SqliteArgListBuilder::append_double( double arg, int pos )
{
    sqlite3_bind_double( statement, pos + 1, arg );
}
//────────────────────────────────────────────────────────────────────────────
void
SqliteArgListBuilder::append_null(int pos)
{
    sqlite3_bind_null(statement, pos + 1);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
SqliteArgListBuilder::run_query()
{
vector<ResultRow> result_rows;

   for (;;)
      {
        const int result = sqlite3_step(statement);
        if (result == SQLITE_DONE)   break;
        if (result != SQLITE_ROW)
           {
             connection->raise_sqlite_error("Error reading sql result");
           }

        ResultRow row;
        row.add_values(statement);
        result_rows.push_back(row);
      }

    if (const int row_count = result_rows.size())
       {
         const ShapeItem col_count = result_rows[0].get_values().size();
         const Shape shape_Z(row_count, col_count);
         Value_P Z = Value_P(shape_Z, LOC);
         loop(r, row_count)
         loop(c, col_count)
             {
               (result_rows[r].get_values())[c]->update(*Z);
             }
         Z->check_value(LOC);
         return Z;
       }
    else
       {
         return Idx0(LOC);
       }
}
//────────────────────────────────────────────────────────────────────────────
