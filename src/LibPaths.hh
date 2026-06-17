/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2015  Dr. Jürgen Sauermann

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

#ifndef __LIBPATHS_HH_DEFINED__
#define __LIBPATHS_HH_DEFINED__

#include "UCS_string.hh"
#include "UTF8_string.hh"

class UCS_string_vector;

//----------------------------------------------------------------------------

/// a library reference number for commands )LOAD, )SAVE, and )COPY.
/// No library reference number is the same as LIB0.
enum LibRef
{
   LIB_NONE = -1,    ///< no library reference specified.
   LIB0 = 0,         ///< library 0
   LIB1 = 1,         ///< library 1
   LIB2 = 2,         ///< library 2
   LIB3 = 3,         ///< library 3
   LIB4 = 4,         ///< library 4
   LIB5 = 5,         ///< library 5
   LIB6 = 6,         ///< library 6
   LIB7 = 7,         ///< library 7
   LIB8 = 8,         ///< library 8
   LIB9 = 9,         ///< library 9
   LIB_MAX,          ///< valid library references are smaller than this
};
//----------------------------------------------------------------------------
/// a library reference number (0..9) and a workspace name
class LibRef_name
{
public:
  /// constructor (from WS name without libref)
  LibRef_name(const UCS_string & wsname, bool allow_LIB_NONE)
  : lib(allow_LIB_NONE ? LIB_NONE : LIB0),
    name(wsname)
  {}

  /// constructor (from WS name with libref)
  LibRef_name(LibRef wslib, const UCS_string & wsname)
  : lib(wslib),

    name(wsname)
  {}

   /// constructor from an optional library reference number (0-9),
   /// followed by the mandatory WS name. Set name == "" on error.
   LibRef_name(ostream & out, const UCS_string_vector & args,
               bool allow_LIB_NONE);

   /// return the workspace name
  const UCS_string & get_name() const
     { return name; }

   /// return the library reference
  LibRef get_libref() const
     { return lib; }

   /// set the library reference
   void set_libref(LibRef new_lib)
      { lib = new_lib; }

protected:
  LibRef lib;
  UCS_string name;
};
//----------------------------------------------------------------------------
/// a class mapping library reference numbers to directories
class LibPaths
{
public:
   /// one library directory
   struct LibDir
      {
        /// constructor: unspecified LibDir
        LibDir()
        : cfg_src(CSRC_NONE)
        {}

        /// the path (name) of the lib directory
        UTF8_string dir_path;

        /// how a dir_path was computed
        enum CfgSrc
           {
             CSRC_NONE      = 0,   ///< not at all
             CSRC_ENV       = 1,   ///< lib root from env. variable APL_LIB_ROOT
             CSRC_PWD       = 2,   ///< lib root from current directory
             CSRC_PREF_SYS  = 3,   ///< path from preferences file below /etc/
             CSRC_PREF_HOME = 4,   ///< path from preferences file below $HOME
             CSRC_CMD       = 5,   ///< lib root from )LIBS command
           };

        /// how this->dir_path was computed
        CfgSrc cfg_src;
      };

   /// initialize library paths based on the location of the APL binary
   static void init(const char * argv0, bool logit);

   /// return the path (directory) of the APL interpreter binary
   static const char * get_APL_bin_path()   { return APL_bin_path; }

   /// return the name (without directory) of the APL interpreter binary
   static const char * get_APL_bin_name()   { return APL_bin_name; }

   /// return directory containing (file or directory) workspaces and wslib1
   static const char * get_APL_lib_root()   { return APL_lib_root; }

   /// set library root to \b new_root
   static void set_APL_lib_root(const char * new_root);

   /// set library path (from config file)
   static void set_lib_dir(LibRef lib, const UTF8_string & path,
                           LibDir::CfgSrc src);

   /// return 0 if directory \b lib) is present, otherwise the reason why not.
   static const char * is_present(LibRef lib);

   /// return library path (from config file or from libroot)
   static UTF8_string get_lib_dir(LibRef lib);

   /// return source that configured \b this entry
   static LibDir::CfgSrc get_cfg_src(LibRef lib)
      { return lib_dirs[lib].cfg_src; }

   /// return the full path for lib/file \b lib_name, possibly adding
   /// the .ext1 or the .ext2 extension (if not provided)
   static UTF8_string get_filename(const LibRef_name & lib_name, bool existing,
                                   const char * ext1, const char * ext2);

protected:
   /// maybe warn the user if two files that differ only by extension exist
   static void maybe_warn_ambiguous(int name_has_extension,
                                    const UTF8_string name,
                                    const char * ext1, const char * ext2);

   /// compute the location of the apl binary
   static void compute_bin_path(const char * argv0, bool logit);

   /// set library root, searching from APL_bin_path
   static void search_APL_lib_root();

   /// return true if directory \b dir contains two (files or sub-directories)
   /// workspaces and wslib1
   static bool is_lib_root(const char * dir);

   /// the path (directory) of the APL interpreter binary
   static char APL_bin_path[];

   /// the name (without directory) of the APL interpreter binary
   static const char * APL_bin_name;

   /// a directory containing sub-directories workspaces and wslib1
   static char APL_lib_root[];

   /// directories for each library reference as specified in file preferences
   static LibDir lib_dirs[LIB_MAX];

   /// true if APL_lib_root was computed from environment variable APL_LIB_ROOT
   static bool root_from_env;

   /// true if APL_lib_root was not found (thus "." taken as fallback)
   static bool root_from_pwd;
};
//----------------------------------------------------------------------------
#endif // __LIBPATHS_HH_DEFINED__
