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
#include <math.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <gtk/gtk.h>
#include <unistd.h>

#include <iostream>
#include <iomanip>

#include "../Common.hh"
#include "Gtk_enums.hh"

using namespace std;

static int verbosity = 2;
static bool verbose__calls     = false;
static bool verbose__draw_data = false;
static bool verbose__do_draw   = false;
static bool verbose__draw_cmd  = false;
static bool verbose__reads     = false;
static bool verbose__writes    = false;

static sem_t __drawarea_sema;
static sem_t * drawarea_sema = &__drawarea_sema;

static cairo_surface_t * surface = 0;

static const char * drawarea_data = 0;
static int drawarea_dlen = 0;

//-----------------------------------------------------------------------------
static GtkBuilder * builder = 0;
static GObject * window     = 0;
static GObject * selected   = 0;
static const char * top_level_widget = 0;

// a mapping netween glade IDs and Gtk objects
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
   const GObject * obj;
} * id_db = 0;

//-----------------------------------------------------------------------------
static struct _draw_param
{
  _draw_param()
  : line_width (100),   // 100% = 2.0 in cairo
    font_size  (20),   // 100% = 2.0 in cairo
    font_slant (CAIRO_FONT_SLANT_NORMAL),
    font_weight(CAIRO_FONT_WEIGHT_NORMAL),
    area_width (-1),
    area_height(-1),
    upside_down(false)
     {
       // default fill color: transparent
       fill_color.red   = 0;
       fill_color.green = 0;
       fill_color.blue  = 0;
       fill_color.alpha = 0;

       // default line color: opaque black
       line_color.red   = 0;
       line_color.green = 0;
       line_color.blue  = 0;
       line_color.alpha = 255;

       // default font: sans-serif
       strncpy(font_family, "sans-serif", sizeof(font_family) - 1);
       font_family[sizeof(font_family) - 1] = 0;
     }

   bool line_visible() const
      { return line_color.alpha && line_width; }

   bool brush_visible() const
      { return fill_color.alpha; }

   int real_Y(int y)
       {
         return upside_down ? area_height - y : y;
       }

  GdkRGBA fill_color;
  GdkRGBA line_color;
  int line_width;   // 100% = 2.0 pixels in cairo
  int font_size;    // 100% = 2.0 pixels in cairo
  cairo_font_slant_t font_slant;
  cairo_font_weight_t font_weight;
  char font_family[100];

   int area_width;
   int area_height;
   bool upside_down;
} draw_param;

//-----------------------------------------------------------------------------
static bool
init_id_db(const char * filename)
{
FILE * f = fopen(filename, "r");
   if (f == 0)   return true;   // error

enum { MAX_level = 10 };
char class_buf      [100*MAX_level];
   memset(class_buf, 0, sizeof(class_buf));
char id_buf[100*MAX_level];
   memset(id_buf, 0, sizeof(id_buf));
char widget_name_buf[100*MAX_level];
   memset(widget_name_buf, 0, sizeof(widget_name_buf));
int level = -1;
char line[200];

   for (;;)
       {
         const char * s = fgets(line, sizeof(line) - 1, f);
         line[sizeof(line) - 1] = 0;
         if (s == 0)   break;

         while (*s == ' ')   ++s;
         if (!strncmp(s, "<object", 7))   ++level;

         char * cb = class_buf       + 100*level;
         char * ib = id_buf          + 100*level;
         char * wb = widget_name_buf + 100*level;
         if (2 == sscanf(line, " <object class=\"%s id=\"%s>", cb, ib))
            {
              if (char * qu = strchr(cb, '"'))   *qu = 0;
              if (char * qu = strchr(ib, '"'))   *qu = 0;

              if (top_level_widget == 0)
                 {
                    top_level_widget = strdup(ib);
                    verbosity > 1 && cerr << "Top-level widget: "
                                          << top_level_widget << endl;
                 }

              verbosity > 1 && cerr <<
                 "See class='" << cb << "' and id='"  << ib << "'" << endl;
            }
         else if (1 == sscanf(line, " <property name=\"name\">%s", wb))
            {
              if (char * ob = strchr(wb, '<'))   *ob = 0;

              verbosity > 1 && cerr << "See widget name='" << wb << endl;
            }
         else if (strstr(line, "</object"))
            {
              verbosity > 1 && cerr << "End of object class="
                                    << class_buf << " id="
                                    << id_buf << " widget-name="
                                    << widget_name_buf << endl;

              if (*cb || *ib || *wb )
                 {
                   _ID_DB * new_db = new _ID_DB(cb, ib, wb, id_db);
                    id_db = new_db;
                    verbosity > 1 && cerr << endl;
                 }

              *cb = 0;
              *ib = 0;
              *wb = 0;
              --level;
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
          const GObject * obj =
                G_OBJECT(gtk_builder_get_object(builder, id_db->xml_id));
          if (obj)
             {
               verbosity > 0 && cerr <<
                  "glade name '" << entry->xml_id << "' mapped to GObject "
                                 << reinterpret_cast<const void *>(obj) << endl;
               id_db->obj = obj;
             }
           else cerr << "object '" << entry->xml_id << "' not found" << endl;
       }


   gtk_builder_connect_signals(builder, NULL);
   verbosity > 0 && cerr << "GUI signals connected.\n";

   if (verbosity > 2)
      {
        for (GSList * objects = gtk_builder_get_objects(builder);
             objects; objects = objects->next)
            {
               GObject * obj = G_OBJECT(objects->data);
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
void *
gtk_drawingarea_draw_commands(GtkDrawingArea * widget, const char * data)
{
   // this function is called when apl does DrawCmd ⎕GTK[H_ID] "draw_commands"

   verbose__calls && cerr << "gtk_drawingarea_draw_commands()..." << endl;
   sem_wait(drawarea_sema);

   while (*data && *data < ' ')   ++data;   // remove leading whitespace

   if (!strncmp(data, "background", 10))   // fresh drawing
      {
        delete[] drawarea_data;
        drawarea_dlen = strlen(data);

        char * cp = new char[drawarea_dlen + 1];
        assert(cp);
        memcpy(cp, data, drawarea_dlen + 1);   // + 1 for trailing 0
        drawarea_data = cp;
      }
   else                                    // append to existing drawing
      {
        const int add_len = strlen(data);
        char * cp = new char[drawarea_dlen + add_len + 1];
        assert(cp);
        memcpy(cp, drawarea_data, drawarea_dlen);        // existing
        memcpy(cp + drawarea_dlen, data, add_len + 1);   // new data
        delete[] drawarea_data;
        drawarea_data = cp;
        drawarea_dlen += add_len;
      }

   sem_post(drawarea_sema);
   verbose__draw_data && cerr << "draw_data[" << drawarea_dlen << "]:\n"
                              << drawarea_data << endl;

   if (surface)   cairo_surface_destroy(surface);
   surface = 0;

   verbose__calls && cerr << "gtk_drawingarea_draw_commands() "
                               "calls gtk_widget_queue_draw()." << endl;
   gtk_widget_queue_draw(GTK_WIDGET(widget));

   verbose__calls && cerr << "gtk_drawingarea_draw_commands() done." << endl;
   return 0;
}
//-----------------------------------------------------------------------------
void *
gtk_drawingarea_set_Y_origin(GtkDrawingArea * widget, int data)
{
  draw_param.upside_down = true;
   return 0;
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
        GtkStyleProvider * style_provider = GTK_STYLE_PROVIDER(css_provider);
        GdkScreen * screen = gdk_screen_get_default();
        gtk_style_context_add_provider_for_screen(
                    screen, style_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
      }
}
//-----------------------------------------------------------------------------
void *
gtk_drawingarea_set_area_size(GtkDrawingArea * widget, long data)
{
const int width = data >> 16 & 0xFFFF;
const int height = data & 0xFFFF;

   draw_param.area_width = width;
   draw_param.area_height = height;
   gtk_widget_set_size_request(GTK_WIDGET(widget), width, height);
   return 0;
}
//-----------------------------------------------------------------------------
void
static cmd_3_show_GUI()
{
  assert(builder);
   if (top_level_widget)
      window = gtk_builder_get_object(builder, top_level_widget);
   else
      window = gtk_builder_get_object(builder, "window1");

  assert(window);
  gtk_widget_show_all(GTK_WIDGET(window));

pthread_t thread = 0;
   pthread_create(&thread, 0, reinterpret_cast<void *(*)(void *)>(gtk_main), 0);
   assert(thread);
   usleep(200000);
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
   if (!selected)
      cerr << "cmd_6_select_widget(id='" << id << "') failed" << endl;
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
          else if (!strncmp(s, "Ns", 2))   cout << "\\'" << wid_name << "'";
          else if (!strncmp(s, "Is", 2))   cout << "\\'" << wid_id << "1\\'";
          else if (!strncmp(s, "Cs", 2))   cout << "\\'" << wid_class << "'";
          else if (!strncmp(s, "Es", 2))   cout << "\\'" << ev_name << "'";
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
#define gtk_event_def(ev_name, ...)   cout << "** " << #ev_name << endl;
#include "Gtk_map.def"
      }
   else if (which == 2)   // details
      {
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
const ssize_t TLV_len = 8 + Vlen;
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
   for (unsigned int l = 0; l < Vlen; ++l)   TLV[8 + l] = data[l];
   if (verbose__writes)
      {
        cerr << "Gtk_server:   write(Tag=" << tag;
        if (Vlen)   cerr << ", Val[" << Vlen << "]=\"" << (TLV + 8) << "\"";
        else        cerr << ",  no Val)";
        cerr << endl;
      }

   errno = 0;
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
inline gdouble S2F(const gchar * s) { return strtod(s, 0); }
inline long    S2I(const gchar * s) { return strtol(s, 0, 10); }

static gchar * F2S(gdouble d)
{
static gchar buffer[40];
unsigned int len = snprintf(buffer, sizeof(buffer) - 1, "%lf", d);
   if (len >= sizeof(buffer))   len = sizeof(buffer) - 1;
   buffer[len] = 0;

 return buffer;
}

static gchar * I2S(gint i)
{
static gchar buffer[40];
unsigned int len = snprintf(buffer, sizeof(buffer) - 1, "%ld",
                            static_cast<long>(i));
   if (len >= sizeof(buffer))   len = sizeof(buffer) - 1;
   buffer[len] = 0;

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

#define result_V(fcall) fcall;
#define result_S(fcall) const gchar * res = fcall;
#define result_F(fcall) const gchar * res = F2S(fcall);
#define result_I(fcall) const gchar * res = I2S(fcall);

int
main(int argc, char * argv[])
{
   __sem_init(drawarea_sema, /* pshared */ 0, 1);

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
               __sem_destroy(drawarea_sema);
               return a;
             }
       }

   if (do_funs)
      {
         print_funs();
         __sem_destroy(drawarea_sema);
         return 0;
      }

   if (do_ev1)
      {
        print_evs(1);
        __sem_destroy(drawarea_sema);
        return 0;
      }

   if (do_ev2)
      {
         print_evs(2);
        __sem_destroy(drawarea_sema);
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
        __sem_destroy(drawarea_sema);
        return 1;
      }

// cerr << "Flags = " << hex << flags << endl;

  setenv("DISPLAY", ":0", true);
  gtk_init(&argc, &argv);

enum { TLV_socket     = 3,
       initial_buflen = 10000
     };

   // start with a buffer of 10k and extend it as needed
   //
unsigned int TLV_buflen = initial_buflen;
char * TLV = new char[TLV_buflen];   // the entire TLV buffer
   assert(TLV);
char * V = TLV + 8;                  // the V part of the TLV buffer

   for (;;)
       {
          // expand buffer if needed
          {
            const ssize_t len = recv(3, TLV, 8, MSG_PEEK);
            if (len != 8)
               {
                 cerr << "TLV socked closed (1): " << strerror(errno) << endl;
                 close(3);
                 __sem_destroy(drawarea_sema);
                 return 0;
               }

            const unsigned int V_len = (TLV[4] & 0xFF) << 24
                                     | (TLV[5] & 0xFF) << 16
                                     | (TLV[6] & 0xFF) << 8
                                     | (TLV[7] & 0xFF);

            if ((V_len + 8) > TLV_buflen)   // re-allocate a larger buffer
               {
                 delete [] TLV;
                 TLV_buflen = V_len + 8;
                 TLV = new char[TLV_buflen];
                 assert(TLV);
                 V = TLV + 8;
               }
          }

          const ssize_t rx_len = read(3, TLV, TLV_buflen);
          if (rx_len < 8)
             {
               cerr << "TLV socked closed (2): " << strerror(errno) << endl;
               close(3);
               __sem_destroy(drawarea_sema);
               return 0;
             }

          const int TLV_tag = (TLV[0] & 0xFF) << 24 | (TLV[1] & 0xFF) << 16
                            | (TLV[2] & 0xFF) << 8 | (TLV[3] & 0xFF);

          const int response_tag = TLV_tag + (Response_0 - Command_0);

          const int V_len = (TLV[4] & 0xFF) << 24 | (TLV[5] & 0xFF) << 16
                          | (TLV[6] & 0xFF) << 8 | (TLV[7] & 0xFF);

          verbose__reads && cerr << "Gtk_server:   read(Tag=" << TLV_tag;

          if (rx_len != V_len + 8)
             {
               cerr << "TLV socked closed (3): "
                    << strerror(errno) << ": V_len=" << V_len
                                       << " rx_len=" << rx_len << endl;
               close(3);
               __sem_destroy(drawarea_sema);
               return 0;
             }
          if (V_len)
             {
               V[V_len] = 0;
               verbose__reads && cerr << ", Val[" << V_len
                                     << "]=\"" << V << "\"" << endl;
             }
          else
             {
               verbose__reads && cerr << ", no Val)" << endl;;
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

   cerr << endl << "Gtk_server closed from client" << endl;
   close(3);
   __sem_destroy(drawarea_sema);
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
inline void
cairo_set_source_rgba(cairo_t * cr, const GdkRGBA & color)
{
   cairo_set_source_rgba(cr, color.red   / 255.0,
                             color.green / 255.0,
                             color.blue  / 255.0,
                             color.alpha / 100.0);
}
//-----------------------------------------------------------------------------
void
draw_line(cairo_t * cr, const GdkRGBA & color, int x0, int y0, int x1, int y1)
{
const int sdelta_x = x1 - x0;
const int sdelta_y = y1 - y0;
const int adelta_x = (sdelta_x >= 0) ? sdelta_x : -sdelta_x;
const int adelta_y = (sdelta_y >= 0) ? sdelta_y : -sdelta_y;
const double rad = 1.0;

   cairo_set_source_rgba(cr, color);

   if (adelta_x >= adelta_y)
      {
        const double dy_dx = double(sdelta_y) / sdelta_x;
        for (int xx = x0; xx < x1; ++xx)
            {
              cairo_rectangle(cr, xx, y0 + (xx - x0)*dy_dx, rad, rad);
            }
        for (int xx = x1; xx < x0; ++xx)
            {
              cairo_rectangle(cr, xx, y0 + (xx - x1)*dy_dx, rad, rad);
            }
      }
   else
      {
        const double dx_dy = double(sdelta_x) / sdelta_y;
        for (int yy = y0; yy < y1; ++yy)
            {
              cairo_rectangle(cr, x0 + (yy - y0)*dx_dy, yy, rad, rad);
            }
        for (int yy = y1; yy < y0; ++yy)
            {
              cairo_rectangle(cr, x0 + (yy - y1)*dx_dy, yy, rad, rad);
            }
      }

   cairo_fill(cr);
}
//-----------------------------------------------------------------------------
void
do_draw_cmd(GtkWidget * drawing_area, const char * cmd, const char * cmd_end)
{
int count, i1, i2, i3, i4;
char s1[100];

   while (cmd < cmd_end && *cmd == ' ')   ++cmd;   // delete leading spaces
const int cmd_len = cmd_end - cmd;
   if (cmd_len == 0)   return;                   // empty line

   count = sscanf(cmd, "background %u %u %u %u", &i1, &i2, &i3, &i4);
   if (count >= 3)
      {
        verbose__draw_cmd && cerr <<
           "DRAW BACKGROUND " << i1 << " " << i2 << " " << i3 << endl;
        const double width  = gtk_widget_get_allocated_width(drawing_area);
        const double height = gtk_widget_get_allocated_height(drawing_area);
        cairo_t * cr1 = cairo_create(surface);
        cairo_rectangle(cr1, 0, 0, width, height);
        cairo_close_path(cr1);
        cairo_set_source_rgb(cr1, i1/255.0, i2/255.0, i3/255.0);
        cairo_fill(cr1);
        cairo_destroy(cr1);
        return;
      }

   i4 = 100; count = sscanf(cmd, "fill-color %u %u %u %u", &i1, &i2, &i3, &i4);
   if (count >= 3)
      {
        verbose__draw_cmd && cerr <<
           "SET BRUSH COLOR (" << i1 << " " << i2 << " "
                                << i3 << ")" << endl;
        draw_param.fill_color.red   = i1;
        draw_param.fill_color.green = i2;
        draw_param.fill_color.blue  = i3;
        draw_param.fill_color.alpha = i4;
        return;
      }

   i4 = 100; count = sscanf(cmd, "line-color %u %u %u %u", &i1, &i2, &i3, &i4);
   if (count >= 3)
      {
        verbose__draw_cmd && cerr <<
           "SET LINE COLOR (" << i1 << " " << i2 << " " << i3 << ")" << endl;
        draw_param.line_color.red   = i1;
        draw_param.line_color.green = i2;
        draw_param.line_color.blue  = i3;
        draw_param.line_color.alpha = i4;
        return;
      }

   count = sscanf(cmd, "font-size %u", &i1);
   if (count == 1)
      {
        draw_param.font_size = i1;
        return;
      }

   count = sscanf(cmd, "font-family %s", s1);
   if (count == 1)
      {
        s1[sizeof(s1) - 1] = 0;
        enum { FONT_LEN = sizeof(draw_param.font_family) };
        strncpy(draw_param.font_family, s1, FONT_LEN - 1);
        draw_param.font_family[FONT_LEN - 1] = 0;
        return;
      }

   count = sscanf(cmd, "font-slant %s", s1);
   if (count == 1)
      {
        s1[sizeof(s1) - 1] = 0;
        if (!strcasecmp(s1, "NORMAL"))
           draw_param.font_slant = CAIRO_FONT_SLANT_NORMAL;
        else if (!strcasecmp(s1, "ITALIC"))
           draw_param.font_slant = CAIRO_FONT_SLANT_ITALIC;
        else if (!strcasecmp(s1, "OBLIQUE"))
           draw_param.font_slant = CAIRO_FONT_SLANT_OBLIQUE;
        else
           cerr << "bad font-slant: " << s1 << endl;
        return;
      }

   count = sscanf(cmd, "font-weight %s", s1);
   if (count == 1)
      {
        s1[sizeof(s1) - 1] = 0;
        if (!strcasecmp(s1, "NORMAL"))
           draw_param.font_weight = CAIRO_FONT_WEIGHT_NORMAL;
        else if (!strcasecmp(s1, "BOLD"))
           draw_param.font_weight = CAIRO_FONT_WEIGHT_BOLD;
        else
           cerr << "bad font-weight: " << s1 << endl;
        return;
      }

   count = sscanf(cmd, "line-width %u", &i1);
   if (count == 1)
      {
        verbose__draw_cmd && cerr <<
           "SET LINE WIDTH " << i1 << endl;
        draw_param.line_width = i1;
        return;
      }

   if  (3 == sscanf(cmd, "circle (%u %u) %u", &i1, &i2, &i3))
      {
        verbose__draw_cmd && cerr <<
           "DRAW CIRCLE (" << i1 << ":" << i2 << ") " << i3 << endl;

        const double x   = i1;   // center X
        const double y   = draw_param.real_Y(i2);   // center Y
        const double rad = i3;   // radius

        if (draw_param.brush_visible())
           {
             cairo_t * cr1 = cairo_create(surface);

             cairo_arc(cr1, x, y, rad, 0.0, 2*M_PI);
             cairo_close_path(cr1);
             cairo_set_source_rgba(cr1, draw_param.fill_color);
             cairo_fill(cr1);
             cairo_destroy(cr1);
           }

        if (draw_param.line_visible())
           {
             cairo_t * cr1 = cairo_create(surface);

             cairo_arc(cr1, x, y, rad, 0.0, 2*M_PI);
             cairo_set_source_rgba(cr1, draw_param.line_color);
             cairo_set_line_width(cr1, draw_param.line_width*0.02);
             cairo_stroke(cr1);
             cairo_destroy(cr1);
           }
        return;
      }

   if  (4 == sscanf(cmd, "ellipse (%u %u) (%u %u)", &i1, &i2, &i3, &i4))
      {
        verbose__draw_cmd && cerr <<
           "DRAW ELLIPSE (" << i1 << ":" << i2 << ") ("
                            << i3 << ":" << i4 << ")" << endl;

        const double x     = i1;   // center X
        const double y     = draw_param.real_Y(i2);   // center I
        const double rad_x = i3 ? i3 : 1.0;   // radius X
        const double rad_y = i4 ? i4 : 1.0;   // radius Y

        if (draw_param.brush_visible())
           {
             cairo_t * cr1 = cairo_create(surface);

             cairo_save(cr1);
             cairo_scale(cr1, rad_x, rad_y);
             cairo_arc(cr1, x/rad_x, y/rad_y, 1.0, 0.0, 2*M_PI);
             cairo_close_path(cr1);
             cairo_set_source_rgba(cr1, draw_param.fill_color);
             cairo_fill(cr1);
             cairo_restore(cr1);
             cairo_destroy(cr1);
           }

        if (draw_param.line_visible())
           {
             cairo_t * cr1 = cairo_create(surface);

             cairo_save(cr1);
             cairo_scale(cr1, rad_x, rad_y);
             cairo_arc(cr1, x/rad_x, y/rad_y, 1.0, 0.0, 2*M_PI);
             cairo_restore(cr1);
             cairo_set_source_rgba(cr1, draw_param.line_color);
             cairo_set_line_width(cr1, draw_param.line_width*0.02);
             cairo_stroke(cr1);
             cairo_destroy(cr1);
           }

        return;
      }

   if  (4 == sscanf(cmd, "line (%u %u) (%u %u)", &i1, &i2, &i3, &i4))
      {
        verbose__draw_cmd && cerr <<
           "DRAW LINE (" << i1 << ":" << i2 << ") ("
                              << i3 << ":" << i4 << ")" << endl;
        const double x0 = i1;
        const double y0 = draw_param.real_Y(i2);
        const double x1 = i3;
        const double y1 = draw_param.real_Y(i4);
        cairo_t * cr1 = cairo_create(surface);

        draw_line(cr1, draw_param.line_color, x0, y0, x1, y1);
        cairo_destroy(cr1);
        return;

        cairo_move_to(cr1, x0, y0);
        cairo_line_to(cr1, x1, y1);
        cairo_set_source_rgba(cr1, draw_param.line_color);
        cairo_set_line_width(cr1, draw_param.line_width*0.02);
        cairo_stroke(cr1);
        cairo_destroy(cr1);
        return;
      }

   if  (4 == sscanf(cmd, "rectangle (%u %u) (%u %u)", &i1, &i2, &i3, &i4))
      {
        verbose__draw_cmd && cerr <<
           "DRAW RECTANGLE (" << i1 << ":" << i2 << ") ("
                                   << i3 << ":" << i4 << ")" << endl;
        const double x0      = i1;
        const double y0      = draw_param.real_Y(i2);
        const double x1      = i3;
        const double y1      = draw_param.real_Y(i4);
        const double width  = x1 - x0;
        const double height = y1 - y0;

        if (draw_param.brush_visible())
           {
             cairo_t * cr1 = cairo_create(surface);
             cairo_rectangle(cr1, x0, y0, width, height);
             cairo_close_path(cr1);
             cairo_set_source_rgba(cr1, draw_param.fill_color);
             cairo_fill(cr1);
             cairo_destroy(cr1);
           }

        if (draw_param.line_visible())
           {
             cairo_t * cr1 = cairo_create(surface);
             cairo_rectangle(cr1, x0, y0, width, height);
             cairo_set_source_rgba(cr1, draw_param.line_color);
             cairo_set_line_width(cr1, draw_param.line_width*0.02);
             cairo_stroke(cr1);
             cairo_destroy(cr1);
           }
        return;
      }

   if  (4 == sscanf(cmd, "polygon (%u %u) (%u %u)", &i1, &i2, &i3, &i4))
      {
        verbose__draw_cmd && cerr << "DRAW POLYGON";
        int point_count = 0;
        for (const char * p = cmd; p < cmd_end;)
            {
              p = strchr(p, '(');
              if (p == 0)         break;   // no more points
              if (p >= cmd_end)   break;   // point in next command
              ++point_count;
              ++p;   // skip (
            }
        double x[point_count];
        double y[point_count];
        int point_idx = 0;
        for (const char * p = cmd; p < cmd_end;)
            {
              p = strchr(p, '(');
              if (p == 0)         break;   // no more points
              if (p >= cmd_end)   break;   // point in next command
              if (2 != sscanf(p, "(%u %u)", &i1, &i2))
                 {
                   cerr << endl
                        << "polygon: bad point " << point_count << endl
                        << "    cmd: " << cmd << endl
                        << "    p: "   << p << endl
                        << p << endl;
                   return;
                 }
             verbose__draw_cmd && cerr << " (" << i1 << " " << i2 << ")";
             x[point_idx] = i1;
             y[point_idx] = draw_param.real_Y(i2);
             ++point_idx;
             ++p;
           }
        verbose__draw_cmd && cerr << endl;

        if (draw_param.brush_visible())
           {
             cairo_t * cr1 = cairo_create(surface);

             cairo_save(cr1);
             cairo_move_to(cr1, x[0], y[0]);
             for (int j = 1; j < point_count; ++j)
                 cairo_line_to(cr1, x[j], y[j]);
             cairo_close_path(cr1);
             cairo_set_source_rgba(cr1, draw_param.fill_color);
             cairo_fill(cr1);
             cairo_restore(cr1);
             cairo_destroy(cr1);
           }

        if (draw_param.line_visible())
           {
             cairo_t * cr1 = cairo_create(surface);
             cairo_move_to(cr1, x[0], y[0]);
             for (int j = 1; j < point_count; ++j)
                 cairo_line_to(cr1, x[j], y[j]);
             cairo_close_path(cr1);
             cairo_set_source_rgba(cr1, draw_param.line_color);
             cairo_set_line_width(cr1, draw_param.line_width*0.02);
             cairo_stroke(cr1);
             cairo_destroy(cr1);
           }
        return;
      }

   count = sscanf(cmd, "text (%u %u) %n", &i1, &i2, &i3);
   if (count == 2)
      {
        const double x = i1;
        const double y = draw_param.real_Y(i2);

        cairo_t * cr1 = cairo_create(surface);

        cairo_move_to(cr1, x, y);
        cairo_select_font_face(cr1, draw_param.font_family,
                                    draw_param.font_slant,
                                    draw_param.font_weight);
        cairo_set_font_size(cr1, draw_param.font_size);
        const char * start = cmd + i3;
        const int slen = cmd_len - i3;
        strncpy(s1, start, sizeof(s1) - 1);
        s1[slen] = 0;
        cairo_show_text(cr1, s1);
        cairo_destroy(cr1);
        return;
      }

   cerr << endl << "BAD DRAW COMMAND: ";
   for (const char * s = cmd; s < cmd_end; ++s)   cerr << *s;
   cerr << endl << endl;
}
//-----------------------------------------------------------------------------


// callbacks that glade can connect to
//
extern "C"
{
#define gtk_event_def(ev_name, _argc, opt, sig, _wid_name, _wid_id,_wid_class) \
void ev_name(GtkWidget * widget opt , gpointer user_data = 0) \
{ generic_callback(widget, #ev_name, #sig); }

#include "Gtk_map.def"

/* e.g:
void
clicked_0(GtkWidget * button, gpointer user_data = 0)
{
  generic_callback(button, __FUNCTION__);
}
 */

//-----------------------------------------------------------------------------
gboolean
do_draw(GtkWidget * drawing_area, cairo_t * cr, gpointer user_data)
{
   verbose__calls && cerr << "*** callback do_draw()..." << endl ;

   if (surface == 0)
      {
        if (drawarea_data == 0)   return false;

        verbose__do_draw && cerr << "   new surface: cr="
                                 << reinterpret_cast<const void *>(cr) << endl;

        sem_wait(drawarea_sema);
        assert(drawarea_data);

        // pretend configure event
        //
        {
          GtkWidget * widget = GTK_WIDGET(drawing_area);
          surface = gdk_window_create_similar_surface(
                               gtk_widget_get_window(widget),
                               CAIRO_CONTENT_COLOR,
                               gtk_widget_get_allocated_width(widget),
                               gtk_widget_get_allocated_height(widget));
        }

        const char * end = drawarea_data + drawarea_dlen;
        for (const char * cmd = drawarea_data; cmd < end; )
            {
              const char * cmd_end = strchr(cmd, '\n');
              assert(cmd_end);   // guaranteed by ⎕GTK
              do_draw_cmd(drawing_area, cmd, cmd_end);
              cmd = cmd_end + 1;
            }

        sem_post(drawarea_sema);
      }

   cairo_set_source_surface(cr, surface, 0, 0);
   cairo_paint(cr);
   verbose__calls && cerr << "do_draw() done." << endl;

   return false;   // propagate the event further
}

//-----------------------------------------------------------------------------
gboolean
top_level_done(GtkWidget * window, gpointer user_data)
{
   // top-level window has been closed
   //
char data[50];

const unsigned int slen = snprintf(data, sizeof(data),
                          "H%s:%s", "top-level", __FUNCTION__);
   if (slen >= sizeof(data))   data[sizeof(data) - 1] = 0;

   generic_callback(window, "destroy", __FUNCTION__);
   // send_TLV(Event_widget_fun, data);
   close(3);
   exit(0);
   return false;   // propagate the event further
}

}   // extern "C"
//-----------------------------------------------------------------------------
