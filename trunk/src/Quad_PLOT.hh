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

#include <math.h>
#include <pthread.h>
#include <semaphore.h>

#include "QuadFunction.hh"
#include "Value.hh"

class Plot_window_properties;
class Plot_data;

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

    default_plot_driver = PltDrv_GTK    if GTK is avaliable, otherwise
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

   enum Plot_driver
      {
        PltDrv_GTK   = 1,
        PltDrv_XCB   = 2,
        PltDrv_ASCII = 3,

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
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const;

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
   static Plot_data * setup_data(const Value & B);

   /// initialize the data to be plotted for a 3D plot
   static Plot_data * setup_data_3D(const Value & B);

   /// initialize the data to be plotted for a 2D plot (except case 2b.)
   static Plot_data * setup_data_2D(const Value & B);

   /// initialize the data to be plotted for a 2D plot (case 2b.)
   static Plot_data * setup_data_2D_2b(const Value & B);

   /// parse the (all-optional) attributes in A
   static ErrorCode parse_attributes(const Value & A,
                                     Plot_window_properties * w_props);

   /// whether to print some debug info during plotting
   static int verbosity;
};

#endif // __Quad_PLOT_DEFINED__
