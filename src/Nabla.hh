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

#ifndef __NABLA_HH_DEFINED__
#define __NABLA_HH_DEFINED__

#include "UCS_string.hh"

class Symbol;

//════════════════════════════════════════════════════════════════════════════
/// a line number like 1.2 while editing it
struct LineLabel
{
   /// empty constructor
   LineLabel()
   : ln_major(0)
   {}

   /// constructor: line mj
   /// @param mj the integer (major) part of the line number
   LineLabel(int mj)
   : ln_major(mj)
   {}

   /// print the line number
   /// @param out output stream to write the line label to
   void print(ostream & out) const;

   /// print a prompt like [1.2] into buffer
   /// @param min_size minimum character width for the prompt field
   UCS_string print_prompt(int min_size) const;

   /// true iff two line numbers are equal
   /// @param other the line label to compare against
   bool operator ==(const LineLabel & other) const;

   /// true iff this line number is smaller than \b other
   /// @param other the line label to compare against
   bool operator <(const LineLabel & other) const;

   /// true if tis line number is valid (i.e. the line exists)
   bool valid() const
      { return ln_major != -1; }

   /// invalidate the line number
   void clear()
      { ln_major = -1;   ln_minor.clear(); }

   /// return true iff this line number is the header line [0]
   bool is_header_line_number() const
      { return ln_major == 0 && ln_minor.size() == 0; }

   /// increase the line number
   void next();

   /// increase the line number
   void insert();

   /// the integer part of the line number
   int ln_major;

   /// the fractional part of the  line number
   UCS_string ln_minor;
};

/// The Nabla editor
class Nabla
{
public:
   /// edit the function specified in \b cmd (e.g. cmd = "∇FUN")
   /// @param cmd the ∇-editor command string (e.g. "∇FUN")
   static void edit_function(const UCS_string & cmd);

   /// Return the line label (sucah as [1]) and the line text.
   /// Set is_current iff \b line is the current line of the editor.
   /// Return 0 if line > lines.size().
   /// @param line 0-based index of the function line to retrieve
   /// @param is_current set to true if this line is the currently edited line
   UCS_string get_label_and_text(int line, bool & is_current) const;

   /// return the number of lines
   int get_line_count() const
      { return lines.size(); }

protected:
   /// constructor
   /// @param cmd the ∇-editor command string used to open the editor
   Nabla(const UCS_string & cmd);

   /// throw a DEFN error with some additional information
   /// @param loc caller location for diagnostics
   void throw_edit_error(const char * loc);

   /// edit this function
   void edit();

   /// skip the body lines (up to the closing ∇) after a refused header
   void skip_body();

   /// a line label and program text, e.g.  [1.1] TEXT←TEXT
   struct FunLine
      {
        /// empty constructor
        FunLine()
        : stop_flag(false),
          trace_flag(false)
        {}

        /// constructor
        /// @param lab the line label (e.g. [1.2])
        /// @param tx the function text for this line
        FunLine(LineLabel lab, const UCS_string & tx)
        : label(lab),
          text(tx),
          stop_flag(false),
          trace_flag(false)
        {}

        /// print the line
        /// @param out output stream to write the line to
        void print(ostream & out) const;

        /// return the line label (sucah as [1]) and the line text
        UCS_string get_label_and_text() const;

        /// the label (like [1.2] of the line
        LineLabel label;

        /// the function text of the line
        UCS_string text;

        /// true if the line has a stop flag (existing function with ∆S)
        bool stop_flag;

        /// true if the line has a trace flag (existing function with ∆T)
        bool trace_flag;
      };

    /// start editing, return source location if \b first_command is bad.
    const char * start();

   /// parse an operation (something like [...], e.g. [⎕3])
   /// @param oper output string receiving the parsed operation text
   /// @param initial true if this is the first command parse
   const char * parse_oper(UCS_string & oper, bool initial);

   /// execute an edit operation
   const char * execute_oper();

   /// show function
   const char * execute_show();

   /// delete function
   const char * execute_delete();

   /// set current_line after deleting lines up to \b highest
   void prompt_after_delete(const LineLabel & highest);

   /// edit function line
   const char * execute_edit();

   /// edit function header
   const char * edit_header_line();

   /// edit body line
   const char * edit_body_line();

   /// restore function
   const char * execute_escape();

   /// create a new function. Return a non-zero error text on error.
   const char * open_new_function();

   /// open an existing function, or create a new one. Return a non-zero
   // error text on error.
   /// @param name APL name of the function to open
   const char * open_existing_function(const UCS_string & name);

   /// return true if \b ucs starts with an axis
   /// @param ucs UCS string to test for an axis prefix
   static bool is_axis(const UCS_string & ucs);

   /// display the lines of a list [⎕n1 n2 ...]
   const char * execute_show_list();

   /// delete the lines of a list [∆n1 n2 ...]
   const char * execute_delete_list();

   /// return the position of the ⍝ that starts a comment in \b line, or -1
   static int comment_pos(const UCS_string & line);

   /// return the position of the first closing ∇ or ⍫ in \b line (outside
   /// of strings and before a comment), or -1 if none
   /// @param line the line entered in the editor
   static int close_pos(const UCS_string & line);

   /// parse [nn.mm] into a LineLabel;
   /// @param c iterator positioned at the '['; advanced past the ']' on return
   LineLabel parse_lineno(UCS_string::iterator & c);

   /// set by parse_lineno() if a line number exceeds 2147483647
   bool lineno_overflow = false;

   /// return index of line with label lab, or -1 if not found.
   /// @param lab the line label to search for
   int find_line(const LineLabel & lab) const;

   /// true if line with label lab exists
   /// @param lab the line label to check
   bool line_exists(const LineLabel & lab) const
      { return find_line(lab) != -1; }

   /// set \b idx_from and \b idx_to to the indices of the first and the last
   /// line whose label lies in the range from-to of line_set (both included).
   /// The endpoints need not exist (LRM pp. 388, 391); idx_from > idx_to if
   /// no line lies in the range.
   void find_range(int & idx_from, int & idx_to) const;

   /// return \b true if line \b lab is inside a multi-line string (or
   /// literal) that an earlier line has started and not yet ended
   bool inside_multi_line(const LineLabel & lab) const;

   /// source the line number (in a .apl script file) of the ∇ that
   /// started the ∇-editor
   const int defn_line_no;

   /** the header of the function being edited. This variable contains the
       opening line up to (excluding) the [ ] (if any). For an existing
       function this is only the function name, while for a new function this
       is the function header, possibly truncated at '[' (i.e. if the header
       contains an axis specification).
     */
   UCS_string fun_header;

   /// the symbol for the function being edited
   Symbol * fun_symbol;

   /// the lines of the function (aka. function texts).
   vector<FunLine> lines;

   /// editor commands
   enum Ecmd
      {
        ECMD_NOP    = 0,   ///< do nothing
        ECMD_SHOW   = 1,   ///< show function line(s) idx_from ... idx_to
        ECMD_EDIT   = 2,   ///< edit function line line_set.get_from()
        ECMD_DELETE = 3,   ///< delete function line(s) in line_set
        ECMD_ESCAPE = 4,   ///< abort editing (discard changes made so far)
      } ecmd;          ///< the current editor command

   /// the line numbers that the user has entered in an editor command, e.g.
   /// [n⎕m] or [∆n-m]: \b from is the number before the command character
   /// (if any), \b to the number after it (if any). A missing number is an
   /// invalid LineLabel. What the numbers mean depends on the command (e.g.
   /// [⎕m] displays lines 0 to m, while [∆m] deletes only line m).
   class Line_set
      {
        public:
           /// the (syntactic) form of the line numbers entered (m ≤ N)
           enum Mode
              {
                LSM_invalid = -1,   ///< out of order, e.g. m > N or [∆4 1]
                LSM_none    =  0,   ///< no line number at all
                LSM_one     =  1,   ///< N    a single line number
                LSM__N      =  2,   ///<  -N  lines up to and including N
                LSM_m_      =  3,   ///< m-   line m and above
                LSM_mN      =  4,   ///< m-N  lines m to N (both including)
                LSM_vec     =  5,   ///< V W ...  a list of line numbers
              };

           /// no line number entered (e.g. [⎕])
           Line_set()
           : from(-1),
             to(-1),
             mode(LSM_none)
           {}

           /// a single line \b N (e.g. [N], or the current line)
           Line_set(const LineLabel & N)
           : from(N),
             to(-1),
             mode(LSM_one)
           {}

           /// the lines from \b m to \b N
           Line_set(const LineLabel & m, const LineLabel & N)
           : from(m),
             to(N),
             mode(N < m ? LSM_invalid : LSM_mN)
           {}

           /// the (syntactic) form of the line numbers entered
           Mode get_mode() const
              { return mode; }

           /// true for the range forms -N, m-, and m-N
           bool is_range() const
              { return mode == LSM__N || mode == LSM_m_ || mode == LSM_mN; }

           /// the number before the command character (invalid if none)
           const LineLabel & get_from() const
              { return from; }

           /// the number after the command character (invalid if none)
           const LineLabel & get_to() const
              { return to; }

           /// set the first line (e.g. a default; the mode is unchanged)
           void set_from(const LineLabel & m)
              { from = m; }

           /// set the last line (e.g. a default; the mode is unchanged)
           void set_to(const LineLabel & N)
              { to = N; }

           /// the parser has seen the number \b N before the command character
           void parse_from(const LineLabel & N);

           /// the parser has seen the number \b N after the command character
           void parse_to(const LineLabel & N);

           /// the parser has seen a - . Return false if that is not allowed
           /// (e.g. a second -)
           bool parse_minus();

           /// the parser has seen another number \b N after a number (e.g. the
           /// 4 in [∆1 4]), which makes a list. Return false if that is not
           /// allowed (e.g. in [2⎕3 4]).
           bool parse_item(const LineLabel & N);

           /// the line numbers of a list (LSM_vec), as entered
           const vector<LineLabel> & get_items() const
              { return items; }

        protected:
           /// the number before the command character (invalid if none)
           LineLabel from;

           /// the number after the command character (invalid if none)
           LineLabel to;

           /// the (syntactic) form of the line numbers entered so far
           Mode mode;

           /// the line numbers of a list V W ... (LSM_vec)
           vector<LineLabel> items;
      };

   /// the line numbers of the current editor command
   Line_set line_set;

   /// true iff this function existed before opening it
   bool function_existed;

   /// true if the function was modified
   bool modified;

   /// true if the function to be editied shall be closed immediately
   /// after processing the first editor line.
   bool trailing_nabla;

   /// true iff this function shall be closed
   bool do_close;

   /// true iff this function shall be locked
   bool locked;

   /// true iff the opening line ended with ⍫ (rather than ∇)
   bool trailing_lock;

   /// true iff the user has abandoned editing with [→]
   bool abandoned;

   /// \b true if the user has (most likely interactively) entered a line
   /// number (so that the input lines are out of order, as opposed to
   /// scripts where the lines are usually entered in order)
   bool out_of_order;

   /// the line number for the currently edited line
   LineLabel current_line;

   /// the command used to open the function
   UCS_string first_command;

   /// the text for line \b current_line
   UCS_string current_text;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __NABLA_HH_DEFINED__
