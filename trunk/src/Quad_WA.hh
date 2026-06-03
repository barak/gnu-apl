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

#ifndef __QUAD_WA_HH_DEFINED__
#define __QUAD_WA_HH_DEFINED__

#include "config.h"   // for HAVE_xxx
#if HAVE_SYS_RESOURCE_H
#  include <sys/resource.h>   // for rlim_t
#else  // windows
typedef uint64_t rlim_t;
#endif // HAVE_SYS_RESOURCE_H

#include "SystemVariable.hh"

//════════════════════════════════════════════════════════════════════════════
/**
   System variable Quad-WA (Workspace Available).
 */
/// The class implementing ⎕WA
class Quad_WA : public RO_SystemVariable
{
public:
   /// Constructor.
   Quad_WA();

   /// initialize total_memory
   /// @param log_startup if true print startup diagnostics
   static void init(bool log_startup);

   /// parse a -mem command line option
   /// @param log_startup if true print startup diagnostics
   static void parse_mem(bool log_startup);

   /// the memory limit at startup
   static rlim_t initial_rlimit;

   /// the sbrk() at startup
   static unsigned long long initial_sbrk;

   /// the estimated (!) the amount of free memory
   static uint64_t total_memory;

   /// a safety margin causing WS FULL before complete memory starvation
   static int64_t WA_margin;

   /// a percentage between 45% and 100% to compensate malloc() losses
   static int WA_scale;

protected:
   /// overloaded Symbol::get_apl_value().
   virtual Value_P get_apl_value() const;

   /// estimate (!) the amount of free memory
   static uint64_t get_free_memory();

   /// read /proc/meminfo, return the number of items found
   static int read_meminfo();

   /// read a (supposedly short) file in /proc
   /// @param filename path to the /proc file to read
   static int64_t read_procfile(const char * filename);

   /// some relevant values in /proc/meminfo
   static struct _mem_info
      {
        _mem_info()
        : Available(0),   ///< bytes available
          Cached(0),      ///< bytes cached
          MemFree(0)      ///< bytes free
          {}

        uint64_t Available;   ///< kilobytes available
        uint64_t Cached;      ///< kilobytes cached
        uint64_t MemFree;     ///< kilobytes free
      } meminfo;              ///< values read from /proc/meminfo

   /// \b true iff total_memory was provided by the user
   static bool total_memory_by_user;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __QUAD_WA_HH_DEFINED__
