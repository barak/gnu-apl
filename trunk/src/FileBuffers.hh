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

#ifndef __FILEBUFFERS_HH_DEFINED__
#define __FILEBUFFERS_HH_DEFINED__

#include <fstream>

#include "DiffOut.hh"
#include "UTF8_string.hh"

//════════════════════════════════════════════════════════════════════════════
/// a filebuf for stdin echo
class CinOut_filebuf : public filebuf
{
   /// overloaded filebuf::overflow
   /// @param c character to overflow into the buffer
   virtual int overflow(int c);
};
extern CinOut_filebuf CIN_filebuf;
//════════════════════════════════════════════════════════════════════════════
// a filebuf for stderr output
class ErrOut_filebuf : public filebuf
{
public:
   /// constructor
   ErrOut_filebuf()
   : expand_LF(false)
   { }

   /// destructor
   ~ErrOut_filebuf()   { used = false; }
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
   filebuf * use()   { used = true;   return this; }

   /// true iff the constructor for CERR was called
   static bool used;   // set when CERR is constructed

   /// expand LF to CRLF
   bool expand_LF;

   /// current column

protected:
   /// overloaded filebuf::overflow()
   /// @param c character to overflow into the buffer
   virtual int overflow(int c);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __FILEBUFFERS_HH_DEFINED__
