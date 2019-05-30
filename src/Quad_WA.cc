/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2019  Dr. Jürgen Sauermann

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

#include <sys/resource.h>
#include "Quad_WA.hh"

extern uint64_t top_of_memory();

rlim_t Quad_WA::initial_rlimit = RLIM_INFINITY;
uint64_t Quad_WA::total_memory = 0x40000000;   // a little more than 1 Gig
int64_t  Quad_WA::WA_margin = 0;  // 100000000;
int      Quad_WA::WA_scale = 90;   // percent
unsigned long long Quad_WA::initial_sbrk = 0;

//=============================================================================
Quad_WA::Quad_WA()
   : RO_SystemVariable(ID_Quad_WA)
{
   Symbol::assign(IntScalar(0, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
Value_P
Quad_WA::get_apl_value() const
{
   return IntScalar(total_memory -
                    (Value::total_ravel_count * sizeof(Cell)
                    + Value::value_count * sizeof(Value)), LOC);
}
//-----------------------------------------------------------------------------
void
Quad_WA::init(bool log_startup)
{
   initial_sbrk = top_of_memory();

rlimit rl;

#ifndef RLIMIT_AS // BSD does not define RLIMIT_AS
# define RLIMIT_AS RLIMIT_DATA
#endif

   getrlimit(RLIMIT_AS, &rl);
   initial_rlimit = rl.rlim_cur;

   if (log_startup)
      {
        CERR << "initial RLIMIT_AS (aka. virtual memory) is: ";
        if (initial_rlimit == ~rlim_t(0))   CERR << "'unlimited'" << endl;
        else CERR << initial_rlimit << endl;
      }

   // if the user has set a memory rlimit (and hopefully knowing what she is
   // doing) then leave it as is; otherwise set the limit to 80 % of the
   // avaiable memory
   //
   total_memory = get_free_memory();
   if (log_startup)
      CERR << "estimated available memory: " << total_memory
           << " bytes (" << (total_memory/1000000) << " MB)" << endl;

   if (rl.rlim_cur == RLIM_INFINITY)
      {
        // the user has not set a memory limit, so we may do that. However,
        // limits > 2GB are treated as RLIM_INFINITY, so setting larger
        // limits may have undesirable effects
        //
        if (total_memory < 0x8000000)
           {
             rl.rlim_cur = total_memory;
             setrlimit(rl.rlim_cur, &rl);
             getrlimit(RLIMIT_AS, &rl);
             if (log_startup)
                CERR << "decreasing RLIMIT_AS to: " << rl.rlim_cur
                     << " bytes (" << int64_t(rl.rlim_cur/1000000)
                     << " MB)" << endl;
           }
      }
   else if (rl.rlim_cur > total_memory)
      {
        // the user has set a memory limit, but it is above the available
        // memory
        //
        CERR <<
"*** Warning: the process memory limit (RLIMIT_AS) of " << rl.rlim_cur << endl
<< " is more than the estimated total_memory of " << total_memory << "." << endl
<< " This could cause improper WS FULL handling." << endl;
      }
}
//-----------------------------------------------------------------------------
uint64_t
Quad_WA::get_free_memory()
{
uint64_t result = 0xC0000000;   // assume ~3 GB on error

   if (FILE * pm = fopen("/proc/meminfo", "r"))
      {
        uint64_t mem_ACF[]      = { 0, 0, 0 };   // available / cached / free
        const char * tags_ACF[] = { "MemAvailable:", "Cached:", "MemFree:" };
        for (;;)
            {
              char buffer[2000];
              if (fgets(buffer, sizeof(buffer) - 1, pm) == 0)   break;
              buffer[sizeof(buffer) - 1] = 0;

              for (size_t t = 0; t < sizeof(tags_ACF) / sizeof(*tags_ACF); ++t)
                  {
                    const char * tag = tags_ACF[t];
                    const size_t tag_len = strlen(tag);
                    if (!strncmp(buffer, tag, tag_len))
                       {
                         mem_ACF[t] = 1024 * strtoll(buffer + tag_len, 0, 10);
                       }
                  }
            }
        fclose(pm);

        result = mem_ACF[0] ? mem_ACF[0]                  // MemAvailable:
                            : (mem_ACF[1] + mem_ACF[2]);  // Cached: + MemFree:
      }

   // limit size on 32-bit machines to 2.95 < 3 GB
   if (sizeof(const void *) == 4 && result > 0xB0000000)  result = 0xB0000000;

   return result;
}
//=============================================================================

