/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2026  Dr. Jürgen Sauermann

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

#ifndef __Quad_SQL_HH_DEFINED__
#define __Quad_SQL_HH_DEFINED__

#include "QuadFunction.hh"
#include "Value.hh"

class ArgListBuilder;
class Connection;
class Provider;   // SQL provider (SQLite, PostgreSQL)

//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕SQL
 */
/// The class implementing ⎕SQL
class Quad_SQL : public QuadFunction
{
public:
   /// Constructor.
   Quad_SQL();

   /// Destructor
   ~Quad_SQL();

   static Quad_SQL fun;          ///< Built-in function.

   /// close all open connections
   static void close_all_connections();

   /// run a (read-only) database query
   /// @param A left-argument APL value (SQL statement or parameters)
   /// @param X axis argument identifying the database connection
   /// @param B right-argument APL value (query parameters)
   static Value_P run_query(const Value & A, const Value & X, const Value & B)
      { return run_generic(param_to_db(X), A, B, /*  query = */ true); }


   /// run a (read/write) database update
   /// @param A left-argument APL value (SQL statement or parameters)
   /// @param X axis argument identifying the database connection
   /// @param B right-argument APL value (update parameters)
   static Value_P run_update(const Value & A, const Value & X, const Value & B)
      { return run_generic(param_to_db(X), A, B, /*  query = */ false); }


protected:
   /// a mapping between function names and function numbers
   static const FunctionGroup::function_info subfunction_infos[];

   /// overloaded Function::eval_AB().
   /// @param A left-argument APL value (SQL command or options)
   /// @param B right-argument APL value (query parameters or data)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_AXB().
   /// @param A left-argument APL value (SQL command or options)
   /// @param X axis argument identifying the database connection
   /// @param B right-argument APL value (query parameters or data)
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_B().
   /// @param B right-argument APL value (SQL sub-function selector)
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_XB().
   /// @param X axis argument identifying the database connection
   /// @param B right-argument APL value (SQL sub-function selector)
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// overloaded FunctionGroup::print_map_syntax()
   /// @param out output stream to write to
   /// @param info descriptor for the sub-function being printed
   virtual void print_map_syntax(ostream & out,
                                 const function_info & info) const;

   /// list ⎕SQL functions
   /// @param out output stream to write the function list to
   Token list_functions(ostream & out) const;

   /// overloaded FunctionGroup::get_legend()
   /// @param lt legend type selector
   virtual const char * get_legend(Legend_type lt) const;

   /// open a database, return its handle
   /// @param A left-argument APL value (connection options)
   /// @param B right-argument APL value (database filename or URL)
   static Value_P open_database(const Value & A, const Value & B);

   /// return the names of the database columns
   /// @param A left-argument APL value identifying the table
   /// @param B right-argument APL value (database handle)
   static Value_P column_names(Value_P A, Value_P B);

   /// perform a generic query
   /// @param conn active database connection
   /// @param A left-argument APL value (SQL statement)
   /// @param B right-argument APL value (bind parameters)
   /// @param query true for read-only query, false for read/write update
   static Value_P run_generic(Connection * conn, const Value & A,
                              const Value & B, bool query);

   /// return the version number of the SQL provider named B
   /// @param ucs_B provider name string
   static Value_P get_version_number(const UCS_string & ucs_B);

   /// return the version string of the SQL provider (synthesized as needed).
   /// @param ucs_B provider name string
   static Value_P get_version_string(const UCS_string & ucs_B);

   /// run one SQL query
   /// @param arg_list prepared argument list builder for the query
   /// @param B right-argument APL value (bind parameters)
   /// @param start first parameter index to bind
   /// @param num_args number of parameters to bind
   static Value_P run_generic_one_query(ArgListBuilder * arg_list,
                                        const Value & B, int start,
                                        int num_args);

   /// @param filename database file path to search for
   static int find_free_connection(const UTF8_string & filename);

   /// @param handle illegal database handle value to report
   static void throw_illegal_db_handle(int handle);

   /// @param db_id numeric database connection identifier
   static Connection * db_id_to_connection(int db_id);

   /// @param B APL value encoding the database connection handle
   static Connection * value_to_db_id(const Value & B);

   /// @param B APL value identifying the database connection to close
   static Token close_database(Value_P B);

   /// @param B APL value identifying the database connection
   static Token run_transaction_begin(Value_P B);

   /// @param B APL value identifying the database connection
   static Token run_transaction_commit(Value_P B);

   /// @param B APL value identifying the database connection
   static Token run_transaction_rollback(Value_P B);

   /// @param B APL value identifying the database connection
   static Token show_tables(Value_P B);

   /// convert axis argument X to a database connection. X may have length 2
   /// (for function number and database handle) or length 1 (function number
   /// only with B nested and ↑B being the database handle).
   ///
   /// @param X axis argument encoding the function number and optional DB handle
   static Connection * param_to_db(const Value & X);

   /// init a table of supported provides (as detected by ./configure)
   static void init_provider_map();

   /// supported providers
   static std::vector<Provider *> SQL_providers;

   /// a connection and its file name
   struct conn_file
      {
        Connection * connection;
        UTF8_string  filename;
      };
   static std::vector<conn_file> SQL_connections;
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕SQL_3.  It is a wrapper for ⎕SQL[3, DB]
   with a simpler axis
 */
//────────────────────────────────────────────────────────────────────────────

class Quad_SQL_3 : public QuadFunction
{
public:
   /// constructor
   Quad_SQL_3();

   static Quad_SQL_3  fun;        ///< Built-in function.

protected:
   /// overloaded Quad_SQL::eval_AXB().
   /// @param A left-argument APL value (SQL statement or options)
   /// @param X axis argument identifying the database connection
   /// @param B right-argument APL value (query parameters)
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;
};

//────────────────────────────────────────────────────────────────────────────

class Quad_SQL_4 : public QuadFunction
{
public:
   /// constructor
   Quad_SQL_4();

   static Quad_SQL_4  fun;        ///< Built-in function.

protected:
   /// overloaded Quad_SQL::eval_AXB().
   /// @param A left-argument APL value (SQL statement or options)
   /// @param X axis argument identifying the database connection
   /// @param B right-argument APL value (query parameters)
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;
};

#endif // __Quad_SQL_HH_DEFINED__


