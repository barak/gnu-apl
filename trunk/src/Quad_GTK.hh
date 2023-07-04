/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2023  Dr. Jürgen Sauermann

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

#ifndef __Quad_GTK_DEFINED__
#define __Quad_GTK_DEFINED__

#include <math.h>

#include "QuadFunction.hh"
#include "Value.hh"
#include "UCS_string_vector.hh"

/// The class implementing ⎕GTK
class Quad_GTK : public QuadFunction
{
public:
   /// Constructor.
   Quad_GTK()
      : QuadFunction(TOK_Quad_GTK)
   {}

   static Quad_GTK  fun;          ///< Built-in function.

   /// close all open windows
   static void close_all_windows();

protected:
   /// the type of a function parameter or return value
   enum Gtype
      {
        gtype_V = 0,   ///< void
        gtype_S = 1,   ///< string
        gtype_I = 2,   ///< integer
        gtype_F = 3,   ///< float
      };

   /// true iff tag is a Gtk command without response (as opposed to
   /// response tags or event classes
   static inline bool is_command_without_response(int tag)
      { return tag > 0 && tag < Command_0; }

   /// true iff tag is a Gtk command with response (as opposed to
   /// response tags or event classes
   static inline bool is_command_with_response(int tag)
      { return tag > Command_0 && tag < Command_max; }

   /// true iff tag is a Gtk response (as opposed to command tags or
   /// event class
   static inline bool is_response(int tag)
      { return tag > Response_0 && tag < Response_max; }

   /// true iff tag is a Gtk event class (as opposed to command tags or
   /// response tags
   static inline bool is_event_class(int tag)
      { return tag > Event_0 && tag < Event_max; }

   /// convert 1-character generic type names (V, I, S, or F) or longer
   /// enum types to the corresponding Gtype.
   static Gtype get_gtype(const char * str)
      {
        if (str[1])        return gtype_I;   // enum -> int
        if (*str == 'V')   return gtype_V;
        if (*str == 'I')   return gtype_I;
        if (*str == 'S')   return gtype_S;
        if (*str == 'F')   return gtype_F;
        FIXME;
      }

#include "Gtk/Gtk_enums.hh"

   /// the context for a GTL window
   struct window_entry
      {
        int fd;   ///< pipe to the windw process
      };

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_AXB()
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_XB()
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// X is supposed to be something like 4,"win_id". Store win_id in window_id
   /// and return the window number (4 in this  example)
   static int resolve_window(const Value * X, UTF8_string & window_id);

   /// B is a function name (-suffix). 
   static Fnum resolve_fun_name(UTF8_string & window_id, const Value * B);

   /// write a TLV with an empty V (thus L=0)
   static int write_TL0(int fd, int tag);

   /// write a TLV
   static int write_TLV(int fd, int tag, const UTF8_string & value);

   /// start a Gtk_server, return its file desriptor
   static int start_Gtk_server();

   /// send file or filename to the Gtk_server
   static void send_name_or_data(int fd, int tag,
                                 const UCS_string & name_or_data);

   /// open the GTH window described by gui_filename and optional css_filename
   static int open_window(const UCS_string & gui_name_or_data,   // mandatory
                   const UCS_string * css_name_or_data);         // optional

   /// return the handles of currently open windows
   static Value_P window_list();

   /// close the window with file descriptor fd
   static Value_P close_window(int fd);

   /// poll all fds and insert events into \b event_queue until no more
   /// events are pending
   static void poll_all();

   /// poll for a TLV on fd with a specific (reponse-) tag
   static Value_P poll_response(int fd, int tag);

   /** read a TLV with a given tag (or any TLV if tag == -1) on a fd
       that is ready for reading. If the TLV is an event (i.e. unexpected)
       then insert it into event_queue and return 0; otherwise the TLV
       is a response that is returned.
    **/
   static Value_P read_fd(int fd, int tag);

   /// return the function name for function number fnum.
   static Value_P fnum_to_function_name(Fnum fnum);

   /// return the class name for function number fnum.
   static Value_P fnum_to_widget_class(Fnum fnum);

   /// the currently open GTK windows
   static std::vector<window_entry> open_windows;

   /// event queue for events from the GTK windows
   static UCS_string_vector event_queue;

   /// whether the next new window shall grab the focus
   static bool focus_on_map;
};

#endif // __Quad_GTK_DEFINED__
