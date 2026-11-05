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
    Cmd_WS:: implements the workspace-lifecycle commands
    )CLEAR, )CONTINUE, )COPY, )COPY_ONCE (aka ]COPY_ONCE), )DROP, )DUMP,
    )ERASE, )LOAD, )OUT, )SAVE, and )WSID.
*/

#include <dirent.h>
#include <errno.h>

#include <algorithm>

#include "Archive.hh"
#include "Avec.hh"
#include "Cmd_WS.hh"
#include "Command.hh"
#include "Common.hh"
#include "IO_Files.hh"
#include "LibPaths.hh"
#include "Security.hh"
#include "Sys.hh"
#include "Value.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
void
Cmd_WS::cmd_CHECK_WS(ostream & out, const UCS_string_vector & args)
{
   // Command is:
   //
   // )CHECK_WS                 (every .apl/.xml file in every present
   //                            library reference directory, see )LIBS)
   // )CHECK_WS wsname
   // )CHECK_WS libnum wsname

   if (args.size() == 0)
      {
        check_all_WS(out);
        return;
      }

const LibRef_name lib_name(out, args, false);
   if (lib_name.get_name().size() == 0)   return;   // error, )MORE set

const bool has_xml = lib_name.get_name().ends_with(".xml");
const bool has_apl = lib_name.get_name().ends_with(".apl");
int checked = 0;

   if (!has_apl)   // check the .xml file (unless an explicit .apl was given)
      {
        const UTF8_string filename = LibPaths::get_filename(lib_name, false,
                                                            ".xml", 0);
        if (access(filename.c_str(), F_OK) == 0)
           {
             ++checked;
             check_WS_file(out, filename, true);
           }
      }

   if (!has_xml)   // check the .apl file (unless an explicit .xml was given)
      {
        const UTF8_string filename = LibPaths::get_filename(lib_name, false,
                                                            ".apl", 0);
        if (access(filename.c_str(), F_OK) == 0)
           {
             ++checked;
             check_WS_file(out, filename, false);
           }
      }

   if (checked == 0)
      {
        out << lib_name.get_name() << ": no such workspace file(s)+" << endl;
        MORE_ERROR() << "neither '" << lib_name.get_name() << ".apl' nor '"
                     << lib_name.get_name() << ".xml' (nor a file with "
                        "that exact name) was found.";
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::check_WS_file(ostream & out, const UTF8_string & filename, bool is_xml)
{
   if (is_xml)
      {
        int dump_fd = -1;
        XML_Loading_Archive in(out, out, filename.c_str(), dump_fd);
        if (dump_fd != -1)   close(dump_fd);   // not really XML

        if (!in.is_open())
           out << "WARNING - " << filename << ": could not be opened"
               << endl;
        else if (dump_fd != -1)
           out << "WARNING - " << filename
               << ": not a GNU APL .xml workspace file" << endl;
        else
           in.check_checksum(out);
      }
   else
      {
        Workspace::check_DUMP_checksum(out, filename);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::check_all_WS(ostream & out)
{
   // )CHECK_WS without an argument: every library reference (0..9) that
   // maps to a present, readable directory (same present/absent test as
   // )LIBS) contributes every .apl and .xml file in that directory --
   // mirroring )LIB's own file-collection logic (Cmd_LIB.cc's LIB_common())
   // for consistency, but scoped to workspace files only (no directories).
   //
   loop(l, LIB_MAX)
      {
        const LibRef lib = LibRef(l);
        if (LibPaths::is_present(lib))   continue;   // absent/unreadable

        const UTF8_string dir_path = LibPaths::get_lib_dir(lib);
        DIR * dir = opendir(dir_path.c_str());
        if (dir == 0)   continue;   // is_present() said OK, but be safe

        UCS_string_vector filenames;
        for (;;)
            {
              const dirent * entry = readdir(dir);
              if (entry == 0)   break;
              if (entry->d_name[0] == '.')   continue;   // hidden files

              const UTF8_string filename_utf8(entry->d_name);
              if (filename_utf8.ends_with(".apl") ||
                  filename_utf8.ends_with(".xml"))
                 filenames.push_back(UCS_string(filename_utf8));
            }
        closedir(dir);

        filenames.sort();

        loop(f, filenames.size())
            {
              const UTF8_string filename(filenames[f]);
              UTF8_string full_path = dir_path;
              full_path += '/';
              full_path << filename;

              out << full_path << ":" << endl;
              check_WS_file(out, full_path, filename.ends_with(".xml"));
            }
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::cmd_CLEAR(ostream & out)
{
   Workspace::clear_WS(out, false);

   // ]FILE_CTL (see IO_Files::Skip_mode's own comment / plan.txt):
   // )CLEAR always resets skip_mode, regardless of what a script left
   // it at -- the mode is deliberately not )SAVEd/persisted.
   IO_Files::skip_mode = IO_Files::SKIP_NONE;
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::cmd_CONTINUE(ostream & out)
{
const UCS_string wsname(U"CONTINUE");
   Workspace::wsid(out, wsname, LIB_NONE, false);     // )WSID CONTINUE
const LibRef_name lib_name(wsname, false);
   Workspace::save_WS(out, lib_name, true);           // )SAVE
   Command::cmd_OFF(0);                                        // )OFF
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::cmd_COPY(ostream & out, UCS_string_vector & args, bool protection)
{
LibRef libref = LIB0;   // default library reference num to copy from

   if (args.size() == 0)   // at least workspace name is required
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "missing workspace name in command )COPY or )PCOPY";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return;
      }

   // process and skip the optional library number
   {
     const Unicode lib = args.front()[0];
     if (args.size() > 1 && Avec::is_digit(lib) && args.front().size() == 1)
        {
          libref = LibRef(lib - '0');
          args.erase(0);
        }
   }

UCS_string wsname = args.front();
   args.erase(0);
   LibRef_name lib_name(libref, wsname);
   Workspace::copy_WS(out, CERR, lib_name, args, protection);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::cmd_COPY_ONCE(ostream & out, UCS_string_vector & args)
{
   if (args.size() > 2)   // too many arguments
      {
        MORE_ERROR() << "too many parameters in command ]COPY_ONCE";
        return;
      }

LibRef libref = LIB_NONE;   // default library reference number to copy from
   if (args.size() == 0)   // no argument means: display current table
      {
        if (Command::copy_once_table.size() == 0)
           {
             out << "There were no )COPY_ONCE workspaces copied." << endl;
             return;
           }

        out << "There were " << Command::copy_once_table.size()
            << " )COPY_ONCE workspaces copied:" << endl;
        UCS_string_vector usv;
        loop(row, Command::copy_once_table.size())
            {
              const UCS_string & src = Command::copy_once_table[row];
              UCS_string ws(UNI_SPACE);
              ws << src.front();      // wsname
              ws << UNI_SPACE;
              ws << UCS_string(src, 2, src.size() - 2);
              ws << UNI_SPACE;
              usv.push_back(ws);
            }
        usv.print_table(out, 1);
        return;
      }

   // process and skip the optional library number
   //
   if (args.size() == 2)
      {
        const UCS_string & arg0 = args.front();   // first argument
        const Unicode lib = arg0.front();         // first character
        if (args.size() > 1 && Avec::is_digit(lib) && args.front().size() == 1)
           {
             libref = LibRef(lib - '0');
             args.erase(0);
           }
      }

   if (args.size() != 1)   // e.g. )COPY_ONCE ab w1 (Bugs35 #65)
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << ")COPY_ONCE: expecting [lib] wsname";
        return;
      }
const UCS_string wsname(args.front());
   args.erase(0);

   // no library number is library 0 (Bugs35 #65: they were different
   // entries in the copy_once_table, and LIB_NONE was printed as -1)
   //
   if (libref == LIB_NONE)   libref = LIB0;

   // lib_wsname is the name in the copy_once_table. Use the file that
   // wsname resolves to, so that e.g. ws and ws.xml are the same workspace
   // (Bugs37 #79).
   //
LibRef_name lib_name(libref, wsname);
const UTF8_string filename = LibPaths::get_filename(lib_name, true,
                                                    ".xml", ".apl");
const char * basename = strrchr(filename.c_str(), '/');
const UTF8_string base_utf(basename ? basename + 1 : filename.c_str());
const UCS_string file_ucs(base_utf);
UCS_string lib_wsname(Unicode('0' + char(libref)));
   lib_wsname << UNI_UNDERSCORE;
   lib_wsname << file_ucs;

   // silently return if wsname is already contained in the copy_once_table
   //
   if (Command::copy_once_table.contains(lib_wsname))   return;

   out << "NEW )COPY_ONCE workspace: ";
   if (libref)   out << libref << " ";
   out << wsname << endl;

   Workspace::copy_WS(out, CERR, lib_name, args, false);

   // add it to the table, but only if the workspace could be copied, so
   // that a failed )COPY_ONCE can be retried (Bugs37 #79)
   //
   if (access(filename.c_str(), R_OK) == 0)
      Command::copy_once_table.push_back(lib_wsname);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::cmd_DROP(ostream & out, const UCS_string_vector & lib_ws)
{
   // Command is:
   //
   // )DROP wsname
   // )DROP libnum wsname

const LibRef_name lib_name(out, lib_ws, false);
   if (lib_name.get_name().size() == 0)   return;   // error, )MORE set

const UTF8_string filename = LibPaths::get_filename(lib_name, true,
                                                    ".xml", ".apl");

const int result = unlink(filename.c_str());
   if (result)
      {
        out << lib_name.get_name() << " NOT DROPPED: "
            << strerror(errno) << "+" << endl;
        MORE_ERROR() << "could not unlink file " << filename;
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
      }
   else
      {
        Workspace::get_v_Quad_TZ().print_timestamp(out, now()) << endl;
      }
}
//════════════════════════════════════════════════════════════════════════════
void
Cmd_WS::cmd_DUMP(ostream & out, const UCS_string_vector & args,
                  bool html, bool silent)
{
   CHECK_SECURITY(disable_SAVE_command);

   // Command is:
   //
   // )DUMP
   // )DUMP workspace
   // )DUMP lib workspace

   if (args.size() > 0)   // workspace or lib workspace
      {
        const LibRef_name lib_name(out, args, false);
        if (lib_name.get_name().size() == 0)   return;   // error, )MORE set
        Workspace::dump_WS(out, lib_name, html, silent);
        return;
      }

   // )DUMP: use )WSID unless it is CLEAR WS
   //
   if (Workspace::is_CLEAR_WS())
      {
        // don't dump CLEAR WS
        //
        COUT << "NOT DUMPED: THIS WS IS CLEAR WS+" << endl;
        MORE_ERROR() <<
        "the workspace was not dumped because 'CLEAR WS' is a special\n"
        "workspace name that cannot be dumped. "
        "First create WS name with )WSID <name>.";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return;
      }

LibRef_name wsid  = Workspace::get_WSID();
   if (wsid.get_libref() == LIB_NONE)   wsid.set_libref(LIB0);
   Workspace::dump_WS(out, wsid, html, silent);
}
//════════════════════════════════════════════════════════════════════════════
void
Cmd_WS::cmd_ERASE(ostream & out, const UCS_string_vector & args)
{
   Workspace::erase_symbols(out, args);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::cmd_LOAD(ostream & out, const UCS_string_vector & args,
                  UCS_string & quad_lx, bool silent)
{
   // Command is:
   //
   // LOAD wsname
   // LOAD libnum wsname

const LibRef_name lib_name(out, args, false);
   if (lib_name.get_name().size() == 0)   return;   // error, )MORE set

   try
      {
        Workspace::load_WS(out, CERR, lib_name, quad_lx, silent);
      }
   catch (Error &)
      {
        MORE_ERROR() << "Loading workspace '" << lib_name.get_libref()
                     << " " << lib_name.get_name() << "' failed.";
      }
   catch (std::bad_alloc &)
      {
        MORE_ERROR() << "Loading workspace '" << lib_name.get_libref()
                     << " " << lib_name.get_name() << "' failed.";
        WS_FULL;
      }
   catch (...)
      { FIXME; }
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::cmd_OUT(ostream & out, UCS_string_vector & args)
{
   // )OUT filename
   // )OUT filename object...
   //
UCS_string fname = args.front();
   args.erase(0);

const LibRef_name lib_name(LIB0, fname);
const UTF8_string filename = LibPaths::get_filename(lib_name, false, ".atf", 0);

FileWriter writer(filename.c_str());
   if (!writer)
      {
        const char * why = strerror(errno);
        out << ")OUT " << fname << " failed: " << why << endl;
        MORE_ERROR() << "command )OUT: could not open file " << fname
                     << " for writing: " << why;
        return;
      }

   Workspace::write_OUT(writer.get_FILE(), args);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::cmd_SAVE(ostream & out, const UCS_string_vector & args)
{
   CHECK_SECURITY(disable_SAVE_command);

   // )SAVE
   // )SAVE workspace
   // )SAVE lib workspace

   if (args.size() > 0)   // wsname or lib wsname
      {
        // allow LIB_NONE: the new )WSID shall be what was given (Bugs35
        // #61); LIB_NONE is the same library as LIB0
        const LibRef_name lib_name(out, args, true);
        if (lib_name.get_name().size() == 0)   return;   // error, )MORE set
        Workspace::save_WS(out, lib_name, false);
        return;
      }

   // )SAVE without arguments: use )WSID unless CLEAR WS...

   if (Workspace::is_CLEAR_WS())
      {
        // don't save CLEAR WS
        COUT << "NOT SAVED, THIS WS IS CLEAR WS+" << endl;
        MORE_ERROR() <<
        "the workspace was not saved because 'CLEAR WS' is a special\n"
        "workspace name that cannot be saved. "
        "First create WS name with )WSID <name>.";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return;
      }

LibRef_name wsid =  Workspace::get_WSID();
   if (wsid.get_libref() == LIB_NONE)   wsid.set_libref(LIB0);
   Workspace::save_WS(out, wsid, true);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_WS::cmd_WSID(ostream & out, const UCS_string_vector & args)
{
   // )WSID
   // )WSID wsname
   // )WSID lib wsname
   //
LibRef lib = LIB_NONE;
UCS_string arg;   // ""
   if (args.size() == 0)        // )WSID
      {
        lib = LIB_NONE;
      }
   else if (args.size() == 1)   // )WSID wsname
      {
        arg = args.front();
      }
   else                        // )WSID lib wsname
      {
        // args.front()[0] - '0' assumed the first word was always a
        // single digit; for anything else (e.g. )WSID a b, a genuine
        // library name typo) it silently computed garbage instead --
        // 'a'-'0' = 49 -- reported downstream as "Bad library
        // reference '49'", which looks like a real (if out-of-range)
        // number the user typed rather than the actual problem (the
        // first word isn't a library digit at all). Report the actual
        // bad word directly instead. See Bugs27 #59(j).
        //
        if (args.front().size() != 1 || !Avec::is_digit(args.front()[0]))
           {
             out << "Bad library reference '" << args.front() << "'" << endl;
             return;
           }
        lib = LibRef(args.front()[0] - '0');
        arg = args[1];   // ""
      }
   Workspace::wsid(out, arg, lib, false);
}
//════════════════════════════════════════════════════════════════════════════
