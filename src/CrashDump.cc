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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// ucontext_t comes with <signal.h> (CrashDump.hh). <ucontext.h> is only
// needed for REG_RIP on GNU/Linux; on macOS it is deprecated and #errors
// unless _XOPEN_SOURCE is defined (which in turn hides NSIG & Co.).
#ifdef __linux__
# include <ucontext.h>
#endif
#include <sys/stat.h>

#if ! MINGW_SRC
# include <sys/mman.h>
# include <sys/utsname.h>
#endif

#ifdef __GLIBC__
# include <gnu/libc-version.h>
#endif

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "Backtrace.hh"
#include "Cmd_DIAG.hh"
#include "Command.hh"
#include "CrashDump.hh"
#include "Function.hh"
#include "IndexExpr.hh"
#include "InputFile.hh"
#include "IO_Files.hh"
#include "LineInput.hh"
#include "Parallel.hh"
#include "Prefix.hh"
#include "StateIndicator.hh"
#include "UserFunction.hh"
#include "UserPreferences.hh"
#include "Workspace.hh"

using namespace std;

sigjmp_buf CrashDump::recovery_point;
volatile sig_atomic_t CrashDump::recovery_point_valid = 0;
volatile sig_atomic_t CrashDump::in_handler = 0;
volatile sig_atomic_t CrashDump::other_thread_crashed = 0;
pthread_t CrashDump::handler_thread;
pthread_t CrashDump::main_thread;
char * CrashDump::signal_stack = 0;

/// the crash signals handled by CrashDump
static const int crash_signals[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };

/// seconds that handling a crash may take. A hang (e.g. a malloc() lock held
/// by the crashed code) then ends the interpreter
enum { DUMP_TIMEOUT = 30 };

//────────────────────────────────────────────────────────────────────────────
/// write \b str to stderr (async-signal-safe)
static void
safe_write(const char * str)
{
   if (write(STDERR_FILENO, str, strlen(str))) { /* nothing to do */ }
}
//════════════════════════════════════════════════════════════════════════════
void
CrashDump::init()
{
#if ! MINGW_SRC
   main_thread = pthread_self();

   // an alternate signal stack, so that a stack overflow (which is also a
   // SIGSEGV) can be handled. The handler does a lot (formatting, reading
   // files, ]STATUS) so the stack is generous.
   //
   enum { STACK_SIZE = 1024*1024 };
   signal_stack = new char[STACK_SIZE];
   stack_t ss;
   memset(&ss, 0, sizeof(ss));
   ss.ss_sp = signal_stack;
   ss.ss_size = STACK_SIZE;
   ss.ss_flags = 0;
   const bool have_stack = sigaltstack(&ss, 0) == 0;

struct sigaction action;
   memset(&action, 0, sizeof(struct sigaction));
   action.sa_sigaction = &crash_handler;

   // SA_NODEFER: a crash while handling a crash re-enters crash_handler()
   // (which then exits) instead of killing the interpreter silently.
   action.sa_flags = SA_SIGINFO | SA_NODEFER;
   if (have_stack)   action.sa_flags |= SA_ONSTACK;
   for (const int sig : crash_signals)   sigaction(sig, &action, 0);

   memset(&action, 0, sizeof(struct sigaction));
   action.sa_handler = &alarm_handler;
   sigaction(SIGALRM, &action, 0);
#endif // ! MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
const char *
CrashDump::signal_name(int sig)
{
   switch(sig)
      {
        case SIGSEGV: return "SIGSEGV";
#ifdef SIGBUS
        case SIGBUS:  return "SIGBUS";
#endif
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        case SIGABRT: return "SIGABRT";
        default:      break;
      }
   return "unknown signal";
}
//────────────────────────────────────────────────────────────────────────────
const char *
CrashDump::signal_description(int sig)
{
   switch(sig)
      {
        case SIGSEGV: return "SEGMENTATION FAULT";
#ifdef SIGBUS
        case SIGBUS:  return "BUS ERROR";
#endif
        case SIGFPE:  return "FLOATING POINT EXCEPTION";
        case SIGILL:  return "ILLEGAL INSTRUCTION";
        case SIGABRT: return "ABORTED";
        default:      break;
      }
   return "UNKNOWN SIGNAL";
}
//────────────────────────────────────────────────────────────────────────────
void
CrashDump::alarm_handler(int)
{
   safe_write("\n*** Collecting the crash information hangs. Giving up.\n");
   LineInput::restore_termios();
   _exit(3);
}
//────────────────────────────────────────────────────────────────────────────
void
CrashDump::crash_handler(int sig, siginfo_t * info, void * context)
{
#if ! MINGW_SRC
   // a pthread_cancel() of this thread (e.g. of a parallel worker at exit)
   // must not act on the cancellation points (write(), pause(), ...) in
   // here: that unwinds through the signal frame and ends in terminate().
   //
   { int old_state;   pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state); }

   if (in_handler)   // a crash while a crash is being handled
      {
        if (!pthread_equal(pthread_self(), handler_thread))
           {
             // another thread crashed while the first crash is being handled
             // (e.g. parallel worker threads). Let the first crash end the
             // interpreter (the process cannot recover from that) and park
             // this thread. The alarm ends the interpreter if that hangs.
             //
             other_thread_crashed = 1;
             safe_write("\n*** another thread crashed as well (");
             safe_write(signal_description(sig));
             safe_write(")\n");
             for (;;)   pause();
           }

        // a crash in the crash handler itself: only signal-safe functions
        safe_write("\n\n===================================================\n");
        safe_write(signal_description(sig));
        safe_write(" (while handling a crash)\n");
        Backtrace::show_signal_safe();
        LineInput::restore_termios();
        _exit(3);
      }

   in_handler = 1;
   handler_thread = pthread_self();

   // end the interpreter if handling the crash hangs (e.g. on a malloc()
   // lock held by the crashed code). Disarmed in recover().
   //
   alarm(DUMP_TIMEOUT);

   // the PC of the crash (for the line number of the crashed function)
   //
#if defined(__linux__) && defined(__x86_64__)
   Backtrace::signal_PC = reinterpret_cast<const void *>(
         static_cast<ucontext_t *>(context)->uc_mcontext.gregs[REG_RIP]);
#elif defined(__linux__) && defined(__aarch64__)
   Backtrace::signal_PC = reinterpret_cast<const void *>(
         static_cast<ucontext_t *>(context)->uc_mcontext.pc);
#endif

   // 1. the async-signal-safe part: tell what happened and show the raw
   // backtrace (to stderr and into a pipe from which write_dump() reads
   // it back for the crash-dump file).
   //
   safe_write("\n\n===================================================\n");
   safe_write(signal_description(sig));
   safe_write("\n");

int pipe_fds[2] = { -1, -1 };
   if (pipe(pipe_fds) == 0)
      {
        // never block (a full pipe only shortens the raw backtrace)
        fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK);
        fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK);
      }
   // the raw backtrace goes to the terminal only if no readable one
   // (write_dump() below) follows.
   //
const bool testing = InputFile::is_validating() && InputFile::current_file();
   Backtrace::show_signal_safe(pipe_fds[1], testing);
   if (pipe_fds[1] != -1)   close(pipe_fds[1]);

#if PARALLEL_ENABLED
   CERR << "thread: " << reinterpret_cast<const void *>(pthread_self()) << endl;
   Thread_context::print_all(CERR);
#endif // PARALLEL_ENABLED

   // count errors
   IO_Files::assert_error();

   // the old behaviour: report the crash and exit (running testcases, after
   // a nested crash, or when the crash cannot be recovered from).
   //
const bool can_recover = recovery_point_valid && !testing &&
                         !IO_Files::exit_on_error() &&
                         pthread_equal(pthread_self(), main_thread);

   // 2. the best-effort part: write the crash-dump file. This uses
   // functions that are not async-signal-safe (malloc() & Co.), therefore
   // a crash in it is caught by the in_handler check above, and a hang by
   // the alarm.
   //
   if (!testing)
      {
        string raw_backtrace;
        if (pipe_fds[0] != -1)
           {
             char buffer[4096];
             for (;;)
                 {
                   const ssize_t len = read(pipe_fds[0], buffer,
                                            sizeof(buffer));
                   if (len <= 0)   break;
                   raw_backtrace.append(buffer, len);
                 }
             close(pipe_fds[0]);
           }

        const string path = write_dump(sig, info ? info->si_addr : 0,
                                       raw_backtrace);

        CERR << endl
             << "*** GNU APL crashed (" << signal_name(sig) << ", "
             << signal_description(sig) << ")." << endl;
        if (path.size())
           CERR <<
"*** A crash report was written to:\n"
"***\n"
"***     " << path << "\n"
"***\n"
"*** Please send this file to bug-apl@gnu.org, together with a short\n"
"*** description of what you did. It contains the APL statement that\n"
"*** crashed, the shapes, ranks, and depths (but not the contents) of the\n"
"*** values involved, and information about GNU APL and your system.\n";
        else
           CERR <<
"*** Writing a crash report failed. Please report the crash (and the\n"
"*** output above) to bug-apl@gnu.org.\n";
      }

   // other threads that crashed meanwhile are parked (see above), so the
   // interpreter cannot continue
   if (can_recover && !other_thread_crashed)   siglongjmp(recovery_point, sig);

   IO_Files::report_abnormal_exit();

   // restore terminal setting and exit. A normal )OFF (which exit()s and
   // runs the cleanup) only from the main thread, and only if no other
   // thread is parked in here.
   //
   LineInput::restore_termios();
   if (other_thread_crashed ||
       !pthread_equal(pthread_self(), main_thread))   _exit(3);
   Command::cmd_OFF(3);
#endif // ! MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
void
CrashDump::recover(int sig)
{
   // the statement that crashed (and every function that called it) cannot
   // continue. Clearing the SI may crash as well, which is then a nested
   // crash (in_handler is still set) and exits.
   //
   Workspace::clear_SI(CERR);
   alarm(0);
   in_handler = 0;

   CERR <<
"*** The SI was cleared and GNU APL continues. However, the workspace may\n"
"*** have been damaged by the crash: )SAVE it under a different name and\n"
"*** restart the interpreter as soon as possible.\n"
"===================================================" << endl;
}
//────────────────────────────────────────────────────────────────────────────
bool
CrashDump::provoke(const char * name)
{
#if MINGW_SRC
   return false;
#else
   if (!strcmp(name, "SEGV"))
      {
        // a volatile pointer, so that the compiler cannot see (and warn
        // about) the invalid address
        volatile char * volatile bad_address =
                        reinterpret_cast<volatile char *>(16);
        *bad_address = 0;
        raise(SIGSEGV);
        return true;
      }

   if (!strcmp(name, "BUS"))   // e.g. an I/O error on a memory-mapped file
      {
        // access a page of a memory-mapped file beyond the end of the file
        char filename[] = "/tmp/gnu-apl-SIGBUS-XXXXXX";
        const int fd = mkstemp(filename);
        if (fd != -1)
           {
             unlink(filename);
             const long page = sysconf(_SC_PAGESIZE);
             void * map = mmap(0, page, PROT_READ, MAP_SHARED, fd, 0);
             close(fd);
             if (map != MAP_FAILED)
                {
                  const char c = *reinterpret_cast<volatile char *>(map);
                  munmap(map, page);
                  if (c == 42)   return false;   // never (use c)
                }
           }
        raise(SIGBUS);   // the mapped file did not raise SIGBUS
        return true;
      }

   if (!strcmp(name, "FPE"))   // integer division by zero
      {
        volatile int a = 1;
        volatile int b = 0;
        a = a / b;   // SIGFPE on x86, but not on all CPUs
        raise(SIGFPE);
        return true;
      }

   if (!strcmp(name, "ILL"))
      {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_trap();   // ud2, i.e. SIGILL (other CPUs: SIGTRAP)
#endif
        raise(SIGILL);
        return true;
      }

   if (!strcmp(name, "ABRT"))
      {
        abort();
        return true;
      }

   return false;
#endif // MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
/// run \b fun with COUT, CERR, UERR, cout, and cerr redirected into a string
/// and return that string.
template<typename F>
static string
capture(F fun)
{
ostringstream out;
streambuf * old_COUT = COUT.rdbuf(out.rdbuf());
streambuf * old_CERR = CERR.rdbuf(out.rdbuf());
streambuf * old_UERR = UERR.rdbuf(out.rdbuf());
streambuf * old_cout = cout.rdbuf(out.rdbuf());
streambuf * old_cerr = cerr.rdbuf(out.rdbuf());

   try { fun(); }
   catch (...) { out << "\n*** (failed)\n"; }

   cerr.rdbuf(old_cerr);
   cout.rdbuf(old_cout);
   UERR.rdbuf(old_UERR);
   CERR.rdbuf(old_CERR);
   COUT.rdbuf(old_COUT);
   return out.str();
}
//────────────────────────────────────────────────────────────────────────────
/// print the shape, rank, depth, and cell types of \b val (but not its
/// contents)
static void
describe_value(ostream & out, const cValue * val)
{
   if (val == 0)   { out << "(no value)";   return; }

   out << "rank " << val->get_rank() << ", shape";
   if (val->get_rank() == 0)   out << " (scalar)";
   loop(r, val->get_rank())   out << " " << val->get_shape_item(r);
   out << ", depth " << val->compute_depth();

   // the cell types (of at most the first 10000 cells)
const ShapeItem count = val->nz_element_count();
ShapeItem chars = 0, ints = 0, reals = 0, complexes = 0, nested = 0, lvals = 0;
   for (ShapeItem c = 0; c < count && c < 10000; ++c)
       {
         if      (val->is_character_cell(c))   ++chars;
         else if (val->is_integer_cell(c))     ++ints;
         else if (val->is_complex_cell(c))     ++complexes;
         else if (val->is_pointer_cell(c))     ++nested;
         else if (val->is_lval_cell(c))        ++lvals;
         else                                  ++reals;
       }

   out << ", cells:";
   if (chars)       out << " " << chars     << " char";
   if (ints)        out << " " << ints      << " int";
   if (reals)       out << " " << reals     << " real";
   if (complexes)   out << " " << complexes << " complex";
   if (nested)      out << " " << nested    << " nested";
   if (lvals)       out << " " << lvals     << " lval";
   if (count > 10000)   out << " (first 10000 of " << count << ")";
}
//────────────────────────────────────────────────────────────────────────────
/// describe \b tok (a token on the prefix parser stack)
static void
describe_token(ostream & out, const Token & tok)
{
   out << Token::short_class_name(tok.get_tag()) << ": ";
   switch(tok.get_ValueType())
      {
        case TV_VAL:
             describe_value(out, tok.get_apl_val().get());
             break;

        case TV_FUN:
             if (const Function * fun = tok.get_function())   out << *fun;
             break;

        case TV_INDEX:
             {
               const IndexExpr & idx = tok.get_index_val();
               out << "index with " << idx.get_rank() << " axes";
               loop(ax, idx.get_rank())
                  {
                    out << "\n            axis " << ax << ": ";
                    if (const cValue * val = idx.get_axis_value(ax))
                       describe_value(out, val);
                    else
                       out << "(elided)";
                  }
             }
             break;

        default:
             out << tok;
      }
}
//────────────────────────────────────────────────────────────────────────────
string
CrashDump::crash_info(int sig, const void * address)
{
ostringstream out;
   out << "signal:  " << signal_name(sig) << " (" << signal_description(sig)
       << ")" << endl
       << "address: " << address << endl;

   {
     char tbuf[100];
     const time_t now = time(0);
     struct tm tm;
     strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S %Z",
              localtime_r(&now, &tm));
     out << "time:    " << tbuf << endl;
   }

   out << endl << "=== )SIS ===" << endl
       << capture([](){ Workspace::list_SI(CERR, SIM_SIS); });

   // the prefix parser stack of every SI entry: the tokens of the
   // statement being reduced, i.e. the function(s), operator(s), and their
   // arguments and axes.
   //
   out << endl << "=== values involved (SI entries, newest first) ==="
       << endl;
   // the symbols localized by the functions above the current SI level.
   // Their value at that level is not the current one (and is not shown).
   //
std::set<const Symbol *> localized;
   for (const StateIndicator * si = Workspace::SI_top(); si;
        si = si->get_parent())
       {
         out << endl << "SI level " << si->get_level() << ": "
             << capture([si](){ CERR << si->function_name()
                                     << "[" << si->get_line() << "]"; })
             << endl;

         // the arguments (and the result) of a user-defined function
         //
         if (const Executable * exec = si->get_executable())
         if (const UserFunction * ufun = exec->get_exec_ufun())
            {
              const UserFunction_header & header = ufun->get_header();
              const Symbol * header_syms[] = { header.Z(), header.A(),
                                               header.LO(), header.X(),
                                               header.RO(), header.B() };
              for (const Symbol * sym : header_syms)
                  {
                    if (sym == 0)   continue;
                    out << "    " << capture([sym](){ CERR << sym->get_name(); })
                        << ": " << capture([sym, &localized](){
                              if (localized.count(sym))
                                 CERR << "(shadowed by a called function)";
                              else if (sym->get_NC() == NC_VARIABLE)
                                 describe_value(CERR,
                                                sym->get_apl_value().get());
                              else
                                 CERR << "(not a value)";
                           })
                        << endl;
                  }

              for (const Symbol * sym : header_syms)
                  if (sym)   localized.insert(sym);
              loop(l, header.local_var_count())
                  localized.insert(header.get_local_var(l));
            }

         const Prefix & prefix = si->get_prefix();
         if (prefix.ssize() == 0)   out << "    (no tokens)" << endl;
         loop(t, prefix.ssize())
             {
               out << "    token " << t << ": "
                   << capture([&prefix, t](){
                         describe_token(CERR, prefix.at(t).get_token()); })
                   << endl;
             }
       }

   return out.str();
}
//────────────────────────────────────────────────────────────────────────────
/// return the content of the (small) file \b path
static string
file_content(const string & path)
{
ifstream in(path.c_str());
   if (!in)   return "(does not exist)\n";
ostringstream out;
   out << in.rdbuf();
   return out.str();
}
//────────────────────────────────────────────────────────────────────────────
string
CrashDump::environment_info()
{
ostringstream out;

   out << "=== version ===" << endl
       << capture([](){ UserPreferences::show_version(CERR); }) << endl;

   out << "=== configure options ===" << endl
       << capture([](){ UserPreferences::show_configure_options(); })
       << endl;

   out << "=== ]STATUS ===" << endl
       << capture([](){ Cmd_DIAG::cmd_STATUS(CERR); }) << endl;

#if ! MINGW_SRC
   {
     struct utsname uts;
     out << "=== uname -a ===" << endl;
     if (uname(&uts) == 0)
        out << uts.sysname << " " << uts.nodename << " " << uts.release
            << " " << uts.version << " " << uts.machine << endl;
     out << endl;
   }
#endif

   out << "=== word size ===" << endl
       << "sizeof(void *): " << sizeof(void *) << endl << endl;

   // the relevant lines of /proc/cpuinfo and /proc/meminfo (Linux)
   //
   {
     out << "=== CPU ===" << endl;
     istringstream cpuinfo(file_content("/proc/cpuinfo"));
     int cpus = 0;
     string model;
     for (string line; getline(cpuinfo, line);)
         {
           if (line.compare(0, 9, "processor") == 0)   ++cpus;
           if (model.empty() && line.compare(0, 10, "model name") == 0)
              model = line;
         }
     out << model << endl << "CPU(s): " << cpus << endl << endl;

     out << "=== memory ===" << endl;
     istringstream meminfo(file_content("/proc/meminfo"));
     for (string line; getline(meminfo, line);)
         {
           if (line.compare(0, 9, "MemTotal:") == 0 ||
               line.compare(0, 13, "MemAvailable:") == 0 ||
               line.compare(0, 10, "SwapTotal:") == 0)   out << line << endl;
         }
     out << endl;
   }

   {
     const char * display = getenv("DISPLAY");
     out << "=== $DISPLAY ===" << endl
         << (display ? display : "<unset>") << endl << endl;
   }

   out << "=== compiler ===" << endl
#ifdef __VERSION__
       << __VERSION__ << endl
#endif
       << endl;

#ifdef __GLIBC__
   out << "=== glibc ===" << endl << gnu_get_libc_version() << endl << endl;
#endif

   // the preferences files
   //
   {
     const char * home = getenv("HOME");
     const string etc = string(apl_DIR__sysconf) + "/gnu-apl.d/";
     vector<string> files;
     files.push_back(etc + "preferences");
     files.push_back(etc + "parallel_thresholds");
     if (home)
        {
          files.push_back(string(home) + "/.gnu-apl/preferences");
          files.push_back(string(home) + "/.gnu-apl/parallel_thresholds");
          files.push_back(string(home) + "/.config/gnu-apl/preferences");
          files.push_back(string(home)
                          + "/.config/gnu-apl/parallel_thresholds");
        }

     for (const string & file : files)
         {
           out << "=== preferences file " << file << " ===" << endl;
           istringstream content(file_content(file));
           for (string line; getline(content, line);)
               {
                 // the settings only (the comments are the same everywhere)
                 size_t pos = line.find_first_not_of(" \t");
                 if (pos == string::npos || line[pos] == '#')   continue;
                 out << "   " << line << endl;
               }
           out << endl;
         }
   }

   return out.str();
}
//────────────────────────────────────────────────────────────────────────────
string
CrashDump::write_dump(int sig, const void * address,
                      const string & raw_backtrace)
{
   // the directory: ~/.config/gnu-apl/ if it exists, otherwise /tmp
   //
string dir = "/tmp";
   if (const char * home = getenv("HOME"))
      {
        const string config = string(home) + "/.config/gnu-apl";
        struct stat st;
        if (stat(config.c_str(), &st) == 0 && S_ISDIR(st.st_mode) &&
            access(config.c_str(), W_OK) == 0)   dir = config;
      }

char stamp[40];
   {
     const time_t now = time(0);
     struct tm tm;
     strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", localtime_r(&now, &tm));
   }

const string name = string("gnu-apl-crash-") + stamp + "-"
                  + to_string(getpid());

vector<Tar_member> members;
   members.push_back({ "README.txt",
"This is a crash report of GNU APL. Please send it to bug-apl@gnu.org,\n"
"together with a short description of what you did.\n"
"\n"
"   crash.txt        the signal, the )SIS, and the shapes, ranks, and\n"
"                    depths of the values involved\n"
"   backtrace.txt    the C++ function call stack of the crash\n"
"   environment.txt  the GNU APL version and configuration, ]STATUS,\n"
"                    the preferences files, and the system\n" });

   // each part separately, so that a failing part loses only itself
   //
string crash;
   try { crash = crash_info(sig, address); }
   catch (...) { crash += "\n*** (collecting the crash information failed)\n"; }
   members.push_back({ "crash.txt", crash });

const string demangled = capture([](){ BACKTRACE });
   CERR << demangled;   // the readable backtrace (also on the terminal)
   members.push_back({ "backtrace.txt", raw_backtrace + "\n" + demangled });

string environment;
   try { environment = environment_info(); }
   catch (...) { environment += "\n*** (collecting the environment failed)\n"; }
   members.push_back({ "environment.txt", environment });

   // keep the last 3 crash reports, like the log files in /var/log:
   // last_crash.tar (newest), last_crash.1.tar, and last_crash.2.tar.
   // /tmp is shared by all users, so the files there have the uid as well.
   //
const string base = dir + (dir == "/tmp" ? "/gnu-apl-" + to_string(getuid())
                                           + "-last_crash"
                                         : string("/last_crash"));
   rename((base + ".1.tar").c_str(), (base + ".2.tar").c_str());
   rename((base + ".tar").c_str(),   (base + ".1.tar").c_str());
   unlink((base + ".tar").c_str());   // in case the rename failed

const string path = base + ".tar";
   if (write_tar(path, name, members))   return path;
   return "";
}
//────────────────────────────────────────────────────────────────────────────
bool
CrashDump::write_tar(const string & path, const string & dir,
                     const vector<Tar_member> & members)
{
const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
   if (fd == -1)   return false;   // errno == EEXIST: try another name

bool ok = true;
auto put = [&ok, fd](const char * data, size_t len)
   {
     if (ok && write(fd, data, len) != ssize_t(len))   ok = false;
   };

const time_t now = time(0);
   for (const Tar_member & member : members)
       {
         // the size field has 11 octal digits, i.e. less than 8 GiB
         if (member.content.size() > 077777777777UL)   { ok = false;   break; }

         // a POSIX ustar header (see 'info tar', "Standard")
         //
         char header[512];
         memset(header, 0, sizeof(header));
         snprintf(header +   0, 100, "%s/%s", dir.c_str(),
                  member.name.c_str());                          // name
         snprintf(header + 100,   8, "%07o", 0644);              // mode
         snprintf(header + 108,   8, "%07o", 0);                 // uid
         snprintf(header + 116,   8, "%07o", 0);                 // gid
         snprintf(header + 124,  12, "%011lo",
                  static_cast<unsigned long>(member.content.size())
                  & 077777777777UL);                             // size
         snprintf(header + 136,  12, "%011lo",
                  static_cast<unsigned long>(now) & 077777777777UL);   // mtime
         memset(header + 148, ' ', 8);                           // chksum
         header[156] = '0';                                      // typeflag
         memcpy(header + 257, "ustar", 6);                       // magic
         memcpy(header + 263, "00", 2);                          // version

         unsigned int checksum = 0;
         for (const char c : header)   checksum += uint8_t(c);
         snprintf(header + 148, 8, "%06o", checksum & 0777777);   // ≤ 512*255
         header[155] = ' ';

         put(header, sizeof(header));
         put(member.content.data(), member.content.size());

         const char zeros[512] = { 0 };
         const size_t pad = (512 - member.content.size() % 512) % 512;
         put(zeros, pad);
       }

   // the end of the archive: two zero blocks
   {
     const char zeros[1024] = { 0 };
     put(zeros, sizeof(zeros));
   }

   if (close(fd))   ok = false;
   if (!ok)   unlink(path.c_str());
   return ok;
}
//════════════════════════════════════════════════════════════════════════════
