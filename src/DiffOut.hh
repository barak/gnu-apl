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

#ifndef __DIFFOUT_HH_DEFINED__
#define __DIFFOUT_HH_DEFINED__

#include <ostream>
#include <fstream>
#include <streambuf>

#include "Assert.hh"
#include "PrintOperator.hh"
#include "UTF8_string.hh"

using namespace std;

//════════════════════════════════════════════════════════════════════════════
/// a streambuf that compares its output with a file. Deliberately derived
/// from streambuf, NOT filebuf -- see ErrOut_streambuf's class comment in
/// StreamBuffers.hh for why: this class never calls filebuf::open(), so a
/// filebuf's own inherited FILE* member would stay permanently NULL, and
/// libc++'s basic_filebuf::xsputn() has its own FILE*-based bulk-write
/// fast path that bypasses our overridden overflow() and segfaults on
/// exactly that NULL FILE* for any multi-character write. Confirmed to
/// affect ErrOut_streambuf/CinOut_streambuf on macOS 27/libc++ (Paul Rockwell,
/// bug-apl@gnu.org, 2026-09); DiffOut backs COUT/UERR and shares the
/// identical latent flaw even though no report has hit it here (COUT's
/// own real output apparently never went through a bulk multi-character
/// write early enough to trigger it) -- converted proactively rather than
/// waiting for a second report.
class DiffOut : public streambuf
{
public:
   /// constructor
   /// @param _errout true for error-message stream, false for normal APL output
   DiffOut(bool _errout)
   : aplout(""),
     errout(_errout),
     expand_LF(false)
   { aplout.clear(); }

   /// set LF → CRLF expansion mode
   /// @param on non-zero to enable LF→CRLF expansion
   int LF_to_CRLF(int on)
      { const int old = expand_LF;   expand_LF = on;   return old; }

   /// discard all characters
   void reset()
   { aplout.clear(); }

protected:
   /// return true iff 0-terminated strings apl and ref differ
   /// at \b pos
   /// @param apl APL output string to compare
   /// @param ref reference string to compare against
   /// @param pos starting position; updated to first difference on return
   bool different(const UTF8 * apl, const UTF8 * ref, size_t & pos);

   /// overloaded streambuf::overflow()
   /// @param c character to overflow into the buffer
   virtual int_type overflow(int_type c);

   /// a buffer for one line of APL output
   UTF8_string aplout;

   /// true for error messages, false for normal APL output
   bool errout;

   /// expand LF to CRLF
   bool expand_LF;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __DIFFOUT_HH_DEFINED__
