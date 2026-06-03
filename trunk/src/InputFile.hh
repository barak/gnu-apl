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

#ifndef __INPUT_FILE_HH_DEFINED__
#define __INPUT_FILE_HH_DEFINED__

#include "UTF8_string.hh"
#include "UCS_string_vector.hh"

//════════════════════════════════════════════════════════════════════════════
/** an input file and its properties. The file can be an apl script(.apl) file
    or a testcase (.tc) file. The file names initially come from the command
    line, but can be extended by )COPY commands 
 */
/// A single input file to be executed by the interpreter
struct InputFile
{
   friend class IO_Files;

   /// default constructor (for vector<InputFile>).
   InputFile() {}

   /// Normal constructor
   /// @param _filename path of the input file
   /// @param _file open FILE pointer (or 0 if not yet open)
   /// @param _test true if this is a testcase (.tc) file
   /// @param _echo true if input lines should be echoed
   /// @param _is_script true if the file is an APL script
   /// @param LX ⎕LX execution mode after the file is processed
   InputFile(const UTF8_string & _filename, FILE * _file,
             bool _test, bool _echo, bool _is_script, LX_mode LX)
   : file     (_file),
     filename (_filename),
     test     (_test),
     pushed_IE(false),
     pushed_pending(false),
     echo     (_echo),
     is_script(_is_script),
     with_LX  (LX),
     line_no  (0),
     in_html  (0),
     from_COPY(false),
     file_seq(++next_file_seq)
   {}

   /// a line-by-line state machine for filtering objects that shall be copied.
   struct COPY_filter
      {
        /// constructor
        COPY_filter()
        : where(WH_outside),
          in_matched(false)
        {}

        /// add \b object to \b this object_filter
        /// @param object APL name to include in the copy filter
        void add_filter_object(UCS_string & object)
           {
             object_filter.push_back(object);
           }

        /// return \b true if this object filter is valid
        /// (from )COPY file names...)
        bool has_object_filter() const
           { return object_filter.size(); }

        /// check the current line and return true if the line is permitted by
        /// the object_filter. Also, update \b in_function and \b in_variable.
        /// @param line the UTF-8 source line to evaluate against the filter
        bool check_filter(const UTF8_string & line);

        // where the current (and subsequent) line(s) are located
        enum
           {
             WH_outside     = 0,   ///< lines are outside any functions or var
             WH_in_function = 1,   ///< lines belong to a function
             WH_in_variable = 2    ///< lines belong to a variable
           } where;

        /// true if the current function or variable was mentioned in
        /// object_filter
        bool in_matched;

        /// the functions and variiables that shall be )COPIED
        UCS_string_vector object_filter;
      };

   /// return the current HTML mode
   int get_html() const
      { return in_html; }

   /// return the sequence number of the current file (if any), 0 if not.
   static int32_t get_file_seq()
      { return files_todo.size() ? files_todo[0].file_seq : 0; }

   /// the current file
   static InputFile * current_file()
      { return files_todo.size() ? &files_todo[0] : 0; }

   /// the name of the currrent file
   static const char * current_filename()
      { return files_todo.size() ? files_todo[0].filename.c_str() : "stdin"; }

   /// the line number of the currrent file
   static int current_line_no()
      { return files_todo.size() ? files_todo[0].line_no : stdin_line_no; }

   /// return true iff the current input file exists and is a pushed
   /// immediate execution
   static bool is_pushed_pending()
      { return files_todo.size() && files_todo[0].pushed_pending; }

   /// return true iff the current input file exists and is a test file
   static bool is_validating()
      { return files_todo.size() && files_todo[0].test; }

   /// return true iff the current input file exists and is a pushed
   /// immediate execution
   static bool pushed_file()
      { return files_todo.size() && files_todo[0].pushed_IE; }

   /// return true iff input comes from a script (as opposed to running
   /// interactively)
   static bool running_script()
      { return files_todo.size() && files_todo[0].is_script; }

   /// the number of testcase (.tc) files
   static int testcase_file_count()
      {
        int count = 0;
        loop(f, files_todo.size())
            {
              if (files_todo[f].test)   ++count;
            }
        return count;
      }

   /// set the from_COPY flag
   void set_COPY()
      { from_COPY = true; }

   /// set the current HTML mode
   /// @param html new HTML mode value (0=none, 1=in-HTML, 2=in-header)
   void set_html(int html)
      { in_html = html; }

   /// set the current line number
   /// @param num the new line number
   void set_line_no(int num)
      { line_no = num; }

   /// set the pushed_IE flag
   void set_pushed_IE()
      { pushed_IE = true; }

   /// set the pushed_pending flag
   /// @param on_off new value of the pushed_pending flag
   void set_pushed_pending(bool on_off)
      { pushed_pending = on_off; }

   /// increment the line number of the current file
   static void increment_current_line_no()
      { if (files_todo.size()) ++files_todo[0].line_no; else ++stdin_line_no; }

   /// true if echo (of the input) is on for the current file
   static bool echo_current_file();

   /// close the current file and perform some cleanup
   static void close_current_file();

   /// open current file unless already open
   static void open_current_file();

   /// randomize the order of test_file_names
   static void randomize_files();

   /// FILE * from fopen (or 0 if file is closed)
   FILE * file;

   /// the file name
   UTF8_string filename;

   /// a COPY_filter instance
   COPY_filter copy_filter;

   /// the initial set of files provided on the command line
   static std::vector<InputFile> files_orig;

   /// files that need to be processed
   static std::vector<InputFile> files_todo;

protected:
   bool test;            ///< true for -T testfile, false for -f APL file
   bool pushed_IE;       ///< true for ]PUSHFILE child
   bool pushed_pending;  ///< true for ]PUSHFILE parent
   bool echo;            ///< echo stdin
   bool is_script;       ///< script (override existing functions)
   LX_mode with_LX;      ///< execute ⎕LX at the end
   int  line_no;         ///< line number in file
   int  in_html;         ///< 0: no HTML, 1: in HTML file 2: in HTML header

   /// true if this file comes from a )COPY XXX.apl (but not XXX.xml)
   bool from_COPY;

   /// a unique number for this (instance of) file.
   int64_t file_seq;

   /// line number in stdin
   static int stdin_line_no;

   /// the next unique file number
   static int64_t next_file_seq;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __INPUT_FILE_HH_DEFINED__
