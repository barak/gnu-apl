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

#ifndef __CRC32_HH_DEFINED__
#define __CRC32_HH_DEFINED__

#include <stddef.h>
#include <stdint.h>

/// a tiny, self-contained CRC-32 (used by both the )SAVE/)LOAD workspace
/// checksum in Archive.cc and the A ⎕CR B byte-vector checksum in
/// Quad_CR.cc). Kept in its own namespace rather than as a bare global
/// crc32() -- some translation units (e.g. Quad_PNG.cc) link zlib, whose
/// own global ::crc32() has a different signature and would collide.
namespace apl_crc
{
   /// fold one more byte \b b into the running (inverted) CRC state
   /// \b crc. Start a new checksum with crc == 0xFFFFFFFF, feed bytes
   /// one at a time (in any source: a contiguous buffer, one Cell at a
   /// time from an APL value, ...), then XOR the final state with
   /// 0xFFFFFFFF to get the standard (zlib/PNG-compatible) CRC-32 --
   /// reflected polynomial 0xEDB88320, the bit-reversal of the IEEE
   /// 802.3 polynomial 0x04C11DB7.
   inline uint32_t
   crc32_update(uint32_t crc, uint8_t b)
      {
        crc ^= b;
        for (int i = 0; i < 8; ++i)
            crc = (crc & 1) ? (0xEDB88320U ^ (crc >> 1)) : (crc >> 1);
        return crc;
      }

   /// compute the standard CRC-32 (see crc32_update() above) over
   /// \b len bytes starting at \b data. Computed bit-by-bit rather than
   /// via a lookup table since neither caller processes data large
   /// enough for the difference to matter, and this keeps the
   /// implementation header-only/dependency-free.
   /// @param data start of the byte range to checksum
   /// @param len number of bytes
   inline uint32_t
   crc32(const void * data, size_t len)
      {
        const uint8_t * bytes = static_cast<const uint8_t *>(data);
        uint32_t crc = 0xFFFFFFFFU;
        for (size_t i = 0; i < len; ++i)   crc = crc32_update(crc, bytes[i]);
        return crc ^ 0xFFFFFFFFU;
      }

} // namespace apl_crc

#endif // __CRC32_HH_DEFINED__
