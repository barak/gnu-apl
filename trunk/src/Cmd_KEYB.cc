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

#include "Common.hh"
#include "Command.hh"
#include "Sys.hh"
#include "UserPreferences.hh"
#include "Workspace.hh"

#if HAVE_LIBX11
# include <X11/Xlib.h>
#endif

#if HAVE_X11_XKBLIB_H
# include <X11/XKBlib.h>
#endif

bool Cmd_KEYB::keymap_from_xkbd = false;

// See: /usr/share/X11/xkb/keycodes/xfree86

const char * FUNKEY_template[] =
{
"┌─────┐   ┌────┬────┬────┬────┐  ┌────┬────┬────┬────┐  ┌────┬────┬────┬────┐   ┌────┬────┬────┐",
"│ ESC │   │FK01│FK02│FK03│FK04│  │FK05│FK06│FK07│FK08│  │FK09│FK10│FK11│FK12│   │PRSC│SCLK│NMLK│",
"│  8  │   │ 67 │ 68 │ 69 │ 70 │  │ 71 │ 72 │ 73 │ 74 │  │ 75 │ 76 │ 95 │ 96 │   │107 │ 78 │ 77 │",
"└─────┘   └────┴────┴────┴────┘  └────┴────┴────┴────┘  └────┴────┴────┴────┘   └────┴────┴────┘",
"",
};

const char * CURSOR_template[] =
{
"┌────┬────┬────┐",
"│INS │HOME│PGUP│",
"│118 │110 │112 │",
"├────┼────┼────┤",
"│DELE│END │PGDN│",
"│119 │115 │117 │",
"└────┴────┴────┘",
"                ",
"                ",
"     ┌────┐     ",
"     │ UP │     ",
"     │111 │     ",
"┌────┼────┼────┐",
"│LEFT│DOWN│RGHT│",
"│113 │116 │114 │",
"└────┴────┴────┘",
};

const char * KEYPAD_template[] =
{
"┌────┬────┬────┬────┐",
"│NMLK│    │    │KPSU│",
"│ 77 │    │    │ 82 │",
"├────┼────┼────┼────┤",
"│KP7 │KP8 │KP9 │    │",
"│ 79 │ 80 │ 81 │KPAD│",
"├────┼────┼────┼ 86 │",
"│KP4 │KP5 │KP6 │    │",
"│ 83 │ 84 │ 85 │    │",
"├────┼────┼────┼────┤",
"│KP1 │KP2 │KP3 │    │",
"│ 87 │ 88 │ 89 │KPEN│",
"├────┼────┼────┤104 │",
"│   KP0   │KPDL│    │",
"│   90    │ 91 │    │",
"└─────────┴────┴────┘",
};

const char * MAIN_template[] =
{
"┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┳━━━━━━━━━┓",
"│TLDE│AE01│AE02│AE03│AE04│AE05│AE06│AE07│AE08│AE09│AE10│AE11│AE12┃  BKSP   ┃",
"│ 49 │ 10 │ 11 │ 12 │ 13 │ 14 │ 15 │ 16 │ 17 │ 18 │ 19 │ 20 │ 21 ┃   22    ┃",
"┢━━━━┷━━┱─┴──┬─┴──┬─┴──┬─┴──┬─┴──┬─┴──┬─┴──┬─┴──┬─┴──┬─┴──┬─┴──┬─┺━━┯━━━━━━┩",
"┃  TAB  ┃AD01│AD02│AD03│AD04│AD05│AD06│AD07│AD08│AD09│AD00│AD10│AD11│ BKSL │",
"┃  23   ┃ 24 │ 25 │ 26 │ 27 │ 28 │ 29 │ 30 │ 31 │ 32 │ 33 │ 34 │ 35 │  51  │",
"┣━━━━━━━┻━┱──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┲━━┷━━━━━━┪",
"┃  CAPS   ┃AC01│AC02│AC03│AC04│AC05│AC06│AC07│AC08│AC09│AC10│AC11┃  RTRN   ┃",
"┃   66    ┃ 38 │ 39 │ 40 │ 41 │ 42 │ 43 │ 44 │ 45 │ 46 │ 47 │ 48 ┃   36    ┃",
"┣━━━━━━━━━┻━━━┱┴───┬┴───┬┴───┬┴───┬┴───┬┴───┬┴───┬┴───┬┴───┬┴───┲┻━━━━━━━━━┫",
"┃    LFSH     ┃AB01│AB02│AB03│AB04│AB05│AB06│AB07│AB08│AB09│AB10┃   RTSH   ┃",
"┃     50      ┃ 52 │ 53 │ 54 │ 55 │ 56 │ 57 │ 58 │ 59 │ 60 │ 61 ┃    62    ┃",
"┣━━━━┳━━━━━┳━━┻━━┱─┴────┴────┴────┴────┴────┴────┴──┲━┷━━━┳┷━━━━╋━━━━━┳━━━━┫",
"┃LCTL┃ LWIN┃     ┃               SPCE               ┃     ┃ RWIN┃COMP ┃RCTL┃",
"┃ 37 ┃ 133 ┃ ALT ┃                65                ┃ ALT ┃ 134 ┃ 135 ┃105 ┃",
"┗━━━━┻━━━━━┻━━━━━┹──────────────────────────────────┺━━━━━┻━━━━━┻━━━━━┻━━━━┛",
                        };

enum {
   FUNKEY_rows = sizeof(FUNKEY_template) / sizeof(char *),
   MAIN_rows   = sizeof(MAIN_template)   / sizeof(char *),
   CURSOR_rows = sizeof(CURSOR_template) / sizeof(char *),
   KEYPAD_rows = sizeof(KEYPAD_template) / sizeof(char *),
     };

static_assert(MAIN_rows == KEYPAD_rows);
static_assert(MAIN_rows == CURSOR_rows);

Cmd_KEYB::map_item Cmd_KEYB::key_map[];

//----------------------------------------------------------------------------
void
Cmd_KEYB::cmd_KEYB(ostream & out, const UCS_string_vector & args)
{
   // clear the key_map from the previous invocation
   //
  loop(k, 256)   key_map[k].keycode = 0;

   enum
      {
        MO_NONE     =  0,
        MO_KEYS     =  1,
        MO_XMODMAP  =  2,
        MO_XKBD     =  4,
        MO_USERFILE =  8,
        MO_BUILTIN  = 16,
        MO_ALL      = MO_XMODMAP | MO_USERFILE | MO_BUILTIN
      };

int mode = MO_NONE;
int area = KB_AREA_MAIN;   // main keys
   loop(a, args.size())
      {
        if      (args[a].starts_iwith("XMOD"))   mode |= MO_XMODMAP;
        else if (args[a].starts_iwith("XKBD"))   mode |= MO_XKBD;
        else if (args[a].starts_iwith("USER"))   mode |= MO_USERFILE;
        else if (args[a].starts_iwith("GUESS"))  mode |= MO_BUILTIN;
        else if (args[a].starts_iwith("KEYS"))   mode |= MO_KEYS;
        else if (args[a].starts_iwith("KPAD"))   area |= KB_AREA_KEYPAD;
        else if (args[a].starts_iwith("FUNK"))   area |= (KB_AREA_FUNKEY |
                                                          KB_AREA_CURSOR);
        else if (args[a].starts_iwith("CURS"))   area |= KB_AREA_CURSOR;
        else
           {
             CERR << "Bad Command+" << endl;
             MORE_ERROR() << "Command ]KEYB: invalid argument '"
                          << args[a] << "'";
             if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
             return;
           }
      }

   if ((mode & MO_XMODMAP) && (mode & MO_XKBD))
      {
        CERR << "Bad Command+" << endl;
        MORE_ERROR() << "]KEYB: Invalid combination of XMOD and XKBD";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return;
      }

const bool no_mode = mode == MO_NONE;
   if (no_mode)   // if no specific mode(s) given
      {
        mode = MO_XMODMAP | MO_USERFILE | MO_BUILTIN;
      }

   // if neither xmodmap nor xkdb is specified then check user preferences
   //
   if (!(mode & (MO_XMODMAP | MO_XKBD)))
      {
        if (UserPreferences::uprefs.no_xmodmap)   mode |= MO_XKBD;
        else                                      mode |= MO_XMODMAP;
      }

   if (mode & MO_KEYS)
      {
        print_keycodes(out, KB_Area(area));
      }

   if (mode & MO_XKBD)
      {
        const bool got_map = ! read_xkbd_map();
        if (got_map)   print_keymap(out, KB_Area(area));
        return;
      }

const bool do_xmodmap = (mode & MO_XMODMAP) &&
                         ! (no_mode && UserPreferences::uprefs.no_xmodmap);
   if (do_xmodmap)
      {
        const bool xmodmap_error = parse_xmodmap();

        if (mode != MO_ALL)
           {
             // in mode MO_ALL failure of xmodmap is kind of expected. However
             // mode was not MO_ALL and then we should complain if xmodmap
             // has failed. Normally parse_xmodmap() sets up the )MORE info,
             // but if it didn;t then we do it here,
             //
             if (xmodmap_error)
                {
                  if (Workspace::more_error().size() == 0)
                     {
                       MORE_ERROR() << "running xmodmap failed.";
                       if (Command::auto_MORE)
                       CERR << Workspace::more_error() << endl;
                     }
                }
             else
                {
                  Workspace::more_error().clear();
                  print_keymap(out, KB_Area(area));
                }
             return;
           }

        if (!xmodmap_error)   // xmodmap succeeded
           {
             print_keymap(out, KB_Area(area));
             return;
           }
      }

   // parse_xmodmap() has provided )MORE infos, but it is no longer of
   // interest at this point.
   //
   Workspace::more_error().clear();

   if (mode & MO_USERFILE)
      {
        const UTF8_string filename =
              UserPreferences::uprefs.keyboard_layout_file;
        if (filename.size())
           {
             if (FILE * layout = fopen(filename.c_str(), "r"))
                {
                  out << "User-defined Keyboard Layout.    Source: "
                      << filename << "\n";
                  for (;;)
                      {
                         const int cc = fgetc(layout);
                         if (cc == EOF)   break;
                         out << char(cc);
                      }
                  out << endl;
                  return;
                }

             out << "Could not open " << filename
                 << ": " << strerror(errno) << endl;
           }
        else
           {
             out << "]KEYB USER: no file name of a user-defined layoutfile "
                             "specified in preferences." << endl;
           }
        if (mode != MO_ALL)   return;
      }
   Workspace::more_error().clear();

   // no user-defined layout file either, show built-in layout
   //
   out << "US Keyboard Layout.     Source: GNU APL builtin."
                             "\n";

UTF8_string_vector utf(
"╔════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦═════════╗\n"
"║ ~  ║ !⌶ ║ @⍫ ║ #⍒ ║ $⍋ ║ %⌽ ║ ^⍉ ║ &⊖ ║ *⍟ ║ (⍱ ║ )⍲ ║ _! ║ +⌹ ║         ║\n"
"║ `◊ ║ 1¨ ║ 2¯ ║ 3< ║ 4≤ ║ 5= ║ 6≥ ║ 7> ║ 8≠ ║ 9∨ ║ 0∧ ║ -× ║ =÷ ║ BACKSP  ║\n"
"╠════╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦══════╣\n"
"║       ║ Q  ║ W⍹ ║ E⍷ ║ R  ║ T⍨ ║ Y¥ ║ U  ║ I⍸ ║ O⍥ ║ P⍣ ║ {⍞ ║ }⍬ ║  |⊣  ║\n"
"║  TAB  ║ q? ║ w⍵ ║ eϵ ║ r⍴ ║ t∼ ║ y↑ ║ u↓ ║ i⍳ ║ o○ ║ p⋆ ║ [← ║ ]→ ║  \\⊢  ║\n"
"╠═══════╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩══════╣\n"
"║ (CAPS   ║ A⍶ ║ S« ║ D» ║ F  ║ G  ║ H  ║ J⍤ ║ K  ║ L⌷ ║ :≡ ║ \"≢ ║         ║\n"
"║  LOCK)  ║ a⍺ ║ s⌈ ║ d⌊ ║ f_ ║ g∇ ║ h∆ ║ j∘ ║ kλ ║ l⎕ ║ ;⍎ ║ '⍕ ║ RETURN  ║\n"
"╠═════════╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═════════╣\n"
"║             ║ Z  ║ Xχ ║ C¢ ║ V  ║ B£ ║ N  ║ Mμ ║ <⍪ ║ >⍙ ║ ?  ║          ║\n"
"║  SHIFT      ║ z⊂ ║ x⊃ ║ c∩ ║ v∪ ║ b⊥ ║ n⊤ ║ m| ║ ,⍝ ║ .⍀ ║ /⌿ ║  SHIFT   ║\n"
"╚═════════════╩════╩════╩════╩════╩════╩════╩════╩════╩════╩════╩══════════╝");

   loop(u, utf.size())   out << utf[u] << endl << flush;
   out << endl;
}
//----------------------------------------------------------------------------
bool
Cmd_KEYB::parse_xmodmap()
{
   if (UserPreferences::uprefs.disable_Quad_FIO__exec)   return true;   // error

   /* xmodmap prints an annoying
 
       xmodmap:  unable to open display ''

       message that we do not want to see (and it seems to be difficult
       to get rid of that message. We therefore test beforehand if
       XOpenDisplay() is likely to succeed.
    */
#if HAVE_LIBX11
   if (Display * display = XOpenDisplay(0))   XCloseDisplay(display);
   else                                       return true;   // error
#else
   return true;   // error
#endif

   if (access("/usr/bin/xmodmap", X_OK))
      {
        return true;
      }

   errno = 0;
FILE * xm = sys_popen("xmodmap -pke", "r");
   if (xm == 0)
      {
        MORE_ERROR() << "Command ]KEYB SCAN: Error starting xmodmap: "
                     << strerror(errno);
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return true;   // error
      }

int good_lines = 0;
int bad_lines  = 0;
   for (int line = 1 ;; ++line)
       {
         enum { BUFSIZE = 200 };
         char buffer[BUFSIZE + 1];
         if (!fgets(buffer, BUFSIZE, xm))   break;

         buffer[BUFSIZE] = 0;
         ssize_t len = strlen(buffer);
         if (buffer[len - 1] == '\n')   buffer[--len] = 0;
         if (parse_xmodmap_line(buffer, line))   ++bad_lines;
         else                                    ++good_lines;
       }

   if (good_lines == 0)
      {
        // xmodmap was started, but then something went wrong.
        //
        sys_pclose(xm);
        return true;
      }

   /* figure the number of Uxxx mappings per key. A typical APL keyboard that
      uses xmodmap (and no other keymap mechanism) has 66 keys with one or
      more Uxxx entries,
     
      If the counts are substantially smaller, then most likely xmodmap is
      used tigether with some other keyboard mapping mechanism, or the xmodmap
      is incomplete. We consider that an error.
    */
   {
     int Ucount = 0;
     loop(k, 256)
         {
           map_item & item = key_map[k];
           if (item.Ucount > 0)   ++Ucount;
         }

     if (Ucount <= 33)
        {
          MORE_ERROR() <<
               "Command ]KEYB SCAN: Too few Uxxx mappings in xmodmap -pke:\n"
               "    " << Ucount << " keys with one or more Uxxx mapping(s),\n";
          if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
          return true;   // error
        }
   }

   if (sys_pclose(xm))
      {
        MORE_ERROR() << "Command ]KEYB SCAN: Error running xmodmap: "
                     << strerror(errno);
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return true;   // error
      }

   /* a typical xmodmap -pke output line has:

      5  bad lines (header lines at the start of the output), and
     248 good lines.
    */

   if (good_lines < 100)
      {
        MORE_ERROR() << "Command ]KEYB SCAN: too few (" << good_lines
                     << ") good lines in the output of: xmodmap -pke";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return true;
      }

   if (bad_lines > 20)
      {
        MORE_ERROR() << "Command ]KEYB SCAN: too many (" << bad_lines
                     << ") bad lines in the output of: xmodmap -pke";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return true;
      }

   return false;   // OK
}
//----------------------------------------------------------------------------
bool
Cmd_KEYB::parse_xmodmap_line(const char * buffer, int line)
{
   keymap_from_xkbd = false;

const bool debug = false;

   // the first few lines, say 12, are header lines that can not be 
   // parsed.
   //
   if (*buffer == 0)   return true;   // empty line:

   debug && cout << endl << "─────────────────────────────" << endl
        << "line " << line << ": '" << buffer << "'" << endl;

int keycode = -1, len;
const char * p = buffer;
int rc = sscanf(p, "keycode %d = %n", &keycode, &len);
   if (rc < 1)
      {
        debug && cout << "line " << line << ": sscanf() failed (rc="
                      << rc << ")" << endl;
        return true;   // error
      }
   if (keycode <  0 || keycode >= 256)
      {
        debug && cout << "line " << line << ": sscanf() failed "
                         "(invalid keycode " << keycode << ")" << endl;
        return true;   // error
      }

   p += len;   // skip keycode

uint32_t unicodes[4] = { 0, 0, 0, 0 };
int j = 0;

int Ucount = 0;
   for (j = 0; j < 4; ++j)
       {
         debug && cout << "   See: '" << p << "'" << endl;
         uint32_t & uni = unicodes[j];
         if (parse_xmodmap_Unicode(Keycode(keycode), p, uni))   ++Ucount;
       }

   if (debug)
      {
        cout << "   └──── keycode[" << Ucount << "] " << keycode << ":";
        loop (jj, j)   cout << " 0x" << hex << unicodes[jj];
        cout << dec << endl;
      }

   // there are typically 136 lines with a keycode but without a Unicode.
   // These lines are reserved. That is, j=0 is not an error.

   map_item & item = key_map[keycode];
   item.keycode = keycode;
   item.Ucount = Ucount;
   loop(jj, j)   item.unicodes[jj] = Unicode(unicodes[jj]);

   return false;   // OK
}
//----------------------------------------------------------------------------
#if HAVE_X11_XKBLIB_H

bool
Cmd_KEYB::read_xkbd_map()
{
   keymap_from_xkbd = true;

   // 1. figure the keycodes in the templates
   //
   read_xkbd_template(FUNKEY_template, FUNKEY_rows);
   read_xkbd_template(MAIN_template,   MAIN_rows);
   read_xkbd_template(CURSOR_template, CURSOR_rows);
   read_xkbd_template(KEYPAD_template, KEYPAD_rows);

   return false;   // OK
}
//---------------------------------------------------------------------------
Unicode
Cmd_KEYB::read_xkbd_Ksym(_XDisplay * display, int keycode, int level)
{
KeySym symbol = XkbKeycodeToKeysym(display, keycode, 0, level);
   if (symbol == 0)
      {
        symbol = XkbKeycodeToKeysym(display, keycode, 1, level & 1);
      }

   if (symbol < 0x80)    return Unicode(symbol);
   if (symbol < 0x100)   return Unicode(symbol);
   if ((symbol & 0xFFFF0000) == 0x01000000)   return Unicode(symbol & 0xFFFF);
   return Unicode_0;
}
//---------------------------------------------------------------------------
void
Cmd_KEYB::read_xkbd_template(const char ** lines, int line_count)
{
Display * display = XOpenDisplay(0);

bool data_coming = false;
   loop(y, line_count - 2)
       {
         const UTF8_string u_utf(lines[y]);
         const UCS_string u(u_utf);

         if (u.size() && (u[0] == 0x250C ||   // ┌
                          u[0] == 0x2522 ||   // ┢
                          u[0] == 0x2523))    // ┣
            {
              data_coming = true;
              continue;
            }

         if (!data_coming)   continue;
         data_coming = false;

         const UTF8_string l_utf(lines[y + 1]);
         const UCS_string l(l_utf);
         for (size_t x = 1; x < u.size() - 1; ++x)
             {
               int ss = x;   while (u[ss] == UNI_SPACE)   ++ss;
               if (Avec::is_A_to_Z(u[ss]))   // keycode marker
                  {
                    // extract the keycode
                    //
                    int keycode = 0;
                    for (int xx = x; ; ++xx)
                        {
                           const Unicode cc = l[xx];
                           if (cc == U'│')   break;   // empty field
                           if (cc == U'┃')   break;   // empty field
                           if (cc == UNI_SPACE)   // leding or trailing space
                              {
                                if (keycode)   break;      // trailing: done
                                else           continue;   // leading:  skip
                              }

                          Assert(cc >= UNI_0 && cc <= UNI_9);
                          keycode = 10*keycode + (cc - UNI_0);
                        }
                    if (keycode == 0)   continue;

                    map_item & item = key_map[keycode];
                    item.keycode = keycode;
                    item.unicodes[0] = read_xkbd_Ksym(display, keycode, 0);
                    item.unicodes[1] = read_xkbd_Ksym(display, keycode, 1);
                    item.unicodes[2] = read_xkbd_Ksym(display, keycode, 2);
                    item.unicodes[3] = read_xkbd_Ksym(display, keycode, 3);

                    // show only base if shifted is the same
                    //
                    if (item.unicodes[0] == item.unicodes[1])
                        item.unicodes[1] = UNI_SPACE;
                    if (item.unicodes[2] == item.unicodes[3])
                        item.unicodes[3] = UNI_SPACE;
                  }
             }
       }

   XCloseDisplay(display);
}

#else // do not HAVE_X11_XKBLIB_H

bool
Cmd_KEYB::read_xkbd_map()
{
   MORE_ERROR() << "]KEYB XKBD failed: missing header file X11/XKBlib.h";
   if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;

   return true;   // error
}
#endif   // do/don't HAVE_X11_XKBLIB_H

//----------------------------------------------------------------------------
bool
Cmd_KEYB::parse_xmodmap_Unicode(Keycode keycode, const char * & p,
                                uint32_t & unicode)
{
   /* parse the keysym starting at p. The full line might look like ths:
 
      keycode  19 = 0 parenright U2227 U2372
                    │ │          │     │
                    └─┴──────────┴─────┴────── keysyms
 
      As can be seen above there are 3 major cases:

      1. a single ASCII like '0'
      2. a symbolic name like 'parenright'm or
      3. a Unicode like 'U2227'
    */
   unicode = Unicode_0;   // assume error

   while (*p == ' ')   ++p;   // skip leading whitespace
   if (*p == 0)   return false;

   if (*p && p[1] == ' ')   // case 1: single ASCII
      {
        unicode = *p;
        p += 2;
        return true;
      }

   if (*p == 'U')   // case 3: Uxxxx
      {
        int len = 0;
        sscanf(p, "U%x%n", &unicode, &len);
        if (len == 5)
           {
             p += 5;
             return true;
           }
      }

   // case 2: symbolic key. We use a subset of 
   // /usr/include/xkbcommon/xkbcommon-keysyms.h
   // to decode the name
   //
static const struct symkey
{
  const char * name;
   int         unicode;
} sym_map[] = {
{ "NoSymbol",       0x0000 },
{ "Escape",         0x001B },
{ "space",          0x0020 },
{ "exclam",         0x0021 },
{ "at macron",      0x0022 },
{ "quotedbl",       0x0022 },
{ "numbersign",     0x0023 },
{ "less",           0x003c },
{ "dollar",         0x0024 },
{ "percent",        0x0025 },
{ "ampersand",      0x0026 },
{ "apostrophe",     0x0027 },
{ "quoteright",     0x0027 },
{ "parenleft",      0x0028 },
{ "parenright",     0x0029 },
{ "asterisk",       0x002A },
{ "plus",           0x002B },
{ "colon",          0x002A },
{ "semicolon",      0x002B },
{ "comma",          0x002C },
{ "minus",          0x002D },
{ "period",         0x002E },
{ "slash",          0x002F },
{ "equal",          0x003D },
{ "greater",        0x003E },
{ "question",       0x003F },
{ "bracketleft",    0x005B },
{ "backslash",      0x005C },
{ "bracketright",   0x005D },
{ "asciicircum",    0x005E },
{ "underscore",     0x005F },
{ "grave",          0x0060 },
{ "braceleft",      0x007B },
{ "bar",            0x007C },
{ "braceright",     0x007D },
{ "asciitilde",     0x007E },
{ "cent",           0x00A2 },
{ "sterling",       0x00A3 },
{ "yen",            0x00A5 },
{ "brokenbar",      0x00A6 },
{ "section",        0x00A7 },
{ "diaeresis",      0x00A8 },
{ "guillemotleft",  0x00AB },
{ "macron",         0x00AF },
{ "guillemotright", 0x00BB },
{ "Odiaeresis",     0x00D6 },
{ "multiply",       0x00D7 },
{ "Udiaeresis",     0x00DC },
{ "ssharp",         0x00DF },
{ "adiaeresis",     0x00E4 },
{ "odiaeresis",     0x00F6 },
{ "udiaeresis",     0x00FC },
{ "division",       0x00F7 },
{ "Adiaeresis",     0x07A5 },
{ "notidentical",   0x2262 },
{ "elementof"   ,   0x2208 },
              };

   loop(s, sizeof(sym_map)/sizeof(*sym_map))
       {
         const symkey & sk = sym_map[s];
         const size_t len = strlen(sk.name);
         if (!strncmp(sk.name, p, len))   // found
            {
              unicode = sk.unicode;
              p += len;
              return true;
            }
       }

   // uppercase symbol names seem to be functions rather than characters
   //
   if (*p >= 'a')
      {
        CERR << "Keycode " << keycode
             << ": missing KEYSYM: '" << p << "'" << endl;
      }

   while (*p > ' ')   ++p;
   return false;
}
//----------------------------------------------------------------------------
void
Cmd_KEYB::get_template(UCS_string_vector & result, KB_Area area)
{
   Assert(result.size() == 0);

   if (area & KB_AREA_FUNKEY)   // including function keys
      {
        loop(y, FUNKEY_rows)
            {
              const UTF8_string utf(FUNKEY_template[y]);
              const UCS_string ucs(utf);
              result.push_back(ucs);
            }
      }

   loop (y, MAIN_rows)
        {
          const UTF8_string utf(MAIN_template[y]);
          const UCS_string ucs(utf);
          result.push_back(ucs);

          if (area & KB_AREA_CURSOR)
             {
               const UTF8_string utf(CURSOR_template[y]);
               const UCS_string ucs(utf);
               result.back() <<  "    " << ucs;
             }

          if (area & KB_AREA_KEYPAD)
             {
               const UTF8_string utf(KEYPAD_template[y]);
               const UCS_string ucs(utf);
               result.back() <<  "    " << ucs;
             }
        }
}
//----------------------------------------------------------------------------
ostream &
Cmd_KEYB::print_keycodes(ostream & out, KB_Area area)
{
   out << "Physical Keyboard:      Source: GNU APL builtin"
       << endl << endl;

UCS_string_vector lines;
   get_template(lines, area);

   loop(y, lines.size())   out << lines[y] << endl;
   out << endl;
   return out;
}
//----------------------------------------------------------------------------
ostream &
Cmd_KEYB::print_keymap(ostream & out, KB_Area area)
{
   out << "Keyboard Layout.    ";
   if (keymap_from_xkbd)   out << "Source: XkbKeycodeToKeysym()";
   else                    out << "Source: xmodmap -pke";
   out << endl << endl;

UCS_string_vector lines;
   get_template(lines, area);

const int rows = lines.size();

bool data_coming = false;
   loop(y, rows - 2)
       {
         UCS_string & u = lines[y];
         if (u.size() && (u[0] == 0x250C ||   // ┌
                          u[0] == 0x2522 ||   // ┢
                          u[0] == 0x2523))    // ┣
         {
              data_coming = true;
              continue;
            }
         if (!data_coming)   continue;
         data_coming = false;

         UCS_string & l = lines[y + 1];
         for (size_t x = 1; x < u.size(); ++x)
             {
               int ss = x;   while (u[ss] == UNI_SPACE)   ++ss;
               if (Avec::is_A_to_Z(u[ss]))   // keycode marker
                  {
                    // extract the keycode
                    //
                    int keycode = 0;
                    int k0 = 0;   // first keycode digit
                    for (int xx = x; ; ++xx)
                        {
                           const Unicode cc = l[xx];
                           if (cc == UNI_SPACE)   // leding or trailing space
                              {
                                if (keycode)   break;      // trailing: done
                                else           continue;   // leading:  skip
                              }

                          if (k0 == 0)   k0 = xx;
                          Assert(cc >= UNI_0 && cc <= UNI_9);
                          keycode = 10*keycode + (cc - UNI_0);
                        }
                    const map_item & item = key_map[keycode];

                    // clear the field
                    //
                    {
                      int xx = x;
                      while (u[xx - 1] != U'│' && u[xx - 1] != U'┃')   --xx;
                      for (; u[xx] != U'│' && u[xx] != U'┃'; ++xx)
                          { u[xx] = l[xx] = UNI_SPACE; }
                    }

                    // fill in characters
                    //
                    switch(keycode)   // special key names
                       {
                         case  8:  copy_text(l[x+1],  "ESC");      goto next;
                         case 22:  copy_text(l[x+1],  "BACKSPC");  goto next;
                         case 23:  copy_text(l[x+2],  "TAB");      goto next;
                         case 36:  copy_text(l[x+1],  "RETURN");   goto next;
                         case 37:  copy_text(l[x],    "CTRL");     goto next;
                         case 50:  copy_text(l[x+4],  "SHIFT");    goto next;
                         case 62:  copy_text(l[x+2],  "SHIFT");    goto next;
                         case 65:  copy_text(l[x+14], "SPACE");    goto next;
                         case 66:  copy_text(u[x+1],  "(CAPS");  
                                   copy_text(l[x+2],  "LOCK)");    goto next;
                         case 105: copy_text(l[x],    "CTRL");     goto next;
                         case 133: 
                         case 134: copy_text(l[x+1],  "Win");      goto next;
                       }

                    if (item.keycode)
                       {
                         if (uint32_t uni = item.unicodes[0])   // lowercase
                            {
                              l[k0] = Unicode(uni & 0xFFFF);
                            }
                         if (uint32_t uni = item.unicodes[1])   // SHIFT
                            {
                              u[k0] = Unicode(uni & 0xFFFF);
                            }
                         if (uint32_t uni = item.unicodes[2])   // ALT
                            {
                              l[k0+1] = Unicode(uni & 0xFFFF);
                            }
                         if (uint32_t uni = item.unicodes[3])   // ALT SHIFT
                            {
                              u[k0+1] = Unicode(uni & 0xFFFF);
                            }
                       }
                  }

               next:
               while (x < u.size() && u[x] != U'│' && u[x] != U'┃')   ++x;
             }
       }

   for (int y = 0; y < rows; ++y)   out << lines[y] << endl;
   return out;
}
//----------------------------------------------------------------------------
void
Cmd_KEYB::copy_text(Unicode & start, const char * text)
{
Unicode * p = &start;
   while (*text)   *p++ = Unicode(*text++);
}
//----------------------------------------------------------------------------
// EOF
