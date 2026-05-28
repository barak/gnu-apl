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

//----------------------------------------------------------------------------
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

   /// a helper for finding sub-values with two parents
   struct val_val
      {
        /// the parent (0 unless \b this is a sub-value)
        const Value * parent;

        /// the value (always valid)
        const Value * child;

        /// compare function for Heapsort::sort()
        /// @param A     first val_val element
        /// @param B     second val_val element
        static bool greater(const val_val & A, const val_val & B, const void *)
           { return A.child > B.child; }

        /// compare function for Heapsort<val_val>::search<const Value *>()
        /// @param key   the Value pointer to search for
        /// @param B     the val_val element to compare against
        static int compare(const Value * key, const val_val * B, const void *)
           { return key - B->child; }
      };

   /// return the number of APL expressions entered in immediate execution mode
   static ShapeItem get_APL_expression_count()
      { return APL_expression_count; }

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

   /// )CHECK: check workspace integrity (stale Value and IndexExpr objects, etc)
   /// @param out  output stream for check results
   /// @param arg  optional command argument string
   static void cmd_CHECK(ostream & out, const UCS_string & arg);

   /// )OFF: clean-up and exit from APL interpreter
   /// @param exit_val  process exit status code
   static void cmd_OFF(int exit_val);

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

   /// format for ]BOXING
   static int boxing_format;

   /// automatically display )MORE info after errors
   static bool auto_MORE;

   /// multi-line status
   static Multiline_status multiline_status;

protected:
   /// On, Off, or Toggle
   enum OOT
      {
        Off    = 0,   ///< Off
        On     = 1,   ///< On
        Toggle = 2,   ///< Toggle
      };

   /// a logging ID and an action (On/Off/Toggle) to be performed with it
   struct lid_OOT
      {
        LogId lid;           ///< the logging ID
        OOT on_off_toggle;   ///< turn lid On, Off, or Toggle it
      };

   /// return True if \b cmd starts with \b prefix
   /// @param cmd     the command string to test
   /// @param prefix  the expected command prefix
   static bool is_command(const UCS_string & cmd, const char * prefix)
      {
        const size_t prefix_len = strlen(prefix);
        return cmd.starts_iwith(prefix) &&
               (prefix_len == cmd.size() || cmd[prefix_len] <= UNI_SPACE);
      }

   /// )BOXING: select output format for APL values
   /// @param out  output stream for command result
   /// @param arg  boxing format argument string
   static void cmd_BOXING(ostream & out, const UCS_string & arg);

   /// check if a command name conflicts with an existing command
   /// @param out   output stream for conflict error message
   /// @param cnew  the new command name being defined
   /// @param cold  an existing command name to compare against
   static bool check_name_conflict(ostream & out, const UCS_string & cnew,
                                   const UCS_string cold);

   /// check the number of parameters in a command
   /// @param out      output stream for error message if check fails
   /// @param command  name of the command being checked
   /// @param argc     actual number of arguments provided
   /// @param args     description of expected arguments
   static bool check_params(ostream & out, const char * command, int argc,
                            const char * args);

   /// check if a command is being redefined
   /// @param out   output stream for redefinition warning
   /// @param cnew  the new command name
   /// @param fnew  the new APL function name implementing the command
   /// @param mnew  the new mode value for the command
   static bool check_redefinition(ostream & out, const UCS_string & cnew,
                                  const UCS_string fnew, const int mnew);

   /// )CLEAR: clear the current Workspace
   /// @param out  output stream for command result
   static void cmd_CLEAR(ostream & out);

   /// )SAVE the current  WS as CONTINUE and then )OFF
   /// @param out  output stream for command result
   static void cmd_CONTINUE(ostream & out);

   /// )COPY: copy a workspace file
   /// @param out         output stream for command result
   /// @param args        workspace name and optional object names
   /// @param protection  true to protect existing objects from being overwritten
   static void cmd_COPY(ostream & out, UCS_string_vector & args,
                        bool protection);

   /// )COPY_ONCE: copy a workspace file, nut at most once
   /// @param out   output stream for command result
   /// @param args  workspace name and optional object names
   static void cmd_COPY_ONCE(ostream & out, UCS_string_vector & args);

   /// ]DOXY: create doxygen-like documentation of the current workspace
   /// @param out   output stream for documentation
   /// @param args  optional arguments controlling output format
   static void cmd_DOXY(ostream & out, const UCS_string_vector & args);

   /// )DROP: delete a workspace file
   /// @param out   output stream for command result
   /// @param args  library reference and workspace name
   static void cmd_DROP(ostream & out, const UCS_string_vector & args);

   /// )DUMP: dump a workspace file (.apl)
   /// @param out     output stream for command result
   /// @param args    workspace name and optional object names
   /// @param html    true to generate HTML output
   /// @param silent  true to suppress informational messages
   static void cmd_DUMP(ostream & out, const UCS_string_vector & args,
                        bool html, bool silent);

   /// )ERASE: erase symbols
   /// @param out   output stream for command result
   /// @param args  names of symbols to erase
   static void cmd_ERASE(ostream & out, const UCS_string_vector & args);

   /// ]EXPECT: set the number of expected errors (in testcase files)
   /// @param out  output stream for command result
   /// @param arg  expected error count as a string
   static void cmd_EXPECT(ostream & out, const UCS_string & arg);

   /// )FNS: show list of functions
   /// @param out  output stream for function list
   /// @param arg  optional name range argument
   static void cmd_FNS(ostream & out, const UCS_string & arg);

   /// show list of commands
   /// @param out  output stream for help text
   /// @param arg  optional topic or primitive to show help for
   static void cmd_HELP(ostream & out, const UCS_string & arg);

   /// show or clear input history
   /// @param out  output stream for history listing
   /// @param arg  optional argument (e.g. "CLEAR")
   static void cmd_HISTORY(ostream & out, const UCS_string & arg);

   /// )HOST: execute OS command
   /// @param out  output stream for command output
   /// @param arg  the OS command string to execute
   static void cmd_HOST(ostream & out, const UCS_string & arg);

   /// )LOAD: load a workspace file
   /// @param out      output stream for command result
   /// @param args     library reference and workspace name
   /// @param quad_lx  receives the ⎕LX expression from the loaded workspace
   /// @param silent   true to suppress informational messages
   static void cmd_LOAD(ostream & out, const UCS_string_vector & args,
                        UCS_string & quad_lx, bool silent);

   /// control logging facilities
   /// @param out  output stream for command result
   /// @param arg  logging facility name or number and on/off flag
   static void cmd_LOG(ostream & out, const UCS_string & arg);

   /// print more error info
   /// @param out   output stream for error details
   /// @param args  optional arguments
   static void cmd_MORE(ostream & out, const UCS_string_vector & args);

   /// continue with (jump to) next input file.
   /// @param out   output stream for command result
   /// @param args  optional capability requirements for the next file
   static void cmd_NEXTFILE(ostream & out, const UCS_string_vector & args);

   /// )NMS: show list of (Symbol-) names
   /// @param out  output stream for name list
   /// @param arg  optional name range argument
   static void cmd_NMS(ostream & out, const UCS_string & arg);

   /// do nothing helper for ]USERCMD
#define _NO_OP_

   /// )FNS: show list of operators
   /// @param out  output stream for operator list
   /// @param arg  optional name range argument
   static void cmd_OPS(ostream & out, const UCS_string & arg);

   /// show or clear optimizarion counters
   /// @param out  output stream for optimization data
   /// @param lib  optional library or function name to filter output
   static void cmd_OPTIM(ostream & out, const UCS_string & lib);

   /// )OUT: export a workspace file in .atf format
   /// @param out   output stream for command result
   /// @param args  workspace name and optional object names
   static void cmd_OUT(ostream & out, UCS_string_vector & args);

   /// )OWNERS: show list of all APL value owners
   /// @param out  output stream for owner list
   static void cmd_OWNERS(ostream & out);

   /// show performance counters
   /// @param out  output stream for performance statistics
   /// @param arg  optional argument to filter or reset counters
   static void cmd_PSTAT(ostream & out, const UCS_string & arg);

   /// PUSHFILE: push one (testcase-) file
   static void cmd_PUSHFILE();

   /// )SAVE: save a workspace file (.xml)
   /// @param out   output stream for command result
   /// @param args  optional library reference and workspace name
   static void cmd_SAVE(ostream & out, const UCS_string_vector & args);

   /// )SI: display the )SI stack
   /// @param out  output stream for SI stack display
   /// @param dbg  true to include debug information
   static void cmd_SI(ostream & out, bool dbg);

   /// )SIC: clear the )SI stack
   /// @param out  output stream for command result
   static void cmd_SIC(ostream & out);

   /// )SINL: display the )SI stack with name list
   /// @param out  output stream for SI stack with name list
   static void cmd_SINL(ostream & out);

   /// )SIS: display the )SI stack with statements
   /// @param out  output stream for SI stack with statements
   /// @param dbg  true to include debug information
   static void cmd_SIS(ostream & out, bool dbg);

   /// ]SVARS: display all shared variables
   /// @param out  output stream for shared variable list
   static void cmd_SVARS(ostream & out);

   /// ]SYMBOL: display the details of one symbol
   /// @param out  output stream for symbol details
   /// @param arg  name of the symbol to display
   static void cmd_SYMBOL(ostream & out, const UCS_string & arg);

   /// ]SYMBOLS: set or display the number of symbols
   /// @param out  output stream for symbol table info
   /// @param arg  optional new symbol table size
   static void cmd_SYMBOLS(ostream & out, const UCS_string & arg);

   /// ]USERCMD: create a user defined command
   /// @param out   output stream for command result
   /// @param arg   the raw argument string (unsplit)
   /// @param args  the split argument tokens
   static void cmd_USERCMD(ostream & out, const UCS_string & arg,
                           UCS_string_vector & args);

   /// )VALUES: show list of all APL values
   /// @param out  output stream for value list
   static void cmd_VALUES(ostream & out);

   /// )VARS: show list of variables
   /// @param out  output stream for variable list
   /// @param arg  optional name range argument
   static void cmd_VARS(ostream & out, const UCS_string & arg);

   /// )WSID: display or change the workspace name
   /// @param out   output stream for command result
   /// @param args  optional new workspace name
   static void cmd_WSID(ostream & out, const UCS_string_vector & args);

   /// ]XTERM: enable and disable colors
   /// @param out   output stream for command result
   /// @param args  "ON", "OFF", or empty to toggle color support
   static void cmd_XTERM(ostream & out, const UCS_string & args);

   /// execute a user defined command
   /// @param out   output stream for command result
   /// @param line  the full input line (modified to APL function call)
   /// @param line1 the first token on the input line
   /// @param cmd   the matched user command prefix
   /// @param args  the remaining arguments after the command name
   /// @param uidx  index into the user_command table
   static void do_USERCMD(ostream & out, UCS_string & line,
                          const UCS_string & line1, const UCS_string & cmd,
                          UCS_string_vector & args, int uidx);

   /// return true iff, according to config.h, capability \b capa is available
   /// @param capa  the capability name to check (e.g. "⎕FFT", "GTK")
   static bool have_capability(const UCS_string & capa);

   /// parse the argument of the ]LOG command and set logging accordingly
   /// @param args  the logging control argument string
   static void log_control(const UCS_string & args);

   /// )HELP: show help for APL primitives
   /// @param out    output stream for help text
   /// @param arg    the argument the user typed after )HELP
   /// @param arity  1 for monadic, 2 for dyadic, 0 for either
   /// @param prim   the primitive symbol as a C string
   /// @param name   the primitive name
   /// @param title  short title line
   /// @param descr  longer description text
   static void primitive_help(ostream & out, const char * arg, int arity,
                              const char * prim, const char * name,
                              const char * title, const char * descr);

   /// split whitespace separated arguments into individual arguments
   /// @param arg  the whitespace-delimited argument string to split
   static UCS_string_vector split_arg(const UCS_string & arg);

   /// the number of APL expressions entered in immediate execution mode
   static ShapeItem APL_expression_count;

   /// workspaces that shall not be copied twice
   static UCS_string_vector copy_once_table;

   /// line number of miltiline start
   static int multiline_start;
};
//----------------------------------------------------------------------------
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

        /// the record number
        int recnum;

        /// the last timestamp (if any)
        APL_time_us timestamp;
      };
};
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
#endif // __COMMAND_HH_DEFINED__
