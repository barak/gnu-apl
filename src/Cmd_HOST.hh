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

#ifndef __CMD_HOST_HH_DEFINED__
#define __CMD_HOST_HH_DEFINED__

#include "Common.hh"
#include "UCS_string.hh"

//════════════════════════════════════════════════════════════════════════════
/// The class implementing host/file interaction commands
class Cmd_HOST
{
public:
   /// )HOST: execute OS command
   /// @param out  output stream for command output
   /// @param arg  the OS command string to execute
   static void cmd_HOST(ostream & out, const UCS_string & arg);

   /// continue with (jump to) next input file.
   /// @param out   output stream for command result
   /// @param args  optional capability requirements for the next file
   static void cmd_NEXTFILE(ostream & out, const UCS_string_vector & args);

   /// ]FILE_CTL: set IO_Files::skip_mode, controlling which subsequent
   /// lines of the current input file read_file_line() discards before
   /// they ever reach the tokenizer/executor. Real APL2 has no such
   /// command; see IO_Files::Skip_mode's own comment.
   /// @param out   output stream for command result
   /// @param args  exactly one of NO_SKIP, SKIP_GNU, SKIP_APL2, SKIP_ALL
   static void cmd_FILE_CTL(ostream & out, const UCS_string_vector & args);

   /// PUSHFILE: push one (testcase-) file
   static void cmd_PUSHFILE();

protected:
   /// return true iff, according to config.h, capability \b capa is available
   /// @param capa  the capability name to check (e.g. "⎕FFT", "GTK")
   static bool have_capability(const UCS_string & capa);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __CMD_HOST_HH_DEFINED__
