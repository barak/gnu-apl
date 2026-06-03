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

#ifndef __CDR_HH_DEFINED__
#define __CDR_HH_DEFINED__

#include "CDR_string.hh"
#include "Value_P.hh"

class Value;

/** a class for creating the Common Data Representation of an APL value.
 **/

//────────────────────────────────────────────────────────────────────────────
/// The class implementing CDR (IBM's Common Data Representation)
class CDR
{
public:
   /// return 2 bytes starting at \b data in network byte order
   /// @param data pointer to at least 2 bytes of raw data
   static uint32_t get_2_be(const uint8_t * data)
      {
        uint32_t ret =  *data++;   ret <<= 8;
                 ret |= *data++;   return ret;
      }

   /// return 4 bytes starting at \b data in network byte order
   /// @param data pointer to at least 4 bytes of raw data
   static uint32_t get_4_be(const uint8_t * data)
      { return CDR_header::get_be32(data); }

   /// return 8 bytes starting at \b data in network byte order
   /// @param data pointer to at least 8 bytes of raw data
   static uint64_t get_8_be(const uint8_t * data)
      {
        uint64_t ret = get_4_be(data);   ret <<= 32;
                 return ret | get_4_be(data + 4);
      }

   /// create a value from a CDR_string, throwing
   /// DOMAIN ERROR if cdr is ill-formed
   /// @param cdr CDR byte string to deserialise
   /// @param loc caller location for diagnostics
   static Value_P from_CDR(const CDR_string & cdr, const char * loc);

   /// convert \b value into a CDR_string
   /// @param result output CDR byte string
   /// @param value APL value to serialise
   static void to_CDR(CDR_string & result, const Value * value);

protected:
   /// fill result with the bytes of the CDR of \b value
   /// @param result output CDR byte string being built
   /// @param type CDR type code for the value
   /// @param len total byte length of the CDR representation
   /// @param val APL value whose CDR bytes are appended
   static void fill(CDR_string & result, int type, int len, const Value & val);
};
//────────────────────────────────────────────────────────────────────────────

#endif // __CDR_HH_DEFINED__
