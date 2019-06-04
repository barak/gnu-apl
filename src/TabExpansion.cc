/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2017  Dr. Jürgen Sauermann

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

#include <dirent.h>
#include <errno.h>

#include "LibPaths.hh"
#include "TabExpansion.hh"
#include "Workspace.hh"

//-----------------------------------------------------------------------------
TabExpansion::TabExpansion(UCS_string & line)
   : have_trailing_blank(line.size() && line.back() == ' ')
{
}
//-----------------------------------------------------------------------------
ExpandResult
TabExpansion::expand_tab(UCS_string & user)
{
   // skip leading and trailing blanks
   //
   user.remove_leading_and_trailing_whitespaces();

   if (user.size() == 0)                      // nothing entered yet
      return expand_user_name(user);

   if (Avec::is_first_symbol_char(user[0]))   // start of user defined name
      return expand_user_name(user);

   if (user[0] == ')' || user[0] == ']')      // APL command
      return expand_APL_command(user);

   return expand_distinguished_name(user);
}
//-----------------------------------------------------------------------------
ExpandResult
TabExpansion::expand_user_name(UCS_string & user)
{
std::vector<const Symbol *> symbols = Workspace::get_all_symbols();

UCS_string_vector matches;
   loop(s, symbols.size())
      {
        const Symbol * sym = symbols[s];
        if (sym->is_erased())    continue;

        const UCS_string & sym_name = sym->get_name();
        if (!sym_name.starts_with(user))   continue;
        matches.push_back(sym_name);
      }

   if (matches.size() == 0)   return ER_IGNORE;   // no match

   if (matches.size() > 1)    // multiple names match user input
      {
        const int user_len = user.size();
        user.clear();
        return show_alternatives(user, user_len, matches);
      }

   // unique match
   //
   if (user.size() < matches[0].size())
      {
        // the name is longer than user, so we expand it.
        //
        user = matches[0];
        return ER_REPLACE;
      }

   return ER_IGNORE;
}
//-----------------------------------------------------------------------------
ExpandResult
TabExpansion::expand_APL_command(UCS_string & user)
{
ExpandHint ehint = EH_NO_PARAM;
const char * shint = 0;
UCS_string_vector matches;
UCS_string cmd = user;
UCS_string arg;
   cmd.split_ws(arg);

#define cmd_def(cmd_str, code, arg, hint)                \
   { UCS_string ustr(cmd_str);                           \
     if (ustr.starts_iwith(cmd))                         \
        { matches.push_back(ustr); ehint = hint; shint = arg; } }
#include "Command.def"

   // no match was found: ignore the TAB
   //
   if (matches.size() == 0)   return ER_IGNORE;

   // if we have multiple matches but the user has provided a command
   // argument then some matches were wrong. For example:
   //
   // LIB 3 matches LIB and LIBS
   //
   // remove wrong matches
   //
   if (matches.size() > 1 && arg.size() > 0)
      {
         again:
        if (matches.size() > 1)
           {
             loop(m, matches.size())
                {
                  if (matches[m].size() != cmd.size())   // wrong match
                     {
                       matches[m].swap(matches.back());
                       matches.pop_back();
                       goto again;
                     }
                }
           }
      }

   // if multiple matches were found then either expand the common part
   // or list all matches
   //
   if (matches.size() > 1)   // multiple commands match cmd
      {
        user.clear();
        return show_alternatives(user, cmd.size(), matches);
      }

   // unique match
   //
   if (cmd.size() < matches[0].size())
      {
        // the command is longer than user, so we expand it.
        //
        user = matches[0];
        if (ehint != EH_NO_PARAM)   user.append(UNI_ASCII_SPACE);
        return ER_REPLACE;
      }

   if (cmd.size() == matches[0].size() &&
       ehint != EH_NO_PARAM            &&
       arg.size() == 0                 &&
       !have_trailing_blank)   // no blank entered
      {
             // the entire command was entered but without a blank. If the
             // command has arguments then append a space to indicate that.
             // Otherwiese fall throught to expand_command_arg();
             //
             user = matches[0];
             user.append(UNI_ASCII_SPACE);
             return ER_REPLACE;
      }

   return expand_command_arg(user, ehint, shint, matches[0], arg);
}
//-----------------------------------------------------------------------------
ExpandResult
TabExpansion::expand_command_arg(UCS_string & user, 
                            ExpandHint ehint, const char * shint,
                            const UCS_string cmd, const UCS_string arg)
{
   switch(ehint)
      {
        case EH_NO_PARAM:
             return ER_IGNORE;

        case EH_oWSNAME:
        case EH_oLIB_WSNAME:
        case EH_oLIB_oPATH:
        case EH_oPATH:
        case EH_FILENAME:
        case EH_DIR_OR_LIB:
        case EH_WSNAME:
             return expand_filename(user, ehint, shint, cmd, arg);

        case EH_PRIMITIVE:
             return expand_help_topics(user);

        default:
             CIN << endl;
             CERR << cmd << " " << shint << endl;
             return ER_AGAIN;
      }
}
//-----------------------------------------------------------------------------
ExpandResult
TabExpansion::expand_help_topics(UCS_string & user)
{
   if (user.size() <= 5)   return expand_help_topics();   // initial display

const UCS_string help(user, 0, 6);
UCS_string prefix(user, 5, user.size() - 5);   // the name prefix
   prefix.remove_leading_whitespaces();

std::vector<const Symbol *> symbols = Workspace::get_all_symbols();

UCS_string_vector matches;
   loop(s, symbols.size())
      {
        const Symbol * sym = symbols[s];
        if (sym->is_erased())    continue;

        const UCS_string & sym_name = sym->get_name();
        if (!sym_name.starts_with(prefix))   continue;
        matches.push_back(sym_name);
      }

   if (matches.size() == 0)   return ER_IGNORE;   // no match
   if (matches.size() > 1)   // multiple matches
      {
        matches.sort();

        const int common_len = compute_common_length(prefix.size(), matches);
        if (common_len > prefix.size())
           {
             // all matches can be extended in a unique way
             //
             matches[0].resize(common_len);
             user = help + matches[0];
             return ER_REPLACE;
           }

        // all possible extensions are different
        //
        return show_alternatives(user, prefix.size(), matches);
      }

   // unique match
   //
   user = help + matches[0];
   return ER_REPLACE;
}
//-----------------------------------------------------------------------------
ExpandResult
TabExpansion::expand_help_topics()
{
   CIN << "\n";
   CERR << "Help topics (APL primitives and user-defined names) are:" << endl;

   // show APL primotives (but only once). For that Help.def must be sorted.
   //
const char * last = "";
int col = 0;
const int max_col = Workspace::get_PW() - 4;

#define help_def(_ar, prim, _name, _title, _descr)                          \
   if (strcmp(prim, last))                                                  \
      { CERR << " " << (last = prim);   col += 2;                           \
        if (col > max_col)   { CERR << endl;   col = 0; } \
      }
#include "Help.def"

std::vector<const Symbol *> symbols = Workspace::get_all_symbols();

UCS_string_vector names;
   loop(s, symbols.size())
      {
        const Symbol * sym = symbols[s];
        if (sym->is_erased())    continue;

        const UCS_string & sym_name = sym->get_name();
        names.push_back(sym_name);
      }

   names.sort();

   // see if printing full names takes more than 5 lines. If so then
   // only print first characters
   //
int rows = 1;   // the current row
int c1 = col;
   loop(n, names.size())
      {
        const int len = 1 + names[n].size();
        c1 += len;
        if (c1 > max_col)   { ++rows;   c1 = len; }
      }

   if (names.size() == 0)   /* nothing to do */;
   else if (rows < 6)
      {
        loop(n, names.size())
            {
              const int len = 1 + names[n].size();
              col += len;
              if (col > max_col)   { CERR << endl;   col = len; }
              CERR << " " << names[n];
            }
      }
   else
      {
        Unicode uni = names[0][0];
        CERR << " " << uni;
        for (ShapeItem n = 1; n < ShapeItem(names.size()); ++n)
            {
              if (names[n][0] == uni)   continue;   // same first character
              uni = names[n][0];
              CERR << " " << uni;   col += 2;
              if (col > max_col)   { CERR << endl;   col = 0; }
            }
      }

   CERR << endl;
   return ER_AGAIN;
}
//-----------------------------------------------------------------------------

ExpandResult
TabExpansion::expand_distinguished_name(UCS_string & user)
{
   // figure the length of longest ⎕xxx name (probably ⎕TRACE == 5)
   //
unsigned int max_e = 2;
#define ro_sv_def(_q, str, _txt) if (max_e < strlen(str))   max_e = strlen(str);
#define rw_sv_def(_q, str, _txt) if (max_e < strlen(str))   max_e = strlen(str);
#define sf_def(_q, str, _txt)    if (max_e < strlen(str))   max_e = strlen(str);
#include "SystemVariable.def"

   // Search for ⎕ backwards from the end
   //
int qpos = -1;

   loop(e, max_e)
      {
        if (e >= user.size())   break;
        if (user[user.size() - e - 1] == UNI_Quad_Quad)
           {
             qpos = user.size() - e;
             break;
           }
      }

   if (qpos != -1)   // ⎕xxx at end
      {
        UCS_string qxx(user, qpos, user.size() - qpos);
        UCS_string_vector matches;

#define ro_sv_def(_q, str, _txt) { UCS_string ustr(str);   \
   if (ustr.size() && ustr.starts_iwith(qxx)) matches.push_back(ustr); }

#define rw_sv_def(_q, str, _txt) { UCS_string ustr(str);   \
   if (ustr.size() && ustr.starts_iwith(qxx)) matches.push_back(ustr); }

#define sf_def(_q, str, _txt) { UCS_string ustr(str);   \
   if (ustr.size() && ustr.starts_iwith(qxx)) matches.push_back(ustr); }

#include "SystemVariable.def"

        if (matches.size() == 0)   return ER_IGNORE;
        if (matches.size() > 1)
           {
            matches.sort();

            const int common_len = compute_common_length(qxx.size(), matches);
            if (common_len == qxx.size())
               {
                 // qxx is already the common part of all matching ⎕xx
                 // display matching ⎕xx
                 //
                 CIN << endl;
                 loop(m, matches.size())
                     {
                        CERR << "⎕" << matches[m] << " ";
                     }
                 CERR << endl;
                 return ER_AGAIN;
               }
            else
               {
                 // qxx is a prefix of the common part of all matching ⎕xx.
                 // expand to common part.
                 //
                 user = matches[0];
                 user.resize(common_len);
                 return ER_REPLACE;
               }
           }

        // unique match
        //
        user = UCS_string(UNI_Quad_Quad);
        user.append(matches[0]);
        return ER_REPLACE;
      }

   return ER_IGNORE;
}
//-----------------------------------------------------------------------------
ExpandResult
TabExpansion::expand_filename(UCS_string & user,
                              ExpandHint ehint, const char * shint,
                              const UCS_string cmd, UCS_string arg)
{
   if (arg.size() == 0)
      {
        // the user has entered a command but nothing else.
        // If the command accepts a library number then we propose one of
        // the existing libraries.
        //
        if (ehint == EH_oLIB_WSNAME || ehint == EH_DIR_OR_LIB)
           {
             // the command accepts a library reference number
             //
             UCS_string libs_present;
             loop(lib, 10)
                 {
                    if (!LibPaths::is_present(LibRef(lib)))
                       continue;

                    libs_present.append(UNI_ASCII_SPACE);
                    libs_present.append(Unicode(UNI_ASCII_0 + lib));
                 }
             if (libs_present.size() == 0)   goto nothing;

             CIN << endl;
             CERR << cmd << libs_present << " <workspace-name>" << endl;
             return ER_AGAIN;
           }

        goto nothing;
      }

   if (arg[0] >= '0' && arg[0] <= '9')
      {
        // library number 0-9. EH_DIR_OR_LIB is complete already so
        // EH_oLIB_WSNAME is the only expansion case left
        //
        if (ehint != EH_oLIB_WSNAME)   goto nothing;

        const LibRef lib = LibRef(arg[0] - '0');

        if (arg.size() == 1 && !have_trailing_blank)   // no space yet
           {
             user = cmd;
             user.append(UNI_ASCII_SPACE);
             user.append(Unicode(arg[0]));
             user.append(UNI_ASCII_SPACE);
             return ER_REPLACE;
           }

         // discard library reference number
         //
         if (arg.size() == 1)   arg.erase(0);
         if (arg.size())        arg.erase(0);
         return expand_wsname(user, cmd, lib, arg);
      }

   // otherwise: real file name
   {
     UCS_string dir_ucs;
     const bool slash_at_1 = arg.size() > 1 && arg[1] == UNI_ASCII_SLASH;
     const bool tilde_at_0 = arg[0] == UNI_ASCII_TILDE ||
                             arg[0] == UNI_TILDE_OPERATOR;

     if (arg[0] == UNI_ASCII_SLASH)                     // absolute path /xxx
        {
          dir_ucs = arg;
        }
     else if (arg[0] == UNI_ASCII_FULLSTOP && slash_at_1) // relative path ./xxx
        {
          const char * pwd = getenv("PWD");
          if (pwd == 0)   goto nothing;
          dir_ucs = UCS_string(pwd);
          dir_ucs.append(arg.drop(1));
        }
     else if (tilde_at_0 && slash_at_1)                 // user's home ~/
        {
          const char * home = getenv("HOME");
          if (home == 0)   goto nothing;
          dir_ucs = UCS_string(home);
          dir_ucs.append(arg.drop(1));
        }
     else if (tilde_at_0)                               // somebody's home
        {
          dir_ucs = UCS_string("/home/");
          dir_ucs.append(arg.drop(1));
        }
     else goto nothing;

     UTF8_string dir_utf = UTF8_string(dir_ucs);

     const char * dir_dirname = dir_utf.c_str();
     const char * dir_basename = strrchr(dir_dirname, '/');
     if (dir_basename == 0)   goto nothing;

     UTF8_string base_utf = UTF8_string(++dir_basename);
     dir_utf.resize(dir_basename - dir_dirname);
     dir_ucs = UCS_string(dir_utf);

     DIR * dir = opendir(dir_utf.c_str());
     if (dir == 0)   goto nothing;

     UCS_string_vector matches;
     read_matching_filenames(dir, dir_utf, base_utf, ehint, matches);
     closedir(dir);

     if (matches.size() == 0)
        {
          CIN << endl;
          CERR << "  no matching filesnames" << endl;
          return ER_AGAIN;
        }

     if (matches.size() > 1)
        {
          UCS_string prefix(base_utf);
          user = cmd;                     // e.g. )LOAD
          user.append(UNI_ASCII_SPACE);
          user.append(dir_ucs);           // e.g. )LOAD /usr/apl/
          return show_alternatives(user, prefix.size(), matches);
        }

     // unique match
     //
     user = cmd;
     user.append(UNI_ASCII_SPACE);
     user.append(dir_ucs);
     user.append(matches[0]);
     return ER_REPLACE;
   }

nothing:
   CIN << endl;
   CERR << cmd << " " << shint << endl;
   return ER_AGAIN;
}
//-----------------------------------------------------------------------------
ExpandResult
TabExpansion::expand_wsname(UCS_string & user, const UCS_string cmd,
                       LibRef lib, const UCS_string filename)
{
UTF8_string path = LibPaths::get_lib_dir(lib);
   if (path.size() == 0)
      {
        CIN << endl;
        CERR << "Invalib library reference " << lib << endl;
        return ER_AGAIN;
      }

DIR * dir = opendir(path.c_str());
   if (dir == 0)
      {
        CIN << endl;
        CERR
<< "  library reference " << lib
<< " is a valid number, but the corresponding directory " << endl
<< "  " << path << " does not exist" << endl
<< "  or is not readable. " << endl
<< "  The relation between library reference numbers and filenames" << endl
<< "  (aka. paths) can be configured file 'preferences'." << endl << endl
<< "  At this point, you can use a path instead of the optional" << endl
<< "  library reference number and the workspace name." << endl;

        user = cmd;
        user.append(UNI_ASCII_SPACE);
        return ER_REPLACE;
      }

UCS_string_vector matches;
UTF8_string arg_utf(filename);
   read_matching_filenames(dir, path, arg_utf, EH_oLIB_WSNAME, matches);
   closedir(dir);

   if (matches.size() == 0)   goto nothing;
   if (matches.size() > 1)
      {
        user = cmd;
        user.append(UNI_ASCII_SPACE);
        user.append_number(lib);
        user.append(UNI_ASCII_SPACE);
        return show_alternatives(user, filename.size(), matches);
      }

   // unique match
   //
   user = cmd;
   user.append(UNI_ASCII_SPACE);
   user.append_UTF8(path.c_str());
   user.append(UNI_ASCII_SLASH);
   user.append(matches[0]);
   return ER_REPLACE;

nothing:
   CIN << endl;
   CERR << cmd << " " << lib << " '" << filename << "'" << endl;
   return ER_AGAIN;
}
//-----------------------------------------------------------------------------
int
TabExpansion::compute_common_length(int len, const UCS_string_vector & matches)
{
   // we assume that all matches have the same case

   for (;; ++len)
       {
         loop(m, matches.size())
            {
              if (len >= matches[m].size())   return matches[m].size();
              if (matches[0][len] != matches[m][len])    return len;
            }
       }
}
//-----------------------------------------------------------------------------
void
TabExpansion::read_matching_filenames(DIR * dir, UTF8_string dirname,
                                 UTF8_string prefix, ExpandHint ehint,
                                 UCS_string_vector & matches)
{
const bool only_workspaces = (ehint == EH_oLIB_WSNAME) ||
                             (ehint == EH_WSNAME     ) ||
                             (ehint == EH_oWSNAME    );

   for (;;)
       {
          struct dirent * dent = readdir(dir);
          if (dent == 0)   break;

          const size_t dlen = strlen(dent->d_name);
          if (dlen == 1 && dent->d_name[0] == '.')   continue;
          if (dlen == 2 && dent->d_name[0] == '.'
                        && dent->d_name[1] == '.')   continue;

          if (strncmp(dent->d_name, prefix.c_str(), prefix.size()))   continue;

          UCS_string name(dent->d_name);

          const bool is_dir = Command::is_directory(dent, dirname);
          if (is_dir)   name.append(UNI_ASCII_SLASH);
          else if (only_workspaces)
             {
               const UTF8_string filename(dent->d_name);
               bool is_wsname = false;
               if (filename.ends_with(".apl"))   is_wsname = true;
               if (filename.ends_with(".xml"))   is_wsname = true;
               if (!is_wsname)   continue;
             }

          matches.push_back(name);
       }
}
//-----------------------------------------------------------------------------
ExpandResult
TabExpansion::show_alternatives(UCS_string & user, int prefix_len,
                           UCS_string_vector & matches)
{
const int common_len = compute_common_length(prefix_len, matches);

   if (common_len == prefix_len)
      {
        // prefix is already the common part of all matching files
        // display matching items
        //
        CIN << endl;
        loop(m, matches.size())
            {
              CERR << matches[m] << " ";
            }
        CERR << endl;
        return ER_AGAIN;
      }
   else
      {
        // prefix is a prefix of the common part of all matching files.
        // expand to common part.
        //
        const int usize = user.size();
        user.append(matches[0]);
        user.resize(usize + common_len);
        return ER_REPLACE;
      }
}
//-----------------------------------------------------------------------------




