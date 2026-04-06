/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2013-2015  Dr. Jürgen Sauermann

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "Error.hh"
#include "LibPaths.hh"
#include "PrintOperator.hh"
#include "UCS_string_vector.hh"
#include "Workspace.hh"

bool LibPaths::root_from_env = false;
bool LibPaths::root_from_pwd = false;

char LibPaths::APL_bin_path[APL_PATH_MAX + 1] = "";
const char * LibPaths::APL_bin_name = LibPaths::APL_bin_path;
char LibPaths::APL_lib_root[APL_PATH_MAX + 10] = "";

LibPaths::LibDir LibPaths::lib_dirs[LIB_MAX];

// realpath() complains if its result is not used, To avoid that warning
// we assign the result to variable unused below.
//
const void * unused = 0;

//----------------------------------------------------------------------------
void
LibPaths::init(const char * argv0, bool logit)
{
   logit && CERR << "\ninitializing paths from argv[0] = " << argv0 << endl;

   compute_bin_path(argv0, logit);
   search_APL_lib_root();

   loop(d, LIB_MAX)
      {
        if      (root_from_env)   lib_dirs[d].cfg_src = LibDir::CSRC_ENV;
        else if (root_from_pwd)   lib_dirs[d].cfg_src = LibDir::CSRC_PWD;
        else                      lib_dirs[d].cfg_src = LibDir::CSRC_NONE;
      }
}
//----------------------------------------------------------------------------
void
LibPaths::compute_bin_path(const char * argv0, bool logit)
{
   // compute APL_bin_path from argv0
   //
   if (strchr(argv0, '/') == 0)
      {
         // if argv0 contains no / then realpath() seems to prepend the current
         // directory to argv0 (which is wrong since argv0 may be in $PATH).
         //
         // we fix this by searching argv0 in $PATH
         //
         if (const char * paths = getenv("PATH"))
            {
              logit && CERR << "initializing paths from  $PATH = "
                            << paths << endl;

              while (*paths)
                    {
                      size_t dir_len;
                      if (const char * colon = strchr(paths, ':'))
                         dir_len = colon - paths;
                      else
                         dir_len = strlen(paths);

                    std::string filename(paths, dir_len);
                    filename += '/';
                    filename.append(argv0);
                    if (access(filename.c_str(), X_OK) == 0)
                       {
                         strncpy(APL_bin_path, filename.c_str(),
                                 sizeof(APL_bin_path) - 1);
                         APL_bin_path[sizeof(APL_bin_path) - 1] = 0;

                         char * slash =   strrchr(APL_bin_path, '/');
                         Assert(slash);   // due to %s/%s above
                         *slash = 0;
                         APL_bin_name = slash + 1;
                         goto done;
                       }

                    paths += dir_len + 1;   // next $PATH item
                  }
            }
           else
            {
              logit && CERR << "initializing paths from $PATH failed because "
                               "it was not set" << endl;
            }
      }

#if MINGW_SRC
   strncpy(APL_bin_path, argv0, sizeof(APL_bin_path) - 1);
#else // ! MINGW_SRC
   unused = realpath(argv0, APL_bin_path);
#endif // // ! MINGW_SRC
   APL_bin_path[APL_PATH_MAX] = 0;
   {
     char * slash = strrchr(APL_bin_path, '/');
     if (slash)   { *slash = 0;   APL_bin_name = slash + 1; }
     else         { APL_bin_name = APL_bin_path;            }
   }

   // if we have a PWD and it is a prefix of APL_bin_path then replace PWD
   // by './'
   //
   if (const char * PWD = getenv("PWD"))   // we have a pwd
      {
        logit && CERR << "initializing paths from  $PWD = " << PWD << endl;
        const int PWD_len = strlen(PWD);
        if (!strncmp(PWD, APL_bin_path, PWD_len) && PWD_len > 1)
           {
             strcpy(APL_bin_path + 1, APL_bin_path + PWD_len);
             APL_bin_path[0] = '.';
           }
      }
   else
      {
        logit && CERR << "initializing paths from $PWD failed because "
                         "it was not set" << endl;
      }

done:
   logit && CERR << "APL_bin_path is: " << APL_bin_path << endl
                 << "APL_bin_name is: " << APL_bin_name << endl;
}
//----------------------------------------------------------------------------
bool
LibPaths::is_lib_root(const char * dir)
{
char filename[APL_PATH_MAX];

   SPRINTF(filename, "%s/workspaces", dir);
   if (access(filename, F_OK))   return false;

   SPRINTF(filename, "%s/wslib1", dir);
   if (access(filename, F_OK))   return false;

   return true;
}
//----------------------------------------------------------------------------
void
LibPaths::search_APL_lib_root()
{
   APL_lib_root[0] = 0;

   if (const char * path = getenv("APL_LIB_ROOT"))
      {
        set_APL_lib_root(path);
        root_from_env = true;
        return;
      }

   root_from_pwd = true;

   // search from "." to "/" for  a valid lib-root
   //
   set_APL_lib_root(".");
   while (strlen(APL_lib_root))
      {
        if (is_lib_root(APL_lib_root))   return;   // lib-root found
        if (char * s = strrchr(APL_lib_root, '/'))         *s = 0;
        else if (char * s = strrchr(APL_lib_root, '\\'))   *s = 0;
        else
           {
             CERR << "*** Cannot locate APL_lib_root: no / or \\ in "
                  << APL_lib_root << endl;
             break;
           }
      }

   set_APL_lib_root(".");
}
//----------------------------------------------------------------------------
void
LibPaths::set_APL_lib_root(const char * new_root)
{
#if MINGW_SRC
   strncpy(APL_lib_root, ".", sizeof(APL_lib_root) - 1);
#else // ! MINGW_SRC
   unused = realpath(new_root, APL_lib_root);
#endif // ! MINGW_SRC
}
//----------------------------------------------------------------------------
void
LibPaths::set_lib_dir(LibRef libref, const UTF8_string & path,
                      LibDir::CfgSrc src)
{
   lib_dirs[libref].dir_path = path;
   lib_dirs[libref].cfg_src = src;
}
//----------------------------------------------------------------------------
UTF8_string
LibPaths::get_lib_dir(LibRef libref)
{
   switch(lib_dirs[libref].cfg_src)
      {
        case LibDir::CSRC_NONE:      return UTF8_string();

        case LibDir::CSRC_ENV:
        case LibDir::CSRC_PWD:       break;   // continue below

        case LibDir::CSRC_PREF_SYS:
        case LibDir::CSRC_PREF_HOME:
        case LibDir::CSRC_CMD:       return lib_dirs[libref].dir_path;
      }

UTF8_string ret(APL_lib_root);
   if (libref == LIB0)   // workspaces
      {
        ret << "/workspaces";
      }
   else                  // wslibN
      {
        (ret << "/wslib") += (libref + '0');
      }

   return ret;
}
//----------------------------------------------------------------------------
void
LibPaths::maybe_warn_ambiguous(int has_extension, const UTF8_string name,
                               const char * ext1, const char * ext2)
{
   if (has_extension)   return;   // extension was provided
   if (ext2 == 0)       return;   // no second extension

UTF8_string filename_ext2 = name;
   filename_ext2 << ext2;
   if (access(filename_ext2.c_str(), F_OK))   return;   // not existing

   CERR << endl 
        << "WARNING: filename " << name << endl
        << "    is ambiguous because another file" << endl << "    "
        << filename_ext2 << endl
        << "    exists as well. Using the first (.xml) file." << endl << endl;
}
//----------------------------------------------------------------------------
UTF8_string
LibPaths::get_filename(const LibRef_name & lib_name, bool existing,
                       const char * ext1, const char * ext2)
{
   // check if name has one of the extensions ext1 or ext2 already.
   //
int has_extension = 0;   // assume name has neither extension ext1 nor ext2
   if      (lib_name.get_name().ends_with(ext1))           has_extension = 1;
   else if (ext2 && lib_name.get_name().ends_with(ext2))   has_extension = 2;

   if (lib_name.get_name().starts_with("/")   || 
       lib_name.get_name().starts_with("./")  || 
       lib_name.get_name().starts_with("../"))
      {
        // paths from / or ./ are fallbacks for the case where the library
        // path setup is wrong. So that the user can survive by using an
        // explicit path
        //
        if (has_extension)   return lib_name.get_name();

        UTF8_string filename(lib_name.get_name());
        if (access(filename.c_str(), F_OK) == 0)   return filename;

        if (ext1)
           {
             UTF8_string filename_ext1 = lib_name.get_name();
             filename_ext1 << ext1;
             if (!access(filename_ext1.c_str(), F_OK))
                {
                   // filename_ext1 exists, but filename_ext2 may exist as well.
                   // warn user unless an explicit extension was given.
                   //
                   maybe_warn_ambiguous(has_extension, lib_name.get_name(),
                                        ext1, ext2);
                   return filename_ext1;
                }
           }

        if (ext2)
           {
             UTF8_string filename_ext2 = lib_name.get_name();
             filename_ext2 << ext2;
             if (!access(filename_ext2.c_str(), F_OK))   return filename_ext2;
           }

         // neither ext1 nor ext2 worked: return original name
         //
         filename = lib_name.get_name();
         return filename;
      }

UTF8_string filename = get_lib_dir(lib_name.get_libref());
   filename += '/';
   filename << lib_name.get_name();

   if (has_extension)   return filename;

   if (existing)
      {
        // file ret is supposed to exist (and will be openend read-only).
        // If it does return filename otherwise filename.extension.
        //
        if (access(filename.c_str(), F_OK) == 0)   return filename;

        if (ext1)
           {
             UTF8_string filename_ext1 = filename;
             filename_ext1 << ext1;
             if (!access(filename_ext1.c_str(), F_OK))
                {
                  maybe_warn_ambiguous(has_extension,
                                       filename, ext1, ext2);
                   return filename_ext1;
                }
           }

        if (ext2)
           {
             UTF8_string filename_ext2 = filename;
             filename_ext2 << ext2;
             if (!access(filename_ext2.c_str(), F_OK))   return filename_ext2;
           }

        return filename;   // without ext
      }
   else
      {
        // file may or may not exist (and will be created if not).
        // therefore checking the existence does not work.
        // check that the file ends with ext1 or ext2 if provided
        //
        if (has_extension)   return filename;

        if      (ext1) filename << ext1;
        else if (ext2) filename << ext2;
        return filename;
      }
}
//----------------------------------------------------------------------------
const char *
LibPaths::is_present(LibRef lib)
{
   // try to open dir and return the reason if that fails
   //
UTF8_string path = LibPaths::get_lib_dir(lib);
DIR * dir = opendir(path.c_str());
   if (dir)   // success
      {
        closedir(dir);
        return 0;
      }
   else
      {
        return strerror(errno);
      }

}
//============================================================================
LibRef_name::LibRef_name(ostream & out, const UCS_string_vector & args,
                         bool allow_LIB_NONE)
{
   Assert(args.size() > 0);
   if (args.size() == 1)   // workspace name without library reference number
      {
        lib  = allow_LIB_NONE ? LIB_NONE : LIB0;
        name = args.front();
        return;   // OK
      }

   // name with library reference number (0-9)
   //
const UCS_string & libref = args.front();
   if (libref.size() == 1 && Avec::is_digit(libref[0]))
      {
        lib  = LibRef(args.front()[0] - '0');
        name = args[1];
        return;   // OK
      }

   out << "BAD COMMAND+" << endl;
   MORE_ERROR() << "invalid library reference '" << args.front() << "'";
   if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;

   // error (lib_name.name is "").
}
//============================================================================

