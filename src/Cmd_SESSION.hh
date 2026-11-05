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

#ifndef __CMD_SESSION_HH_DEFINED__
#define __CMD_SESSION_HH_DEFINED__

#include "Common.hh"
#include "Logging.hh"
#include "UCS_string.hh"

//════════════════════════════════════════════════════════════════════════════
/// The class implementing the session/UI-setting commands
class Cmd_SESSION
{
public:
   /// ]AV: print ⎕AV character table
   /// @param out  output stream for command result
   static void cmd_AV(ostream & out);

   /// )BOXING: select output format for APL values
   /// @param out  output stream for command result
   /// @param arg  boxing format argument string
   static void cmd_BOXING(ostream & out, const UCS_string & arg);

   /// ]EXPECT: set the number of expected errors (in testcase files)
   /// @param out  output stream for command result
   /// @param arg  expected error count as a string
   static void cmd_EXPECT(ostream & out, const UCS_string & arg);

   /// control logging facilities
   /// @param out  output stream for command result
   /// @param arg  logging facility name or number and on/off flag
   static void cmd_LOG(ostream & out, const UCS_string & arg);

   /// print more error info
   /// @param out   output stream for error details
   /// @param args  optional arguments
   static void cmd_MORE(ostream & out, const UCS_string_vector & args);

   /// ]XTERM: enable and disable colors
   /// @param out   output stream for command result
   /// @param args  "ON", "OFF", or empty to toggle color support
   static void cmd_XTERM(ostream & out, const UCS_string & args);

protected:
   /// On, Off, or Toggle
   enum OOT
      {
        Off    = 0,   ///< Off
        On     = 1,   ///< On
        Toggle = 2,   ///< Toggle
      };

   /// a logging ID and an action (On/Off/Toggle) to be performed with it
   struct lid_OOT
      {
        LogId lid;           ///< the logging ID
        OOT on_off_toggle;   ///< turn lid On, Off, or Toggle it
      };

   /// parse the argument of the ]LOG command and set logging accordingly
   /// @param args  the logging control argument string
   static void log_control(const UCS_string & args);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __CMD_SESSION_HH_DEFINED__
