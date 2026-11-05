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

#ifndef __CMD_SI_HH_DEFINED__
#define __CMD_SI_HH_DEFINED__

#include "Common.hh"

//════════════════════════════════════════════════════════════════════════════
/// The class implementing the SI-stack commands
class Cmd_SI
{
public:
   /// )SI: display the )SI stack
   /// @param out  output stream for SI stack display
   /// @param dbg  true to include debug information
   static void cmd_SI(ostream & out, bool dbg);

   /// )SIC: clear the )SI stack
   /// @param out  output stream for command result
   static void cmd_SIC(ostream & out);

   /// )SINL: display the )SI stack with name list
   /// @param out  output stream for SI stack with name list
   static void cmd_SINL(ostream & out);

   /// )SIS: display the )SI stack with statements
   /// @param out  output stream for SI stack with statements
   /// @param dbg  true to include debug information
   static void cmd_SIS(ostream & out, bool dbg);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __CMD_SI_HH_DEFINED__
