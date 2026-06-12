/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2020  Dr. Jürgen Sauermann

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

/*
   This program tries to determine the ESC sequences that are relevant
   for GNU APL using libcurses.

   On success the ESC sequences are printed on stdout.

   Build like this:

   g++ -o ncurses_emul ncurses_emul.cc -lncurses   -or-
   g++ -o ncurses_emul ncurses_emul.cc -ltinfo
 */

#include "../config.h"   // for HAVE_XXX macros

#if HAVE_LIBTINFO && HAVE_NCURSES_H && HAVE_TERM_H

#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <term.h>
#include <unistd.h>

#include <iostream>
#include <iomanip>

using namespace std;

enum { MAX_ESC_LEN = 100 };


#define CSI "\x1B["

/// VT100 escape sequence to change to cin color
char color_CIN[MAX_ESC_LEN] = CSI "0;30;47m";

/// VT100 escape sequence to change to cout color
char color_COUT[MAX_ESC_LEN] = CSI "0;39;49m";

/// VT100 escape sequence to change to cerr color
char color_CERR[MAX_ESC_LEN] = CSI "0;35;49m";

/// VT100 escape sequence to change to uerr color
char color_UERR[MAX_ESC_LEN] = CSI "0;35;49m";

/// VT100 escape sequence to reset colors to their default
char color_RESET[MAX_ESC_LEN] = CSI "0;39;49m";

/// VT100 escape sequence to clear to end of line
char clear_EOL[MAX_ESC_LEN] = CSI "K";

/// VT100 escape sequence to clear to end of screen
char clear_EOS[MAX_ESC_LEN] = CSI "J";

/// VT100 escape sequence to exit attribute mode
char exit_attr_mode[MAX_ESC_LEN] = CSI "m" "\x1B"  "(B";

/// the ESC sequences sent by the cursor keys...
char ESC_CursorUp   [MAX_ESC_LEN]   = CSI "A";    ///< Key ↑
char ESC_CursorDown [MAX_ESC_LEN]   = CSI "B";    ///< Key ↓
char ESC_CursorRight[MAX_ESC_LEN]   = CSI "C";    ///< Key →
char ESC_CursorLeft [MAX_ESC_LEN]   = CSI "D";    ///< Key ←
char ESC_CursorEnd  [MAX_ESC_LEN]   = CSI "F";    ///< Key End
char ESC_CursorHome [MAX_ESC_LEN]   = CSI "H";    ///< Key Home
char ESC_InsertMode [MAX_ESC_LEN]   = CSI "2~";   ///< Key Ins
char ESC_Delete     [MAX_ESC_LEN]   = CSI "3~";   ///< Key Del

/// the ESC sequences sent by the cursor keys with SHIFT and/or CTRL...
/// the "\0" in the middle is a wildcard match for 0x32/0x35/0x36
char ESC_CursorUp_1   [MAX_ESC_LEN] = CSI "1;" "\0" "A";   ///< Key ↑
char ESC_CursorDown_1 [MAX_ESC_LEN] = CSI "1;" "\0" "B";   ///< Key ↓
char ESC_CursorRight_1[MAX_ESC_LEN] = CSI "1;" "\0" "C";   ///< Key →
char ESC_CursorLeft_1 [MAX_ESC_LEN] = CSI "1;" "\0" "D";   ///< Key ←
char ESC_CursorEnd_1  [MAX_ESC_LEN] = CSI "1;" "\0" "F";   ///< Key End
char ESC_CursorHome_1 [MAX_ESC_LEN] = CSI "1;" "\0" "H";   ///< Key Home
char ESC_InsertMode_1 [MAX_ESC_LEN] = CSI "2;" "\0" "~";   ///< Key Ins
char ESC_Delete_1     [MAX_ESC_LEN] = CSI "3;" "\0" "~";   ///< Key Del

int color_CIN_foreground = 0;
int color_CIN_background = 7;
int color_COUT_foreground = 0;
int color_COUT_background = 8;
int color_CERR_foreground = 5;
int color_CERR_background = 8;
int color_UERR_foreground = 5;
int color_UERR_background = 8;

//----------------------------------------------------------------------------
static int
read_ESC_sequence(char * dest, int destlen, int append,
                  const char * capname, char * str, int p1,
                  const char * prefname)
{
   if (str == 0)
      {
        const char * term = getenv("TERM");
        cerr << "capability '" << capname
             << "' is not contained in the description";
        if (term)   cerr << " of terminal " << term;
        cerr << endl;
        return 1;
      }

   if (str == reinterpret_cast<const char *>(-1))
      {
        const char * term = getenv("TERM");
        cerr << "capability '" << capname
             << "' is not a string capability";
        cerr << endl;
        if (term)   cerr << " of terminal " << term;
        return 1;
      }

// cerr << "BEFORE: ";
// for (int i = 0; i < strlen(dest); ++i)   cerr << " " << HEX2(dest[i]);
// cerr << endl;

   if (!append)   *dest = 0;

const int offset = strlen(dest);
const char * seq = tparm(str, p1, 0, 0, 0, 0, 0, 0, 0, 0);
const int seq_len = strlen(seq);

   if (seq_len + offset >= (destlen - 1))
      {
        cerr << "ESC sequence too long" << endl;
        return 1;
      }

   strncpy(dest + offset, seq, destlen - offset - 1);

//   cerr << "AFTER:  ";
//   for (int i = 0; i < strlen(dest); ++i)   cerr << " " << HEX2(dest[i]);
//   cerr << endl;

   if (append)
      {
        cout << left << hex;
        for (const char * s = seq; *s; ++s)   cout << " " << int(*s);
        cout << right << dec;
      }
   else
      {
        cout << left << setw(24) << prefname << hex;
        for (const char * s = seq; *s; ++s)   cout << " " << int(*s);
        cout << right << dec;
      }

   return 0;
}
//----------------------------------------------------------------------------
int doit()
{
int errors = 0;
   if (const int ret = setupterm(0, STDOUT_FILENO, 0))   ++errors;

#define READ_Seq(dest, app, cap, p1, pref) \
   errors += read_ESC_sequence(dest, MAX_ESC_LEN, app, #cap, cap, p1, #pref);

   READ_Seq(clear_EOL,      0, clr_eol, 0, CLEAR-EOL-SEQUENCE);
   cout << endl;

   READ_Seq(clear_EOS,      0, clr_eos, 0, CLEAR-EOS-SEQUENCE);
   cout << endl;

// READ_Seq(exit_attr_mode, 0, exit_attribute_mode, 0, EXIT-ATTR);
// cout << endl;

   //                  ┌───── append
   //                  │
   READ_Seq(color_CIN, 0, set_foreground, color_CIN_foreground, CIN-SEQUENCE);
   READ_Seq(color_CIN, 1, set_background, color_CIN_background, PREF);
   cout << endl;

   READ_Seq(color_COUT, 0, set_foreground, color_COUT_foreground, COUT-SEQUENCE);
   READ_Seq(color_COUT, 1, set_background, color_COUT_background, PREF);
   cout << endl;

   READ_Seq(color_CERR, 0, set_foreground, color_CERR_foreground, CERR-SEQUENCE);
   READ_Seq(color_CERR, 1, set_background, color_CERR_background, PREF);
   cout << endl;

   READ_Seq(color_UERR, 0, set_foreground, color_UERR_foreground, UERR-SEQUENCE);
   READ_Seq(color_UERR, 1, set_background, color_UERR_background, PREF);
   cout << endl;

   READ_Seq(ESC_CursorUp,    0, key_up,    0, KEY-CURSOR-UP);
   cout << endl;
   READ_Seq(ESC_CursorDown,  0, key_down,  0, KEY-CURSOR-DOWN);
   cout << endl;
   READ_Seq(ESC_CursorLeft,  0, key_left,  0, KEY-CURSOR-LEFT);
   cout << endl;
   READ_Seq(ESC_CursorRight, 0, key_right, 0, KEY-CURSOR-RIGHT);
   cout << endl;
   READ_Seq(ESC_CursorEnd,   0, key_end,   0, KEY-CURSOR-END);
   cout << endl;
   READ_Seq(ESC_CursorHome,  0, key_home,  0, KEY-CURSOR-HOME);
   cout << endl;
   READ_Seq(ESC_InsertMode,  0, key_ic,    0, KEY-INSMODE);
   cout << endl;
   READ_Seq(ESC_Delete,      0, key_dc,    0, KEY-DELETE);
   cout << endl;


  return errors;
}

#else // NOT HAVE_LIBTINFO && HAVE_NCURSES_H && HAVE_TERM_H

#include <stdio.h>
int
doit()
{
   return fprintf(stderr, "*** LIBCURSES etc. missing\n");
}
#endif
//----------------------------------------------------------------------------
int
main(int, char *[])
{
   return doit();
}
//----------------------------------------------------------------------------

