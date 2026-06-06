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

#include <sys/stat.h>

#include "config.h"

#include "Common.hh"
#include "Command.hh"

//════════════════════════════════════════════════════════════════════════════
void
Cmd_LIB::cmd_LIBS(ostream & out, const UCS_string_vector & args)
{
   // Command is one of:
   //
   // 1a. )LIBS N path          (set libdir N to path)
   // 1b. )LIBS N ?             (display the state of libdir N
   // 2.  )LIBS path            (set libroot to path)
   // 3. )LIBS                 (display root and path states)
   //
   //
const bool query = args.size() &&
           args.back().size()  &&
           args.back()[0] == UNI_QUESTION;

   if (args.size() == 2)   // cases 1a. or 1b.: set or query single libdir
      {
        const UCS_string & libref_ucs = args.front();
        const LibRef libref = LibRef(libref_ucs.front() - '0');
        if (libref_ucs.size() != 1 || libref < 0 || libref > 9)
           {
             CERR << "Invalid library reference '" << libref_ucs << "'"
                     ". Valid library references are the digits 0..9." << endl;
             return;
           }

        if (query)
           {
             out << "library reference " << libref << " stands for directory "
                 << LibPaths::get_lib_dir(libref) << endl
                 << "    That directory ";
             if (const char * cause = LibPaths::is_present(libref))
                out << "is not accessible (" << cause << ").\n"
                       "    See command )LIBS without arguments for details.\n";
             else
                out << "exists.\n";
           }
        else
           {
             UTF8_string path(args[1]);
             LibPaths::set_lib_dir(libref, path, LibPaths::LibDir::CSRC_CMD);
             out << "LIBRARY REFERENCE " << libref << " SET TO " << path << endl;
           }
        return;
      }

   if (args.size() == 1)   // set root
      {
        UTF8_string utf(args.front());
        LibPaths::set_APL_lib_root(utf.c_str());
        out << "LIBRARY ROOT SET TO " << args.front() << endl;
        return;
      }

   out << "Library root: " << LibPaths::get_APL_lib_root() << 
"\n"
"\n"
"Library reference number (Ref) to (absolute) path mapping:\n"
"\n"
u8"╔═══╤═════╤═════════════╤══════════════════════════════════════════════════════╗\n"
u8"║Ref│Conf │State (errno)│ Path to the directory containing the workspace files ║\n"
u8"╟───┼─────┼─────────────┼──────────────────────────────────────────────────────╢\n";

   loop(d, 10)
       {
          out << u8"║ " << d << u8" │";
          UTF8_string path = LibPaths::get_lib_dir(LibRef(d));
          switch(LibPaths::get_cfg_src(LibRef(d)))
             {
                case LibPaths::LibDir::CSRC_NONE:      out << u8"NONE │" << endl;
                                                       continue;
                case LibPaths::LibDir::CSRC_ENV:       out << u8"ENV  │";   break;
                case LibPaths::LibDir::CSRC_PWD:       out << u8"PWD  │";   break;
                case LibPaths::LibDir::CSRC_PREF_SYS:  out << u8"PSYS │";   break;
                case LibPaths::LibDir::CSRC_PREF_HOME: out << u8"PUSER│";   break;
                case LibPaths::LibDir::CSRC_CMD:       out << u8"CMD  │";   break;
             }

        if (DIR * dir = opendir(path.c_str()))
           { out << u8" present     │ ";   closedir(dir); }
        else
           {
             char cc[10];
             SPRINTF(cc, "(%u)", errno);
             out << " missing " << setw(4) << cc << u8"│ ";
           }

        out << left << setw(53) << path.c_str() << u8"║\n";
      }

   out <<
u8"╚═══╧══╤══╧═════════════╧══════════════════════════════════════════════════════╝\n"
u8"       │\n"
u8"       ├── NONE:  found no method to compute the library path\n"
u8"       ├── CMD:   the path was set with )LIBS N path\n"
u8"       ├── ENV:   the path came from environment variable $APL_LIB_ROOT\n"
u8"       ├── PSYS:  the path came from the system preferences in file\n"
u8"       │                   " << apl_DIR__sysconf << "/gnu-apl.d/preferences\n"
u8"       ├── PUSER: the path came from user preferences in file\n"
u8"       │                   $HOME/.config/gnu-apl or $HOME/.gnu-apl\n"
u8"       └── PWD:   the path is relative to current directory $PWD (last resort)"
       << endl;

   out << right;   // restore the default
}
//────────────────────────────────────────────────────────────────────────────
DIR *
Cmd_LIB::open_LIB_dir(UTF8_string & path, ostream & out,
                      const UCS_string_vector & args)
{
   /* args should be one of:
                                   example:
      1.                           )LIB
      2.  N                        )LIB 1
      3.  )LIB directory-name      )LIB /usr/lib/...
    */

UCS_string arg(UNI_0);
   if (args.size())   arg = args.front();

   if (args.size() == 0)                            // case 1.
      {
        path = LibPaths::get_lib_dir(LIB0);
      }
   else if (arg.size() == 1 &&
            Avec::is_digit(Unicode(arg.front())))   // case 2.
      {
        path = LibPaths::get_lib_dir(LibRef(arg.front() - '0'));
      }
   else                                             // case 3.
      {
        path = UTF8_string(arg);
      }

   // follow symbolic links, but not too often (because symbolic links are
   // subject to creating an endless loop)...
   //
#if ! MINGW_SRC
   loop(depth, 20)
       {
         char buffer[FILENAME_MAX + 1];
         const ssize_t len = readlink(path.c_str(), buffer, FILENAME_MAX);
         if (len <= 0)   break;   // not a symlink

         buffer[len] = 0;
         if (buffer[0] == '/')   // absolute path
            {
              path = UTF8_string(buffer);
            }
          else                   // relative path
            {
              path += '/';
              path << UTF8_string(buffer);
            }
       }
#endif // ! MINGW_SRC

   if (DIR * dir = opendir(path.c_str()))   return dir;   // OK

   // error opening path
   //
const char * why = strerror(errno);
   out << "IMPROPER LIBRARY REFERENCE '" << arg << "': " << why << endl;

   MORE_ERROR() << "Could not open the directory '" << path << ".\n"
                << "Reason: " << why;
   return 0;   // error
}
//────────────────────────────────────────────────────────────────────────────
bool
Cmd_LIB::is_directory(const dirent * entry, const UTF8_string & path)
{
#ifdef _DIRENT_HAVE_D_TYPE
   return entry->d_type == DT_DIR;
#endif

UTF8_string filename = path;
UTF8_string entry_name(entry->d_name);
   filename += '/';
   filename <<entry_name;

DIR * dir = opendir(filename.c_str());
   if (dir) closedir(dir);
   return dir != 0;
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LIB::LIB_common(ostream & out, const UCS_string_vector & cmd_args, bool dbg)
{
   // check for (and then extract) optional range and sort parameters...
   //
UCS_string_vector args;
const UCS_string * range = 0;
SORT_ORDER sort = SORT_NONE;
   loop(a, cmd_args.size())
      {
        const UCS_string & arg = cmd_args[a];
        if (arg.size() == 5 && arg.starts_iwith("-size"))
           {
             sort = SORT_SIZE;
             continue;
           }

        if (arg.size() == 5 && arg.starts_iwith("-time"))
           {
             sort = SORT_TIME;
             continue;
           }

        // at this point, arg could be a range (e.g. A-F), or a
        // path (e.g. ./file.apl), or simply a WSID. Assume a WSID,
        //
        bool is_range = false;
        bool is_path = false;
        loop(aa, arg.size())
            {
              const Unicode uni = arg[aa];
              if (uni == UNI_MINUS)
                 {
                   if (!is_path)   is_range = true;
                   break;
                 }

              if (!Avec::is_symbol_char(uni))
                 {
                   is_path = true;
                   break;
                 }
            }

         if (is_path)   // arg is a filename
            {
              args.push_back(arg);
            }
         else if (!is_range)   // arg is a WSID
            {
              args.push_back(arg);
            }
         else if (range)   // second non-range arg
            {
              MORE_ERROR() <<
              "multiple range parameters in )LIB or ]LIB command";
              return;
            }
         else
            {
              range = &arg;
            }
      }

UCS_string from;
UCS_string to;
   if (range)
      {
        if (Command::parse_from_to(from, to, *range))
           {
             CERR << "bad range argument" << endl;
             MORE_ERROR() << "bad range argument " << *range
                  << ", expecting from-to";
             return;
           }
      }

   // 2. open directory
   //
UTF8_string path;
DIR * dir = open_LIB_dir(path, out, args);
   if (dir == 0)   return;

   // 3. collect the WS files and sub-directories in the )LIBS N directory
   //
UCS_string_vector files;
UCS_string_vector directories;

   for (;;)
       {
         const dirent * entry = readdir(dir);
         if (entry == 0)   break;   // directory loop done
         const size_t dlen = strlen(entry->d_name);
         if (entry->d_name[0] == '.')   continue;   // ignore hidden files

         const UTF8_string filename_utf8(entry->d_name);
         UCS_string filename(filename_utf8);

         // check the range of the name (if any)...
         //
         if (!filename.is_in_range(from, to))   continue;

         if (is_directory(entry, path))
            {
              filename << UNI_SLASH;
              directories.push_back(filename);
              continue;
            }

         if (filename[dlen - 1] == '~')   continue;  // editor backup file

         if (dbg)   // ]LIB ...
            {
              files.push_back(filename);
            }
         else       // )LIB ...
            {
              if (filename_utf8.ends_with(".apl"))
                 {
                   files.push_back(filename);
                 }
              else if (filename_utf8.ends_with(".xml"))
                 {
                   files.push_back(filename);
                 }
            }
       }
   closedir(dir);

   // 4. sort dirctories and files alphabetically
   //
   directories.sort();   // sort directories alphabetically
   files.sort();         // sort files alphabetically

   // 5. print the directories, then the files
   //
   if (sort)   LIB_print_sorted(out, path, directories, files, sort);
   else        LIB_print_flat(out, path, directories, files);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LIB::LIB_print_flat(ostream & out, const UTF8_string lib_path,
                     const UCS_string_vector & directories,
                     const UCS_string_vector & files)
{
   // 1. start with directories and append files, removing duplicates
   //    file names (caused by .apl and .xml extensions). After that
   //    all names are in directories with the extensions removed.
   //
UCS_string_vector all_names;
   loop(d, directories.size())   all_names.push_back(directories[d]);
   loop(f, files.size())
      {
        const UCS_string filename(files[f], 0, files[f].size() - 4);
        if (all_names.size() && all_names.back() == filename)
           {
             // this happens when thetre is both an .apl and an .xml file.
             // Skip the second // to avoid duplicate file names. Otherwise
             // append the name to all.
             //
             continue;
           }
        all_names.push_back(filename);
      }

   // At this point, all_names contains all names, with directories
   // before files and duplicate names removed.
   //
        
   // figure column widths
   //
   enum { tabsize = 4 };

std::vector<int> col_widths;
   all_names.compute_column_width(tabsize, col_widths);

   loop(c, all_names.size())
      {
        const size_t col = c % col_widths.size();
        out << all_names[c];
        if (col == size_t(col_widths.size() - 1) ||
              c == ShapeItem(all_names.size() - 1))
           {
             // last column or last item: print newline
             //
             out << endl;
           }
        else
           {
             // intermediate column: print spaces
             //
             const int len = tabsize*col_widths[col] - all_names[c].size();
             Assert(len > 0);
             loop(l, len)   out << " ";
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LIB::LIB_print_sorted(ostream & out, const UTF8_string lib_path,
                          const UCS_string_vector & directories,
                          const UCS_string_vector & files, SORT_ORDER sort)
{
const size_t max_dir_name = directories.max_width(0, 1);
const size_t max_file_name = files.max_width(0, 1);
const size_t max_name = max(max_dir_name, max_file_name);

   // print directories
   //
   loop(d, directories.size())
       {
         const size_t blanks = max_name - directories[d].size() + 9;
         out << directories[d] << string(blanks, ' ') << "(DIR)" << endl;
       }

   if (files.size() == 0)   return;

   // re-sort files by size or by time
   //
UCS_string_vector sorted_files;
vector<size_t> sorted_props;
vector<int> file_properties;   // the properties for sorting
vector<bool> appended;         // appended[f] is true if file[f] was appended
   loop(f, files.size())
       {
         const size_t property = sort_property(sort, lib_path, files[f]);
         file_properties.push_back(property);
         appended.push_back(false);
       }

   // find the index of the smallest sort_property and append its
   // corresponding name
   //
   Assert(file_properties.size() == size_t(files.size()));
   while (sorted_files.size() < files.size())   // until done
      {
        int smallest_unused = -1;
        loop(f, files.size())
            {
              if (appended[f])   continue;   // already appended
              if (smallest_unused == -1 ||
                  file_properties[f] < file_properties[smallest_unused])
                 {
                  smallest_unused = f;
                 }
            }

        // at this point the smallest not yet appended index was found
        //
        sorted_files.push_back(files[smallest_unused]);
        sorted_props.push_back(file_properties[smallest_unused]);
        appended[smallest_unused] = true;   // mark it as appended
      }

   loop(f, sorted_files.size())
       {
         const size_t blanks = max_name - sorted_files[f].size();
         out << sorted_files[f] << string(blanks, ' ');

         if (sort == SORT_SIZE)
            {
               out << setw(8) << sorted_props[f] << " bytes" << endl;
            }
         else if (sort == SORT_TIME)
            {
               const time_t when = sorted_props[f];
               out << "  " << ctime(&when);   // ctime() does '\n'
            }
         else FIXME;
       }
}
//────────────────────────────────────────────────────────────────────────────
size_t
Cmd_LIB::sort_property(SORT_ORDER sort, const UTF8_string & lib_path,
                       const UCS_string & wsid)
{
   Assert(sort != SORT_NONE);   // 1 or 2

UTF8_string wsid_utf8(wsid);
UTF8_string name(lib_path);   // e.g. /home/workspaces
   name += '/';               //      /home/workspaces/
   name << wsid_utf8;         //      /home/workspaces/wsid

   if (access(name.c_str(), R_OK) == 0)   // file is readable
      {
        struct stat st;
        if (stat(name.c_str(), &st) == 0)   // got stat
           {
             if (sort == SORT_SIZE)   return st.st_size;
             if (sort == SORT_TIME)   return st.st_mtime;
             else                          FIXME;
           }
      }

   return 0;   // invalid
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LIB::cmd_LIB1(ostream & out, const UCS_string_vector & args)
{
   /* Command is:

      )LIB [N] [RANGE] [sort]
   
      where:

      N is an optional library number (0-9, default 0)
      RANGE is a range for the file names (two ASCII characters A-Z)
      sort is a sorting order: -T (sort by time) or -S (sort by size)
    */

   LIB_common(out, args, false);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LIB::cmd_LIB2(ostream & out, const UCS_string_vector & args)
{
   /* Command is:

      ]LIB [N] [RANGE] [sort]
   
      where:

      N is an optional library number (0-9, default 0)
      RANGE is a range for the file names (two ASCII characters A-Z)
      sort is a sorting order: -sT (sort by time) or -sS (sort by size)
    */

   // The difference between cmd_LIB1 and cmd_LIB2 is the output
   // channel, i.e. )LIB vs. ]LIB.

   LIB_common(out, args, true);
}
//════════════════════════════════════════════════════════════════════════════
