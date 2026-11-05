/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2026  Dr. Jürgen Sauermann

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

#include <vector>

#include "FloatCell.hh"
#include "Quad_FFT.hh"
#include "Workspace.hh"

#include "Workspace.icc"

Quad_FFT   Quad_FFT::fun;

bool Quad_FFT::system_wisdom_loaded = false;

#if apl_FFT

#include <fftw3.h>
#include "ComplexCell.hh"

const FunctionGroup::function_info Quad_FFT::subfunction_infos[] =
{
#define fftdef(N, fun, comm) { N, #fun, "", comm, -1 },

  fftdef(-15,flat_top_window       ,"no FFT; Z is B × flat-top window"        )
  fftdef(-14,blackman_nuttal_window,"no FFT; Z is B × Blackman-Nuttal window" )
  fftdef(-13,blackman_harris_window, "no FFT; Z is B × Blackman-Harris window" )
  fftdef(-12,blackman_window       ,"no FFT; Z is B × Blackman window"        )
  fftdef(-11,hamming_window        ,"no FFT; Z is B × Hamming window"         )
  fftdef(-10,hann_window           ,"no FFT; Z is B × Hann window"            )
  fftdef( -1,inverse_fft           ,"Z is the inverse FFT of B"               )
  fftdef(  0,fft                   ,"Z is FFT without window; same as ⎕FFT B" )
  fftdef( 10,fft_hann              ,"Z is FFT with Hann window"               )
  fftdef( 11,fft_hamming           ,"Z is FFT with Hamming window"            )
  fftdef( 12,fft_blackman          ,"Z is FFT with Blackman window"           )
  fftdef( 13,fft_blackman_harris   ,"Z is FFT with Blackman-Harris window"    )
  fftdef( 14,fft_blackman_nuttal   ,"Z is FFT with Blackman-Nuttal window"    )
  fftdef( 15,fft_flat_top          ,"Z is FFT with flat-top window"           )
};

//════════════════════════════════════════════════════════════════════════════
Quad_FFT::Quad_FFT()
      : QuadFunction(TOK_Quad_FFT)
{
enum { count = sizeof(subfunction_infos) / sizeof(*subfunction_infos) };
   init_function_group(subfunction_infos, count, "⎕FFT");

   system_wisdom_loaded = false;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FFT::eval_AB(cValue_R A, cValue_R B) const
{
  return do_eval_AorX_B(A, CLONE(&B, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FFT::eval_B(cValue_R B) const
{
   if (B.element_count())   return do_fft(FFTW_FORWARD, CLONE(&B, LOC), 0);
   if (B.is_str0())         return list_functions(CERR);
   if (B.is_zilde())        return list_mappings(CERR);
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FFT::eval_XB(cValue_R X, cValue_R B) const
{
  return do_eval_AorX_B(X, CLONE(&B, LOC));
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_FFT::print_fun_syntax(ostream & out,
                           const function_info & info) const
{
const sRank axis = info.axis;
   out << "    Z ← ";
   if      (axis < -9)   out << "¯"  << -axis;
   else if (axis <  0)   out << " ¯" << -axis;
   else if (axis < 10)   out << "  " <<   axis;
   else                  out << " "  <<   axis;
   out<< " ⎕FFT B   ⍝ " << info.comment_fun << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_FFT::print_map_syntax(ostream & out,
                           const function_info & info) const
{
char NN[10];   SPRINTF(NN, "%3d", int(info.axis));
const char * name = info.function_name;
const UCS_string blanks(max_function_name_length - strlen(name), UNI_SPACE);

   out << "    " << NN << " ⎕FFT  ←→  ⎕FFT['" << name << "']"
       << blanks << "  ←→  ⎕FFT." << name << endl;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FFT::do_eval_AorX_B(const cValue & A_or_X, Value_P B) const
{
const sAxis subfunction = value_to_subfun(A_or_X);
   switch(subfunction)
      {
        case  15: return do_fft(FFTW_FORWARD, B, &flat_top);
        case  14: return do_fft(FFTW_FORWARD, B, &blackman_nuttall_window);
        case  13: return do_fft(FFTW_FORWARD, B, &blackman_harris_window);
        case  12: return do_fft(FFTW_FORWARD, B, &blackman_window);
        case  11: return do_fft(FFTW_FORWARD, B, &hamming_window);
        case  10: return do_fft(FFTW_FORWARD, B, &hann_window);

        case   0: return do_fft(FFTW_FORWARD,  B, 0);
        case  -1: return do_fft(FFTW_BACKWARD, B, 0);

        case -10: return do_window(B, &hann_window);
        case -11: return do_window(B, &hamming_window);
        case -12: return do_window(B, &blackman_window);
        case -13: return do_window(B, &blackman_harris_window);
        case -14: return do_window(B, &blackman_nuttall_window);
        case -15: return do_window(B, &flat_top);
      }

   bad_subfun_number_ERROR(subfunction);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FFT::do_fft(int dir, Value_P B, window_function win)
{
   if (!system_wisdom_loaded)
      {
        fftw_import_system_wisdom();
        system_wisdom_loaded = true;   // try only once
      }

const APL_Integer N = B->element_count();
   if (N == 0)   LENGTH_ERROR;

   // allocate Z before in/out so that a WS_FULL here (Z needs more bytes
   // than in/out combined, since sizeof(Cell) > sizeof(fftw_complex))
   // cannot leak the FFTW buffers below.
   //
Value_P Z(B->get_shape(), LOC);

const ShapeItem io_size = N * sizeof(fftw_complex);

fftw_complex * in  =  reinterpret_cast<fftw_complex *>(fftw_malloc(io_size));
   if (in == 0)    WS_FULL;
fftw_complex * out =  reinterpret_cast<fftw_complex *>(fftw_malloc(io_size));
   if (out == 0)    { fftw_free(in);   WS_FULL; }

   enum { flags = FFTW_ESTIMATE | FFTW_DESTROY_INPUT };

   // fill in[] with B
   //
   if (B->get_rank() <= 1)   // one-dimensional FFT
      {
        fftw_plan plan = fftw_plan_dft_1d(N, in, out, dir, flags);
        if (plan == 0)
           {
             fftw_free(in);
             fftw_free(out);
             WS_FULL;
           }

        // init_in() can throw (e.g. DOMAIN_ERROR on a non-numeric cell in
        // B) -- without this try/catch, an exception there unwound past
        // this function leaking in/out/plan (the read path already
        // hardened the analogous case). fftw_free()/fftw_destroy_plan()
        // then re-throw so the error still propagates normally.
        try
           {
             init_in(in, B, win);   // do this after plan was created
             fftw_execute(plan);
           }
        catch (...)
           {
             fftw_free(in);
             fftw_free(out);
             fftw_destroy_plan(plan);
             throw;
           }
        fftw_destroy_plan(plan);
      }
   else if (B->get_rank() == 2)   // two-dimensional FFT
      {
        fftw_plan plan = fftw_plan_dft_2d(B->get_shape_item(0),
                                          B->get_shape_item(1),
                                          in, out, dir, flags);
        if (plan == 0)
           {
             fftw_free(in);
             fftw_free(out);
             WS_FULL;
           }

        // init_in() can throw (e.g. DOMAIN_ERROR on a non-numeric cell in
        // B) -- without this try/catch, an exception there unwound past
        // this function leaking in/out/plan (the read path already
        // hardened the analogous case). fftw_free()/fftw_destroy_plan()
        // then re-throw so the error still propagates normally.
        try
           {
             init_in(in, B, win);   // do this after plan was created
             fftw_execute(plan);
           }
        catch (...)
           {
             fftw_free(in);
             fftw_free(out);
             fftw_destroy_plan(plan);
             throw;
           }
        fftw_destroy_plan(plan);
      }
   else if (B->get_rank() == 3)   // two-dimensional FFT
      {
        fftw_plan plan = fftw_plan_dft_3d(B->get_shape_item(0),
                                          B->get_shape_item(1),
                                          B->get_shape_item(2),
                                          in, out, dir, flags);
        if (plan == 0)
           {
             fftw_free(in);
             fftw_free(out);
             WS_FULL;
           }

        // init_in() can throw (e.g. DOMAIN_ERROR on a non-numeric cell in
        // B) -- without this try/catch, an exception there unwound past
        // this function leaking in/out/plan (the read path already
        // hardened the analogous case). fftw_free()/fftw_destroy_plan()
        // then re-throw so the error still propagates normally.
        try
           {
             init_in(in, B, win);   // do this after plan was created
             fftw_execute(plan);
           }
        catch (...)
           {
             fftw_free(in);
             fftw_free(out);
             fftw_destroy_plan(plan);
             throw;
           }
        fftw_destroy_plan(plan);
      }
   else                           // k-dimensional FFT
      {
        int ish[MAX_RANK];
        loop(r, B->get_rank())   ish[r] = B->get_shape_item(r);

        fftw_plan plan = fftw_plan_dft(B->get_rank(), ish, in, out, dir, flags);
        if (plan == 0)
           {
             fftw_free(in);
             fftw_free(out);
             WS_FULL;
           }

        // init_in() can throw (e.g. DOMAIN_ERROR on a non-numeric cell in
        // B) -- without this try/catch, an exception there unwound past
        // this function leaking in/out/plan (the read path already
        // hardened the analogous case). fftw_free()/fftw_destroy_plan()
        // then re-throw so the error still propagates normally.
        try
           {
             init_in(in, B, win);   // do this after plan was created
             fftw_execute(plan);
           }
        catch (...)
           {
             fftw_free(in);
             fftw_free(out);
             fftw_destroy_plan(plan);
             throw;
           }
        fftw_destroy_plan(plan);
      }

const double norm = sqrt(N);
   loop(n, N)   Z->next_ravel_Complex(out[n][0]/norm, out[n][1]/norm);

   fftw_free(in);
   fftw_free(out);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FFT::do_window(Value_P B, window_function win)
{
   Assert(win);

   if (B->get_rank() == 0)   return Token(TOK_APL_VALUE1, IntScalar(1, LOC));

const ShapeItem N = B->element_count();
   if (N < 2)   LENGTH_ERROR;

Value_P Z(B->get_shape(), LOC);
const RavelType rt = B->get_ravel_type();
   if (B->get_rank() == 1)
      {
        if (rt == RPT_CELLS)
           { loop(n, N)
                {
                  const double w = win(n, N);
                  Cell cache;
                  const Cell & cell_B = B->get_cravel(n, cache);
                  if (cell_B.is_complex_cell())
                     Z->next_ravel_Complex(w*cell_B.get_real_value(),
                                           w*cell_B.get_imag_value());
                  else
                     Z->next_ravel_Float(w * cell_B.get_real_value());
                }
           }
        else if (rt == RPT_COMPLEX)
           { loop(n, N)   { const double w = win(n, N);
                            Z->next_ravel_Complex(w*B->get_real_value(n),
                                                  w*B->get_imag_value(n)); } }
        else   // RPT_integer or RPT_FLOAT64
           { loop(n, N)   { Z->next_ravel_Float(win(n, N) * B->get_real_value(n)); } }
      }
   else
      {
        std::vector<double> wp(N);
        fill_window(wp.data(), B->get_shape(), win);

        if (rt == RPT_CELLS)
           { loop(n, N)
                {
                  const double w = wp[n];
                  Cell cache;
                  const Cell & cell_B = B->get_cravel(n, cache);
                  if (cell_B.is_complex_cell())
                     Z->next_ravel_Complex(w*cell_B.get_real_value(),
                                           w*cell_B.get_imag_value());
                  else
                     Z->next_ravel_Float(w*cell_B.get_real_value());
                }
           }
        else if (rt == RPT_COMPLEX)
           { loop(n, N)   { Z->next_ravel_Complex(wp[n]*B->get_real_value(n),
                                                   wp[n]*B->get_imag_value(n)); } }
        else   // RPT_integer or RPT_FLOAT64
           { loop(n, N)   { Z->next_ravel_Float(wp[n] * B->get_real_value(n)); } }
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_FFT::fill_window(double * result, const Shape & shape, window_function win)
{
ShapeItem rlen = 1;
   result[0] = 1.0;

   for (sRank r = shape.get_rank() - 1; r >= 0; --r)
       {
         const ShapeItem axis_len = shape.get_shape_item(r);
         double * e = result + rlen * axis_len;
         for (ShapeItem a = axis_len - 1; a >= 0; --a)
             {
               // every window function divides by (axis_len - 1); for a
               // length-1 axis that is a division by 0 (NaN), poisoning
               // the whole transform. A single sample has no window
               // shape to apply, so use weight 1.0 (confirmed via
               // 10 ⎕FFT (1 4⍴1 2 3 4)).
               const double wa = (axis_len < 2) ? 1.0 : win(a, axis_len);
               for (ShapeItem r = rlen - 1; r >= 0; --r)
                   {
                     *--e = wa * result[r];
                   }
             }

         rlen *= axis_len;
       }
}
//────────────────────────────────────────────────────────────────────────────

//────────────────────────────────────────────────────────────────────────────
void
Quad_FFT::init_in(void * _in, Value_P B, window_function win)
{
fftw_complex * in = reinterpret_cast<fftw_complex *>(_in);
const APL_Integer N = B->element_count();

   if (N < 2)
      {
        // a window is undefined for fewer than 2 samples (win(0, N-1) would
        // divide by zero); the win argument is therefore ignored here, same
        // as do_window()'s LENGTH_ERROR for N < 2 in the standalone case.
        //
        in[0][0] = B->get_real_value(0);
        in[0][1] = B->get_imag_value(0);
        return;
      }

   if (win == 0)
      {
        loop(n, N)
           {
             in[n][0] = B->get_real_value(n);
             in[n][1] = B->get_imag_value(n);
           }
      }
   else if (B->get_rank() == 1)
      {
        loop(n, N)
           {
             const double w = win(n, N);
             in[n][0] = w * B->get_real_value(n);
             in[n][1] = w * B->get_imag_value(n);
           }
      }
   else
      {
        std::vector<double> wp(N);
        fill_window(wp.data(), B->get_shape(), win);
        loop(n, N)
           {
             const double w = wp[n];
             in[n][0] = w * B->get_real_value(n);
             in[n][1] = w * B->get_imag_value(n);
           }
      }
}
#else   // not apl_FFT

//────────────────────────────────────────────────────────────────────────────
Quad_FFT::Quad_FFT()
   : QuadFunction(TOK_Quad_FFT)
{
   system_wisdom_loaded = false;
}
//────────────────────────────────────────────────────────────────────────────

Token Quad_FFT::eval_AB(cValue_R A, cValue_R B) const 
{
   return eval_B(B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FFT::eval_B(cValue_R B) const
{
const char * libs[] = { "libfftw3.so",   0 };
const char * hdrs[] = { "fftw3.h",      0 };
const char * pkgs[] = { "libfftw3-dev", 0 };

   return missing_files("⎕FFT", libs, hdrs, pkgs);
}
//────────────────────────────────────────────────────────────────────────────

Token Quad_FFT::eval_XB(cValue_R A, cValue_R B) const 
{
   return eval_B(B);
}
void
Quad_FFT::print_fun_syntax(ostream & out, const function_info & info) const
{
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_FFT::print_map_syntax(ostream & out, const function_info & info) const
{
}
//════════════════════════════════════════════════════════════════════════════
#endif // (not) apl_FFT

