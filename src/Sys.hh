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
#include <cstring>

#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <cerrno>

#include "config.h"   // for HAVE_SYS_MMAN_H

#if ! MINGW_SRC
# include <sys/wait.h>
#endif

#if HAVE_SYS_MMAN_H
# include <sys/mman.h>   // must be at file scope: its extern "C" block
                          // cannot appear inside a class body
#endif

#include "Common.hh"   // for CERR and InterruptContext

//════════════════════════════════════════════════════════════════════════════
/// A FILE * that pclose()s itself FILE * when destructed.
class PipePointer
{
public:
   /// @param fptr the pipe FILE pointer to manage
   PipePointer(FILE * fptr) : ptr(fptr) {}
   ~PipePointer()   { close(); }

   FILE * operator->()
      { return ptr; }

   int close()
      {
        if (ptr)
           {
             const int ret = pclose(ptr);
             ptr = 0;
             return ret;
           }

        return 0;
      }

protected:
   FILE * ptr;
};

class Sys
{
public:

#if HAVE_SYS_MMAN_H

   /// @param fd open file descriptor to map
   /// @param len number of bytes to map
   static inline const uint8_t * mmap(int fd, int len)
      {
      // ::mmap, not mmap: an unqualified call here would resolve to
      // this very function (Sys::mmap hides the global ::mmap once
      // found, regardless of the mismatched argument count) -- silent
      // infinite recursion, not a call to the POSIX syscall. This
      // branch was dead code until the HAVE_MMAN_H/HAVE_SYS_MMAN_H
      // macro-name mismatch below was fixed, so it had never been
      // compiled; both this and the equivalent bug in munmap() below
      // were caught only then.
      const void * vp = ::mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
         if (vp == MAP_FAILED)   return 0;
         return reinterpret_cast<const uint8_t *>(vp);
      }

   /// @param data pointer returned by mmap()
   /// @param len number of bytes originally mapped
   static inline void munmap(const uint8_t * data, int len)
      { ::munmap(const_cast<void *>(reinterpret_cast<const void *>(data)),
                len); }

#else // ! HAVE_SYS_MMAN_H ===================================================

   /// @param fd open file descriptor to read
   /// @param len number of bytes to read
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

   /// @param data pointer returned by mmap()
   /// @param len number of bytes originally allocated (unused on this platform)
   static inline void munmap(const uint8_t * data, size_t len)
      { delete [] data; }

#endif // ! HAVE_SYS_MMAN_H

   /// invasive memory test
   /// @param base start address of the memory region to probe
   /// @param blocks number of 64-bit blocks to probe
   /// @param verbosity level of diagnostic output
   static int64_t probe_memory(uint64_t * base, uint64_t blocks, int verbosity);

   /// outcome of sem_wait_safe(): why the wait ended.
   enum Wait_result
      {
        WAIT_OK = 0,     ///< the semaphore was actually posted
        WAIT_CTRL_C,     ///< the user hit ^C while waiting
        WAIT_TIMEOUT,    ///< timeout_seconds elapsed unposted
        WAIT_ERROR,      ///< sem_timedwait() itself failed unexpectedly
      };

   /// sem_wait() that (a) retries on EINTR, except for the user's own ^C,
   /// and (b) gives up after timeout_seconds instead of blocking forever
   /// if the wait is never satisfied at all. A plain sem_wait() returns
   /// prematurely (with the semaphore left un-posted) if a signal arrives
   /// while blocked, regardless of what that signal is about; a premature,
   /// signal-triggered return silently reintroduces exactly the race the
   /// wait exists to prevent unless the caller checks sem_wait()'s return
   /// value, which most callers don't. Incidental signals are therefore
   /// retried transparently -- but if the wait is never satisfied at all
   /// (a genuine deadlock or lost wakeup on the other side), blindly
   /// retrying forever would make that hang un-interruptible, which is
   /// worse than the original race: the user's own ^C (GNU APL installs a
   /// real SIGINT handler, see main.cc) must still be able to break out,
   /// so EINTR caused by an already-raised attention ends the wait instead
   /// of being retried -- and so must a wait that is simply never going
   /// to be satisfied.
   ///
   /// This function may be called from threads other than the interpreter
   /// thread (e.g. a GUI driver's own callback/event-loop threads), so it
   /// must never raise a C++/APL exception itself (Workspace/SI access is
   /// not safe from those threads). It returns WAIT_OK if the semaphore
   /// was actually posted, or a specific WAIT_xxx reason otherwise --
   /// logged to CERR either way since that is safe from any thread.
   /// Callers running on the interpreter thread that want a proper APL
   /// error on failure should catch the non-WAIT_OK outcomes themselves
   /// and raise their own error (see Quad_PLOT::sem_wait_safe_I() for an
   /// example).
   /// @param sema the semaphore to wait for
   /// @param what human-readable description of what is being waited
   ///        for, used only in the CERR diagnostic printed on failure
   /// @param timeout_seconds how long to wait before giving up
#if HAVE_SEM_TIMEDWAIT

   static inline Wait_result
   sem_wait_safe(sem_t * sema, const char * what, int timeout_seconds)
      {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += timeout_seconds;

        for (;;)
           {
             if (sem_timedwait(sema, &deadline) == 0)   return WAIT_OK;

             if (errno == EINTR)
                {
                  if (InterruptContext::attention_is_raised())
                     {
                       CERR << "*** Sys::sem_wait_safe(): ^C hit while "
                               "waiting in sem_timedwait() for " << what
                            << endl;
                       return WAIT_CTRL_C;
                     }
                  continue;
                }

             if (errno == ETIMEDOUT)
                {
                  CERR << "*** Sys::sem_wait_safe(): timed out after "
                       << timeout_seconds << "s waiting for " << what
                       << endl;
                  return WAIT_TIMEOUT;
                }

             CERR << "*** Sys::sem_wait_safe(): sem_timedwait() while "
                     "waiting for " << what << " failed unexpectedly: "
                  << strerror(errno) << endl;
             return WAIT_ERROR;
           }
      }

#else // !HAVE_SEM_TIMEDWAIT -- e.g. macOS: Darwin's <semaphore.h> declares
      // sem_wait()/sem_trywait() for unnamed semaphores but never
      // sem_timedwait() at all (a compile-time absence, not the runtime
      // one HAVE_SEM_INIT works around). Poll sem_trywait() against a
      // deadline computed the same way, sleeping briefly between polls
      // instead of blocking natively in the kernel; the ^C/timeout/error
      // outcomes and their CERR wording are unchanged.

   static inline Wait_result
   sem_wait_safe(sem_t * sema, const char * what, int timeout_seconds)
      {
      enum { POLL_NANOSECONDS = 10 * 1000 * 1000 };   // 10ms between polls

        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += timeout_seconds;

        for (;;)
           {
             if (sem_trywait(sema) == 0)   return WAIT_OK;

             if (errno == EINTR)   continue;   // a plain, unrelated signal

             if (errno != EAGAIN)
                {
                  CERR << "*** Sys::sem_wait_safe(): sem_trywait() while "
                          "waiting for " << what << " failed unexpectedly: "
                       << strerror(errno) << endl;
                  return WAIT_ERROR;
                }

             if (InterruptContext::attention_is_raised())
                {
                  CERR << "*** Sys::sem_wait_safe(): ^C hit while waiting "
                          "in sem_trywait() for " << what << endl;
                  return WAIT_CTRL_C;
                }

             struct timespec now;
             clock_gettime(CLOCK_REALTIME, &now);
             if (now.tv_sec > deadline.tv_sec ||
                 (now.tv_sec == deadline.tv_sec &&
                  now.tv_nsec >= deadline.tv_nsec))
                {
                  CERR << "*** Sys::sem_wait_safe(): timed out after "
                       << timeout_seconds << "s waiting for " << what
                       << endl;
                  return WAIT_TIMEOUT;
                }

             struct timespec poll_delay = { 0, POLL_NANOSECONDS };
             nanosleep(&poll_delay, 0);
           }
      }

#endif // HAVE_SEM_TIMEDWAIT
};
//────────────────────────────────────────────────────────────────────────────
/* Child processes and SIGCHLD.

   SIGCHLD keeps a handler (installed by sys_init_SIGCHLD() in main()) for
   the entire session. It reaps only the child processes that were
   registered with sys_auto_reap(): children that nobody waits for, e.g.
   those of ⎕FIO[57] (fork/execve). Every other child, in particular those
   of popen(), is left alone, so that pclose() (and system()) always find
   their child and return its real exit status.

   This replaces an earlier scheme that switched SIGCHLD between SIG_IGN
   (auto-reap all children) and SIG_DFL (while popen() streams were open,
   counted by a nesting counter, Bugs30 #19): a child of ⎕FIO[57] that
   exited while some popen() stream was open became a zombie for good
   (Blake McBride, Bugs32 #11), since setting SIG_IGN later does not reap
   existing zombies. Unlike SIG_IGN, a handler is also reset by execve(),
   so that programs started by GNU APL get a default SIGCHLD.
 */
/// the maximum number of not yet reaped children registered at the same
/// time. 256 was easily reached by a server-style workspace that starts a
/// ⎕FIO[57] child per request (Bugs34 #38). The table cannot grow while the
/// SIGCHLD handler (possibly in another thread) may be using it.
enum { MAX_AUTO_REAP = 4096 };

/// the pids of children to be reaped by sys_SIGCHLD_handler() (0 = free)
inline volatile sig_atomic_t * sys_auto_reap_pids()
{
   static volatile sig_atomic_t pids[MAX_AUTO_REAP];
   return pids;
}
//────────────────────────────────────────────────────────────────────────────
/// reap the registered children that have terminated (async-signal-safe)
inline void sys_SIGCHLD_handler(int)
{
#if ! MINGW_SRC
const int saved_errno = errno;
volatile sig_atomic_t * pids = sys_auto_reap_pids();
   for (int p = 0; p < MAX_AUTO_REAP; ++p)
       {
         const pid_t pid = pids[p];
         if (pid <= 0)   continue;
         int status;
         const pid_t ret = waitpid(pid, &status, WNOHANG);
         if (ret == pid || (ret == -1 && errno == ECHILD))   pids[p] = 0;
       }
   errno = saved_errno;
#endif // ! MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
/// install sys_SIGCHLD_handler() (once, at startup)
inline void sys_init_SIGCHLD()
{
#if ! MINGW_SRC
struct sigaction sa;
   memset(&sa, 0, sizeof(sa));
   sa.sa_handler = &sys_SIGCHLD_handler;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
   sigaction(SIGCHLD, &sa, 0);
#endif // ! MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
/// register child \b pid, which nobody waits for, to be reaped when it
/// terminates
inline void sys_auto_reap(pid_t pid)
{
#if ! MINGW_SRC
   if (pid <= 0)   return;

   // block SIGCHLD while the table is modified, then reap once, in case
   // the child has terminated already (before it was registered)
   //
sigset_t block, old;
   sigemptyset(&block);
   sigaddset(&block, SIGCHLD);
   pthread_sigmask(SIG_BLOCK, &block, &old);

volatile sig_atomic_t * pids = sys_auto_reap_pids();
bool registered = false;
   for (int p = 0; p < MAX_AUTO_REAP; ++p)
       {
         if (pids[p] == 0)   { pids[p] = pid;   registered = true;   break; }
       }
   sys_SIGCHLD_handler(SIGCHLD);

   // the pid used to be dropped silently, so that the child became a
   // zombie for good: at least say so (once)
   //
   if (!registered)
      {
        static bool warned = false;
        if (!warned)
           {
             warned = true;
             std::cerr << "*** more than " << int(MAX_AUTO_REAP)
                       << " child processes are running; children that"
                          " terminate from now on may remain zombies"
                       << std::endl;
           }
      }

   pthread_sigmask(SIG_SETMASK, &old, 0);
#endif // ! MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
/// open a pipe for reading or writing (see "Child processes and SIGCHLD"
/// above)
/// @param command shell command string to execute
/// @param mode open mode passed to popen() (e.g. "r" or "w")
/// @return the FILE pointer returned by popen(), or 0 on failure
inline FILE * sys_popen(const char * command, const char * mode)
{
   return popen(command, mode);
}
//────────────────────────────────────────────────────────────────────────────
/// close a pipe opened with sys_popen()
/// @param stream FILE pointer returned by sys_popen()
/// @return the exit status returned by pclose()
inline int sys_pclose(FILE * stream)
{
   return pclose(stream);
}
//════════════════════════════════════════════════════════════════════════════
/// A MAII compliant wrapper for popen()/pclose()
class PipeReader
{
public:
   /// default constructor for placement new
   PipeReader() : fp(0) {}


   /// constructor
   /// @param command shell command to open as a readable pipe
   PipeReader(const char * command)
      {
        fp = sys_popen(command, "r");
      }

   /// destructor: pclose fp (unless already closed).
   ~PipeReader()
      {
        if (fp)   sys_pclose(fp);
      }

   int fgetc() const
      {
        if (fp)   return ::fgetc(fp);
        return EOF;
      }

   /// @param buffer destination buffer for the line
   /// @param buflen capacity of buffer in bytes
   char * fgets(char * buffer, int buflen) const
      {
        if (fp)   return ::fgets(buffer, buflen, fp);
        return 0;
      }

   /// @param buffer destination buffer for the data
   /// @param buflen maximum number of bytes to read
   size_t fread(char * buffer, int buflen) const
      {
        if (fp)   return ::fread(buffer, 1, buflen, fp);
        return 0;
      }

   /// return \b true if ptr is not open
   bool operator !() const
      { return fp == 0; }

   /// return \b true if ptr is (open)
   bool operator +() const
      { return fp != 0; }

   int close()
      {
       if (fp)
          {
            const int ret = sys_pclose(fp);
            fp = 0;
            return ret;
          }
        return 0;
      }

protected:
   FILE * fp;
};
//════════════════════════════════════════════════════════════════════════════
/// A MAII compliant wrapper for fopen()/fclose()
class FileReader
{
public:
   /// default constructor for placement new
   FileReader() : fp(0) {}


   /// constructor: from alrady open FILE *
   /// @param file already-open FILE pointer to manage
   FileReader(FILE * file)
      {
        fp = file;
      }

   /// constructor: from open fd
   /// @param fd open file descriptor to wrap
   FileReader(int fd)
      {
        fp = fdopen(fd, "r");
      }

   /// constructor: from filename
   /// @param filename path of the file to open for reading
   FileReader(const char * filename)
      {
        fp = fopen(filename, "r");
      }

   /// destructor: pclose fp (unless already closed).
   ~FileReader()
      {
        if (fp)   fclose(fp);
      }

   int fgetc() const
      {
        if (fp)   return ::fgetc(fp);
        return EOF;
      }

   /// @param buffer destination buffer for the line
   /// @param buflen capacity of buffer in bytes
   char * fgets(char * buffer, int buflen) const
      {
        if (fp)   return ::fgets(buffer, buflen, fp);
        return 0;
      }

   /// @param buffer destination buffer for the data
   /// @param buflen maximum number of bytes to read
   size_t fread(char * buffer, int buflen) const
      {
        if (fp)   return ::fread(buffer, 1, buflen, fp);
        return 0;
      }

   FILE * get_FILE() const
      { return fp; }

   /// return \b true if ptr is not open
   bool operator !() const
      { return fp == 0; }

   /// return \b true if ptr is (open)
   bool operator +() const
      { return fp != 0; }

protected:
   FILE * fp;
};
//════════════════════════════════════════════════════════════════════════════
/// A MAII compliant wrapper for fopen()/fclose()
class FileWriter
{
public:
   /// default constructor for placement new
   FileWriter() : fp(0) {}


   /// constructor: from alrady open FILE *
   /// @param file already-open FILE pointer to manage
   FileWriter(FILE * file)
      {
        fp = file;
      }

   /// constructor: from open fd
   /// @param fd open file descriptor to wrap
   /// @param mode fopen-style open mode string
   FileWriter(int fd, const char * mode = "w")
      {
        fp = fdopen(fd, mode);
      }

   /// constructor: from filename
   /// @param filename path of the file to open for writing
   /// @param mode fopen-style open mode string
   FileWriter(const char * filename, const char * mode = "w")
      {
        fp = fopen(filename, mode);
      }

   /// destructor: pclose fp (unless already closed).
   ~FileWriter()
      {
        if (fp)   fclose(fp);
      }

   FILE * get_FILE() const
      { return fp; }

   /// return \b true if ptr is not open
   bool operator !() const
      { return fp == 0; }

   /// return \b true if ptr is (open)
   bool operator +() const
      { return fp != 0; }

   /// @param buffer source data to write
   /// @param bufsize number of bytes to write
   size_t fwrite(const void * buffer, size_t bufsize)
      { if (fp)   return ::fwrite(buffer, 1, bufsize, fp);
        return 0;
      }

protected:
   FILE * fp;
};
//════════════════════════════════════════════════════════════════════════════

//════════════════════════════════════════════════════════════════════════════
// Platform-independent wrappers for socket option functions.
// On POSIX, setsockopt()/getsockopt() take const void * / void * for optval.
// On Windows (winsock2) the same argument is const char * / char *.

#if HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif

#if HAVE_WINSOCK2_H
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

#if HAVE_SYS_SOCKET_H || HAVE_WINSOCK2_H

/// platform-independent setsockopt()
inline int
sys_setsockopt(int fd, int level, int optname,
               const void * optval, socklen_t optlen)
{
#if HAVE_WINSOCK2_H
   return setsockopt(fd, level, optname,
                     reinterpret_cast<const char *>(optval), optlen);
#else
   return setsockopt(fd, level, optname, optval, optlen);
#endif
}

/// platform-independent getsockopt()
inline int
sys_getsockopt(int fd, int level, int optname,
               void * optval, socklen_t * optlen)
{
#if HAVE_WINSOCK2_H
   return getsockopt(fd, level, optname,
                     reinterpret_cast<char *>(optval), optlen);
#else
   return getsockopt(fd, level, optname, optval, optlen);
#endif
}

#endif // HAVE_SYS_SOCKET_H || HAVE_WINSOCK2_H
//════════════════════════════════════════════════════════════════════════════

#endif // SYS_HH_DEFINED

