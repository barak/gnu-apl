/*
        if (count == 0)   // no connections
           {
             new (&Z->get_wproto()) IntCell(0);
           }

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

#include "sql/Connection.hh"
#include "sql/Provider.hh"

#include "Quad_SQL.hh"
#include "Security.hh"

// !!! declare providers before Quad_SQL::_fun !!!
std::vector<Provider *> Quad_SQL::SQL_providers;
std::vector<Quad_SQL::conn_file> Quad_SQL::SQL_connections;

Quad_SQL Quad_SQL::fun;
Quad_SQL_3 Quad_SQL_3::fun;   // ⎕SQL[3, DB]
Quad_SQL_4 Quad_SQL_4::fun;   // ⎕SQL[4, DB]

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
const FunctionGroup::function_info Quad_SQL::subfunction_infos[] =
{
#define sql_def(N, name, map_comm, fun_comm) \
   { N, #name, map_comm, fun_comm, -1 },
   sql_def(  0, list     , "⎕SQL function names/numbers", "")
   sql_def(  1, open     , "open database file"         , "")
   sql_def(  2, close    , "close a database handle"    , "")
   sql_def(  3, query    , "SQL database query"         , "")
   sql_def(  4, update   , "SQL database update"        , "")
   sql_def(  5, begin    , "begin a transaction"        , "")
   sql_def(  6, commit   , "end a transaction"          , "")
   sql_def(  7, rollback , "roll a transaction back"    , "")
   sql_def(  8, tables   , "show all tables"            , "")
   sql_def(  9, columns  , "show the columns of a table", "")
   sql_def( 10, version  , "SQL provider version number", "")
   sql_def( 11, vstring  , "SQL provider version string", "")
};
//----------------------------------------------------------------------------
Quad_SQL::Quad_SQL()
   : QuadFunction(TOK_Quad_SQL)
{
enum { count = sizeof(subfunction_infos) / sizeof(*subfunction_infos) };
   init_function_group(subfunction_infos, count, "⎕SQL");

   init_provider_map();
}
//----------------------------------------------------------------------------
void
Quad_SQL::close_all_connections()
{
   loop(c, SQL_connections.size())
      {
        conn_file slot = SQL_connections[c];
        if (Connection * conn = slot.connection)
           {
             CERR << "*** WARNING: ⎕SQL handle " << c
                  << " (file " << slot.filename
                  << ") is still open. Closing it," << endl;
             delete conn;
           }
      }
   SQL_connections.clear();
}
//----------------------------------------------------------------------------
void
Quad_SQL::init_provider_map()
{
#if apl_SQLITE3
   {
     Provider * sqliteProvider = new SqliteProvider();
     Assert(sqliteProvider);
     SQL_providers.push_back(sqliteProvider);
   }
#elif cfg_USER_WANTS_SQLITE3
# warning "SQLite3 unavailable since ./configure could not detect it"
#endif

#if apl_POSTGRES
   {
Provider * postgresProvider = new PostgresProvider();
   Assert(postgresProvider);
   SQL_providers.push_back(postgresProvider);
   }
#elif cfg_USER_WANTS_POSTGRES
#  warning "PostgreSQL unavailable since ./configure could not detect it."
# if apl_POSTGRES
#  warning "The PostgreSQL library seems to be installed, but the header file(s) are missing"
# endif
#endif
}
//----------------------------------------------------------------------------
Quad_SQL::~Quad_SQL()
{
   loop(p, SQL_providers.size())   delete SQL_providers[p];
   SQL_providers.clear();
}
//----------------------------------------------------------------------------
const char * Quad_SQL::get_legend(Legend_type lt) const
{
   switch(lt)
      {
        default:             return "";
        case LET_FUN_PREFIX:
        case LET_MAP_PREFIX: return
"    ┌─── Legend ────────────────────────────────────────────────┐\n"
"    │    Db - database handle (small integer)                   │\n"
"    │    Fs - database file name (path)                         │\n"
"    │    Pv - query parameters (APL values, to be bound to Qs)  │\n"
"    │    Qs - SQL query string                                  │\n"
"    │    Ts - name of a table in the database (string)          │\n"
"    │    Ty - database type ('sqlite' or 'postgres')            │\n"
"    │    Vi - DB provider (library) version (integer)           │\n"
"    │    Vs - DB provider (library) version (string)            │\n"
"    └───────────────────────────────────────────────────────────┘\n"
"\n";
      }
}
//----------------------------------------------------------------------------
Token
Quad_SQL::list_functions(ostream & out) const
{
   out << "\n"
          "⎕SQL is a function group. It is comprized of "
                "the following (sub-)functions:\n"
          "\n"
       << get_legend(LET_FUN_PREFIX)
       << "    ⎕SQL ''   ⍝ display this list\n"
          "    ⎕SQL ⍬    ⍝ display syntax alternatives for ⎕SQL\n"
          "\n";

const char * funs[] =
{
   "        ⎕SQL[ 0] ''    ", "display the ⎕SQL (sub-)function names"      ,
   "        ⎕SQL[ 0] ⍬     ", "display the ⎕SQL (sub-)function numbers"    ,
   "Db ← Ty ⎕SQL[ 1] Fs    ", "open database Fs & return a handle for it"  ,
   "        ⎕SQL[ 2] Db    ", "close database handle Db"                   ,
   "     Qs ⎕SQL[ 3, Db] Pv", "perform SQL query Qs"                       ,
   "     Qs ⎕SQL[ 4, Db] Pv", "perform SQL update Qs"                      ,
   "        ⎕SQL[ 5] Db    ", "begin a transaction"                        ,
   "        ⎕SQL[ 6] Db    ", "commit the current transaction"             ,
   "        ⎕SQL[ 7] Db    ", "roll the current transaction back"          ,
   "Z  ←    ⎕SQL[ 8] Db    ", "the tables in database Db"                  ,
   "Z  ← Db ⎕SQL[ 9] Ts    ", "the column names and types of table Ts"     ,
   "Vi ←    ⎕SQL[10] Ty   ", "the provider version number for DB type Ty" ,
   "Vs ←    ⎕SQL[11] Ty   ", "the provider version string for DB type Ty" ,
   0                       , 0
 };

   for (const char ** f = funs; *f; )
       {
         out << " " << *f++;
         out << "   ⍝ " << *f++ << endl;
       }

   out << "\nThe functions of ⎕SQL"
          " can be called with one of several syntax alternatives.\n"
          "The syntax alternatives for ⎕SQL can be displayed with:\n\n";

   COUT << "      ⎕SQL ⍬   ⍝ display the syntax alternatives for ⎕SQL\n\n";

   return Token();
}
//----------------------------------------------------------------------------
void
Quad_SQL::print_map_syntax(ostream & out, const function_info & info) const
{
const sAxis axis = info.axis;
const UTF8_literal name = info.function_name;

   // print axis number syntax
   //
   out << "    ⎕SQL[" << axis;
   if (axis == 3 || axis == 4)   out << ", Db] Pv"; 
   else                          out << "]       ";

   // print member name syntax
   //
   out << "  ←→  ⎕SQL." << name;

   if (axis == 3 || axis == 4)   out << " Db Pv";
   while (Output::get_column() < 44)    out << UNI_SPACE;

   // comment
   out << "⍝ " << info.comment_map << endl;   // comment
}
//----------------------------------------------------------------------------
int
Quad_SQL::find_free_connection(const UTF8_string & filename)
{
   loop(h, SQL_connections.size())
       {
         conn_file & slot = SQL_connections[h];
         if (slot.connection == 0)   // free slot
            {
              slot.filename = filename;
              return h;              // re-use handle h
            }
       }

   // no free connection: append a new slot to SQL_connections
   //
   conn_file slot = { 0, filename };
   SQL_connections.push_back(slot);
   return SQL_connections.size() - 1;
}
//----------------------------------------------------------------------------
Value_P
Quad_SQL::open_database(const Value & A, const Value & B)
{
   // open an SQL database of type A in database file B
   //
    if (!A.is_apl_char_vector())
       {
         MORE_ERROR() <<
              "A ⎕SQL[1] B: Illegal database type A (string expected).";
         DOMAIN_ERROR;
      }

    if (!B.is_apl_char_vector() )
       {
         MORE_ERROR() <<
              "A ⎕SQL[1] B: Illegal database file name B (string expected).";
         DOMAIN_ERROR;
      }

const UTF8_string type_utf(A.get_UCS_ravel());
const UTF8_string file_utf(B.get_UCS_ravel());

   /// find a provider for type A and let iy open database file B.
   loop(p, SQL_providers.size())
       {
         Provider * provider = SQL_providers[p];
         if (!strcasecmp(provider->get_provider_name(), type_utf.c_str()))
            {
              const APL_Integer handle = find_free_connection(file_utf);
              SQL_connections[handle].connection = provider->open_database(B);
              return IntScalar(handle, LOC);
            }
       }

UCS_string & more = MORE_ERROR();
   more << "⎕SQL: Unknown database provider type: " << type_utf.c_str() << "\n"
           "      Supported providers are:";
   loop(p, SQL_providers.size())
       {
         more << " " << SQL_providers[p]->get_provider_name();
       }

   DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
void
Quad_SQL::throw_illegal_db_handle(int handle)
{
   if (handle == -1)  MORE_ERROR() << "⎕SQL: Illegal database handle type" ;
   else  MORE_ERROR() << "⎕SQL: Illegal database handle " << handle;
    DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Connection *
Quad_SQL::db_id_to_connection(int db_id)
{
   if (db_id < 0 || db_id >= SQL_connections.size())
      {
        throw_illegal_db_handle(db_id);
      }

   if (Connection * conn = SQL_connections[db_id].connection)   return conn;
   throw_illegal_db_handle(db_id);
}
//----------------------------------------------------------------------------
Connection *
Quad_SQL::Quad_SQL::value_to_db_id(const Value & B)
{
   if (!B.is_scalar())
      {
        MORE_ERROR() << "⎕SQL: non-scalar database handle";
        RANK_ERROR;
      }

   if (!B.is_int_scalar())
      {
        MORE_ERROR() << "⎕SQL: non-integer database handle";
        DOMAIN_ERROR;
      }

const APL_Integer db_id = B.get_cfirst().get_int_value();
    return db_id_to_connection(db_id);
 }
//----------------------------------------------------------------------------
Token
Quad_SQL::close_database(Value_P B)
{
    if (!B->is_int_scalar())
       {
         MORE_ERROR() <<
         "Close database command requires database id as argument";
        DOMAIN_ERROR;
       }

int db_id = B->get_cfirst().get_int_value();
   if (db_id < 0 || db_id >= SQL_connections.size())
       {
         throw_illegal_db_handle(db_id);
        }

   if (Connection * conn = SQL_connections[db_id].connection)
      {
        SQL_connections[db_id].connection = 0;  // mark slot as free
        delete conn;

        return Token(TOK_APL_VALUE1, Str0(LOC));
      }

   throw_illegal_db_handle(db_id);
}
//----------------------------------------------------------------------------
Value_P
Quad_SQL::run_generic_one_query(ArgListBuilder * arg_list, const Value & B,
                                int start, int num_args)
{
    loop (i, num_args)
         {
           const Cell & cell = B.get_cravel(start + i);
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

    return arg_list->run_query();
}
//----------------------------------------------------------------------------
Value_P
Quad_SQL::run_generic(Connection * conn, const Value & A, const Value & B,
                      bool query)
{
   // A is the SQL query string
   //
   if (!A.is_char_string())
      {
        MORE_ERROR() <<
       "A ⎕SQL B: Illegal query string A (string expected)";
        DOMAIN_ERROR;
      }

const sRank rank_B = B.get_rank();
    if (rank_B > 2)
       {
         MORE_ERROR() <<
         "A ⎕SQL B: Bind parameters B have illegal rank " << rank_B << " > 2";
         RANK_ERROR;
       }

const string statement = conn->replace_bind_args(
                                   UTF8_string(A.get_UCS_ravel()).c_str());
ArgListBuilder * builder = query ? conn->make_prepared_query(statement)
                                 : conn->make_prepared_update(statement);

    if (rank_B < 2)
       {
         const int num_args = B.element_count();
         Value_P Z = run_generic_one_query(builder, B, 0, num_args);
         delete builder;
         return Z;
       }

    if (rank_B == 2)   // matrix B
       {
         if (const int rows = B.get_rows())
            {
              const int cols = B.get_cols();
              Assert_fatal(rows > 0);
              Value_P Z;
              loop (row, rows)
                   {
                    const bool more = row < rows - 1;
                    Z = run_generic_one_query(builder, B, row * cols, cols);
                    if (more)   builder->clear_args();
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
Token
Quad_SQL::run_transaction_begin(Value_P B)
{
Connection * conn = value_to_db_id(*B);
    conn->transaction_begin();
    return Token(TOK_APL_VALUE1, Idx0(LOC));
}
//----------------------------------------------------------------------------
Token
Quad_SQL::run_transaction_commit(Value_P B)
{
Connection * conn = value_to_db_id(*B);
    conn->transaction_commit();
    return Token(TOK_APL_VALUE1, Idx0(LOC));
}
//----------------------------------------------------------------------------
Token
Quad_SQL::run_transaction_rollback(Value_P B)
{
Connection * conn = value_to_db_id(*B);
   conn->transaction_rollback();
   return Token(TOK_APL_VALUE1, Idx0(LOC));
}
//----------------------------------------------------------------------------
Token
Quad_SQL::show_tables(Value_P B)
{
Connection * conn = value_to_db_id(*B);
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
   return Token(TOK_APL_VALUE1, value);
}
//----------------------------------------------------------------------------
Value_P 
Quad_SQL::get_version_number(const UCS_string & ucs_B)
{
const UTF8_string utf_B(ucs_B);
const char * provider_name = utf_B.c_str();
   loop(p, SQL_providers.size())
       {
         // ucs_B should match either the provider type or the provider name
         //
         const Provider * provider = SQL_providers[p];
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
   loop(p, SQL_providers.size())
       {
         const Provider * provider = SQL_providers[p];
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
   loop(p, SQL_providers.size())
       {
         // ucs_B should match either the provider type or the provider name
         //
         const Provider * provider = SQL_providers[p];
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
   loop(p, SQL_providers.size())
       {
         const Provider * provider = SQL_providers[p];
         more << "    " << provider->get_provider_name()
              << "  ("   << provider->get_provider_type() << ")\n";
       }
   DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Value_P
Quad_SQL::column_names(Value_P A, Value_P B)
{
Connection * conn = value_to_db_id(*A);
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

   if (B->is_int_scalar() && B->get_cfirst().get_int_value() == 0)
      {
        // ⎕SQL 0 : show open handles
        //
        ShapeItem count = 0;
        loop(c, SQL_connections.size())
            {
              if (SQL_connections[c].connection)   ++count;

            }

        Value_P Z(count, 3, LOC);   // ⍴Z is count, 2
        if (count == 0)   // no connections
           {
             new (&Z->get_wproto()) IntCell(0);   // prototype
           }

        loop(c, count)
            {
              const conn_file slot = SQL_connections[c];
              if (const Connection * conn = slot.connection)
                 {
                   Z->next_ravel_Int(c);    // handle
                   const UTF8_string name(conn->get_provider_name());
                   const UTF8_string file(slot.filename);
                   Value_P Z2(name, LOC);            // the SQL provider
                   Z->next_ravel_Pointer(Z2.get());
                   Value_P Z3(file, LOC);            // the filename
                   Z->next_ravel_Pointer(Z3.get());
                 }
            }

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (B->is_zilde())   return list_mappings(CERR);
   if (B->is_str0())    return list_functions(CERR);
   if (B->element_count() != 0)   LENGTH_ERROR;
   DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Token
Quad_SQL::eval_AB(Value_P A, Value_P B) const
{
   CHECK_SECURITY(disable_Quad_SQL);
   return list_functions(COUT);
}
//----------------------------------------------------------------------------
Token
Quad_SQL::eval_XB(Value_P X, Value_P B) const
{
   CHECK_SECURITY(disable_Quad_SQL);

const sAxis subfunction = value_to_subfun(*X);

const bool map = B->get_cfirst().is_character_cell();

    switch(subfunction)
       {
         case  0: if (map)   return list_mappings(CERR);
                  else       return list_functions(CERR);
         case  1: VALENCE_ERROR;
         case  2: return close_database(B);
         case  3:
         case  4:
         case  5: return run_transaction_begin(B);
         case  6: return run_transaction_commit(B);
         case  7: return run_transaction_rollback(B);
         case  8: return show_tables(B);
         case  9:
         case 10: return Token(TOK_APL_VALUE1,
                               get_version_number(UCS_string(*B)));
         case 11: return Token(TOK_APL_VALUE1,
                               get_version_string(UCS_string(*B)));

         default: bad_subfun_number_ERROR(subfunction);
       }
}
//----------------------------------------------------------------------------
Connection *
Quad_SQL::param_to_db(const Value & X)
{

const Shape & shape = X.get_shape();
   if (shape.get_volume() != 2 )
      {
        MORE_ERROR() << "⎕SQL: Database id missing from axis parameter";
        RANK_ERROR;
      }

    return db_id_to_connection(X.get_cravel(1).get_near_int());
}
//----------------------------------------------------------------------------
Token
Quad_SQL::eval_AXB(const Value_P A, const Value_P X, const Value_P B) const
{
   CHECK_SECURITY(disable_Quad_SQL);

const APL_Integer function_number = X->get_cfirst().get_near_int();

   switch(function_number)
      {
        case  0: return list_functions(CERR);
        case  1: return Token(TOK_APL_VALUE1, open_database(*A, *B));
        case  2: VALENCE_ERROR;
        case  3: return Token(TOK_APL_VALUE1, run_query(*A, *X, *B));
        case  4: return Token(TOK_APL_VALUE1, run_update(*A, *X, *B));
        case  5:
        case  6:
        case  7:
        case  8: VALENCE_ERROR;
        case  9: return Token(TOK_APL_VALUE1, column_names(A, B));
        case 10: VALENCE_ERROR;
        case 11: VALENCE_ERROR;
      }

   MORE_ERROR() << "A ⎕SQL[X] B: Illegal function number X="
                << function_number;
   DOMAIN_ERROR;
}
//============================================================================
Quad_SQL_3::Quad_SQL_3()
   : QuadFunction(TOK_Quad_SQL_3)
{
}
//----------------------------------------------------------------------------
Token
Quad_SQL_3::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
   /// convert integer scalar X to (3, X) or (4, X)
const APL_Integer DB = X->get_cfirst().get_int_value();
const APL_Integer subfun = 3;

Value_P FUN_DB(2, LOC);
   FUN_DB->next_ravel_Int(subfun);
   FUN_DB->next_ravel_Int(DB);
   FUN_DB->check_value(LOC);
   return Token(TOK_APL_VALUE1, Quad_SQL::run_query(*A, *FUN_DB, *B));
}
//============================================================================
Quad_SQL_4::Quad_SQL_4()
   : QuadFunction(TOK_Quad_SQL_4)
{
}
//----------------------------------------------------------------------------
Token
Quad_SQL_4::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
   /// convert integer scalar X to (3, X) or (4, X)
const APL_Integer DB = X->get_cfirst().get_int_value();
const APL_Integer subfun = 4;

Value_P FUN_DB(2, LOC);
   FUN_DB->next_ravel_Int(subfun);
   FUN_DB->next_ravel_Int(DB);
   FUN_DB->check_value(LOC);
   return Token(TOK_APL_VALUE1, Quad_SQL::run_update(*A, *FUN_DB, *B));
}
//----------------------------------------------------------------------------
