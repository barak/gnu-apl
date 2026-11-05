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

bool Output::stdout_is_tty = false;

int Output::color_CIN_foreground  = 0;
int Output::color_CIN_background  = 7;
int Output::color_COUT_foreground = 0;
int Output::color_COUT_background = 8;
int Output::color_CERR_foreground = 5;
int Output::color_CERR_background = 8;
int Output::color_UERR_foreground = 5;
int Output::color_UERR_background = 8;

int Output::output_column = 0;

/// a streambuf for CERR
ErrOut_streambuf CERR_streambuf;

DiffOut DOUT_streambuf(false);
DiffOut UERR_streambuf(true);

// Android is supposed to define its own CIN, COUT, CERR, and UERR ostreams
#ifndef apl_TARGET_ANDROID

CinOut_streambuf CIN_streambuf;
CIN_ostream CIN;

ostream COUT(&DOUT_streambuf);
ostream CERR(CERR_streambuf.use());
ostream UERR(&UERR_streambuf);

#endif

/** get_CERR is an ostream that can safely be used BEFORE and AFTER
    CERR was constructed. IOW: BEFORE and AFTER main() was reached.

    The use of CERR directly will segfault if used before main() is
    reached (e.g. in the constructors of static class instances).

    SUCH CONSTRUCTORS MUST USE get_CERR() INSTEAD OF CERR.

    This is independent of whether CERR is routed to cout of ro cerr.
 **/
extern ostream & get_CERR();
ostream & get_CERR()
{
   if (UserPreferences::uprefs.output_to_cout)
      return ErrOut_streambuf::used ? CERR : cout;
   else
      return ErrOut_streambuf::used ? CERR : cerr;
};
//────────────────────────────────────────────────────────────────────────────
/// write \b s directly to the real C-runtime FILE* \b out, bypassing
/// std::cout/std::cerr entirely, for consistency with CinOut_streambuf::
/// overflow()/ErrOut_streambuf::overflow() below -- kept as plain fputc()
/// even though plain std::cerr/std::cout were never actually broken
/// (Paul Rockwell, macOS 27, 2026-09-25/26: his own standalone
/// "std::cerr << "hi"" test never crashed; the real bug, root-caused
/// later, was ErrOut_streambuf's own inherited-but-unopened FILE*, back
/// when it still derived from filebuf -- see the class comment in
/// StreamBuffers.hh). Output::set_color_mode()/reset_colors() write ANSI
/// color escapes via plain "cerr <<"/"cout <<" and were NOT covered by
/// the SVN 2126 fix (which only touched the two overflow() functions'
/// own per-character writes); converted here purely for consistency, not
/// because they were ever part of the actual crash.
static void
write_direct(FILE * out, const char * s)
{
   while (*s)   fputc(*s++, out);
   if (out == stdout)   fflush(out);   // stderr is unconditionally unbuffered
}

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
CinOut_streambuf::int_type
CinOut_streambuf::overflow(CinOut_streambuf::int_type c)
{
   // overflow(EOF) is how ostream::flush()/sync() ask "flush now, nothing
   // new to insert" -- must never be treated as a real character (see
   // ErrOut_streambuf::overflow() below for the historical reason this
   // matters beyond just correctness: it also determines whether
   // streambuf::xsputn()'s bulk-write fast path proceeds to fwrite()).
   if (traits_type::eq_int_type(c, traits_type::eof()))
      return traits_type::not_eof(c);

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

   set_column(c);   // update the output column

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
   // Write directly to the real, C-runtime stderr rather than through
   // std::cerr: avoids a redundant second pass through std::cerr's own
   // streambuf/locale/sentry machinery for every single character (we
   // already are a streambuf). Deliberately unbuffered (fflush() would be
   // needed after every character if we went through a buffered layer):
   // GNU APL's line editor does a lot of mid-line cursor repositioning and
   // character echo that never ends in '\n', so deferring visibility to
   // the next newline (as a tty/non-tty distinction here once did) made
   // interactive line editing feel stale/laggy. The per-character syscall
   // cost is imperceptible at human typing speed -- the "100x slower"
   // concern this whole class of buffering exists for (Bugs6 #6, see
   // DiffOut.cc) is about *bulk* non-interactive output (large arrays),
   // not per-keystroke echo. No fflush() here: stderr is unconditionally
   // unbuffered on both glibc and BSD/macOS libc (confirmed live via
   // strace: a bare fputc(c, stderr), with no flush at all, still issues
   // its own write() syscall immediately) -- unlike stdout, which is only
   // conditionally unbuffered (line-buffered on a tty, fully buffered
   // otherwise), stderr's unbuffered-ness is unconditional by design
   // (POSIX), specifically so error output is never held back regardless
   // of where it's redirected to.
   //
const char ch = traits_type::to_char_type(c);
   Output::set_column(ch);   // update the output column
   fputc(ch, stderr);
PERFORMANCE_END(fs_CERR_B, cerr_perf, 1)
   return traits_type::not_eof(c);
#endif
}
//════════════════════════════════════════════════════════════════════════════
ErrOut_streambuf::int_type
ErrOut_streambuf::overflow(ErrOut_streambuf::int_type c)
{
   // overflow(EOF) is how ostream::flush()/sync() ask "flush now, nothing
   // new to insert" -- must return here rather than fall through: this is
   // also the exact call that used to reach basic_filebuf::xsputn()'s own
   // bulk-write fast path when this class still derived from filebuf (see
   // the class comment in StreamBuffers.hh) -- returning early, before doing
   // anything else, is what actually matters, not merely what we'd have
   // written for c==EOF.
   if (traits_type::eq_int_type(c, traits_type::eof()))
      return traits_type::not_eof(c);

PERFORMANCE_START(cerr_perf)

   Output::set_color_mode(Output::COLM_ERROR);

#if MINGW_SRC
   // Same as CinOut_streambuf::overflow() — see that function for explanation.
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

   set_column(c);   // update the output column

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
   // see CinOut_streambuf::overflow() above for why fputc() on the real
   // FILE* rather than cout/cerr. Unlike that function, this one can
   // target stdout (output_to_cout) as well as stderr -- stdout, unlike
   // stderr, is only conditionally unbuffered (line-buffered on a tty,
   // fully buffered otherwise), so it alone still needs an explicit flush.
const bool to_cout = UserPreferences::uprefs.output_to_cout;
FILE * const out = to_cout ? stdout : stderr;
const char ch = traits_type::to_char_type(c);
   Output::set_column(ch);
   fputc(ch, out);
   if (to_cout)   fflush(out);

PERFORMANCE_END(fs_CERR_B, cerr_perf, 1)

   return traits_type::not_eof(c);
#endif
}
//════════════════════════════════════════════════════════════════════════════
void
Output::init(bool logit)
{
   stdout_is_tty = isatty(fileno(stdout));

   if (!stdout_is_tty)
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
        // in CinOut_streambuf::overflow() and ErrOut_streambuf::overflow() to
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
        write_direct(stdout, color_RESET);   write_direct(stdout, clear_EOL);
        write_direct(stderr, color_RESET);   write_direct(stderr, clear_EOL);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Output::reset_dout()
{
   DOUT_streambuf.reset();
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
        case COLM_INPUT:  write_direct(stderr, color_CIN);
                          write_direct(stderr, clear_EOL);   break;
        case COLM_OUTPUT: write_direct(stdout, color_COUT);
                          write_direct(stdout, clear_EOL);   break;
        case COLM_ERROR:  write_direct(stderr, color_CERR);
                          write_direct(stderr, clear_EOL);   break;
        case COLM_UERROR: write_direct(stdout, color_UERR);
                          write_direct(stdout, clear_EOL);   break;
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
