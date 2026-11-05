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

#ifndef __CMD_LISTING_HH_DEFINED__
#define __CMD_LISTING_HH_DEFINED__

#include "Common.hh"
#include "UCS_string.hh"

//════════════════════════════════════════════════════════════════════════════
/// The class implementing the workspace-introspection/listing commands
class Cmd_LISTING
{
public:
   /// )FNS: show list of functions
   /// @param out  output stream for function list
   /// @param arg  optional name range argument
   static void cmd_FNS(ostream & out, const UCS_string & arg);

   /// )NMS: show list of (Symbol-) names
   /// @param out  output stream for name list
   /// @param arg  optional name range argument
   static void cmd_NMS(ostream & out, const UCS_string & arg);

   /// )FNS: show list of operators
   /// @param out  output stream for operator list
   /// @param arg  optional name range argument
   static void cmd_OPS(ostream & out, const UCS_string & arg);

   /// )OWNERS: show list of all APL value owners
   /// @param out  output stream for owner list
   static void cmd_OWNERS(ostream & out);

   /// ]SVARS: display all shared variables
   /// @param out  output stream for shared variable list
   static void cmd_SVARS(ostream & out);

   /// ]SYMBOL: display the details of one symbol
   /// @param out  output stream for symbol details
   /// @param arg  name of the symbol to display
   static void cmd_SYMBOL(ostream & out, const UCS_string & arg);

   /// ]SYMBOLS: set or display the number of symbols
   /// @param out  output stream for symbol table info
   /// @param arg  optional new symbol table size
   static void cmd_SYMBOLS(ostream & out, const UCS_string & arg);

   /// )VALUES: show list of all APL values
   /// @param out  output stream for value list
   static void cmd_VALUES(ostream & out);

   /// )VARS: show list of variables
   /// @param out  output stream for variable list
   /// @param arg  optional name range argument
   static void cmd_VARS(ostream & out, const UCS_string & arg);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __CMD_LISTING_HH_DEFINED__
