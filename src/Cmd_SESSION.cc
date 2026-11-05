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
    Cmd_SESSION:: implements the session/UI-setting commands
    )AV, ]BOXING, )EXPECT, ]LOG (+ log_control), )MORE, ]XTERM (aka ]COLOR).
*/

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "Avec.hh"
#include "Cmd_SESSION.hh"
#include "Command.hh"
#include "Common.hh"
#include "IO_Files.hh"
#include "Output.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
void
Cmd_SESSION::cmd_AV(ostream & out)
{
   out << "⎕AV:";
   for (int pos = 0x20; pos < Avec::MAX_AV; pos += 0x10)
       {
         loop(pp, 0x10)
            out << " " << Avec::unicode(Avec::CHT_Index(pos + pp));
         out << endl << "    ";
       }
   out << endl;
}
//════════════════════════════════════════════════════════════════════════════
void
Cmd_SESSION::cmd_BOXING(ostream & out, const UCS_string & arg)
{
int format = arg.atoi();

   // the parameter is OFF or an integer (Bugs35 #67: atoi() made every
   // non-number OFF, e.g. ON, and ignored trailing garbage, e.g. 4abc)
   //
bool valid = arg.size() > 0;
   if (arg.size() == 3 && arg.starts_iwith("OFF"))   format = 0;
   else
      {
        // a negative N may be written as -N or (APL style) as ¯N
        //
        loop(a, arg.size())
            {
              if (a == 0 && arg.size() > 1 &&
                  (arg[a] == UNI_MINUS || arg[a] == UNI_OVERBAR))   continue;
              if (!Avec::is_digit(arg[a]))   valid = false;
            }

        // every valid N has at most 2 digits; a longer digit string could
        // overflow atoi() and wrap to a valid N (Bugs37 #57)
        //
        if (arg.size() > 3)   valid = false;
      }

   if (arg.size() == 0)
      {
        out << "]BOXING ";
        if (Command::boxing_format == 0)     out << "OFF";
        else if (Command::boxing_format < 0) out << UNI_OVERBAR
                                                 << -Command::boxing_format;
        else                                 out << Command::boxing_format;
        out << endl;
        return;
      }

   if (valid)   switch (format)
      {
        case -29:
        case -25: case -24: case -23:
        case -22: case -21: case -20:
        case -9: case  -8: case  -7:
        case -4: case  -3: case  -2:
        case  0:
        case  2: case   3: case   4:
        case  7: case   8: case   9:
        case 20: case  21: case  22:
        case 23: case  24: case  25:
        case 29:
                 // through ⎕PS so that ⎕PS[2] follows ]BOXING
                 //
                 Workspace::get_v_Quad_PS().set_style(format);
                 return;
      }

   out << "BAD ]BOXING PARAMETER+" << endl;
   MORE_ERROR() << "Parameter " << arg << " is not valid for command ]BOXING.\n"
      "  Valid parameters are OFF, N, and -N (or ¯N) with\n"
      "  N ϵ { 2, 3, 4, 7, 8, 9, 20, 21, 22, 23, 24, 25, 29 }";
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_SESSION::cmd_EXPECT(ostream & out, const UCS_string & arg)
{
   IO_Files::expect_apl_errors(arg.atoi());
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_SESSION::cmd_LOG(ostream & out, const UCS_string & arg)
{
#ifdef cfg_DYNAMIC_LOG_WANTED

   log_control(arg);

#else

   out <<
"\n"
"The debug command ]LOG is not available, because dynamic logging was not\n"
"./configure'd for this APL interpreter. To configure dynamic logging (which\n"
"will slightly decrease performance), recompile the interpreter as follows:\n"
"\n"
"   $ ./configure DYNAMIC_LOG_WANTED=yes (... other configure options)\n"
"   $ make\n"
"   $ sudo make install (or: src/apl to run the recompiled interpreter\n"
"                        without installing it)\n"
"\n"
"in the top-level GNU APL directory (i.e. above the src directory)."
"\n";

#endif
}
//────────────────────────────────────────────────────────────────────────────
#ifdef cfg_DYNAMIC_LOG_WANTED
void
Cmd_SESSION::log_control(const UCS_string & arg)
{
UCS_string_vector args = Command::split_arg(arg);

   if (args.size() == 0 || arg.front() == UNI_QUESTION)  // no arg or '?'
      {
        for (LogId l = LID_MIN; l < LID_MAX; l = LogId(l + 1))
            {
              const char * info = Log_info(l);
              Assert(info);

              const bool val = Log_status(l);
              CERR << "    " << setw(2) << right << l << ": "
                   << (val ? "(ON)  " : "(OFF) ") << info << endl;
            }

        return;
      }

   // at this point args is a sequence of numbers (LIDs) and actions (ON or
   // OFF). We parse args back to front. to acreate a vector of actions
   // which is then executed from front to back.
   //
vector<struct lid_OOT> lid_OOTs;
OOT action = Toggle;
   loop(a, args.size())
       {
         const UCS_string & ucs = args[args.size() - a - 1];
         if      (ucs.starts_iwith("ON"))    action = On;
         else if (ucs.starts_iwith("OFF"))   action = Off;
         else   // remember the LogId and its action and
            {
              const LogId lid = LogId(ucs.atoi());
              const lid_OOT lo = { lid, action };
              if (lid >= LID_MIN && lid <= LID_MAX)
                 lid_OOTs.push_back(lo);
              else
                 CERR << "    Invalid logging facility " << lid
                      << " ignored. Valid logging facilities are: "
                      << LID_MIN << ".." << (LID_MAX - 1) << endl;
            }
       }

   loop(a, lid_OOTs.size())
       {
         const lid_OOT lo = lid_OOTs[lid_OOTs.size() - a - 1];
         const LogId lid = lo.lid;
         const char * info = Log_info(lid);
         bool new_status = !Log_status(lid);   // assume toggle
         if      (lo.on_off_toggle == On)   new_status = true;
         else if (lo.on_off_toggle == Off)  new_status = false;
         Log_control(lid, new_status);

         CERR << "    Logging facility " << lid << ": " << info
              << " is now " << (new_status ? "ON " : "OFF") << endl;
       }
}
#endif
//────────────────────────────────────────────────────────────────────────────
void
Cmd_SESSION::cmd_MORE(ostream & out, const UCS_string_vector & args)
{
   if (args.size() > 0)   // optional AUTO ?
      {
        if (!args.front().starts_iwith("AUTO"))   // no
           {
             CERR << "BAD COMMAND+" << endl;
             MORE_ERROR() << "Bad )MORE argument: " << args.front()
                          << ". Use none or AUTO.";
             if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
             return;
           }

        // )MORE AUTO...
        //
        if (args.size() > 1)   // optional ON/OFF ?
           {
             if      (args[1].starts_iwith("ON"))    Command::auto_MORE = true;
             else if (args[1].starts_iwith("OFF"))   Command::auto_MORE = false;
             else
                {
                  CERR << "BAD COMMAND+" << endl;
                  MORE_ERROR() << "Bad )MORE AUTO argument: " << args[1]
                               << ". Use none, ON, or OFF.";
                  if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
                  return;
                }
           }
        else                   // no ON or OFF:
           {
             Command::auto_MORE = ! Command::auto_MORE;   // toggle
           }
        out << "Automatic )MORE is now: "
            << (Command::auto_MORE ? "ON" : "OFF") << endl;
        return;
      }

   if (Workspace::more_error().size() == 0)
      {
        out << "NO )MORE ERROR INFO" << endl;
        return;
      }

   out << Workspace::more_error() << endl;
   return;
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_SESSION::cmd_XTERM(ostream & out, const UCS_string & arg)
{
const char * term = getenv("TERM");
   if (term && !strncmp(term, "dumb", 4) && arg.starts_iwith("ON"))
      {
        out << "impossible on dumb terminal" << endl;
      }
   else if (arg.starts_iwith("OFF") || arg.starts_iwith("ON"))
      {
        Output::toggle_color(arg);
      }
   else if (arg.size() == 0)
      {
        out << "]COLOR/XTERM ";
        if (Output::color_enabled()) out << "ON"; else out << "OFF";
        out << endl;
      }
   else
      {
        out << "BAD COMMAND" << endl;
        return;
      }
}
//════════════════════════════════════════════════════════════════════════════
