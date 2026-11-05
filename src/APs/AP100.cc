/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2026  Dr. Jürgen Sauermann

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

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "APmain.hh"
#include "CDR_string.hh"
#include "Svar_DB.hh"
#include "Svar_signals.hh"

using namespace std;

//════════════════════════════════════════════════════════════════════════════
/** true iff popen()/execve()-like command execution is disabled by the
    preferences file. AP100's whole job is to run a shell command received
    from a coupled variable and return its output (a documented, intentional
    capability -- see doc/apl.texi, "Shared Variables"), not a bug in itself;
    but that means AP100 should respect the same
    CHECK_SECURITY(disable_Quad_FIO__exec) (see Security.hh/Security.def)
    that gates execve()/popen() inside the main interpreter (⎕FIO.exec etc.),
    so an installation that has disabled shell execution for the interpreter
    does not still expose it via this AP.

    AP100 is a separate, minimal executable (see APs/Makefile.am:
    AP100_SOURCES only pulls in APmain/Backtrace/Svar_DB/Svar_record) that
    does not link UserPreferences/Workspace, so the full CHECK_SECURITY
    macro (which throws an APL DOMAIN_ERROR via Workspace) is not usable
    here -- this re-implements just enough of the same preferences-file
    lookup (same two file locations, same flag name, same
    cfg_SECURITY_LEVEL_WANTED semantics) instead of linking in the entire
    interpreter core for one flag.

    Only the "matches all profiles" section of the preferences file (file
    Profile 0, i.e. before any "Profile N" line, or after an explicit
    "Profile 0" line) is honored: AP100 has no -p N profile selection of
    its own, unlike the main interpreter.
**/
static bool
exec_disabled()
{
#if !defined(cfg_SECURITY_LEVEL_WANTED) || cfg_SECURITY_LEVEL_WANTED == 0
   return false;
#elif cfg_SECURITY_LEVEL_WANTED == 2
   return true;
#else
   string paths[2];
   paths[0] = string(apl_DIR__sysconf) + "/gnu-apl.d/preferences";
   if (const char * home = getenv("HOME"))
      paths[1] = string(home) + "/.gnu_apl/preferences";

   for (int p = 0; p < 2; ++p)
       {
         if (paths[p].empty())   continue;
         ifstream in(paths[p].c_str());
         if (!in.is_open())   continue;

         int file_profile = 0;
         string line;
         while (getline(in, line))
             {
               istringstream iss(line);
               string opt, arg;
               iss >> opt >> arg;
               if (opt.empty() || opt[0] == '#')   continue;

               if (opt == "Profile")
                  {
                    file_profile = atoi(arg.c_str());
                    continue;
                  }

               if (file_profile != 0)   continue;   // not our (default) profile

               if (opt == "disable_Quad_FIO__exec")
                  return arg == "yes" || arg == "Yes" || arg == "YES";
             }
       }

   return false;   // no preferences file, or flag not set: allowed
#endif
}

//════════════════════════════════════════════════════════════════════════════
const char * prog_name()
{
   return "AP100";
}
//════════════════════════════════════════════════════════════════════════════
struct SVAR_context
{
   SVAR_context()
  {}
};
//════════════════════════════════════════════════════════════════════════════
int
set_ACK(Coupled_var & var, int exco)
{
const uint8_t _cdr[] =
   {
     0x00, 0x00, 0x20, 0x20, // ptr
     0x00, 0x00, 0x00, 0x20, // nb: total len
     0x00, 0x00, 0x00, 0x01, // nelm: element count
     0x01, 0x00, 0x00, 0x00, // type, rank, pad, pad
     uint8_t(exco), uint8_t(exco>>8), uint8_t(exco>>16), uint8_t(exco>>24),
     0x00, 0x00, 0x00, 0x00, // pad
     0x00, 0x00, 0x00, 0x00, // pad
     0x00, 0x00, 0x00, 0x00, // pad
   };

const CDR_string * cdr = new CDR_string(_cdr, sizeof(_cdr));
   delete var.data;
   var.data = cdr;
   Svar_DB::set_state(var.key, true, LOC);

   return exco;
}
//════════════════════════════════════════════════════════════════════════════
void
handle_var(Coupled_var & var)
{
FILE * fp = 0;
const CDR_string & cdr = *var.data;
const CDR_header & header = cdr.header();
   if (cdr.size() < 20)   // less than min. size of CDR header
      {
        get_CERR() << "CDR record too short (" << cdr.size()
             << " bytes)" << endl;
        set_ACK(var, 444);
        return;
      }

   if (cdr[13] != 1)   // not a vector
      {
        get_CERR() << "Bad CDR rank (" << int(cdr[13]) << endl;
        set_ACK(var, 445);
        return;
      }

   if (cdr[12] != 4)   // not a char vector
      {
        get_CERR() << "Bad CDR record type (" << int(cdr[12]) << endl;
        set_ACK(var, 446);
        return;
      }

   if (header.get_nelm() > cdr.size() - 20)   // element count exceeds buffer
      {
        get_CERR() << "Bad CDR nelm (" << header.get_nelm()
             << " exceeds " << (cdr.size() - 20) << " available bytes)"
             << endl;
        set_ACK(var, 447);
        return;
      }

const string cmd(reinterpret_cast<const char *> (cdr.get_items()) + 20,
                 header.get_nelm());

   if (verbose)   get_CERR() << pref << " got command[" << cmd.size() << "] '"
                             << cmd << "'" << endl;

   if (exec_disabled())
      {
        get_CERR() << pref << " popen() refused: disabled by preferences"
                      " (disable_Quad_FIO__exec)" << endl;
        set_ACK(var, 2);  // 2 := DISABLED (distinct from 1 := INVALID COMMAND)
        return;
      }

   fp = popen(cmd.c_str(), "r");
   if (fp == 0)   // bad command
      {
        get_CERR() << pref << " popen() failed" << endl;
        set_ACK(var, 1);  // 1 := INVALID COMMAND
        return;
      }

   for (;;)
       {
         const int cc = fgetc(fp);
         if (cc == EOF)   break;
         get_CERR() << char(cc);
       }

   get_CERR() << flush;
const int result = pclose(fp);
   if (verbose)
   get_CERR() << pref << " finished command with exit code "
              << result << endl;

// set_ACK(var, WEXITSTATUS(result));
      set_ACK(var, result >> 8 & 0xFF);
}
//════════════════════════════════════════════════════════════════════════════
bool
is_valid_varname(const uint32_t * varname)
{
   // AP100 supports all variable names
   //
   return *varname;   // varname is not empty
}
//════════════════════════════════════════════════════════════════════════════
bool
initialize(Coupled_var & var)
{
   Svar_DB::set_state(var.key, true, LOC);
   Svar_DB::set_control(var.key, USE_BY_1);
   return false;   // OK
}
//════════════════════════════════════════════════════════════════════════════
bool
make_counter_offer(SV_key key)
{
   return true;   // make counter offer
}
//════════════════════════════════════════════════════════════════════════════

APL_error_code
assign_value(Coupled_var & var, const string & data)
{
   // check CDR size, type, and shape of data. We expect a char string
   // for both the CTL and the DAT variable.
   //
   if (data.size() < sizeof(CDR_header))   // too short
      { error_loc = LOC;   return E_LENGTH_ERROR; }

   delete var.data;
   var.data = new CDR_string(reinterpret_cast<const uint8_t *>(data.c_str()),
                             data.size());
   handle_var(var);
   return E_NO_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
APL_error_code
get_value(Coupled_var & var, string & data)
{
   if (var.data == 0)   return E_VALUE_ERROR;

   data = string(reinterpret_cast<const char *>(var.data->get_items()),
                 var.data->size());
   error_loc = "no_error";   return E_NO_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
void
retract(Coupled_var & var)
{
SVAR_context * ctx = var.context;
   if (ctx)
      {
        delete ctx;
      }
}

//════════════════════════════════════════════════════════════════════════════
