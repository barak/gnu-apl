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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <unistd.h>

#include <iostream>
#include <iomanip>

#include "Gtk_enums.hh"

using namespace std;

int verbosity = 2;

//-----------------------------------------------------------------------------
static GtkBuilder * builder = 0;
static GObject * window     = 0;
static GObject * selected   = 0;

// a mapping of glade IDs and Gtl objects
static struct _ID_DB
{
  _ID_DB(const char * cls, const char * id, const char * wn, _ID_DB * nxt)
  : next(nxt),
    xml_class(strdup(cls)),
    xml_id(strdup(id)),
    widget_name(strdup(wn)),
    obj(0)
    {}

  /// the next _ID_DB entry
  _ID_DB * next;

  /// class in the XML .ui file, e.g. "GtkButton"
  const char * xml_class;

  /// id in the XML .ui file, e.g. "button1"
  const char * xml_id;

   /// widget name property in the XML .ui file, e.g. "OK-button"
  const char * widget_name;

   /// the Gtk object
   GObject * obj;
} * id_db = 0;

//-----------------------------------------------------------------------------
static bool
init_id_db(const char * filename)
{
FILE * f = fopen(filename, "r");
   if (f == 0)   return true;   // error

char class_buf[100]       = { 0 };
char id_buf[100]          = { 0 };
char widget_name_buf[100] = { 0 };
char line[200];

   for (;;)
       {
         const char * s = fgets(line, sizeof(line) - 1, f);
         line[sizeof(line) - 1] = 0;
         if (s == 0)   break;

         if (2 == sscanf(line, " <object class=\"%s id=\"%s>",
                                         class_buf, id_buf))
            {
              if (char * qu = strchr(class_buf, '"'))   *qu = 0;
              if (char * qu = strchr(id_buf, '"'))      *qu = 0;

              verbosity > 1 && cerr << "See class='" << class_buf <<
                 "' and id='" << id_buf << "'" << endl;
            }
         else if (1 == sscanf(line, " <property name=\"name\">%s",
                                                widget_name_buf))
            {
              if (char * ob = strchr(widget_name_buf, '<'))   *ob = 0;

              verbosity > 1 && cerr << "See widget name='"
                                    << widget_name_buf << endl;
            }
         else if (strstr(line, "</object"))
            {
              verbosity > 1 && cerr << "End of object class="
                                    << class_buf << " id="
                                    << id_buf << " widget-name="
                                    << widget_name_buf << endl;

              if (*class_buf || *id_buf || *widget_name_buf )
                 {
                   _ID_DB * new_db = new _ID_DB(class_buf, id_buf,
                                                widget_name_buf, id_db);
                    id_db = new_db;
                    verbosity > 1 && cerr << endl;
                 }

              *class_buf = 0;
              *id_buf = 0;
              *widget_name_buf = 0;
            }
         else if (strstr(line, " <property name=\"name\""))   // name= property
            {
              verbosity > 1 && cerr << "Existing name= property" << endl;
              continue;   // discard
            }
       }

   fclose(f);
   return false;   // OK
}
//-----------------------------------------------------------------------------
static void
cmd_1_load_GUI(const char * filename)
{
   verbosity > 0 && cerr << "Loading GUI: " << filename << endl;

   if (init_id_db(filename))
      {
        cerr << "*** reading " << filename << " failed: "
          << strerror(errno) << endl;
      }

   builder = gtk_builder_new_from_file(filename);
   assert(builder);

   // insert objects into id_db...
   //
   for (_ID_DB * entry = id_db; entry; entry = entry->next)
       {
          GObject * obj = gtk_builder_get_object(builder, id_db->xml_id);
          if (obj)
             {
               cerr << "object '" << entry->xml_id << "' mapped" << endl;
               id_db->obj = obj;
             }
           else cerr << "object '" << entry->xml_id << "' not found" << endl;
       }


   gtk_builder_connect_signals(builder, NULL);
   cerr << "GUI signals connected.\n";

   if (verbosity > 2)
      {
        for (GSList * objects = gtk_builder_get_objects(builder);
             objects; objects = objects->next)
            {
               GObject * obj = reinterpret_cast<GObject *>(objects->data);
               if (obj)
                  {
                    char * name = 0;
                    g_object_get (obj, "name\0", &name, NULL);
                    name && cerr << "NAME=" << name << endl;
                  }
            }
      }

}
//-----------------------------------------------------------------------------
static void
cmd_2_load_CSS(const char * filename)
{
   verbosity > 0 && cerr << "Loading CSS: " << filename << endl;

GtkCssProvider * css_provider = gtk_css_provider_get_default();
   assert(css_provider);

GError * err = 0;
   gtk_css_provider_load_from_path(css_provider, filename, &err);
   if (err)
      {
        cerr << "error " << err->code << " when parsing stylesheet file '"
             << filename << "':\n    " << err->message << endl;
      }
   else
      {
        GtkStyleProvider * style_provider =
               reinterpret_cast<GtkStyleProvider *>(css_provider);
        GdkScreen * screen = gdk_screen_get_default();
        gtk_style_context_add_provider_for_screen(
                    screen, style_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
      }
}
//-----------------------------------------------------------------------------
static void *
gtk_main1(void *)
{
   gtk_main();
   return 0;
}
//-----------------------------------------------------------------------------
void
static cmd_3_show_GUI()
{
  assert(builder);
  window = gtk_builder_get_object(builder, "window1");
  assert(window);
  gtk_widget_show_all((GtkWidget *)window);

pthread_t thread = 0;
   pthread_create(&thread, 0, gtk_main1, 0);
   assert(thread);
}
//-----------------------------------------------------------------------------
void
static cmd_4_get_widget_class(const char * id)
{
  assert(builder);
GObject * obj = gtk_builder_get_object(builder, id);
   if (obj)   cerr << "object " << id << " exists" << endl;
   else       cerr << "object " << id << " DOES NOT EXIST" << endl;
}
//-----------------------------------------------------------------------------
void
static cmd_6_select_widget(const char * id)
{
  assert(builder);
  selected = gtk_builder_get_object(builder, id);
   if (!selected)   cerr << "cmd_6_select_widget" << id << ") failed" << endl;
}
//-----------------------------------------------------------------------------
enum Gtype
{
  gtype_V = 0,   // void
  gtype_S = 1,   // string
  gtype_I = 2,   // integer
  gtype_F = 3,   // float
};
//-----------------------------------------------------------------------------
static void
print_fun(Fnum N, const char * gid, const char * gclass, const char * gfun,
            const char * ZAname, Gtype Zt, Gtype At, const char * help)
{
   cout << "| ";
   if (Zt != gtype_V)   cout << ZAname << " ← ";
   if (At != gtype_V)   cout << ZAname << " ";
   cout << "⎕GTK[H_ID] " << N;
   cout << "     *--or--* +" << endl << " ";
   if (Zt != gtype_V)   cout << ZAname << " ← ";
   if (At != gtype_V)   cout << ZAname << " ";
   cout << "⎕GTK[H_ID] \"" << gfun << "\"";
   cout << endl;

   cout << "| " << N << endl;

   cout << "| " << help << endl;
}
//-----------------------------------------------------------------------------
static void
print_funs()
{
#define gtk_event_def(ev_name, ...)
#define gtk_fun_def(glade_ID, gtk_class, gtk_function, ZAname, Z, A, help) \
   print_fun(FNUM_ ## gtk_class ## _ ## gtk_function, #glade_ID, \
   #gtk_class, #gtk_function, #ZAname, gtype_ ## Z, gtype_ ## A, #help);

#include "Gtk_map.def"
}
//-----------------------------------------------------------------------------
static void
print_ev2(const char * ev_name, int argc, const char * sig,
          const char * wid_name, const char * wid_id, const char *  wid_class)
{
   cout << "| +*" << ev_name
        << "*+ | " << argc
        << "| ";

const char * end = sig + strlen(sig);
    for (const char * s = sig; s < end;)
        {
          cout << *s++;
          cout << *s++;
          if (s < end)   cout << ", ";
        }

    cout << " | ";
    for (const char * s = sig; s < end; s += 2)
        {
          cout << " +";
          if      (!strncmp(s, "Gi", 2))   cout << "6";
          else if (!strncmp(s, "Ns", 2))   cout << "\\'" << wid_name << "1\\'";
          else if (!strncmp(s, "Is", 2))   cout << "\\'" << ev_name << "'";
          else if (!strncmp(s, "Cs", 2))   cout << "\\'" << wid_id << "'";
          else if (!strncmp(s, "Es", 2))   cout << "\\'" << wid_class << "'";
          else assert(0 && "Bad signature");
          cout << "+ ";
        }
   cout << endl;
}
//-----------------------------------------------------------------------------
void
static print_evs(int which)
{
   if (which == 1)   // names
      {
#define gtk_fun_def(_glade_ID, _gtk_class, _gtk_function, _ZAname, _Z,_A, _help)
#define gtk_event_def(ev_name, ...) \
        cout << "** " << #ev_name << endl;
#include "Gtk_map.def"
      }
   else if (which == 2)   // details
      {
#define gtk_fun_def(_glade_ID, _gtk_class, _gtk_function, _ZAname, _Z,_A, _help)
#define gtk_event_def(ev_name, argc, _opt, sig, wid_name, wid_id, wid_class) \
        print_ev2(#ev_name, argc, #sig, #wid_name, #wid_id, #wid_class);
#include "Gtk_map.def"
      }
   else assert(0 && "bad which");

}
//-----------------------------------------------------------------------------
static void
send_TLV(int tag, const char * data)
{
const unsigned int Vlen = strlen(data);
const unsigned int TLV_len = 8 + Vlen;
char TLV[TLV_len + 1];
   TLV[0] = tag >> 24 & 0xFF;
   TLV[1] = tag >> 16 & 0xFF;
   TLV[2] = tag >> 8  & 0xFF;
   TLV[3] = tag       & 0xFF;
   TLV[4] = Vlen >> 24;
   TLV[5] = Vlen >> 16;
   TLV[6] = Vlen >>  8;
   TLV[7] = Vlen;
   TLV[TLV_len] = 0;
   for (int l = 0; l < Vlen; ++l)   TLV[8 + l] = data[l];
   if (verbosity > 0)
      {
        cerr << "Gtk_server:   write(Tag=" << tag;
        if (Vlen)   cerr << ", Val[" << Vlen << "]=\"" << (TLV + 8) << "\"";
        else        cerr << ",  no Val)";
        cerr << endl;
      }

   if (TLV_len != write(3, TLV, TLV_len))
      {
        cerr << "Gtk_server: write(tag " << tag << " failed: "
             << strerror(errno) << endl;
      }
}
//-----------------------------------------------------------------------------
//
// conversion functions. A TLV always uses a char string for values, which
// is being converted to ther types by the functions below.
//
// The suffixes stand for:
//
// _V (void) a not existing type
// _S (string):  a string (which is then not converted)
// _F (float):   a double value
// I (integer):  an integer value
//
inline gdouble S2F(const gchar * s) { return strtof(s, 0); }
inline gdouble S2I(const gchar * s) { return strtod(s, 0); }

static gchar * F2S(gdouble d)
{
static gchar buffer[40];
int len = snprintf(buffer, sizeof(buffer) - 1, "%lf", d);
   if (len >= sizeof(buffer))   len = sizeof(buffer) - 1;
   buffer[len] - 0;

 return buffer;
}

static gchar * I2S(gint i)
{
static gchar buffer[40];
int len = snprintf(buffer, sizeof(buffer) - 1, "%ld", static_cast<long>(i));
   if (len >= sizeof(buffer))   len = sizeof(buffer) - 1;
   buffer[len] - 0;

 return buffer;
}

// APL string → Gtk type
#define arg_V(X)
#define arg_S(X) , X
#define arg_F(X) , S2F(X)
#define arg_I(X) , S2I(X)

#define TLV_arg_V , ""
#define TLV_arg_S , res
#define TLV_arg_F , res
#define TLV_arg_I , res

#define result_V(fcall) const gchar * res = "";   fcall;
#define result_S(fcall) const gchar * res = fcall;
#define result_F(fcall) const gchar * res = F2S(fcall);
#define result_I(fcall) const gchar * res = I2S(fcall);

int
main(int argc, char * argv[])
{
bool do_funs = false;
bool do_ev1 = false;
bool do_ev2 = false;
   for (int a = 1; a < argc; ++a)
       {
          if      (!strcmp(argv[a], "--funs"))   do_funs = true;
          else if (!strcmp(argv[a], "--ev1"))    do_ev1 = true;
          else if (!strcmp(argv[a], "--ev2"))    do_ev2 = true;
          else if (!strcmp(argv[a], "-v"))       ++verbosity;
          else
             {
               cerr << argv[0] << ": invalid option '"
                    << argv[a] << "'" << endl
                    << "try: --funs, --ev1, --ev2, or -v" << endl;
               return a;
             }
       }

   if (do_funs)
      {
        print_funs();
        return 0;
      }

   if (do_ev1)
      {
        print_evs(1);
        return 0;
      }

   if (do_ev2)
      {
        print_evs(2);
        return 0;
      }

const int flags = fcntl(3, F_GETFD);
   if (flags == -1)
      {
        cerr << argv[0] <<
": fcntl(3, F_GETFD) failed: " << strerror(errno) << endl <<
"    That typically happens if this program is started directly, more\n"
"    precisely: without opening file descriptor 3 first. The anticipated\n"
"    usage is to open this program from GNU APL using ⎕FIO[57] and then to\n"
"    encode TLV buffers with 33 ⎕CR and send them to this program with ⎕FIO[43]"
   << endl;
        return 1;
      }

// cerr << "Flags = " << hex << flags << endl;

  setenv("DISPLAY", ":0", true);
  gtk_init(&argc, &argv);

enum { TLV_socket = 3 };
char TLV[1000];       // the entire TLV buffer
char * V = TLV + 8;   // the value part of the TLV buffer

   for (;;)
       {
          // read the fixed size TL
          //
          const ssize_t rx_len = read(3, TLV, sizeof(TLV));
          if (rx_len < 8)
             {
               cerr << "TLV socked closed (1): " << strerror(errno) << endl;
               close(3);
               return 0;
             }

          const int TLV_tag = (TLV[0] & 0xFF) << 24 | (TLV[1] & 0xFF) << 16
                            | (TLV[2] & 0xFF) << 8 | (TLV[3] & 0xFF);

          const int response_tag = TLV_tag + (Response_0 - Command_0);

          const int V_len = (TLV[4] & 0xFF) << 24 | (TLV[5] & 0xFF) << 16
                          | (TLV[6] & 0xFF) << 8 | (TLV[7] & 0xFF);

          verbosity > 0 && cerr << "Gtk_server:   read(Tag=" << TLV_tag;

          if (rx_len != V_len + 8)
             {
               cerr << "TLV socked closed (2): "
                    << strerror(errno) << endl;
               close(3);
               return 0;
             }
          if (V_len)
             {
               V[V_len] = 0;
               verbosity > 0 && cerr << ", Val[" << V_len
                                     << "]=\"" << V << "\"" << endl;
             }
          else
             {
               verbosity > 0 && cerr << ", no Val)" << endl;;
             }

          switch(TLV_tag)
             {
                case 1:  cmd_1_load_GUI(V);             continue;
                case 2:  cmd_2_load_CSS(V);             continue;
                case 3:  cmd_3_show_GUI();              continue;
                case 4:  cmd_4_get_widget_class(V);     continue;
                case 5:  break;  // stop Gtk_server
                case 6:  cmd_6_select_widget(V);        continue;
                case 7:  cerr << "increased verbosity to: "
                              << ++verbosity << endl;
                                                        continue;
                case 8: cerr << "decreased verbosity to: "
                             << --verbosity << endl;    continue;

                // widget functions...
                //
#define gtk_event_def(ev_name, ...)
#define gtk_fun_def(glade_ID, gtk_class, gtk_function, _ZAname, Z, A, _help)  \
         case Command_ ## gtk_class ## _ ## gtk_function:                     \
{ gtk_class * widget = reinterpret_cast<gtk_class *>(selected);               \
  result_ ## Z(gtk_ ## glade_ID ## _ ## gtk_function(widget arg_ ## A(V)));   \
  send_TLV(response_tag TLV_arg_ ## Z);                                       \
}               continue;

#include "Gtk_map.def"

                default:
                    cerr << endl << argv[0]
                         << " got unexpected command " << TLV_tag << endl;
                    continue;
             }
          break;
       }

   close(3);
   return 0;
}
//-----------------------------------------------------------------------------
static void
generic_callback(GtkWidget * widget, const char * callback, const char * sig)
{
   verbosity > 0 && cerr << "callback " << callback << "() called" << endl;

   for (_ID_DB * entry = id_db; entry; entry = entry->next)
       {
         if (GTK_WIDGET(entry->obj) == widget)
            {
              verbosity > 1 && cerr << "callback " << callback
                   << " found object in DB: class=" << entry->xml_class
                   << " id=" << entry->xml_id << " name="
                   << entry->widget_name << endl;

              char data[strlen(10 + callback) + strlen(entry->xml_class)
                        + strlen(entry->xml_id) + strlen(entry->widget_name)];
              sprintf(data, "H%s:%s:%s:%s", entry->widget_name, callback,
                             entry->xml_id, entry->xml_class);
              send_TLV(Event_widget_fun_id_class, data);
              verbosity > 0 &&
                  cerr << "callback " << callback << "(1) done" << endl;
              return;
            }
       }

   // fallback: old-style
   //
gchar * widget_name = 0;
   g_object_get(widget, "name", &widget_name, NULL);
   verbosity > 0 && cerr << "    widget_name is: " << widget_name << endl
                         << "    callback is: " << callback << endl;

char data[strlen(callback) + strlen(widget_name) + 10];
   sprintf(data, "H%s:%s", widget_name, callback);
   send_TLV(Event_widget_fun, data);

   verbosity > 0 && cerr << "callback " << callback << "(2) done" << endl;
}
//-----------------------------------------------------------------------------

// callbacks that glade can connect to
//
extern "C"
{
#define gtk_fun_def(_entry, ...)
#define gtk_event_def(ev_name, _argc, opt, sig, _wid_name, _wid_id,_wid_class) \
void ev_name(GtkWidget * button opt , gpointer user_data = 0) \
{ generic_callback(button, #ev_name, #sig); }

#include "Gtk_map.def"

/* e.g:
void
clicked_0(GtkWidget * button, gpointer user_data = 0)
{
  generic_callback(button, __FUNCTION__);
}
 */

}   // extern "C"

//-----------------------------------------------------------------------------
