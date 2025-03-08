/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

class Connection;

//----------------------------------------------------------------------------
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

   static Quad_SQL  fun;          ///< Built-in function.

   /// close all open connections
   static void close_all_connections();

protected:
   /// overloaded Function::eval_AB().
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_B().
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_XB().
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// list ⎕SQL functions
   static Value_P list_functions(ostream & out);

   /// open a database, return its handle
   static Value_P open_database(Value_P A, Value_P B);

   /// run a (read-only) database query
   static Value_P run_query(Connection * conn, Value_P A, Value_P B);

   /// run a (read/write) database update
   static Value_P run_update(Connection * conn, Value_P A, Value_P B);

   /// return the names of the database columns
   static Value_P column_names(Value_P A, Value_P B);

   /// perform a generic query
   static Value_P run_generic(Connection * conn, Value_P A, Value_P B,
                              bool query);

   /// return the version number of the SQL provider named B
   static Value_P get_version_number(const UCS_string & ucs_B);

   /// return the version string of the SQL provider (synthesized as needed).
   static Value_P get_version_string(const UCS_string & ucs_B);
};
//----------------------------------------------------------------------------

#endif // __Quad_SQL_HH_DEFINED__


