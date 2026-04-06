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

#include "Sys.hh"
#include "Common.hh"

//---------------------------------------------------------------------------
int64_t
Sys::probe_memory(uint64_t * base, uint64_t blocks, int verbosity)
{
   enum
      {
        Block_bytes = 4096,
        Block_uint64 = Block_bytes / sizeof(uint64_t),
        Base_offset = 13,
        Base_mult   = 37,
        Idx_mult    = 41,
      };

   if (verbosity & 1)
      {
        CERR << "Sys::probe_memory(addr = " << voidP(base)
             << ", " << blocks << " blocks)..." << endl;
      }

const uint64_t base_0 = Base_offset
                      + reinterpret_cast<uint64_t>(base) * Base_mult;
   loop(b, blocks)
       {
         const uint64_t magic = base_0 + b * Idx_mult;
         const uint64_t addr = b * Block_uint64;
         base[addr]     =  magic;
         base[addr + 1] = ~magic;
       }

   if (verbosity & 1)
      {
        const uint64_t K_bytes = blocks * (Block_bytes / 1024);
        const uint64_t M_bytes = K_bytes / 1024;
        CERR << M_bytes << " Mbyte written." << endl;
      }

int64_t errors = 0;
   loop(b, blocks)
       {
         const uint64_t magic = base_0 + b * Idx_mult;
         const uint64_t addr = b * Block_uint64;
         const uint64_t m0 = base[addr];
         const uint64_t m1 = base[addr + 1];
         if (m0 != magic || m1 != ~magic)
            {
              errors++ == 0 && CERR << endl
                   << "*** Sys::probe_memory() failed: " << endl
                   << "    base:   " << voidP(base) << endl
                   << "    block:  " << HEX8(b) << endl
                   << "    magic:  " << HEX8(magic)
                                     << " (got " << HEX8(m0) << ")" << endl
                   << "    ~magic: " << HEX8(~magic)
                                     << " (got " << HEX8(m1) << ")" << endl
                   << endl;
            }
       }

   (verbosity & 1) && CERR << "Sys::probe_memory() done: "
                           << errors << " error(s)." << endl;
   return errors;
}
//---------------------------------------------------------------------------

