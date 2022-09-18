/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2018-2019  Dr. Jürgen Sauermann

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

#ifndef __Quad_PLOT_DEFINED__
#define __Quad_PLOT_DEFINED__

#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <vector>

#include "QuadFunction.hh"
#include "Value.hh"

class Plot_window_properties;
class Plot_data;

/// ⎕PLOT verbosity
enum
{
   SHOW_EVENTS = 1,   ///< show X events
   SHOW_DATA   = 2,   ///< show APL data
   SHOW_DRAW   = 4,   ///< show draw details
};

/// The class implementing ⎕PLOT
class Quad_PLOT : public QuadFunction
{
public:
   /// Constructor.
   Quad_PLOT();

   static Quad_PLOT * fun;          ///< Built-in function.
   static Quad_PLOT  _fun;          ///< Built-in function.

   /// a semaphore blocking until the plot window has been EXPOSED
   static sem_t * plot_window_sema;

   /// a semaphore protecting plot_threads
   static sem_t * plot_threads_sema;

   /// an array of threads (one per plot window) handling X events from the
   /// window
   static std::vector<pthread_t> plot_threads;

protected:
   /// Denstructor.
   ~Quad_PLOT();

   /// overloaded Function::eval_AB()
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B()
   virtual Token eval_B(Value_P B) const;

   /// print attribute help text
   static void help();

   /// plot the data (creating a new plot window in X)
   static Value_P do_plot_data(Plot_window_properties * w_props,
                               const Plot_data * data);

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
