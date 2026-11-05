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
    Cmd_USERCMD:: implements the ]USERCMD command: check_name_conflict(),
    check_redefinition(), cmd_USERCMD(), and do_USERCMD() (the latter is
    invoked from Command::do_APL_command() once a previously registered
    user command matches).
*/

#include "Avec.hh"
#include "Cmd_USERCMD.hh"
#include "Command.hh"
#include "Common.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
bool
Cmd_USERCMD::check_name_conflict(ostream & out, const UCS_string & cnew,
                             const UCS_string cold)
{
   // commands are dispatched on the whole word (see Command::is_command()),
   // and cannot be abbreviated, so only the same name (case-insensitive,
   // including the ] or ) in front) is a conflict. Any common prefix was a
   // conflict before, e.g. ]LOGGER (with ]LOG) or )X (with ]XTERM) (Bugs34
   // #26).
   //
   if (cnew.size() != cold.size())   return false;   // OK: different

   loop(l, cnew.size())
      {
        int c1 = cnew[l];
        int c2 = cold[l];
        if (c1 >= 'a' && c1 <= 'z')   c1 -= 0x20;   // uppercase
        if (c2 >= 'a' && c2 <= 'z')   c2 -= 0x20;   // uppercase
        if (c1 != c2)   return false;   // OK: different
     }

   out << "BAD COMMAND+" << endl;
   MORE_ERROR() << "conflict with existing command name in command ]USERCMD";
   if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;

   return true;
}
//────────────────────────────────────────────────────────────────────────────
int
Cmd_USERCMD::check_redefinition(ostream & out, const UCS_string & cnew,
                            const UCS_string fnew, const int mnew)
{
   loop(u, Workspace::get_user_commands().size())
     {
       const UCS_string cold = Workspace::get_user_commands()[u].prefix;
       const UCS_string fold = Workspace::get_user_commands()[u].apl_function;
       const int mold = Workspace::get_user_commands()[u].mode;

       if (cnew != cold)   continue;

       // user command name matches; so must mode and function
       if (mnew != mold || fnew != fold)
         {
           out << "BAD COMMAND+" << endl;
           MORE_ERROR() <<
           "conflict with existing user command definition in command ]USERCMD";
           if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
           return 2;   // refused
         }
       return 1;   // identical
     }

   return 0;   // new
}
//════════════════════════════════════════════════════════════════════════════
void
Cmd_USERCMD::cmd_USERCMD(ostream & out, const UCS_string & cmd,
                     UCS_string_vector & args)
{
   // case 1:    ]USERCMD
   // case 2a:   ]USERCMD REMOVE-ALL
   // case 2b:   ]USERCMD REMOVE        ]existing-command
   // case 3a:   ]USERCMD ]new-command  APL-fun
   // case 3b:   ]USERCMD ]new-command  APL-fun  mode
   // case 3c:   ]USERCMD ]new-command  { ... }
   //
   if (args.size() == 0)   // case 1: list all commands
      {
        if (Workspace::get_user_commands().size())
           {
             for (size_t u = Workspace::get_user_commands().size(); u;)
                {
                  const Command::user_command & ucmd =
                                       Workspace::get_user_commands()[--u];
                  out << setw(12) << ucmd.prefix << " → ";
                  if (ucmd.mode)   out << "A ";   // if dyadic
                  out << ucmd.apl_function << " B"
                      << " (mode " << ucmd.mode << ")" << endl;
                }
           }
        return;
      }

  if (args.size() == 1 && args.front().starts_iwith("REMOVE-ALL"))   // case 2a.
     {
       Workspace::get_user_commands().clear();
       out << "    All user-defined commands removed." << endl;
       return;
     }

  if (args.size() == 2 && args.front().starts_iwith("REMOVE"))   // case 2b.
     {
       loop(u, Workspace::get_user_commands().size())
           {
             if (Workspace::get_user_commands()[u].prefix
                                                  .starts_iwith(args[1]) &&
                 args[1].starts_iwith(Workspace::get_user_commands()[u]
                                                          .prefix))   // same
                {
                  // print first and remove then!
                  //
                  out << "    User-defined command "
                      << Workspace::get_user_commands()[u].prefix
                      << " removed." << endl;
                  Workspace::get_user_commands().
                     erase(Workspace::get_user_commands().begin() + u);
                  return;
                }
           }

       out << "BAD COMMAND+" << endl;
       MORE_ERROR() << "user command in command"
                       " ]USERCMD REMOVE does not exist";
       if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
       return;
     }

  // cases 3a, 2b, and 3c...
  //

  // check that the user command is not followed by the string
  if (args.size() == 1)
     {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "The user command syntax is: ]USERCMD "
                        " ]new-command  APL-function [mode]";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return;
     }

UCS_string command_name = args.front();
UCS_string apl_fun = args[1];
int mode = 0;

   // check if lambda
   bool is_lambda = false;
   if (apl_fun.front() == '{')
      {
         // looks like the user command is a lambda function.
         // Collect space-separated tokens until curly braces balance; any
         // token after the closing } is an explicit mode (e.g. ]USERCMD ]f
         // {⍵+1} 0).
         UCS_string result;
         int depth = 0;
         ShapeItem lambda_end_idx = -1;
         for (ShapeItem i = 1; i < args.ssize(); ++i)
            {
               for (ShapeItem k = 0; k < args[i].ssize(); ++k)
                  {
                    const Unicode uni = args[i][k];
                    if      (uni == UNI_L_CURLY)   ++depth;
                    else if (uni == UNI_R_CURLY)   --depth;
                  }
               result << args[i];
               if (depth == 0)   { lambda_end_idx = i;   break; }
            }
         // check if lambda-function closed properly
         if (depth == 0 && result.back() == UNI_R_CURLY)
            {
               is_lambda = true;
               apl_fun = result;
               // explicit mode may follow the closing }
               if (lambda_end_idx + 1 < args.ssize())
                  mode = args[lambda_end_idx + 1].atoi();
               else
                  // determine the mode: if both alpha and omega present then
                  // assume dyadic, otherwise monadic usage
                  mode = (apl_fun.contains(UNI_OMEGA) &&
                          apl_fun.contains(UNI_ALPHA)) ? 1 : 0;
            }
         else
            {
               out << "BAD COMMAND+" << endl;
               MORE_ERROR() << "closing } in lambda function not found";
               if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
               return;
            }
      }

   if (args.size() > 3 && !is_lambda)
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "too many parameters in command ]USERCMD";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return;
      }

   // check mode
   //
   if (!is_lambda && args.size() == 3)   mode = args[2].atoi();
   if (mode < 0 || mode > 1)
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "unsupported mode " << mode
                     << " in command ]USERCMD (0 or 1 expected)";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return;
      }

   // check command name
   //
   loop(c, command_name.size())
      {
        bool error = false;
        if (c == 0)   // command must start with ] or )
           {
             if (command_name[c] != ']' && command_name[c] != ')')
                error = true;
           }
        else // and continue with symbol characters
           {
             error = error || !Avec::is_symbol_char(command_name[c]);
           }

        if (error)
           {
             out << "BAD COMMAND+" << endl;
             MORE_ERROR() << " bad user command name in command ]USERCMD";
             if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
             return;
           }
      }

   // check conflicts with existing commands
   //
#define cmd_def(cmd_str, _cmd, _arg, _hint) \
   if (check_name_conflict(out, UTF8_string(cmd_str), command_name))   return;
#include "Command.def"
   // an identical redefinition is fine, a different one has been refused
   // (BAD COMMAND+, and then not "installed" as well, Bugs34 #47)
   //
   switch(check_redefinition(out, command_name, apl_fun, mode))
     {
       case 0:   break;   // new user command
       case 1:   out << "    User-defined command "
                     << command_name << " installed." << endl;
                 return;
       default:  return;   // refused
     }

   // check APL function name
   // Only needed when not a lambda function
   if (!is_lambda)
      {
         loop(c, apl_fun.size())
            {
               if (!Avec::is_symbol_char(apl_fun[c]))
                  {
                     out << "BAD COMMAND+" << endl;
                     MORE_ERROR() <<
                          "bad APL function name in command ]USERCMD";
                     if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
                     return;
                  }
            }
      }

const Command::user_command new_user_command = { command_name, apl_fun, mode };

   // keep user commands sorted by command_name
   //
vector<Command::user_command> & user_commands = Workspace::get_user_commands();
bool inserted = false;
   loop(u, user_commands.size())
       {
         const Comp_result comp = user_commands[u].prefix.compare(command_name);
         if (comp == COMP_LT)
            {
              user_commands.insert(user_commands.begin() + u, new_user_command);
              inserted = true;
              break;
            }
      }

   if (!inserted)   user_commands.push_back(new_user_command);

   out << "    User-defined command "
       << new_user_command.prefix << " installed." << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_USERCMD::do_USERCMD(ostream & out, UCS_string & apl_cmd,
                    const UCS_string & line, const UCS_string & cmd,
                    UCS_string_vector & args, int uidx)
{
   /*
      apl_cmd   is '' (due to line.clear() in Command::do_APL_command().
      line      is the user input from (including) the command
                                  to the end of the line.
      cmd       is the command (first token in line)
      args is the tokenized line
    */

  if (Workspace::get_user_commands()[uidx].mode > 0)   // dyadic
     {
        // construct the left argument of the (dyadic) 'apl_function'
        apl_cmd.append_single_quoted(cmd);
        apl_cmd << UNI_SPACE;
        loop(a, args.size())
           {
             apl_cmd.append_single_quoted(args[a]);
             apl_cmd << UNI_SPACE;
           }
     }

   apl_cmd << Workspace::get_user_commands()[uidx].apl_function << UNI_SPACE;
   apl_cmd.append_single_quoted(line);
}
//════════════════════════════════════════════════════════════════════════════
