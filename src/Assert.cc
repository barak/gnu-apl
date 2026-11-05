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

#include <string.h>

#include "Assert.hh"
#include "Backtrace.hh"
#include "Common.hh"
#include "Error_macros.hh"
#include "IO_Files.hh"
#include "Output.hh"
#include "Workspace.hh"

#include "Workspace.icc"

/// prevent recursive do_Assert() calls. Set for the *entire* duration of
/// a do_Assert() call (including the throw_apl_error() below), not just
/// while printing the backtrace/SI stack -- see Asserting_guard.
static bool asserting = false;

/// RAII guard for \b asserting: sets it on construction, clears it on
/// destruction -- including when do_Assert() "returns" via the C++
/// exception thrown by throw_apl_error() rather than a normal return,
/// so a later, unrelated assertion failure is not mistaken for a
/// recursive one just because a previous do_Assert() call threw out of
/// its frame without reaching an explicit "asserting = false" statement.
struct Asserting_guard
{
   Asserting_guard()    { asserting = true;  }
   ~Asserting_guard()   { asserting = false; }
};

//════════════════════════════════════════════════════════════════════════════
void
do_Assert(const char * cond, const char * fun, const char * file, int line)
{
   if (asserting)
      {
        // do_Assert() was called again while already unwinding from an
        // earlier assertion failure -- typically because that failure's
        // own error-reporting path (do_Assert -> throw_apl_error ->
        // ... -> Error::print_EM() -> cout) writes through a streambuf
        // whose overflow() contains an Assert() of its own (e.g.
        // DiffOut::overflow()'s Assert(report.is_open())), which fails
        // for the same underlying reason and re-enters here. Recursing
        // into the full handling below -- and, critically, calling
        // throw_apl_error() again -- is exactly what turns one assertion
        // failure into unbounded recursion and a stack-overflow SIGSEGV
        // (see Savannah SVN r3480-era nightly-build crash in JUMPS/AAA1.tc,
        // caused by exactly this cycle). Report it and return instead,
        // so the caller that hit the *second* Assert() falls through to
        // its own (degraded but safe) behavior after the failed check,
        // the same way it would if Assert() were compiled out entirely.
        get_CERR() << "*** do_Assert() called recursively -- "
                    << (cond ? cond : (fun ? fun : "?")) << " at " << file
                    << ":" << line << " -- ignoring to avoid infinite"
                       " recursion ***" << endl;
        return;
      }

Asserting_guard guard;
char loc[FILENAME_MAX + 20];

   Log(LOG_delete)
      get_CERR() << "new    " << voidP(loc) << " at " LOC << endl;

   SPRINTF(loc, "%s:%d", file, line);

   get_CERR() << endl
        << "======================================="
           "=======================================" << endl;


   if (cond)       // normal assert()
      {
        get_CERR() << "Assertion failed: " << cond << endl
             << "in Function:      " << fun  << endl
             << "in file:          " << loc  << endl << endl;
      }
   else if (fun)   // segfault etc.
      {
        get_CERR() << "\n\n================ " << fun <<  " ================\n";
      }

   get_CERR() << "C/C++ call stack:" << endl;

   BACKTRACE

   get_CERR() << endl << "SI stack:" << endl << endl;
   Workspace::list_SI(get_CERR(), SIM_SIS_dbg);

   get_CERR() << "======================================="
           "=======================================" << endl;

   // count errors
   IO_Files::assert_error();

   if (Error * err = Workspace::get_error())
      new (err) Error(E_ASSERTION_FAILED, loc);

   throw_apl_error(E_ASSERTION_FAILED, LOC);
}
//════════════════════════════════════════════════════════════════════════════

