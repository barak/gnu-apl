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

#include "config.h"

#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "Bif_F1_EXECUTE.hh"
#include "Common.hh"
#include "InputFile.hh"
#include "IO_Files.hh"
#include "PrintOperator.hh"
#include "UserFunction.hh"
#include "UserPreferences.hh"
#include "Workspace.hh"

vector<InputFile> InputFile::files_todo;
vector<InputFile> InputFile::files_orig;
int InputFile::stdin_line_no = 1;
int64_t InputFile::next_file_seq = 0;

//════════════════════════════════════════════════════════════════════════════
bool
InputFile::COPY_filter::replace_object(const UCS_string & name, Obj_kind kind)
{
   // the object name of kind in a .apl file shall be copied: prepare the
   // global symbol name for it and return true, or else print why it is
   // not copied and return false (Bugs37 #26, #27, Bugs38 #25, #26).
   //
Symbol * sym = Workspace::lookup_existing_symbol(name);
   if (sym == 0)   return true;   // new name

   // LRM p. 423: )COPY replaces global objects only, "the definitions of
   // local objects are not affected by the )COPY command". A localized
   // name would be fixed or assigned at its local level (Bugs38 #25).
   //
   if (sym->value_stack_size() > 1)
      {
        CERR << "NOT COPIED: " << name
             << " (localized in a suspended function)" << endl;
        return false;
      }

const NameClass nc = sym->get_NC();
bool replace = false;   // expunge the existing object
   switch(kind)
      {
        case OBJ_FUNCTION:   // a function replaces a variable
             replace = nc == NC_VARIABLE;
             break;

        case OBJ_VALUE:      // a variable replaces a function
             replace = (nc & NC_FUN_OPER) != 0;
             break;

        case OBJ_STRUCT:     // a structured variable or a lambda
        case OBJ_LAMBDA:     // replace a variable or a function
             replace = nc == NC_VARIABLE || (nc & NC_FUN_OPER) != 0;
             break;
      }

   if (!replace)   return true;

   if (nc & NC_FUN_OPER)
      if (const char * why = sym->cant_be_defined())   // e.g. on the )SI
         {
           CERR << "NOT COPIED: " << name << " (" << why << ")" << endl;
           return false;
         }

   sym->expunge();
   return true;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
InputFile::COPY_filter::sysvar_assigned(const UCS_string & line)
{
   // )DUMP writes a system variable as one line ⎕XX←value
   //
   if (line.size() < 3 || !Avec::is_quad(line[0]))   return UCS_string();

UCS_string name(1, line[0]);
   for (ShapeItem u = 1; u < line.ssize(); ++u)
       {
         const Unicode uni = line[u];
         if (uni == UNI_LEFT_ARROW)   return name;
         if (!Avec::is_symbol_char(uni))   break;
         name << uni;
       }
   return UCS_string();
}
//────────────────────────────────────────────────────────────────────────────
bool
InputFile::COPY_filter::copyable_sysvar(const UCS_string & name)
{
   return name == ID::get_name_UCS(ID_Quad_CT) ||
          name == ID::get_name_UCS(ID_Quad_FC) ||
          name == ID::get_name_UCS(ID_Quad_IO) ||
          name == ID::get_name_UCS(ID_Quad_LX) ||
          name == ID::get_name_UCS(ID_Quad_PP) ||
          name == ID::get_name_UCS(ID_Quad_PR) ||
          name == ID::get_name_UCS(ID_Quad_RL);
}
//────────────────────────────────────────────────────────────────────────────
bool
InputFile::COPY_filter::session_sysvar(const UCS_string & name)
{
   return name == ID::get_name_UCS(ID_Quad_PW) ||
          name == ID::get_name_UCS(ID_Quad_TZ) ||
          name == ID::get_name_UCS(ID_Quad_PS);
}
//────────────────────────────────────────────────────────────────────────────
bool
InputFile::COPY_filter::is_selected(const UCS_string & name) const
{
   if (object_filter.size() == 0)   return true;   // no filter: all names
   loop(n, object_filter.size())
      {
        if (name == object_filter[n])   return true;
      }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
InputFile::COPY_filter::check_filter(const UTF8_string & line)
{
UCS_string ucs_line(line);
   ucs_line.remove_leading_and_trailing_whitespaces();

   // )LOAD of a .apl file: execute every line, except the assignments of
   // the session variables ⎕PW, ⎕TZ, and ⎕PS outside of functions (like
   // )LOAD of a .xml file, Bugs38 #23).
   //
   if (loading)
      {
        if (ucs_line.size() && ucs_line.front() == UNI_NABLA)
           where = (ucs_line.size() > 1) ? WH_in_function : WH_outside;
        else if (ucs_line.size() == 1 && ucs_line.front() == UNI_DEL_TILDE)
           where = WH_outside;   // end of a locked function (Bugs39 #50)

        if (where == WH_outside)
           {
             const UCS_string sysvar = sysvar_assigned(ucs_line);
             if (sysvar.size() && session_sysvar(sysvar))   return false;
           }
        return true;
      }

static const UCS_string struct_start(UTF8_string("⍝ structured variable "));
static const UCS_string struct_end(UTF8_string("⍝ end of structured variable "));

   // )COPY or )PCOPY: an object (function, variable, or structured variable)
   // of the file spans one or more lines. Copy (i.e. execute) all lines of
   // a selected object and none of an unselected one. A plain )COPY (no
   // object filter, no )PCOPY protection) also executes all other lines.
   //
const bool plain_copy = !has_object_filter() && !protection;

   /* if we are in a function or in a variable (matched or not) then look for
      the end of the function or variable and return the current state (aka.
      in_matched)...
    */
   if (where == WH_in_function)
      {
        bool ret = in_matched;   // state of the current line
        if (ucs_line.size() && (ucs_line[0] == UNI_NABLA ||   // end of a
                                ucs_line[0] == UNI_DEL_TILDE))   // function
           {                                       // (⍫: locked, Bugs39 #50)
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

   // a structured variable S, written by Symbol::dump() as a block:
   //
   // ⍝ structured variable S
   //   S.b←2
   //   S.a←1
   // ⍝ end of structured variable S
   //
   // The block (as a whole) belongs to S, not to the names of its member
   // lines (Blake McBride, Bugs32 #8).
   //
   if (where == WH_in_struct)
      {
        if (ucs_line.starts_with(struct_end))   // end of the block
           {
             where = WH_outside;
             const bool ret = in_matched;
             in_matched  = false;
             return ret;
           }
        return in_matched;
      }

   /* at this point we are outside any function or variable.
      Look for the start of a new function or variable...
    */

   // the )WSID that )DUMP writes near the end of the file belongs to )LOAD;
   // )COPY does not change the WSID (Bugs41 #38)
   //
   if (ucs_line.starts_with(UCS_ASCII_string(")WSID")))   return false;

   // a system variable: )COPY copies only ⎕CT, ⎕FC, ⎕IO, ⎕LX, ⎕PP, ⎕PR,
   // and ⎕RL, with or without object names (LRM p. 423, Bugs38 #23, #82),
   // and )PCOPY none of them (they always exist).
   //
   {
     const UCS_string sysvar = sysvar_assigned(ucs_line);
     if (sysvar.size())
        {
          where = WH_in_variable;
          return in_matched = copyable_sysvar(sysvar) && !protection
                                                      && is_selected(sysvar);
        }
   }

   if (ucs_line.starts_with(struct_start))   // new structured variable
      {
        where = WH_in_struct;
        UCS_string var_name(ucs_line, struct_start.size(),
                            ucs_line.size() - struct_start.size());
        var_name.remove_leading_and_trailing_whitespaces();

        // )PCOPY: do not overwrite an already existing object. )COPY:
        // replace an existing variable S as a whole, so that neither a
        // plain variable S (whose member lines would fail) nor old members
        // of S survive.
        //
        bool selected = is_selected(var_name);
        if (selected && protection &&
            Workspace::lookup_existing_symbol(var_name))   selected = false;
        if (selected && !protection)
           selected = replace_object(var_name, OBJ_STRUCT);

        return in_matched = selected;
      }

   if (ucs_line.size() && (ucs_line.front() == UNI_NABLA))   // new function
      {
        where = WH_in_function;
        in_matched  = false;
        ucs_line = ucs_line.drop(1);
        UserFunction_header uh(ucs_line, false);
        const UCS_string & fun_name = uh.get_name();

        // )PCOPY: do not overwrite an already existing function/variable
        //
        bool selected = is_selected(fun_name);
        if (selected && protection &&
            Workspace::lookup_existing_symbol(fun_name))   selected = false;
        if (selected && !protection)
           selected = replace_object(fun_name, OBJ_FUNCTION);

        return in_matched = selected;
      }

   // a ∇-function written by )DUMP as 0 0⍴⎕FX ('header') ... (or with 1 ⎕FX
   // if locked), or a lambda written as ⍎'NAME←{...}' (if they contain a
   // character that UTF-8 cannot encode, Bugs39 #60). They were never
   // selected (Bugs40 #25).
   //
   {
     static const UCS_string fx_1(UTF8_string("0 0⍴⎕FX"));
     static const UCS_string fx_2(UTF8_string("0 0⍴1 ⎕FX"));
     static const UCS_string execute(UTF8_string("⍎'"));
     const bool is_fx = ucs_line.starts_with(fx_1) ||
                        ucs_line.starts_with(fx_2);
     if (is_fx || ucs_line.starts_with(execute))
        {
          // the first quoted string: the header resp. NAME←{...
          //
          UCS_string text;
          ShapeItem q = 0;
          while (q < ucs_line.ssize() && ucs_line[q] != UNI_SINGLE_QUOTE) ++q;
          for (++q; q < ucs_line.ssize(); ++q)
              {
                if (ucs_line[q] == UNI_SINGLE_QUOTE)
                   {
                     if (q + 1 < ucs_line.ssize() &&
                         ucs_line[q + 1] == UNI_SINGLE_QUOTE)   ++q;   // ''
                     else                                   break;  // end
                   }
                text << ucs_line[q];
              }

          UCS_string name;
          Obj_kind obj_kind = OBJ_FUNCTION;
          if (is_fx)
             {
               UserFunction_header uh(text, false);
               name = uh.get_name();
             }
          else
             {
               obj_kind = OBJ_LAMBDA;
               loop(t, text.size())
                  {
                    if (text[t] == UNI_LEFT_ARROW)   break;
                    name << text[t];
                  }
               name.remove_leading_and_trailing_whitespaces();
             }

          where = WH_in_variable;   // one line, ended by an empty line
          bool selected = name.size() && is_selected(name);
          if (selected && protection &&
              Workspace::lookup_existing_symbol(name))   selected = false;
          if (selected && !protection)
             selected = replace_object(name, obj_kind);
          return in_matched = selected;
        }
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
Obj_kind kind = OBJ_VALUE;
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

             // a named lambda, e.g. L←{⍺+⍵}
             //
             for (ShapeItem v = u + 1; v < ucs_line.ssize(); ++v)
                 {
                   if (ucs_line[v] == UNI_SPACE)   continue;
                   if (ucs_line[v] == UNI_L_CURLY)   kind = OBJ_LAMBDA;
                   break;
                 }

             ucs_line.resize(u);
             ucs_line.remove_leading_and_trailing_whitespaces();
             break;
           }
      }
   else                                                       // something else
      {
        return plain_copy;   // since where = WH_outside
      }

   where = WH_in_variable;   // start of a new variable
bool selected = is_selected(ucs_line);

   // )PCOPY: do not overwrite an already existing function/variable
   //
   if (selected && protection && Workspace::lookup_existing_symbol(ucs_line))
      selected = false;
   if (selected && !protection)   selected = replace_object(ucs_line, kind);

   return in_matched = selected;
}
//────────────────────────────────────────────────────────────────────────────

bool
InputFile::echo_current_file()
{
   if (files_todo.size()) return files_todo[0].echo;
   return UserPreferences::uprefs.echo_CIN ||
          ! UserPreferences::uprefs.do_not_echo;
}
//────────────────────────────────────────────────────────────────────────────
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
             // restore the ]FILE_CTL skip mode of the input source that
             // opened this file, saved at the first read of this file (see
             // IO_Files::read_file_line()). This is the end of the file in
             // every case: EOF, ]NEXTFILE, or a testcase file aborted after
             // an error.
             //
             if (files_todo[0].bom_checked)   // i.e. the mode was saved
                IO_Files::skip_mode =
                          IO_Files::Skip_mode(files_todo[0].saved_skip_mode);

             if (files_todo[0].file != stdin)
                {
                  fclose(files_todo[0].file);
                  files_todo[0].file = 0;
                  files_todo[0].line_no = -1;
                }
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
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
           {
             // not a directory (fopen() succeeds and it "passed" as an
             // empty testcase, Bugs39 #76)
             struct stat st;
             if (stat(current_filename(), &st) == 0 && S_ISDIR(st.st_mode))
                files_todo[0].file = 0;
             else
                files_todo[0].file = fopen(current_filename(), "r");
           }

        files_todo[0].line_no = 0;
        files_todo[0].bom_checked = false;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
InputFile::randomize_files()
{
#if ! MINGW_SRC
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

   // the loop below swaps only files that are not named AAA... or ZZZ...,
   // so it would never end with fewer than 2 such files (Bugs39 #18).
   //
size_t swappable = 0;
   loop(f, files_todo.size())
      {
        const char * ff = strrchr(files_todo[f].filename.c_str(), '/');
        if (!ff++)   ff = files_todo[f].filename.c_str();
        if (strncmp(ff, "AAA", 3) && strncmp(ff, "ZZZ", 3))   ++swappable;
      }
   if (swappable < 2)   return;

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
#endif // ! MINGW_SRC
}
//════════════════════════════════════════════════════════════════════════════
