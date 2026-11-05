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

#ifndef __CMD_WS_HH_DEFINED__
#define __CMD_WS_HH_DEFINED__

#include "Common.hh"
#include "UCS_string.hh"
#include "UTF8_string.hh"

//════════════════════════════════════════════════════════════════════════════
/// The class implementing the workspace-lifecycle commands
class Cmd_WS
{
public:
   /// )CHECK_WS: check the on-disk checksum(s) of a workspace file
   /// (.apl and/or .xml) without loading it
   /// @param out   output stream for command result
   /// @param args  optional library reference and workspace name
   static void cmd_CHECK_WS(ostream & out, const UCS_string_vector & args);

   /// )CLEAR: clear the current Workspace
   /// @param out  output stream for command result
   static void cmd_CLEAR(ostream & out);

   /// )SAVE the current  WS as CONTINUE and then )OFF
   /// @param out  output stream for command result
   static void cmd_CONTINUE(ostream & out);

   /// )COPY: copy a workspace file
   /// @param out         output stream for command result
   /// @param args        workspace name and optional object names
   /// @param protection  true to protect existing objects from being overwritten
   static void cmd_COPY(ostream & out, UCS_string_vector & args,
                        bool protection);

   /// )COPY_ONCE: copy a workspace file, nut at most once
   /// @param out   output stream for command result
   /// @param args  workspace name and optional object names
   static void cmd_COPY_ONCE(ostream & out, UCS_string_vector & args);

   /// )DROP: delete a workspace file
   /// @param out   output stream for command result
   /// @param args  library reference and workspace name
   static void cmd_DROP(ostream & out, const UCS_string_vector & args);

   /// )DUMP: dump a workspace file (.apl)
   /// @param out     output stream for command result
   /// @param args    workspace name and optional object names
   /// @param html    true to generate HTML output
   /// @param silent  true to suppress informational messages
   static void cmd_DUMP(ostream & out, const UCS_string_vector & args,
                        bool html, bool silent);

   /// )ERASE: erase symbols
   /// @param out   output stream for command result
   /// @param args  names of symbols to erase
   static void cmd_ERASE(ostream & out, const UCS_string_vector & args);

   /// )LOAD: load a workspace file
   /// @param out      output stream for command result
   /// @param args     library reference and workspace name
   /// @param quad_lx  receives the ⎕LX expression from the loaded workspace
   /// @param silent   true to suppress informational messages
   static void cmd_LOAD(ostream & out, const UCS_string_vector & args,
                        UCS_string & quad_lx, bool silent);

   /// )OUT: export a workspace file in .atf format
   /// @param out   output stream for command result
   /// @param args  workspace name and optional object names
   static void cmd_OUT(ostream & out, UCS_string_vector & args);

   /// )SAVE: save a workspace file (.xml)
   /// @param out   output stream for command result
   /// @param args  optional library reference and workspace name
   static void cmd_SAVE(ostream & out, const UCS_string_vector & args);

   /// )WSID: display or change the workspace name
   /// @param out   output stream for command result
   /// @param args  optional new workspace name
   static void cmd_WSID(ostream & out, const UCS_string_vector & args);

protected:
   /// )CHECK_WS without an argument: check every .apl/.xml workspace file
   /// in every present library reference directory (0..9, as per )LIBS).
   /// @param out   output stream for command result
   static void check_all_WS(ostream & out);

   /// check the on-disk checksum of one workspace file, already known to
   /// exist, and already known to be .apl or .xml from its own extension.
   /// Shared by cmd_CHECK_WS() (explicit name) and check_all_WS() (library
   /// directory scan).
   /// @param out       output stream for command result
   /// @param filename  full path of an existing .apl or .xml file
   /// @param is_xml    true for a .xml file, false for a .apl file
   static void check_WS_file(ostream & out, const UTF8_string & filename,
                             bool is_xml);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __CMD_WS_HH_DEFINED__
