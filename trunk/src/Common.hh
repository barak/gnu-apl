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

#ifndef __COMMON_HH_DEFINED__
#define __COMMON_HH_DEFINED__

// NOTE: the path to config.h is set as -I $(pwd) in ./configure, so we do not
// need ../config.h
//
#include "config.h"   // for xxx_WANTED and other macros from ./configure

/// stringify x
#define STR(x) #x

// #include some notoriously needed include files, but only if they
// exist on the platform

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#ifdef HAVE_DIRENT_H
#  include <dirent.h>   // typedefs DIR as struct __dirstream
#else
  typedef struct __dirstream DIR;
#endif

#include <string.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>   // for gettimeofday()

#ifndef RLIM_INFINITY   // Raspberry
#  define RLIM_INFINITY (~rlim_t(0))
#endif

#ifndef cfg_MAX_RANK_WANTED
#  define cfg_MAX_RANK_WANTED 8
#endif

// if someone (like curses on Solaris) has #defined erase() then
// #undef it because class vector<> would complain about it
#ifdef erase
#  undef erase
#endif

#include <complex>
#include <iostream>
#include <iomanip>
#include <fstream>

#include "APL_types.hh"
#include "Assert.hh"
#include "Logging.hh"
#include "SystemLimits.hh"

using namespace std;

/// one byte (not character !) of a UTF8 encoded Unicode (RFC 3629) string
typedef uint8_t UTF8;

/// true when a WINCH (window size changed) signal was received
extern bool got_WINCH;

/// libapl_version > 0, or 0 if not using libapl
extern const int libapl_version;

/// return the address of main(int argc, char * argv[]) if preset (apl binary)
/// or 0 (for shared libraries) as to avoid that the main symbol is undefined.
extern int64_t get_main();

/// interpreter capabilities (for ]NEXTFILE)
#define  apl_CAPABILITIES  "⎕FFT", "GTK",  "GUI", "⎕PNG", "POSTGRES", "⎕PLOT", \
 "⎕RE", "⎕SQL", "SQLITE3", "X11",  "XCB",

/// true when gtk_init() was called (from ⎕PLOT or from ⎕PNG)
extern bool gtk_init_done;

/// initialize those modules (⎕WA, rlimits, Avec, LibPaths, Value, and VH_entry)
/// that depend on the command line arguments. Note that different
/// build targets (main.cc, libapl.cc, and python_apl.cc) provide their own
/// \b init_modules() function.
/// @param argv0        path to the interpreter executable (argv[0])
/// @param log_startup  true to log startup progress to CERR
extern void init_modules(const char * argv0, bool log_startup);

/// initialize those modules (Output, Svar_DB, LineInput, and Parallel)
/// that are independent of the command line arguments.
/// @param log_startup  true to log startup progress to CERR
extern void init_modules2(bool log_startup);

/// clean up
/// @param soft  true for a soft (non-fatal) cleanup, false for hard exit
extern void cleanup(bool soft);

class InterruptContext
{
public:
  InterruptContext()
  : attention_raised(false),
    interrupt_raised(false),
    interrupt_when(0),
    attention_count(0),
    interrupt_count(0)
  {}

  // ^C handler
  static void control_C(int);

  /// true if Control-C was hit (once)
  static bool attention_is_raised()
     {
       return interrupt_context.attention_raised;
     }

  /// true if Control-C was hit twice within 500 ms
  static bool interrupt_is_raised()
     {
       return interrupt_context.interrupt_raised;
     }

  /// @param loc  caller location for diagnostics
  static void set_attention_raised(const char * loc)
     {
       interrupt_context.attention_raised = true;
     }

  /// @param loc  caller location for diagnostics
  static void clear_attention_raised(const char * loc)
     {
       interrupt_context.attention_raised = false;
     }

   /// @param loc  caller location for diagnostics
   static void set_interrupt_raised(const char * loc)
      {
        interrupt_context.interrupt_raised = true;
      }

   /// @param loc  caller location for diagnostics
   static void clear_interrupt_raised(const char * loc)
      {
        interrupt_context.interrupt_raised = false;
      }

   /// return the number of interrupts
   static uint64_t get_interrupt_count()
      { return interrupt_context.interrupt_count; }

  /// return the range in the prefix parser when ^C was hit (once)
  static const Function_PC2 & get_attention_range()
     { return interrupt_context.attention_range; };

  /// return the range in the prefix parser when ^C was hit (twice)
  static const Function_PC2 & get_interrupt_range()
     { return interrupt_context.interrupt_range; }

protected:
  /// true if ^C was hit (once)
  bool attention_raised;

  /// true if ^C was hit (twice)
  bool interrupt_raised;

  APL_time_us interrupt_when = 0;   // to detect double ^C

  /// the number of attentions
  uint64_t attention_count;

  /// the number of interrupts
  uint64_t interrupt_count;

  /// The range in the prefix parser when ^C was hit (once)
  Function_PC2 attention_range;

  /// The range in the prefix parser when ^C was hit (twice)
  Function_PC2 interrupt_range;

  /// the context for Attention and Interrupt.
  static InterruptContext interrupt_context;
};

/// signal handler for ^C
/// @param sig  signal number (SIGINT)
extern void control_C(int sig);

/// normal APL output (to stdout)
extern ostream COUT;

/// debug output (to stderrear_interrupt_raised
extern ostream CERR;

/// debug output (to stderr)
extern ostream UERR;

class UCS_string;
extern UCS_string & MORE_ERROR();   // in Workspace.cc; clears MORE info.

/// loop from (including) 0 up to (excluding) e
#define loop(v, e)       for (ShapeItem v = 0, __end__ = e; v < __end__; ++v)

/// loop from (excluding) e down to (including) 0
#define rev_loop(v, e)   for (ShapeItem v = e; --v >= 0;)

// #define TROUBLESHOOT_NEW_DELETE

void * common_new(size_t size);
void common_delete(void * p);

/// current time as microseconds since epoch
APL_time_us now();

/// CPU cycle counter (if present)
#if HAVE_RDTSC
inline uint64_t cycle_counter()
{
uint32_t lo, hi;
   __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
   return uint64_t(hi) << 32 | lo;
}

#elif HAVE_GETTIMEOFDAY

inline uint64_t cycle_counter()
{
timeval tv;
   gettimeofday(&tv, 0);
   return tv.tv_sec * 1000000ULL + tv.tv_usec;
}
#else // neither HAVE_RDTSC nor HAVE_GETTIMEOFDAY
#  define cycle_counter() 0
#endif // HAVE_RDTSC

//----------------------------------------------------------------------------
#if HAVE_SEM_INIT

#  define __sem_destroy(sem) sem_destroy(sem)
#  define __sem_init(sem, pshared, value) sem_init(sem, pshared, value)

#else // not HAVE_SEM_INIT

#  define __sem_destroy(sem) { if (sem) sem_close(sem); }
#  define __sem_init(sem, _pshared, value)                   \
{                                                          \
char sname[100];                                           \
   { /* create a unique name */                            \
     timeval tv;   gettimeofday(&tv, 0);                   \
     unsigned int secs = tv.tv_sec, usecs = tv.tv_usec;    \
     snprintf(sname, sizeof(sname), "/gnu_apl_%s_%u_%u",   \
              #sem, secs, usecs);                          \
   }                                                       \
   enum { mode = S_IRWXU | S_IRWXG | S_IRWXO };            \
   sem = sem_open(sname, O_CREAT, mode, value);            \
}
#endif // HAVE_SEM_INIT
//----------------------------------------------------------------------------
/**
  Software probes. A probe is a measurement of CPU cycles executed between two
  points P1 and P2 in the source code.
 */
/// CPU cycle counter for performance measurements
class Probe
{
public:
   /// some Proble related parameters
   enum { PROBE_COUNT = 100,   ///< the number of probes
          PROBE_LEN   = 20     ///< the number of measurements in each probe
        };

   /// constructor
   Probe()
      { init(); }

   /// initialize this probe
   void init()
      {
        idx = 0;
        start_p = &dummy;
        stop_p  = &dummy;
      }

   /// init the p'th probe
   /// @param p  probe index (0 .. PROBE_COUNT-1)
   static int init(int p)
      { if (p >= PROBE_COUNT)   return -3;
        probes[p].init();
        return 0;
      }

   /// initialize all probes
   static void init_all()
      { loop(p, PROBE_COUNT)   init(p); }

   /// start time measurement
   void start()
      {
         if (idx < PROBE_LEN)
            {
               measurement & m = measurements[idx];
               start_p = &m.cycles_from;
               stop_p =  &m.cycles_to;
            }
         else
            {
               start_p = &dummy;
               stop_p  = &dummy;
            }

         // write to *start_p and *stop_p so that they are loaded into the cache
         //
         *stop_p = cycle_counter();
         *start_p = cycle_counter();

         // now the real start of the measurment
         //
         *start_p = cycle_counter();
      }

   /// stop time measurement
   void stop()
      {
        // the real end of the measurment
        //
        *stop_p = cycle_counter();
        ++idx;
      }

   /// get the m'th time (from P1 to P2) of this probe
   /// @param m  measurement index within this probe
   int64_t get_time(int m) const
      { if (m >= idx)   return -1;
        const int64_t diff = measurements[m].cycles_to
                           - measurements[m].cycles_from;
        if (diff < 0)   return -2;
        return diff;
      }

   /// get the m'th time of the p'th probe
   /// @param p  probe index (0 .. PROBE_COUNT-1)
   /// @param m  measurement index within the probe
   static int get_time(int p, int m)
      { if (p >= PROBE_COUNT)   return -3;
        return probes[p].get_time(m);
      }

   /// get the m'th start time of this probe
   /// @param m  measurement index within this probe
   int64_t get_start(int m) const
      { if (m >= idx)   return -1;
        return measurements[m].cycles_from;
      }

   /// get the m'th start time of the p'th probe
   /// @param p  probe index (0 .. PROBE_COUNT-1)
   /// @param m  measurement index within the probe
   static int get_start(int p, int m)
      { if (p >= PROBE_COUNT)   return -3;
        return probes[p].get_start(m);
      }

   /// get the m'th stop time of this probe
   /// @param m  measurement index within this probe
   int64_t get_stop(int m) const
      { if (m >= idx)   return -1;
        return measurements[m].cycles_to;
      }

   /// get the m'th stop time of the p'th probe
   /// @param p  probe index (0 .. PROBE_COUNT-1)
   /// @param m  measurement index within the probe
   static int get_stop(int p, int m)
      { if (p >= PROBE_COUNT)   return -3;
        return probes[p].get_stop(m);
      }

   /// get the number of times int the p'th probe
   /// @param p  probe index (0 .. PROBE_COUNT-1)
   static int get_length(int p)
      { if (p >= PROBE_COUNT)   return -3;
        return probes[p].idx;
      }

   static Probe & P0;    ///< start of vector probes
   static Probe & P_1;   ///< individual probe 1
   static Probe & P_2;   ///< individual probe 2
   static Probe & P_3;   ///< individual probe 3
   static Probe & P_4;   ///< individual probe 4
   static Probe & P_5;   ///< individual probe 5

protected:
   /// one time measurement, measuring cCPU cycles spent between
   /// ttw points in the source code
   struct measurement
      {
        int64_t cycles_from;   ///< the cycle counter at point 1
        int64_t cycles_to;     ///< the cycle counter at point 2
      };

   /// all measurements
   measurement measurements[PROBE_LEN];

   /// index into \b probes
   int idx;

   /// start time
   int64_t * start_p;

   /// end time
   int64_t * stop_p;

   /// a dummy used when \b idx exceeds \b probes
   static int64_t dummy;

   /// all probes
   static Probe probes[];
};
//----------------------------------------------------------------------------
/// Year, Month, Day, hour, minute, second, μsecond
struct YMDhmsu
{
   /// construct YMDhmsu from usec since Jan. 1. 1970 00:00:00
   /// @param t  microseconds since Unix epoch (Jan. 1, 1970 00:00:00)
   YMDhmsu(APL_time_us t);

   /// return usec since Jan. 1. 1970 00:00:00
   APL_time_us get() const;

   int year;     ///< year, e.g. 2013
   int month;    ///< month 1-12
   int day;      ///< day 1-31
   int hour;     ///< hour 0-23
   int minute;   ///< minute 0-59
   int second;   ///< second 0-59
   int micro;    ///< microseconds 0-999999
};
//----------------------------------------------------------------------------

#ifdef TROUBLESHOOT_NEW_DELETE
inline void * operator new(size_t size)   { return common_new(size); }
inline void   operator delete(void * p)   { common_delete(p); }
#endif

using namespace std;

//----------------------------------------------------------------------------

/// a function to be used if CERR might not (yet) be initialized. Returns cerr
/// in that case.
extern std::ostream & get_CERR();   // defined in: Output.cc 

/// The current location in the source file.
#define LOC Loc(__FILE__, __LINE__)
/// The location line l in file f.
#define Loc(f, l) f ":" STR(l)

/// print x and its source code location
#define Q(x) get_CERR() << std::left << setw(20) << #x ":" \
                        << " '" << x << "' at " LOC << endl;
#define Qn(n) get_CERR() << std::left << "══════════ " << #n << "=" << int(n) \
                         << " ══════════" << "' at " LOC << endl;

/// same as Q1 (for printouts guarded by Log macros). Unlike Q () which MUST
/// NOT REMAIN IN THE CODE, Q1() SHOULD remain in the code.
#define Q1(x) get_CERR() << std::left << setw(20) << #x ":" \
                         << " '" << x << "' at " LOC << endl;

/// replacement for variable length arrays
#define ALLOCA(typ, count) \
        reinterpret_cast<typ *>(alloca((count) * sizeof(typ)))

//----------------------------------------------------------------------------

#ifdef cfg_VALUE_HISTORY_WANTED

   enum { VALUEHISTORY_SIZE = 100000 };
   /// @param val  the APL value the event applies to
   /// @param ev   the value-history event type
   /// @param ia   auxiliary integer parameter for the event
   /// @param loc  caller location for diagnostics
   extern void add_event(const Value * val, VH_event ev, int ia,
                        const char * loc);
#  define ADD_EVENT(val, ev, ia, loc)   add_event(val, ev, ia, loc);

#else

   enum { VALUEHISTORY_SIZE = 0 };
#  define ADD_EVENT(_val, _ev, _ia, _loc)

#endif

//============================================================================
/// @param p  pointer into a C string, advanced past any leading whitespace
inline void skip_spaces(const char * & p)
{
   while (*p && *p <= ' ')   ++p;
}
//---------------------------------------------------------------------------
inline const char * yes_no(bool yes)   { return yes ? "yes" : "no"; }
//============================================================================
/// @param pc     function program counter
/// @param offset byte offset to add
inline Function_PC
operator +(Function_PC pc, int offset)
{
   return Function_PC(int(pc) + offset);
}
//----------------------------------------------------------------------------
/// @param pc     function program counter
/// @param offset byte offset to subtract
inline Function_PC
operator -(Function_PC pc, int offset)
{
   return Function_PC(int(pc) - offset);
}
//----------------------------------------------------------------------------
/// Function_PC ++ (post increment)
/// @param pc  function program counter to increment
/// @param     unused post-increment distinguisher
inline Function_PC
operator ++(Function_PC & pc, int)
{
const Function_PC before_increment = pc;
   pc = pc + 1;
   return before_increment;
}
//----------------------------------------------------------------------------
/// Function_PC ++ (pre increment)
/// @param pc  function program counter to increment
inline Function_PC &
operator ++(Function_PC & pc)
{
   pc = pc + 1;
   return pc;
}
//----------------------------------------------------------------------------
/// Function_PC -- (pre decrement)
/// @param pc  function program counter to decrement
inline Function_PC &
operator --(Function_PC & pc)
{
   pc = pc - 1;
   return pc;
}
//----------------------------------------------------------------------------
/// frequently used cast to const char *
/// @param utf  UTF-8 byte pointer to reinterpret as char *
inline const char *
charP(const UTF8 * utf)
{
   return reinterpret_cast<const char *>(utf);
}
//----------------------------------------------------------------------------
/// frequently used cast to const char *
/// @param vp  void pointer to reinterpret as char *
inline const char *
charP(const void * vp)
{
   return reinterpret_cast<const char *>(vp);
}
//----------------------------------------------------------------------------
/// frequently used cast to a const void *
/// @param addr  address to return as const void *
inline const void *
voidP(const void * addr)
{
   return addr;
}
//----------------------------------------------------------------------------

#define uhex  std::hex << uppercase << setfill('0')
#define uhexs  std::hex << uppercase
#define lhex  std::hex << nouppercase << setfill('0')
#define nohex std::dec << nouppercase << setfill(' ')

/// formatting for hex (and similar) values
#define HEX(x)     "0x" << uhex <<             int64_t(x) << nohex
#define HEX2(x)    "0x" << uhex << std::right << \
                           setw(2) << int(x) << std::left << nohex
#define HEX4(x)    "0x" << uhex << std::right << \
                           setw(4) << int(x) << std::left << nohex
#define HEX8(x)    "0x" << uhex << std::right << \
                           setw(8) << int32_t(x) << std::left << nohex
#define HEX16(x)   "0x" << uhex << std::right << \
                           setw(16) << int64_t(x) << std::left << nohex
#define HEX16s(x)          uhexs << std::right << \
                           setw(16) << int64_t(x) << std::left << nohex
#define UNI(x)     "U+" << uhex <<      setw(4) << int(x) << nohex

/// set the last byte in buffer to 0 (so that string functions won't fail
/// even if the  buffer was not 0-terminated for some reason.
#define NULL_TERMINATE(buffer)   { buffer[sizeof(buffer) - 1] = 0; }

#define SPRINTF(buffer, ...) \
{ snprintf(buffer, sizeof(buffer), __VA_ARGS__); NULL_TERMINATE(buffer) }

#endif // __COMMON_HH_DEFINED__
