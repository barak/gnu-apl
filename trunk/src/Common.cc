/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "Common.hh"
#include "LineInput.hh"
#include "LibPaths.hh"
#include "NativeFunction.hh"
#include "Output.hh"
#include "Parallel.hh"
#include "ProcessorID.hh"
#include "Quad_GTK.hh"
#include "Quad_WA.hh"
#include "Svar_DB.hh"
#include "Symbol.hh"
#include "StateIndicator.hh"
#include "Thread_context.hh"
#include "Token.hh"
#include "Value.hh"
#include "UserFunction.hh"
#include "UserPreferences.hh"
#include "ValueHistory.hh"
#include "Workspace.hh"

bool got_WINCH = false;

bool gtk_init_done = false;

InterruptContext InterruptContext::interrupt_context;

//----------------------------------------------------------------------------
void
init_modules(const char * argv0, bool log_startup)
{
   // init the workspace memory limits
   //
   Quad_WA::init(log_startup);

   enum { VALUE_HEADER = sizeof(Value)
                       - sizeof(Shape)
                       - cfg_SHORT_VALUE_LENGTH_WANTED * sizeof(Cell)
        };

   if (log_startup)
      CERR << endl
           << "sizeof(int) is            " << sizeof(int)               << endl
           << "sizeof(long) is           " << sizeof(long)              << endl
           << "sizeof(void *) is         " << sizeof(void *)            << endl
           << endl
           << "sizeof(Cell) is           " << sizeof(Cell)              << endl
           << "sizeof(SI stack item) is  " << sizeof(StateIndicator)    << endl
           << "sizeof(Svar_partner) is   " << sizeof(Svar_partner)      << endl
           << "sizeof(Svar_record) is    " << sizeof(Svar_record)       << endl
           << "sizeof(Symbol) is         " << sizeof(Symbol)            << endl
           << "sizeof(Token) is          " << sizeof(Token)             << endl
           << "sizeof(Value) is          " << sizeof(Value)
           << " (" << VALUE_HEADER << " byte header + "
                   << sizeof(Shape) << " byte shape + "
                   << cfg_SHORT_VALUE_LENGTH_WANTED << " Cells)"        << endl
           << "sizeof(ValueStackItem) is " << sizeof(ValueStackItem)    << endl
           << "sizeof(UCS_string) is     " << sizeof(UCS_string)        << endl
           << "sizeof(UserFunction) is   " << sizeof(UserFunction)      << endl
           << endl
           << "⎕WA total memory is       " << Quad_WA::total_memory
           << " bytes (" << (Quad_WA::total_memory/1000000) << " MB, 0x"
           << hex << Quad_WA::total_memory << ")" << dec                << endl
                                                                        << endl
           << "configure command: " << cfg_CONFIGURE_ARGS               << endl
                                                                        << endl;

   // CYGWIN does not have RLIMIT_NPROC
   //
#ifdef RLIMIT_NPROC

   // unlimit the number of threads and processes...
   //
rlimit rl;
   getrlimit(RLIMIT_NPROC, &rl);
   if (log_startup)
      CERR << "increasing rlimit RLIMIT_NPROC from " <<  rl.rlim_cur
           << " to infinity" << endl;
   rl.rlim_cur = RLIM_INFINITY;
   setrlimit(RLIMIT_NPROC, &rl);

   // limit the virtual memory size to avoid new() problems with large values
   //
#endif

   Avec::init();
   LibPaths::init(argv0, log_startup);
   Value::init();
   VH_entry::init();
}
//----------------------------------------------------------------------------
/// initialize subsystems that depend on argv[]
void
init_modules2(bool log_startup)
{
const int retry_max = UserPreferences::uprefs.emacs_mode ? 15 : 5;

   Output::init(log_startup);
   Svar_DB::init(LibPaths::get_APL_bin_path(),
                 LibPaths::get_APL_bin_name(),
                 retry_max,
                 log_startup,
                 UserPreferences::uprefs.system_do_svars);

   LineInput::init(true);

   Parallel::init(log_startup || LOG_Parallel);
}
//----------------------------------------------------------------------------
/// the opposite of init()
void
cleanup(bool soft)
{
   if (soft)   // proper clean-up
      {
        ProcessorID::disconnect();

        NativeFunction::cleanup();

        // write line history
        //
        LineInput::close(false);

        Output::reset_colors();

        Thread_context::cleanup();

        ID::cleanup();

        Quad_GTK::close_all_windows();
      }
   else        // minimal clean-up
      {
        LineInput::close(true);
        Output::reset_colors();
      }
}
//----------------------------------------------------------------------------
void
InterruptContext::control_C(int)
{
APL_time_us when = now();

   CIN << "^C";

   if (StateIndicator * si = Workspace::SI_top())
      {
        const Prefix & prefix = si->get_prefix();
        
        interrupt_context.attention_range.low  = prefix.get_range_low();
        interrupt_context.attention_range.high = prefix.get_range_high();
      }
   else
      {
        interrupt_context.attention_range.low  = Function_PC_invalid;
        interrupt_context.attention_range.high = Function_PC_invalid;
      }
   interrupt_context.attention_raised = true;
   ++interrupt_context.attention_count;
   if ((when - interrupt_context.interrupt_when) < 1000000)   // second ^C within 1 second
      {
        interrupt_context.interrupt_raised = true;
        interrupt_context.interrupt_range  = interrupt_context.attention_range;
        ++interrupt_context.interrupt_count;
      }

   interrupt_context.interrupt_when = when;
}
//----------------------------------------------------------------------------
int
axis_compare(const void * key, const void * info)
{
   return strcasecmp(reinterpret_cast<const char *>(key),
                     reinterpret_cast<const sub_function_info *>
                                     (info)->sub_name);
}
//----------------------------------------------------------------------------
void
list_mapping(ostream & out, const char * top_fun, int idx,
             size_t max_idx_len, const char * subfun, size_t max_subfun_len)
{
  out << "      " << top_fun << "[" << setw(max_idx_len) << idx
      << "]  ←→  " << top_fun << "['" << subfun << "']"
      << UCS_string(max_subfun_len - strlen(subfun), UNI_SPACE)
      << "  ←→  " << top_fun << "." << subfun << endl;
}
//----------------------------------------------------------------------------
void
list_all_mappings(ostream & out, const char * top_fun,
                  const sub_function_info * sub_functions, size_t subfun_count)
{
   // figure the maximum idx and funxtion name lengths
   //
size_t max_idx_len  = 1;
size_t max_name_len = 1;
   loop(idx, subfun_count)
       {
         size_t idx_len = 1;
         if (sub_functions[idx].axis >  9)   idx_len = 2;
         if (sub_functions[idx].axis <  0)   idx_len = 2;
         if (sub_functions[idx].axis < -9)   idx_len = 3;
         if (max_idx_len < idx_len)   max_idx_len = idx_len;

         const size_t name_len = strlen(sub_functions[idx].sub_name);
         if (max_name_len < name_len)   max_name_len = name_len;
       }

   out <<
   "      With a small performance penalty, " << top_fun <<
   "[ ] also accepts the " "following\n"
       "      strings instead of function numbers as axis argument:\n\n";

   loop(idx, subfun_count)
       {
         list_mapping(out, top_fun, sub_functions[idx].axis, max_idx_len,
                                    sub_functions[idx].sub_name, max_name_len);
       }
   out << "\n      For a more detailed description of all sub-functions:\n\n"
          "      " << top_fun << " ⍬" << endl;
}
//----------------------------------------------------------------------------

// Probes...

int64_t Probe::dummy = 0;
Probe Probe::probes[Probe::PROBE_COUNT];

Probe & Probe::P0 = probes[0];
Probe & Probe::P_1 = probes[Probe::PROBE_COUNT - 1];
Probe & Probe::P_2 = probes[Probe::PROBE_COUNT - 2];
Probe & Probe::P_3 = probes[Probe::PROBE_COUNT - 3];
Probe & Probe::P_4 = probes[Probe::PROBE_COUNT - 4];
Probe & Probe::P_5 = probes[Probe::PROBE_COUNT - 5];

//----------------------------------------------------------------------------
void *
common_new(size_t size)
{
void * ret = malloc(size);
const uint64_t iret = uint64_t(ret);
   CERR << "NEW " << HEX(iret) << "-" << HEX(iret + size)
        << "  (" << HEX(size) << ")" << endl;
   return ret;
}
//----------------------------------------------------------------------------
void
common_delete(void * p)
{
   CERR << "DEL " << HEX(uint64_t(p)) << endl;
   free(p);
}
//----------------------------------------------------------------------------
APL_time_us
now()
{
timeval tv_now;
   gettimeofday(&tv_now, 0);

APL_time_us ret = tv_now.tv_sec;
   ret *= 1000000;
   ret += tv_now.tv_usec;
   return ret;
}
//----------------------------------------------------------------------------
YMDhmsu::YMDhmsu(APL_time_us at)
   : micro(at % 1000000)
{
const time_t secs = at/1000000;
struct tm * t = gmtime(&secs);

   year   = t->tm_year + 1900;
   month  = t->tm_mon  + 1;
   day    = t->tm_mday;
   hour   = t->tm_hour;
   minute = t->tm_min;
   second = t->tm_sec;
}
//----------------------------------------------------------------------------
APL_time_us
YMDhmsu::get() const
{
tm t;
   t.tm_year  = year - 1900;
   t.tm_mon   = month - 1;
   t.tm_mday  = day;
   t.tm_hour  = hour;
   t.tm_min   = minute;
   t.tm_sec   = second;
   t.tm_isdst = 0;

APL_time_us ret =  mktime(&t);
   ret *= 1000000;
   ret += micro;
   return ret;
}
//----------------------------------------------------------------------------
ostream &
operator << (ostream & out, const Function_PC2 & ft)
{
   return out << ft.low << ":" << ft.high;
}
//----------------------------------------------------------------------------
ostream &
print_flags(ostream & out, ValueFlags flags)
{
   return out << ((flags & VF_marked)   ?  "M" : "-")
              << ((flags & VF_complete) ?  "C" : "-");
}
//----------------------------------------------------------------------------
int
nibble(Unicode uni)
{
   switch(uni)
      {
        case UNI_0 ... UNI_9:   return      (uni - UNI_0);
        case UNI_A ... UNI_F:   return 10 + (uni - UNI_A);
        case UNI_a ... UNI_f:   return 10 + (uni - UNI_a);
        default: break;
      }

   return -1;   // uni is not a hex digit
}
//----------------------------------------------------------------------------
int
sixbit(Unicode uni)
{
   switch(uni)
      {
        case UNI_A ... UNI_Z:   return      (uni - UNI_A);
        case UNI_a ... UNI_z:   return 26 + (uni - UNI_a);
        case UNI_0 ... UNI_9:   return 52 + (uni - UNI_0);

        case UNI_PLUS:                             // standard    62
        case UNI_MINUS:               return 62;   // alternative 62

        case UNI_SLASH:                            // standard    63
        case UNI_UNDERSCORE:          return 63;   // alternative 63

        case UNI_EQUAL:               return 64;   // fill character

        default: break;
      }

   return -1;   // uni is not a hex digit
}
//----------------------------------------------------------------------------
