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

#include "Assert.hh"
#include "PrintOperator.hh"
#include "UTF8_string.hh"

using namespace std;

//════════════════════════════════════════════════════════════════════════════
/// a filebuf that compares its output with a file.
class DiffOut : public filebuf
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

   /// overloaded filebuf::overflow()
   /// @param c character to overflow into the buffer
   virtual int overflow(int c);

   /// a buffer for one line of APL output
   UTF8_string aplout;

   /// true for error messages, false for normal APL output
   bool errout;

   /// expand LF to CRLF
   bool expand_LF;
};
//════════════════════════════════════════════════════════════════════════════
/// a filebuf for stderr output
class ErrOut : public filebuf
{
public:
   /// constructor
   ErrOut()
   : expand_LF(false)
   {}

   /// destructor
   ~ErrOut()   { used = false; }

   /// set LF → CRLF expansion mode
   /// @param on non-zero to enable LF→CRLF expansion
   int LF_to_CRLF(int on)
      { const int old = expand_LF;   expand_LF = on;   return old; }

   /** a helper function telling whether the constructor for CERR was called
       if CERR is used before its constructor was called (which can happen in
       when constructors of static objects are called and use CERR) then a
       segmentation fault would occur.

       We avoid that by using get_CERR() instead of CERR in such constructors.
       get_CERR() checks \b used and returns cerr instead of CERR if it is
       false.
    **/
   filebuf * use()   { used = true;   return this; }

   /// expand LF to CRLF
   bool expand_LF;

   /// true iff the constructor for CERR was called
   static bool used;   // set when CERR is constructed

protected:
   /// overloaded filebuf::overflow()
   /// @param c character to overflow into the buffer
   virtual int overflow(int c);
};
//════════════════════════════════════════════════════════════════════════════

#endif // __DIFFOUT_HH_DEFINED__
