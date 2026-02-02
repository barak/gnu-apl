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

// See: /usr/share/X11/xkb/keycodes/xfree86

const char * Cmd_KEYB::funkey_template[] = {
"╔════╗    ╔════╦════╦════╦════╗    ╔════╦════╦════╦════╗    ╔════╦════╦════╦════╗    ╔════╦════╦════╗",
"║ Kc ║    ║ Kc ║ Kc ║ Kc ║ Kc ║    ║ Kc ║ Kc ║ Kc ║ Kc ║    ║ Kc ║ Kc ║ Kc ║ Kc ║    ║    ║ Kc ║ Kc ║",
"║ 09 ║    ║ 67 ║ 68 ║ 69 ║ 70 ║    ║ 71 ║ 72 ║ 73 ║ 74 ║    ║ 75 ║ 76 ║ 95 ║ 96 ║    ║    ║ 78 ║ 77 ║",
"╚════╝    ╚════╩════╩════╩════╝    ╚════╩════╩════╩════╝    ╚════╩════╩════╩════╝    ╚════╩════╩════╝",
"",
};

const char * Cmd_KEYB::keypad_template[] = {
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
"║ Kc      ║    ║    ║",
"║ 90      ║    ║    ║",
"╚═════════╩════╩════╝",
};

const char * Cmd_KEYB::layout_template[] = {
"╔════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦════╦═════════╗",
"║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc      ║",
"║ 49 ║ 10 ║ 11 ║ 12 ║ 13 ║ 14 ║ 15 ║ 16 ║ 17 ║ 18 ║ 19 ║ 20 ║ 21 ║ 22      ║",
"╠════╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦═╩══╦══════╣",
"║  Kc   ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc   ║",
"║  23   ║ 24 ║ 25 ║ 26 ║ 27 ║ 28 ║ 29 ║ 30 ║ 31 ║ 32 ║ 33 ║ 34 ║ 35 ║ 51   ║",
"╠═══════╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩═╦══╩══════╣",
"║ Kc      ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc      ║",
"║ 66      ║ 38 ║ 39 ║ 40 ║ 41 ║ 42 ║ 43 ║ 44 ║ 45 ║ 46 ║ 47 ║ 48 ║ 36      ║",
"╠═════════╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═══╦╩═════════╣",
"║  Kc         ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║ Kc ║  Kc      ║",
"║  50         ║ 52 ║ 53 ║ 54 ║ 55 ║ 56 ║ 57 ║ 58 ║ 59 ║ 60 ║ 61 ║  62      ║",
"╠════╦═════╦══╩══╦═╩════╩════╩════╩════╩════╩════╩══╦═╩═══╦╩════╬═════╦════╣",
"║ Kc ║ Kc  ║ Kc  ║              Kc                  ║ Kc  ║ Kc  ║ Kc  ║ Kc ║",
"║ 37 ║115  ║ 64  ║              65                  ║113  ║116  ║ 109 ║105 ║",
"╚════╩═════╩═════╩══════════════════════════════════╩═════╩═════╩═════╩════╝",
                        };

Cmd_KEYB::map_item Cmd_KEYB::key_map[];

//----------------------------------------------------------------------------
void
Cmd_KEYB::cmd_KEYB(ostream & out, const UCS_string_vector & args)
{
bool keys = false;
int area = 0;   // main keys

   enum
      {
        MO_NONE     = 0,
        MO_XMODMAP  = 1,
        MO_USERFILE = 2,
        MO_BUILTIN  = 4,
        MO_ALL      = MO_XMODMAP | MO_USERFILE | MO_BUILTIN
      };

int mode = MO_NONE;
   loop(a, args.size())
      {
        if      (args[a].starts_iwith("SCAN"))   mode |= MO_XMODMAP;
        else if (args[a].starts_iwith("USER"))   mode |= MO_USERFILE;
        else if (args[a].starts_iwith("GUESS"))  mode |= MO_BUILTIN;
        else if (args[a].starts_iwith("KEYS"))   keys = true;
        else if (args[a].starts_iwith("KPAD"))   area |= 1;
        else if (args[a].starts_iwith("FUNK"))   area |= 2;
        else
           {
             MORE_ERROR() << "Command ]KEYB: invalid argument '"
                          << args[a] << "'";
             if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
             return;
           }
      }
   if (mode == MO_NONE)   mode = MO_ALL;   // if no specific mode(s) given

   if (mode & MO_XMODMAP)
      {
        // reset SIGCHLD to its default so that pclose() works as expected
        //
        signal(SIGCHLD, SIG_DFL);
        const bool xmodmap_error = parse_xmodmap();
        signal(SIGCHLD, SIG_IGN);

        if (mode != MO_ALL)
           {
             if (xmodmap_error)
                {
                  MORE_ERROR() << "running xmodmap failed.";
                  if (Command::auto_MORE)
                     CERR << Workspace::more_error() << endl;
                }
             else
                {
                  Workspace::more_error().clear();
                  print_xmodmap(out, keys, area);
                }
             return;
           }

        if (!xmodmap_error)   // xmodmap succeeded
           {
             print_xmodmap(out, keys, area);
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
FILE * xm = popen("xmodmap -pk", "r");
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
        return true;
      }

   if (pclose(xm) && (errno != ECHILD))
      {
        MORE_ERROR() << "Command ]KEYB SCAN: Error running xmodmap: "
                     << strerror(errno);
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return true;   // error
      }

   /* a typical xmodmap has:

      5  bad lines (header lines at the start of the output), and
     248 good lines.
    */

   if (good_lines < 100)
      {
        MORE_ERROR() << "Command ]KEYB SCAN: too few (" << good_lines
                     << ") good lines in the output of: xmodmap -pk";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return true;
      }

   if (bad_lines > 20)
      {
        MORE_ERROR() << "Command ]KEYB SCAN: too many (" << bad_lines
                     << ") bad lines in the output of: xmodmap -pk";
        if (Command::auto_MORE)   CERR << Workspace::more_error() << endl;
        return true;
      }

   return false;   // OK
}
//----------------------------------------------------------------------------
bool
Cmd_KEYB::parse_xmodmap_line(const char * buffer, int line)
{
const bool debug = false;

   // the first few lines, say 12, are header lines that can not be 
   // parsed.
   //
   if (*buffer == 0)   return true;   // empty line:

   debug && cout << endl << "─────────────────────────────" << endl
        << "line " << line << ": '" << buffer << "'" << endl;

int keycode = -1, len;
const char * p = buffer;
int rc = sscanf(p, " %d%n", &keycode, &len);
   if (rc < 1)
      {
        debug && cout << "line " << line << ": sscanf() failed ("
                      << rc << ")" << endl;
        return true;   // error
      }
   p += len;

unsigned int unicodes[4] = { 0, 0, 0, 0 };
int j = 0;

char keyname[42];   // ignored
   for (j = 0; j < 4; ++j)
       {
         *keyname = 0;
         if (debug)
         debug && cout << "   See: '" << p << "'";
         rc = sscanf(p, " %x %s%n", &unicodes[j], keyname, &len);
         debug && cout << "  j=" << j << " rc=" << rc << " len=" << len
              << "    " << keyname << endl;
         if (*keyname == 0)   break;
         p += len;
       }

   if (debug)
      {
        cout << "   └──── keycode[" << j << "] " << keycode << ":";
        for (int jj = 0; jj < j; ++jj)
            cout << " 0x" << hex << unicodes[jj];
        cout << dec << endl;
      }

   if (keycode <  0)     return true;
   if (keycode >= 256)   return true;

   // there are typically 136 lines with a keycode but without s Unicode.
   // Thse lines are reserved. That is, j=0 is not an error.

   map_item & item = key_map[keycode];
   item.keycode = keycode;
   for (int jj = 0; jj < j; ++jj)
       {
         item.unicodes[jj] = Unicode(unicodes[jj]);
       }

   return false;   // OK
}
//----------------------------------------------------------------------------
ostream &
Cmd_KEYB::print_xmodmap(ostream & out, bool keys, int area)
{
   out << "US Keyboard Layout.    ";
   if (keys)   out << "Source: GNU APL builtin" << endl << endl;
   else        out << "Source: xmodmap -pk" << endl << endl;

const int main_rows = sizeof(layout_template) / sizeof(*layout_template);
int fun_rows = 0;
UCS_string_vector lines;

   if (keys && (area & 2))   // including function keys
      {
        fun_rows = sizeof(funkey_template) / sizeof(*funkey_template);
        for (int y = 0; y < fun_rows; ++y)
            {
              const UTF8_string utf(funkey_template[y]);
              const UCS_string ucs(utf);
              lines.push_back(ucs);
            }
      }

   for (int y = 0; y < main_rows; ++y)
       {
         const UTF8_string utf(layout_template[y]);
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
   if (keys)
      {
        for (int y = 0; y < rows; ++y)   out << lines[y] << endl;
        out << endl;
         return out;
      }

   for (int y = 0; y < (rows - 2); ++y)
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
                    switch(keycode)
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
