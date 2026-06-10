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
#include <fcntl.h>

#include "Common.hh"
#include "LibPaths.hh"
#include "PointerCell.hh"
#include "Quad_GTK.hh"
#include "Security.hh"
#include "Workspace.hh"

Quad_GTK Quad_GTK::fun;

std::vector<Quad_GTK::window_entry> Quad_GTK::open_windows;
UCS_string_vector Quad_GTK::event_queue;

bool Quad_GTK::focus_on_map = false;   // focus for new windows

#if apl_GTK3 && apl_X11 && ! MINGW_SRC

#include <poll.h>
#include <sys/ioctl.h>

//════════════════════════════════════════════════════════════════════════════
void
Quad_GTK::close_all_windows()
{
   while(open_windows.size())
      {
        close_window(open_windows.back().fd);   // does open_windows.pop_back()
      }
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_GTK::eval_AB(Value_P A, Value_P B) const
{
   CHECK_SECURITY(disable_Quad_GTK);

   if (A->get_rank() > 1)   RANK_ERROR;
   if (B->get_rank() > 1)   RANK_ERROR;
   if (A->is_char_array() && B->is_char_array())
      {
         const UCS_string css_name_or_data = A->get_UCS_ravel();
         const UCS_string gui_name_or_data = B->get_UCS_ravel();
         const int fd = open_window(gui_name_or_data, &css_name_or_data);
         return Token(TOK_APL_VALUE1, IntScalar(fd, LOC));
      }

   if (!B->is_int_scalar())
      {
        MORE_ERROR() <<
"A ⎕GTK B expects an integer scalar B (function number)\n"
"      or a text vector B (XML filename or string";
        DOMAIN_ERROR;
      }

const int function = B->get_cfirst().get_int_value();
int fd = -1;
   switch(function)
      {
        case 0: // close window/GUI
             if (!A->is_int_scalar())   goto bad_fd;
             fd = A->get_cfirst().get_int_value();
             return Token(TOK_APL_VALUE1, close_window(fd));

        case 3: // increase verbosity
             if (!A->is_int_scalar())   goto bad_fd;
             fd = A->get_cfirst().get_int_value();
             if (write_TL0(fd, 7))
                {
                  CERR << "write(Tag 7) failed in Ah ⎕GTK 3" << endl;
                  return Token(TOK_APL_VALUE1, IntScalar(-3, LOC));
                }
             return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

        case 4: // decrease verbosity
             if (!A->is_int_scalar())   goto bad_fd;
             fd = A->get_cfirst().get_int_value();
             if (write_TL0(fd, 8))
                {
                  CERR << "write(Tag 8) failed in Ah ⎕GTK 4" << endl;
                  return Token(TOK_APL_VALUE1, IntScalar(-4, LOC));
                }
             return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

        case 5: // function name for function number A
             if (A->is_scalar())
                return Token(TOK_APL_VALUE1, fnum_to_function_name(
                                     Fnum(A->get_cscalar().get_int_value())));
             RANK_ERROR;

        case 6: // widget class for function number A
             if (A->is_scalar())
                return Token(TOK_APL_VALUE1, fnum_to_widget_class(
                                     Fnum(A->get_cscalar().get_int_value())));
             RANK_ERROR;

        default: MORE_ERROR() << "Invalid function number Bi=" << function
                              << " in A ⎕GTK Bi";
                 DOMAIN_ERROR;
      }

   MORE_ERROR() << "Unexpected A or B in A ⎕GTK B";
   DOMAIN_ERROR;

bad_fd:
   MORE_ERROR() << "Ah ⎕GTK " << function
                << " expects a handle (i.e. an integer scalar) Ah";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_GTK::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
   CHECK_SECURITY(disable_Quad_GTK);

   /*
      1. Let:

         GTK ← CSS ⎕GTK UI   ⍝ GTK handle, CSS is optional

      2. Let UI be a file or string like:
         ...
         <object class="GtkWindow" id="window1">         (1)
         ...
             <object class="GtkEntry" id="entry1">       (2)
         ...

      3. Let:

         X ← GTK, "window1"   ⍝ X is aka. H_ID
         B ← "set_text"       ⍝ aka. the function name    (3)
         Z ← A ⎕GTK[X] B

         That selects per (1) above a GUI window, and per (2) a widget.
         The Gtk_server resolves the class= in (1) and (3) into a function
         and calls it.
    */

   if (B->get_rank() > 1)   RANK_ERROR;

   // split X into handle ↑X and widget_id 1↓X
   //
UTF8_string widget_id;            // e.g. "entry1" from id= in .ui
const int fd = resolve_window(X.get(), widget_id);
   write_TLV(fd, 6, widget_id);   // select widget

Fnum fun = FNUM_INVALID;
   if      (B->is_int_scalar())    fun = Fnum(B->get_cfirst().get_int_value());
   else if (B->is_char_string())   fun = resolve_fun_name(widget_id, B.get());
   else
      {
        MORE_ERROR() <<
"A ⎕GTK B expects an integer scalar B (function number)\n"
"      or a text vector B (XML filename or string";
        DOMAIN_ERROR;
      }

   if (fun == FNUM_INVALID)   DOMAIN_ERROR;   // not found

int command_tag = -1;
int response_tag = -1;
Gtype Atype = gtype_V;   // assume void

   switch(fun)
      {
        case FNUM_INVALID: DOMAIN_ERROR;
        case FNUM_0:   return Token(TOK_APL_VALUE1, IntScalar(fd, LOC));

#define gtk_event_def(ev_ename, ...)
#define gtk_fun_def(ID_prefix, gtk_class, gtk_function, _Z, A)               \
        case FNUM_ ## gtk_class ## _ ## gtk_function:                      \
             command_tag = Command_ ## gtk_class ## _ ## gtk_function;     \
             response_tag = Response_ ## gtk_class ## _ ## gtk_function;   \
             Atype = get_gtype(#A);                                        \
             break;
#include "Gtk/Gtk_map.def"

        default:
             MORE_ERROR() << "Bad function B in A ⎕GTK B (B='"
                          << *B << ", fun=" << fun;
             DOMAIN_ERROR;
      }

   if (Atype == gtype_V)    VALENCE_ERROR;
   if (A->get_rank() > 1)   RANK_ERROR;

UCS_string ucs_A;
   if (fun == FNUM_GtkDrawingArea_draw_commands)
      {
        loop(a, A->element_count())
            {
              const Cell & cell = A->get_cravel(a);
              if (!cell.is_pointer_cell())
                 {
                    MORE_ERROR() << "A ⎕GTK " << fun
                                 << " expects A to be a vector of "
                                    "draw commands (strings)";
                    DOMAIN_ERROR;
                 }
              Value_P command = cell.get_pointer_value();
              ucs_A << UCS_string(*command);
              ucs_A << UNI_LF;
            }
      }
   else
      {
        if (!A->is_char_string())
           {
             MORE_ERROR() << "A ⎕GTK[X] B expects A to be a text vector";
             DOMAIN_ERROR;
           }

        ucs_A = UCS_string(*A);
      }
UTF8_string utf_A(ucs_A);
   write_TLV(fd, command_tag, utf_A);
   return Token(TOK_APL_VALUE1, poll_response(fd, response_tag));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_GTK::eval_B(Value_P B) const
{
   CHECK_SECURITY(disable_Quad_GTK);

   if (B->get_rank() > 1)   RANK_ERROR;

   if (B->element_count() == 0)   // empty B: print GTK help
      {
        COUT <<
"   ⎕GTK Usage:\n"
"\n"
"   H←    ⎕GTK XML   ⍝ with string XML (filename or content): open window\n"
"   H←CSS ⎕GTK XML   ⍝ with strings CSS and XML: open window with styles CSS\n"
"       H ⎕GTK 0     ⍝ close the ⎕GTK window with handle H\n"
"       H ⎕GTK 3     ⍝ increase the (debug-) verbosity for window handle H\n"
"       H ⎕GTK 4     ⍝ decrease the (debug-) verbosity for window handle H\n"
"   Z←  A ⎕GTK 5     ⍝ Z is the function name for function number A\n"
"   Z←  A ⎕GTK 6     ⍝ Z is the widget class for function number A\n"
"   Z←    ⎕GTK 0     ⍝ list Z of open ⎕GTK handles\n"
"   E←    ⎕GTK 1     ⍝ wait for next GTK Event E (blocking)\n"
"   E←    ⎕GTK 2     ⍝ poll for next GTK Event E (non-blocking)\n"
"         ⎕GTK 3     ⍝ close all open ⎕GTK windows"
"         ⎕GTK 4     ⍝ focus_on_map (subsequent windows will grab the focus)\n"
"         ⎕GTK 5     ⍝ clear focus_on_map (APL will remain focused)\n"
             ;

        return Token(TOK_APL_VALUE1, Idx0(LOC));
      }

   if (B->is_char_array())
      {
         const UCS_string gui_filename = B->get_UCS_ravel();
         const int fd = open_window(gui_filename, /* no CSS */ 0);
         return Token(TOK_APL_VALUE1, IntScalar(fd, LOC));
      }

   if (!B->is_int_scalar())
      {
        MORE_ERROR() <<
"⎕GTK B expects an integer scalar B (function number)\n"
"      or a text vector B (XML filename or string";
        DOMAIN_ERROR;
      }

const int function = B->get_cfirst().get_int_value();
   switch(function)
      {
        case 0:   // list of open fds
             return Token(TOK_APL_VALUE1, window_list());

        case 1: // blocking poll for next event
        case 2: // non-blocking wait for next event
             poll_all();
             if (function == 2 && event_queue.size() == 0)   // non-blocking
                return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

             // blocking or events in event_queue...
             //
             // We can not really block because the user may want to bump
             // out of ⎕GTK with ^C.
             //
             while (event_queue.size() == 0 && !InterruptContext::interrupt_is_raised())
                   {
                     usleep(10000);
                     poll_all();
                   }

             // at this point either event_queue.size() > 0 or an interrupt
             // was raised
             //
             if (event_queue.size() == 0)   // hence interrupt was raised
                {
                   InterruptContext::clear_interrupt_raised(LOC);
                   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));
                }

             {
               UCS_string HWF = event_queue[0];   // H (fd) : Widget : Fun
               event_queue.erase(0);

               // split (Unicode)Handle:"widget:function"
               // into a 3-element APL vector Handle (⊂"widget") (⊂"function")
               //
               UCS_string_vector args;
               UCS_string arg;
               for (ShapeItem j = 1; j < HWF.ssize(); ++j)
                   {
                     if (HWF[j] == UNI_COLON)
                        {
                          args.push_back(arg);
                          arg.clear();
                        }
                     else
                        {
                          arg << HWF[j];
                        }
                   }
               args.push_back(arg);

               Value_P Z(1 + args.size(), LOC);
               Z->next_ravel_Int(HWF[0]);   // handle
               loop(a, args.size())
                   {
                     Value_P Za(args[a], LOC);
                     Z->next_ravel_Pointer(Za.get());
                   }
               Z->check_value(LOC);
               return Token(TOK_APL_VALUE1, Z);
             }

        case 3: // close all open windowa
             {
               Value_P Z(open_windows.size(), LOC);
               loop(w, open_windows.size())
                  Z->next_ravel_Int(open_windows[w].fd);
               close_all_windows();
               Z->check_value(LOC);
               return Token(TOK_APL_VALUE1, Z);
             }

        case 4: // set focus_on_map
             focus_on_map = true;
             return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

        case 5: // clear focus_on_map (default)
             focus_on_map = false;
             return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

        default: MORE_ERROR() << "Invalid function number Bi=" << function
                              << " in ⎕GTK Bi";
                 DOMAIN_ERROR;
      }

   MORE_ERROR() << "Unexpected B in ⎕GTK B";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_GTK::eval_XB(Value_P X, Value_P B) const
{
   CHECK_SECURITY(disable_Quad_GTK);

   // see eval_AXB above for an explanation of X and B.

   if (B->get_rank() > 1)   RANK_ERROR;

   // split X into handle ↑X and widget_id 1↓X
   //
UTF8_string widget_id;                // e.g. "entry1"
const int fd = resolve_window(X.get(), widget_id);
   write_TLV(fd, 6, widget_id);   // select widget

int fun = FNUM_INVALID;
   if (B->is_int_scalar())         fun = B->get_cfirst().get_int_value();
   else if (B->is_char_string())   fun = resolve_fun_name(widget_id, B.get());
   else                            DOMAIN_ERROR;

int command_tag = -1;
int response_tag = -1;
Gtype Atype = gtype_V;

   switch(fun)
      {
        case 0:
           write_TLV(fd, 4, widget_id);
           return Token(TOK_APL_VALUE1, IntScalar(fd, LOC));

#define gtk_event_def(ev_ename, ...)
#define gtk_fun_def(ID_prefix, gtk_class, gtk_function, _Z, A)               \
         case FNUM_ ## gtk_class ## _ ## gtk_function:                      \
              command_tag = Command_ ## gtk_class ## _ ## gtk_function;     \
              response_tag = Response_ ##gtk_class ## _ ## gtk_function;   \
              Atype = get_gtype(#A);                                        \
              break;
#include "Gtk/Gtk_map.def"
      }

   if (Atype != gtype_V)   VALENCE_ERROR;

   write_TL0(fd, command_tag);   // send command to TLV_server
   return Token(TOK_APL_VALUE1, poll_response(fd, response_tag));
}
//────────────────────────────────────────────────────────────────────────────
Quad_GTK::Fnum
Quad_GTK::resolve_fun_name(UTF8_string & widget_id, const Value * B)
{
   // By convention, widget_id is a class prefix (lowercase a-z),
   // possibly followed by an instance number (if glade is used),
   // or something else (typically -suffix or _suffix).
   // Compute the length of the class prefix.
   //
int wid_len = 0;
   while (widget_id[wid_len] >= 'a' && widget_id[wid_len] <= 'z')   ++wid_len;

const UCS_string ucs_B(*B);
UTF8_string utf_B(ucs_B);
const char * wid_class = widget_id.c_str();   // e.g. button-OK
const char * fun_name = utf_B.c_str();        // gtk_widget_get_state_flags

   /* normally the user has to specify only the function name (without the
      class prefix and we math both to obtain the FNUM_. Sometimes, for
      example when the user calls a function in a parent class of a widget,
      she may alternatively provide the GTK class. For example:

      gtk_fun_def(button, GtkButton, clicked,        V,V)
      gtk_fun_def(widget, GtkWidget, get_state_flags,I,V)

                     wid_class   fun_name
                     ---------   --------
      Normal case:   GtkButton   foo             → FNUM_GtkButton_foo
      Special case:      -       GtkWidget::foo  → FNUM_GtkWidget_foo

   */
   if (const char * fun = strstr(fun_name, "::"))   // special case
      {
        wid_class = fun_name;
        fun_name  = fun + 2;
        wid_len   = (fun - fun_name) + 2;   // e.g. GtkButton → gtk_button_
      }

#define gtk_event_def(ev_ename, ...)
#define gtk_fun_def(ID_prefix, gtk_class, gtk_function, _Z,_A)         \
   if (!(strncmp(wid_class, #ID_prefix, wid_len) ||                    \
         strcmp(fun_name,   #gtk_function)))                          \
      return FNUM_ ## gtk_class ## _ ## gtk_function;

#include "Gtk/Gtk_map.def"

   MORE_ERROR() << "⎕GTK: function string class=" << wid_class
        << ", function=" << fun_name << " could not be resolved";
   return FNUM_INVALID;
}
//────────────────────────────────────────────────────────────────────────────
int
Quad_GTK::write_TL0(int fd, int tag)
{
unsigned char TLV[8];
   memset(TLV, 0, 8);
   TLV[0] = tag >> 24;
   TLV[1] = tag >> 16;
   TLV[2] = tag >> 8;
   TLV[3] = tag;

   errno = 0;
const size_t tx_len = write(fd, TLV, 8);
    if (tx_len != 8)
       {
         CERR << "write(Tag = " << tag << ") failed in ⎕GTK::write_TL0()"
              << endl << "   tx_len = " << tx_len << endl
              << "   errno says: " << strerror(errno) << endl;
         return -1;
       }

   return 0;
}
//────────────────────────────────────────────────────────────────────────────
int
Quad_GTK::write_TLV(int fd, int tag, const UTF8_string & value)
{
const int TLV_len = 8 + value.size();
unsigned char TLV[TLV_len];
   TLV[0] = tag >> 24 & 0xFF;
   TLV[1] = tag >> 16 & 0xFF;
   TLV[2] = tag >> 8  & 0xFF;
   TLV[3] = tag       & 0xFF;
   TLV[4] = value.size() >> 24;
   TLV[5] = value.size() >> 16;
   TLV[6] = value.size() >>  8;
   TLV[7] = value.size();
   loop(s, value.size())   TLV[8 + s] = value[s];

    if (write(fd, TLV, TLV_len) != TLV_len)
       {
         CERR << "write(Tag = " << tag << ") failed in ⎕GTK::write_TLV()";
         return -1;
       }

   return 0;
}
//────────────────────────────────────────────────────────────────────────────
int
Quad_GTK::start_Gtk_server()
{
   // locate the Gtk_server. It should live in one of two places:
   //
   // 1, for an installed apl: in the same directory as apl, or
   // 2. during development in subdir Gtk of the src directory
   //
char path1[APL_PATH_MAX + 1 + 8];
char path2[APL_PATH_MAX + 1 + 8];
const char * bin_dir = LibPaths::get_APL_bin_path();
   SPRINTF(path1, "%s/Gtk/Gtk_server", bin_dir);
   SPRINTF(path2, "%s/Gtk_server", bin_dir);

const char * path = 0;
   if      (!access(path1, X_OK))   path = path1;
   else if (!access(path2, X_OK))   path = path2;
   else
      {
        MORE_ERROR() << "No Gtk_server found in " << path1
                   << "\nnor in                 " << path2;
        DOMAIN_ERROR;
      }

const char * evars[] = { "DISPLAY", "XAUTHORITY", "XAUTHLOCALHOST" };
enum { evar_count = sizeof(evars) / sizeof(*evars) };

char * envp[evar_count + 1] = { 0 };
int envp_idx = 0;
   loop(c, evar_count)
       {
         const char * var = evars[c];
         if (char * val = getenv(var))
            {
              const size_t len = strlen(var) + 1 + strlen(val) + 1;
              char * env = new char[len];
              snprintf(env, len, "%s=%s", var, val);
              env[len - 1] = 0;
              envp[envp_idx++] = env;
            }
       }
   envp[envp_idx] = 0;   // 0-terminator

const int fd = Quad_FIO::do_FIO_57(UTF8_string(path), envp);
   loop(c, evar_count)   delete [] envp[c];
   return fd;
}
//════════════════════════════════════════════════════════════════════════════
void
Quad_GTK::send_name_or_data(int fd, int tag, const UCS_string & name_or_data)
{
UTF8_string name_or_data_utf8(name_or_data);
const size_t Vlen = name_or_data_utf8.size();
const size_t TLV_len = 8 + Vlen;   // 4 nyte tag + byte 4 len + value
char * del = 0;
char short_path[1000];
char * path = short_path;

   if (TLV_len >= sizeof(short_path))
      {
        del = path = new char[TLV_len];
      }

   path[0] = tag >> 24;    // Tag: MSB
   path[1] = tag >> 16;    // Tag: ..
   path[2] = tag >> 8;     // Tag: ..
   path[3] = tag;          // Tag: LSB (1 or 2)
   path[4] = Vlen >> 24;   // Length MSB
   path[5] = Vlen >> 16;   // Length ..
   path[6] = Vlen >>  8;   // Length ..
   path[7] = Vlen;         // Length LSB
   memcpy(path + 8, name_or_data_utf8.c_str(), Vlen);
const size_t wlen = write(fd, path, TLV_len);
   if (wlen != TLV_len)
      {
         delete del;
         Quad_FIO::close_handle(fd);
         MORE_ERROR() << "write(Tag " << tag << ") failed in ⎕GTK";
         DOMAIN_ERROR;
      }

   delete del;
}
//────────────────────────────────────────────────────────────────────────────
int
Quad_GTK::open_window(const UCS_string & gui_name_or_data,   // mandatory
                      const UCS_string * css_name_or_data)   // optional
{
const int fd = start_Gtk_server();
   fcntl(fd, F_SETFD, FD_CLOEXEC | fcntl(fd, F_GETFD, 0));

   // maybe request that the new window shall grab the focus. This is normally
   // annoying, but might be useful for taking screenshots (scrot -u).
   //
   if (focus_on_map)
      {
         write_TL0(fd, 9);
      }

   // write either: TLVs 1 and 3       (no css_name_or_data)
   // or else:      TLVs 1, 2, and 3   (with css_name_or_data)
   // to the Gtk_server...
   //
   send_name_or_data(fd, 1 , gui_name_or_data);
   if (css_name_or_data)   send_name_or_data(fd, 2, *css_name_or_data);


   if (write_TL0(fd, 3))
      {
         Quad_FIO::close_handle(fd);
         MORE_ERROR() << "write(Tag 3) failed in ⎕GTK";
         DOMAIN_ERROR;
      }

window_entry we = { fd };
   open_windows.push_back(we);
   return fd;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_GTK::close_window(int fd)
{
   loop(w, open_windows.size())
      {
        window_entry & we = open_windows[w];
        if (fd == we.fd)
           {
             we = open_windows.back();
             open_windows.pop_back();
             if (write_TL0(fd, 5))
                {
                  CERR << "write(close Tag) failed in ⎕GTK::close_window()";
                }
             const int err = Quad_FIO::close_handle(fd);
             return IntScalar(err, LOC);
           }
      }

   MORE_ERROR() << "Invalid ⎕GTK handle " << fd;
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_GTK::poll_all()
{
const int count = open_windows.size();
pollfd fds[count];
   loop(w, count)
       {
         fds[w].fd = open_windows[w].fd;
#ifdef POLLRDHUP
         fds[w].events = POLLIN | POLLRDHUP;
#else
         fds[w].events = POLLIN;
#endif
         fds[w].revents = 0;
       }

   errno = 0;
const int ready = poll(fds, count, 0);
   if (ready == 0)   return;
   if (ready > 0)
      {
        loop(w, count)
            {
              if (fds[w].revents)   read_fd(fds[w].fd, -1);
            }
      }
   else
      {
        if (errno == EINTR)   return;   // user hits ^C
        CERR << "*** poll() failed: " << strerror(errno) << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_GTK::poll_response(int fd, int tag)
{
again:

pollfd pfd;
   pfd.fd = fd;
#ifdef POLLRDHUP
   pfd.events = POLLIN | POLLRDHUP;
#else
   pfd.events = POLLIN;
#endif
   pfd.revents = 0;

const int ready = poll(&pfd, 1, 0);
   if (ready == 0)
      {
        usleep(100000);
        goto again;
      }

   if (ready > 0 && pfd.revents)
      {
        Value_P Z = read_fd(fd, tag);
        if (!Z)   goto again;   // unexpected
        return Z;
      }

   CERR << "*** poll() failed" << endl;
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_GTK::window_list()
{
Value_P Z(open_windows.size(), LOC);
   loop(w, open_windows.size())
      {
        const APL_Integer fd = open_windows[w].fd;
        Z->next_ravel_Int(fd);
      }

   Z->set_proto_Int();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
int
Quad_GTK::resolve_window(const Value * X, UTF8_string & widget_id)
{
   if (X->get_rank() > 1)   RANK_ERROR;
const int fd = X->get_cfirst().get_int_value();

   // verify that the handle ↑X is an open window...
   //
bool window_valid = false;
   loop(w, open_windows.size())
       {
         if (fd == open_windows[w].fd)
            {
              window_valid = true;
              break;
            }
       }

   if (!window_valid)
      {
         MORE_ERROR() << "Invalid GTK handle " << fd
                      << " in Quad_GTK::resolve_window()";
         DOMAIN_ERROR;
      }

   // copy string 1↓X into widget_id
   loop(i, X->element_count())
       {
         if (i)   widget_id += X->get_cravel(i).get_char_value();
       }

   return fd;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_GTK::read_fd(int fd, int expected_tag)
{
   // tag == -1 indicates a poll for ANY tag (= an event as opposed to a
   // command response). In that case the poll is non-blocking.

char TLV[1000];

   errno = 0;
const ssize_t rx_len = read(fd, TLV, sizeof(TLV));

   if (rx_len < 8)
      {
        MORE_ERROR() << "read() failed in Quad_GTK::read_fd(): "
                     << strerror(errno);
        DOMAIN_ERROR;
      }

const int TLV_tag = (TLV[0] & 0xFF) << 24 | (TLV[1] & 0xFF) << 16
                  | (TLV[2] & 0xFF) <<  8 | (TLV[3] & 0xFF);


const uint32_t V_len = (TLV[4] & 0xFF) << 24 | (TLV[5] & 0xFF) << 16
                     | (TLV[6] & 0xFF) <<  8 | (TLV[7] & 0xFF);

   Assert(rx_len == ssize_t(V_len + 8));

char * V = TLV + 8;
   V[V_len] = 0;   // avoid trouble

   /* 1. check for events from generic_callback() or other callbacks
         in Gtk_serverc.cc. The TLV_tags are:

      3001: a widget without an ID in XML,
      3002: a widget with ID and class in XML,
      3003: the top-level windows was closed, or
      3004: a row_selected in a textview
    */
   if (is_event_class(TLV_tag))
      {
        // V is a string of the form H%s:%s:%s:% where H is a placeholder
        // for the fd over which we have received V (which Gtk_server cannot
        // know).
        //
        // replace placeholder H with the actual fd and store the result
        // in event_queue.
        //
        UTF8_string data_utf;   // H:widget:callback
        loop(v, V_len)   data_utf += V[v];
        UCS_string data_ucs(data_utf);
        data_ucs[0] = Unicode(fd);

        event_queue.push_back(data_ucs);

        return Value_P();   // i.e. NULL
      }

   if (expected_tag == -1)   // not waiting for a specific tag (function result)
      {
        // unexpected, so we complain unconditionally.
        CERR << "*** ⎕GTK: ignoring event: " << V << endl;
        return Value_P();   // i.e. NULL
      }

   if (V_len == 0)   // result of void function()
      {
        return Idx0(LOC);   // fo not: Value_P()
      }

   if (TLV_tag != expected_tag)
      {
        // unexpected, so we complain unconditionally.
        CERR << "Got unexpected tag " << TLV_tag
             << " when waiting for response " << expected_tag << endl;
        return Value_P();
      }

   // TLV_tag is the expected one. Strip off the response offset
   // Response_0 (= 2000) from the TLV_tag to get the function number
   // and return APL vector function,"result-value"
   //
const UTF8 Z_type = V[0];   // s, d, or f (like conversion char in snprintf())
   if (Z_type == 'd')   // integer
      {
        Value_P Z(2, LOC);
        Z->next_ravel_Int(TLV_tag - Response_0);   // function that sent V
        Z->next_ravel_Int(strtoll(V + 1, 0, 10));   // function result
        Z->check_value(LOC);
        return  Z;
      }

   if (Z_type == 'f')   // double
      {
        Value_P Z(2, LOC);
        Z->next_ravel_Int(TLV_tag - Response_0);   // function that sent V
        Z->next_ravel_Float(strtod(V + 1, 0));     // function result
        Z->check_value(LOC);
        return  Z;
      }

   if (Z_type == 's')   // string
      {
        const UTF8_string utf(V + 1);
        const UCS_string ucs(utf);

        Value_P Z(1 + ucs.size(), LOC);
        Z->next_ravel_Int(TLV_tag - Response_0);
        loop(u, ucs.size())   Z->next_ravel_Char(ucs[u]);
        Z->check_value(LOC);
        return  Z;
      }

  FIXME;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_GTK::fnum_to_function_name(Fnum fnum)
{
const char * fname = 0;
   switch(fnum)
      {
        case FNUM_INVALID:         break;
        case FNUM_0:               break;
#define gtk_event_def(ev_ename, ...)
#define gtk_fun_def(_ID_prefix, gtk_class, gtk_function, _Z,_A)   \
        case FNUM_ ## gtk_class ## _ ## gtk_function:             \
           fname = #gtk_function;   break;
#include "Gtk/Gtk_map.def"
      }

   if (fname)   // found
      {
        const UTF8_string fname_utf(fname);
        const UCS_string fname_ucs(fname_utf);

        Value_P Z(fname_ucs, LOC);
        return Z;
      }

   MORE_ERROR() << "A ⎕GTK 5: Invalid function number A=" << fnum;
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_GTK::fnum_to_widget_class(Fnum fnum)
{
const char * cname = 0;
   switch(fnum)
      {
        case FNUM_INVALID:         break;
        case FNUM_0:               break;
#define gtk_event_def(ev_ename, ...)
#define gtk_fun_def(_ID_prefix, gtk_class, gtk_function, _Z,_A)   \
        case FNUM_ ## gtk_class ## _ ## gtk_function:             \
           cname = #gtk_class;   break;
#include "Gtk/Gtk_map.def"
      }

   if (cname)   // found
      {
        const UTF8_string cname_utf(cname);
        const UCS_string cname_ucs(cname_utf);

        Value_P Z(cname_ucs, LOC);
        return Z;
      }

   MORE_ERROR() << "A ⎕GTK 6: Invalid function number A=" << fnum;
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────

#else   // ! apl_GTK3 or ! apl_X11

//────────────────────────────────────────────────────────────────────────────
void
Quad_GTK::close_all_windows()
{
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_GTK::eval_AB(Value_P A, Value_P B) const
{
   return eval_B(B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_GTK::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
   return eval_B(B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_GTK::eval_B(Value_P B) const
{
const char * libs[] = { "libgtk-3.so",  0 };
const char * hdrs[] = { "gtk/gtk.h",    0 };
const char * pkgs[] = { "libgtk-3-dev", 0 };

   return missing_files("⎕GTK", libs, hdrs, pkgs);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_GTK::eval_XB(Value_P X, Value_P B) const
{
   return eval_B(B);
}
//════════════════════════════════════════════════════════════════════════════
#endif   // apl_GTK3

