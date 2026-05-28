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

#ifndef __LINEINPUT_HH_DEFINED__
#define __LINEINPUT_HH_DEFINED__

#if HAVE_TERMIOS_H
#  include <termios.h>
#else
struct termios { int x; };
#endif // HAVE_TERMIOS_H

#include "Output.hh"
#include "PrintOperator.hh"
#include "UCS_string.hh"
#include "UCS_string_vector.hh"

class Nabla;

//----------------------------------------------------------------------------
/// kind of input
enum LineInputMode
{
   LIM_ImmediateExecution = 0,
   LIM_Quote_Quad         = 1,
   LIM_Quad_Quad          = 2,
   LIM_Quad_INP           = 3,
   LIM_Nabla              = 4,
};
//----------------------------------------------------------------------------
/// the lines that the user has previously entered
class LineHistory
{
public:
   /// constructor: empty line history
   /// @param maxl maximum number of history lines to retain
   LineHistory(int maxl);

   /// constructor: line history from function being ∇-edited
   /// @param nabla the Nabla editor whose lines seed the history
   LineHistory(const Nabla & nabla);

   /// print relevant indices
   ostream &  info(ostream & out) const
      { return out << "   CUR=" << current_line
                   << "/P=" << put
                   << "/S=" << hist_lines.size() << endl; }

   /// start a new up/down sequence
   void next()
      { current_line = put;
        if (current_line < 0)   current_line += hist_lines.size();   // wrap
        if (current_line >= int(hist_lines.size()))
           current_line = 0;  // wrap
      }

   /// print history to \b out, maybe filter wirh filter
   /// @param out output stream to print to
   /// @param filter substring filter; only matching lines are printed
   void print_history(ostream & out, const UCS_string & filter) const;

   /// add one line to \b this history
   /// @param line the line to append to the history
   void add_line(const UCS_string & line);

   /// clear history
   /// @param out output stream for confirmation message
   void clear_history(ostream & out);

   /// update history search substring
   const void clear_search(void);

   /// move to next newer entry
   const UCS_string * down();

   /// read history from file
   /// @param filename path to the history file to read
   void read_history(const char * filename);

   /// replace the last line of \b this history
   /// @param line replacement text for the most recent history entry
   void replace_line(const UCS_string & line);

   /// save history to file
   /// @param filename path to the history file to write
   void save_history(const char * filename);

   /// find entries like current line in history
   /// @param cur_line the current user-input line; updated to matched entry
   const UCS_string * search(UCS_string &cur_line);

   /// update search substring (called when current line is edited)
   /// @param cur_line the current user-input line used as new search key
   const void update_search(const UCS_string &cur_line);

   /// move to next older entry
   const UCS_string * up();

   /// the history for ⎕INP
   static LineHistory quad_INP_history;

   /// the history for ⎕
   static LineHistory quad_quad_history;

   /// the history for ⍞
   static LineHistory quote_quad_history;

protected:
   /// the current searched-for substring
   UCS_string cur_search_substr;

   /// the current line (controlled by up()/down())
   int current_line;

   /// the history
   UCS_string_vector hist_lines;

   /// the last searched-for line match
   int last_search_line;

   /// the max. history size
   const int max_lines;

   /// the oldest line
   int put;
};
//----------------------------------------------------------------------------
/// a context for one user-input line
class LineEditContext
{
public:
   /// constructor
   /// @param mode input mode (immediate execution, ⍞, ⎕, etc.)
   /// @param rows number of terminal screen rows
   /// @param cols number of terminal screen columns
   /// @param hist line history for up/down navigation
   /// @param prmt prompt string displayed before user input
   LineEditContext(LineInputMode mode, int rows, int cols,
                   LineHistory & hist, const UCS_string & prmt);

   /// destructor
   ~LineEditContext();

   /// return the number of screen columns
   int get_screen_cols() const
      { return screen_cols; }

   /// return the number of screen rows
   int get_screen_rows() const
      { return screen_rows; }

   /// total length (prompt + user_line)
   int get_total_length() const
      { return prompt.size() + user_line.size(); }

   /// return current user input
   const UCS_string & get_user_line() const
      { return user_line; }

   /// return true if prompt + offset is on column 0
   bool on_bad_col(int offset) const
      { const int  col = (prompt.size() + offset) % screen_cols;
        return col == 0 || col == (screen_cols - 1); }

   /// move cursor left and delete char
   void backspc()
      { if (uidx > 0)   { move_idx(--uidx);   delete_char(); } }

   /// clear (after ^C)
   void clear()
      { user_line.clear(); }

   /// move cursor to end
   void cursor_END()   { move_idx(uidx = user_line.size()); }

   /// move cursor home
   void cursor_HOME()   { move_idx(uidx = 0); }

   /// move cursor left
   void cursor_LEFT()   { if (uidx > 0)   move_idx(--uidx); }

   /// move cursor right
   void cursor_RIGHT()   { if (uidx < user_line.ssize())   move_idx(++uidx); }

   /// get current cursor column
   int get_idx(void)
      { return uidx; }

   /// move the cursor
   /// @param new_idx new cursor offset into user_line
   void move_idx(int new_idx)
      { uidx = new_idx;   set_cursor(); }

   /// refresh cursor if its position may be wrong (due to forward or
   /// backward wraparound being enabled or not)
   void refresh_wrapped_cursor()
      { if (on_bad_col(uidx))   set_cursor(); }

   /// set the cursor (writing the appropriate ESC sequence to CIN)
   void set_cursor()
      { const int offs = uidx + prompt.size();
        CIN.set_cursor(offs/screen_cols - allocated_height, offs%screen_cols);
      }

   /// adjust (increment) the allocated height (long lines)
   void adjust_allocated_height();

   /// reset search substring
   void cursor_CLEAR_SEARCH();

   /// move forward in history
   void cursor_DOWN();

   /// search line history
   void cursor_SEARCH();

   /// move backwards in history
   void cursor_UP();

   /// cut from cursor to end of line
   void cut_to_EOL();

   /// delete char at cursor position
   void delete_char();

   /// insert uni at cursor position
   /// @param uni Unicode character to insert at the current cursor position
   void insert_char(Unicode uni);

   /// paste to cursor position
   void paste();

   /// refresh screen from prompt (including) onwards
   void refresh_all();

   /// refresh screen from cursor onwards
   void refresh_from_cursor();

   /// tab expansion
   /// @param mode current input mode (affects completion candidates)
   void tab_expansion(LineInputMode mode);

   /// toggle the insert mode
   void toggle_ins_mode();

   /// shortcut calling update_search() of member \b history with argument
   /// \b user_line
   void update_SEARCH();

protected:
   /// the number of screen rows used for editing
   int allocated_height;

   /// the line history
   LineHistory history;

   /// true if history was entered
   bool history_entered;

   /// true if input is in insert mode (as opposed to replace mode)
   bool ins_mode;

   /// the prompt
   UCS_string prompt;

   /// the number of screen columns
   int screen_cols;

   /// the number of screen rows
   int screen_rows;

   /// current offset into user_line
   int uidx;

   /// the line being edited
   UCS_string user_line;

   /// ditto
   UCS_string user_line_before_history;

   /// a buffer for ^K/^Y
   static UCS_string cut_buffer;
};
//----------------------------------------------------------------------------
/// a callback function to be called instead of get_line()
typedef void get_line_cb(LineInputMode mode, const UCS_string & prompt,
                         UCS_string & line, bool & eof, LineHistory & hist);

/** InputMux fetches one line from either an input file, or interactively
   from the user if no input file is present
 **/
/// a multiplexer selecting the input to the APL interpreter
class InputMux
{
public:
   /// install a get_line() replacement, return old one
   static get_line_cb * install_get_line_callback(get_line_cb * new_callback)
      {
        get_line_cb * ret = get_line_callback;
        get_line_callback = new_callback;
        return ret;
      }

   /// get one line
   /// @param mode input mode (immediate execution, ⍞, ⎕, etc.)
   /// @param prompt prompt string shown before user input
   /// @param line output: the line entered by the user
   /// @param eof output: set to true if end-of-file was reached
   /// @param hist line history for up/down navigation
   static void get_line(LineInputMode mode, const UCS_string & prompt,
                        UCS_string & line, bool & eof, LineHistory & hist);

protected:
   /// the callback that was installed with \b install_get_line_callback().
   /// It will be called instead of \b get_line() if non-0
   static get_line_cb * get_line_callback;
};
//----------------------------------------------------------------------------
/// a class for obtaining one line of input from the user (editable)
class LineInput
{
public:
   /// add a line to the history
   /// @param line the line to append to the history
   static void add_history_line(const UCS_string & line)
      {  the_line_input->history.add_line(line); }

   /// clear history
   /// @param out output stream for confirmation message
   static void clear_history(ostream & out)
      {  the_line_input->history.clear_history(out); }

   /// close this line input, maybe updating the history
   /// @param do_not_write_hist true to suppress writing history to disk
   static void close(bool do_not_write_hist)
      { if (the_line_input && do_not_write_hist)
            the_line_input->write_history = false;
        delete the_line_input;   the_line_input = 0; }

   /// return the history
   static LineHistory & get_history()
      { return the_line_input->history; }

   /// print history to \b out
   /// @param out output stream to print to
   /// @param filter substring filter; only matching lines are printed
   static void print_history(ostream & out, const UCS_string & filter)
      {  the_line_input->history.print_history(out, filter); }

   /// replace the last line of the history
   /// @param line replacement text for the most recent history entry
   static void replace_history_line(const UCS_string & line)
      {  the_line_input->history.replace_line(line); }

   /// get a line from from user
   /// @param mode input mode (immediate execution, ⍞, ⎕, etc.)
   /// @param prompt prompt string shown before user input
   /// @param user_line output: the line entered by the user
   /// @param eof output: set to true if end-of-file was reached
   /// @param hist line history for up/down navigation
   static void edit_line(LineInputMode mode, const UCS_string & prompt,
                         UCS_string & user_line, bool & eof,
                         LineHistory & hist);

   /// get a line from the user
   /// @param mode input mode (immediate execution, ⍞, ⎕, etc.)
   /// @param prompt prompt string shown before user input
   /// @param line output: the line entered by the user
   /// @param eof output: set to true if end-of-file was reached
   /// @param hist line history for up/down navigation
   static void get_terminal_line(LineInputMode mode, const UCS_string & prompt,
                                 UCS_string & line, bool & eof,
                                 LineHistory & hist);

   /// initialize the input subsystem
   /// @param do_read_history true to restore history from disk at startup
   static void init(bool do_read_history);

   // old-fashioned ^N (shift out) / ^O (shift in) replacement for the ALT keys
   /// @param alt_map_profile keyboard mapping profile index
   /// @param line input/output line with characters to remap
   static void map_alt(int alt_map_profile, UCS_string & line);

   /// undo init()
   static void restore_termios();

protected:
   /// constructor
   /// @param do_read_history true to restore history from disk at startup
   LineInput(bool do_read_history);

   /// destructor
   ~LineInput();

   /// get one character from user
   static Unicode get_uni();

   /// interrupt-safe fgetc()
   static int safe_fgetc();

   /// the current stdin termios.
   termios current_termios;

   /// lines previously entered
   LineHistory history;

   /// write history when done
   bool write_history;

   /// the stdin termios at startup of the interpreter. Will be restored
   /// when the interpreter exits.
   static termios initial_termios;

   /// the first tcgetattr() errno (or 0 if none)
   static int initial_termios_errno;

   /// map all subsequent ASCII chars to APL
  static bool map_all;

   /// map the next ASCII char to APL
  static bool map_next;

   /// single LineInput instance that restores stdin termios on destruction
   static LineInput * the_line_input;
};
//----------------------------------------------------------------------------
/** A mapping from ESC sequences to (internal) pseudo-Unicodes such as
    UNI_CursorUp and friends
 */
/// A mapping from ANSI ESC sequences to internal Unicodes
struct ESCmap
{
   /// return true if seq is a true prefix of \b this ESCmap
   /// @param seq the byte sequence to test
   /// @param seq_len length of \b seq in bytes
   bool has_prefix(const char * seq, int seq_len) const;

   /// return true if seq is the sequence of \b this ESCmap
   /// @param seq the byte sequence to test
   /// @param seq_len length of \b seq in bytes
   bool is_equal(const char * seq, int seq_len) const;

   /// return true if an entry has prefix \b seq of length len
   /// @param seq the byte sequence to test
   /// @param len length of \b seq in bytes
   static bool need_more(const char * seq, int len);

   /// refresh the lengths (after keyboard strings have been updated)
   static void refresh_lengths();

   /// the length of \b seqence
   int len;

   /// the escape sequence (including ESC, not 0-terminated!)
   const char * seqence;

   /// the (pseudo-) Unicode for the sequence
   Unicode uni;

   /// a mapping from keyboard escape sequences to Unicodes
   static ESCmap the_ESCmap[];
};
//----------------------------------------------------------------------------

#endif // __LINEINPUT_HH_DEFINED__
