/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

#ifndef __Quad_FFT_DEFINED__
#define __Quad_FFT_DEFINED__

#include <math.h>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#include "QuadFunction.hh"
#include "Value.hh"

/// The class implementing ⎕FFT
class Quad_FFT : public QuadFunction
{
public:
   /// Constructor.
   Quad_FFT();

   static Quad_FFT fun;   ///< Built-in function.

protected:
   /// overloaded FunctionGroup::print_fun_syntax()
   /// @param out output stream to write to
   /// @param info descriptor for the sub-function being printed
   virtual void print_fun_syntax(ostream & out,
                                 const function_info & info) const;

   /// overloaded FunctionGroup::print_map_syntax()
   /// @param out output stream to write to
   /// @param info descriptor for the sub-function being printed
   virtual void print_map_syntax(ostream & out,
                                 const function_info & info) const;

   /// overloaded Function::eval_AB()
   /// @param A left-argument APL value (FFT mode selector)
   /// @param B right-argument APL value (input signal)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B()
   /// @param B right-argument APL value (input signal)
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_XB().
   /// ⎕FFT[X] B  ←→  X ⎕FFT B
   /// @param X axis/mode specification
   /// @param B right-argument APL value (input signal)
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// compute FFT with mode \b A of B
   /// @param A_or_X APL value specifying the FFT mode or axis
   /// @param B right-argument APL value (input signal)
   Token do_eval_AorX_B(const Value & A_or_X, Value_P B) const;

   /// window function for sample n of N with parameters a = a0, a1, ...
   typedef double (*window_function)(ShapeItem n, ShapeItem N);

   /// compute forward or backward FFT
   /// @param dir transform direction (+1 forward, -1 backward)
   /// @param B right-argument APL value (input signal)
   /// @param win window function applied to samples before transform
   static Token do_fft(int dir, Value_P B, window_function win);

   /// return the values of the window function \b win for length \b N
   /// @param B right-argument APL value providing the sample length
   /// @param win window function to evaluate
   static Token do_window(Value_P B, window_function win);

   /// initialize \b in from B
   /// @param in fftw input buffer to populate
   /// @param B right-argument APL value (input signal samples)
   /// @param win window function applied to each sample
   static void init_in(void * in, Value_P B, window_function win);

   /// return the no window value for sample n of N
   /// @param n sample index (0-based)
   /// @param N total number of samples
   static double no_window(ShapeItem n, ShapeItem N)
      { return 1.0; }

   /// return the Hann window value for sample n of N
   /// @param n sample index (0-based)
   /// @param N total number of samples
   static double hann_window(ShapeItem n, ShapeItem N)
      { return 0.5 - 0.5*cos(2*n*M_PI / (N-1)); }

   /// return the Hamming window value for sample n of N
   /// @param n sample index (0-based)
   /// @param N total number of samples
   static double hamming_window(ShapeItem n, ShapeItem N)
      { return 0.54 - 0.46*cos(2*n*M_PI / (N-1)); }

   /// return the Hann window value for sample n of N
   /// @param n sample index (0-based)
   /// @param N total number of samples
   static double blackman_window(ShapeItem n, ShapeItem N)
      { return 0.42 - 0.5*cos(2*n*M_PI / (N-1)) + 0.08*cos(4*M_PI*n / (N-1)); }

   /// return the Blackman-Harris window value for sample n of N
   /// @param n sample index (0-based)
   /// @param N total number of samples
   static double blackman_harris_window(ShapeItem n, ShapeItem N)
      { return 0.35875
             - 0.48829*cos(2*n*M_PI / (N-1))
             + 0.14128*cos(4*n*M_PI / (N-1))
             - 0.01168*cos(6*n*M_PI / (N-1)); }

   /// return the Blackman-Nuttallwindow value for sample n of N
   /// @param n sample index (0-based)
   /// @param N total number of samples
   static double blackman_nuttall_window(ShapeItem n, ShapeItem N)
      { return 0.3635819
             - 0.4891775*cos(2*n*M_PI / (N-1))
             + 0.1365995*cos(4*n*M_PI / (N-1))
             - 0.0106411*cos(6*n*M_PI / (N-1)); }

   /// return the Flat-Top window value for sample n of N
   /// @param n sample index (0-based)
   /// @param N total number of samples
   static double flat_top(ShapeItem n, ShapeItem N)
      { return 1.0
             - 1.93 *cos(2*n*M_PI / (N-1))
             + 1.29 *cos(4*n*M_PI / (N-1))
             - 0.388*cos(6*n*M_PI / (N-1))
             + 0.028*cos(8*n*M_PI / (N-1)); }

   /// set up a multi-dimensional window for shape sh, using the window
   /// function \b win
   /// @param result output buffer for window coefficients
   /// @param shape shape of the multi-dimensional input array
   /// @param win window function to evaluate at each sample position
   static void fill_window(double * result, const Shape & shape,
                           window_function win);

   /// true if fftw_import_system_wisdom() was called
   static bool system_wisdom_loaded;

   /// a mapping between function names and function numbers
   static const FunctionGroup::function_info subfunction_infos[];
};

#endif // __Quad_FFT_DEFINED__
