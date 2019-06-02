/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2014-2016  Elias Mårtenson

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

#include <string.h>
#include <typeinfo>
#include <vector>

#include "config.h"   // for HAVE_xxx macros

#include "apl-sqlite.hh"
#include "Connection.hh"
#include "Provider.hh"

#include "Quad_SQL.hh"
#include "Security.hh"

// !!! declare providers before Quad_SQL::_fun !!!
static std::vector<Provider *> providers;
static std::vector<Connection *> connections;

Quad_SQL  Quad_SQL::_fun;
Quad_SQL * Quad_SQL::fun = &Quad_SQL::_fun;

#ifdef HAVE_SQLITE3
# include "SqliteResultValue.hh"
# include "SqliteConnection.hh"
# include "SqliteProvider.hh"
#endif

#ifdef USABLE_PostgreSQL
# include "PostgresConnection.hh"
# include "PostgresProvider.hh"
#endif

//-----------------------------------------------------------------------------
inline void
init_provider_map()
{
#ifdef HAVE_SQLITE3
   Provider *sqliteProvider = new SqliteProvider();
   Assert(sqliteProvider);
   providers.push_back(sqliteProvider);
#elif REALLY_WANT_SQLITE3
# warning "SQLite3 unavailable since ./configure could not detect it"
#endif

#ifdef USABLE_PostgreSQL
   Provider *postgresProvider = new PostgresProvider();
   Assert(postgresProvider);
   providers.push_back(postgresProvider);
#elif REALLY_WANT_PostgreSQL
#  warning "PostgreSQL unavailable since ./configure could not detect it."
# if HAVE_POSTGRESQL
#  warning "The PostgreSQL library seems to be installed, but the header file(s) are missing"
# endif
#endif
}
//-----------------------------------------------------------------------------
Quad_SQL::Quad_SQL()
   : QuadFunction(TOK_Quad_SQL)
{
   init_provider_map();
}
//-----------------------------------------------------------------------------
Quad_SQL::~Quad_SQL()
{
   loop(p, providers.size())   delete providers[p];
}
//-----------------------------------------------------------------------------
static Token list_functions( ostream &out )
{
    out << "Available function numbers:" << endl
<< "type  ⎕SQL[1] file      - open a database file,"
                            " return reference ID for it" << endl
<< "      ⎕SQL[2] ref       - close database" << endl
<< "query ⎕SQL[3,db] params - send SQL query" << endl
<< "query ⎕SQL[4,db] params - send SQL update" << endl
<< "      ⎕SQL[5] ref       - begin a transaction" << endl
<< "      ⎕SQL[6] ref       - commit current transaction" << endl
<< "      ⎕SQL[7] ref       - rollback current transaction" << endl
<< "      ⎕SQL[8] ref       - list tables" << endl
<< "ref   ⎕SQL[9] table     - list columns for table" << endl;
    return Token(TOK_APL_VALUE1, Str0( LOC ) );
}
//-----------------------------------------------------------------------------
static int find_free_connection( void )
{
    for ( int i = 0 ; i < connections.size(); ++i)
        {
          if (connections[i] == 0)   return i;
        }

    connections.push_back(0);
    return connections.size() - 1;
}
//-----------------------------------------------------------------------------
Token open_database(Value_P A, Value_P B)
{
    if(!A->is_apl_char_vector() ){
        MORE_ERROR() << "Illegal database name";
        VALUE_ERROR;
    }

string type = to_string(A->get_UCS_ravel());
   loop(p, providers.size())
       {
         if (providers[p]->get_name() == type)
            {
              const int connection_index = find_free_connection();
              connections[connection_index] = providers[p]->open_database(B);
              return Token(TOK_APL_VALUE1, IntScalar(connection_index, LOC));
            }
       }

   MORE_ERROR() << "Unknown database type: " << type.c_str();
   VALUE_ERROR;
}
//-----------------------------------------------------------------------------
static void throw_illegal_db_id( void )
{
    MORE_ERROR() << "Illegal database id";
    DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
static Connection *
db_id_to_connection(int db_id)
{
   if (db_id < 0 || db_id >= connections.size() )
      {
        throw_illegal_db_id();
      }

Connection * conn = connections[db_id];
    if (conn == 0 )
       {
         throw_illegal_db_id();
       }

    return conn;
}
//-----------------------------------------------------------------------------
static Connection *value_to_db_id( Value_P value )
{
    if( !value->is_int_scalar( ) ) {
        throw_illegal_db_id();
    }

    int db_id = value->get_ravel( 0 ).get_int_value();
    return db_id_to_connection( db_id );
 }
//-----------------------------------------------------------------------------
static Token close_database( Value_P B )
{
    if (!B->is_int_scalar( ) ) {
        MORE_ERROR() << "Close database command requires database id as argument";
        DOMAIN_ERROR;
    }

    int db_id = B->get_ravel(0).get_int_value();
    if (db_id < 0 || db_id >= connections.size())
       {
         throw_illegal_db_id();
        }

Connection *conn = connections[db_id];
    if(conn == 0)
      {
        throw_illegal_db_id();
      }

    connections[db_id] = NULL;
    delete conn;

    return Token( TOK_APL_VALUE1, Str0( LOC ) );
}
//-----------------------------------------------------------------------------
static Value_P
run_generic_one_query(ArgListBuilder *arg_list, Value_P B, int start,
                      int num_args, bool ignore_result )
{
    for( int i = 0 ; i < num_args ; i++ ) {
        const Cell &cell = B->get_ravel( start + i );
        if( cell.is_integer_cell() ) {
            arg_list->append_long( cell.get_int_value(), i );
        }
        else if( cell.is_float_cell() ) {
            arg_list->append_double( cell.get_real_value(), i );
        }
        else {
            Value_P value = cell.to_value( LOC );
            if( value->get_shape().get_volume() == 0 ) {
                arg_list->append_null( i );
            }
            else if( value->is_char_string() ) {
                arg_list->append_string( to_string( value->get_UCS_ravel() ), i );
            }
            else {
                MORE_ERROR() << "Illegal data type in argument "
                             << i << " of arglist";
                VALUE_ERROR;
            }
        }
    }

    return arg_list->run_query( ignore_result );
}
//-----------------------------------------------------------------------------
static Value_P
run_generic(Connection *conn, Value_P A, Value_P B, bool query)
{
   if (!A->is_char_string())
      {
        MORE_ERROR() << "Illegal query argument type";
        VALUE_ERROR;
      }

string statement = conn->replace_bind_args(to_string( A->get_UCS_ravel()));
ArgListBuilder * builder;
    if (query)   builder = conn->make_prepared_query( statement );
    else         builder = conn->make_prepared_update( statement );

const Shape &shape = B->get_shape();
    if (shape.get_rank() == 0 || shape.get_rank() == 1) {
        int num_args = shape.get_volume();
        Value_P result = run_generic_one_query( builder, B, 0, num_args, false);
        delete builder;
        return result;
    }

    if (shape.get_rank() == 2) {
        int rows = shape.get_rows();
        int cols = shape.get_cols();
        if( rows == 0 ) {
            delete builder;
            return Idx0( LOC );
        }
        else {
            Assert_fatal( rows > 0 );
            Value_P result;
            loop (row, rows)
                 {
                   const bool not_last = row < rows - 1;
                   result = run_generic_one_query(builder, B, row * cols,
                                                  cols, not_last );
                   if (not_last)   builder->clear_args();
                 }
            delete builder;
            return result;
        }
    }

   delete builder;
   MORE_ERROR() << "Bind params have illegal rank";
   RANK_ERROR;
}
//-----------------------------------------------------------------------------
static Token run_query( Connection *conn, Value_P A, Value_P B )
{
    return Token( TOK_APL_VALUE1, run_generic( conn, A, B, true ) );
}
//-----------------------------------------------------------------------------
static Token run_update( Connection *conn, Value_P A, Value_P B )
{
    return Token( TOK_APL_VALUE1, run_generic( conn, A, B, false ) );
}
//-----------------------------------------------------------------------------
static Token run_transaction_begin( Value_P B )
{
    Connection *conn = value_to_db_id( B );
    conn->transaction_begin();
    return Token( TOK_APL_VALUE1, Idx0( LOC ) );
}
//-----------------------------------------------------------------------------
static Token run_transaction_commit( Value_P B )
{
    Connection *conn = value_to_db_id( B );
    conn->transaction_commit();
    return Token( TOK_APL_VALUE1, Idx0( LOC ) );
}
//-----------------------------------------------------------------------------
static Token run_transaction_rollback( Value_P B )
{
    Connection *conn = value_to_db_id( B );
    conn->transaction_rollback();
    return Token( TOK_APL_VALUE1, Idx0( LOC ) );
}
//-----------------------------------------------------------------------------
static Token show_tables( Value_P B )
{
    Connection *conn = value_to_db_id( B );
    vector<string> tables;
    conn->fill_tables( tables );

    Value_P value;
    if( tables.size() == 0 ) {
        value = Idx0( LOC );
    }
    else {
        Shape shape( tables.size () );
        value = Value_P( shape, LOC );
        for( vector<string>::iterator i = tables.begin() ; i != tables.end() ; i++ ) {
            new (value->next_ravel()) PointerCell( make_string_cell( *i, LOC ).get(),
               value.getref() );
        }
    }

    value->check_value( LOC );
    return Token( TOK_APL_VALUE1, value );
}
//-----------------------------------------------------------------------------
static Token
show_cols(Value_P A, Value_P B)
{
Connection * conn = value_to_db_id(A);
vector<ColumnDescriptor> cols;

    if( !B->is_apl_char_vector() ) {
        MORE_ERROR() << "Illegal table name";
        VALUE_ERROR;
    }

string name = to_string(B->get_UCS_ravel());
    conn->fill_cols(name, cols);

Value_P value;
    if( cols.size() == 0 ) {
        value = Idx0( LOC );
    }
    else {
        Shape shape(cols.size(), 2);
        value = Value_P(shape, LOC);
        for( vector<ColumnDescriptor>::iterator i = cols.begin() ; i != cols.end() ; i++ ) {
            new (value->next_ravel())
                PointerCell(make_string_cell(i->get_name(), LOC ).get(),
                            value.getref());

            Value_P type;
            if( i->get_type().size() == 0 ) {
                type = Str0( LOC );
            }
            else {
                type = make_string_cell( i->get_type(), LOC );
            }
            new (value->next_ravel()) PointerCell( type.get(), value.getref() );
        }
    }

    value->check_value( LOC );
    return Token( TOK_APL_VALUE1, value );
}
//-----------------------------------------------------------------------------
Token
Quad_SQL::eval_B(Value_P B)
{
   CHECK_SECURITY(disable_Quad_SQL);
   return list_functions(COUT);
}
//-----------------------------------------------------------------------------
Token
Quad_SQL::eval_AB( Value_P A, Value_P B )
{
   CHECK_SECURITY(disable_Quad_SQL);
   return list_functions(COUT);
}
//-----------------------------------------------------------------------------
Token
Quad_SQL::eval_XB(Value_P X, Value_P B)
{
   CHECK_SECURITY(disable_Quad_SQL);

const int function_number = X->get_ravel( 0 ).get_near_int( );

    switch( function_number ) {
    case 0:
        return list_functions( CERR );

    case 2:
        return close_database( B );

    case 5:
        return run_transaction_begin( B );

    case 6:
        return run_transaction_commit( B );

    case 7:
        return run_transaction_rollback( B );

    case 8:
        return show_tables( B );

    default:
        MORE_ERROR() << "Illegal function number";
        DOMAIN_ERROR;
    }
}
//-----------------------------------------------------------------------------
static Connection *param_to_db( Value_P X )
{
    const Shape &shape = X->get_shape();
    if( shape.get_volume() != 2 ) {
        MORE_ERROR() << "Database id missing from axis parameter";
        RANK_ERROR;
    }
    return db_id_to_connection( X->get_ravel( 1 ).get_near_int( ) );
}
//-----------------------------------------------------------------------------
Token
Quad_SQL::eval_AXB(const Value_P A, const Value_P X, const Value_P B)
{
   CHECK_SECURITY(disable_Quad_SQL);

    const int function_number = X->get_ravel( 0 ).get_near_int( );

    switch( function_number ) {
    case 0:
        return list_functions( CERR );

    case 1:
        return open_database( A, B );

    case 3:
        return run_query( param_to_db( X ), A, B );

    case 4:
        return run_update( param_to_db( X ), A, B );

    case 9:
        return show_cols( A, B );

    default:
        MORE_ERROR() << "Illegal function number";
        DOMAIN_ERROR;
    }
}
//-----------------------------------------------------------------------------
static const UCS_string
ucs_string_from_string( const std::string &string )
{
const size_t length = string.size();
const char * buf = string.c_str();
UTF8_string utf(utf8P(buf), length);
    return UCS_string(utf);
}
//-----------------------------------------------------------------------------
Value_P
make_string_cell( const std::string &string, const char *loc )
{
    UCS_string s = ucs_string_from_string( string );
    Shape shape( s.size() );
    Value_P cell( shape, loc );
    for( int i = 0 ; i < s.size() ; i++ ) {
        new (cell->next_ravel()) CharCell( s[i] );
    }
    cell->check_value( loc );
    return cell;
}
//-----------------------------------------------------------------------------
