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

#ifndef __OUTPUT_HH_DEFINED__
#define __OUTPUT_HH_DEFINED__

#include <stdio.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>

#include "Assert.hh"
#include "StreamBuffers.hh"
#include "UCS_string.hh"

using namespace std;

/*
 The classes below are used to combine normal user I/O and automatic
 testcase execution. It works like this:

       (CIN)
         │
         │
         V
     ┌───────┐                              ┌───────────────────┐
     │ Input │  <─────────────┬─────────────┤ testcase files(s) │
     └───┬───┘                │             │        *.tc       │
         │                    │            └───────────────────┘
         │                    │
         V                    │
      ┌─────┐                 │
      │ APL │                 │
      └──┬──┘                 │
         │                    │
         │  -T testcase       │
         ├────────────────┐   │
         │                │   │
         │                V   V
         │             ┌─────────┐
         │             │ compare │
         │             └────┬────┘
         │                  │
         │                  │
         V                  V
       (COUT)           (test results)
                        (  *.tc.log  )
 */

//════════════════════════════════════════════════════════════════════════════
/** the output of the APL interpreter to cout and maybe to a test result file
 **/
/// The class handling all output from the APL interpreter
class Output
{
public:
   /// the color to be usedd for output
   enum ColorMode
      {
         COLM_UNDEF,    ///< color undefined
         COLM_INPUT,    ///< color for (echo of) input characters (CIN)
         COLM_OUTPUT,   ///< color for normal APL output (COUT)
         COLM_ERROR,    ///< color for debug output (CERR)
         COLM_UERROR,   ///< color for APL error output (UERR)
      };

   enum { MAX_ESC_LEN = 100 };   ///< max. length of an ESC sequence

   /// true if xterm/color is ON
   static bool color_enabled()
      { return colors_enabled; }

   /// return the current column (chars since last LF).
   static int get_column()
       { return output_column; }
 
   /// advance or reset the output_column according to (possibly UTF8-encoded)
   //  output byte \b cc.
   static void set_column(char cc)
      {
        if      (cc == '\n')             output_column = 0;
        else if ((cc & 0x80) == 0)       ++output_column;   // ASCII
        else if ((cc & 0xC0) == 0xC0)    ++output_column;   // first UTF
      }

   /// add spaces until \b column is reached
   static void indent(int column)
      // fputc() directly on the real stderr, not "cerr << ' '" -- kept
      // consistent with ErrOut_streambuf::overflow()/Output::
      // set_color_mode() (Output.cc), which write this way for a real
      // reason (see ErrOut_streambuf's class comment in StreamBuffers.hh);
      // plain std::cerr itself was never actually broken (Paul Rockwell,
      // macOS 27, 2026-09-25/26), so this is a consistency choice here,
      // not a fix for anything.
      //
      {
        while (output_column < column)
           {
             fputc(' ', stderr);
             ++output_column;
           }
      }

   /// initialize terminal output (ANSI sequences)
   /// @param logit true to log initialization steps to the startup log
   static void  init(bool logit);

   /// reset colors to black and white
   static void reset_colors();

   /// mark CERR unsafe to use via get_CERR() from here on. Must be called
   /// (via atexit(), see main.cc) before static destruction begins: within
   /// Output.cc, ostream CERR is destroyed *before* ErrOut_streambuf
   /// CERR_streambuf (constructed first, so -- per C++'s reverse-order-of-
   /// construction destruction rule -- destroyed last), which left a real
   /// window where ErrOut_streambuf::used was still true, and therefore
   /// get_CERR() still returned CERR, after CERR itself had already been
   /// destroyed. Any code reachable from another global's destructor during
   /// that window (an Assert() failure, most plausibly) was reading through
   /// an already-destroyed ostream -- undefined behavior, in practice a
   /// crash after )OFF or at any other static-teardown exit path.
   //
   static void mark_CERR_unsafe()
      {
        ErrOut_streambuf::used = false;
      }

   /// reset() dout_filebuf
   static void reset_dout();

   /// set the color mode (if colors_enabled). Outputs the escape sequence
   /// for \b mode when the color mode changes
   /// @param mode the new color mode to apply
   static void set_color_mode(ColorMode mode);

   /// set or toggle color mode (implementation of command ]XTERM)
   /// @param arg UCS string argument from the ]XTERM command
   static void toggle_color(const UCS_string & arg);

   /// escape sequence for CIN colors
   static char color_CIN[MAX_ESC_LEN];

   /// escape sequence for COUT colors
   static char color_COUT[MAX_ESC_LEN];

   /// escape sequence for CERR colors
   static char color_CERR[MAX_ESC_LEN];

   /// escape sequence for UERR colors
   static char color_UERR[MAX_ESC_LEN];

   /// escape sequence for resetting colors
   static char color_RESET[MAX_ESC_LEN];

   /// foreground color for CIN
   static int color_CIN_foreground;

   /// background color for CIN
   static int color_CIN_background;

   /// foreground color for COUT
   static int color_COUT_foreground;

   /// background color for COUT
   static int color_COUT_background;

   /// foreground color for CERR
   static int color_CERR_foreground;

   /// background color for CERR
   static int color_CERR_background;

   /// foreground color for UERR
   static int color_UERR_foreground;

   /// background color for UERR
   static int color_UERR_background;

   /// escape sequence for clear to end of line
   static char clear_EOL[MAX_ESC_LEN];

   /// escape sequence for clear to end of screen
   static char clear_EOS[MAX_ESC_LEN];

   /// default ESC sequence for Cursor Up key
   static char ESC_CursorUp[MAX_ESC_LEN];

   /// default ESC sequence for Cursor Down key
   static char ESC_CursorDown[MAX_ESC_LEN];

   /// default ESC sequence for Cursor Right key
   static char ESC_CursorRight[MAX_ESC_LEN];

   /// default ESC sequence for Cursor Left key
   static char ESC_CursorLeft[MAX_ESC_LEN];

   /// default ESC sequence for End key
   static char ESC_CursorEnd[MAX_ESC_LEN];

   /// default ESC sequence for Home key
   static char ESC_CursorHome[MAX_ESC_LEN];

   /// default ESC sequence for Insert key
   static char ESC_InsertMode[MAX_ESC_LEN];

   /// default ESC sequence for Delete key
   static char ESC_Delete[MAX_ESC_LEN];

   /// default ESC sequence for Cursor Up key with SHIFT and/or CTRL
   static char ESC_CursorUp_1[MAX_ESC_LEN];

   /// default ESC sequence for Cursor Down key with SHIFT and/or CTRL
   static char ESC_CursorDown_1[MAX_ESC_LEN];

   /// default ESC sequence for Cursor Right key with SHIFT and/or CTRL
   static char ESC_CursorRight_1[MAX_ESC_LEN];

   /// default ESC sequence for Cursor Left key with SHIFT and/or CTRL
   static char ESC_CursorLeft_1[MAX_ESC_LEN];

   /// default ESC sequence for End key with SHIFT and/or CTRL
   static char ESC_CursorEnd_1[MAX_ESC_LEN];

   /// default ESC sequence for Home key with SHIFT and/or CTRL
   static char ESC_CursorHome_1[MAX_ESC_LEN];

   /// default ESC sequence for Insert key with SHIFT and/or CTRL
   static char ESC_InsertMode_1[MAX_ESC_LEN];

   /// default ESC sequence for Delete key with SHIFT and/or CTRL
   static char ESC_Delete_1[MAX_ESC_LEN];

   /// true if stdout is a real terminal (computed once, in Output::init())
   static bool stdout_is_tty;

protected:
   /// the current output column
   static int output_column;

   /// true if colors were changed (and then reset_colors() shall reset
   /// them when leaving the interpreter
   static bool colors_changed;

   /// the current color mode
   static ColorMode color_mode;

   /// true if colors are currently enabled (by XTERM command)
   static bool colors_enabled;
};

/// an ostream for stdin echo and a few editing capabilities
class CIN_ostream : public ostream
{
public:
   CIN_ostream()
   : ostream(&CIN_streambuf)
   {}

   /// set cursor to y:x (upper left corner is 0:0, negative y: from bottom)
   /// @param y row position (0 = top; negative counts from bottom)
   /// @param x column position (0 = leftmost column)
   void set_cursor(int y, int x);

   /// clear to end of line
   ostream & clear_EOL()
      { return *this << Output::clear_EOL; }

   /// clear to end of screen supported ?
   bool can_clear_EOS() const
      { return *Output::clear_EOS != 0; }

   /// clear to end of line
   ostream & clear_EOS()
      { return *this << Output::clear_EOS; }
};
//════════════════════════════════════════════════════════════════════════════
extern CIN_ostream CIN;

#endif // __OUTPUT_HH_DEFINED__
