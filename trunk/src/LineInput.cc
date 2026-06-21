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

#include <errno.h>
#include <signal.h>

#if HAVE_WINDOWS_H
# include <io.h>      // _setmode
# include <windows.h>
#endif

#include "Assert.hh"
#include "Command.hh"
#include "Common.hh"
#include "Error.hh"
#include "InputFile.hh"
#include "IO_Files.hh"
#include "LineInput.hh"
#include "Nabla.hh"
#include "Parallel.hh"
#include "SystemVariable.hh"
#include "Sys.hh"
#include "TabExpansion.hh"
#include "UserPreferences.hh"
#include "Workspace.hh"

struct termios LineInput::initial_termios = { 0 };
int LineInput::initial_termios_errno = 0;

#if MINGW_SRC
unsigned long LineInput::initial_console_mode = 0;
// Ring buffer and state for ReadConsoleInputW-based keyboard input.
// Declared here (before any constructor) so all member functions can see them.
static unsigned char g_kbuf[64];
static int           g_khead      = 0;
static int           g_ktail      = 0;
static bool          g_is_console = false;
static bool          g_keof       = false;
#endif

// hooks for external editors (emacs)
extern void (*start_input)();
void (*start_input)() = 0;

extern void (*end_input)();
void (*end_input)() = 0;

LineInput * LineInput::the_line_input = 0;

bool LineInput::map_next = false;
bool LineInput::map_all  = false;
//════════════════════════════════════════════════════════════════════════════

LineHistory LineHistory::quote_quad_history(10);
LineHistory LineHistory::quad_quad_history(10);
LineHistory LineHistory::quad_INP_history(2);

UCS_string LineEditContext::cut_buffer;

get_line_cb * InputMux::get_line_callback = 0;

//════════════════════════════════════════════════════════════════════════════

ESCmap ESCmap::the_ESCmap[] =
{
   // normal sequences
  { 3, Output::ESC_CursorUp,      UNI_CursorUp     },
  { 3, Output::ESC_CursorDown,    UNI_CursorDown   },
  { 3, Output::ESC_CursorRight,   UNI_CursorRight  },
  { 3, Output::ESC_CursorLeft,    UNI_CursorLeft   },
  { 3, Output::ESC_CursorEnd,     UNI_CursorEnd    },
  { 3, Output::ESC_CursorHome,    UNI_CursorHome   },
  { 4, Output::ESC_InsertMode,    UNI_InsertMode   },
  { 4, Output::ESC_Delete,        UNI_DELETE },

   // sequences with SHIFT and/or CTRL
  { 6, Output::ESC_CursorUp_1,    UNI_CursorUp     },
  { 6, Output::ESC_CursorDown_1,  UNI_CursorDown   },
  { 6, Output::ESC_CursorRight_1, UNI_CursorRight  },
  { 6, Output::ESC_CursorLeft_1,  UNI_CursorLeft   },
  { 6, Output::ESC_CursorEnd_1,   UNI_CursorEnd    },
  { 6, Output::ESC_CursorHome_1,  UNI_CursorHome   },
  { 6, Output::ESC_InsertMode_1,  UNI_InsertMode   },
  { 6, Output::ESC_Delete_1,      UNI_DELETE },
};

enum { ESCmap_entry_count = sizeof(ESCmap::the_ESCmap) / sizeof(ESCmap) };

//════════════════════════════════════════════════════════════════════════════
LineHistory::LineHistory(int maxl)
   : current_line(0),
     last_search_line(0),
     max_lines(maxl),
     put(0)
{
const UCS_string u(U"xxx");
   add_line(u);
}
//────────────────────────────────────────────────────────────────────────────
LineHistory::LineHistory(const Nabla & nabla)
   : current_line(0),
     last_search_line(0),
     max_lines(1000),
     put(0)
{
   UCS_string u(U"xxx");
   add_line(u);
int cl = -1;
   loop(l, nabla.get_line_count())
       {
         bool on_current = false;
         const UCS_string lab_text = nabla.get_label_and_text(l, on_current);
         add_line(lab_text);   // overrides this->current_line !
         if (on_current)   cl = l + 1;   // + 1 due to "xxx" above
       }

   if (cl != -1)   current_line = cl;
}
//────────────────────────────────────────────────────────────────────────────
void
LineHistory::print_history(ostream & out, const UCS_string & filter) const
{
   // hist_lines is a ring buffer. first print its tail, then its head.

   for (ShapeItem p = put + 1; p < hist_lines.ssize(); ++p)   // tail
      {
        if (filter.size() && !hist_lines[p].starts_iwith(filter))   continue;
        out << "      " << hist_lines[p] << endl;
      }
   for (int p = 0; p < put; ++p)                          // head
      {
        if (filter.size() && !hist_lines[p].starts_iwith(filter))   continue;
        out << "      " << hist_lines[p] << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
LineHistory::add_line(const UCS_string & line)
{
   if (max_lines == 0)      return;   // no history
   if (!line.has_black())   return;   // almost empty

   // a repeated cut-and-paste of entire lines increases the indentation every
   // time due to the APL input prompt). We therefore limit this effect
   // to 6 blanks.
   //
UCS_string line1 = line;
   line1.remove_leading_and_trailing_whitespaces();

   if (int(hist_lines.size()) < max_lines)   // append
      {
        hist_lines.push_back(line1);
        put = 0;
      }
   else                            // override
      {
        if (put >= int(hist_lines.size()))   put = 0;   // wrap
        hist_lines[put++] = line1;
      }

   next();   // update current_line
}
//────────────────────────────────────────────────────────────────────────────
void
LineHistory::clear_history(ostream & out)
{
   current_line = 0;
   put = 0;
   hist_lines.clear();
UCS_string u((U"xxx"));
   add_line(u);
}
//────────────────────────────────────────────────────────────────────────────
const void
LineHistory::clear_search(void)
{
    cur_search_substr.clear();
}
//────────────────────────────────────────────────────────────────────────────
const UCS_string *
LineHistory::down()
{
   if (hist_lines.size() == 0)     return 0;   // no history
   if (current_line == put)   return 0;

int new_current_line = current_line + 1;
   if (new_current_line >= int(hist_lines.size()))
      new_current_line = 0;   // wrap
   current_line = new_current_line;
   if (current_line == put)   return 0;

   return &hist_lines[current_line];
}
//────────────────────────────────────────────────────────────────────────────
void
LineHistory::read_history(const char * filename)
{
FileReader reader(filename);
   if (!reader)
      {
        Log(LOG_get_line)
           CERR << "Cannot open history file " << filename
                << ": " << strerror(errno) << endl;
        return;
      }

char buffer[4000];
   while (reader.fgets(buffer, sizeof(buffer) - 1))
       {
         buffer[sizeof(buffer) - 1] = 0;

         int slen = strlen(buffer);
         if (slen && (buffer[slen - 1] == '\n'))   buffer[--slen] = 0;
         if (slen && (buffer[slen - 1] == '\r'))   buffer[--slen] = 0;

         UTF8_string utf(buffer);
         UCS_string ucs(utf);
         add_line(ucs);
       }

   next();
}
//────────────────────────────────────────────────────────────────────────────
void
LineHistory::replace_line(const UCS_string & line)
{
   if (put > 0)   hist_lines[put - 1] = line;
   else           hist_lines.back() = line;
}
//────────────────────────────────────────────────────────────────────────────
void
LineHistory::save_history(const char * filename)
{
   if (hist_lines.size() == 0)   return;

ofstream outf(filename);
   if (!outf.is_open())
      {
        CERR << "Cannot write history file " << filename
             << ": " << strerror(errno) << endl;
         return;
      }

int count = 0;
   for (ShapeItem p = put + 1; p < hist_lines.ssize(); ++p)
      {
        outf << hist_lines[p] << endl;
        ++count;
      }
   for (int p = 0; p < put; ++p)
      {
        outf << hist_lines[p] << endl;
        ++count;
      }

   Log(LOG_get_line)
      cerr << count << " history lines written to " << filename << endl;
}
//────────────────────────────────────────────────────────────────────────────
const UCS_string *
LineHistory::search(UCS_string &cur_line)
{
    if( hist_lines.size() == 0 ) return 0;  // no history

    // For now, a simple substring search of hist_lines[]
    int search_start_line = last_search_line - 1;
    if( search_start_line < 0) {
        search_start_line = hist_lines.size()-1;
    }
    int idx = search_start_line;
    bool found = false;
    do {
        if( hist_lines[idx].substr_pos(cur_search_substr) >= 0 ) {
            current_line = idx;
            found = true;
            continue;
        }

        idx--;
        if( idx < 0 ) {
            idx = hist_lines.size()-1;
        }
        if( idx == search_start_line ) {
            break;
        }
    } while(!found);

    if( !found ) {
        idx = hist_lines.size()-1;
    }

    last_search_line = idx;
    if( idx == 0 ) return 0;

    return &hist_lines[current_line];
}
//────────────────────────────────────────────────────────────────────────────
const void
LineHistory::update_search(const UCS_string &cur_line)
{
    cur_search_substr = cur_line;
}
//────────────────────────────────────────────────────────────────────────────
const UCS_string *
LineHistory::up()
{
   if (hist_lines.size() == 0)   return 0;   // no history

int new_current_line = current_line - 1;
    if (new_current_line < 0)   new_current_line += hist_lines.size();   // wrap
    if (new_current_line == put)   return 0;

   return &hist_lines[current_line = new_current_line];
}
//════════════════════════════════════════════════════════════════════════════
LineEditContext::LineEditContext(LineInputMode mode, int rows, int cols,
                                 LineHistory & hist, const UCS_string & prmt)
   : allocated_height(1),
     history(hist),
     history_entered(false),
     ins_mode(true),
     screen_cols(cols),
     screen_rows(rows),
     uidx(0)
{
   if (mode == LIM_Quote_Quad)
      {
        // the prompt was printed by ⍞ already. Make it the beginning of
        // user_line so that it can be edited.
        //
        user_line = prmt.no_pad();
        uidx = user_line.size();
      }
   else
      {
        prompt = prmt.no_pad();
      }

   refresh_all();
}
//────────────────────────────────────────────────────────────────────────────
LineEditContext::~LineEditContext()
{
   // restore block cursor
   //
   if (!ins_mode)   CIN << "\x1B[1 q" << flush;
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::adjust_allocated_height()
{
const int rows = 1 + get_total_length() / screen_cols;

   if (allocated_height >= rows)   return;

   // scroll some lines so that prior text is not overridden.
   //
   CIN.set_cursor(-1, 0);
   loop(a, rows - allocated_height)   CIN << endl;

   allocated_height = rows;

   // redraw screen from -allocated_height:0 onwards
   //
   if (CIN.can_clear_EOS())
      {
        CIN.set_cursor(-allocated_height, 0);
        CIN.clear_EOS();
      }
   else
      {
        loop(a, allocated_height)
           {
             CIN.set_cursor(a - allocated_height, 0);
             CIN.clear_EOL();
           }
      }

   refresh_all();
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::cursor_CLEAR_SEARCH()
{
   Log(LOG_get_line)   history.info(CERR << "cursor_CLEAR_SEARCH()") << endl;
   history.clear_search();
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::cursor_DOWN()
{
   Log(LOG_get_line)   history.info(CERR << "cursor_DOWN()" << endl);

const UCS_string * ucs = history.down();
   if (ucs == 0)   // no line below
      {
        Log(LOG_get_line)   CERR << "hit bottom of history()" << endl;
        // if inside history: restore user_line
        //
        if (history_entered)   user_line = user_line_before_history;
        history_entered = false;
        goto refresh;
      }

   if (!history_entered)   // not yet in history: remember user_line
      {
        user_line_before_history = user_line;
        history_entered = true;
      }

   user_line = *ucs;
   adjust_allocated_height();

refresh:
   uidx = 0;
   refresh_from_cursor();
   move_idx(user_line.size());
   Log(LOG_get_line)   history.info(CERR << "cursor_DOWN() done" << endl);
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::cursor_SEARCH()
{
    user_line_before_history = user_line;
    history_entered = true;

    const UCS_string * ucs = history.search(user_line_before_history);
    if (ucs == 0)   // no line above
    {
        Log(LOG_get_line)
           {
             CERR << "hit top of history()" << endl;
             history.info(CERR << "cursor_SEARCH() done" << endl);
           }
        return;
    }

    adjust_allocated_height();

    uidx = 0;
    user_line = *ucs;
    refresh_from_cursor();
    move_idx(user_line.size());
    Log(LOG_get_line)   history.info(CERR << "cursor_SEARCH() done" << endl);
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::cursor_UP()
{
   Log(LOG_get_line)   history.info(CERR << "cursor_UP()") << endl;

const UCS_string * ucs = history.up();
   if (ucs == 0)   // no line above
      {
        Log(LOG_get_line)   CERR << "hit top of history()" << endl;
        Log(LOG_get_line)   history.info(CERR << "cursor_UP() done" << endl);
        return;
      }

   if (!history_entered)   // not yet in history: remember user_line
      {
        user_line_before_history = user_line;
        history_entered = true;
      }

   user_line = *ucs;
   adjust_allocated_height();

   uidx = 0;
   refresh_from_cursor();
   move_idx(user_line.size());
   Log(LOG_get_line)   history.info(CERR << "cursor_UP() done" << endl);
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::cut_to_EOL()
{
   if (uidx >= user_line.ssize())   return;   // nothing to cut

   cut_buffer = UCS_string(user_line, uidx);
   user_line.resize(uidx);
   refresh_from_cursor();
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::delete_char()
{
   if (uidx == (user_line.ssize() - 1))   // cursor on last char
      {
        CIN << ' ' << UNI_BS;
        user_line.pop_back();
      }
   else
      {
        user_line.erase(uidx);
        refresh_from_cursor();
      }

   if (get_total_length() >= screen_cols)   set_cursor();
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::insert_char(Unicode uni)
{
   if (uidx >= user_line.ssize())   // append char
      {
        user_line << uni;
        adjust_allocated_height();
        refresh_wrapped_cursor();
        CIN << uni;
      }
   else if (ins_mode)              // insert char
      {
        user_line.insert(uidx, uni);
        adjust_allocated_height();
        refresh_wrapped_cursor();
        refresh_from_cursor();
      }
   else                            // replace char
      {
        user_line[uidx] = uni;
        adjust_allocated_height();
        refresh_wrapped_cursor();
        refresh_from_cursor();
      }

   move_idx(uidx + 1);
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::paste()
{
   if (cut_buffer.size() == 0)   return;

   if (uidx >= user_line.ssize())   // append cut buffer
      {
        user_line << cut_buffer;
      }
   else                            // insert cut buffer
      {
        const UCS_string rest(user_line, uidx);
        user_line.resize(uidx);
        user_line << cut_buffer << rest;
      }

   refresh_from_cursor();
   move_idx(uidx + cut_buffer.size());
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::refresh_all()
{
const int saved_uidx = uidx;
   uidx = 0;

   CIN.set_cursor(-allocated_height, 0);
   CIN << prompt << user_line;
   refresh_from_cursor();
   move_idx(saved_uidx);
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::refresh_from_cursor()
{
const int saved_uidx = uidx;

   adjust_allocated_height();
   set_cursor();
   for (; uidx < user_line.ssize(); ++uidx)
       {
         refresh_wrapped_cursor();
         CIN << user_line[uidx];
       }

   // clear from end of user_line
   //
   move_idx(user_line.size());
   if (CIN.can_clear_EOS())
      {
        CIN.clear_EOS();
      }
   else
      {
        CIN.clear_EOL();

        // clear subsequent lines
        //
        for (int a = 1; a < allocated_height; ++a)
            {
              CIN.set_cursor(a - allocated_height, 0);
              CIN.clear_EOL();
            }
      }

   move_idx(saved_uidx);
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::tab_expansion(LineInputMode mode)
{
   if (mode != LIM_ImmediateExecution)   return;

UCS_string line = user_line;
TabExpansion tab_exp(line);
const ExpandResult expand_result = tab_exp.expand_tab(line);

   switch(expand_result)
      {
        case ER_IGNORE: return;

        case ER_AGAIN:
             // expand_tab has shown a list of options.
             // Reset the input window and redisplay.
             allocated_height = 1;
             adjust_allocated_height();
             refresh_all();
             return;

        case ER_REPLACE:
             user_line.clear();
             user_line << line;
             uidx = 0;
             refresh_from_cursor();
             move_idx(user_line.size());
             return;

        default: FIXME;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::toggle_ins_mode()
{
    ins_mode = ! ins_mode;

   // CSI [0 q       : blinking block
   // CSI [1 q       : blinking block
   // CSI [2 q       : steady   block
   // CSI [3 q       : blinking underline
   // CSI [4 q       : steady   underline
   // CSI [5 q       : blinking bar (doesn't work) 
   // CSI [6 q       : steady   bar (doesn't work) 

   if (ins_mode)   CIN << "\x1B[0 q" << flush;
   else            CIN << "\x1B[3 q" << flush;
}
//────────────────────────────────────────────────────────────────────────────
void
LineEditContext::update_SEARCH(void)
{
    history.update_search(user_line);
}
//════════════════════════════════════════════════════════════════════════════
void
InputMux::get_line(LineInputMode mode, const UCS_string & prompt,
                   UCS_string & line, bool & eof, LineHistory & hist)
{
   if (get_line_callback)
      {
        get_line_callback(mode, prompt, line, eof, hist);
        return;
      }

   if (InputFile::is_validating())   Quad_QUOTE::done(true, LOC);

   InputFile::increment_current_line_no();

   // check if we have input from a file. We do NOT use the file if the input
   // is for ⍞ unless we are in a .tc testcase file.
   //
bool interactive = (mode == LIM_Quote_Quad) || (mode == LIM_Quad_Quad);
   if (InputFile::is_validating())   interactive = false;
   if (InputFile::pushed_file())     interactive = true;

   if (!interactive)
      {
        UTF8_string file_line;
        bool file_eof = false;
        IO_Files::get_file_line(file_line, file_eof);

        if (!file_eof)
           {
             line = UCS_string(file_line);

             switch(mode)
                {
                  case LIM_ImmediateExecution:
                  case LIM_Quad_Quad:
                  case LIM_Quad_INP:
                       CIN << prompt << line << endl;
                       break;

                  case LIM_Quote_Quad:
                       line = prompt;

                       // for each leading backspace in line: discard last
                       // prompt character. This is for testing the user
                       // backspacing over the ⍞ prompt
                       //
                       while (line.size()      &&
                              file_line.size() &&
                              file_line[0] == UNI_BS)
                             {
                               file_line.erase(0);
                               line.pop_back();
                             }
                       line << file_line;
                       break;

                  case LIM_Nabla:
                       break;

                  default: FIXME;
             }

          return;
        }
   }

   // no (more) input from files: get line from terminal
   //
   if (UserPreferences::uprefs.raw_cin)
      {
        Quad_QUOTE::done(mode != LIM_Quote_Quad, LOC);
        CIN << '\r' << prompt;
        char buffer[4000];
        const APL_time_us from = now();
         const char * s = fgets(buffer, sizeof(buffer) - 1, stdin);
         Workspace::add_wait(now() - from);

        if (s == 0)
           {
             eof = true;
             return;
           }
        buffer[sizeof(buffer) - 1] = 0;

        int slen = strlen(buffer);
        if (slen && buffer[slen - 1] == '\n')   buffer[--slen] = 0;
        if (slen && buffer[slen - 1] == '\r')   buffer[--slen] = 0;

        UTF8_string line_utf(buffer);
        line = UCS_string(line_utf);
        return;
      }

   Quad_QUOTE::done(mode != LIM_Quote_Quad, LOC);

const APL_time_us from = now();
   if (start_input)   (*start_input)();

#if PARALLEL_ENABLED
   CPU_pool::lock_pool(false);
#endif

   for (int control_D_count = 0;;)
       {
         bool _eof = false;
         LineInput::get_terminal_line(mode, prompt, line, _eof, hist);
         if (!_eof)   break;

         ++control_D_count;

         // ^D or end of file
         if (UserPreferences::uprefs.control_Ds_to_exit)   // ^D limit desired
            {
              if (control_D_count >= UserPreferences::uprefs.control_Ds_to_exit)
                 {
                   CIN << endl;
#if PARALLEL_ENABLED
                   Thread_context::kill_pool();
#endif // PARALLEL_ENABLED
                   UserPreferences::uprefs.silence = NO_BANNER;   // exit silently
                   Command::cmd_OFF(4);    // exit()s
                   return;  // not reached
                 }
            }
         else if (control_D_count < 5)
            {
              CIN << endl;
              COUT << "      ^D or end-of-input detected ("
                   << control_D_count << "). Use )OFF to leave APL!"
                   << endl;
           }

         eof = true;

         if (control_D_count > 10 && (now() - from)/control_D_count < 10000)
            {
              // we got 10 or more times EOF (or possibly ^D) at a rate
              // of 10 ms or faster. That looks like end-of-input rather
              // than ^D typed by the user. Abort the interpreter.
              //
              CIN << endl;
              COUT << "      *** end of input" << endl;
#if PARALLEL_ENABLED
              Thread_context::kill_pool();
#endif
              Command::cmd_OFF(2);   // exit()s
              return;  // not reached
            }
      }
#if PARALLEL_ENABLED
   CPU_pool::unlock_pool(false);
#endif

   Log(LOG_get_line)   CERR << " '" << line << "'" << endl;

   Workspace::add_wait(now() - from);
   if (end_input)   (*end_input)();

   if (UserPreferences::uprefs.echo_CIN)   COUT << prompt << line << endl;
}
//════════════════════════════════════════════════════════════════════════════
void
LineInput::edit_line(LineInputMode mode, const UCS_string & prompt,
                     UCS_string & user_line, bool & eof, LineHistory & hist)
{
#if ! MINGW_SRC
   the_line_input->current_termios.c_lflag &= ~ISIG;   // disable ^C
# ifndef apl_TARGET_LIBAPL
   tcsetattr(STDIN_FILENO, TCSANOW, &the_line_input->current_termios);
# endif // apl_TARGET_LIBAPL
#endif // ! MINGW_SRC

   user_line.clear();

LineEditContext lec(mode, 24, Workspace::get_PW(), hist, prompt);

   for (;;)
       {
         const Unicode uni = get_uni();
         switch(uni)
            {
              case UNI_InsertMode:
                   lec.toggle_ins_mode();
                   continue;

              case UNI_CursorHome:
                   lec.cursor_HOME();
                   continue;

              case UNI_CursorEnd:
                   lec.cursor_END();
                   continue;

              case UNI_CursorLeft:
                   lec.cursor_LEFT();
                   continue;

              case UNI_CursorRight:
                   lec.cursor_RIGHT();
                   continue;

              case UNI_CursorDown:
                   lec.cursor_DOWN();
                   continue;

              case UNI_CursorUp:
                   lec.cursor_UP();
                   continue;

              case UNI_DC2:  // ^R - search line history
                   lec.cursor_SEARCH();
                   continue;

              case UNI_EOF:  // end of file
                   eof = user_line.size() == 0;
                   break;

              case UNI_ETX:   // ^C
                   lec.clear();
                   InterruptContext::control_C(SIGINT);
                   break;


#ifdef cfg_WANT_CTRLD_DEL
              case UNI_SUB:   // ^Z
                   CERR << "^Z";
                   eof = true;
                   break;

              case UNI_EOT:   // ^D
                   lec.delete_char();
                   lec.update_SEARCH();
                   continue;
#else
              case UNI_EOT:   // ^D
                   CERR << "^D";
                   eof = true;
                   break;
#endif

              case UNI_BS:    // ^H (backspace)
                   lec.backspc();
                   lec.update_SEARCH();
                   continue;

              case UNI_HT:    // ^I (tab)
                   lec.tab_expansion(mode);
                   lec.update_SEARCH();
                   continue;

              case UNI_VT:    // ^K
                   lec.cut_to_EOL();
                   lec.update_SEARCH();
                   continue;

              case UNI_DELETE:
                   lec.delete_char();
                   lec.update_SEARCH();
                   continue;

              case UNI_CR:   // '\r' : ignore
                   continue;

              case UNI_LF:   // '\n': done
                   lec.cursor_CLEAR_SEARCH();
                   break;

              case UNI_EM:    // ^Y
                   lec.paste();
                   lec.update_SEARCH();
                   continue;

              case Invalid_Unicode:
                   continue;

              default:  // regular APL character
                   lec.insert_char(uni);
                   lec.update_SEARCH();
                   continue;
            }

         break;
       }

#if ! MINGW_SRC
   the_line_input->current_termios.c_lflag |= ISIG;   // enable ^C
#ifndef apl_TARGET_LIBAPL
   tcsetattr(STDIN_FILENO, TCSANOW, &the_line_input->current_termios);
#endif // apl_TARGET_LIBAPL
#endif // ! MINGW_SRC

   user_line = lec.get_user_line();

   // maybe add history line
   //
bool add_hist = false;
   switch(mode)
      {
        case LIM_ImmediateExecution:
             add_hist = !InputFile::is_validating() && user_line.has_black();
             break;

        case LIM_Quote_Quad:
        case LIM_Quad_Quad:
             add_hist = !InputFile::is_validating();
             break;

        case LIM_Quad_INP:
             add_hist = false;
             break;

        case LIM_Nabla:
             // ∇-history is handled in a special way in Nabla.cc
             // so we don't do it here.
             //
             add_hist = false;
             break;
      }
 
   if (add_hist)   hist.add_line(user_line);

   CIN << endl;
}
//════════════════════════════════════════════════════════════════════════════
void
LineInput::get_terminal_line(LineInputMode mode, const UCS_string & prompt,
                             UCS_string & line, bool & eof,
                             LineHistory & hist)
{
   // no file input: get line interactively
   //
   switch(mode)
      {
        case LIM_ImmediateExecution:
        case LIM_Quote_Quad:
        case LIM_Quad_Quad:
        case LIM_Quad_INP:
             Output::set_color_mode(Output::COLM_INPUT);

             /* fall through */

        case LIM_Nabla:
             edit_line(mode, prompt, line, eof, hist);
             return;

        default: FIXME;
      }

   Assert(0 && "Bad LineInputMode");
}
//────────────────────────────────────────────────────────────────────────────
void
LineInput::init(bool do_read_history)
{
   the_line_input = new LineInput(do_read_history);
}
//────────────────────────────────────────────────────────────────────────────
void
LineInput::restore_termios()
{
#if MINGW_SRC
   SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), initial_console_mode);
#else
   if (initial_termios_errno == 0)
      {
#ifndef apl_TARGET_LIBAPL
# ifndef apl_TARGET_PYTHON
        tcsetattr(STDIN_FILENO, TCSANOW, &initial_termios);
# endif // not apl_TARGET_PYTHON
#endif // not apl_TARGET_LIBAPL
      }
   initial_termios_errno = 1;   // prevent multiple calls
#endif // MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
LineInput::LineInput(bool do_read_history)
   : history(UserPreferences::uprefs.line_history_len),
     write_history(false)
{
#if MINGW_SRC
   {
      const HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
      GetConsoleMode(hStdin, &initial_console_mode);
      // Disable line buffering and echo; keep ENABLE_PROCESSED_INPUT so that
      // Ctrl+C and other control events are processed correctly.
      // We use ReadConsoleInputW for keyboard input (see win_refill()), so
      // ENABLE_VIRTUAL_TERMINAL_INPUT is not needed here.
      SetConsoleMode(hStdin,
                     (initial_console_mode | ENABLE_PROCESSED_INPUT)
                     & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
      // Switch stdin to binary mode (used for piped/file input fallback).
      _setmode(fileno(stdin), 0x8000);   // 0x8000 = _O_BINARY
      g_is_console = _isatty(fileno(stdin)) != 0;
   }
   if (do_read_history)
      {
        history.read_history(UserPreferences::uprefs.line_history_path.c_str());
        write_history = true;
      }
#else
   initial_termios_errno = 0;

   if (tcgetattr(STDIN_FILENO, &initial_termios))
      initial_termios_errno = errno;

   if (do_read_history)
      {
        history.read_history(UserPreferences::uprefs.line_history_path.c_str());
        write_history = true;
      }

   current_termios = initial_termios;

   // set current_termios to raw mode
   //
   current_termios.c_iflag &= ~( ISTRIP | // don't strip off bit 8
                                 INLCR  | // don't NL → CR
                                 IGNCR);  // don't ignore CR
   current_termios.c_iflag |=    IGNBRK | // ignore break
                                 IGNPAR |
                                 ICRNL  ; // CR → NL

   current_termios.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
   current_termios.c_lflag |= ISIG;

# ifndef apl_TARGET_LIBAPL
#  ifndef apl_TARGET_PYTHON
    tcsetattr(STDIN_FILENO, TCSANOW, &current_termios);
#  endif // apl_TARGET_PYTHON
# endif // apl_TARGET_LIBAPL
#endif // MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
LineInput::~LineInput()
{
   restore_termios();
   if (write_history)
      history.save_history(UserPreferences::uprefs.line_history_path.c_str());
}
//────────────────────────────────────────────────────────────────────────────
Unicode
LineInput::get_uni()
{
again:

const int b0 = safe_fgetc();
   if (b0 == EOF)
      {
        map_next = false;
        map_all  = false;
        return UNI_EOF;
      }

   if (b0 & 0x80)   // non-ASCII unicode
      {
        int len;
        uint32_t bx = b0;   // the "significant" bits in b0
        if ((b0 & 0xE0) == 0xC0)        { len = 2;   bx &= 0x1F; }
        else if ((b0 & 0xF0) == 0xE0)   { len = 3;   bx &= 0x0F; }
        else if ((b0 & 0xF8) == 0xF0)   { len = 4;   bx &= 0x07; }
        else if ((b0 & 0xFC) == 0xF8)   { len = 5;   bx &= 0x03; }
        else if ((b0 & 0xFE) == 0xFC)   { len = 6;   bx &= 0x01; }
        else
           {
#if MINGW_SRC
             goto again;   // discard stray continuation/invalid byte
#else
             CERR << "Bad UTF8 sequence start at " << LOC << endl;
             return Invalid_Unicode;
#endif
           }

        uint32_t uni = 0;
        loop(l, len - 1)
            {
              const UTF8 subc = safe_fgetc();
              if ((subc & 0xC0) != 0x80)
                 {
#if MINGW_SRC
                   // On Windows, 0xE0 + scancode is an extended key (arrow
                   // keys in legacy cmd.exe).  Discard silently.
                   goto again;
#else
                   CERR << "Bad UTF8 sequence: " << HEX(b0)
                        << "... at " LOC << endl;
                   return Invalid_Unicode;
#endif
                 }

              bx  <<= 6;
              uni <<= 6;
              uni |= subc & 0x3F;
            }

        return Unicode(bx | uni);
      }

   // at this point b0 is an ASCII character
   //
#if cfg_ALT_MAP_WANTED
# define keymap(ascii_u, apl_u, ascii_s, apl_s) \
   case ascii_u: return Unicode(apl_u);         \
   case ascii_s: return Unicode(apl_s);

   if (map_next || map_all)
      {
        map_next = false;

        enum { PROFILE = cfg_ALT_MAP_WANTED };
        if (PROFILE == 1)   switch(b0)
           {
             keymap( '1'  , U'¨' , '!' , U'⌶' )
             keymap( '2'  , U'¯' , '@' , U'⍫' )
             keymap( '3'  , U'<' , '#' , U'⍒' )
             keymap( '4'  , U'≤' , '$' , U'⍋' )
             keymap( '5'  , U'=' , '%' , U'⌽' )
             keymap( '6'  , U'≥' , '^' , U'⍉' )
             keymap( '7'  , U'>' , '&' , U'⊖' )
             keymap( '8'  , U'≠' , '*' , U'⍟' )
             keymap( '9'  , U'∨' , '(' , U'⍱' )
             keymap( '0'  , U'∧' , ')' , U'⍲' )
             keymap( '-'  , U'×' , '_' , U'!' )
             keymap( '='  , U'÷' , '+' , U'⌹' )

             keymap( 'q'  , U'?' , 'Q' , U'?' )
             keymap( 'w'  , U'⍵' , 'W' , U'⍹' )
             keymap( 'e'  , U'∊' , 'E' , U'⍷' )
             keymap( 'r'  , U'⍴' , 'R' , U'⍴' )
             keymap( 't'  , '~'  , 'T' , U'⍨' )
             keymap( 'y'  , U'↑' , 'Y' , U'¥' )
             keymap( 'u'  , U'↓' , 'U' , U'↓' )
             keymap( 'i'  , U'⍳' , 'I' , U'⍸' )
             keymap( 'o'  , U'○' , 'O' , U'⍥' )
             keymap( 'p'  , U'⋆' , 'P' , U'⍣' )
             keymap( '['  , U'←' , '{' , U'⍞' )
             keymap( ']'  , U'→' , '}' , U'⍬' )
             keymap( '\\' , U'⊢' , '|' , U'⊣' )

             keymap( 'a'  , U'⍺' , 'A' , U'⍶' )
             keymap( 's'  , U'⌈' , 'S' , U'⌈' )
             keymap( 'd'  , U'⌊' , 'D' , U'⌊' )
             keymap( 'f'  , U'_' , 'F' , U'∇' )
             keymap( 'g'  , U'∇' , 'G' , U'∇' )
             keymap( 'h'  , U'∆' , 'H' , U'⍙' )
             keymap( 'j'  , U'∘' , 'J' , U'⍤' )
             keymap( 'k'  , U'λ' , 'K' , U'λ' )
             keymap( 'l'  , U'⎕' , 'L' , U'⌷' )
             keymap( ';'  , U'⍎' , ':' , U'≡' )
             keymap( '\'' , U'⍕' , '"' , U'≢' )

             keymap( 'z'  , U'⊂' , 'Z' , U'⊂' )
             keymap( 'x'  , U'⊃' , 'X' , U'χ' )
             keymap( 'c'  , U'∩' , 'C' , U'¢' )
             keymap( 'v'  , U'∪' , 'V' , U'∪' )
             keymap( 'b'  , U'⊥' , 'B' , U'⊥' )
             keymap( 'n'  , U'⊤' , 'N' , U'⊤' )
             keymap( 'm'  , '|'  , 'M' , U'μ' )
             keymap( ','  , U'⍝' , '<' , U'⍪' )
             keymap( '.'  , U'⍀' , '>' , U'⍙' )
             keymap( '/'  , U'⌿' , '?' , U'⍠' )
           }
      }

# undef keymap
#endif

   if (b0 == UNI_ESC)
      {
        char seq[Output::MAX_ESC_LEN];   seq[0] = UNI_ESC;
        for (int s = 1; s < Output::MAX_ESC_LEN; ++s)
            {
              const int bs = safe_fgetc();
              if (bs == EOF)   return UNI_EOF;
              seq[s] = bs;

              // check for exact match
              //
              loop(e, ESCmap_entry_count)
                  {
                  if (ESCmap::the_ESCmap[e].is_equal(seq, s + 1))
                     return ESCmap::the_ESCmap[e].uni;
                  }

              // check for prefix match
              //
              if (ESCmap::need_more(seq, s))   continue;

              return Invalid_Unicode;
            }
      }
   else if (b0 < UNI_SPACE)   // ^something (except ESC)
      {
        switch(b0)
           {
             case UNI_SOH: return UNI_CursorHome;    // ^A
             case UNI_STX: return UNI_CursorLeft;    // ^B
             case UNI_ETX: return UNI_ETX;           // ^C
             case UNI_EOT: return UNI_EOT;           // ^D
             case UNI_ENQ: return UNI_CursorEnd;     // ^E
             case UNI_ACK: return UNI_CursorRight;   // ^F
             case UNI_BS:  return UNI_BS;            // ^H
             case UNI_HT:  return UNI_HT;            // ^I
             case UNI_LF:  return UNI_LF;            // ^J
             case UNI_VT:  return UNI_VT;            // ^K
             case UNI_SO:  return UNI_CursorDown;    // ^N
             case UNI_DLE: return UNI_CursorUp;      // ^P
             case UNI_DC2: return UNI_DC2;           // ^R
             case UNI_EM:  return UNI_EM;            // ^Y

#ifdef cfg_WANT_CTRLD_DEL
             // the user prefers to delete with ^D
             // and to mark the end a file with ^Z.
             case UNI_SUB: return UNI_SUB;           // ^Z
#endif

             default: goto again;
           }
      }
   else if (b0 == UNI_DELETE)   return UNI_BS;

   return Unicode(b0);
}
//────────────────────────────────────────────────────────────────────────────
#if MINGW_SRC
// ── Windows keyboard input via ReadConsoleInputW ──────────────────────────────
// Reads raw KEY_EVENT records so RIGHT_ALT_PRESSED is captured at keypress
// time (not polled asynchronously).  Generates the VT escape sequences that
// the ESCmap expects for navigation keys (Up/Down/Left/Right/Home/End/Ins/Del).
// Used only when stdin is an interactive console; piped/file input falls back
// to ordinary fgetc() so APL scripts work unchanged.

static void
kpush(unsigned char c)
{
   const int nx = (g_ktail + 1) & (int(sizeof g_kbuf) - 1);
   if (nx != g_khead)   { g_kbuf[g_ktail] = c; g_ktail = nx; }
}
static bool  kempty() { return g_khead == g_ktail; }
static int   kpop()
{
   if (kempty()) return EOF;
   unsigned char c = g_kbuf[g_khead];
   g_khead = (g_khead + 1) & (int(sizeof g_kbuf) - 1);
   return c;
}
static void
kpush_utf8(unsigned int u)
{
   if      (u <    0x80) { kpush(u); }
   else if (u <   0x800) { kpush(0xC0|(u>>6));  kpush(0x80|(u&0x3F)); }
   else if (u < 0x10000) { kpush(0xE0|(u>>12)); kpush(0x80|((u>>6)&0x3F)); kpush(0x80|(u&0x3F)); }
}
static void
kpush_str(const char * s) { for (; *s; ++s) kpush((unsigned char)*s); }

// Return APL Unicode for VK+shift via profile-1 map; 0 = no mapping.
static unsigned int
win_apl_char(WORD vk, bool sh)
{
   // Letter keys (VK_A–VK_Z == 'A'–'Z')
   if (vk >= 'A' && vk <= 'Z')
      {
        const int c = sh ? vk : vk + 32;
# define KM(u,a,s,b) if (c==(u)) return (unsigned int)(a); if (c==(s)) return (unsigned int)(b);
        KM('q',U'?','Q',U'?')   KM('w',U'⍵','W',U'⍹')   KM('e',U'∊','E',U'⍷')
        KM('r',U'⍴','R',U'⍴')   KM('t','~','T',U'⍨')    KM('y',U'↑','Y',U'¥')
        KM('u',U'↓','U',U'↓')   KM('i',U'⍳','I',U'⍸')   KM('o',U'○','O',U'⍥')
        KM('p',U'⋆','P',U'⍣')   KM('a',U'⍺','A',U'⍶')   KM('s',U'⌈','S',U'⌈')
        KM('d',U'⌊','D',U'⌊')   KM('f',U'_','F',U'∇')   KM('g',U'∇','G',U'∇')
        KM('h',U'∆','H',U'⍙')   KM('j',U'∘','J',U'⍤')   KM('k',U'λ','K',U'λ')
        KM('l',U'⎕','L',U'⌷')   KM('z',U'⊂','Z',U'⊂')   KM('x',U'⊃','X',U'χ')
        KM('c',U'∩','C',U'¢')   KM('v',U'∪','V',U'∪')   KM('b',U'⊥','B',U'⊥')
        KM('n',U'⊤','N',U'⊤')   KM('m','|','M',U'μ')
# undef KM
        return 0;
      }
   // Digit and OEM keys: derive base char then look up
   char c = 0;
   switch (vk)
      {
        case '1': c=sh?'!':'1'; break;  case '2': c=sh?'@':'2'; break;
        case '3': c=sh?'#':'3'; break;  case '4': c=sh?'$':'4'; break;
        case '5': c=sh?'%':'5'; break;  case '6': c=sh?'^':'6'; break;
        case '7': c=sh?'&':'7'; break;  case '8': c=sh?'*':'8'; break;
        case '9': c=sh?'(':'9'; break;  case '0': c=sh?')':'0'; break;
        case VK_OEM_MINUS:  c=sh?'_':'-';  break;
        case VK_OEM_PLUS:   c=sh?'+':'=';  break;
        case VK_OEM_4:      c=sh?'{':'[';  break;
        case VK_OEM_6:      c=sh?'}':']';  break;
        case VK_OEM_5:      c=sh?'|':'\\'; break;
        case VK_OEM_1:      c=sh?':':';';  break;
        case VK_OEM_7:      c=sh?'"':'\''; break;
        case VK_OEM_COMMA:  c=sh?'<':',';  break;
        case VK_OEM_PERIOD: c=sh?'>':'.';  break;
        case VK_OEM_2:      c=sh?'?':'/';  break;
        default: return 0;
      }
# define KM(u,a,s,b) if (c==(u)) return (unsigned int)(a); if (c==(s)) return (unsigned int)(b);
   KM('1',U'¨','!',U'⌶') KM('2',U'¯','@',U'⍫') KM('3',U'<','#',U'⍒')
   KM('4',U'≤','$',U'⍋') KM('5',U'=','%',U'⌽') KM('6',U'≥','^',U'⍉')
   KM('7',U'>','&',U'⊖') KM('8',U'≠','*',U'⍟') KM('9',U'∨','(',U'⍱')
   KM('0',U'∧',')',U'⍲') KM('-',U'×','_',U'!')  KM('=',U'÷','+',U'⌹')
   KM('[',U'←','{',U'⍞') KM(']',U'→','}',U'⍬') KM('\\',U'⊢','|',U'⊣')
   KM(';',U'⍎',':',U'≡') KM('\'',U'⍕','"',U'≢')
   KM(',',U'⍝','<',U'⍪') KM('.',U'⍀','>',U'⍙') KM('/',U'⌿','?',U'⍠')
# undef KM
   return 0;
}

// Fill g_kbuf from the next usable keyboard event.
static void
win_refill()
{
   const HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
   for (;;)
      {
        INPUT_RECORD ir;   DWORD nr = 0;
        if (!ReadConsoleInputW(hin, &ir, 1, &nr) || nr == 0)
           { g_keof = true;   return; }
        if (ir.EventType != KEY_EVENT)    continue;
        const KEY_EVENT_RECORD & ke = ir.Event.KeyEvent;
        if (!ke.bKeyDown)                 continue;

        const WORD    vk   = ke.wVirtualKeyCode;
        const DWORD   ctrl = ke.dwControlKeyState;
        const bool    sh   = (ctrl & SHIFT_PRESSED)     != 0;
        const bool    ra   = (ctrl & RIGHT_ALT_PRESSED) != 0;
        const wchar_t wch  = ke.uChar.UnicodeChar;

        // Skip lone modifier keys
        switch (vk)
           {
             case VK_SHIFT: case VK_LSHIFT:   case VK_RSHIFT:
             case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
             case VK_MENU:  case VK_LMENU:   case VK_RMENU:
             case VK_CAPITAL: case VK_NUMLOCK: case VK_SCROLL:
                continue;
           }

        // Right Alt (AltGr) → APL glyph
        if (ra)
           {
             const unsigned int apl = win_apl_char(vk, sh);
             if (apl)   { kpush_utf8(apl);   return; }
             // no APL mapping: emit the normal character below
           }

        // Navigation keys → VT sequences (matches existing ESCmap entries)
        switch (vk)
           {
             case VK_UP:     kpush_str("\x1B[A");   return;
             case VK_DOWN:   kpush_str("\x1B[B");   return;
             case VK_RIGHT:  kpush_str("\x1B[C");   return;
             case VK_LEFT:   kpush_str("\x1B[D");   return;
             case VK_END:    kpush_str("\x1B[F");   return;
             case VK_HOME:   kpush_str("\x1B[H");   return;
             case VK_INSERT: kpush_str("\x1B[2~");  return;
             case VK_DELETE: kpush_str("\x1B[3~");  return;
           }

        // Regular character (includes Ctrl+key as control codes)
        if (wch)   { kpush_utf8((unsigned int)wch);   return; }
        // No character (e.g. unbound Alt+key) → skip this event
      }
}
#endif  // MINGW_SRC
//────────────────────────────────────────────────────────────────────────────
int
LineInput::safe_fgetc()
{
   for (;;)
       {
#if MINGW_SRC
          // Interactive console: use ReadConsoleInputW so RIGHT_ALT_PRESSED is
          // captured at keypress time.  Pipe/file input falls back to fgetc().
          int ret_raw;
          if (g_is_console)
             {
               if (g_keof)          return EOF;
               if (kempty())        win_refill();
               if (g_keof)          return EOF;
               ret_raw = kpop();
             }
          else
             {
               errno = 0;
               ret_raw = fgetc(stdin);
             }
          const int ret = (ret_raw == UNI_CR) ? UNI_LF : ret_raw;
#else
          errno = 0;
          const int ret_raw = fgetc(stdin);
          if (errno == EINTR)   continue;
          const int ret = ret_raw;
#endif

#if cfg_ALT_MAP_WANTED

          // maybe change the current ASCII to APL mapping
          //
          enum { PROFILE = cfg_ALT_MAP_WANTED };
          if (PROFILE == 1)   switch(ret)
             {
               default:                             break;
#if MINGW_SRC
               // ^A is "select all" in the Windows 10/11 console; use ^O instead.
               case UNI_SI:  map_next = true;                   // ^O
#else
               case UNI_SOH: map_next = true;                   // ^A
#endif
                             map_all  = false;      continue;
               case UNI_SO:  map_all = ! map_all;   continue;   // ^N
               case UNI_LF:  map_all  = false;      break;      // LF ends map_all
             }
#endif

          if (ret != EOF)   return ret;

#if ! MINGW_SRC
          if (got_WINCH)
             {
               got_WINCH = false;
               continue;
             }
#endif
          return EOF;
       }
}
//════════════════════════════════════════════════════════════════════════════
bool
ESCmap::has_prefix(const char * seq, int seq_len) const
{
   if (seq_len >= len)   return false;
   loop(s, seq_len)
      {
        if ((seq[s] != seqence[s]) && seqence[s])   return false;
      }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
ESCmap::is_equal(const char * seq, int seq_len) const
{
   if (len != seq_len)   return false;
   loop(s, seq_len)
      {
        if ((seq[s] != seqence[s]) && seqence[s])   return false;
      }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
ESCmap::need_more(const char * seq, int len)
{
   loop(e, ESCmap_entry_count)
       {
         if (ESCmap::the_ESCmap[e].has_prefix(seq, len))   return true;
       }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
ESCmap::refresh_lengths()
{
return;
   loop(e, ESCmap_entry_count)
     the_ESCmap[e].len = strlen(the_ESCmap[e].seqence);
}
//════════════════════════════════════════════════════════════════════════════
