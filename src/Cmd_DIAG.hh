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

#ifndef __CMD_DIAG_HH_DEFINED__
#define __CMD_DIAG_HH_DEFINED__

#include "Common.hh"
#include "UCS_string.hh"
#include "Value.hh"

//════════════════════════════════════════════════════════════════════════════
/// The class implementing the diagnostic/debug commands
class Cmd_DIAG
{
public:
   /// a helper for finding sub-values with two parents
   struct val_val
      {
        /// the parent (0 unless \b this is a sub-value)
        const cValue * parent;

        /// the value (always valid)
        const cValue * child;

        /// compare function for Heapsort::sort()
        /// @param A     first val_val element
        /// @param B     second val_val element
        static bool greater(const val_val & A, const val_val & B, const void *)
           { return A.child > B.child; }

        /// compare function for Heapsort<val_val>::search<const cValue *>()
        /// @param key   the Value pointer to search for
        /// @param B     the val_val element to compare against
        static int compare(const cValue * key, const val_val * B, const void *)
           { return key - B->child; }
      };

   /// )CHECK: check workspace integrity (stale Value and IndexExpr objects, etc)
   /// @param out  output stream for check results
   /// @param arg  optional command argument string
   static void cmd_CHECK(ostream & out, const UCS_string & arg);

   /// ]DOXY: create doxygen-like documentation of the current workspace
   /// @param out   output stream for documentation
   /// @param args  optional arguments controlling output format
   static void cmd_DOXY(ostream & out, const UCS_string_vector & args);

   /// show or clear input history
   /// @param out  output stream for history listing
   /// @param arg  optional argument (e.g. "CLEAR")
   static void cmd_HISTORY(ostream & out, const UCS_string & arg);

   /// show or clear optimizarion counters
   /// @param out  output stream for optimization data
   /// @param lib  optional library or function name to filter output
   static void cmd_OPTIM(ostream & out, const UCS_string & lib);

   /// show performance counters
   /// @param out  output stream for performance statistics
   /// @param arg  optional argument to filter or reset counters
   static void cmd_PSTAT(ostream & out, const UCS_string & arg);

   /// ]STATUS: list the optional features of GNU APL, whether they are
   /// enabled, and if not, why not
   static void cmd_STATUS(ostream & out);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __CMD_DIAG_HH_DEFINED__
