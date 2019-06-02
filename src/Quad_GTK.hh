/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2017  Dr. Jürgen Sauermann

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

#ifndef __Quad_GTK_DEFINED__
#define __Quad_GTK_DEFINED__

#include <math.h>
#include <vector>

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

   static Quad_GTK * fun;          ///< Built-in function.
   static Quad_GTK  _fun;          ///< Built-in function.

   /// close all open windows
   void clear();

protected:
   /// the type of a function parameter or return value
   enum Gtype
      {
        gtype_V = 0,   ///< void
        gtype_S = 1,   ///< string
        gtype_I = 2,   ///< integer
        gtype_F = 3,   ///< float
      };

#include "Gtk/Gtk_enums.hh"

   /// the context for a GTL window
   struct window_entry
      {
        int fd;   ///< pipe to the windw process
      };

   /// overloaded Function::eval_AB()
   Token eval_AB(Value_P A, Value_P B);

   /// overloaded Function::eval_AXB()
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B);

   /// overloaded Function::eval_B()
   Token eval_B(Value_P B);

   /// overloaded Function::eval_XB()
   virtual Token eval_XB(Value_P X, Value_P B);

   /// X is supposed to be something like 4,"win_id". Store win_id in window_id
   /// and return the window number (4 in this  example)
   int resolve_window(const Value * X, UTF8_string & window_id);

   /// B is a function name (-suffix). 
   Fnum resolve_fun_name(UTF8_string & window_id, const Value * B);

   /// write a TLV with an empty V (thus L=0)
   int write_TL0(int fd, int tag);

   /// write a TLV
   int write_TLV(int fd, int tag, const UTF8_string & value);

   /// open the GTH window described by gui_filename and optional css_filename
   int open_window(const UCS_string & gui_filename,
                   const UCS_string * css_filename);

   /// return the handles of currently open windows
   Value_P window_list() const;

   /// close the window with file descriptor fd
   Value_P close_window(int fd);

   /// poll all fds and insert events into \b event_queue until no more
   /// events are pending
   void poll_all();

   /// poll for a TLV on fd with a specific (reponse-) tag
   Value_P poll_response(int fd, int tag);

   /** read a TLV with a given tag (or any TLV if tag == -1) on a fd
       that is ready for reading. If the TLV is an event (i.e. unexpected)
       then insert it into event_queue and return 0; otherwise the TLV
       is a response that is returned.
    **/
   Value_P read_fd(int fd, int tag);

   /// the currently open GTK windows
   std::vector<window_entry> open_windows;

   /// event queue for events from the GTK windows
   UCS_string_vector event_queue;
};

#endif // __Quad_GTK_DEFINED__
