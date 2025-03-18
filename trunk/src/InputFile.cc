/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

#include <limits.h>
#include <string.h>
#include <sys/time.h>

#include "Common.hh"
#include "InputFile.hh"
#include "IO_Files.hh"
#include "PrimitiveFunction.hh"
#include "PrintOperator.hh"
#include "UserFunction.hh"
#include "UserPreferences.hh"

vector<InputFile> InputFile::files_todo;
vector<InputFile> InputFile::files_orig;
int InputFile::stdin_line_no = 1;
int64_t InputFile::next_file_seq = 0;

//----------------------------------------------------------------------------
void
InputFile::open_current_file()
{
   if (files_todo.size() && files_todo[0].file == 0)
      {
        if (!strcmp(current_filename(), "-"))
           files_todo[0].file = stdin;
        else if (!strcmp(current_filename(), "stdin"))
           files_todo[0].file = stdin;
        else
             files_todo[0].file = fopen(current_filename(), "r");

        files_todo[0].line_no = 0;
      }
}
//----------------------------------------------------------------------------
void
InputFile::close_current_file()
{
   if (files_todo.size())   // there are input files
      {
        if (files_todo[0].from_COPY)
           {
              files_todo[0].from_COPY = false;
              --Bif_F1_EXECUTE::copy_pending;
           }

        if (files_todo[0].file)   // and the first file is open
           {
             if (files_todo[0].file != stdin)
                {
                  fclose(files_todo[0].file);
                  files_todo[0].file = 0;
                  files_todo[0].line_no = -1;
                }
           }
      }
}
//----------------------------------------------------------------------------
void
InputFile::randomize_files()
{
   {
     timeval now;
     gettimeofday(&now, 0);
     srandom(now.tv_sec + now.tv_usec);
   }

   // check that all file are test files
   //
   loop(f, files_todo.size())
      {
        if (files_todo[f].test == false)   // not a test file
           {
             CERR << "Cannot randomise testfiles when a mix of -T"
                     " and -f is used (--TR ignored)" << endl;
             return;
           }
      }

   for (size_t done = 0; done < 4*files_todo.size();)
       {
         const int n1 = random() % files_todo.size();
         const int n2 = random() % files_todo.size();
         if (n1 == n2)   continue;

         InputFile f1 = files_todo[n1];
         InputFile f2 = files_todo[n2];

         const char * ff1 = strrchr(f1.filename.c_str(), '/');
         if (!ff1++)   ff1 = f1.filename.c_str();
         const char * ff2 = strrchr(f2.filename.c_str(), '/');
         if (!ff2++)   ff2 = f2.filename.c_str();

         // the order of files named AAA... or ZZZ... shall not be changed
         //
         if (strncmp(ff1, "AAA", 3) == 0)   continue;
         if (strncmp(ff1, "ZZZ", 3) == 0)   continue;
         if (strncmp(ff2, "AAA", 3) == 0)   continue;
         if (strncmp(ff2, "ZZZ", 3) == 0)   continue;

         // at this point f1 and f2 shall be swapped.
         //
         files_todo[n1] = f2;
         files_todo[n2] = f1;
         ++done;
       }
}
//----------------------------------------------------------------------------
bool
InputFile::echo_current_file()
{
   if (files_todo.size()) return files_todo[0].echo;
   return UserPreferences::uprefs.echo_CIN ||
          ! UserPreferences::uprefs.do_not_echo;
}
//----------------------------------------------------------------------------
bool
InputFile::COPY_filter::check_filter(const UTF8_string & line)
{
UCS_string ucs_line(line);
   ucs_line.remove_leading_and_trailing_whitespaces();

   /* if we are in a function or in a variable (matched or not) then look for
      the end of the function or variable and return the current state (aka.
      in_matched)...
    */
   if (where == WH_in_function)
      {
        bool ret = in_matched;   // state of the current line
        if (ucs_line.size() && ucs_line[0] == UNI_NABLA)   // end of a function
           {
             where = WH_outside;
             in_matched  = false;
           }
        return ret;
      }

   if (where == WH_in_variable)
      {
        bool ret = in_matched;   // state of the current line
        if (ucs_line.size() == 0)                          // end of a vriable
           {
             where = WH_outside;
             in_matched  = false;
           }
        return ret;
      }

   /* at this point we are outside any function or variable.
      Look for the start of a new function or variable...
    */
   if (ucs_line.size() && (ucs_line.front() == UNI_NABLA))   // new function
      {
        where = WH_in_function;
        in_matched  = false;
        ucs_line = ucs_line.drop(1);
        UserFunction_header uh(ucs_line, false);
        const UCS_string & fun_name = uh.get_name();
        loop(n, object_filter.size())
           {
             if (fun_name == object_filter[n])   return in_matched = true;
           }
        return false;
      }

   /* maybe (the start of) a new variable.
      There are two cases (see Symbol::dump()):

      1. multiple lines, for example:   ... ⍝ VAR←
      2. single line, for example:      VAR←1 2 3

      Note that case 2. also matches case 1., therefore case 1.
           needs to be checked first.

      Strip off any garbage from ucs_line so that only the symbol name
      remains (or ucs_line is empty)
    */
const Unicode u0 = ucs_line.size() ? ucs_line.front() : Invalid_Unicode;
const Unicode u1 = ucs_line.size() ? ucs_line.back()  : Invalid_Unicode;
   if (u1 == UNI_LEFT_ARROW)                             // case 1.
      {
        ShapeItem pos = ucs_line.size() - 1;
        while (pos && Avec::is_symbol_char(ucs_line[pos - 1]))   --pos;
        ucs_line = UCS_string(ucs_line, pos, ucs_line.size() - pos - 1);
      }
   else if (Avec::is_quad(u0) || Avec::is_first_symbol_char(u0))   // case 2.
      {
        loop(u, ucs_line.size())
           {
             if (ucs_line[u] != UNI_LEFT_ARROW)   continue;   // not ←

             ucs_line.resize(u);
             ucs_line.remove_leading_and_trailing_whitespaces();
           }
      }
   else                                                       // something else
      {
        return false;   // since where = WH_outside
      }

   where = WH_in_variable;   // start of a new variable
   loop(of, object_filter.size())
       {
         if (ucs_line == object_filter[of])   return in_matched = true;
       }
   return in_matched = false;
}
//----------------------------------------------------------------------------

