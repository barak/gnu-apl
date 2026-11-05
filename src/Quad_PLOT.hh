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

#ifndef __Quad_PLOT_DEFINED__
#define __Quad_PLOT_DEFINED__

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>

#include "Common.hh"
#include "QuadFunction.hh"
#include "Sys.hh"
#include "Value.hh"

class Plot_window_properties;
class Plot_data;

/// how long sem_wait_safe() waits for a ⎕PLOT semaphore before giving up.
/// All of the waits it guards (GTK/XCB event loop startup, a plot window
/// being shown, a quick lock/unlock of all_PLOT_windows) normally take
/// well under a second even on a slow (e.g. 32-bit) system; this is
/// generous enough to never fire under legitimate load, while still
/// turning a genuine hang (Bill Heagy, 2026-08-09, /tmp/trouble.eml --
/// on his system the underlying wait was never satisfied at all, for
/// reasons unrelated to speed) into a bounded, reportable failure
/// instead of an indefinite one that only ^C could escape.
enum { PLOT_SEM_TIMEOUT_SECONDS = 15 };

/// like Sys::sem_wait_safe(), but for call sites that run on the
/// interpreter thread (i.e. reached directly from Quad_PLOT::eval_AB()/
/// eval_B(), never from a driver callback thread): raise a DOMAIN_ERROR
/// with a MORE_ERROR() hint instead of silently returning on ^C/timeout.
/// Must NOT be used from a GTK/XCB driver-owned thread -- throwing a
/// C++/APL exception there is not safe, see Sys::sem_wait_safe() for why.
inline void
sem_wait_safe_I(sem_t * sema, const char * what)
{
const Sys::Wait_result result =
      Sys::sem_wait_safe(sema, what, PLOT_SEM_TIMEOUT_SECONDS);
   if (result == Sys::WAIT_OK)   return;

   switch (result)
      {
        case Sys::WAIT_CTRL_C:
             MORE_ERROR() << "A ⎕PLOT B: ^C was needed to escape "
                             "sem_timedwait() while waiting for " << what
                          << ". Like a timeout, this normally means a "
                             "⎕PLOT driver thread got stuck or died "
                             "unexpectedly -- ^C only ended the wait "
                             "sooner than the " << PLOT_SEM_TIMEOUT_SECONDS
                          << "s timeout would have.";
             break;

        case Sys::WAIT_TIMEOUT:
             MORE_ERROR() << "A ⎕PLOT B: timed out after "
                          << PLOT_SEM_TIMEOUT_SECONDS << "s while waiting "
                             "for " << what << ". This normally means a "
                             "⎕PLOT driver thread got stuck or died "
                             "unexpectedly.";
             break;

        default:
             MORE_ERROR() << "A ⎕PLOT B: unexpected internal error while "
                             "waiting for " << what << " (see the *** "
                             "Sys::sem_wait_safe(): ... message on stderr "
                             "for detail).";
             break;
      }
   DOMAIN_ERROR;
}

/// ⎕PLOT verbosity bitmap
enum
{
   SHOW_EVENTS = 1,   ///< show X events
   SHOW_DATA   = 2,   ///< show APL data
   SHOW_DRAW   = 4,   ///< show draw details
};

/// a shrt delay needed when the user wants window borders to be saved
//  into a .png file
enum { SAVE_BORDER_DELAY_ms = 100 };

//════════════════════════════════════════════════════════════════════════════
/** The class implementing ⎕PLOT. \b Quad_PLOT implements the APL
    side of plotting, while the actual output work is performed by
    plot drivers for different GUI environments (X11/XCB or GTK).

    Class \b Quad_PLOT handles the APL aspects of plotting n GNU APL.
    It is accompanied by currently 3 platform dependent driver files
    that do the actual output work:

    1. Plot_ascii.cc (always used and linked in Makefile),
    2. Plot_gtk.cc   (used and linked if apl_GTK3 was set by ./configure), and
    3. Plot_XCB      (used and linked if apl_CCB  was set by ./configure).

    Plot_ascii runs in the same thread as the GNU APL interpreter. It plots
    to stderr and returns a committed value with 3 planes for characters,
    VT100 foreground colors, and VT100 background colors.

    Plot_gtk.cc uses a separate phtread for every plot window. It returns an
    integer window handle which ⎕PLOT case use to manipulate the window (e.g
    to close it from APL).

    Plot_XCB uses a single phtread for all plot windows. It returns an
    integer window handle which ⎕PLOT case use to manipulate the window (e.g
    to close it from APL).

    PLOT synchronizes with the driver thread(s) of GTK or XCB by means of
    two mutuial exclusion semaphores:

    1. all_PLOT_windows_sema: a process semaphore; initially 1, that protects
       the Quad_PLOT::all_PLOT_windows data structure. Used (only by XCB) to
       remove a window from all_PLOT_windows (which could happen simultaneous
       from ⎕PLOT in APL and from an XCB_CLIENT_MESSAGE in XCB. Whoever aquires
       it is allowed change Quad_PLOT::all_PLOT_windows.

    2. expose_sema: a threads semaphore; initially 0, that tells the interpreter
       when a plot window was exposed (shown). Posted by GTK and XCB after the
       plot window was shown. At this point has finishied drawing the initial
       plot window and only reacts to (mouse-) events like window resize,
       window close, etc. It is also the point where Quad_PLOT::start_GUI()
       (and therefore also eval_AB() rersp. eval_B() return. This is to make
       eval_AB() rersp. eval_B() kind of atomic operations.

    A \b default_plot_driver is chosen at compile time based on the
    ./configure result (i.e. \b config.h).

    default_plot_driver = PltDrv_GTK    if GTK AND X11 are avaliable, otherwise
    default_plot_driver = PltDrv_XCB    if XCB is avaiable, otherwise
    PltDrv_ASCII        = PltDrv_ASCII.

   \b eval_B() (which has no plot attributes) always uses \b default_plot_driver.
   \b eval_AB() uses \b default_plot_driver unless a differen driver is
   specified in plot attribute \b w_props->get_gui_driver().
 **/
class Quad_PLOT : public QuadFunction
{
public:
   /// Constructor.
   Quad_PLOT();

   /// a small number that identifies a window
   typedef int Handle;

   /// the GUI-independent part of ⎕PLOT (at APL level).
   class PLOT_context
      {
        public:
           /// constructor
           /// @param h unique window handle assigned to this context
           PLOT_context(Handle h)
           : handle(h)
           {}

           /// return the per-window thread (for GUIs that have one, i.e. XCB).
           virtual pthread_t get_thread() const
              { return 0; }

           /// close the plot window in the GUI
           virtual void plot_stop() = 0;

           /// destructor (shall clean up the GUI-dependent context)
           virtual ~PLOT_context() {}

           /// z unique number that identifies \b this PLOT_context (in
           /// \b all_PLOT_windows).
           const Handle handle;

           /// close \b hnadle from all_PLOT_windows
           /// @param handle window handle to remove from all_PLOT_windows
           static Handle remove_handle(Handle handle);   // GTK only
      };

   /// the GTK window that handles one plot window. Always declared here
   /// (to make doxygen happy, but only implemented if apl_GTK3
   /// @param vp_props pointer to the Plot_window_properties for this window
   /// @param handle window handle assigned to this plot window
   static void plot_main_GTK(void * vp_props, Handle handle);

   /// the pthread that handles all XCB plot windows. Always declared here
   /// (to make doxygen happy, but only implemented if NOT apl_GTK3
   /// @param vp_props pointer to the Plot_window_properties for this window
   static void * plot_main_XCB(void * vp_props);

   /// the native Windows (GDI+) window that handles one plot window.
   /// Always declared here (to make doxygen happy), but only implemented
   /// if MINGW_SRC. Spawns one worker pthread per plot window (mirroring
   /// plot_main_GTK's calling convention) that creates the HWND, draws
   /// into it, and runs its own message loop.
   /// @param vp_props pointer to the Plot_window_properties for this window
   /// @param handle window handle assigned to this plot window
   static void plot_main_WIN32(void * vp_props, Handle handle);

   enum Plot_driver
      {
        PltDrv_GTK   = 1,
        PltDrv_XCB   = 2,
        PltDrv_ASCII = 3,
        PltDrv_WIN32 = 4,

      };

   static int get_verbosity()
      { return verbosity; }

   static Quad_PLOT  fun;          ///< Built-in function.

   /// a semaphore blocking until the plot window has been EXPOSED
   static sem_t * expose_sema;

   /// a semaphore protecting vector all_PLOT_windows
   static sem_t * all_PLOT_windows_sema;

   /// the next handle (-1)
   static Handle next_handle;

   /// all open ⎕PLOT windows.
   static vector<PLOT_context *> all_PLOT_windows;

protected:
   /// Destructor.
   ~Quad_PLOT();

   /// initialize the GUI
   /// @param w_props plot window properties for the new window
   /// @param handle window handle assigned to this plot window
   /// @param driver_type GUI driver to use (GTK, XCB, or ASCII)
   static void start_GUI(Plot_window_properties * w_props, int handle,
                         Plot_driver driver_type);

   /// overloaded Function::eval_AB()
   /// @param A left argument APL value (plot attributes)
   /// @param B right argument APL value (data to plot)
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_B()
   virtual Token eval_B(cValue_R B) const;

   /// control logging etc. of ⎕PLOT
   Value_P window_control(APL_Integer B) const;

   /// print attribute help text
   static void help();

   /// plot the data (creating a new plot window in X)
   static APL_Integer do_plot_data(Plot_window_properties * w_props,
                                   const Plot_data * data);

   /// close the plot window with \b handle (from APL)
   static Handle plot_stop_APL(Handle handle);

   /// initialize the data to be plotted
   static Plot_data * setup_data(const cValue & B);

   /// initialize the data to be plotted for a 3D plot
   static Plot_data * setup_data_3D(const cValue & B);

   /// initialize the data to be plotted for a 2D plot (except case 2b.)
   static Plot_data * setup_data_2D(const cValue & B);

   /// initialize the data to be plotted for a 2D plot (case 2b.)
   static Plot_data * setup_data_2D_2b(const cValue & B);

   /// parse the (all-optional) attributes in A
   static ErrorCode parse_attributes(const cValue & A,
                                     Plot_window_properties * w_props);

   /// whether to print some debug info during plotting
   static int verbosity;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __Quad_PLOT_DEFINED__
