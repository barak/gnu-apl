/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2022  Dr. Jürgen Sauermann

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

/* This file contains eval_XXX() functions that are (conditionally) compiled
   only if their corresponding real functions (with the same name) could not
   be compiled because some library or header file could not be found by
   ./configure, i.e. before compliling GNU APL.

   All such functions in this file add some more information to )MORE and then
   raise a SYNTAX ERROR.
 */

#include "Common.hh"
#include "Error_macros.hh"
#include "Quad_FFT.hh"
#include "Quad_GTK.hh"
#include "Quad_RE.hh"
#include "Token.hh"

/// a generic function for all ⎕XXX errors. Declared extern (rather than
/// static) to avoid -Wunused-function warnings.
extern Token missing_files(const char * qfun,
                           const char ** libs,
                           const char ** hdrs,
                           const char ** pkgs);

//============================================================================
Token
missing_files(const char * qfun,   // the function, e.g. "⎕RE"
              const char ** libs,  // the required libraries
              const char ** hdrs,  // the required header files
              const char ** pkgs)  // the proposed packages
{
UCS_string & more = MORE_ERROR() <<
"Bad luck. The system function " << qfun <<
" has raised a SYNTAX ERROR even though the\n"
"syntax used was correct. The real reason for the SYNTAX ERROR was that a\n"
"library or header file on which " << qfun << " depends:\n"
"\n"
" ⋆ could not be found by ./configure, and/or\n"
" ⋆ was explitly disabled by a ./configure argument\n"
"\n"
"just before GNU APL was compiled.\n";

   if (libs && libs[0])
      {
        const char * numerus1 = libs[1] ? "ies "   : "y ";
        const char * numerus2 = libs[1] ? " were:" : " was:";
        more << "\nThe possibly missing (or disabled) librar" << numerus1
             << "needed by " << qfun << numerus2;
        for (int j = 0; libs[j]; ++j)   more << " " << libs[j];
        more << "\nTo locate installed versions of it, run e.g.:\n\n";
        for (int j = 0; libs[j]; ++j)
            more << "      )HOST find /usr -name '" << libs[j]
                 << "*' 2>/dev/null\n";
      }

   if (hdrs && hdrs[0])
      {
        const char * numerus = hdrs[1] ? "s were" : " was";
        more << "\nThe possibly missing header file" << numerus << ":";
        for (int j = 0; hdrs[j]; ++j)   more << " " << hdrs[j];
        more << "\nTo locate it, run e.g.:\n\n";
        for (int j = 0; hdrs[j]; ++j)
            more << "      )HOST find /usr -name '" << hdrs[j]
                 << "' 2>/dev/null\n";
      }

   more <<
"\n"
"This GNU APL interpreter instance was probably configured like this:\n"
"\n"
"      " CONFIGURE_ARGS "\n\n";

   if (pkgs && pkgs[0])
      {
        more <<
"If the problem was caused by a missing library or header file, then (on a\n"
" standard GNU/Linux/Debian system) it can usually be installed with the\n"
"following command (as root and in a shell):\n"
"\n"
"      apt install";
        for (int j = 0; pkgs[j]; ++j)   more << " " << pkgs[j];
        more <<
"\n"
"\n"
"and after that, reconfigure, recompile, and reinstall GNU APL:\n\n" 
"      ./configure    # if the ./configure options were incorrect, or else\n"
"      ./configure " CONFIGURE_ARGS "\n"
"      make\n"
"      sudo make install\n"
"\n"
"in the top-level directory of the GNU APL package.\n";
      }

   SYNTAX_ERROR;
   return Token();
}
//============================================================================
//============================================================================
#if ! (defined(HAVE_LIBFFTW3) && defined(HAVE_FFTW3_H))

Token
Quad_FFT::eval_B(Value_P B) const
{
const char * libs[] = { "libfftw3.so",   0 };
const char * hdrs[] = { "fftw3.h",      0 };
const char * pkgs[] = { "libfftw3-dev", 0 };

   return missing_files("⎕FFT", libs, hdrs, pkgs);
}
//----------------------------------------------------------------------------
Token
Quad_FFT::eval_AB(Value_P A, Value_P B) const
{
const char * libs[] = { "libfftw3.so",   0 };
const char * hdrs[] = { "fftw3.h",      0 };
const char * pkgs[] = { "libfftw3-dev", 0 };

   return missing_files("⎕FFT", libs, hdrs, pkgs);
}
#endif
//============================================================================
#if ! HAVE_GTK3

Token
Quad_GTK::eval_AB(Value_P A, Value_P B) const
{
const char * libs[] = { "libgtk-3.so",  0 };
const char * hdrs[] = { "gtk/gtk.h",    0 };
const char * pkgs[] = { "libgtk-3-dev", 0 };

   return missing_files("⎕GTK", libs, hdrs, pkgs);
}
//----------------------------------------------------------------------------
Token
Quad_GTK::eval_B(Value_P B) const
{
const char * libs[] = { "libgtk-3.so",  0 };
const char * hdrs[] = { "gtk/gtk.h",    0 };
const char * pkgs[] = { "libgtk-3-dev", 0 };

   return missing_files("⎕GTK", libs, hdrs, pkgs);
}
//----------------------------------------------------------------------------
Token
Quad_GTK::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
const char * libs[] = { "libgtk-3.so",  0 };
const char * hdrs[] = { "gtk/gtk.h",    0 };
const char * pkgs[] = { "libgtk-3-dev", 0 };

   return missing_files("⎕GTK", libs, hdrs, pkgs);
}
//----------------------------------------------------------------------------
Token
Quad_GTK::eval_XB(Value_P X, Value_P B) const
{
const char * libs[] = { "libgtk-3.so",  0 };
const char * hdrs[] = { "gtk/gtk.h",    0 };
const char * pkgs[] = { "libgtk-3-dev", 0 };

   return missing_files("⎕GTK", libs, hdrs, pkgs);
}
//----------------------------------------------------------------------------
void
Quad_GTK::clear()
{
}
//----------------------------------------------------------------------------
#endif   // ! HAVE_GTK3
//============================================================================
#ifndef HAVE_LIBPCRE2_32
Token
Quad_RE::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
const char * libs[] = { "libpcre.so",   0 };
const char * hdrs[] = { "pcre2.h",      0 };
const char * pkgs[] = { "libpcre3-dev", 0 };

   return missing_files("⎕RE", libs, hdrs, pkgs);
}
#endif
//============================================================================
