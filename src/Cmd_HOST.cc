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
    Cmd_HOST:: implements host/file interaction: )HOST, ]NEXTFILE
    (+ have_capability), and ]PUSHFILE.
*/

#include "config.h"

#include <errno.h>
#include <string.h>
#include <sys/wait.h>

#include "Cmd_HOST.hh"
#include "Common.hh"
#include "InputFile.hh"
#include "IO_Files.hh"
#include "Sys.hh"
#include "UserPreferences.hh"

//════════════════════════════════════════════════════════════════════════════
void
Cmd_HOST::cmd_HOST(ostream & out, const UCS_string & arg)
{
   if (UserPreferences::uprefs.safe_mode)
      {
        out <<
"This interpreter was started in \"safe mode\" (command line option --safe,\n"
"see ⎕ARG). The APL command )HOST is not permitted in safe mode." << endl;
        return;
      }

UTF8_string host_cmd(arg);
PipeReader reader(host_cmd.c_str());
   if (!reader)   // popen() failed
      {
        out << ")HOST command failed: " << strerror(errno) << endl;
        return;
      }

   for (;;)
       {
         const int cc = reader.fgetc();
         if (cc == EOF)   break;
         out << char(cc);
       }

   errno = 0;
const int result = reader.close();
   Log(LOG_verbose_error)
      {
        if (result)   CERR << "NOTE: pclose(" << arg << ") says: errno="
                           << errno << " (" << strerror(errno) << ")" << endl;
      }

   // result == -1 means pclose() itself failed (typically waitpid()
   // returning ECHILD because the child was already reaped elsewhere,
   // e.g. by a SIGCHLD-disposition race -- see sys_popen()/sys_pclose()
   // in Sys.hh) rather than reporting the child's own termination.  That
   // is not an exit code at all, and printing the bare -1 here used to
   // look exactly like one (indistinguishable from a real
   // WEXITSTATUS()==-1-shaped value to anyone reading the transcript).
   // Report it as the system failure it is instead.
   if (result == -1)
      {
        out << endl << ")HOST: pclose() failed: " << strerror(errno)
            << endl;
        return;
      }

   // result is pclose()'s raw wait status (WIFEXITED/WEXITSTATUS-
   // encoded), not the child's exit code directly -- )HOST 'exit 3'
   // used to print 768 (3<<8) instead of 3. Decode it for a
   // normally-exited child; fall back to the raw status for a
   // signal-terminated one (no single "exit code" applies there).
   // See Bugs27 #59(g).
   //
const int exit_code = WIFEXITED(result) ? WEXITSTATUS(result) : result;
   out << endl << exit_code << ' ' << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_HOST::cmd_NEXTFILE(ostream & out, const UCS_string_vector & args)
{
   loop(a, args.size())
       {
         const UCS_string & arg = args[a];

         // SKIP/ELSE_SKIP/NOSKIP: pure no-ops here (GNU APL never skips
         // ahead for them) -- unlike a HAVE-/NO- capability check below,
         // these are not routed through have_capability() at all, since
         // they are not fictitious capabilities, just inert markers for
         // an external tool ("the workspace", e.g. a GNU-APL<->real-APL2
         // cross-checking script) to key off of when marking a stretch
         // of this file's testcases as using GNU/ISO-only features with
         // no real-APL2 equivalent: SKIP starts such a stretch (excluded
         // from an APL2 run), an optional ELSE_SKIP switches to an
         // APL2-safe substitute for it (included instead, when one is
         // worth writing), and NOSKIP ends the whole thing, back to
         // normal -- all without needing a second copy of the file or
         // reordering its testcases. See
         // project_apl2_and_or_boolean_only.md.
         //
         if ((arg.size() == 4 && arg.starts_iwith("SKIP"))
             || (arg.size() == 9 && arg.starts_iwith("ELSE_SKIP"))
             || (arg.size() == 6 && arg.starts_iwith("NOSKIP")))
            {
              return;   // true no-op: do NOT fall through to next_file()
            }

         // figure HAVE- or NO- prefix and capability
         //
         if (arg.starts_iwith("HAVE-"))
            {
              if (! have_capability(arg.drop(5)))   return;
            }
         else if (arg.starts_iwith("NO-"))
            {
              if (have_capability(arg.drop(3)))   return;
            }
         else
            {
              CERR << "]NEXTFILE: unknown capability '" << arg
                   << "' (ignored).\n"
                      "Capabilities start with HAVE- or with NO- "
                      "(try TAB expansion)."
                   << endl;
              continue;   // next arg
            }
       }

   IO_Files::next_file();
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_HOST::cmd_FILE_CTL(ostream & out, const UCS_string_vector & args)
{
static const char * mode_names[] =
   { "NO_SKIP", "SKIP_GNU", "SKIP_APL2", "SKIP_ALL" };

   if (args.size() == 0)   // ]FILE_CTL: show the current skip mode
      {
        out << mode_names[IO_Files::skip_mode] << endl;
        return;
      }

   if (args.size() != 1)
      {
        CERR << "]FILE_CTL: expecting exactly one of "
                "NO_SKIP, SKIP_GNU, SKIP_APL2, SKIP_ALL" << endl;
        return;
      }

   // the whole word (case-insensitive), not merely its start: e.g.
   // SKIP_GNUXX was silently accepted (Blake McBride, Bugs32 #29)
   //
const UCS_string & arg = args[0];
   if      (arg.starts_iwith("NO_SKIP")   && arg.size() == 7)
      IO_Files::skip_mode = IO_Files::SKIP_NONE;
   else if (arg.starts_iwith("SKIP_GNU")  && arg.size() == 8)
      IO_Files::skip_mode = IO_Files::SKIP_GNU;
   else if (arg.starts_iwith("SKIP_APL2") && arg.size() == 9)
      IO_Files::skip_mode = IO_Files::SKIP_APL2;
   else if (arg.starts_iwith("SKIP_ALL")  && arg.size() == 8)
      IO_Files::skip_mode = IO_Files::SKIP_ALL;
   else
      {
        CERR << "]FILE_CTL: unknown mode '" << arg << "' (ignored).\n"
                "Expecting one of NO_SKIP, SKIP_GNU, SKIP_APL2, SKIP_ALL"
             << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
bool
Cmd_HOST::have_capability(const UCS_string & capa)
{
const int len = capa.size();
   if (len == 0)   return false;   // e.g. bare "]NEXTFILE HAVE-"/"NO-"
   if (capa.front() == UNI_Quad_Quad)   // ⎕xx
      {
        const UCS_string capa1 = capa.drop(1);
        if (len == 4 && capa1.starts_iwith("FFT"))       return apl_FFT;
        if (len == 4 && capa1.starts_iwith("PNG"))       return apl_PNG;
        if (len == 3 && capa1.starts_iwith("RE"))        return apl_PCRE;
        // ⎕SQL and ⎕PLOT are not their own config.h flags (no AC_DEFINE
        // for either -- confirmed: apl_SQL only ever exists as a
        // configure.ac shell variable used for the ./configure summary
        // line, never exported to config.h/C++ at all) -- each is really
        // an umbrella over the specific backend(s) that implement it.
        if (len == 4 && capa1.starts_iwith("SQL"))
           return apl_POSTGRES || apl_SQLITE3;
        if (len == 5 && capa1.starts_iwith("PLOT"))
           return apl_GTK3 || apl_XCB;
      }
   else
      {
        if (len == 3 && capa.starts_iwith("GSL"))        return apl_GSL;
        if (len == 3 && capa.starts_iwith("GTK"))
           return apl_GTK3 && apl_X11;
        if (len == 3 && capa.starts_iwith("GUI"))        return apl_GUI;
        if (len == 8 && capa.starts_iwith("POSTGRES"))   return apl_POSTGRES;
        if (len == 7 && capa.starts_iwith("SQLITE3"))    return apl_SQLITE3;
        // shared variables: not a build option, but enabled at run time
        // (unless --noSV, SharedVars Disabled, or no APserver connection)
        if (len == 5 && capa.starts_iwith("SVARS"))
           return UserPreferences::uprefs.system_do_svars;
        if (len == 3 && capa.starts_iwith("X11"))        return apl_X11;
        if (len == 3 && capa.starts_iwith("XCB"))        return apl_XCB;
      }

   CERR << "]NEXTFILE: Unknown capability '" << capa << "'" << endl;
   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_HOST::cmd_PUSHFILE()
{
   CERR <<
"*** Pushing an immediate execution context (leave it with ]NEXTFILE)"
        << endl;

   if (InputFile::files_todo.size())
      InputFile::files_todo.front().set_pushed_pending(true);

InputFile fam("stdin", stdin, false, true, true, no_LX);
   fam.set_pushed_IE();
   InputFile::files_todo.insert(InputFile::files_todo.begin(), fam);
}
//════════════════════════════════════════════════════════════════════════════
