/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2018-2026  Dr. Jürgen Sauermann

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
#include <signal.h>
#include <string.h>

#include <iostream>
#include <iomanip>

#include "Common.hh"

// X11

#if HAVE_LIBX11
# define MISSING_LIBX11 ""
#else   // not HAVE_LIBX11
# define MISSING_LIBX11 " libX11.so"
#endif   // (don't) HAVE_LIBX11

#if HAVE_X11_XLIB_H
# define MISSING_X11_XLIB_H ""
#else   // not HAVE_X11_XLIB_H
# define MISSING_X11_XLIB_H " X11/Xlib.h"
#endif   // (don't) HAVE_X11_XLIB_H

#if HAVE_X11_XUTIL_H
# define MISSING_X11_XUTIL_H ""
#else   // not HAVE_X11_XUTIL_H
# define MISSING_X11_XUTIL_H " X11/Xutil.h"
#endif   // (don't) HAVE_X11_XUTIL_H

// X11 summary
#define MISSING_X11 \
    MISSING_LIBX11 \
    MISSING_X11_XLIB_H \
    MISSING_X11_XUTIL_H

// XCB

#if HAVE_LIBX11_XCB
# define MISSING_LIBX11_XCB ""
#else   // not HAVE_LIBX11_XCB
# define MISSING_LIBX11_XCB " libX11-xcb.so"
#endif   // (don't) HAVE_LIBX11_XCB

#if HAVE_LIBXCB
# define MISSING_LIBXCB ""
#else   // not HAVE_LIBXCB
# define MISSING_LIBXCB " libX11-xcb.so"
#endif   // (don't) HAVE_LIBXCB

#if HAVE_X11_XLIB_XCB_H
# define MISSING_X11_XLIB_XCB_H ""
#else   // not HAVE_X11_XLIB_XCB_H
# define MISSING_X11_XLIB_XCB_H " X11/Xlib-xcb.h"
#endif   // (don't) HAVE_X11_XLIB_XCB_H

#if HAVE_XCB_XCB_H
# define MISSING_XCB_XCB_H ""
#else   // not HAVE_XCB_XCB_H
# define MISSING_XCB_XCB_H " xcb/xcb.h"
#endif   // (don't) HAVE_XCB_XCB_H

// XCB summary
#define MISSING_XCB \
    MISSING_LIBX11_XCB \
    MISSING_LIBXCB \
    MISSING_X11_XLIB_XCB_H \
    MISSING_XCB_XCB_H

// GTK

#if HAVE_LIBGTK_3
# define MISSING_LIBGTK_3 ""
#else   // not HAVE_LIBGTK_3
# define MISSING_LIBGTK_3 " libgtk-3.so"
#endif   // (don't) HAVE_LIBGTK_3

#if HAVE_LIBCAIRO
# define MISSING_LIBCAIRO ""
#else   // not HAVE_LIBCAIRO
# define MISSING_LIBCAIRO " libcairo.so"
#endif   // (don't) HAVE_LIBCAIRO

// GTK summary
#define MISSING_GTK \
    MISSING_LIBGTK_3 \
    MISSING_LIBCAIRO

#include "Avec.hh"
#include "Common.hh"
#include "Quad_PLOT.hh"

extern Value_P do_plot_ASCII(const Plot_window_properties & w_props,
                          const Plot_data & data);

Quad_PLOT  Quad_PLOT::fun;

vector<Quad_PLOT::PLOT_context *> Quad_PLOT::all_PLOT_windows;

Quad_PLOT::Handle Quad_PLOT::next_handle = 0;

sem_t __all_PLOT_windows_sema;
sem_t * Quad_PLOT::all_PLOT_windows_sema = &__all_PLOT_windows_sema;

sem_t __expose_sema;
sem_t * Quad_PLOT::expose_sema = &__expose_sema;

int Quad_PLOT::verbosity = 0;

#if MINGW_SRC
const Quad_PLOT::Plot_driver default_plot_driver = Quad_PLOT::PltDrv_WIN32;
#elif apl_GTK3 && apl_X11
   // matches the runtime gate in start_GUI(): the PltDrv_GTK case there is
   // only implemented '#if apl_GTK3 && apl_X11' (Bill Heagy, /tmp/trouble1.eml,
   // 2026-08-12: a build with GTK3 but no X11 picked PltDrv_GTK here anyway,
   // so every attribute-but-no-gui_driver ⎕PLOT hit start_GUI()'s "GTK was
   // not completely installed" DOMAIN_ERROR instead of falling back to ASCII)
const Quad_PLOT::Plot_driver default_plot_driver = Quad_PLOT::PltDrv_GTK;
#elif apl_XCB
const Quad_PLOT::Plot_driver default_plot_driver = Quad_PLOT::PltDrv_XCB;
#else
const Quad_PLOT::Plot_driver default_plot_driver = Quad_PLOT::PltDrv_ASCII;
#endif

# include <stdio.h>
# include <math.h>
# include <stdlib.h>
# include <string.h>

# include <iostream>
# include <iomanip>

using namespace std;

# include "ComplexCell.hh"
# include "FloatCell.hh"
#include "Plot_data.hh"
#include "Plot_line_properties.hh"
#include "Plot_window_properties.hh"
# include "Security.hh"
# include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════

Quad_PLOT::Quad_PLOT()
  : QuadFunction(TOK_Quad_PLOT)
{
   __sem_init(all_PLOT_windows_sema, /* thread sema */ 0, /* value */ 1);
   __sem_init(expose_sema,           /* thread sema */ 0, /* value */ 0);
   verbosity = 0;
}
//────────────────────────────────────────────────────────────────────────────
Quad_PLOT::Handle
Quad_PLOT::PLOT_context::remove_handle(Handle handle)
{
// CERR << "remove_handle(" << handle << ")" << endl;

   // called from the "destroy" signal handler, i.e. on the GTK/XCB driver's
   // own thread -- concurrently with window_control() (interpreter thread)
   // reading/mutating the same vector. Same lock as window_control().
   //
   if (Sys::sem_wait_safe(all_PLOT_windows_sema, "the ⎕PLOT window list lock "
                          "(remove_handle)", PLOT_SEM_TIMEOUT_SECONDS)
       != Sys::WAIT_OK)
      {
        // a driver callback thread like this one must not raise a C++/APL
        // exception on failure (see Sys::sem_wait_safe()'s comment); give up on
        // this particular removal instead. A stale entry left behind in
        // all_PLOT_windows is far less harmful than touching the vector
        // without actually holding its semaphore.
        //
        return 0;
      }

   loop(h, all_PLOT_windows.size())
      {
        if (all_PLOT_windows[h]->handle == handle)
           {
             all_PLOT_windows[h] = all_PLOT_windows.back();
             all_PLOT_windows.pop_back();
             sem_post(all_PLOT_windows_sema);
             return handle;
           }
      }

   sem_post(all_PLOT_windows_sema);
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
Quad_PLOT::~Quad_PLOT()
{
   __sem_destroy(all_PLOT_windows_sema);
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_PLOT::start_GUI(Plot_window_properties * w_props, int handle,
                     Plot_driver driver_type)
{
Plot_driver driver;
const string driver_attr = w_props->get_gui_driver();
 
   if      (driver_attr == "")        driver = default_plot_driver;
   else if (driver_attr == "GTK")     driver = PltDrv_GTK;
   else if (driver_attr == "XCB")     driver = PltDrv_XCB;
   else if (driver_attr == "ASCII")   driver = PltDrv_ASCII;
   else if (driver_attr == "WIN32")   driver = PltDrv_WIN32;
   else
           {
             MORE_ERROR() << "A ⎕PLOT B: invalid gui_driver '"
                          << driver_attr.c_str() << "'";
             DOMAIN_ERROR;
           }

   if (driver == PltDrv_GTK)
      {
#if apl_GTK3 && apl_X11
        // plot_main_GTK() pushes a new GTK_context into variable
        // Quad_PLOT::all_PLOT_windows and  posts expose_sema after
        // its plot window was exposed.
        //
        plot_main_GTK(w_props, handle);
        sem_wait_safe_I(expose_sema, "the GTK plot window to be shown");
        sem_post(expose_sema);   // for the next window (if any)
        Log(LOG_Quad_PLOT)   CERR << "Plot driver GTK loaded." << endl;
        return;
#else   // not apl_GTK3

   MORE_ERROR() << "A ⎕PLOT B: gui_driver=GTK requested, "
                   "but GTK was not completely installed.\n"
                   "Missing: " MISSING_X11 MISSING_GTK;
   DOMAIN_ERROR;
#endif   // (not) apl_GTK3 && apl_X11
      }

   if (driver == PltDrv_XCB)
      {
#if apl_XCB
        // start a thread that pushes a XCB_context and then posts the
        // expose_sema after its plot window was exposed.
        //
        pthread_t th;
        const int perr = pthread_create(&th, 0, plot_main_XCB, w_props);
        if (perr)
           {
             // same unchecked-pthread_create() hang as plot_main_GTK() (see
             // its comment) -- an un-postable expose_sema wait right below
             // would hang until ^C without this check.
             //
             MORE_ERROR() << "A ⎕PLOT B: pthread_create() for the XCB event "
                             "loop thread failed: " << strerror(perr);
             DOMAIN_ERROR;
           }
        sem_wait_safe_I(expose_sema, "the XCB plot window to be shown");
        sem_post(expose_sema);   // for the next window (if any)

        // plot_main_XCB() could not start (e.g. no X-server): it recorded
        // the reason in w_props (Bugs16 #1) instead of throwing across the
        // pthread entry function, and is raised here, back on the
        // interpreter thread, once safe to do so.
        //
        if (w_props->get_gui_thread_error().size())
           {
             MORE_ERROR() << "A ⎕PLOT B: "
                          << w_props->get_gui_thread_error().c_str();
             DOMAIN_ERROR;
           }

        Log(LOG_Quad_PLOT)   CERR << "Plot driver XCB loaded." << endl;
        return;
#else   // not apl_XCB
   MORE_ERROR() << "A ⎕PLOT B: gui_driver=XCB requested, "
                   "but XCB was not completely installed.\n"
                   "Missing: " MISSING_X11 MISSING_XCB;
   DOMAIN_ERROR;
#endif   // (not) apl_XCB
      }

   if (driver == PltDrv_WIN32)
      {
#if MINGW_SRC
        // plot_main_WIN32() pushes a new WIN32_context into variable
        // Quad_PLOT::all_PLOT_windows and posts expose_sema after
        // its plot window was exposed.
        //
        plot_main_WIN32(w_props, handle);
        sem_wait_safe_I(expose_sema, "the WIN32 plot window to be shown");
        sem_post(expose_sema);   // for the next window (if any)
        Log(LOG_Quad_PLOT)   CERR << "Plot driver WIN32 loaded." << endl;
        return;
#else   // not MINGW_SRC
   MORE_ERROR() << "A ⎕PLOT B: gui_driver=WIN32 requested, but this "
                   "build was not compiled for a Windows/MinGW target.";
   DOMAIN_ERROR;
#endif   // (not) MINGW_SRC
      }

   // neither GTK, XCB, nor WIN32. Use ASCII fallback
   Assert(driver == PltDrv_ASCII);
   Log(LOG_Quad_PLOT)
      CERR << "gui_driver: " << driver_attr.c_str() 
           << " (no driver needed)." << endl;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_PLOT::eval_AB(cValue_R A, cValue_R B) const
{
   CHECK_SECURITY(disable_Quad_PLOT);

   if (B.get_rank() > 3)        RANK_ERROR;
   if (B.element_count() < 2)   LENGTH_ERROR;

   // plot window with default attributes
   //
Plot_data * data = setup_data(B);
   if (data == 0)   DOMAIN_ERROR;

Plot_window_properties * w_props = new Plot_window_properties(data, verbosity);

   Log(LOG_Quad_PLOT)
     CERR << "wprops = " << w_props << " created." << endl;

   // from here on 'data' is owned by 'w_props' (whose destructor
   // will delete it).
   //
   if (const ErrorCode ec = parse_attributes(A, w_props))
      {
        delete w_props;
        throw_apl_error(ec, LOC);
      }
   if (w_props->update(verbosity))   { delete w_props;   DOMAIN_ERROR; }

   // plot driver selection. The user may or may not have requested a particular
   // driver
   //
   if (w_props->get_gui_driver() == "GTK")    // GTK requested
      {
#if not apl_GTK3
        MORE_ERROR() << "A ⎕PLOT B: gui_driver=GTK, but GTK was "
                        "not (completely) installed.\nMissing:"
                        MISSING_X11 MISSING_GTK;
        DOMAIN_ERROR;
#endif
      }
   else if (w_props->get_gui_driver() == "XCB")    // XCB requested
      {
#if not apl_XCB
        MORE_ERROR() << "A ⎕PLOT B: gui_driver=XCB, but XCB was "
                        "not (completely) installed.\nMissing:"
                        MISSING_X11 MISSING_XCB;
        DOMAIN_ERROR;
#endif
      }
   else if (w_props->get_gui_driver() == "WIN32")    // WIN32 requested
      {
#if not MINGW_SRC
        MORE_ERROR() << "A ⎕PLOT B: gui_driver=WIN32, but this build "
                        "was not compiled for a Windows/MinGW target.";
        DOMAIN_ERROR;
#endif
      }
   else if (w_props->get_gui_driver() == "ASCII" ||      // ASCII requested
            (w_props->get_gui_driver() == "" &&           // no driver requested
             default_plot_driver == PltDrv_ASCII))        // and no GUI available
      {
        Value_P Z = do_plot_ASCII(*w_props, *data);
        delete w_props;
        return Token(TOK_APL_VALUE2, Z);
      }
   else if (w_props->get_gui_driver() != "")
      {
        MORE_ERROR() << "A ⎕PLOT B: invalid gui_driver "
                     << w_props->get_gui_driver().c_str();
        DOMAIN_ERROR;
      }
   // empty gui_driver with GTK/XCB available → fall through to do_plot_data()

   // from here on we use GTK or XCB...

   // do_plot_data takes ownership of w_props and will delete w_props
   //
const APL_Integer Z = do_plot_data(w_props, data);
   return Token(TOK_APL_VALUE2, IntScalar(Z, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_PLOT::eval_B(cValue_R B) const
{
   CHECK_SECURITY(disable_Quad_PLOT);

   if (B.get_rank() == 0 && !B.is_pointer_cell(0))
      {
        // scalar (integer) argument: window control and logging
        //
        Cell cache;
        const APL_Integer B0 = B.get_cscalar(cache).get_int_value();
        Value_P Z = window_control(B0);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (B.get_rank() == 1 && B.element_count() == 0)
      {
        help();
        return Token(TOK_APL_VALUE1, Idx0(LOC));
      }

   if (B.get_rank() > 3)        RANK_ERROR;
   if (B.element_count() < 2)   LENGTH_ERROR;

   // plot window with default attributes
   //
Plot_data * data = setup_data(B);
   if (data == 0)   DOMAIN_ERROR;

Plot_window_properties * w_props = new Plot_window_properties(data, verbosity);

   if (default_plot_driver == PltDrv_ASCII)     // no GUI available
      {
        Value_P Z = do_plot_ASCII(*w_props, *data);
        delete w_props;
        return Token(TOK_APL_VALUE2, Z);
      }

const APL_Integer Z = do_plot_data(w_props, data);
   return Token(TOK_APL_VALUE2, IntScalar(Z, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_PLOT::window_control(APL_Integer B0) const
{
   // scalar (integer) argument: plot window control
   //
   if (B0 == 0)                 // reset plot verbosity
      {
        verbosity = 0;
        CERR << "⎕PLOT verbosity turned off" << endl;
        return Idx0(LOC);
      }

   if (B0 == -1)                // enable SHOW_EVENTS
      {
        verbosity |= SHOW_EVENTS;
        CERR << "⎕PLOT will show X events " << endl;
        return Idx0(LOC);
      }

   if (B0 == -2)                // enable SHOW_DATA
      {
        verbosity |= SHOW_DATA;
        CERR << "⎕PLOT will  show APL data (and debug infos)" << endl;
        return Idx0(LOC);
      }

   if (B0 == -3)   // close all ⎕PLOT windows, return their handles
      {
        Value_P Z = window_control(-6);   // get all open handles, see below

         // all_PLOT_windows is also touched by PLOT_context::remove_handle()
         // (called from the "destroy" signal handler, i.e. on the GTK/XCB
         // driver's own thread once plot_stop() actually closes a window --
         // plot_stop() itself no longer closes synchronously, see its own
         // comment). Protect our own read+clear with the same semaphore
         // the single-handle close path below already uses, so the two
         // don't race on the vector.
         //
         sem_wait_safe_I(all_PLOT_windows_sema, "the ⎕PLOT window list lock "
                         "(close all windows)");
         loop(h, all_PLOT_windows.size())
            {
              all_PLOT_windows[h]->plot_stop();
            }

         all_PLOT_windows.clear();
         sem_post(all_PLOT_windows_sema);

         next_handle = 0;
         Z->check_value(LOC);
         return Z;
       }

    if (B0 == -4)                // enable SHOW_DRAW
       {
         verbosity |= SHOW_DRAW;
         CERR << "⎕PLOT will  show rendering details " << endl;
         return Idx0(LOC);
       }

    if (B0 == -6)                // (sorted) list of handles
       {
         Value_P Z(all_PLOT_windows.size(), LOC);
         for (int offset = 0; Z->more(); offset += 64)
             {
               uint64_t bits = 0;
               loop(h, all_PLOT_windows.size())
                   {
                     const int handle = all_PLOT_windows[h]->handle;
                     const int bit = handle - offset;
                     if (bit >= 0 && bit < 64)   bits |= 1ULL << bit;
                   }

               loop(bit, 64)
                   {
                     if (bits & 1ULL << bit)   Z->next_ravel_Int(offset+bit);
                   }
             }
         Z->check_value(LOC);
         return Z;
       }

   // close one plot window...
   //
bool found = false;

   sem_wait_safe_I(all_PLOT_windows_sema, "the ⎕PLOT window list lock "
                   "(close one window)");
       loop(w, Quad_PLOT::all_PLOT_windows.size())
           {
             if (all_PLOT_windows[w]->handle != B0)   continue;

             all_PLOT_windows[w]->plot_stop();
             all_PLOT_windows[w] = all_PLOT_windows.back();
             all_PLOT_windows.pop_back();
             found = true;
             break;
           }
   sem_post(all_PLOT_windows_sema);

   if (all_PLOT_windows.size() == 0)   next_handle = 0;   // restart numbering

   return IntScalar(found ? B0 : 0, LOC);
}
//────────────────────────────────────────────────────────────────────────────
static UCS_string
fill_14(const std::string & name)
{
UTF8_string name_utf(name.c_str());
UCS_string ret(name_utf);
   while (ret.size() < 14)   ret << UNI_SPACE;
   return ret;
}

void
Quad_PLOT::help()
{
   CERR <<
"\n"
"   ⎕PLOT Usage:\n"
"\n"
"   ⎕PLOT B     with ⍴⍴B > 0: plot B with default attribute values\n"
"   ⎕PLOT B     with integer scalar B: special ⎕PLOT functions\n"
"   A ⎕PLOT B   plot B with attribute overrides specified by A\n"
"           ├────────  0: verbosity OFF\n"
"           ├──────── ¯1: show X events\n"
"           ├──────── ¯2: show data\n"
"           ├──────── ¯3: close all ⎕PLOT windows\n"
"           ├──────── ¯4: show rendering\n"
"           ├──────── ¯6: show open ⎕PLOT windows\n"
"           └───── N > 0: close ⎕PLOT window N\n"
"\n"
"   A is a nested vector of strings.\n"
"   Each string A[i] has the form \"Attribute: Value\"\n"
"   Colors are specified either as #RGB or as #RRGGBB or as RR GG BB)\n"
"\n"
"   The attributes understood by ⎕PLOT and their default values are:\n"
"\n"
"   1. Global (plot window) Attributes:\n"
"\n";

   CERR << left;

# define gdef(ty,  na,  val, descr)                                        \
   CERR << setw(20) << #na ":  " << fill_14(Plot_data::ty ## _to_str(val)) \
        << " (" << descr << ")" << endl;
# include "Quad_PLOT.def"

   CERR <<
"\n"
"color_level-P:      (none)         "
                    "(color gradient at P% (surface plots only))\n"
"\n"
"   2. Local (plot line N) Attributes:\n"
"\n";

# define ldef(ty,  na,  val, descr)             \
   CERR << setw(20) << #na "-N:  " << setw(14) \
        << Plot_data::ty ## _to_str(val) << " (" << descr << ")" << endl;
# include "Quad_PLOT.def"

   CERR << reset_format;
}
//────────────────────────────────────────────────────────────────────────────
// the ⎕PLOT workhorse
APL_Integer
Quad_PLOT::do_plot_data(Plot_window_properties * w_props,
                        const Plot_data * data)
{
   w_props->set_verbosity(verbosity);
   verbosity > 0 && w_props->print(CERR);

   // check (possibly again) for empty plot ranges which could be caused
   // by bad plot data but also by bad window properties.
   //
   if (w_props->get_min_X() >= w_props->get_max_X())
      {
        MORE_ERROR() << "A ⎕PLOT B: empty X range in A.";
        DOMAIN_ERROR;
      }
   if (w_props->get_min_Y() >= w_props->get_max_Y())
      {
        MORE_ERROR() << "A ⎕PLOT B: empty Y range in A.";
        DOMAIN_ERROR;
      }

   if (w_props->get_min_Z() >= w_props->get_max_Z())
      {
        MORE_ERROR() << "A ⎕PLOT B: eZ range in A.";
        DOMAIN_ERROR;
      }

const APL_Integer Z = ++next_handle;
   sem_wait_safe_I(all_PLOT_windows_sema, "the ⎕PLOT window list lock "
                   "(new window)");
       // start_GUI() throws DOMAIN_ERROR on several paths (invalid/missing
       // gui_driver, and -- since 2026-08-09, Bill Heagy, /tmp/trouble.eml --
       // a failed pthread_create() for the GTK/XCB event-loop thread). Any
       // of those must still release all_PLOT_windows_sema on the way out,
       // or every later ⎕PLOT (including "⎕PLOT ¯3", close all windows)
       // deadlocks forever on the next sem_wait_safe(all_PLOT_windows_sema)
       // -- confirmed live via the pthread_create fix above.
       //
       try
          {
            start_GUI(w_props, Z, PltDrv_GTK);
          }
       catch (...)
          {
            sem_post(all_PLOT_windows_sema);
            throw;
          }
   sem_post(all_PLOT_windows_sema);

   if (w_props->get_with_border())
      {
        // the user wants window borders, which requires a delay (of 100 ms,
        // see WM_timeout() in Event_handler 'Draw' in Plot_gtk.cc). We add
        // twice that timeout at ⎕PLOT level so that the next ⎕PLOT (if any)
        // does not interfer with the saving of the current plot window.
        //
        usleep(2000 * SAVE_BORDER_DELAY_ms);
      }

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Plot_data *
Quad_PLOT::setup_data(const cValue & B)
{
   /** check data. We expect B to be either:

       1a. a numeric vector (for a single plot line), or
       1b. a numeric matrix (for one plot line for each row of the matrix), or
       2a. a scalar of a nested numeric vector (for a single plot line), or
       2b. a vector of nested numeric vectors (for one plot line per item)
       3.  a 3-dimensional real vector for surface plots
    **/

const cValue * pB = &B;
   if (B.is_scalar())   // case 2a. → 1a. by disclosing scalar B
      {
        if (!B.is_pointer_cell(0))   DOMAIN_ERROR;
        pB = B.get_pointer_value(0).get();   // B ← ⊃ B
      }

   if (pB->get_rank() == 3)                   return setup_data_3D(*pB);
   if (!pB->is_pointer_cell(0))   return setup_data_2D(*pB);
   return setup_data_2D_2b(*pB);
}
//────────────────────────────────────────────────────────────────────────────
Plot_data *
Quad_PLOT::setup_data_3D(const cValue & B)
{
   /** initialize the data for a 3D (surface-) plot. B is the right argument
       of ⎕PLOT B or A ⎕PLOT B and can be:

       1. A single nested array Y. In this case the X and Z coordintes
          are derived from ⍴Y and run from X=0...cols and from Z=0 to rows.
          The double * data contains only the values from Y (height).

          The double * Y contains the values from for Y (height):

           Y
           ↓
          ┌───────────────┐
          │ Y-coordinates │
          └───────────────┘
          │← data_points →│


       2. Two nested arrays X and Y. In this case the Z coordinate is derived
          from ⍴Y and run from Z=0 to rows.

          The double * X contains the values from X, followed by the values
          for Y (height):

           X               Y               Z
           ↓               ↓              
          ┌───────────────┬───────────────┐
          │ X-coordinates │ Y-coordinates │
          └───────────────┴───────────────┘
          │← data_points →│← data_points →│


       3. B← (⊂X) (⊂Y) (⊂Z). This indicates a 3D aka, surface plot.
          All coordinates come from B and the double * X contains
          Y and Z like this:

           X               Y               Z
           ↓               ↓               ↓
          ┌───────────────┬───────────────┬───────────────┐
          │ X-coordinates │ Y-coordinates │ Z-coordinates │
          └───────────────┴───────────────┴───────────────┘
          │← data_points →│← data_points →│← data_points →│

    **/

const ShapeItem planes = B.get_shape_item(0);   // (X), Y, and (Z)
const ShapeItem rows   = B.get_shape_item(1);
const ShapeItem cols   = B.get_shape_item(2);

   if (planes < 1 || planes > 3)    LENGTH_ERROR;
   if (rows < 1)                    LENGTH_ERROR;
   if (cols < 1)                    LENGTH_ERROR;

Plot_data * data = 0;

const ShapeItem data_points = rows * cols;

   if (planes == 3)   // X, Y, and Z
      {
        // validate every cell before allocating anything below (mirroring
        // setup_data_2D()): X/Y/Z/data are raw pointers with no owner, so
        // a DOMAIN_ERROR thrown mid-loop after they were allocated would
        // leak them.
        loop(p, data_points)
            {
              if (!B.is_integer_cell(p) && !B.is_real_cell(p))
                 DOMAIN_ERROR;
              if (!B.is_integer_cell(p + data_points) &&
                  !B.is_real_cell(p + data_points))
                 DOMAIN_ERROR;
              if (!B.is_integer_cell(p + 2*data_points) &&
                  !B.is_real_cell(p + 2*data_points))
                 DOMAIN_ERROR;
            }

        if ((size_t)data_points > SIZE_MAX / 3)   WS_FULL;
        double * X = new double[3*data_points];
        double * Y = X + data_points;
        double * Z = Y + data_points;

        data = new Plot_data(rows);
Cell cX_cache, cY_cache, cZ_cache;
        loop(r, rows)
            {
              loop(c, cols)
                  {
                    const ShapeItem p = c + r*cols;

                    // NOT the no-cache get_cravel(): for a packed B (e.g.
                    // homogeneous numeric 3-plane plot data), all three
                    // no-cache fetches would materialise into B's single
                    // shared ravel.cell_fetch_cache -- cY's fetch would
                    // silently invalidate cX, and cZ's would invalidate
                    // both, so cX/cY/cZ (and hence X[p]/Y[p]/Z[p] below)
                    // would all end up reading cZ's value. Confirmed live
                    // (3 1 1000⍴(1000+⍳1000),(2000+⍳1000),(3000+⍳1000) --
                    // large enough to pack -- gave X[p]==Y[p]==Z[p], all
                    // three equal to the Z coordinate) before this fix.
                    //
                    const Cell & cX = B.get_cravel(p, cX_cache);
                    const Cell & cY = B.get_cravel(p +   data_points, cY_cache);
                    const Cell & cZ = B.get_cravel(p + 2*data_points, cZ_cache);

                    if (!(cX.is_integer_cell() ||
                          cX.is_real_cell()))   DOMAIN_ERROR;
                    if (!(cY.is_integer_cell() ||
                          cY.is_real_cell()))   DOMAIN_ERROR;
                    if (!(cZ.is_integer_cell() ||
                          cZ.is_real_cell()))   DOMAIN_ERROR;

                    X[p] = cX.get_real_value();
                    Y[p] = cY.get_real_value();
                    Z[p] = cZ.get_real_value();
                  }
                   const double * pX = X + r*cols;
                   const double * pY = Y + r*cols;
                   const double * pZ = Z + r*cols;
                   const Plot_data_row * pdr = new Plot_data_row(pX, pY, pZ,
                                                                 r, cols);
                   data->add_row(pdr);
            }
      }
   else if (planes == 2)   // X and Y, but no Z
      {
        // validate first -- see the planes==3 comment above.
        loop(p, data_points)
            {
              if (!B.is_integer_cell(p) && !B.is_real_cell(p))
                 DOMAIN_ERROR;
              if (!B.is_integer_cell(p + data_points) &&
                  !B.is_real_cell(p + data_points))
                 DOMAIN_ERROR;
            }

        if ((size_t)data_points > SIZE_MAX / 2)   WS_FULL;
        double * X = new double[2*data_points];
        double * Y = X + data_points;

        data = new Plot_data(rows);
Cell cX_cache, cY_cache;
        loop(r, rows)
            {
              loop(c, cols)
                  {
                    const ShapeItem p = c + r*cols;

                    // see the planes==3 case above: NOT the no-cache
                    // get_cravel() -- same B-is-packed hazard.
                    //
                    const Cell & cX = B.get_cravel(p, cX_cache);
                    const Cell & cY = B.get_cravel(p + data_points, cY_cache);

                    if (!(cX.is_integer_cell() ||
                    cX.is_real_cell()))   DOMAIN_ERROR;
                    if (!(cY.is_integer_cell() ||
                               cY.is_real_cell()))   DOMAIN_ERROR;

                    X[p] = cX.get_real_value();
                    Y[p] = cY.get_real_value();
                  }
              const double * pX = X + r*cols;
              const double * pY = Y + r*cols;
              const Plot_data_row * pdr = new Plot_data_row(pX, pY, 0, r, cols);
              data->add_row(pdr);
            }
      }
   else                    // Y, but no X and no Z
      {
        // validate first -- see the planes==3 comment above.
        loop(p, data_points)
            {
              if (!B.is_integer_cell(p) && !B.is_real_cell(p))
                 DOMAIN_ERROR;
            }

        double * Y = new double[data_points];

        data = new Plot_data(rows);
        loop(r, rows)
            {
              loop(c, cols)
                  {
                    const ShapeItem p = c + r*cols;

                    if (!(B.is_integer_cell(p) ||
                          B.is_real_cell(p)))   DOMAIN_ERROR;

                    Y[p] = B.get_real_value(p);
                  }
              const double * pY = Y + r*cols;
              const Plot_data_row * pdr = new Plot_data_row(0, pY, 0, r, cols);
              data->add_row(pdr);
            }
      }

   data->surface = true;
   return data;
}
//────────────────────────────────────────────────────────────────────────────
Plot_data *
Quad_PLOT::setup_data_2D(const cValue & B)
{
   /** initialize the data for a 2D plot. B is the right argument
       of ⎕PLOT B or A ⎕PLOT B and contains the plot coordinates of
       the plot points as simple numeric values. Every B[line;] is one
       plot line.

       A complex number xJy means 2D plot coordinates (x, y) while a
       real number x means 2D plot coordinates (N, y) where N is the
       position of x in B[d;];

       The third dimension Z is set to Y although not used.
    **/

const ShapeItem cols_B = B.get_cols();
const ShapeItem rows_B = B.get_rows();
const ShapeItem len_B = rows_B * cols_B;

   // all items of B shall be simple numbers (integer, real, or complex).
   loop(b, len_B)
       {
         if (!B.is_numeric(b))   return 0;
       }

   // split B into X=real B, Y=imag Y

if ((size_t)len_B > SIZE_MAX / 3)   WS_FULL;
double * X = new double[3*len_B];
double * Y = X + len_B;
double * Z = Y + len_B;

const APL_Integer qio = Workspace::get_IO();
   loop(b, len_B)
       {
         if (B.is_complex_cell(b))   // (x, y)
            {
              X[b] = B.get_real_value(b);
              Z[b] = Y[b] = B.get_imag_value(b);
            }
         else                        // (N, y)
            {
              X[b] = qio + b % cols_B;
              Z[b] = Y[b] = B.get_real_value(b);
            }
       }

Plot_data * data = new Plot_data(rows_B);
   loop(r, rows_B)
       {
         const double * pX = X + r*cols_B;
         const double * pY = Y + r*cols_B;
         const double * pZ = Z + r*cols_B;
         const Plot_data_row * pdr = new Plot_data_row(pX, pY, pZ, r, cols_B);
         data->add_row(pdr);
       }

   return data;
}
//════════════════════════════════════════════════════════════════════════════
Plot_data *
Quad_PLOT::setup_data_2D_2b(const cValue & B)
{
   /** initialize the data for a 2D plot. B is the right argument
       of ⎕PLOT B or A ⎕PLOT B and contains the plot lines as nested
       values. Every ⊃B[line] is one plot line.

       The third dimension Z is set to Y although not used.
    **/

ShapeItem data_points = 0;
   if (B.get_rank() > 1)   RANK_ERROR;
const ShapeItem rows = B.element_count();   // number of plot rows
   loop(r, rows)
       {
         const cValue * vrow = B.get_pointer_value(r).get();
         if (vrow->get_rank() > 1)   RANK_ERROR;
         const ShapeItem row_len = vrow->element_count();
         data_points += row_len;
         loop(rb, row_len)
             {
               if (!vrow->is_numeric(rb))   DOMAIN_ERROR;
             }
       }

if ((size_t)data_points > SIZE_MAX / 3)   WS_FULL;
double * X = new double[3*data_points];
double * Y = X + data_points;
double * Z = Y + data_points;

ShapeItem idx = 0;
const APL_Integer qio = Workspace::get_IO();

   loop(r, rows)
       {
         const cValue * vrow = B.get_pointer_value(r).get();
         Cell cache;
         loop(v, vrow->element_count())
             {
               const Cell & cB = vrow->get_cravel(v, cache);
               if (cB.is_complex_cell())
                  {
                    X[idx] = cB.get_real_value();
                    Z[idx] = Y[idx] = cB.get_imag_value();
                  }
               else
                  {
                    X[idx] = qio + v;
                    Z[idx] = Y[idx] = cB.get_real_value();
                  }
               ++idx;
             }
       }

Plot_data * data = new Plot_data(rows);
   idx = 0;
   loop(r, rows)
       {
         const double * pX = X + idx;
         const double * pY = Y + idx;
         const double * pZ = Z + idx;
         const cValue * vrow = B.get_pointer_value(r).get();
         const ShapeItem row_len = vrow->element_count();
         const Plot_data_row * pdr = new Plot_data_row(pX, pY, pZ, r,
                                                            row_len);
         data->add_row(pdr);
         idx += row_len;
       }

   return data;
}
//════════════════════════════════════════════════════════════════════════════
ErrorCode
Quad_PLOT::parse_attributes(const cValue & A, Plot_window_properties * w_props)
{
   if (A.is_member())   // new-style attributes
      {
        loop(row, A.get_rows())
            {
              Cell cache_name, cache_val;
              const Cell & att_name = A.get_cravel(2*row, cache_name);     // A[row; 1]
              const Cell & att_val  = A.get_cravel(2*row + 1, cache_val); // A[row; 2]
              if (Value_P v = att_name.try_pointer_value())   // used entry in A
                 {
                   const UCS_string ucs = v->get_UCS_ravel();
                   if (const char * error = w_props->set_attribute(ucs, att_val))
                      {
                        MORE_ERROR() << "A ⎕PLOT B: " << error
                                     << " in attribute A." << ucs;
                        return E_DOMAIN_ERROR;
                      }
                 }
            }
        return E_NO_ERROR;
      }

   // old-style attributes
   //
   if (A.get_rank() > 1)   { delete w_props;   RANK_ERROR; }

const ShapeItem len_A = A.element_count();
   if (len_A < 1)   return E_LENGTH_ERROR;

const APL_Integer qio = Workspace::get_IO();

   loop(a, len_A)
       {
         if (!A.is_pointer_cell(a))
            {
               MORE_ERROR() << "A[" << (a + qio)
                            << "] is not a string in A ⎕PLOT B";
               return E_DOMAIN_ERROR;
            }

         const cValue * attr = A.get_pointer_value(a).get();
         if (!attr->is_char_string())
            {
               MORE_ERROR() << "A[" << (a + qio)
                            << "] is not a string in A ⎕PLOT B";
               return E_DOMAIN_ERROR;
            }

         UCS_string ucs = attr->get_UCS_ravel();
         ucs.remove_leading_and_trailing_whitespaces();
         if (ucs.size() == 0)               continue;
         if (Avec::is_comment(ucs[0]))      continue;
         UTF8_string utf(ucs);
         if (const char * error = w_props->set_attribute(utf.c_str()))
            {
              MORE_ERROR() << error << " in ⎕PLOT attribute '" << ucs << "'";
              return E_DOMAIN_ERROR;
            }
       }

   return E_NO_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
