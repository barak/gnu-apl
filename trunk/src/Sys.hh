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

// This file provides an API to functions that differ on different platforms


#ifndef SYS_HH_DEFINED
#define SYS_HH_DEFINED

#include <cstdint>
#include <cstdio>

#include <unistd.h>
#include <signal.h>

#include "config.h"   // for HAVE_MMAN_H

class Sys
{
public:

#if HAVE_MMAN_H

#include <sys/mman.h>

   static inline const uint8_t * mmap(int fd, int len)
      {
      const void * vp = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
         if (vp == MAP_FAILED)   return 0;
         return reinterpret_cast<const uint8_t *>(vp);
      }

   static inline void munmap(const uint8_t * data, int len)
      { munmap(data, len); }

#else // ! HAVE_MMAN_H =======================================================

   static inline const uint8_t * mmap(int fd, size_t len)
      {
         if (uint8_t * buffer = new uint8_t[len + 1])
            {
              buffer[len] = 0;   // for strXXX()
              if (int(len) == read(fd, buffer, len))   return buffer;

              delete[] buffer;
            }

         return 0;
      }

   static inline void munmap(const uint8_t * data, size_t len)
      { delete [] data; }

#endif // ! MINGW_SRC

   /// invasive memory test
   static int64_t probe_memory(uint64_t * base, uint64_t blocks, int verbosity);
};
//---------------------------------------------------------------------------
inline FILE * sys_popen(const char * command, const char * mode)
{
FILE * file = popen(command, mode);

#if ! MINGW_SRC
   signal(SIGCHLD, SIG_DFL);
#endif // ! MINGW_SRC

   return file;
}
//---------------------------------------------------------------------------
inline int sys_pclose(FILE *stream)
{
const int ret = pclose(stream);

#if ! MINGW_SRC
   signal(SIGCHLD, SIG_IGN);
#endif // ! MINGW_SRC
   return ret;
}
//---------------------------------------------------------------------------



#endif // SYS_HH_DEFINED

