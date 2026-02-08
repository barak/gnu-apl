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
   /// process a single line entered by the user (in immediate execution mode)
   static void process_lines();

   /// return \b true if the input os inside a multiline string or literal
   static bool inside_multi()
      { return multiline_status >= MLS_Start_of_multi; }

   /// return the line number where a multiline string or literal began
   static int get_multiline_start()
      { return multiline_start; }

   /// process \b line which contains a command or statements
   static void process_line(UCS_string & line, ostream * out);

   /// process \b line which contains an APL command. Return true iff the
   /// command was user-defined (and then the function for that command is
   /// stored in \b line and shall be executed)).
   static bool do_APL_command(ostream & out, UCS_string & line);

   /// process \b line which contains APL statements
   static void do_APL_expression(UCS_string & line, Value_P suffix);

   /// finish the current SI->top() and pop it when done
   static void finish_context();

   /// parse user-suplied argument (of )VARS, )OPS, or )NMS commands)
   /// into strings from and to
   static bool parse_from_to(UCS_string & from, UCS_string & to,
                             const UCS_string & user_input);

   /// return true if \b lib looks like a library reference (a 1-digit number
   /// or a path containing . or / chars
   static bool is_lib_ref(const UCS_string & lib);

   /// return the current boxing format
   static int get_boxing_format()
      { return boxing_format; }

   /// return the number of APL expressions entered in immediate execution mode
   static ShapeItem get_APL_expression_count()
      { return APL_expression_count; }

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
        static bool greater(const val_val & A, const val_val & B, const void *)
           { return A.child > B.child; }

        /// compare function for Heapsort<val_val>::search<const Value *>()
        static int compare(const Value * key, const val_val * B, const void *)
           { return key - B->child; }
      };

   /// clear the copy_once_table.
   static void clear_copy_once_table();

   /// )CHECK: check workspace integrity (stale Value and IndexExpr objects, etc)
   static void cmd_CHECK(ostream & out, const UCS_string & arg);

   /// )OFF: clean-up and exit from APL interpreter
   static void cmd_OFF(int exit_val);

   /// format for ]BOXING
   static int boxing_format;

   /// automatically display )MORE info after errors
   static bool auto_MORE;

   /// multi-line status
   static Multiline_status multiline_status;

protected:
   /// line number of miltiline start
   static int multiline_start;

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
   static bool is_command(const UCS_string & cmd, const char * prefix)
      {
        const size_t prefix_len = strlen(prefix);
        return cmd.starts_iwith(prefix) &&
               (prefix_len == cmd.size() || cmd[prefix_len] <= UNI_SPACE);
      }
   /// )BOXING: select output format for APL values
   static void cmd_BOXING(ostream & out, const UCS_string & arg);

   /// )CLEAR: clear the current Workspace
   static void cmd_CLEAR(ostream & out);

   /// )SAVE the current  WS as CONTINUE and then )OFF
   static void cmd_CONTINUE(ostream & out);

   /// )COPY: copy a workspace file
   static void cmd_COPY(ostream & out, UCS_string_vector & args,
                        bool protection);

   /// )COPY_ONCE: copy a workspace file, nut at most once
   static void cmd_COPY_ONCE(ostream & out, UCS_string_vector & args);

   /// ]DOXY: create doxygen-like documentation of the current workspace
   static void cmd_DOXY(ostream & out, const UCS_string_vector & args);

   /// )DROP: delete a workspace file
   static void cmd_DROP(ostream & out, const UCS_string_vector & args);

   /// )DUMP: dump a workspace file (.apl)
   static void cmd_DUMP(ostream & out, const UCS_string_vector & args,
                        bool html, bool silent);

   /// )ERASE: erase symbols
   static void cmd_ERASE(ostream & out, UCS_string_vector & args);

   /// ]EXPECT: set the number of expected errors (in testcase files)
   static void cmd_EXPECT(ostream & out, const UCS_string & arg);

   /// )FNS: show list of functions
   static void cmd_FNS(ostream & out, const UCS_string & arg);

   /// show list of commands
   static void cmd_HELP(ostream & out, const UCS_string & arg);

   /// show or clear input history
   static void cmd_HISTORY(ostream & out, const UCS_string & arg);

   /// )HOST: execute OS command
   static void cmd_HOST(ostream & out, const UCS_string & arg);

   /// )LOAD: load a workspace file
   static void cmd_LOAD(ostream & out, UCS_string_vector & args, 
                        UCS_string & quad_lx, bool silent);

   /// control logging facilities
   static void cmd_LOG(ostream & out, const UCS_string & arg);

   /// print more error info
   static void cmd_MORE(ostream & out, const UCS_string_vector & args);

   /// continue with (jump to) next input file.
   static void cmd_NEXTFILE(ostream & out, const UCS_string_vector & args);

   /// )NMS: show list of (Symbol-) names
   static void cmd_NMS(ostream & out, const UCS_string & arg);

   /// do nothing helper for ]USERCMD
#define _NO_OP_

   /// )FNS: show list of operators
   static void cmd_OPS(ostream & out, const UCS_string & arg);

   /// show or clear optimizarion counters
   static void cmd_OPTIM(ostream & out, const UCS_string & lib);

   /// )OUT: export a workspace file in .atf format
   static void cmd_OUT(ostream & out, UCS_string_vector & args);

   /// )OWNERS: show list of all APL value owners
   static void cmd_OWNERS(ostream & out);

   /// show performance counters
   static void cmd_PSTAT(ostream & out, const UCS_string & arg);

   /// PUSHFILE: push one (testcase-) file
   static void cmd_PUSHFILE();

   /// )SAVE: save a workspace file (.xml)
   static void cmd_SAVE(ostream & out, const UCS_string_vector & args);

   /// )SI: display the )SI stack
   static void cmd_SI(ostream & out, bool dbg);

   /// )SIC: clear the )SI stack
   static void cmd_SIC(ostream & out);

   /// )SINL: display the )SI stack with name list
   static void cmd_SINL(ostream & out);

   /// )SIS: display the )SI stack with statements
   static void cmd_SIS(ostream & out, bool dbg);

   /// ]SVARS: display all shared variables
   static void cmd_SVARS(ostream & out);

   /// ]SYMBOL: display the details of one symbol
   static void cmd_SYMBOL(ostream & out, const UCS_string & arg);

   /// ]SYMBOLS: set or display the number of symbols
   static void cmd_SYMBOLS(ostream & out, const UCS_string & arg);

   /// ]USERCMD: create a user defined command
   static void cmd_USERCMD(ostream & out, const UCS_string & arg,
                           UCS_string_vector & args);

   /// )VALUES: show list of all APL values
   static void cmd_VALUES(ostream & out);

   /// )VARS: show list of variables
   static void cmd_VARS(ostream & out, const UCS_string & arg);

   /// )WSID: display or change the workspace name
   static void cmd_WSID(ostream & out, const UCS_string_vector & args);

   /// ]XTERM: enable and disable colors
   static void cmd_XTERM(ostream & out, const UCS_string & args);

   /// )HELP: show help for APL primitives
   static void primitive_help(ostream & out, const char * arg, int arity,
                              const char * prim, const char * name,
                              const char * title, const char * descr);

   /// split whitespace separated arguments into individual arguments
   static UCS_string_vector split_arg(const UCS_string & arg);

   /// execute a user defined command
   static void do_USERCMD(ostream & out, UCS_string & line,
                          const UCS_string & line1, const UCS_string & cmd,
                          UCS_string_vector & args, int uidx);

   /// check if a command name conflicts with an existing command
   static bool check_name_conflict(ostream & out, const UCS_string & cnew,
                                   const UCS_string cold);

   /// check if a command is being redefined
   static bool check_redefinition(ostream & out, const UCS_string & cnew,
                                  const UCS_string fnew, const int mnew);

   /// check the number of parameters in a command
   static bool check_params(ostream & out, const char * command, int argc,
                            const char * args);

   /// parse the argument of the ]LOG command and set logging accordingly
   static void log_control(const UCS_string & args);

   /// the number of APL expressions entered in immediate execution mode
   static ShapeItem APL_expression_count;

   /// workspaces that shall not be copied twice
   static UCS_string_vector copy_once_table;

   /// return true iff, according to config.h, capability \b capa is available
   static bool have_capability(const UCS_string & capa);
};
//----------------------------------------------------------------------------
class Cmd_IN
{
public:
   /// )IN: import an .atf workspace file
   static void cmd_IN(ostream & out, UCS_string_vector & args, bool protect);

protected:
   /// a helper struct for the )IN command
   struct transfer_context
      {
        /// constructor
        transfer_context(bool prot)
        : new_record(true),
          recnum(0),
          timestamp(0),
          protection(prot)
        {}

        /// process one record of a workspace file
        void process_record(const UTF8 * record,
                            const UCS_string_vector & objects);

        /// get the name, rank, and shape of a 1 ⎕TF record
        uint32_t get_nrs(UCS_string & name, Shape & shape) const;

        /// process a 'A' (array in 2 ⎕TF format) item.
        void array_2TF(const UCS_string_vector & objects) const;

        /// process a 'C' (character, in 1 ⎕TF format) item.
        void chars_1TF(const UCS_string_vector & objects) const;

        /// process an 'F' (function in 2 ⎕TF format) item.
        void function_2TF(const UCS_string_vector & objects) const;

        /// process an 'N' (numeric in 1 ⎕TF format) item.
        void numeric_1TF(const UCS_string_vector & objects) const;

        /// add \b len UTF8 bytes to \b this transfer_context
        void add(const UTF8 * str, int len);

        /// true if a new record has started
        bool new_record;

        /// true if record is EBCDIC (not yet supported)
        bool is_ebcdic;

        /// the record number
        int recnum;

        /// the record type ('A', 'C', 'N', or 'F')
        int item_type;

        /// the last timestamp (if any)
        APL_time_us timestamp;

        /// true if )IN shall not iverride existing objects
        bool protection;   // protect existing objects

        /// accumulator for data of different records
        UCS_string data;
      };
};
//----------------------------------------------------------------------------
class Cmd_KEYB
{
public:
   /// show keyboard layout
   static void cmd_KEYB(ostream & out, const UCS_string_vector & args);

   /// execute xmodmap -pk and parse its output.
  static bool parse_xmodmap();

   /// read the mappings for all templates
   static bool read_xkbd_map();

   /// read the mappings for one template
   static void read_xkbd_template(const char ** lines, int line_count);

   /// print the keycodes
   static ostream & print_keycodes(ostream & out, int area);

   /// print the keyboard layout according to xmodmap -pk to \b out.
   static ostream & print_keymap(ostream & out, int area);

   /// read a key symbol and translate it to a Unicode
    static Unicode read_ksym(_XDisplay * display, int keycode, int level);

   /// true for XkbKeycodeToKeysym(), false for xmodmap -pke
   static bool keymap_from_xkbd;
protected:
   enum Keycode
      {
        keycode_NONE =  -1,
        keycode_min  =   8,  // including
        keycode_max  = 255   // including;
      };

   /// parse one output line of xmodmap -pke.
   static bool parse_xmodmap_line(const char * buffer, int line);

   /// parse one unicode (like Uxxxx); increment \b p, and return \b true
   /// on success.
   static bool parse_Unicode(Keycode keycode, const char * & p, uint32_t & unicode);

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
   /// list paths of workspace and wslib directories
   static void cmd_LIBS(ostream & out, const UCS_string_vector & lib_ref);

   /// list content of workspace and wslib directories: )LIB [N]
   static void cmd_LIB1(ostream & out, const UCS_string_vector & args);

   /// list content of workspace and wslib directories: ]LIB [N]
   static void cmd_LIB2(ostream & out, const UCS_string_vector & args);

   /// return true if entry is a directory
   static bool is_directory(const dirent * entry, const UTF8_string & path);
protected:
   /// sort order
   enum SORT_ORDER
      {
        SORT_NONE = 0,      ///< do not sort
        SORT_SIZE = 1,      ///< sort by size
        SORT_TIME = 2,      ///< sort by time
      };

   /// open directory arg and follow symlinks
   static DIR * open_LIB_dir(UTF8_string & path, ostream & out,
                            const UCS_string_vector & args);

   /// list library: common helper. variant tells apart )LIB and ]LIB.
   static void LIB_common(ostream & out, const UCS_string_vector & args,
                          bool dbg);

   /// print the workspace names in the LIB directory w/o sorting
   static void LIB_print_flat(ostream & out, const UTF8_string lib_path,
                           const UCS_string_vector & directories,
                           const UCS_string_vector & files);

   /// print the workspace names in the LIB directory with sorting
   static void LIB_print_sorted(ostream & out, const UTF8_string lib_path,
                           const UCS_string_vector & directories,
                           const UCS_string_vector & files, SORT_ORDER sort);

   /// return the property by which file names shall be sorted
   static size_t sort_property(SORT_ORDER sort, const UTF8_string & lib_path,
                               const UCS_string & filename);
};
//----------------------------------------------------------------------------
#endif // __COMMAND_HH_DEFINED__
