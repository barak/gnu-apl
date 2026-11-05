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

#ifndef __CRASH_DUMP_HH_DEFINED__
#define __CRASH_DUMP_HH_DEFINED__

#include "Common.hh"   // for MINGW_SRC

#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <string>
#include <vector>

#if MINGW_SRC   // no crash handling (CrashDump::init() does nothing)
typedef jmp_buf sigjmp_buf;
# define sigsetjmp(env, savemask) setjmp(env)
typedef void siginfo_t;
#endif

//════════════════════════════════════════════════════════════════════════════
/** Handling of crash signals (SIGSEGV, SIGBUS, SIGFPE, SIGILL, and SIGABRT).

    When the interpreter crashes, then:

    1. a backtrace is printed (with async-signal-safe functions only),

    2. a crash-dump tar file with the backtrace, the )SIS of the crash, the
       shapes, ranks, and depths of the values involved, and information
       about the interpreter and the system is written to
       ~/.config/gnu-apl/last_crash.tar (if that directory exists) or else
       to /tmp/gnu-apl-<uid>-last_crash.tar (the two previous reports are
       kept as last_crash.1.tar and last_crash.2.tar), and the user is
       asked to send it to bug-apl@gnu.org, and

    3. the interpreter continues in immediate execution with a cleared SI
       (via siglongjmp() to main()'s recovery point).

    Step 3 is skipped (and the interpreter exits as before) when running
    testcase files (-T), with --TM 4 (exit on error), for a crash in a
    thread other than the main thread, before main() has set its recovery
    point, for a crash while handling a crash, and if another thread crashes
    while a crash is being handled. Step 2 is skipped when
    running testcase files: IO_Files::report_abnormal_exit() reports the
    crash there.
 **/
class CrashDump
{
public:
   /// install the signal handlers for all crash signals
   static void init();

   /// the recovery point in main() (to which a crash returns)
   static sigjmp_buf recovery_point;

   /// true iff \b recovery_point was set
   static volatile sig_atomic_t recovery_point_valid;

   /// continue after siglongjmp() to \b recovery_point: clear the SI and
   /// tell the user. \b sig is the signal that caused the crash.
   static void recover(int sig);

   /// return the name of signal \b sig, e.g. "SIGSEGV"
   static const char * signal_name(int sig);

   /// return a short description of signal \b sig
   static const char * signal_description(int sig);

   /// raise crash signal \b name ("SEGV", "BUS", "FPE", "ILL", or "ABRT")
   /// the way it typically happens in practice (⎕FIO ¯21). Return false
   /// if \b name is not a crash signal (or the signal was not raised).
   static bool provoke(const char * name);

protected:
   /// one file in the crash-dump tar file
   struct Tar_member
      {
        /// the file name (relative to the directory in the tar file)
        std::string name;

        /// the file content
        std::string content;
      };

   /// the handler for all crash signals
   static void crash_handler(int sig, siginfo_t * info, void * context);

   /// the handler for SIGALRM (a hang while handling a crash)
   static void alarm_handler(int sig);

   /// collect the crash information and write the crash-dump tar file.
   /// Return its file name (or an empty string if writing failed).
   static std::string write_dump(int sig, const void * address,
                                 const std::string & raw_backtrace);

   /// return information about the crash (the SI and the values involved)
   static std::string crash_info(int sig, const void * address);

   /// return information about the interpreter and the system
   static std::string environment_info();

   /// write \b members as a POSIX ustar archive to \b path
   static bool write_tar(const std::string & path, const std::string & dir,
                         const std::vector<Tar_member> & members);

   /// the thread that runs main() (the only one that can recover)
   static pthread_t main_thread;

   /// non-zero while a crash is being handled
   static volatile sig_atomic_t in_handler;

   /// non-zero if another thread crashed while a crash was being handled
   static volatile sig_atomic_t other_thread_crashed;

   /// the thread that handles the (first) crash
   static pthread_t handler_thread;

   /// the signal stack (so that stack overflows can be handled as well)
   static char * signal_stack;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __CRASH_DUMP_HH_DEFINED__
