/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2018  Dr. Jürgen Sauermann

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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>

#include "../config.h"
#include "LibPaths.hh"
#include "PointerCell.hh"
#include "Quad_GTK.hh"
#include "Workspace.hh"

Quad_GTK  Quad_GTK::_fun;
Quad_GTK * Quad_GTK::fun = &Quad_GTK::_fun;

#if HAVE_GTK3
//-----------------------------------------------------------------------------
Token
Quad_GTK::eval_AB(Value_P A, Value_P B)
{
   if (A->get_rank() > 1)   RANK_ERROR;
   if (B->get_rank() > 1)   RANK_ERROR;
   if (A->is_char_array() && B->is_char_array())
      {
         const UCS_string css_filename = A->get_UCS_ravel();
         const UCS_string gui_filename = B->get_UCS_ravel();
         const int fd = open_window(gui_filename, &css_filename);
         return Token(TOK_APL_VALUE1, IntScalar(fd, LOC));
      }

   if (!B->is_int_scalar())
      {
        MORE_ERROR() << "A ⎕GTK B expects B to be an integer scalar"
                        " or a text vector";
        DOMAIN_ERROR;
      }

const int function = B->get_ravel(0).get_int_value();
int fd = -1;
   switch(function)
      {
        case 0: // close window/GUI
             if (!A->is_int_scalar())   goto bad_fd;
             fd = A->get_ravel(0).get_int_value();
             return Token(TOK_APL_VALUE1, close_window(fd));

        case 3: // increase verbosity
             if (!A->is_int_scalar())   goto bad_fd;
             if (write_TL0(fd, 7))
                {
                  CERR << "write( Tag 7) failed in Ah ⎕GTK 3";
                  return Token(TOK_APL_VALUE1, IntScalar(-3, LOC));
                }
             return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

        case 4: // decrease verbosity
             if (!A->is_int_scalar())   goto bad_fd;
             if (write_TL0(fd, 8))
                {
                  CERR << "write( Tag 8) failed in Ah ⎕GTK 3";
                  return Token(TOK_APL_VALUE1, IntScalar(-4, LOC));
                }
             return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

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
//-----------------------------------------------------------------------------
Token
Quad_GTK::eval_B(Value_P B)
{
   if (B->get_rank() > 1)   RANK_ERROR;
   if (B->is_char_array())
      {
         const UCS_string gui_filename = B->get_UCS_ravel();
         const int fd = open_window(gui_filename, /* no CSS */ 0);
         return Token(TOK_APL_VALUE1, IntScalar(fd, LOC));
      }

   if (!B->is_int_scalar())
      {
        MORE_ERROR() << "⎕GTK B expects B to be an integer scalar"
                        " or a text vector";
        DOMAIN_ERROR;
      }

const int function = B->get_ravel(0).get_int_value();
   switch(function)
      {
        case 0:   // list of open fds
             return Token(TOK_APL_VALUE1, window_list());

        case 1: // blocking poll for next event
        case 2: // non-blocking wait for next event
             poll_all();
             if (function == 2 && event_queue.size() == 0)   // non-blocking
                return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

             // blocking
             //
             while (event_queue.size() == 0 && !interrupt_is_raised())
                   poll_all();

             // at this point either event_queue.size() > 0 or an interrupt
             // was raised
             //
             if (event_queue.size() == 0)   // hence interrupt was raised
                {
                   clear_interrupt_raised(LOC);
                   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));
                }

             {
               UCS_string HWF = event_queue[0];   // fd, widget : fun
               event_queue.erase(event_queue.begin());

               // split (Unicode)Handle,"widget:function"
               // into a 3-element APL vector Handle (⊂"widget") (⊂"function")
               //
               UCS_string_vector args;
               UCS_string arg;
               for (ShapeItem j = 1; j < HWF.size(); ++j)
                   {
                     if (HWF[j] == UNI_ASCII_COLON)
                        {
                          args.push_back(arg);
                          arg.clear();
                        }
                     else
                        {
                          arg.append(HWF[j]);
                        }
                   }
               args.push_back(arg);

               Value_P Z(1 + args.size(), LOC);
               new (Z->next_ravel()) IntCell(HWF[0]);
               loop(a, args.size())
                   {
                     Value_P Za(args[a], LOC);
                     new (Z->next_ravel())   PointerCell(Za.get(), Z.getref());
                   }
               Z->check_value(LOC);
               return Token(TOK_APL_VALUE1, Z);
             }

        default: MORE_ERROR() << "Invalid function number Bi=" << function
                              << " in ⎕GTK Bi";
                 DOMAIN_ERROR;
      }

   MORE_ERROR() << "Unexpected B in ⎕GTK B";
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
Token
Quad_GTK::eval_AXB(Value_P A, Value_P X, Value_P B)
{
   if (B->get_rank() > 1)   RANK_ERROR;

UTF8_string window_id;                // e.g. "entry1"
const int fd = resolve_window(X.get(), window_id);
   write_TLV(fd, 6, window_id);   // select wodget

int fun = FNUM_INVALID;
   if (B->is_int_scalar())         fun = B->get_ravel(0).get_int_value();
   else if (B->is_char_string())   fun = resolve_fun_name(window_id, B.get());
   else
      {
        MORE_ERROR() << "A ⎕GTK[X] B expects B to be an integer scalar"
                        " or a text vector";
        DOMAIN_ERROR;
      }

int command_tag = -1;
int response_tag = -1;
Gtype Atype = gtype_V;

   switch(fun)
      {
         case FNUM_INVALID: DOMAIN_ERROR;
         case FNUM_0:   return Token(TOK_APL_VALUE1, IntScalar(fd, LOC));

#define gtk_event_def(ev_ename, ...)
#define gtk_fun_def(glade_ID, gtk_class, gtk_function, _ZAname,_Z, A,_help) \
         case FNUM_ ## gtk_class ## _ ## gtk_function:                      \
              command_tag = Command_ ## gtk_class ## _ ## gtk_function;     \
              response_tag = Response_ ## gtk_class ## _ ## gtk_function;   \
              Atype = gtype_ ## A;                                          \
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
              const Cell & cell = A->get_ravel(a);
              if (!cell.is_pointer_cell())
                 {
                    MORE_ERROR() << "A ⎕GTK " << fun
                                 << " expects A to be a vector of "
                                    "draw commands (strings)";
                    DOMAIN_ERROR;
                 }
              Value_P command = cell.get_pointer_value();
              ucs_A.append(UCS_string(*command));
              ucs_A.append(UNI_ASCII_LF);
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
//-----------------------------------------------------------------------------
Token
Quad_GTK::eval_XB(Value_P X, Value_P B)
{
   if (B->get_rank() > 1)   RANK_ERROR;

UTF8_string window_id;                // e.g. "entry1"
const int fd = resolve_window(X.get(), window_id);
   write_TLV(fd, 6, window_id);   // select wodget

int fun = FNUM_INVALID;
   if (B->is_int_scalar())         fun = B->get_ravel(0).get_int_value();
   else if (B->is_char_string())   fun = resolve_fun_name(window_id, B.get());
   else                            DOMAIN_ERROR;

int command_tag = -1;
int response_tag = -1;
Gtype Atype = gtype_V;

   switch(fun)
      {
        case 0:
           write_TLV(fd, 4, window_id);
           return Token(TOK_APL_VALUE1, IntScalar(fd, LOC));

#define gtk_event_def(ev_ename, ...)
#define gtk_fun_def(glade_ID, gtk_class, gtk_function, _ZAname, _Z, A, _help) \
         case FNUM_ ## gtk_class ## _ ## gtk_function:                        \
              command_tag = Command_ ## gtk_class ## _ ## gtk_function;       \
              response_tag = Response_ ## gtk_class ## _ ## gtk_function;     \
              Atype = gtype_ ## A;                                            \
              break;
#include "Gtk/Gtk_map.def"
      }

   if (Atype != gtype_V)   VALENCE_ERROR;

   write_TL0(fd, command_tag);   // command
   return Token(TOK_APL_VALUE1, poll_response(fd, response_tag));
}
//-----------------------------------------------------------------------------
Value_P
Quad_GTK::read_fd(int fd, int tag)
{
   // tag == -1 indicates a poll for ANY tag (= an event as opposed to a
   // command response). In that case the poll is non-blocking.

char TLV[1000];

const ssize_t rx_len = read(fd, TLV, sizeof(TLV));

   if (rx_len < 8)
      {
        MORE_ERROR() << "read() failed in Quad_GTK::read_fd(): "
                     << strerror(errno);
        DOMAIN_ERROR;
      }

const int TLV_tag = (TLV[0] & 0xFF) << 24 | (TLV[1] & 0xFF) << 16
                  | (TLV[2] & 0xFF) <<  8 | (TLV[3] & 0xFF);


const int V_len = (TLV[4] & 0xFF) << 24 | (TLV[5] & 0xFF) << 16
                | (TLV[6] & 0xFF) <<  8 | (TLV[7] & 0xFF);

   Assert(rx_len == (V_len + 8));

char * V = TLV + 8;
   V[V_len] = 0;

   // 1. check for events from generic_callback()
   //
   if (TLV_tag == Event_widget_fun ||            // "H:button1:clicked"
       TLV_tag == Event_widget_fun_id_class ||   // dito + :id:class
       TLV_tag == Event_toplevel_window_done)
      {
        // V is a string of the form H%s:%s:%s:% where H is a placeholder
        // for the fd over which we have received V.
        //
        // replace H by the actual fd and store the result in event_queue.
        //
        UTF8_string data_utf;   // H:widget:callback
        loop(v, V_len)   data_utf += V[v];
        UCS_string data_ucs(data_utf);
        data_ucs[0] = (Unicode(fd));

        event_queue.push_back(data_ucs);

        return Value_P();   // i.e. NULL
      }

   if (tag == -1)   // not waiting for a specific tag
      {
        CERR << "*** ⎕GTK: ignoring event: " << V << endl;
        return Value_P();   // i.e. NULL
      }

   if (TLV_tag != tag)
      {
        CERR << "Got unexpected tag " << TLV_tag
             << " when waiting for response " << tag << endl;
        return Value_P();
      }

   // TLV_tag is the expected one. Strip off the response offset
   // Response_0 (= 2000) from the TLV_tag to get the function number
   // and return APL vector function,"result-value"
   //
UTF8_string utf;
   loop(u, V_len)   utf += V[u];
   UCS_string ucs(utf);

Value_P Z(1 + ucs.size(), LOC);
   new (Z->next_ravel())   IntCell(TLV_tag - Response_0);
   loop(u, ucs.size())   new (Z->next_ravel())   CharCell(ucs[u]);
   Z->check_value(LOC);
   return  Z;
}
//-----------------------------------------------------------------------------
void
Quad_GTK::poll_all()
{
const int count = open_windows.size();
pollfd fds[count];
   loop(w, count)
       {
         fds[w].fd = open_windows[w].fd;
         fds[w].events = POLLIN | POLLRDHUP;
         fds[w].revents = 0;
       }

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
//-----------------------------------------------------------------------------
Value_P
Quad_GTK::poll_response(int fd, int tag)
{
again:

pollfd pfd;
   pfd.fd = fd;
   pfd.events = POLLIN | POLLRDHUP;
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
        if (!Z)   goto again;
        return Z;
      }

   CERR << "*** poll() failed" << endl;
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
Value_P
Quad_GTK::window_list() const
{
Value_P Z(open_windows.size(), LOC);
   loop(w, open_windows.size())
      {
        const APL_Integer fd = open_windows[w].fd;
        new (Z->next_ravel()) IntCell(fd);
      }

   Z->set_default_Int();
   Z->check_value(LOC);
   return Z;
}
//-----------------------------------------------------------------------------
int
Quad_GTK::resolve_window(const Value * X, UTF8_string & window_id)
{
   if (X->get_rank() > 1)   RANK_ERROR;
const int fd = X->get_ravel(0).get_int_value();

   // verify that ↑X is an open window...
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
         MORE_ERROR() << "Invalid window " << fd
                      << " in Quad_GTK::resolve_window()";
         DOMAIN_ERROR;
      }

   loop(i, X->element_count())
       {
         if (i)   window_id += X->get_ravel(i).get_char_value();
       }

   return fd;
}
//-----------------------------------------------------------------------------
Quad_GTK::Fnum
Quad_GTK::resolve_fun_name(UTF8_string & window_id, const Value * B)
{
   // window_id is a class and an instance number, such as entry1
   // Determine the length of the class prefixx
   //
int wid_len = window_id.size();
   while (wid_len &&
          window_id[wid_len - 1] >= '0' &&
          window_id[wid_len - 1] <= '9')   --wid_len;

const UCS_string ucs_B(*B);
UTF8_string utf_B(ucs_B);
const char * wid_class = window_id.c_str();
const char * fun_name = utf_B.c_str();

#define gtk_event_def(ev_ename, ...)
#define gtk_fun_def(glade_ID, gtk_class, gtk_function, _ZAname,_Z,_A,_help) \
   if (!(strncmp(wid_class, #glade_ID, wid_len) ||                          \
         strcmp(fun_name,   #gtk_function)))                                 \
      return FNUM_ ## gtk_class ## _ ## gtk_function;

#include "Gtk/Gtk_map.def"

   MORE_ERROR() << "function string class=" << wid_class
        << ", function=" << fun_name << " could not be resolved";
   return FNUM_INVALID;
}
//-----------------------------------------------------------------------------
void
Quad_GTK::clear()
{
   loop(w, open_windows.size())   close_window(open_windows[w].fd);
}
//-----------------------------------------------------------------------------
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
             const int err = Quad_FIO::fun->close_handle(fd);
             return IntScalar(err, LOC);
           }
      }

   MORE_ERROR() << "Invalid ⎕GTK handle " << fd;
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
int
Quad_GTK::write_TL0(int fd, int tag)
{
unsigned char TLV[8];
   memset(TLV, 0, 8);
   TLV[0] = tag >> 24;
   TLV[1] = tag >> 16;
   TLV[2] = tag >> 8;
   TLV[3] = tag;

    if (write(fd, TLV, 8) != 8)
       {
         CERR << "write(Tag = " << tag << ") failed in ⎕GTK::write_TL0()";
         return -1;
       }

   return 0;
}
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
int
Quad_GTK::open_window(const UCS_string & gui_filename,
                   const UCS_string * css_filename)
{
   // locate the Gtk_server. It should  live in one of two places:
   //
   // 1, for an installed apl: in the same directory as apl, or
   // 2. during development in subdir Gtk of the src directory
   //
char path1[APL_PATH_MAX + 1 + 8];
char path2[APL_PATH_MAX + 1 + 8];
const char * bin_dir = LibPaths::get_APL_bin_path();
int slen = snprintf(path1, APL_PATH_MAX, "%s/Gtk/Gtk_server", bin_dir);
   if (slen >= APL_PATH_MAX)   path1[APL_PATH_MAX] = 0;
   slen = snprintf(path2, APL_PATH_MAX, "%s/Gtk_server", bin_dir);
   if (slen >= APL_PATH_MAX)   path2[APL_PATH_MAX] = 0;

const char * path = 0;
   if (!access(path1, X_OK))        path = path1;
   else if (!access(path2, X_OK))   path = path2;
   else
      {
        MORE_ERROR() << "No Gtk_server found in " << path1
                     << "\nor " << path2;
        DOMAIN_ERROR;
      }

const int fd = Quad_FIO::fun->do_FIO_57(path);

   // write TLVs 1 and 3 or 1, 2, and 3 to Gtk_server...
   //
UTF8_string gui_utf8(gui_filename);
   slen = snprintf(path1 + 8, APL_PATH_MAX, "%s", gui_utf8.c_str());
   if (slen >= APL_PATH_MAX)   path1[APL_PATH_MAX] = 0;
   memset(path1, 0, 8);
   path1[3] = 1;   // tag = 1: gui filename
   path1[6] = gui_utf8.size() >> 8;
   path1[7] = gui_utf8.size() & 0xFF;
   slen = write(fd, path1, 8 + gui_utf8.size());
   if (slen == -1)
      {
         Quad_FIO::fun->close_handle(fd);
         MORE_ERROR() << "write(Tag 1) failed in ⎕GTK";
         DOMAIN_ERROR;
      }

   if (css_filename)
      {
        UTF8_string css_utf8(*css_filename);
        memset(path2, 0, 8);
        slen = snprintf(path2 + 8, APL_PATH_MAX, "%s", css_utf8.c_str());
        path2[3] = 2;   // tag = 2: css filename
        path2[6] = css_utf8.size() >> 8;
        path2[7] = css_utf8.size() & 0xFF;
        slen = write(fd, path2, 8 + css_utf8.size());
        if (slen == -1)
           {
              Quad_FIO::fun->close_handle(fd);
              MORE_ERROR() << "write(Tag 2) failed in ⎕GTK";
              DOMAIN_ERROR;
           }
      }

   if (write_TL0(fd, 3))
      {
         Quad_FIO::fun->close_handle(fd);
         MORE_ERROR() << "write(Tag 3) failed in ⎕GTK";
         DOMAIN_ERROR;
      }

window_entry we = { fd };
   open_windows.push_back(we);
   return fd;
}
//-----------------------------------------------------------------------------

#else // not HAVE_GTK3

//-----------------------------------------------------------------------------
Token
Quad_GTK::eval_AB(Value_P A, Value_P B)
{
   MORE_ERROR() << "libgtk+ version 3 was not found at ./configure time";
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
Token
Quad_GTK::eval_B(Value_P B)
{
   MORE_ERROR() << "libgtk+ version 3 was not found at ./configure time";
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
Token
Quad_GTK::eval_AXB(Value_P A, Value_P X, Value_P B)
{
   MORE_ERROR() << "libgtk+ version 3 was not found at ./configure time";
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
Token
Quad_GTK::eval_XB(Value_P X, Value_P B)
{
   MORE_ERROR() << "libgtk+ version 3 was not found at ./configure time";
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
void
Quad_GTK::clear()
{
}
//-----------------------------------------------------------------------------
#endif   // HAVE_GTK3


