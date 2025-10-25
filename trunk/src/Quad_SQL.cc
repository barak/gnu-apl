/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2014-2016  Elias Mårtenson

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

#include <string.h>
#include <typeinfo>

#include "Common.hh"

#include "sql/apl-sqlite.hh"
#include "sql/Connection.hh"
#include "sql/Provider.hh"

#include "Quad_SQL.hh"
#include "Security.hh"

// !!! declare providers before Quad_SQL::_fun !!!
static std::vector<Provider *> providers;
static std::vector<Connection *> connections;

Quad_SQL  Quad_SQL::fun;

#if apl_SQLITE3
# include "sql/SqliteResultValue.hh"
# include "sql/SqliteConnection.hh"
# include "sql/SqliteProvider.hh"
#endif

#if apl_POSTGRES
# include "sql/PostgresConnection.hh"
# include "sql/PostgresProvider.hh"
#endif

//----------------------------------------------------------------------------
int
Quad_SQL::function_name_to_int(const char * function_name)
{
  if (0 == strcmp(function_name, "list"))       return  0;
  if (0 == strcmp(function_name, "open"))       return  1;
  if (0 == strcmp(function_name, "close"))      return  2;
  if (0 == strcmp(function_name, "query"))      return  3;
  if (0 == strcmp(function_name, "update"))     return  4;
  if (0 == strcmp(function_name, "begin"))      return  5;
  if (0 == strcmp(function_name, "commit"))     return  6;
  if (0 == strcmp(function_name, "rollback"))   return  7;
  if (0 == strcmp(function_name, "tables"))     return  8;
  if (0 == strcmp(function_name, "columns"))    return  9;
  if (0 == strcmp(function_name, "version"))    return 10;
  if (0 == strcmp(function_name, "vstring"))    return 11;

  MORE_ERROR() << "Invalid sub-function name '" << function_name
               <<"'for ⎕SQL.\nNOTE: ⎕SQL.list ⍬ will show the valid "
                 "sub-function names.";
  return -1;   // invalid function name
}

//----------------------------------------------------------------------------
sAxis
Quad_SQL::subfun_to_axis(const UCS_string & name) const
{
const UTF8_string name_utf(name);
   return function_name_to_int(name_utf.c_str());
}

//----------------------------------------------------------------------------
void
Quad_SQL::close_all_connections()
{
   loop(c, connections.size())
      {
        if (connections[c])   delete connections[c];
        connections.clear();
        connections.push_back(0);   // for the next connection
      }
}
//----------------------------------------------------------------------------
static void
init_provider_map()
{
#if apl_SQLITE3
   {
     Provider * sqliteProvider = new SqliteProvider();
     Assert(sqliteProvider);
     providers.push_back(sqliteProvider);
   }
#elif cfg_USER_WANTS_SQLITE3
# warning "SQLite3 unavailable since ./configure could not detect it"
#endif

#if apl_POSTGRES
   {
Provider * postgresProvider = new PostgresProvider();
   Assert(postgresProvider);
   providers.push_back(postgresProvider);
   }
#elif cfg_USER_WANTS_POSTGRES
#  warning "PostgreSQL unavailable since ./configure could not detect it."
# if apl_POSTGRES
#  warning "The PostgreSQL library seems to be installed, but the header file(s) are missing"
# endif
#endif
}
//----------------------------------------------------------------------------
Quad_SQL::Quad_SQL()
   : QuadFunction(TOK_Quad_SQL)
{
   init_provider_map();
}
//----------------------------------------------------------------------------
Quad_SQL::~Quad_SQL()
{
   loop(p, providers.size())   delete providers[p];
}
//----------------------------------------------------------------------------
Value_P
Quad_SQL::list_functions(ostream & out, bool mapping)
{
const char * legend =
"   Legend: Fs - database file name (path)\n"
"           Ty - database type ('sqlite' or 'postgres')\n"
"           Db - database handle (small integer)\n"
"           Qs - SQL query string\n"
"           Pv - query parameters (APL values, to be bound to Qs)\n"
"           Ts - name of a table in the database\n"
"           Vi - DB provider version number (integer)\n"
"           Vs - DB provider version string\n"
"\n";

   if (mapping)
      {
         // ⎕SQL "": print the (axis-)number to function name mappings
         //
         out <<
"      With a small performance penalty, ⎕SQL also accepts the following\n"
"      sub-function names instead of sub-function numbers as axis argument:\n"
"\n" << legend <<
"       ⎕SQL[0] ''      ←→  ⎕SQL.list ''        ⍝ list ⎕SQL function names\n"
"       ⎕SQL[0] ⍬       ←→  ⎕SQL.list ⍬         ⍝ list ⎕SQL function numbers)\n"
"    Ty ⎕SQL[1] Fs      ←→  ⎕SQL.open Fs        ⍝ open the database file Fs\n"
"       ⎕SQL[2] Db      ←→  ⎕SQL.close Db       ⍝ close a database handle Db\n"
"    Qs ⎕SQL[3, Db]     ←→  ⎕SQL.query (Db B)   ⍝ SQL database query\n"
"    Qs ⎕SQL[4, Db] Pv  ←→  ⎕SQL.update         ⍝ SQL database update\n"
"       ⎕SQL[5] Db   Pv ←→  ⎕SQL.begin          ⍝ begin a transaction\n"
"       ⎕SQL[6] Db      ←→  ⎕SQL.commit         ⍝ end a transaction\n"
"       ⎕SQL[7] Db      ←→  ⎕SQL.rollback       ⍝ roll a transaction back\n"
"       ⎕SQL[8] Db      ←→  ⎕SQL.tables         ⍝ show all tables\n"
"    Db ⎕SQL[9] Ts      ←→  ⎕SQL.columns        ⍝ show the columns of a table\n"
"       ⎕SQL[10] Ty     ←→  ⎕SQL.version        ⍝ SQL provider version number\n"
"       ⎕SQL[11] Ty     ←→  ⎕SQL.vstring        ⍝ SQL provider version string\n"
;
      }
   else
      {
        out <<
"   Valid sub-function numbers for ⎕SQL...\n"
"\n" << legend <<
"           ⎕SQL[0] ''       ⍝ list the ⎕SQL function names\n"
"           ⎕SQL[0] ⍬        ⍝ list the ⎕SQL function numbers\n"
"   Db ← Ty ⎕SQL[1] Fs       ⍝ open database Fs & return a handle for it\n"
"           ⎕SQL[2] Db       ⍝ close database handle Db\n"
"        Qs ⎕SQL[3, Db] Pv   ⍝ send SQL query Qs\n"
"        Qs ⎕SQL[4, Db] Pv   ⍝ send SQL update Qs\n"
"           ⎕SQL[5] Db       ⍝ begin a transaction\n"
"           ⎕SQL[6] Db       ⍝ commit the current transaction\n"
"           ⎕SQL[7] Db       ⍝ roll the current transaction back\n"
"           ⎕SQL[8] Db       ⍝ list the tables in database Db\n"
"        Db ⎕SQL[9] Ts       ⍝ list the column names and types of table Tn\n"
"   Vi ←    ⎕SQL[10] Ty      ⍝ the provider version number for type Ty\n"
"   Vs ←    ⎕SQL[11] Ty      ⍝ the provider version string for type Ty\n"
;
      }

   return Str0(LOC);
}
//----------------------------------------------------------------------------
static int find_free_connection( void )
{
    for ( int i = 0 ; i < connections.size(); ++i)
        {
          if (connections[i] == 0)   return i;
        }

    connections.push_back(0);
    return connections.size() - 1;
}
//----------------------------------------------------------------------------
Value_P
Quad_SQL::open_database(Value_P A, Value_P B)
{
    if (!A->is_apl_char_vector() )
       {
         MORE_ERROR() << "A ⎕SQL[1] B: Illegal database type A";
         VALUE_ERROR;
      }

const UTF8_string type_utf(A->get_UCS_ravel());
   loop(p, providers.size())
       {
         if (!strcasecmp(providers[p]->get_provider_name(), type_utf.c_str()))
            {
              const APL_Integer connection_index = find_free_connection();
              connections[connection_index] = providers[p]->open_database(B);
              return IntScalar(connection_index, LOC);
            }
       }

UCS_string & more = MORE_ERROR();
   more << "⎕SQL: Unknown database provider type: " << type_utf.c_str() << "\n"
           "      Supported providers are:";
   loop(p, providers.size())
       {
         more << " " << providers[p]->get_provider_name();
       }

   DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
static void
throw_illegal_db_handle(int handle)
{
   if (handle == -1)  MORE_ERROR() << "⎕SQL: Illegal database handle type" ;
   else  MORE_ERROR() << "⎕SQL: Illegal database handle " << handle;
    DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
static Connection *
db_id_to_connection(int db_id)
{
   if (db_id < 0 || db_id >= connections.size() )
      {
        throw_illegal_db_handle(db_id);
      }

Connection * conn = connections[db_id];
    if (conn == 0 )
       {
         throw_illegal_db_handle(db_id);
       }

    return conn;
}
//----------------------------------------------------------------------------
static Connection *
value_to_db_id(Value_P B)
{
   if (!B->is_scalar())
      {
        MORE_ERROR() << "⎕SQL: non-scalar database handle";
        RANK_ERROR;
      }

   if (!B->is_int_scalar())
      {
        MORE_ERROR() << "⎕SQL: non-integer database handle";
        DOMAIN_ERROR;
      }


const APL_Integer db_id = B->get_cfirst().get_int_value();
    return db_id_to_connection(db_id);
 }
//----------------------------------------------------------------------------
static Token
close_database(Value_P B)
{
    if (!B->is_int_scalar())
       {
         MORE_ERROR() <<
         "Close database command requires database id as argument";
        DOMAIN_ERROR;
       }

int db_id = B->get_cfirst().get_int_value();
   if (db_id < 0 || db_id >= connections.size())
       {
         throw_illegal_db_handle(db_id);
        }

Connection *conn = connections[db_id];
    if (conn == 0)
      {
        throw_illegal_db_handle(db_id);
      }

    connections[db_id] = NULL;
    delete conn;

    return Token(TOK_APL_VALUE1, Str0(LOC));
}
//----------------------------------------------------------------------------
static Value_P
run_generic_one_query(ArgListBuilder * arg_list, Value_P B, int start,
                      int num_args, bool ignore_result)
{
    loop (i, num_args)
         {
           const Cell & cell = B->get_cravel(start + i);
           if (cell.is_integer_cell())
              {
                arg_list->append_long(cell.get_int_value(), i);
              }
           else if(cell.is_float_cell())
              {
                arg_list->append_double(cell.get_real_value(), i);
              }
           else
              {
                Value_P value = cell.to_value(LOC);
                if (value->get_shape().get_volume() == 0)
                   {
                     arg_list->append_null( i );
                   }
                else if(value->is_char_string())
                   {
                     arg_list->append_string(
                               UTF8_string(value->get_UCS_ravel()).c_str(),
                                             i);
                   }
               else {
                      MORE_ERROR() << "Illegal data type in argument "
                                   << i << " of arglist";
                      VALUE_ERROR;
                    }
             }
         }

    return arg_list->run_query(ignore_result);
}
//----------------------------------------------------------------------------
Value_P
Quad_SQL::run_generic(Connection * conn, Value_P A, Value_P B, bool query)
{
   if (!A->is_char_string())
      {
        MORE_ERROR() << "Illegal query argument type";
        DOMAIN_ERROR;
      }

    if (B->get_rank() > 2)
       {
         MORE_ERROR() << "⎕SQL: Bind params have illegal rank";
         RANK_ERROR;
       }

const string statement = conn->replace_bind_args(
                                   UTF8_string(A->get_UCS_ravel()).c_str());
ArgListBuilder * builder = query ? conn->make_prepared_query(statement)
                                 : conn->make_prepared_update(statement);

    if (B->get_rank() < 2)
       {
         const int num_args = B->element_count();
         Value_P Z = run_generic_one_query(builder, B, 0, num_args, false);
         delete builder;
         return Z;
       }

    if (B->get_rank() == 2)   // matrix B
       {
         if (const int rows = B->get_rows())
            {
              const int cols = B->get_cols();
              Assert_fatal(rows > 0);
              Value_P Z;
              loop (row, rows)
                   {
                    const bool not_last = row < rows - 1;
                    Z = run_generic_one_query(builder, B, row * cols,
                                              cols, not_last);
                    if (not_last)   builder->clear_args();
                   }
             delete builder;
             return Z;
            }
         else
            {
              delete builder;
              return Idx0(LOC);
           }
       }
}
//----------------------------------------------------------------------------
Value_P
Quad_SQL::run_query(Value_P A, Value_P X, Value_P B)
{
bool nested_B = false;
Connection * conn = param_to_db(X, B, nested_B);
   if (!nested_B)   return run_generic(conn, A, B, true);
Value_P B1 = B->get_cravel(1).get_pointer_value();
   return run_generic(conn, A, B1, /* query = */ true);
}
//----------------------------------------------------------------------------
Value_P
Quad_SQL::run_update(Value_P A, Value_P X, Value_P B)
{
bool nested_B = false;
Connection * conn = param_to_db(X, B, nested_B);
   if (!nested_B)   return run_generic(conn, A, B, /*  query = */false);
Value_P B1 = B->get_cravel(1).get_pointer_value();
   return run_generic(conn, A, B1, false);
}
//----------------------------------------------------------------------------
static Token
run_transaction_begin(Value_P B)
{
Connection *conn = value_to_db_id(B);
    conn->transaction_begin();
    return Token(TOK_APL_VALUE1, Idx0(LOC));
}
//----------------------------------------------------------------------------
static Token
run_transaction_commit(Value_P B)
{
    Connection *conn = value_to_db_id( B );
    conn->transaction_commit();
    return Token(TOK_APL_VALUE1, Idx0(LOC));
}
//----------------------------------------------------------------------------
static Token
run_transaction_rollback(Value_P B)
{
Connection * conn = value_to_db_id(B);
   conn->transaction_rollback();
   return Token(TOK_APL_VALUE1, Idx0(LOC));
}
//----------------------------------------------------------------------------
static Token
show_tables(Value_P B)
{
Connection * conn = value_to_db_id(B);
vector<string> tables;
   conn->fill_tables(tables);

Value_P value;
   if (tables.size() == 0 )
     {
       value = Idx0(LOC);
     }
   else
     {
       Shape shape(tables.size());
       value = Value_P(shape, LOC);
       for (vector<string>::iterator i = tables.begin();i != tables.end(); i++)
           {
             const UTF8_string utf(i->c_str());
             Value_P ZZ(utf, LOC);
             value->next_ravel_Pointer(ZZ.get());
           }
     }

   value->check_value( LOC );
   return Token( TOK_APL_VALUE1, value );
}
//----------------------------------------------------------------------------
Value_P 
Quad_SQL::get_version_number(const UCS_string & ucs_B)
{
const UTF8_string utf_B(ucs_B);
const char * provider_name = utf_B.c_str();
   loop(p, providers.size())
       {
         // ucs_B should match either the provider type or the provider name
         //
         const Provider * provider = providers[p];
         if (strcmp(provider_name, provider->get_provider_name()) == 0 ||
             strcmp(provider_name, provider->get_provider_type()) == 0)
            {
              Value_P Z(LOC);
              Z->next_ravel_Int(provider->version_number());
              Z->check_value(LOC);
              return Z;
            }
       }

UCS_string & more = MORE_ERROR();
   more << "Invalid SQL provider '" << ucs_B << "'. The known providers are:\n";
   loop(p, providers.size())
       {
         const Provider * provider = providers[p];
         more << "    " << provider->get_provider_name()
              << "  ("   << provider->get_provider_type() << ")\n";
       }
   DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Value_P 
Quad_SQL::get_version_string(const UCS_string & ucs_B)
{
const UTF8_string utf_B(ucs_B);
const char * provider_name = utf_B.c_str();

   // provider_name could be the provider name (e.g. SQLite) or the
   // providder type (e.g. sqlite)
   loop(p, providers.size())
       {
         // ucs_B should match either the provider type or the provider name
         //
         const Provider * provider = providers[p];
         if (strcmp(provider_name, provider->get_provider_name()) == 0 ||
             strcmp(provider_name, provider->get_provider_type()) == 0)
            {
              const UTF8_string utf(provider->version_string());
              const UCS_string ucs(utf);
              Value_P Z(ucs, LOC);
              Z->check_value(LOC);
              return Z;
            }
       }

UCS_string & more = MORE_ERROR();
   more << "Invalid SQL provider '" << ucs_B << "'. The known providers are:\n";
   loop(p, providers.size())
       {
         const Provider * provider = providers[p];
         more << "    " << provider->get_provider_name()
              << "  ("   << provider->get_provider_type() << ")\n";
       }
   DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Value_P
Quad_SQL::column_names(Value_P A, Value_P B)
{
Connection * conn = value_to_db_id(A);
vector<ColumnDescriptor> cols;

   if (!B->is_apl_char_vector())
      {
        MORE_ERROR() << "Illegal table name";
        DOMAIN_ERROR;
      }

string name(UTF8_string(B->get_UCS_ravel()).c_str());
   conn->fill_cols(name, cols);

   if (cols.size() == 0)   return Idx0(LOC);

Shape shape(cols.size(), 2);
Value_P Z(shape, LOC);

   for (vector<ColumnDescriptor>::iterator i = cols.begin();
        i != cols.end(); i++)
       {
         const UTF8_string utf(i->get_name().c_str());
         Value_P ZZ(utf, LOC);
         Z->next_ravel_Pointer(ZZ.get());

         if (i->get_type().size() == 0)
            {
              Value_P type = Str0(LOC);
              Z->next_ravel_Pointer(type.get());
            }
         else
            {
              const UTF8_string utf(i->get_type().c_str());
              Value_P ZZ(utf, LOC);
              Z->next_ravel_Pointer(ZZ.get());
            }
       }

    Z->check_value( LOC );
    return Z;
}
//----------------------------------------------------------------------------
Token
Quad_SQL::eval_B(Value_P B) const
{
   CHECK_SECURITY(disable_Quad_SQL);

   if (B->get_rank() > 1)         RANK_ERROR;
   if (B->element_count() != 0)   LENGTH_ERROR;

   if (B->get_cfirst().is_character_cell())
      return Token(TOK_APL_VALUE1, list_functions(COUT, true));

   if (B->get_cfirst().is_integer_cell())
      return Token(TOK_APL_VALUE1, list_functions(COUT, false));

   DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Token
Quad_SQL::eval_AB(Value_P A, Value_P B) const
{
   CHECK_SECURITY(disable_Quad_SQL);
   return Token(TOK_APL_VALUE1, list_functions(COUT, false));
}
//----------------------------------------------------------------------------
Token
Quad_SQL::eval_XB(Value_P X, Value_P B) const
{
   CHECK_SECURITY(disable_Quad_SQL);

const int function_number = X->get_cfirst().get_near_int( );
const bool map = B->get_cfirst().is_character_cell();

    switch(function_number)
       {
         case  0: return Token(TOK_APL_VALUE1, list_functions(CERR, map));
         case  2: return close_database(B);
         case  5: return run_transaction_begin(B);
         case  6: return run_transaction_commit(B);
         case  7: return run_transaction_rollback(B);
         case  8: return show_tables(B);
         case 10: return Token(TOK_APL_VALUE1,
                               get_version_number(UCS_string(*B)));
         case 11: return Token(TOK_APL_VALUE1,
                               get_version_string(UCS_string(*B)));

         default: MORE_ERROR() << "⎕SQL[X] B: Illegal function number X="
                               << function_number;
                  DOMAIN_ERROR;
       }
}
//----------------------------------------------------------------------------
Connection *
Quad_SQL::param_to_db(Value_P X, Value_P B, bool & nested_B)
{
const uRank rank_X = X->get_rank();
   if (rank_X > 1)
      {
        MORE_ERROR() << "⎕SQL[X]: Bad rank " << rank_X << " of axis X.";
        RANK_ERROR;
      }

const ShapeItem ec_X = X->element_count();
   if (ec_X == 2)   // [sub-function number, DB handle]; parameters in B
      {
        // ⎕SQL[function, DB-handle] and plain B
        //
        nested_B = false;
        const int DB_handle = X->get_cravel(1).get_near_int();
        return db_id_to_connection(DB_handle);
      }

   if (ec_X == 1)   // [sub-function name]; handle ↑B, parameters 1↓B
      {
        // ⎕SQL[function-handle] and nested B = [DB-handle] parameters]
        //
        nested_B = true;
        const uRank rank_B = B->get_rank();
        const int DB_handle = B->get_cravel(0).get_near_int();
        if (rank_B != 1)
           {
             MORE_ERROR() << "A ⎕SQL[X] B: Bad rank " << rank_B
                          << " of right argument B (expected: ⍴⍴B = 1).";
             RANK_ERROR;
           }

        const ShapeItem ec_B = B->element_count();
        if (ec_B != 2)
           {
             MORE_ERROR() << "A ⎕SQL[X] B: Bad length " << ec_B
                          << " of right argument B (expected: ⍴B = 2).";
             LENGTH_ERROR;
           }

        if (!B->get_cravel(1).is_pointer_cell())
           {
             MORE_ERROR() << "A ⎕SQL[X] B: 1↓B not nested "
                             "(expected: nested query parameters 1↓B).";
             DOMAIN_ERROR;
           }
        return db_id_to_connection(DB_handle);
      }

   if (ec_X == 0)
      {
        MORE_ERROR() << "⎕SQL[X]: Empty axis argument X.";
        LENGTH_ERROR;
      }

   MORE_ERROR() << "⎕SQL[X]: Axis argument X too long.";
   LENGTH_ERROR;
}
//----------------------------------------------------------------------------
Token
Quad_SQL::eval_AXB(const Value_P A, const Value_P X, const Value_P B) const
{
   CHECK_SECURITY(disable_Quad_SQL);

const APL_Integer function_number = X->get_cfirst().get_near_int();

   switch(function_number)
      {
        case 0: return Token(TOK_APL_VALUE1, list_functions(CERR, false));
        case 1: return Token(TOK_APL_VALUE1, open_database(A, B));
        case 3: return Token(TOK_APL_VALUE1, run_query(A, X, B));
        case 4: return Token(TOK_APL_VALUE1, run_update(A, X, B));
        case 9: return Token(TOK_APL_VALUE1, column_names(A, B));
      }

   MORE_ERROR() << "A ⎕SQL[X] B: Illegal function number X="
                << function_number;
   DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
