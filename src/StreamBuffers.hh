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

#ifndef __STREAMBUFFERS_HH_DEFINED__
#define __STREAMBUFFERS_HH_DEFINED__

#include <fstream>
#include <streambuf>

#include "DiffOut.hh"
#include "UTF8_string.hh"

//════════════════════════════════════════════════════════════════════════════
/// a streambuf for stdin echo. Deliberately derived from streambuf, NOT
/// filebuf (see ErrOut_streambuf below for why).
class CinOut_streambuf : public streambuf
{
   /// overloaded streambuf::overflow
   /// @param c character to overflow into the buffer
   virtual int_type overflow(int_type c);
};
extern CinOut_streambuf CIN_streambuf;
//════════════════════════════════════════════════════════════════════════════
/**
  A streambuf for stderr output. Deliberately derived from streambuf, NOT
  filebuf: this class never calls filebuf::open(), so a filebuf's own
  inherited FILE* member stays permanently NULL -- harmless as long as
  every write goes through our overridden overflow(), but libc++'s
  basic_filebuf::xsputn() has its own bulk-write fast path that, once the
  (never-managed) put-buffer looks "full" (true for us: pbase()==epptr()
  ==0), calls overflow() ONCE to ask "anything to flush?" and then --
  unless that returned eof() -- writes the ENTIRE remaining string via
  fwrite() straight onto that inherited, never-opened, NULL FILE*,
  bypassing our own overflow() for the actual character data entirely.

  Confirmed root cause of a macOS 27/libc++ segfault (Paul Rockwell,
  bug-apl@gnu.org, 2026-09-24..27): every multi-character write to CERR
  (e.g. the "-h"/"--v" usage text, a single "CERR << "usage: " << ..."
  chain) took that fast path and crashed in fwrite() on a NULL FILE* --
  previously misdiagnosed as std::cerr's own internal FILE* being NULL,
  but Paul's own standalone "std::cerr << "hi"" test never crashed, and
  the crash backtrace's "this->epptr()-this->pbase()"/"this->__file_" are
  OUR ErrOut_streambuf's own inherited members, not std::cerr's -- proven
  by Paul's fix (switching this class's base to streambuf, which has no
  FILE*-based xsputn() fast path at all) eliminating the crash completely.

  A plain streambuf's default xsputn() just calls sputc() in a loop, so
  every character reliably reaches our own overflow() override instead.
**/
class ErrOut_streambuf : public streambuf
{
public:
   /// constructor
   ErrOut_streambuf()
   : expand_LF(false)
   { }

   /// destructor
   ~ErrOut_streambuf()   { used = false; }

   /// set LF → CRLF expansion mode
   /// @param on true to enable LF→CRLF expansion
   bool LF_to_CRLF(bool on)
      {
        const int ret = expand_LF;
        expand_LF = on;
        return ret;
      }

   /** a helper function telling whether the constructor for CERR was called
       if CERR is used before its constructor was called (which can happen in
       when constructors of static objects are called and use CERR) then a
       segmentation fault would occur.

       We avoid that by using get_CERR() instead of CERR in such constructors.
       get_CERR() checks \b used and returns cerr instead of CERR if it is
       false.
    **/
   streambuf * use()   { used = true;   return this; }

   /// true iff the constructor for CERR was called
   static bool used;   // set when CERR is constructed

   /// expand LF to CRLF
   bool expand_LF;

protected:
   /// overloaded streambuf::overflow()
   /// @param c character to overflow into the buffer
   virtual int_type overflow(int_type c);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __STREAMBUFFERS_HH_DEFINED__
