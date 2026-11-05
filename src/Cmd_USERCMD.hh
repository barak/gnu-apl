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

#ifndef __CMD_USERCMD_HH_DEFINED__
#define __CMD_USERCMD_HH_DEFINED__

#include "Common.hh"
#include "UCS_string.hh"

//════════════════════════════════════════════════════════════════════════════
/// The class implementing the ]USERCMD command
class Cmd_USERCMD
{
public:
   /// ]USERCMD: create a user defined command
   /// @param out   output stream for command result
   /// @param arg   the raw argument string (unsplit)
   /// @param args  the split argument tokens
   static void cmd_USERCMD(ostream & out, const UCS_string & arg,
                           UCS_string_vector & args);

   /// execute a user defined command
   /// @param out   output stream for command result
   /// @param line  the full input line (modified to APL function call)
   /// @param line1 the first token on the input line
   /// @param cmd   the matched user command prefix
   /// @param args  the remaining arguments after the command name
   /// @param uidx  index into the user_command table
   static void do_USERCMD(ostream & out, UCS_string & line,
                          const UCS_string & line1, const UCS_string & cmd,
                          UCS_string_vector & args, int uidx);

protected:
   /// check if a command name conflicts with an existing command
   /// @param out   output stream for conflict error message
   /// @param cnew  the new command name being defined
   /// @param cold  an existing command name to compare against
   static bool check_name_conflict(ostream & out, const UCS_string & cnew,
                                   const UCS_string cold);

   /// check if a command is being redefined
   /// @param out   output stream for redefinition warning
   /// @param cnew  the new command name
   /// @param fnew  the new APL function name implementing the command
   /// @param mnew  the new mode value for the command
   /// @return 0 for a new command, 1 for an identical redefinition, and 2
   ///         for a different (refused) redefinition
   static int check_redefinition(ostream & out, const UCS_string & cnew,
                                 const UCS_string fnew, const int mnew);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __CMD_USERCMD_HH_DEFINED__
