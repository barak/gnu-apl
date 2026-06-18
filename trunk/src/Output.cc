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

#include "config.h"
#if HAVE_WINDOWS_H
#include <windows.h>
#endif // HAVE_WINDOWS_H

#include "Common.hh"
#include "Command.hh"
#include "InputFile.hh"
#include "LineInput.hh"
#include "Output.hh"
#include "Performance.hh"
#include "PrintOperator.hh"
#include "Svar_DB.hh"
#include "UserPreferences.hh"   // for preferences and command line options

#if MINGW_SRC
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
# define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static bool
is_wine()
{
static int cached = -1;
   if (cached < 0)
      {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        cached = (ntdll && GetProcAddress(ntdll, "wine_get_version")) ? 1 : 0;
      }
   return cached != 0;
}

#endif   // MINGW_SRC

bool Output::colors_enabled = false;
bool Output::colors_changed = false;

int Output::color_CIN_foreground  = 0;
int Output::color_CIN_background  = 7;
int Output::color_COUT_foreground = 0;
int Output::color_COUT_background = 8;
int Output::color_CERR_foreground = 5;
int Output::color_CERR_background = 8;
int Output::color_UERR_foreground = 5;
int Output::color_UERR_background = 8;

int Output::output_column = 0;

/// a filebuf for CERR
ErrOut_filebuf CERR_filebuf;

DiffOut DOUT_filebuf(false);
DiffOut UERR_filebuf(true);

// Android is supposed to define its own CIN, COUT, CERR, and UERR ostreams
#ifndef apl_TARGET_ANDROID

CinOut_filebuf CIN_filebuf;
CIN_ostream CIN;

ostream COUT(&DOUT_filebuf);
ostream CERR(CERR_filebuf.use());
ostream UERR(&UERR_filebuf);

#endif

extern ostream & get_CERR();
ostream & get_CERR()
{
   if (UserPreferences::uprefs.output_to_cout)
      return ErrOut_filebuf::used ? CERR : cout;
   else
      return ErrOut_filebuf::used ? CERR : cerr;
};

Output::ColorMode Output::color_mode = COLM_UNDEF;

/// CSI sequence for ANSI/VT100 terminals (ESC [)
#define CSI "\x1B["

// Fixed (non-"terminal default") colors, identical on every platform, so
// that the look of GNU APL no longer depends on the terminal's own theme
// (ANSI codes 39/49 mean "terminal default fg/bg" and silently follow
// whatever theme the terminal currently has - that caused GNU APL's
// appearance to drift on its own when a terminal/desktop theme changed,
// even though nothing in GNU APL or its preferences had changed).
//
/// VT100 escape sequence to change to the CIN color (blue on white)
char Output::color_CIN[MAX_ESC_LEN] = CSI "0;34;47m";

/// VT100 escape sequence to change to the COUT color (black on bright white)
char Output::color_COUT[MAX_ESC_LEN] = CSI "0;30;107m";

/// VT100 escape sequence to change to the CERR color (bright red on bright white)
char Output::color_CERR[MAX_ESC_LEN] = CSI "0;91;107m";

/// VT100 escape sequence to change to the UERR color (same as CERR)
char Output::color_UERR[MAX_ESC_LEN] = CSI "0;91;107m";

/// VT100 escape sequence to reset colors to their default
char Output::color_RESET[MAX_ESC_LEN] = CSI "0;39;49m";

/// VT100 escape sequence to clear to end of line
char Output::clear_EOL[MAX_ESC_LEN] = CSI "K";

/// VT100 escape sequence to clear to end of screen
char Output::clear_EOS[MAX_ESC_LEN] = CSI "J";

/// the ESC sequences sent by the cursor keys...
char Output::ESC_CursorUp   [MAX_ESC_LEN]   = CSI "A";    ///< Key ↑
char Output::ESC_CursorDown [MAX_ESC_LEN]   = CSI "B";    ///< Key ↓
char Output::ESC_CursorRight[MAX_ESC_LEN]   = CSI "C";    ///< Key →
char Output::ESC_CursorLeft [MAX_ESC_LEN]   = CSI "D";    ///< Key ←
char Output::ESC_CursorEnd  [MAX_ESC_LEN]   = CSI "F";    ///< Key End
char Output::ESC_CursorHome [MAX_ESC_LEN]   = CSI "H";    ///< Key Home
char Output::ESC_InsertMode [MAX_ESC_LEN]   = CSI "2~";   ///< Key Ins
char Output::ESC_Delete     [MAX_ESC_LEN]   = CSI "3~";   ///< Key Del

/// the ESC sequences sent by the cursor keys with SHIFT and/or CTRL...
/// the "\0" in the middle is a wildcard match for 0x32/0x35/0x36
char Output::ESC_CursorUp_1   [MAX_ESC_LEN] = CSI "1;" "\0" "A";   ///< Key ↑
char Output::ESC_CursorDown_1 [MAX_ESC_LEN] = CSI "1;" "\0" "B";   ///< Key ↓
char Output::ESC_CursorRight_1[MAX_ESC_LEN] = CSI "1;" "\0" "C";   ///< Key →
char Output::ESC_CursorLeft_1 [MAX_ESC_LEN] = CSI "1;" "\0" "D";   ///< Key ←
char Output::ESC_CursorEnd_1  [MAX_ESC_LEN] = CSI "1;" "\0" "F";   ///< Key End
char Output::ESC_CursorHome_1 [MAX_ESC_LEN] = CSI "1;" "\0" "H";   ///< Key Home
char Output::ESC_InsertMode_1 [MAX_ESC_LEN] = CSI "2;" "\0" "~";   ///< Key Ins
char Output::ESC_Delete_1     [MAX_ESC_LEN] = CSI "3;" "\0" "~";   ///< Key Del

//════════════════════════════════════════════════════════════════════════════
int
CinOut_filebuf::overflow(int c)
{
PERFORMANCE_START(cerr_perf)
   if (!InputFile::echo_current_file())   return 0;

   Output::set_color_mode(Output::COLM_INPUT);

#if MINGW_SRC
   // Accumulate bytes until a complete UTF-8 sequence is ready.
   // ASCII bytes go to cerr directly.
   // Multi-byte APL chars use WriteConsoleW(hout, wchar_t): UTF-8 continuation
   // bytes 0x80–0x9F overlap with C1 control codes (e.g. ⍴=U+2374 has byte
   // 0x8D=RI which moves the cursor up).  WriteConsoleW takes a wchar_t and
   // bypasses the VT/C1 scanner entirely.
   // Wine and PTY handles (mintty/MSYS2) fall back to cout.
   {
   static unsigned char utf8buf[4];
   static int           utf8len = 0;
   utf8buf[utf8len++] = (unsigned char)c;

   const unsigned char b0 = utf8buf[0];
   int needed;
   if      ((b0 & 0x80) == 0x00)   needed = 1;
   else if ((b0 & 0xE0) == 0xC0)   needed = 2;
   else if ((b0 & 0xF0) == 0xE0)   needed = 3;
   else if ((b0 & 0xF8) == 0xF0)   needed = 4;
   else   { utf8len = 0; return 0; }

   if      (c == '\n')            Output::output_column = 0;
   else if ((c & 0xC0) != 0x80)   ++Output::output_column;

   if (utf8len < needed)   return 0;

   if (needed == 1)
      {
        cerr << char(c);
      }
   else
      {
        wchar_t wchar = 0;
        if (utf8len == 2)
           wchar = wchar_t(((utf8buf[0] & 0x1F) << 6)
                         |  (utf8buf[1] & 0x3F));
        else if (utf8len == 3)
           wchar = wchar_t(((utf8buf[0] & 0x0F) << 12)
                         | ((utf8buf[1] & 0x3F) <<  6)
                         |  (utf8buf[2] & 0x3F));

        bool wrote = false;
        if (wchar && !is_wine())
           {
             const HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
             DWORD mode;
             if (GetConsoleMode(hout, &mode))
                {
                  DWORD written = 0;
                  wrote = WriteConsoleW(hout, &wchar, 1, &written, NULL)
                          && written == 1;
                }
           }
        if (!wrote)
           {
             cout.write((const char *)utf8buf, utf8len);
             cout.flush();
           }
      }
   utf8len = 0;
   }
PERFORMANCE_END(fs_CERR_B, cerr_perf, 1)
   return 0;
#else
   cerr << char(c);
PERFORMANCE_END(fs_CERR_B, cerr_perf, 1)

   if      (c == '\n')            Output::output_column = 0;
   else if ((c & 0xC0) != 0x80)   ++Output::output_column;
   return 0;
#endif
}
//════════════════════════════════════════════════════════════════════════════
int
ErrOut_filebuf::overflow(int c)
{
PERFORMANCE_START(cerr_perf)

   Output::set_color_mode(Output::COLM_ERROR);

#if MINGW_SRC
   // Same as CinOut_filebuf::overflow() — see that function for explanation.
   {
   static unsigned char utf8buf[4];
   static int           utf8len = 0;
   utf8buf[utf8len++] = (unsigned char)c;

   const unsigned char b0 = utf8buf[0];
   int needed;
   if      ((b0 & 0x80) == 0x00)   needed = 1;
   else if ((b0 & 0xE0) == 0xC0)   needed = 2;
   else if ((b0 & 0xF0) == 0xE0)   needed = 3;
   else if ((b0 & 0xF8) == 0xF0)   needed = 4;
   else   { utf8len = 0; return 0; }

   if      (c == '\n')            Output::output_column = 0;
   else if ((c & 0xC0) != 0x80)   ++Output::output_column;

   if (utf8len < needed)   return 0;

   if (needed == 1)
      {
        cerr << char(c);
      }
   else
      {
        wchar_t wchar = 0;
        if (utf8len == 2)
           wchar = wchar_t(((utf8buf[0] & 0x1F) << 6)
                         |  (utf8buf[1] & 0x3F));
        else if (utf8len == 3)
           wchar = wchar_t(((utf8buf[0] & 0x0F) << 12)
                         | ((utf8buf[1] & 0x3F) <<  6)
                         |  (utf8buf[2] & 0x3F));

        bool wrote = false;
        if (wchar && !is_wine())
           {
             const HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
             DWORD mode;
             if (GetConsoleMode(hout, &mode))
                {
                  DWORD written = 0;
                  wrote = WriteConsoleW(hout, &wchar, 1, &written, NULL)
                          && written == 1;
                }
           }
        if (!wrote)
           {
             cout.write((const char *)utf8buf, utf8len);
             cout.flush();
           }
      }
   utf8len = 0;
   }
PERFORMANCE_END(fs_CERR_B, cerr_perf, 1)
   return 0;
#else
   if (UserPreferences::uprefs.output_to_cout)   cout << char(c);
   else                                          cerr << char(c);

   if      (c == '\n')            Output::output_column = 0;
   else if ((c & 0xC0) != 0x80)   ++Output::output_column;   // unless subsequent UTF

PERFORMANCE_END(fs_CERR_B, cerr_perf, 1)

   return 0;
#endif
}
//════════════════════════════════════════════════════════════════════════════
void
Output::init(bool logit)
{
   if (!isatty(fileno(stdout)))
      {
        cout.flush();
        cout.setf(ios::unitbuf);
      }

   if (logit)
      {
        CERR << "using ANSI terminal output ESC sequences (or those "
                "configured in your preferences file(s))" << endl;


        CERR << "using ANSI terminal input ESC sequences (or those "
                "configured in your preferences file(s))" << endl;
      }

#if MINGW_SRC
   if (is_wine())
      {
        // Wine does not process VT sequences; disable them to avoid
        // literal ESC garbage in the terminal output.
        clear_EOL[0] = 0;
        clear_EOS[0] = 0;
      }
   else
      {
        // Real Windows 10+: enable VT sequence processing for colors and
        // line-clearing sequences.
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
# define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
        DWORD mode;
        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE herr = GetStdHandle(STD_ERROR_HANDLE);
        if (GetConsoleMode(hout, &mode))
           SetConsoleMode(hout, mode | ENABLE_PROCESSED_OUTPUT
                                     | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        if (GetConsoleMode(herr, &mode))
           SetConsoleMode(herr, mode | ENABLE_PROCESSED_OUTPUT
                                     | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        // Switch to UTF-8 so multi-byte APL characters written byte-by-byte
        // via the CRT (cerr/cout) are decoded correctly by the console.
        SetConsoleOutputCP(CP_UTF8);

        // Multi-byte APL characters are echoed via WriteConsoleW (wchar_t)
        // in CinOut_filebuf::overflow() and ErrOut_filebuf::overflow() to
        // avoid C1 control-code interference from UTF-8 continuation bytes
        // (e.g. ⍴=U+2374 has byte 0x8D=RI in its UTF-8 encoding).
      }
#endif // MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
void
Output::reset_colors()
{
   if (colors_changed)
      {
        cout << color_RESET << clear_EOL;
        cerr << color_RESET << clear_EOL;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Output::reset_dout()
{
   DOUT_filebuf.reset();
}
//────────────────────────────────────────────────────────────────────────────
void
Output::set_color_mode(Output::ColorMode mode)
{
   if (!colors_enabled)      return;   // colors disabled
   if (color_mode == mode)   return;   // no change in color mode

   // mode changed
   //
   color_mode = mode;
   colors_changed = true;   // to reset them in reset_colors()

   switch(color_mode)
      {
        case COLM_INPUT:  cerr << color_CIN  << clear_EOL;   break;
        case COLM_OUTPUT: cout << color_COUT << clear_EOL;   break;
        case COLM_ERROR:  cerr << color_CERR << clear_EOL;   break;
        case COLM_UERROR: cout << color_UERR << clear_EOL;   break;
        default: break;
      }
}
//────────────────────────────────────────────────────────────────────────────
void 
Output::toggle_color(const UCS_string & arg)
{
   if (arg.starts_iwith("ON"))         colors_enabled = true;
   else if (arg.starts_iwith("OFF"))   colors_enabled = false;
   else                                colors_enabled = !colors_enabled;
}
//════════════════════════════════════════════════════════════════════════════
void
CIN_ostream::set_cursor(int y, int x)
{
   if (UserPreferences::uprefs.raw_cin)   return;

#if MINGW_SRC
   if (is_wine())   return;
   {
     // On real Windows consoles use SetConsoleCursorPosition (Win32 API).
     // Try STD_ERROR_HANDLE first (overflow() writes to stderr); fall back
     // to STD_OUTPUT_HANDLE so a redirected stderr still works.
     HANDLE hcon = GetStdHandle(STD_ERROR_HANDLE);
     CONSOLE_SCREEN_BUFFER_INFO csbi;
     if (!GetConsoleScreenBufferInfo(hcon, &csbi))
        {
          hcon = GetStdHandle(STD_OUTPUT_HANDLE);
          if (!GetConsoleScreenBufferInfo(hcon, &csbi))
             hcon = NULL;
        }
     if (hcon)
        {
          // y < 0: rows from bottom of viewport (-1 = last visible row).
          // y >= 0: rows from top of viewport.
          const int row = (y < 0)
                          ? csbi.srWindow.Bottom + y + 1
                          : csbi.srWindow.Top    + y;
          COORD pos = { (SHORT)x, (SHORT)row };
          SetConsoleCursorPosition(hcon, pos);
          return;
        }
     // GetConsoleScreenBufferInfo failed on both handles: PTY (mintty/MSYS2).
     // Fall through to ESC sequence path below.
   }
#endif

   if (y < 0)
      {
        // y < 0 means from bottom upwards
        //
        *this << CSI << "30;" << (1 + x) << 'H'
              << CSI << "99B" << std::flush;
        if (y < -1)   *this << CSI << (-(y + 1)) << "A";
      }
   else
      {
        // y ≥ 0 is from top downwards. This is currently not used.
        //
        *this << CSI << (1 + y) << ";" << (1 + x) << 'H' << std::flush;
      }
}
//════════════════════════════════════════════════════════════════════════════
