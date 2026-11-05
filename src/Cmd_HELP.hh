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

#ifndef __CMD_HELP_HH_DEFINED__
#define __CMD_HELP_HH_DEFINED__

#include "Common.hh"
#include "UCS_string.hh"

//════════════════════════════════════════════════════════════════════════════
/// The class implementing the )HELP and ]HELP commands
class Cmd_HELP
{
public:
   /// show help for APL primitives, commands, or a user-defined name
   /// @param out  output stream for help text
   /// @param arg  optional topic or primitive to show help for
   static void cmd_HELP(ostream & out, const UCS_string & arg);

protected:
   /// )HELP: show help for APL primitives
   /// @param out    output stream for help text
   /// @param arg    the argument the user typed after )HELP
   /// @param arity  1 for monadic, 2 for dyadic, 0 for either
   /// @param prim   the primitive symbol as a C string
   /// @param name   the primitive name
   /// @param title  short title line
   /// @param descr  longer description text
   /// @param first  true initially; set false after the first Help.def
   ///               entry matching \b arg was printed, so that a blank
   ///               line can separate multiple forms (e.g. monadic and
   ///               dyadic) of the same primitive from each other
   static void primitive_help(ostream & out, const char * arg, int arity,
                              const char * prim, const char * name,
                              const char * title, const char * descr,
                              bool & first);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __CMD_HELP_HH_DEFINED__
