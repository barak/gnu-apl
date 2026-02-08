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

#include "Common.hh"
#include "Command.hh"
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

const char * funkey_template[] = {
"╔════╗    ╔════╦════╦════╦════╗    ╔════╦════╦════╦════╗    ╔════╦════╦════╦════╗    ╔════╦════╦════╗",
"║ Kc ║    ║ Kc ║ Kc ║ Kc ║ Kc ║    ║ Kc ║ Kc ║ Kc ║ Kc ║    ║ Kc ║ Kc ║ Kc ║ Kc ║    ║    ║ Kc ║ Kc ║",
"║ 09 ║    ║ 67 ║ 68 ║ 69 ║ 70 ║    ║ 71 ║ 72 ║ 73 ║ 74 ║    ║ 75 ║ 76 ║ 95 ║ 96 ║    ║    ║ 78 ║ 77 ║",
"╚════╝    ╚════╩════╩════╩════╝    ╚════╩════╩════╩════╝    ╚════╩════╩════╩════╝    ╚════╩════╩════╝",
"",
};

const char * keypad_template[] = {
"╔════╦════╦════╦════╗",
"║ Kc ║    ║ Kc ║ Kc ║",
"║ 77 ║    ║ 82 ║ 82 ║",
"╠════╬════╬════╬════╣",
"║ Kc ║ Kc ║ Kc ║    ║",
"║ 79 ║ 80 ║ 81 ║ Kc ║",
"╠════╬════╬════╣ 86 ║",
"║ Kc ║ Kc ║ Kc ║    ║",
"║ 83 ║ 84 ║ 85 ║    ║",
"╠════╬════╬════╬════╣",
"║ Kc ║ Kc ║ Kc ║    ║",
"║ 87 ║ 88 ║ 89 ║ Kc ║",
"╠════╩════╬════╣108 ║",
"║ Kc      ║ Kc ║    ║",
"║ 90      ║ 91 ║    ║",
"╚═════════╩════╩════╝",
};

const char * main_template[] = {
"╔════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦═════════╗",
"║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc      ║",
"║ 49 ║ 10 ║ 11 ║ 12 ║ 13 ║ 14 ║ 15 ║ 16 ║ 17 ║ 18 ║ 19 ║ 20 ║ 21 ║ 22      ║",
"╠════╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦══════╣",
"║  Kc   ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║  Kc  ║",
"║  23   ║ 24 ║ 25 ║ 26 ║ 27 ║ 28 ║ 29 ║ 30 ║ 31 ║ 32 ║ 33 ║ 34 ║ 35 ║  51  ║",
"╠═══════╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩══════╣",
"║  Kc     ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc      ║",
"║  66     ║ 38 ║ 39 ║ 40 ║ 41 ║ 42 ║ 43 ║ 44 ║ 45 ║ 46 ║ 47 ║ 48 ║ 36      ║",
"╠═════════╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═════════╣",
"║    Kc       ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║   Kc     ║",
"║    50       ║ 52 ║ 53 ║ 54 ║ 55 ║ 56 ║ 57 ║ 58 ║ 59 ║ 60 ║ 61 ║   62     ║",
"╠════╦═════╦══╩══╦═╩════╩════╩════╩════╩════╩════╩══╦═╩═══╦╩════╬═════╦════╣",
"║ Kc ║ Kc  ║ Kc  ║                Kc                ║ Kc  ║ Kc  ║ Kc  ║ Kc ║",
"║ 37 ║115  ║ 64  ║                65                ║113  ║116  ║ 109 ║105 ║",
"╚════╩═════╩═════╩══════════════════════════════════╩═════╩═════╩═════╩════╝",
                        };

enum {
   FUNKEY_rows = sizeof(funkey_template) / sizeof(char *),
   MAIN_rows = sizeof(main_template)     / sizeof(char *),
   KEYPAD_rows = sizeof(keypad_template) / sizeof(char *),
     };

static_assert(MAIN_rows == KEYPAD_rows);

Cmd_KEYB::map_item Cmd_KEYB::key_map[];

//----------------------------------------------------------------------------
void
Cmd_KEYB::cmd_KEYB(ostream & out, const UCS_string_vector & args)
{
int area = 0;   // main keys

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
   loop(a, args.size())
      {
        if      (args[a].starts_iwith("XMOD"))   mode |= MO_XMODMAP;
        else if (args[a].starts_iwith("XKBD"))   mode |= MO_XKBD;
        else if (args[a].starts_iwith("USER"))   mode |= MO_USERFILE;
        else if (args[a].starts_iwith("GUESS"))  mode |= MO_BUILTIN;
        else if (args[a].starts_iwith("KEYS"))   mode |= MO_KEYS;
        else if (args[a].starts_iwith("KPAD"))   area |= 1;
        else if (args[a].starts_iwith("FUNK"))   area |= 2;
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

   // if neither xmodmap nor xkdb is specified then use xmodmap
   //
   if (!(mode & (MO_XMODMAP | MO_XKBD)))   mode |= MO_XMODMAP;

   if (mode & MO_KEYS)
      {
        print_keycodes(out, area);
      }

   if (mode & MO_XKBD)
      {
        const bool got_map = ! read_xkbd_map();
        if (got_map)   print_keymap(out, area);
        return;
      }

const bool do_xmodmap = (mode & MO_XMODMAP) &&
                         ! (no_mode && UserPreferences::uprefs.no_xmodmap);
   if (do_xmodmap)
      {
        // reset SIGCHLD to its default so that pclose() works as expected
        //
        signal(SIGCHLD, SIG_DFL);
        const bool xmodmap_error = parse_xmodmap();
        signal(SIGCHLD, SIG_IGN);

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
                  print_keymap(out, area);
                }
             return;
           }

        if (!xmodmap_error)   // xmodmap succeeded
           {
             print_keymap(out, area);
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
FILE * xm = popen("xmodmap -pke", "r");
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
        pclose(xm);
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

   if (pclose(xm) && (errno != ECHILD))
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
         if (parse_Unicode(Keycode(keycode), p, unicodes[j]))   ++Ucount;
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
   read_xkbd_template(main_template,   MAIN_rows);
   read_xkbd_template(funkey_template, FUNKEY_rows);
   read_xkbd_template(keypad_template, KEYPAD_rows);

   return false;   // OK
}
//---------------------------------------------------------------------------
Unicode
Cmd_KEYB::read_ksym(_XDisplay * display, int keycode, int level)
{
const KeySym symbol = XkbKeycodeToKeysym(display,keycode, 0, level);

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

   loop(y, line_count - 1)
       {
         const char * line  = lines[y];
         const char * line1 = lines[y + 1];
         const size_t len = strlen(line);
         for (size_t x = 1; x < len - 1; ++x)
             {
               if (line[x] == 'K' && line[x + 1] == 'c')
                  {
                    // fix UTF8 offsets
                    const char * p = line1 + x - 1;
                    while (*p & 0x80)   ++p;

                    const int keycode = strtol(p, 0, 10) & 0xFF;
                    map_item & item = key_map[keycode];
                    item.keycode = keycode;
                    item.unicodes[0] = read_ksym(display, keycode, 0);
                    item.unicodes[1] = read_ksym(display, keycode, 1);
                    item.unicodes[2] = read_ksym(display, keycode, 2);
                    item.unicodes[3] = read_ksym(display, keycode, 3);
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
Cmd_KEYB::parse_Unicode(Keycode keycode, const char * & p, uint32_t & unicode)
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
   unicode = 0;   // assume error

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
ostream &
Cmd_KEYB::print_keycodes(ostream & out, int area)
{
   out << "Physical Keyboard:      Source: GNU APL builtin"
       << endl << endl;

int fun_rows = 0;
UCS_string_vector lines;

   if (area & 2)   // including function keys
      {
        fun_rows = FUNKEY_rows;
        loop(y, fun_rows)
            {
              const UTF8_string utf(funkey_template[y]);
              const UCS_string ucs(utf);
              lines.push_back(ucs);
            }
      }

   loop (y, MAIN_rows)
       {
         const UTF8_string utf(main_template[y]);
         const UCS_string ucs(utf);
         lines.push_back(ucs);
         if (area & 1)
            {
              const UTF8_string utf(keypad_template[y]);
              UCS_string ucs(utf);
              lines.back() <<  "    " << ucs;
            }
       }

const int rows = lines.size();
   loop(y, rows)   out << lines[y] << endl;
   out << endl;
   return out;
}
//----------------------------------------------------------------------------
ostream &
Cmd_KEYB::print_keymap(ostream & out, int area)
{
   out << "Keyboard Layout.    ";
   if (keymap_from_xkbd)   out << "Source: XkbKeycodeToKeysym()";
   else                    out << "Source: xmodmap -pke";
   out << endl << endl;

UCS_string_vector lines;

   loop (y, MAIN_rows)
       {
         const UTF8_string utf(main_template[y]);
         const UCS_string ucs(utf);
         lines.push_back(ucs);
         if (area & 1)
            {
              const UTF8_string utf(keypad_template[y]);
              UCS_string ucs(utf);
              lines.back() <<  "    " << ucs;
            }
       }

const int rows = lines.size();

   loop(y, rows - 2)
       {
         UCS_string & u = lines[y];
         UCS_string & l = lines[y + 1];
         for (size_t x = 1; x < u.size(); ++x)
             {
               if (u[x] == UNI_K && u[x+1] == UNI_c)   // Kc marker
                  {
                    // get keycode
                    const int h = l[x - 1] == UNI_SPACE   // hundreds
                                            ? 0 : l[x - 1] - UNI_0;
                    const int t = l[x] - UNI_0;                    // tens
                    const int o = l[x + 1] - UNI_0;                // tens
                    const int keycode = 100*h + 10*t + o;
                    const map_item & item = key_map[keycode];

                   // clear key field
                   //
                    u[x] = u[x+1] = l[x-1] = l[x] = l[x+1] = UNI_SPACE;

                    // fill in characters
                    //
                    switch(keycode)   // special key names
                       {
                         case  9:   // ESC
                              l[x] = UNI_E;   l[x+1] = UNI_S; l[x+2] = UNI_C;
                              continue;

                         case 22:   // BACKSPACE
                              l[x] = UNI_B;   l[x+1] = UNI_A; l[x+2] = UNI_C;
                              l[x+3] = UNI_K; l[x+4] = UNI_S; l[x+5] = UNI_P;
                              continue;

                         case 23:   // TAB
                              l[x] = UNI_T;   l[x+1] = UNI_A; l[x+2] = UNI_B;
                              continue;   // BACKSPACE

                         case 36:   // RETURN
                              l[x] = UNI_R;   l[x+1] = UNI_E; l[x+2] = UNI_T;
                              l[x+3] = UNI_U; l[x+4] = UNI_R; l[x+5] = UNI_N;
                              continue;

                         case 37:
                         case 105:   // CTRL
                              l[x-1] = UNI_C;   l[x] = UNI_T; l[x+1] = UNI_R;
                              l[x+2] = UNI_L;
                              continue;

                         case 50:
                         case 62:   // SHIFT
                              l[x] = UNI_S;   l[x+1] = UNI_H; l[x+2] = UNI_I;
                              l[x+3] = UNI_F; l[x+4] = UNI_T;
                              continue;

                         case 65:   // SPACE
                              l[x-2] = UNI_S; l[x-1] = UNI_P; l[x] = UNI_A;
                              l[x+1] = UNI_C; l[x+2] = UNI_E;
                              continue;

                         case 66:   // CAPS LOCK
                              u[x] = UNI_L_PARENT; u[x+1] = UNI_C;
                              u[x+2] = UNI_A; u[x+3] = UNI_P; u[x+4] = UNI_S;
                              l[x] = UNI_L;   l[x+1] = UNI_O; l[x+2] = UNI_C;
                              l[x+3] = UNI_K; l[x+4] = UNI_R_PARENT;
                              continue;

                         case  64:
                         case 113:
                              l[x] = UNI_A;   l[x+1] = UNI_L; l[x+2] = UNI_T;
                              continue;
                         case 115:
                         case 116:
                              l[x] = UNI_W;   l[x+1] = UNI_i; l[x+2] = UNI_n;
                              continue;
                       }

                    if (const Unicode uni = item.unicodes[0])   // lowercase
                       {
                         l[x] = Unicode(uni & 0xFFFF);
                       }
                    if (const Unicode uni = item.unicodes[1])   // SHIFT
                       {
                         u[x] = Unicode(uni & 0xFFFF);
                       }
                    if (const Unicode uni = item.unicodes[2])   // ALT
                       {
                         l[x+1] = Unicode(uni & 0xFFFF);
                       }
                    if (const Unicode uni = item.unicodes[3])   // ALT
                       {
                         u[x+1] = Unicode(uni & 0xFFFF);
                       }
                  }
             }
       }

   for (int y = 0; y < rows; ++y)   out << lines[y] << endl;
   return out;
}
//----------------------------------------------------------------------------
// EOF
