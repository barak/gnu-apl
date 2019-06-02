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

/// The class implementing ⎕PLOT
class Quad_PLOT : public QuadFunction
{
public:
   /// Constructor.
   Quad_PLOT();

   /// Denstructor.
   ~Quad_PLOT();

   static Quad_PLOT * fun;          ///< Built-in function.
   static Quad_PLOT  _fun;          ///< Built-in function.

   /// a semaphore protecting plot_threads
   static sem_t * plot_threads_sema;

   /// an array of threads (one per plot window) handling X events from the
   /// window
   static std::vector<pthread_t> plot_threads;

protected:
   /// overloaded Function::eval_AB()
   Token eval_AB(Value_P A, Value_P B);

   /// overloaded Function::eval_B()
   Token eval_B(Value_P B);

   /// print attribute help text
   void help() const;

   /// plot the data (creating a new plot window in X)
   Value_P plot_data(Plot_window_properties * w_props,
                     const Plot_data * data);

   /// initialize the data to be plotted
   Plot_data * setup_data(const Value * B);

   /// whether to print some debug info during plotting
   int verbosity;
};

#endif // __Quad_PLOT_DEFINED__
