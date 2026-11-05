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

#ifndef __COMMAND_HH_DEFINED__
#define __COMMAND_HH_DEFINED__

#include "Common.hh"
#include "LibPaths.hh"
#include "Value.hh"
#include "UCS_string.hh"
#include "UTF8_string.hh"

class Workspace;
struct dirent;
struct _XDisplay;

//════════════════════════════════════════════════════════════════════════════
/*!
    Some command related functions, including the main input loop
    of the APL interpreter.
 */
/// The class implementing all APL commands
class Command
{
public:
   /// one user defined command
   struct user_command
      {
        /// the first characters of the command
        UCS_string prefix;

        /// the APL function that implements the command
        UCS_string apl_function;

        /// how the left arg of \b apl_function is computed.
        int mode;
      };

   /// return the number of APL expressions entered in immediate execution mode
   static ShapeItem get_APL_expression_count()
      { return APL_expression_count; }

   /// return true iff the most recently completed do_APL_expression()
   /// (parse-time OR run-time) ended in an APL error. Parse errors are
   /// caught and printed inside do_APL_expression() itself, and
   /// run-time errors are settled by the SI machinery as a TOK_ERROR
   /// result there too -- neither throws back out to a caller, so
   /// --eval (main.cc), which needs a real exit status, has no other
   /// way to tell. See Bugs27 #55.
   static bool last_expression_failed()
      { return expression_failed; }

   /// return the current boxing format
   static int get_boxing_format()
      { return boxing_format; }

   /// return the line number where a multiline string or literal began
   static int get_multiline_start()
      { return multiline_start; }

   /// return \b true if the input os inside a multiline string or literal
   static bool inside_multi()
      { return multiline_status >= MLS_Start_of_multi; }

   /// clear the copy_once_table.
   static void clear_copy_once_table();

   /// )OFF: clean-up and exit from APL interpreter
   /// @param exit_val  process exit status code
   static void cmd_OFF(int exit_val);

   /// )OFF [exit_code] with a validated exit_code (0-255)
   /// @param out stream for the BAD COMMAND message
   /// @param arg the (optional) exit code
   static void cmd_OFF(ostream & out, const UCS_string & arg);

   /// process \b line which contains an APL command. Return true iff the
   /// command was user-defined (and then the function for that command is
   /// stored in \b line and shall be executed)).
   /// @param out   output stream for command result
   /// @param line  the input command line (modified to hold APL function if user-defined)
   static bool do_APL_command(ostream & out, UCS_string & line);

   /// process \b line which contains APL statements
   /// @param line    the APL statement text to execute
   /// @param suffix  optional value appended as result suffix, or empty
   static void do_APL_expression(const UCS_string & line, Value_P suffix);

   /// finish the current SI->top() and pop it when done
   static void finish_context();

   /// return true if \b lib looks like a library reference (a 1-digit number
   /// or a path containing . or / chars
   /// @param lib  the string to test as a library reference
   static bool is_lib_ref(const UCS_string & lib);

   /// parse user-suplied argument (of )VARS, )OPS, or )NMS commands)
   /// into strings from and to
   /// @param from        filled with the lower bound of the name range
   /// @param to          filled with the upper bound of the name range
   /// @param user_input  the raw argument string from the user
   static bool parse_from_to(UCS_string & from, UCS_string & to,
                             const UCS_string & user_input);

   /// process \b line which contains a command or statements
   /// @param line  the input line to process (modified in place)
   /// @param out   output stream, or null to suppress output
   static void process_line(UCS_string & line, ostream * out);

   /// process a single line entered by the user (in immediate execution mode)
   static void process_lines();

   /// split whitespace separated arguments into individual arguments
   /// @param arg  the whitespace-delimited argument string to split
   static UCS_string_vector split_arg(const UCS_string & arg);

   /// format for ]BOXING
   static int boxing_format;

   /// automatically display )MORE info after errors
   static bool auto_MORE;

   /// multi-line status
   static Multiline_status multiline_status;

   /// workspaces that shall not be copied twice
   static UCS_string_vector copy_once_table;

protected:
   /// return True if \b cmd starts with \b prefix
   /// @param cmd     the command string to test
   /// @param prefix  the expected command prefix
   static bool is_command(const UCS_string & cmd, const char * prefix)
      {
        const size_t prefix_len = strlen(prefix);
        return cmd.starts_iwith(prefix) &&
               (prefix_len == cmd.size() || cmd[prefix_len] <= UNI_SPACE);
      }

   /// check the number of parameters in a command
   /// @param out      output stream for error message if check fails
   /// @param command  name of the command being checked
   /// @param argc     actual number of arguments provided
   /// @param args     description of expected arguments
   static bool check_params(ostream & out, const char * command, int argc,
                            const char * args);

   /// do nothing helper for ]USERCMD
#define _NO_OP_

   /// the number of APL expressions entered in immediate execution mode
   static ShapeItem APL_expression_count;

   /// true iff the most recently completed do_APL_expression() ended
   /// in an APL error. See last_expression_failed() above.
   static bool expression_failed;

   /// line number of miltiline start
   static int multiline_start;
};
//════════════════════════════════════════════════════════════════════════════
class Cmd_IN
{
public:
   /// )IN: import an .atf workspace file
   /// @param out      output stream for command result
   /// @param args     filename and optional object names to import
   /// @param protect  true to protect existing objects from being overwritten
   static void cmd_IN(ostream & out, UCS_string_vector & args, bool protect);

protected:
   /// a helper struct for the )IN command
   struct transfer_context
      {
        /// constructor
        /// @param prot  true to protect existing objects
        transfer_context(bool prot)
        : new_record(true),
          protection(prot),
          recnum(0),
          timestamp(0)
        {}

        /// process a 'A' (array in 2 ⎕TF format) item.
        /// @param objects  list of object names to selectively import (empty = all)
        void array_2TF(const UCS_string_vector & objects) const;

        /// process a 'C' (character, in 1 ⎕TF format) item.
        /// @param objects  list of object names to selectively import (empty = all)
        void chars_1TF(const UCS_string_vector & objects) const;

        /// process an 'F' (function in 2 ⎕TF format) item.
        /// @param objects  list of object names to selectively import (empty = all)
        void function_2TF(const UCS_string_vector & objects) const;

        /// get the name, rank, and shape of a 1 ⎕TF record
        /// @param name   filled with the object name from the record
        /// @param shape  filled with the array shape from the record
        uint32_t get_nrs(UCS_string & name, Shape & shape) const;

        /// process an 'N' (numeric in 1 ⎕TF format) item.
        /// @param objects  list of object names to selectively import (empty = all)
        void numeric_1TF(const UCS_string_vector & objects) const;

        /// add \b len UTF8 bytes to \b this transfer_context
        /// @param str  pointer to UTF-8 bytes to append
        /// @param len  number of bytes to append
        void add(const UTF8 * str, int len);

        /// process one record of a workspace file
        /// @param record   pointer to the UTF-8 encoded record data
        /// @param objects  list of object names to selectively import (empty = all)
        void process_record(const UTF8 * record,
                            const UCS_string_vector & objects);

        /// accumulator for data of different records
        UCS_string data;

        /// true if record is EBCDIC (not yet supported)
        bool is_ebcdic;

        /// the record type ('A', 'C', 'N', or 'F')
        int item_type;

        /// true if a new record has started
        bool new_record;

        /// true if )IN shall not iverride existing objects
        bool protection;   // protect existing objects

        /// return true (and remember \b name for the NOT COPIED: message)
        /// if \b protection and \b name already exists (Bugs35 #64)
        /// @param name  the name of an object in the transfer file
        bool is_protected(const UCS_string & name) const;

        /// the objects that )PIN did not copy
        mutable UCS_string_vector not_copied;

        /// the record number
        int recnum;

        /// the last timestamp (if any)
        APL_time_us timestamp;
      };
};
//════════════════════════════════════════════════════════════════════════════
class Cmd_KEYB
{
   enum KB_Area
      {
        KB_AREA_FUNKEY = 1,
        KB_AREA_MAIN   = 2,
        KB_AREA_CURSOR = 4,
        KB_AREA_KEYPAD = 8,
      };

public:
   /// show keyboard layout
   /// @param out   output stream for keyboard layout display
   /// @param args  optional area specifiers (e.g. "MAIN", "CURSOR")
   static void cmd_KEYB(ostream & out, const UCS_string_vector & args);

   /// copy text into UCS_string
   /// @param start  reference to the first Unicode character position to fill
   /// @param text   the ASCII/UTF-8 text to copy
   static void copy_text(Unicode & start, const char * text);

   /// execute xmodmap -pk and parse its output.
  static bool parse_xmodmap();

   /// print the keycodes
   /// @param out   output stream for keycode table
   /// @param area  keyboard area bitmask to print
   static ostream & print_keycodes(ostream & out, KB_Area area);

   /// print the keyboard layout according to xmodmap -pk to \b out.
   /// @param out   output stream for keyboard layout
   /// @param area  keyboard area bitmask to print
   static ostream & print_keymap(ostream & out, KB_Area area);

   /// read a key symbol and translate it to a Unicode
   /// @param display  the X11 display connection
   /// @param keycode  the X11 keycode to look up
   /// @param level    the shift level (0=unshifted, 1=shifted, etc.)
    static Unicode read_xkbd_Ksym(_XDisplay * display, int keycode, int level);

   /// read the mappings for all templates
   static bool read_xkbd_map();

   /// read the mappings for one template
   /// @param lines       array of C strings forming the template
   /// @param line_count  number of lines in the template array
   static void read_xkbd_template(const char ** lines, int line_count);

   /// true for XkbKeycodeToKeysym(), false for xmodmap -pke
   static bool keymap_from_xkbd;

protected:
   enum Keycode
      {
        keycode_NONE =  -1,
        keycode_min  =   8,  // including
        keycode_max  = 255   // including;
      };

   /// fill \b result with a keyboard templace according to bitmap \b area
   /// @param result  filled with the keyboard template lines
   /// @param area    keyboard area bitmask selecting which sections to include
   static void get_template(UCS_string_vector & result, KB_Area area);

   /// parse one output line of xmodmap -pke.
   /// @param buffer  the line text to parse
   /// @param line    the line number (for diagnostics)
   static bool parse_xmodmap_line(const char * buffer, int line);

   /// parse one unicode (like Uxxxx); increment \b p, and return \b true
   /// on success.
   /// @param keycode  the keycode being parsed (for diagnostics)
   /// @param p        parse position, advanced past the parsed token on success
   /// @param unicode  filled with the parsed Unicode codepoint
   static bool parse_xmodmap_Unicode(Keycode keycode, const char * & p,
                                     uint32_t & unicode);

   static struct map_item
      {
        map_item()
        : keycode(-1),
          Ucount(0)
          { unicodes[0] = unicodes[1] = unicodes[2] = unicodes[3] = Unicode_0; }

        int keycode;           ///< the keycode
        int Ucount;            ///< the number of Uxxx mappings
        Unicode unicodes[4];   ///< the unicodes
      } key_map[256];
};
//════════════════════════════════════════════════════════════════════════════
class Cmd_LIB
{
public:
   /// list content of workspace and wslib directories: )LIB [N]
   /// @param out   output stream for library listing
   /// @param args  optional library reference
   static void cmd_LIB1(ostream & out, const UCS_string_vector & args);

   /// list content of workspace and wslib directories: ]LIB [N]
   /// @param out   output stream for library listing
   /// @param args  optional library reference and sort options
   static void cmd_LIB2(ostream & out, const UCS_string_vector & args);

   /// list paths of workspace and wslib directories
   /// @param out      output stream for library path listing
   /// @param lib_ref  optional library reference (digit 0-9) to filter
   static void cmd_LIBS(ostream & out, const UCS_string_vector & lib_ref);

   /// return true if entry is a directory
   /// @param entry  the directory entry to test
   /// @param path   the parent directory path for stat() fallback
   static bool is_directory(const dirent * entry, const UTF8_string & path);
protected:
   /// sort order
   enum SORT_ORDER
      {
        SORT_NONE = 0,      ///< do not sort
        SORT_SIZE = 1,      ///< sort by size
        SORT_TIME = 2,      ///< sort by time
      };

   /// list library: common helper. variant tells apart )LIB and ]LIB.
   /// @param out   output stream for library listing
   /// @param args  library reference and optional sort arguments
   /// @param dbg   true for ]LIB (extended debug format)
   static void LIB_common(ostream & out, const UCS_string_vector & args,
                          bool dbg);

   /// print the workspace names in the LIB directory w/o sorting
   /// @param out          output stream for the listing
   /// @param lib_path     path to the library directory
   /// @param directories  subdirectory names to list
   /// @param files        workspace file names to list
   static void LIB_print_flat(ostream & out, const UTF8_string lib_path,
                           const UCS_string_vector & directories,
                           const UCS_string_vector & files);

   /// print the workspace names in the LIB directory with sorting
   /// @param out          output stream for the listing
   /// @param lib_path     path to the library directory
   /// @param directories  subdirectory names to list
   /// @param files        workspace file names to list
   /// @param sort         sort order to apply
   static void LIB_print_sorted(ostream & out, const UTF8_string lib_path,
                           const UCS_string_vector & directories,
                           const UCS_string_vector & files, SORT_ORDER sort);

   /// open directory arg and follow symlinks
   /// @param path  filled with the resolved directory path
   /// @param out   output stream for error messages
   /// @param args  library reference argument tokens
   static DIR * open_LIB_dir(UTF8_string & path, ostream & out,
                            const UCS_string_vector & args);

   /// return the property by which file names shall be sorted
   /// @param sort      the sort order (SORT_SIZE or SORT_TIME)
   /// @param lib_path  path to the library directory
   /// @param filename  the workspace filename to stat
   static size_t sort_property(SORT_ORDER sort, const UTF8_string & lib_path,
                               const UCS_string & filename);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __COMMAND_HH_DEFINED__
