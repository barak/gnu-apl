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

#ifndef __BACKTRACE_HH_DEFINED__
#define __BACKTRACE_HH_DEFINED__

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "Common.hh"
#include "PrintOperator.hh"

//════════════════════════════════════════════════════════════════════════════
/// show the current function call stack.
class Backtrace
{
public:
   /// show the current caller (only valid while being called)
   /// @param offset  stack frame offset from the current call
   static const char * caller(int offset);

   /// show the current function call stack.
   /// @param file  source filename where the call is made
   /// @param line  source line number where the call is made
   static void show(const char * file, int line);

   /// show the current function call stack using only async-signal-safe
   /// operations (raw backtrace() + backtrace_symbols_fd() + write();
   /// no malloc, no __cxa_demangle, no iostream). Call this -- never
   /// show() above -- from inside a signal handler: show()'s use of
   /// backtrace_symbols()/__cxa_demangle() calls malloc() internally,
   /// which can deadlock or crash a second time if the fault interrupted
   /// the crashing thread while it already held glibc's malloc arena
   /// lock. Prints raw (non-demangled) addresses only.
   ///
   /// @param extra_fd  if >= 0, an already-open file descriptor that the
   /// same raw backtrace is ALSO written to (via the same signal-safe
   /// backtrace_symbols_fd() call), so a crash's backtrace survives on
   /// disk independent of whatever else happens to CERR/STDERR_FILENO
   /// afterward. The fd is written to, but neither opened nor closed
   /// here -- the caller (main.cc, before installing/triggering the
   /// signal) is responsible for both, since open()/close() are not
   /// guaranteed async-signal-safe either.
   /// @param to_stderr  false: write only to extra_fd
   static void show_signal_safe(int extra_fd = -1, bool to_stderr = true);

   /// the PC of the instruction that caused a crash signal (set by the
   /// signal handler, 0 if unknown). Unlike the other PCs in a backtrace
   /// it is not a return address (that points behind a call).
   static const void * signal_PC;

protected:
   /// what is known about one stack frame from the symbol tables and the
   /// line numbers (DWARF) in the binaries themselves
   struct Frame_info
      {
        /// the (demangled) name of the function (empty if unknown)
        std::string fun;

        /// the source location (file:line) in \b fun (empty if unknown)
        std::string src_loc;

        /// the functions that were inlined into \b fun at this point
        /// (outermost first), as "function at file:line"
        std::vector<std::string> inlined;
      };

   /// resolve the function names and source locations of the \b size PCs
   /// in \b buffer (as returned by backtrace())
   static void resolve_frames(void * const * buffer, int size,
                              std::vector<Frame_info> & frames);

   /// run addr2line (if installed) for the \b addrs (pairs of frame index
   /// and address in \b file) and store the results in \b frames
   static void run_addr2line(const std::string & file,
                     const std::vector<std::pair<int, uint64_t>> & addrs,
                     std::vector<Frame_info> & frames);

   /// demangle a line returned by backtrace_symbols()
   /// @param result      output buffer for demangled name
   /// @param result_max  size of the output buffer
   /// @param buf         raw mangled symbol string from backtrace_symbols()
   static int demangle_line(char * result, size_t result_max, const char * buf);

   /// print one item in the backtrace to cerr. NOTE: modifies s.
   /// @param idx  index (depth) of the stack frame
   /// @param s    mutable backtrace symbol string for this frame
   /// @param frame what resolve_frames() found out about this frame
   static void show_item(int idx, char * s, const Frame_info & frame);
};
//════════════════════════════════════════════════════════════════════════════

#define BACKTRACE Backtrace::show(__FILE__, __LINE__);

#endif // __BACKTRACE_HH_DEFINED__
