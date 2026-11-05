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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif // HAVE_NETINET_IN_H

#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif // HAVE_SYS_IOCTL_H

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif // HAVE_SYS_SOCKET_H
#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>   // TCP_NODELAY for ⎕FIO[62]
#endif // HAVE_NETINET_TCP_H

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif // HAVE_SYS_SELECT_H

#include <sys/stat.h>

#if HAVE_SYS_UN_H
#include <sys/un.h>   // for sockaddr_un
#endif // HAVE_SYS_UN_H

#if HAVE_WINSOCK2_H
# include <winsock2.h>
#endif // HAVE_WINSOCK2_H

#include "Bif_OPER2_INNER.hh"
#include "Bif_OPER2_OUTER.hh"
#include "Bif_OPER1_EACH.hh"
#include "Common.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "PointerCell.hh"
#include "CrashDump.hh"
#include "Quad_FIO.hh"
#include "Performance.hh"
#include "Security.hh"
#include "StateIndicator.hh"
#include "SystemLimits.hh"
#include "Tokenizer.hh"
#include "Workspace.hh"

#if MINGW_SRC
#define NOT_MINGW(x) { MINGW_more(__FUNCTION__, __LINE__); }
#else
#define NOT_MINGW(x) x
#define SOCKET(x) x
#endif // MINGW_SRC

GNUC__noreturn void
MINGW_more(const char * fun, int line)
{
   MORE_ERROR() <<
   "Function is not available when GNU is cross-compiled (!) to\n"
   "Windows. On Windows: consider compiling GNU APL under CYGWIN or WSL.";
   DOMAIN_ERROR;
}
extern uint64_t top_of_memory();
uint64_t Quad_FIO::benchmark_cycles_from = 0;
APL_Integer Quad_FIO::TM3_trigger = 0;

std::vector<Quad_FIO::file_entry> Quad_FIO::open_files;
APL_Integer Quad_FIO::last_handle = -1;

//────────────────────────────────────────────────────────────────────────────
void
Quad_FIO::check_path(const UCS_string & path)
{
   // a NUL character would silently truncate the path in its C string
   //
   loop(p, path.size())
      {
        if (path[p] == 0)
           {
             MORE_ERROR() << "⎕FIO: path contains a NUL character (⎕UCS 0)";
             DOMAIN_ERROR;
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
Quad_FIO::Handle_errno::~Handle_errno()
{
   if (!active || last_handle == -1)   return;

const int err = errno;
   loop(h, open_files.size())   // the handle may have been closed
      {
        if (open_files[h].fe_fd == last_handle)
           {
             open_files[h].fe_errno = err;
             break;
           }
      }
}

Quad_FIO  Quad_FIO::fun;

//────────────────────────────────────────────────────────────────────────────
bool
Quad_FIO::TM3_trigger_armed()
{
const APL_Integer trigger = TM3_trigger;
   TM3_trigger = 0;   // one-shot: consume it regardless of the outcome

   if (trigger == TM3_TRIGGER_ARMED)   return true;

   CERR << "NOTE: this ⎕FIO function is disarmed (arm it first with "
        << int(TM3_TRIGGER_ARMED) << " ⎕FIO ¯19) -- doing nothing." << endl;
   return false;
}

// A union holding a sockaddr and a sockaddr_in as to avoid casting
/// between sockaddr and a sockaddr_in
union SockAddr
{
  /// an arbitrary socket address
  sockaddr    addr;

  /// an AF_INET socket address
  sockaddr_in inet;

#if HAVE_SYS_UN_H
  ///  an AF_UNIX socket address
  sockaddr_un uNix;
#endif // HAVE_SYS_UN_H
};

   // CONVENTION: all functions must have an axis argument (like X
   // in A fun[X] B); the axis argument is a function number that selects
   // one of several functions provided by ⎕FIO...
   //
   // If the axis is missing, then a list of functions implemented by
   // ⎕FIO is displayed

//════════════════════════════════════════════════════════════════════════════
const FunctionGroup::function_info Quad_FIO::subfunction_infos[] =
{
#define fio_def(N, name)   { N, #name, "", "", -1 },
#include "Quad_FIO.def"
};
//════════════════════════════════════════════════════════════════════════════
Quad_FIO::Quad_FIO()
   : QuadFunction(TOK_Quad_FIO)
{
enum { count = sizeof(subfunction_infos) / sizeof(*subfunction_infos) };
   init_function_group(subfunction_infos, count, "⎕FIO");

   // init stdin, stdout, stderr, and maybe fd 3 
   //
file_entry f0(stdin,  STDIN_FILENO);    f0.path << "stdin";
file_entry f1(stdout, STDOUT_FILENO);   f1.path << "stdout";
file_entry f2(stderr, STDERR_FILENO);   f2.path << "stderr";
   open_files.push_back(f0);
   open_files.push_back(f1);
   open_files.push_back(f2);

#if ! MINGW_SRC
   if (-1 != fcntl(3, F_GETFD))   // this process was forked from another APL
      {
        file_entry f3(0, 3);
        f3.path << "pipe-to_client";
        open_files.push_back(f3);
      }
#endif // ! MINGW_SRC
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AB(cValue_R A, cValue_R B) const
{
   CHECK_SECURITY(disable_Quad_FIO);

   if (A.get_rank() > 1)   RANK_ERROR;
   if (B.get_rank() > 1)   RANK_ERROR;

   if (B.is_str0())    return list_functions(CERR);
   if (B.is_zilde())   return list_mappings(CERR);

const APL_Integer function_number = B.get_near_int(0);
Value_P B_vp = CLONE(&B, LOC);
   switch(function_number)
      {
        case -3: // read probe A and clear it
             return eval_AB___3(CLONE(&A, LOC));

        case -19: // arm/disarm the --TM 3 test trigger
             return eval_AB___19(CLONE(&A, LOC));

        case -21: // provoke a crash signal (only if the trigger is armed)
             return eval_AB___21(CLONE(&A, LOC));

        default: break;
      }

   return list_functions(COUT);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   CHECK_SECURITY(disable_Quad_FIO);

   if (A.get_rank() > 1)   RANK_ERROR;
   if (B.get_rank() > 1)   RANK_ERROR;

Value_P A_vp = CLONE(&A, LOC);
Value_P B_vp = CLONE(&B, LOC);
const sAxis function_number = value_to_subfun(X);
const Handle_errno handle_errno(true);
   switch(function_number)
      {
         case 0:   // list functions
              return list_functions(COUT);

         case 3:   // fopen(Bs, As) filename Bs mode As
              return eval_AXB__3(A_vp, B_vp);

         case 6:   // fread(Zi, 1, Ai, Bh) 1 byte per Zi
              return eval_AXB__6(A_vp, B_vp);

         case 7:   // fwrite(Ai, 1, ⍴Ai, Bh) 1 byte per Zi
              return eval_AXB__7(A_vp, B_vp);

         case 8:   // fgets(Zi, Ai, Bh) 1 byte per Zi
              return eval_AXB__8(A_vp, B_vp);

         case 13:   // fseek(Bh, Ai, SEEK_SET)
              return eval_AXB__13(A_vp, B_vp);

         case 14:   // fseek(Bh, Ai, SEEK_CUR)
              return eval_AXB__14(A_vp, B_vp);

         case 15:   // fseek(Bh, Ai, SEEK_END)
              return eval_AXB__15(A_vp, B_vp);

         case 20:   // mkdir(Bc, Ai)
              return eval_AXB__20(A_vp, B_vp);

         case 22:   // fprintf(Bh, A) ←→
                    // fprintf(FILE *stream, const char *format, ...)
              return eval_AXB__22(A_vp, B_vp);

         case 23:   // fwrite(Ac, 1, ⍴Ac, Bh) Unicode Ac Output UTF-8
              return eval_AXB__23(A_vp, B_vp);

         case 24:   // popen(Bs, As) command Bs mode As
              return eval_AXB__24(A_vp, B_vp);

         case 27:   // rename(As, Bs)
              return eval_AXB__27(A_vp, B_vp);

         case 31:   // access
              return eval_AXB__31(A_vp, B_vp);

         case 33:   // bind(Bh, Aa)
              return eval_AXB__33(A_vp, B_vp);

         case 34:   // listen(Bh, Ai)
              return eval_AXB__34(A_vp, B_vp);

         case 36:   // connect(Bh, Aa)
              return eval_AXB__36(A_vp, B_vp);

         case 37:   // recv(Bh, Zi, Ai, 0) 1 byte per Zi
              return eval_AXB__37(A_vp, B_vp);

         case 38:   // send(Bh, Ai, ⍴Ai, 0) 1 byte per Zi
              return eval_AXB__38(A_vp, B_vp);

         case 39:   // send(Bh, Ac, ⍴Ac, 0) Unicode Ac Output UTF-8
              return eval_AXB__39(A_vp, B_vp);

         case 41:   // read(Bh, Zi, Ai) 1 byte per Zi
              return eval_AXB__41(A_vp, B_vp);

         case 42:   // write(Bh, Ai, ⍴Ai) 1 byte per Zi
              return eval_AXB__42(A_vp, B_vp);

         case 43:   // write(Bh, Ac, ⍴Ac) Unicode Ac Output UTF-8
              return eval_AXB__43(A_vp, B_vp);

         case 46:   // getsockopt(Bh, A_level, A_optname, Zi)
              return eval_AXB__46(A_vp, B_vp);

         case 47:   // setsockopt(Bh, A_level, A_optname, A_optval)
              return eval_AXB__47(A_vp, B_vp);

         case 48:   // fscanf(Bh, A_format)
              return eval_AXB__48(A_vp, B_vp);

         case 55:   // sscanf(Bh, A_format)
              return eval_AXB__55(A_vp, B_vp);

         case 56:   // write nested lines As to file Bs
              return eval_AXB__56(A_vp, B_vp);

         case 58:   // sprintf(Af, B...)
              return eval_AXB__58(A_vp, B_vp);

         case 59:   // fcntl(Bh, Ai...)
              return eval_AXB__59(A_vp, B_vp);

         case 60:   // random value(s)
              return eval_AXB__60(A_vp, B_vp);

         case 202:   // set monadic parallel threshold
         case 203:   // set dyadic parallel threshold
              return eval_AXB__202(A_vp, B_vp, function_number);

        default: bad_subfun_number_ERROR(function_number);
      }
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B(cValue_R B) const
{
   CHECK_SECURITY(disable_Quad_FIO);

   if (B.get_rank() > 1)   RANK_ERROR;

   if (B.element_count() == 0)   // '' or ⍬
      {
        if (B.is_str0())    return list_functions(CERR);
        if (B.is_zilde())   return list_mappings(CERR);
        DOMAIN_ERROR;
      }

const APL_Integer function_number = B.get_near_int(0);
Value_P B_vp = CLONE(&B, LOC);
   switch(function_number)
      {
        // function_numbers < 0 refer to "hacker functions" that should not be
        // used by normal mortals.
        //
        case -20: // simulate a FIXME (only if the --TM 3 trigger is armed)
             return eval_B___20();

        case -18: // memory test
             return eval_B___18(B_vp);

        case -17: // emulate Assert1()
             Assert1(0 && "Simulated Assert1() (aka. ⎕FIO ¯17)");
             return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

        case -16: // emulate Assert()
             Assert(0 && "Simulated Assert() (aka. ⎕FIO ¯16)");
             return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

        case -15: // print performance IDs and names...
             return eval_B___15();

        case -14: // print stacks. SI_top is the ⎕FIO call, so dont show it.
             return eval_B___14();

        case -13: // total number of UCS strings
             return Token(TOK_APL_VALUE1,
                          IntScalar(UCS_string::get_total_count(), LOC));

        case -12: // sbrk()
NOT_MINGW(
             return Token(TOK_APL_VALUE1, IntScalar(top_of_memory(), LOC));
         )
        case -11: // fnew
             return Token(TOK_APL_VALUE1,
                          IntScalar(Value::fast_new_count, LOC));
        case -10: // slow new
             return Token(TOK_APL_VALUE1,
                          IntScalar(Value::slow_new_count, LOC));

        case -9: // screen height
             return eval_B___9();

        case -8: // screen width
             return eval_B___8();

        case -7: // throw a segfault
             return eval_B___7();

        case -6: // throw a segfault
             return eval_B___6();

        case -5: // return ⎕AV of IBM APL2
             return eval_B___5();

        case -4: // clear all probes (ignores B, returns 0)
             Probe::init_all();
             return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

        case -3: // return the type of cycle_counter
#if HAVE_RDTSC
             return Token(TOK_APL_VALUE1, IntScalar(1, LOC));
#else
             return Token(TOK_APL_VALUE1, IntScalar(2, LOC));
#endif

        case -2: // return CPU frequency
             return eval_B___2();

        case -1: // return CPU cycle counter
             return Token(TOK_APL_VALUE1, IntScalar(cycle_counter(), LOC));

        case 0:   // list of open file descriptors
             return eval_B__0();

        case 30:   // getcwd()
             return eval_B__30(B_vp);

        default: break;
      }

   return list_functions(CERR);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB(cValue_R X, cValue_R B) const
{
   CHECK_SECURITY(disable_Quad_FIO);

   if (B.get_rank() > 1)   RANK_ERROR;

Value_P B_vp = CLONE(&B, LOC);
const APL_Integer function_number = value_to_subfun(X);
const Handle_errno handle_errno(function_number != 5);
   switch(function_number)
      {
         case 0:   // list functions
              return list_functions(CERR);

         case 1:   // return (last) errno
              goto out_errno;

         case 30:   // getcwd(); B is ignored, same as the ⎕FIO 30 form.
                    // fio_def(30, getcwd) in Quad_FIO.def advertises
                    // ⎕FIO[30]/⎕FIO['getcwd']/⎕FIO.getcwd as equivalent
                    // to ⎕FIO 30 (via ⎕FIO ⍬'s listing and the name
                    // lookup table), but only eval_B() had a case for
                    // it -- the three axis/name spellings all fell into
                    // default: and reported themselves as invalid
                    // (Blake McBride, Bugs25 #9).
              return eval_B__30(B_vp);

         case 2:   // return strerror(B)
              return eval_XB__2(B_vp);

         case 3:   // fopen(Bs, "r") filename Bs
              return eval_XB__3(B_vp);

         case 4:   // fclose(Bh)
              return eval_XB__4(B_vp);

         case 5:   // errno of Bh
              {
                errno = 0;
                file_entry & fe = get_file_entry(B);
                return Token(TOK_APL_VALUE1, IntScalar(fe.fe_errno, LOC));
              }

         case 6:   // fread(Zi, 1, SMALL_BUF, Bh) 1 byte per Zi
              return eval_XB__6(B_vp);

         case 8:   // fgets(Zi, SMALL_BUF, Bh) 1 byte per Zi
              return eval_XB__8(B_vp);

         case 9:   // fgetc(Bh)
              {
                FILE * file = get_FILE(B);
                return Token(TOK_APL_VALUE1, IntScalar(fgetc(file), LOC));
              }

         case 10:   // feof(Bh)
              {
                FILE * file = get_FILE(B);
                return Token(TOK_APL_VALUE1, IntScalar(feof(file), LOC));
              }

         case 11:   // ferror(Bh)
              {
                FILE * file = get_FILE(B);
                return Token(TOK_APL_VALUE1, IntScalar(ferror(file),LOC));
              }

         case 12:   // ftell(Bh)
              {
                FILE * file = get_FILE(B);
                return Token(TOK_APL_VALUE1, IntScalar(ftell(file),LOC));
              }

         case 16:   // fflush(Bh)
              return eval_XB__16(B_vp);

         case 17:   // fsync(Bh)
              return eval_XB__17(B_vp);

         case 18:   // fstat(Bh)
              return eval_XB__18(B_vp);

         case 19:   // unlink(Bc)
              return eval_XB__19(B_vp);

         case 20:   // mkdir(Bc)
              return eval_XB__20(B_vp);

         case 21:   // rmdir(Bc)
              return eval_XB__21(B_vp);

         case 24:   // popen(Bs, "r") command Bs
              return eval_XB__24(B_vp);

         case 25:   // pclose(Bh)
              return eval_XB__25(B_vp);

         case 26:   // read entire file
              return eval_XB__26(B_vp);

         case 28:   // read directory Bs
         case 29:   // read file names in directory Bs
              return eval_XB__28(B_vp, function_number);

         case 32:   // socket(Bi=AF_INET, SOCK_STREAM, 0)
              return eval_XB__32(B_vp);

         case 34:   // listen(Bh, 10)
              return eval_XB__34(B_vp);

         case 35:   // accept(Bh)
              return eval_XB__35(B_vp);

         case 37:   // recv(Bh, Zi, SMALL_BUF, 0) 1 byte per Zi
              return eval_XB__37(B_vp);

         case 40:   // select(Br, Bw, Be, Bt)
              return eval_XB__40(B_vp);

         case 41:   // read(Bh, Zi, SMALL_BUF) 1 byte per Zi
              return eval_XB__41(B_vp);

         case 44:   // getsockname(Bh, Zi)
              return eval_XB__44(B_vp);

         case 45:   // getpeername(Bh, Zi)
              return eval_XB__45(B_vp);

         case 49:   // read entire file as nested lines
              return eval_XB__49(B_vp);

         case 50:   // gettimeofday
              return eval_XB__50(B_vp);

         case 51:   // mktime
              return eval_XB__51(B_vp);

         case 52:   // localtime
         case 53:   // gmtime
              return eval_XB__52(B_vp, function_number);

         case 54:    // chdir
              return eval_XB__54(B_vp);

         case 57:   // fork() + execve() in the child
              return eval_XB__57(B_vp);

         case 60:   // random value
              return eval_XB__60(B_vp);

         case 62:   // socket constants
              return eval_XB__62(B_vp);

         case 200:   // clear statistics Bi
         case 201:   // get statistics Bi
              return eval_XB__200(B_vp, function_number);

         case 202:   // get monadic parallel threshold
         case 203:   // get dyadic  parallel threshold
              return eval_XB__202(B_vp, function_number);

         case 61:   // secs_epoch(Bh)
              return Token(TOK_APL_VALUE1, IntScalar(secs_epoch(B), LOC));

        default: bad_subfun_number_ERROR(function_number);
      }

out_errno:
   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_FIO::clear()
{
   // close open files, but leave stdin, stdout, and stderr open
   // we move backwards from the end until only stdin, stdout, and stderr
   // are left in open_files.
   //
   while(open_files.size() > 3)
      {
         file_entry & fe = open_files.back();
         // a popen() handle must be closed with sys_pclose(), which keeps
         // its SIGCHLD nesting counter balanced (Blake McBride, Bugs32 #11)
         if (fe.fe_popen)       sys_pclose(fe.fe_FILE);
         else if (fe.fe_FILE)   fclose(fe.fe_FILE);   // also closes fe.fe_fd
         else                   close(fe.fe_fd);
         // fe.path is empty for a handle opened via popen()/execve()
         // (⎕FIO[24]/[57], which don't record a path the way a plain
         // file open does) -- printing the handle number always keeps
         // this message useful, and skipping the path clause entirely
         // when it's empty (rather than printing it unconditionally)
         // avoids the double-space "File  (handle 4) ..." artifact
         // that produced. See Bugs27 #59(d).
         //
         CERR << "WARNING: File ";
         if (fe.path.size())   CERR << fe.path << " ";
         CERR << "(handle " << fe.fe_fd << ") still open - closing it"
              << endl;
        // fe.fe_fd was already closed above (either via fclose() or the
        // direct close()); closing it again here is a double-close --
        // normally a harmless EBADF, but if another thread reopened
        // that same fd number in between, this closes an unrelated
        // descriptor instead.
        open_files.pop_back();
      }
}
//────────────────────────────────────────────────────────────────────────────
int
Quad_FIO::close_handle(APL_Integer fd)
{
   loop(h, open_files.size())
      {
        file_entry & fe = open_files[h];
        if (fe.fe_fd != fd)   continue;

        // never close stdin, stdout, or stderr
        if (fe.fe_fd <= STDERR_FILENO)
           {
             MORE_ERROR() << "Attempt to close fd " << fe.fe_fd
                          << " (stdin, stdout, or stderr)";
             DOMAIN_ERROR;
           }

        // a popen() handle must be closed with sys_pclose(), which keeps
        // its SIGCHLD nesting counter balanced: fclose() left it
        // incremented for good, so that every later ⎕FIO[57] child became
        // a zombie (Blake McBride, Bugs32 #11).
        //
        if (fe.fe_popen)       sys_pclose(fe.fe_FILE);
        else if (fe.fe_FILE)   fclose(fe.fe_FILE);   // also closes fe.fe_fd
        else                   close(fe.fe_fd);

        open_files.erase(open_files.begin() + h);
        return 0;   // OK
      }

   MORE_ERROR() << "Invalid ⎕FIO handle " << fd;
   return -EBADF;
}
//────────────────────────────────────────────────────────────────────────────
int
Quad_FIO::do_FIO_57(const UCS_string & B, char * const * envp)
{
NOT_MINGW(
int spair[2];
   if (socketpair(AF_UNIX, SOCK_STREAM, 0, spair))
      {
        MORE_ERROR() << "⎕FIO[49]: socketpair() failed: " << strerror(errno);
        DOMAIN_ERROR;
      }

const pid_t child = fork();
   if (child == -1)
      {
        MORE_ERROR() << "⎕FIO[49]: fork() failed: " << strerror(errno);
        DOMAIN_ERROR;
      }

   if (child)   // parent process: return handle
      {
        sys_auto_reap(child);   // nobody waits for the child (see Sys.hh)
        ::close(spair[1]);   // close child's end so read(spair[0]) gets EOF when child exits
        file_entry fe(0, spair[0]);
        fe.fe_may_read = true;
        fe.fe_may_write = true;
        open_files.push_back(fe);
        return fe.fe_fd;
      }

   // code executed in the forked child.,,
   // Close some fds and then execve(Bs)
   // No need to free any strings allocated here
   //
const int sock = spair[1];
   ::close(STDIN_FILENO);
   for (int j = STDERR_FILENO+2; j < 100; ++j)
       {
         if (j != sock)   ::close(j);
       }

   // make the communication socket file descriptor 3
   //  (== STDERR + 1) in the client
   //
   dup2(sock, 3);
   ::close(sock);   // close the extra reference so fd 3 is the sole owner

const UCS_string path_ucs(B);
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
char * filename = strdup(path.c_str());
if (!filename)   DOMAIN_ERROR;
int argc = 1;
   for (const char * f = filename; *f; ++f)
       if (f[0] == ' ' && f[1] != ' ')   ++ argc;

char ** argv = new char *[argc + 2];
int ai = 0;
char * from = filename;
   for (char * f = filename; *f; ++f)
       {
         if (*f == ' ')   // end of argument
            {
              argv[ai++] = from;
              *f = 0;
              while (f[1] == ' ')   ++f;
              from = f + 1;
            }
       }

   if (*from)  argv[ai++] = from;
   argv[ai] = 0;

   execve(filename, argv, envp);   // no return on success

   // execve() failed
   //
   free(filename);
   ::close(3);

   usleep(100000);
   CERR << "*** execve() failed in 57 ⎕FIO: " << strerror(errno) << endl;
         ) // NOT_MINGW
   exit(-1);
}
//────────────────────────────────────────────────────────────────────────────
Unicode
Quad_FIO::fget_utf8(FILE * file, ShapeItem & fget_count)
{
const int b0 = fgetc(file);
   if (b0 == EOF)   return UNI_EOF;

   ++fget_count;
   if (!(b0 & 0x80))   return Unicode(b0);   // ASCII

int len,bx;
   if      ((b0 & 0xE0) == 0xC0)   { len = 2;   bx = b0 & 0x1F; }
   else if ((b0 & 0xF0) == 0xE0)   { len = 3;   bx = b0 & 0x0F; }
   else if ((b0 & 0xF8) == 0xF0)   { len = 4;   bx = b0 & 0x07; }
   else if ((b0 & 0xFC) == 0xF8)   { len = 5;   bx = b0 & 0x03; }
   else if ((b0 & 0xFE) == 0xFC)   { len = 6;   bx = b0 & 0x01; }
   else return UNI_EOF;

uint32_t uni = 0;
   loop(l, len - 1)
       {
         const int subc = fgetc(file);
         if (subc == EOF)   return UNI_EOF;
         bx  <<= 6;
         uni <<= 6;
         uni |= subc & 0x3F;
       }


   return Unicode(bx | uni);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_ALXB(cValue_R A, Token & LO, cValue_R X, cValue_R B) const
{
const APL_Integer function_number = X.get_near_int(0);
   switch(function_number)
      {
        case -1:   // benchmark monadic LO with argument B
             return eval_ALXB___1(CLONE(&A, LOC), LO, CLONE(&B, LOC));
      }

   MORE_ERROR() << "Bad function number (axis X) in operator A LO ⎕FIO[X] B";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_LXB(Token & LO, cValue_R X, cValue_R B) const
{
   CHECK_SECURITY(disable_Quad_FIO);

   /* a common "mistake" is to suppress the printout of ⎕FIO by e.g.

      ⊣⎕FIO[X] B

      which lands here instead of calling eval_XB(). We "fix" that mistake...
    */
   if (LO.get_tag() == TOK_F2_LEFT)
      {
        Token result = eval_XB(X, B);
        if (result.get_tag() == TOK_APL_VALUE1)
           result.ChangeTag(TOK_APL_VALUE2);
         return result;
      }

const APL_Integer function_number = X.get_near_int(0);
   switch (function_number)
      {
        case -1:   // benchmark monadic LO with argument B
             return eval_LXB___1(LO, CLONE(&B, LOC));

        case 49:
           {
             Token lines_B = eval_XB(X, B);
             return Bif_OPER1_EACH::do_eval_LB(LO, *lines_B.get_apl_val());
           }
      }

   MORE_ERROR() <<
"Bad function number (axis X) " << function_number << " in LO ⎕FIO[X] B.\n"
"Chances are that you meant to use ⎕FIO[X] B and not LO ⎕FIO[X] B. In that\n"
"case use (⎕FIO[X]) or H←⎕FIO[X]";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::list_functions(ostream & out) const
{
   out << "\n"
       << "⎕FIO is a function group. It is comprised of "
                        "the following (sub-)functions:\n"
"\n"
"   ┌─── Legend:────────────────────────────────────────────────────┐\n"
"   │    a - address family, IPv4 address, port (or errno)          │\n"
"   │    b - byte(s) (Integer(s) in the range [0-255])              │\n"
"   │    d - table of dirent structs                                │\n"
"   │    e - error code (integer as per errno.h)                    │\n"
"   │    f - format string (printf(), scanf())                      │\n"
"   │    h - file handle (small integer)                            │\n"
"   │    i - integer                                                │\n"
"   │    n - names (nested vector of strings)                       │\n"
"   │    p - path (filename string)                                 │\n"
"   │    s - string                                                 │\n"
"   │    u - time divisor: 1       - second                         │\n"
"   │                      1000    - milli second                   │\n"
"   │                      1000000 - micro second                   │\n"
"   │    y4 - seconds, wday, yday, dst (daylight saving time)       │\n"
"   │    y67- year, mon, day, hour, minute, second, [dst]           │\n"
"   │    y9 - year, mon, day, hour, minute, second, wday, yday, dst │\n"
"   │    A1, A2, ...  nested vector with elements A1, A2, ...       │\n"
"   └───────────────────────────────────────────────────────────────┘\n"
"\n"
"           ⎕FIO     ''   ⍝ print this text on stderr\n"
"           ⎕FIO      ⍬   ⍝ print function-number to -name mapping on stderr\n"
"           ⎕FIO      0   ⍝ return a list of open file descriptors\n"
"           ⎕FIO[ 0] ''   ⍝ print this text on stderr\n"
"        '' ⎕FIO[ 0] ''   ⍝ print this text on stdout\n"
"\n"
"   Zi ←    ⎕FIO[ 1] ''   ⍝ errno (of last call)\n"
"   Zs ←    ⎕FIO[ 2] Be   ⍝ strerror(Be)\n"
"   Zh ← As ⎕FIO[ 3] Bp   ⍝ fopen(Bs, As) filename Bp mode As\n"
"   Zh ←    ⎕FIO[ 3] Bp   ⍝ fopen(Bs, \"r\") filename Bp\n"
"\n"
"File I/O functions:\n"
"\n"
"   Ze ←    ⎕FIO[ 4] Bh   ⍝ fclose(Bh)\n"
"   Ze ←    ⎕FIO[ 5] Bh   ⍝ errno (of the last call using Bh)\n"
"   Zb ←    ⎕FIO[ 6] Bh   ⍝ fread(Zi, 1, " << SMALL_BUF
                                          << ", Bh) 1 byte per Zb\n"
"   Zb ← Ai ⎕FIO[ 6] Bh   ⍝ fread(Zi, 1, Ai, Bh) 1 byte per Zb\n"
"   Zi ← Ab ⎕FIO[ 7] Bh   ⍝ fwrite(Ab, 1, ⍴Ab, Bh) 1 byte per Ab\n"
"   Zb ←    ⎕FIO[ 8] Bh   ⍝ fgets(Zb, " << SMALL_BUF << ", Bh) 1 byte per Zb\n"
"   Zb ← Ai ⎕FIO[ 8] Bh   ⍝ fgets(Zb, Ai, Bh) 1 byte per Zb\n"
"   Zb ←    ⎕FIO[ 9] Bh   ⍝ fgetc(Zb, Bh) 1 byte\n"
"   Zi ←    ⎕FIO[10] Bh   ⍝ feof(Bh)\n"
"   Ze ←    ⎕FIO[11] Bh   ⍝ ferror(Bh)\n"
"   Zi ←    ⎕FIO[12] Bh   ⍝ ftell(Bh)\n"
"   Zi ← Ai ⎕FIO[13] Bh   ⍝ fseek(Bh, Ai, SEEK_SET)\n"
"   Zi ← Ai ⎕FIO[14] Bh   ⍝ fseek(Bh, Ai, SEEK_CUR)\n"
"   Zi ← Ai ⎕FIO[15] Bh   ⍝ fseek(Bh, Ai, SEEK_END)\n"
"   Zi ←    ⎕FIO[16] Bh   ⍝ fflush(Bh)\n"
"   Zi ←    ⎕FIO[17] Bh   ⍝ fsync(Bh)\n"
"   Zi ←    ⎕FIO[18] Bh   ⍝ fstat(Bh)\n"
"   Zi ←    ⎕FIO[19] Bp   ⍝ unlink(Bp)\n"
"   Zi ←    ⎕FIO[20] Bp   ⍝ mkdir(Bp, 0777)\n"
"   Zi ← Ai ⎕FIO[20] Bp   ⍝ mkdir(Bp, Ai)\n"
"   Zi ←    ⎕FIO[21] Bp   ⍝ rmdir(Bp)\n"
"   Zi ← A  ⎕FIO[22] 1    ⍝ printf(         A1, A2...) format A1\n"
"   Zi ← A  ⎕FIO[22] 2    ⍝ fprintf(stderr, A1, A2...) format A1\n"
"   Zi ← A  ⎕FIO[22] Bh   ⍝ fprintf(Bh,     A1, A2...) format A1\n"
"   Zi ← Ac ⎕FIO[23] Bh   ⍝ fwrite(Ac, 1, ⍴Ac, Bh) 1 Unicode per Ac, Output UTF8\n"
"   Zh ← As ⎕FIO[24] Bs   ⍝ popen(Bs, As) command Bs mode As\n"
"   Zh ←    ⎕FIO[24] Bs   ⍝ popen(Bs, \"r\") command Bs\n"
"   Ze ←    ⎕FIO[25] Bh   ⍝ pclose(Bh)\n"
"   Zb ←    ⎕FIO[26] Bp   ⍝ return entire file Bp as byte vector\n"
"   Ze ← Ap ⎕FIO[27] Bp   ⍝ rename file Ap to Bp\n"
"   Zd ←    ⎕FIO[28] Bp   ⍝ return content of directory Bp\n"
"   Zn ←    ⎕FIO[29] Bp   ⍝ return file names in directory Bp\n"
"   Zs ←    ⎕FIO 30       ⍝ getcwd()\n"
"   Ze ← As ⎕FIO[31] Bp   ⍝ access(As, Bp) As ϵ 'RWXF'\n"
"   Zh ←    ⎕FIO[32] Bi   ⍝ socket(Bi=AF_INET, SOCK_STREAM, 0)\n"
"   Ze ← Aa ⎕FIO[33] Bh   ⍝ bind(Bh, Aa)\n"
"   Ze ←    ⎕FIO[34] Bh   ⍝ listen(Bh, 10)\n"
"   Ze ← Ai ⎕FIO[34] Bh   ⍝ listen(Bh, Ai)\n"
"   Za ←    ⎕FIO[35] Bh   ⍝ accept(Bh)\n"
"   Ze ← Aa ⎕FIO[36] Bh   ⍝ connect(Bh, Aa)\n"
"   Zb ←    ⎕FIO[37] Bh   ⍝ recv(Bh, Zb, " << SMALL_BUF << ", 0) 1 byte per Zb\n"
"   Zb ← Ai ⎕FIO[37] Bh   ⍝ recv(Bb, Zi, Ai, 0) 1 byte per Zb\n"
"   Zi ← Ab ⎕FIO[38] Bh   ⍝ send(Bh, Ab, ⍴Ab, 0) 1 byte per Ab\n"
"   Zi ← Ac ⎕FIO[39] Bh   ⍝ send(Bh, Ac, ⍴Ac, 0) 1 Unicode per Ac, Output UTF8\n"
"   Zi ←    ⎕FIO[40] B    ⍝ select(B_read, B_write, B_exception, B_timeout)\n"
"   Zi ←    ⎕FIO[41] Bh   ⍝ read(Bh, Zi, " << SMALL_BUF << ") 1 byte per Zi\n"
"   Zb ← Ai ⎕FIO[41] Bh   ⍝ read(Bh, Zb, Ai) 1 byte per Zb\n"
"   Zi ← Ab ⎕FIO[42] Bh   ⍝ write(Bh, Ab, ⍴Ab) 1 byte per Ab\n"
"   Zi ← Ac ⎕FIO[43] Bh   ⍝ write(Bh, Ac, ⍴Ac) 1 Unicode per Ac, Output UTF8\n"
"   Za ←    ⎕FIO[44] Bh   ⍝ getsockname(Bh)\n"
"   Za ←    ⎕FIO[45] Bh   ⍝ getpeername(Bh)\n"
"   Zi ← Ai ⎕FIO[46] Bh   ⍝ getsockopt(Bh, A_level, A_optname, Zi)\n"
"   Ze ← Ai ⎕FIO[47] Bh   ⍝ setsockopt(Bh, A_level, A_optname, A_optval)\n"
"   Ze ← As ⎕FIO[48] Bh   ⍝ fscanf(Bh, As)\n"
"   Zs ←    ⎕FIO[49] Bp   ⍝ return entire file Bp as nested lines\n"
"   Zs ← LO ⎕FIO[49] Bp   ⍝ ⎕FIO[49] Bp and pipe each line through LO.\n"
"   Zi ←    ⎕FIO[50] Bu   ⍝ gettimeofday()\n"
"   Zy4←    ⎕FIO[51] By67 ⍝ mktime(By67)  Note: Jan 2, 2017 is: 2017 1 2 ...\n"
"   Zy9←    ⎕FIO[52] Bi   ⍝ localtime(Bi) Note: Jan 2, 2017 is: 2017 1 2 ...\n"
"   Zy9←    ⎕FIO[53] Bi   ⍝ gmtime(Bi)    Note: Jan 2, 2017 is: 2017 1 2 ...\n"
"   Zi ←    ⎕FIO[54] Bs   ⍝ chdir(Bs)\n"
"   Ze ← Af ⎕FIO[55] Bs   ⍝ sscanf(Bs, Af) Af is the format string\n"
"   Zs ← As ⎕FIO[56] Bp   ⍝ write nested lines As to file named Bp\n"
"   Zh ←    ⎕FIO[57] Bs   ⍝ fork() and execve(Bs, { Bs, 0}, {0})\n"
"   Zs ← Af ⎕FIO[58] B    ⍝ snprintf(Af, B...) As is the format string\n"
"   Zi ← Ai ⎕FIO[59] Bh   ⍝ fcntl(Bh, Ai...) file control\n"
"   Zi ←    ⎕FIO[60] Bi   ⍝ return a Bi-byte random integer (for setting ⎕RL)\n"
"   Zi ← 0  ⎕FIO[60] Bi   ⍝ same as monadic ⎕FIO[60] Bi (random scalar, Bi≤8)\n"
"   Zb ← 1  ⎕FIO[60] Bi   ⍝ return a vector of random bytes, (Bi ≥ ⍴Zi)\n"
"   Zi ←    ⎕FIO[61] Bv   ⍝ seconds since 1/1/1970; Bv←YYYY [MM DD [HH MM SS]]\n"
"   Zi ←    ⎕FIO[62] Bs   ⍝ value of the socket constant named Bs, e.g. 'AF_INET'\n"
"   Zi ←    ⎕FIO[62] Bn   ⍝ values of the socket constants named Bn\n"
"   Zn ←    ⎕FIO[62] ''   ⍝ all socket constants (N×2 matrix: name value)\n"
"\n"
"Benchmarking functions:\n"
"\n"
"           ⎕FIO[200] Bi   ⍝ clear statistics with ID Bi\n"
"   Zn ←    ⎕FIO[201] Bi   ⍝ get statistics with ID Bi\n"
"           ⎕FIO[202] Bs   ⍝ get monadic parallel threshold for primitive Bs\n"
"        Ai ⎕FIO[202] Bs   ⍝ set monadic parallel threshold for primitive Bs\n"
"           ⎕FIO[203] Bs   ⍝ get dyadic parallel threshold for primitive Bs\n"
"        Ai ⎕FIO[203] Bs   ⍝ set dyadic parallel threshold for primitive Bs\n"
"\n"
"The functions of ⎕FIO can be called with one of several syntax alternatives.\n"
"The syntax alternatives for ⎕FIO can be displayed with:\n\n";

   COUT <<"      " << group_name << " ⍬   ⍝ display the "
        << " syntax alternatives for " << group_name << "\n\n";

   return Token();
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_FIO::print_map_syntax(ostream & out, const function_info & info) const
{
const char * name = info.function_name;
const UCS_string blanks(max_function_name_length - strlen(name), UNI_SPACE);

   out << "    ⎕FIO[" << info.axis << "]  ←→  ⎕FIO['" << name << "']"
       << blanks << "←→  ⎕FIO." << name << endl;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::do_fprintf(FILE * outf, Value_P A)
{
   CHECK_SECURITY(disable_Quad_FIO__write);

   // A is expected to be a nested APL value. A[1] is the format string,
   // A[2...] are the values for each % field in A[1]. The result returned
   // is the number of characters (not bytes!) written to the file.
   //
UCS_string UZ;
const Value & A1 = *A->get_pointer_value(0);
UCS_string A_format(A1);
   do_snprintf(UZ, A_format, *A, 1, "⎕FIO.fprintf B");
UTF8_string utf(UZ);
   errno = 0;
   // a handle opened for reading only (e.g. ⎕FIO[22] to handle 0, i.e.
   // stdin) makes fwrite() fail here; that is a normal, expectable I/O
   // outcome, not an internal error -- report it the same way the other
   // ⎕FIO write functions (e.g. eval_AXB__7(), ⎕FIO[7]) do: return the
   // actual (short) result and leave errno for ⎕FIO[1] to inspect,
   // instead of throwing SYSTEM ERROR.
   //
   if (fwrite(utf.c_str(), 1, utf.size(), outf) != utf.size())
      return Token(TOK_APL_VALUE1, IntScalar(0, LOC));
   return Token(TOK_APL_VALUE1, IntScalar(UZ.size(), LOC));
}
//────────────────────────────────────────────────────────────────────────────
/// a Unicode source from either a file or a UCS_string
class File_or_String
{
public:
   /// constructor: from file
   File_or_String(FILE * f)
   : file(f),
     string(0),
     lookahead(Invalid_Unicode),
     unicodes_read(0)
   { offset = ftell(f);  }

   /// constructor: from string
   File_or_String(const UCS_string * u)
   : file(0),
     string(u),
     offset(0),
     lookahead(Invalid_Unicode),
     unicodes_read(0)
   {}

   /// return the number of characters
   ShapeItem get_count() const
      {
        if (file)    return ftell(file) - offset;
        return unicodes_read;
      }

   /// get the next character
   Unicode get_next()
      {
        if (lookahead != Invalid_Unicode)   // there is an ungotten char
           {
             const Unicode ret = lookahead;
             lookahead = Invalid_Unicode;
             // counted already, so don't count it twice
             return ret;
           }

         if (file)   // data comes from a UTF-8 encoded file
            {
              ShapeItem utf8_len = 0;
              const Unicode ret = Quad_FIO::fget_utf8(file, utf8_len);
              if (ret != UNI_EOF)   ++unicodes_read;
              return ret;
            }
         else        // data comes from a UCS_string
            {
              if (offset >= string->ssize())   return UNI_EOF;
              ++unicodes_read;
              return (*string)[offset++];
            }
      }

   /// return the number of characters consumed thus far, i.e. without the
   /// lookahead character \b la (if any) of the caller (Bugs37 #30)
   ShapeItem get_consumed(Unicode la) const
      {
        ShapeItem count = get_count();
        if (la == Invalid_Unicode || la == UNI_EOF)   return count;
        if (file)   return count - UTF8_string(UCS_string(la)).size();
        return count - 1;
      }

   /// return the lookahead character \b la (if any) of the caller, as
   /// well as an ungotten character, to the file, so that the next
   /// operation on the file sees it (like C's fscanf() does). Bugs37 #30.
   void finish(Unicode la)
      {
        if (!file)   return;
        put_back(lookahead);
        lookahead = Invalid_Unicode;
        put_back(la);
      }

   /// put \b uni back to the file
   void put_back(Unicode uni)
      {
        if (uni == Invalid_Unicode || uni == UNI_EOF)   return;
        const long len = UTF8_string(UCS_string(uni)).size();
        if (len == 1)   ungetc(uni, file);
        else            fseek(file, -len, SEEK_CUR);
      }

   /// unget uni. At most one Unicode (= lookahead char) can be ungotten.
   void unget(Unicode uni)
      {
        Assert(lookahead == Invalid_Unicode);
        lookahead = uni;
      }

   /// scan a long long in a string or in a file. Return either 0 ion error,
   /// or 1 on success (for /// the number of successful conversions).
   /// @param conv_len field width from the format (e.g. 5 for "%5d"),
   ///        or 0 for none (Bugs28 #100(j): this used to be silently
   ///        ignored, so e.g. "%5d" ⎕FIO[55] '1234567' consumed and
   ///        returned the whole 1234567 instead of stopping at 12345).
   int scanf_long_long(const char conv, long long & value, int64_t conv_len)
      {
         if (file)
            {
              // if we have read the next character from a file, then we
              // need to push it back so that fscanf() will get it.
              //
              if (lookahead != Invalid_Unicode)
                 {
                   put_back(lookahead);   // also for a multi-byte char
                   lookahead = Invalid_Unicode;
                 }
              char fmt[32];
              if (conv_len > 0)
                 snprintf(fmt, sizeof(fmt), "%%%lldll%c",
                          (long long)conv_len, conv);
              else
                 snprintf(fmt, sizeof(fmt), "%%ll%c", conv);
              errno = 0;
              const int ret = fscanf(file, fmt, &value);
              check_range(ret == 1 && errno == ERANGE, 0);
              return ret;
            }

         // string input...
         //
         // the digits collected in cc have no length limit (other than
         // conv_len), like C. A fixed buffer silently cut long fields,
         // which then gave a different number (Bugs37 #53).
         //
         const bool hex = (conv == 'X') || (conv == 'x');
         std::string cc;
         if (lookahead == UNI_OVERBAR || lookahead == UNI_MINUS)
            {
              cc += '-';
              lookahead = Invalid_Unicode;
            }
         else if (lookahead == UNI_PLUS)   // C scanf: optional + (Bugs37 #31)
            {
              cc += '+';
              lookahead = Invalid_Unicode;
            }
         else if (hex ? Avec::is_hex_digit(lookahead)
                      : Avec::is_digit(lookahead))
            {
              // caution: the lookahead was accepted unconditionally here
              // (any character, not just a digit), so e.g. 'x' became
              // cc = "x" and strtoll("x", ...) reports "no digits
              // converted" only via errno, which strtoll() does NOT set
              // for that case (only for overflow) -- so a failed
              // conversion silently returned 0, a value never in the
              // input (Blake McBride, Bugs25 #4). Filter the lookahead
              // the same way the loop below already does (and, unlike
              // the loop below before this fix, accept hex letters for
              // %x/%X so e.g. 'ff' converts fully instead of stopping
              // after the first digit).
              cc += char(lookahead);
              lookahead = Invalid_Unicode;
            }
            // else: a non-numeric lookahead is left untouched here; the
            // loop below's get_next() will hand it back as its first
            // character and reject it the same way.

         while ((conv_len == 0 || int64_t(cc.size()) < conv_len) &&
                offset < string->ssize())
            {
              // a sign is only valid as the first character (Bugs37 #31)
              const Unicode uni = get_next();
              const bool first = cc.size() == 0;
              if (hex && Avec::is_hex_digit(uni))
                                              cc += char(uni);   // 0-9a-fA-F
              else if (Avec::is_digit(uni))  cc += char(uni);   // 0-9
              else if (first && uni == UNI_OVERBAR)   cc += '-';
              else if (first && uni == UNI_MINUS)     cc += '-';
              else if (first && uni == UNI_PLUS)      cc += '+';
              else { unget(uni);   break; }
            }

         // nothing was consumed: not a conversion, and errno alone
         // can't tell the difference from a genuine strtoll() failure
         // (Blake McBride, Bugs23 #9).
         if (cc.size() == 0)   return 0;
         const int base =  (conv == 'X') || (conv == 'x') ? 16 : 10;
         errno = 0;
         const char * cp = cc.c_str();
         char * endptr = const_cast<char *>(cp);
         value = strtoll(cp, &endptr, base);
         if (endptr == cp)   return 0;   // no digits converted (Bugs25 #4)
         check_range(errno == ERANGE, cc.c_str());
         return errno ? 0 : 1;
      }

   /// throw a DOMAIN ERROR if a scanned number does not fit into an APL
   /// number (C would return a clamped integer or an infinite value).
   /// Losing the number silently is not an option (Bugs37 #53).
   /// @param overflow true if the number did not fit
   /// @param text the scanned text (if known)
   static void check_range(bool overflow, const char * text)
      {
        if (!overflow)   return;
        UCS_string & more = MORE_ERROR();
        more << "⎕FIO scanf: ";
        if (!text)   more << "a number";
        else
           {
             more << "the number ";
             const UCS_string ucs{UTF8_string(text)};
             if (ucs.size() <= 40)   more << ucs;
             else   more << UCS_string(ucs, 0, 20) << "…"
                         << UCS_string(ucs, ucs.size() - 10, 10)
                         << " (" << ucs.size() << " characters)";
           }
        more << " in B is out of range";
        DOMAIN_ERROR;
      }

   /// scan a double in string or file. Return either 0 or 1 for
   /// the number of successful conversions.
   /// @param conv_len field width from the format, or 0 for none
   ///        (Bugs28 #100(j), same fix as scanf_long_long() above).
   int scanf_double(const char conv, APL_Float & value, int64_t conv_len)
      {
         if (file)
            {
              if (lookahead != Invalid_Unicode)
                 {
                   put_back(lookahead);   // also for a multi-byte char
                   lookahead = Invalid_Unicode;
                 }
              char fmt[32];
              if (conv_len > 0)
                 snprintf(fmt, sizeof(fmt), "%%%lldl%c",
                          (long long)conv_len, conv);
              else
                 snprintf(fmt, sizeof(fmt), "%%l%c", conv);
              const int ret = fscanf(file, fmt, &value);
              check_range(ret == 1 && !isfinite(value), 0);
              return ret;
            }

         // string input...
         //
         std::string cc;   // no length limit, see scanf_long_long()
         if (lookahead == UNI_OVERBAR)
            {
              cc += '-';
              lookahead = Invalid_Unicode;
            }
         else if (lookahead > 0 && lookahead < 128 &&
                  strchr(".0123456789eE-+", char(lookahead)))
            {
              // strchr() truncates its int argument to char, so without
              // the ASCII range check above, any code point whose low
              // byte happened to match one of these ASCII characters
              // (e.g. U+012E -> 0x2E '.', U+0130 -> 0x30 '0') was wrongly
              // accepted and then truncated into cc, feeding a mangled
              // character to strtod(). Also fixes a ⎕UCS 0 inside B
              // being accepted (strchr(s, 0) matches the NUL terminator).
              // (Blake McBride, Bugs23 #9.)
              cc += char(lookahead);
              lookahead = Invalid_Unicode;
            }
            // else: a non-numeric lookahead is left untouched here; the
            // loop below's get_next() will hand it back as its first
            // character and reject it the same way.

         while ((conv_len == 0 || int64_t(cc.size()) < conv_len) &&
                offset < string->ssize())
            {
              // a sign is only valid as the first character or after the
              // e or E of an exponent, e.g. 1e+5 (Bugs37 #31)
              //
              const Unicode uni = get_next();
              const bool sign_ok = cc.size() == 0 ||
                                   cc.back() == 'e' || cc.back() == 'E';
              if (uni == UNI_OVERBAR && sign_ok)   cc += '-';   // ¯
              else if ((uni == UNI_MINUS || uni == UNI_PLUS) && sign_ok)
                 cc += char(uni);
              else if (uni > 0 && uni < 128 &&
                       strchr(".0123456789eE", char(uni)))
                 cc += char(uni);
              else { unget(uni);   break; }
            }

         // nothing was consumed: not a conversion, and errno alone
         // can't tell the difference from a genuine strtod() failure
         // (Blake McBride, Bugs23 #9).
         if (cc.size() == 0)   return 0;
         errno = 0;
         const char * cp = cc.c_str();
         char * endptr = const_cast<char *>(cp);
         value = strtod(cp, &endptr);
         if (endptr == cp)   return 0;   // no digits converted (Bugs37 #52)

         // an overflow is an error, an underflow (ERANGE with a result
         // of 0 or a denormal) gives that result, like C
         //
         check_range(!isfinite(value), cc.c_str());
         return 1;
      }

protected:
   /// UTF8-encoded source file (or 0 for string input)
   FILE * file;

   /// UCS_string source (or 0 for file source)
   const UCS_string * string;

   /// the next character in a string input
   ShapeItem offset;

   /// ungotten char
   Unicode lookahead;

   /// number of chars consumed thus far
   ShapeItem unicodes_read;
};
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::do_scanf(File_or_String & input, const UCS_string & format,
                   int function_number)
{
   if (format.ssize() == 0)   LENGTH_ERROR;

   // we take the total number of % as an upper bound for the number of items.
   // the real count may be lower due to %% and assugnment suppression. We fix
   // that after label out: below
   //
ShapeItem count = 0;
   loop(f, format.ssize() - 1)
      {
        if (format[f] == UNI_PERCENT)   ++count;
      }

Value_P Z(count, LOC);

   // lookahead is the next (unconsumed) input character, or UNI_EOF at
   // the end of the input, or Invalid_Unicode if it was handed back to
   // input (input.unget()). The end of the input only ends the scan for
   // a directive that needs input (unlike %n). Bugs37 #29.
   //
Unicode lookahead = input.get_next();

   // white space as defined by C's isspace()
   //
struct _ { static bool is_space(Unicode uni)
   { return uni == UNI_SPACE || (uni >= UNI_HT && uni <= UNI_CR); } };

   loop(f, format.ssize())
      {
        const Unicode fmt_ch = format[f];
        if (_::is_space(fmt_ch))
           {
             // white space in the format matches 0 or more white space
             // characters in the input (like C)
             //
             while (_::is_space(lookahead))   lookahead = input.get_next();
             continue;
           }

        if (fmt_ch != UNI_PERCENT)
           {
             // normal character in format: match with input
             //
             match:
             if (fmt_ch != lookahead)   goto out;   // incl. UNI_EOF
             lookahead = input.get_next();
             continue;
           }

        ++f;   // skip (first) %
        if (f >= format.ssize())
           {
             MORE_ERROR() << "trailing '%' in scanf format string";
             DOMAIN_ERROR;
           }
        const Unicode fmt_ch1 = format[f];
        if (fmt_ch1 == UNI_PERCENT)   goto match;   // double % is %

        Unicode conv = Unicode_0;   // no conversion specifier
        int64_t conv_len = 0;
        bool suppress = false;
        for (;f < format.ssize(); ++f)
           {
             const Unicode cc = format[f];
             if (cc == UNI_ASTERISK ||      // *: assignment character
                 cc == UNI_STAR_OPERATOR)   // ⋆: assignment character
                {
                  suppress = true;
                  continue;
                }

             if (strchr("hjlLmqtz", cc))   // type modifier
                {
                  // we provide our own conversion modifiers and ignore
                  // the conversion modifiers given by the user (that only
                  // make sense in the C/C++ context).
                  continue;
                }

             if (strchr("0123456789", cc))   // field length
                {
                  // accumulate in int64_t and bound it, same pattern (and
                  // same class of bug, Bugs15 #8) as pad_and_append()'s
                  // printf-side field width just below: an unbounded
                  // plain int here overflows (UB) for a user-supplied
                  // field width with enough digits, wrapping to a
                  // negative conv_len that silently changes the %c/%s
                  // conversion instead of erroring (Bugs18 #8).
                  //
                  conv_len = 10*conv_len + (cc - '0');
                  if (conv_len > 1000000000)
                     {
                       MORE_ERROR() << "⎕FIO scanf: field width in format '"
                                    << format << "' is too large";
                       DOMAIN_ERROR;
                     }
                  continue;
                }

             if (strchr("cdDfFeginousxX[", cc))
                {
                  conv = cc;
                  break; // conversion
                }
           }

        if (conv == Unicode_0)   // no conversion specifier
           {
             MORE_ERROR() << "expecting conversion character "
                             "%, c, d, f, i, n, o, u, s, or x after %";
             DOMAIN_ERROR;
           }

        // like C, all conversions except %c, %[, and %n skip leading white
        // space in the input (Bugs37 #53)
        //
        if (conv != UNI_c && conv != UNI_L_BRACK && conv != UNI_n)
           while (_::is_space(lookahead))   lookahead = input.get_next();

        if (strchr("dDiouxX", conv))  // integer conversion
           {
             if (lookahead == UNI_EOF)   goto out;
             input.unget(lookahead);   // so that scanf_long_long() can read it
             lookahead = Invalid_Unicode;
             long long value = 0;
             const int count = input.scanf_long_long(conv, value, conv_len);
             // Bugs28 #49: a suppressed conversion (%*d) still
             // consumes and validates an item -- count==1 means the
             // scan itself succeeded, and only the ASSIGNMENT is
             // suppressed. Ending the whole scan (goto out) whenever
             // suppress was set, even on a successful count==1, threw
             // away every directive after a %*d instead of continuing
             // past it. Only an actual scan failure (count != 1)
             // should end the scan.
             //
             if (count != 1)   goto out;
             if (!suppress)   Z->next_ravel_Int(value);

             lookahead = input.get_next();
           }
        else if (strchr("fFeg", conv))  // float conversion
           {
             if (lookahead == UNI_EOF)   goto out;
             input.unget(lookahead);   // let scanf_double() read it
             lookahead = Invalid_Unicode;
             APL_Float value = 0;
             const int count = input.scanf_double(conv, value, conv_len);
             // Bugs28 #49: see the matching, more detailed comment on
             // the integer conversion branch above.
             //
             if (count != 1)   goto out;
             if (!suppress)   Z->next_ravel_Float(value);

             lookahead = input.get_next();
           }
        else if (conv == UNI_c)  // char(s)
           {
             if (lookahead == UNI_EOF)   goto out;   // no char (Bugs37 #29)
             if (conv_len == 0)   // default: single char
                {
                  if (!suppress)   Z->next_ravel_Char(lookahead);
                  lookahead = input.get_next();
                }
             else
                {
                  UCS_string ucs;
                  loop(c, conv_len)
                     {
                       ucs << lookahead;
                       lookahead = input.get_next();
                       if (lookahead == UNI_EOF)   break;
                     }

                  if (!suppress)
                     {
                       Value_P ZZ(ucs, LOC);
                       Z->next_ravel_Pointer(ZZ.get());
                     }
                }
           }
        else if (conv == UNI_s)  // string
           {
             if (lookahead == UNI_EOF)   goto out;   // no string (Bugs37 #29)
             UCS_string ucs;
             while (lookahead > UNI_SPACE)
                 {
                   ucs << lookahead;
                   lookahead = input.get_next();   // Bugs37 #28
                   if (conv_len && ucs.ssize() >= conv_len)   break;
                 }

             if (!suppress)
                {
                  Value_P ZZ(ucs, LOC);
                  Z->next_ravel_Pointer(ZZ.get());
                }
           }
        else if (conv == UNI_n)  // characters consumed thus far
           {
             if (!suppress)   Z->next_ravel_Int(input.get_consumed(lookahead));
           }
        else if (conv == UNI_L_BRACK)   // character range
           {
             ++f;   // skip [

             // 1. read the pattern range [...]
             //

             // if the first char is ^ or ∧ then the characters in the range
             // shall be excluded rather than included. The range itself is
             // not affected, though.
             //
             if (f == format.ssize())   LENGTH_ERROR;
             const bool excluding = format[f] == UNI_CIRCUMFLEX   // ^
                                 || format[f] == UNI_AND;
            if (excluding)   ++f;   // skip ^ or ∧

             UCS_string range;   // the character range to be in- or excluded
             range.reserve(format.ssize());

             // if the next char is ] then it shall belong to the range,
             // otherwise it terminates the range than ending it.
             if (f == format.ssize())   LENGTH_ERROR;
             if (format[f] == UNI_R_BRACK)   range << format[f++];

            // the characters of the range, terminated by ]
            //
            for (;;)
                {
                 if (f == format.ssize())   // end of format string
                    {
                      MORE_ERROR() << "No ] in character range A of A ⎕FIO["
                                   << function_number << "] B";
                      DOMAIN_ERROR;
                    }

                  Unicode funi = format[f];
                  if ( funi == UNI_MINUS            // maybe a character range
                    && f + 1 < format.ssize()
                    && format[f+1] != UNI_R_BRACK   // otherwise its not
                    && range.size())                // otherwise its not
                     {
                           // the minus belongs to a range A-B. With Unicode
                           // the range could become ridiculously large and
                           // we throw a length error if that happens.
                           //
                           const Unicode from = range.back();
                           const Unicode to   = format[++f];
                           if (from == to)   {}   // 1-character range
                           else if (from > to)
                              {
                                MORE_ERROR() << "Invalid character range '"
                                             << from << "'-'" << to << "'\n";
                                DOMAIN_ERROR;
                              }

                           if ((to - from) >= 96)   // more than visible ASCII
                              {
                                MORE_ERROR() << "character range '" << from
                                             << "'-'" << to << "' too large.\n";
                                LENGTH_ERROR;
                              }

                       // at this point the A-B construct is valid. Insert
                       // the characters into range... f is at 'to'; move
                       // past it and continue, so that the fall-through
                       // below (which would otherwise add funi, i.e. the
                       // '-' itself, to the range) is not reached.
                       //
                       for (int u = from + 1; u <= to;)
                           range << Unicode(u++);
                       ++f;
                       continue;
                     }
                  if (funi == UNI_R_BRACK)   break;   // end of range
                  ++f;
                  range << funi;
                }

             // 2. create the APL result
             //
             UCS_string ucs;
             ucs.reserve(format.ssize());
             for (;;)
                 {
                   if (lookahead == UNI_EOF)                 break;
                   if (conv_len && ucs.ssize() >= conv_len)   break;

                   if (range.contains(lookahead) == excluding)
                      {
                        // lookahead is the first character after the range
                        break;
                      }
                   ucs << lookahead;

                   lookahead = input.get_next();
                 }

             // ] shall only match a nonempty sequence of characters
             //
             if (ucs.size() == 0)   goto out;   // empty sequence

             if (!suppress)
                {
                  Value_P ZZ(ucs, LOC);
                  Z->next_ravel_Pointer(ZZ.get());
                }
           }
      }

out:
   input.finish(lookahead);   // return the lookahead to a file

   // shrink Z to the actual number of converted items
   //
const Shape sh_Z(Z->get_valid_item_count());
   while (Z->more())   Z->next_ravel_0();
   Z->check_value(LOC);
   Z->set_shape(sh_Z);

   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
/// pad \b content to the field width encoded in fmt[1 .. fm-1] (the
/// flags/width accumulated so far for a %s, %c, or %m conversion, which
/// -- unlike the numeric conversions -- never gets a trailing conversion
/// character appended) and append the result to \b UZ. A '-' flag left-
/// justifies (pad on the right); otherwise the field is right-justified
/// (pad on the left), matching normal printf() semantics.
static void
pad_and_append(UCS_string & UZ, const UCS_string & content,
               const char * fmt, unsigned int fm)
{
bool left = false;
   // fmt can carry up to ~36 user-supplied digits (Quad_FIO.cc's fmt[40]
   // accumulator), so the plain 'int width' this used to accumulate into
   // would overflow (UB) long before the loop ends, wrapping to a
   // negative value that silently suppressed padding instead of erroring
   // (Blake McBride, Bugs15 #8; the numeric %d/%f/etc. conversions were
   // hardened against the same class of problem in round 8, by letting
   // snprintf(0,0,...) size the field and reject an over-large one -- %s/
   // %c/%m have no such delegate since their content isn't a plain C
   // string, so the accumulation itself needs the same bound here).
   //
int64_t width = 0;
int64_t precision = -1;   // -1 means "no precision given"
bool in_precision = false;
   for (unsigned int f = 1; f < fm; ++f)
       {
         if (fmt[f] == '-')                         left = true;
         else if (fmt[f] == '.')
            {
              in_precision = true;
              precision = 0;
            }
         else if (fmt[f] >= '0' && fmt[f] <= '9')
            {
              int64_t & digits = in_precision ? precision : width;
              digits = digits*10 + (fmt[f] - '0');
              // keep well clear of int64_t overflow even for fmt's full
              // ~36-digit capacity (checked every digit, not just once at
              // the end, so the accumulation itself never overflows).
              //
              if (digits > 1000000000)
                 {
                   MORE_ERROR() << "⎕FIO printf: field width/precision in "
                                   "format '" << fmt << "' is too large";
                   DOMAIN_ERROR;
                 }
            }
       }

UCS_string truncated;
const UCS_string * cont = &content;
   if (precision >= 0 && precision < int64_t(content.size()))
      {
        truncated = UCS_string(content, 0, precision);
        cont = &truncated;
      }

const int64_t pad = width - int64_t(cont->size());
   if (pad > 0 && Value::check_WS_FULL("⎕FIO printf", pad, LOC))   WS_FULL;
   if (pad > 0 && !left)   loop(p, pad)   UZ << UNI_SPACE;
   UZ << *cont;
   if (pad > 0 && left)    loop(p, pad)   UZ << UNI_SPACE;
}
//────────────────────────────────────────────────────────────────────────────
/// format \b val with \b fmt (a complete, already flags/width/precision/
/// length-modifier-assembled printf() conversion spec of length \b fm,
/// as built up by do_snprintf() below), insert thousands' separators,
/// and pad the *grouped* result to the field width encoded in \b fmt --
/// appending to \b UZ. Width/zero-flag/conversion are re-derived from
/// \b fmt itself rather than threaded through separately.
template<typename T>
void
Quad_FIO::group_thousands_width(UCS_string & UZ, const char * fmt,
                                unsigned int fm, T val, bool flt, char conv)
{
   // Build fmt without the width digits (and without the zero-flag,
   // which is just another digit at this point -- see below) so that
   // snprintf() below produces the bare, natural-length digit string:
   // grouping THAT (rather than the already width/zero-padded string
   // the old code grouped) means the separators can't blow the field
   // past the requested width (Blake McBride, Bugs20 #6).
   //
   // Per the printf() grammar, digits before the first '.' are either
   // the zero-flag or width -- either way they must not survive into
   // fmt_nw. Digits after '.' are precision and must be kept. The
   // zero-flag itself is exactly the digit run's *first* character
   // being '0' (a real width can't start with '0'); remembered here so
   // the padding below can use '0' instead of blank (Bugs23 #7).
   //
char fmt_nw[40];
unsigned int fm_nw = 0;
bool left = false;
bool zero = false;
bool first_digit = true;
int64_t width = 0;
bool seen_dot = false;
   for (unsigned int f = 0; f < fm; ++f)
       {
         const char c = fmt[f];
         if (c == '-')   left = true;
         if (!seen_dot && c >= '0' && c <= '9')
            {
              if (first_digit && c == '0')   zero = true;
              first_digit = false;
              width = width*10 + (c - '0');   // 0-flag contributes 0*10+0
              continue;                       // drop from fmt_nw
            }
         if (c == '.')   seen_dot = true;
         fmt_nw[fm_nw++] = c;
       }
   fmt_nw[fm_nw] = 0;

const int need = snprintf(0, 0, fmt_nw, val);
   if (need < 0)
      {
        MORE_ERROR() << "⎕FIO printf: format '" << fmt_nw
                     << "' rejected by snprintf() (width/precision too"
                        " large)";
        DOMAIN_ERROR;
      }
   if (Value::check_WS_FULL("⎕FIO printf", need, LOC))   WS_FULL;
vector<char> buf(need + 1);
   snprintf(&buf[0], buf.size(), fmt_nw, val);

UCS_string grouped;
   Quad_FIO::group_thousands(grouped, &buf[0], flt, conv);

const int gsize = grouped.size();
const int64_t pad = width - int64_t(gsize);
   if (pad > 0 && Value::check_WS_FULL("⎕FIO printf", pad, LOC))   WS_FULL;

   if (pad > 0 && zero && !left)
      {
        // zero-pad *after* any sign or 0x/0X prefix, like C does, and
        // as plain '0' characters -- not grouped with the digits
        // (Blake McBride, Bugs23 #7).
        int insert_at = 0;
        if (insert_at < gsize &&
            (grouped[insert_at] == UNI_MINUS || grouped[insert_at] == UNI_PLUS))
           ++insert_at;
        if (insert_at + 1 < gsize && grouped[insert_at] == UNI_0 &&
            (grouped[insert_at + 1] == Unicode('x') ||
             grouped[insert_at + 1] == Unicode('X')))
           insert_at += 2;

        loop(p, insert_at)         UZ << grouped[p];
        loop(p, pad)                UZ << UNI_0;
        for (int p = insert_at; p < gsize; ++p)   UZ << grouped[p];
      }
   else
      {
        if (pad > 0 && !left)   loop(p, pad)   UZ << UNI_SPACE;
        UZ << grouped;
        if (pad > 0 && left)    loop(p, pad)   UZ << UNI_SPACE;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_FIO::do_snprintf(UCS_string & UZ, const UCS_string & A_format,
                    cValue_R B, int off_B, const char * funname)
{
   // A is the format string, B the nested APL values for each % field in A.
   // The result UZ is the formatted string. off_B is the starting point (1
   // for fprintf(A, ...) and 0 for snprintf(format, B ...).
   //
char numbuf[50];
const int arg_count_B = B.element_count() - off_B;
int conversion_count_A = 0;   // the number of conversions (in A_format)

#define COUNT_ARG if   (conversion_count_A++ >= arg_count_B)   goto missing_arg

   for (int fA = 0; fA < A_format.ssize(); /* no fA++ */ )
       {
         const Unicode uni = A_format[fA++];
         if (uni != UNI_PERCENT)   // not %
            {
              UZ << uni;
              continue;
            }

         // uni is % which is the start of a format field. Copy the format field
         // into to string fmt and printf() it with the next argument.
         // That is, for e.g. format = "...%42.42llf..." we want fmt to be
         // "%42.42llf"
         //
         char fmt[40];             // the naked format for one printf() item
         unsigned int fm = 0;      // an index into fmt;
         fmt[fm++] = '%';          // copy the % into fmt
         fmt[fm] = 0;              // ... and NUL-terminate: fmt is on the
                                   // stack and uninitialized, so without
                                   // this a dangling '%' at the very end
                                   // of A_format (caught below) would
                                   // print garbage left over from a
                                   // previous iteration's fmt instead of
                                   // just "%".
         bool thousands = false;   // print thousands separator (',')
         for (;;)
             {
               if (fA >= A_format.ssize())
                  {
                    // After having seen a %, the end of the format string was
                    // reached without seeing the conversion specifier.
                    // Print the string and return. This happends only if
                    // A_format is somewhat mal-formed (e.g. "ab %555"),
                    // but the C/C++ printf() functions accept it.
                    //
                    UTF8_string utf(fmt);
                    UCS_string ufmt(utf);
                    UZ << ufmt;
                    goto check_unused;   // not return (Bugs37 #83e)
                  }

               if (fm >= sizeof(fmt) - 3)
                  {
                    // After seeing a %, no conversion specifier was seen
                    // within (almost) 40 characters. Most likely the user
                    // has forgotten it. In theory the format string could
                    // be proper, but we assumt it is mal-formed.
                    // (- 3 above: every write below is fmt[fm++]=uni_1;
                    // fmt[fm]=0; i.e. 2 bytes, and the integer conversion
                    // case below may additionally insert a 2-byte "ll"
                    // length modifier, i.e. up to 4 bytes, so fm must stay
                    // 3 short of sizeof(fmt) to avoid an overflow.)
                    //
                    UTF8_string utf(fmt);
                    UCS_string ufmt(utf);
                    UZ << ufmt;
                    goto field_done;
                  }

               const Unicode uni_1 = A_format[fA++];
               switch(uni_1)
                  {
                     // flag chars and field width/precision
                     //
                     case '\'': thousands = true;
                                continue;

                     case '#':
                     case '0' ... '9':
                     case '-':
                     case ' ':
                     case '+':
                     case 'I':    // glibc
                     case '.':

                     // length modifiers
                     //
                     case 'h':
                     case 'l':
                     case 'L':
                     case 'q':
                     case 'j':
                     case 'z':
                     case 't': fmt[fm++] = uni_1;   fmt[fm] = 0;
                               continue;

                         // conversion specifiers
                         //
                     case 'd':   case 'i':   case 'o':
                     case 'u':   case 'x':   case 'X':   case 'p':
                          {
                            COUNT_ARG;
                            const ShapeItem idx_B = off_B++;
                            APL_Integer int_val;
                            if (B.is_integer_cell(idx_B))
                               {
                                 int_val = B.get_near_int(idx_B);
                               }
                            else
                               {
                                 // Bugs28 #100(j): get_real_value() on a
                                 // genuinely (not just tolerantly) complex
                                 // cell silently returns only its real
                                 // part (ComplexCell::get_real_value()'s
                                 // documented contract assumes the caller
                                 // already checked near-realness) -- so
                                 // e.g. '%d' ⎕FIO[58] 1J2 printed "1"
                                 // instead of rejecting the imaginary part.
                                 //
                                 if (B.is_complex_cell(idx_B) &&
                                     !B.is_near_real(idx_B))
                                    {
                                      MORE_ERROR() << "argument " << idx_B
                                         << " is complex and cannot be used"
                                            " with integer format '%"
                                         << uni_1 << "' in " << funname;
                                      DOMAIN_ERROR;
                                    }

                                 const double fv = B.get_real_value(idx_B);
                                 if (!(fv > -BIG_INT64_F && fv < BIG_INT64_F))
                                    {
                                      MORE_ERROR() << "value " << fv
                                         << " is out of range for integer"
                                            " format '%" << uni_1
                                         << "' in " << funname;
                                      DOMAIN_ERROR;
                                    }

                                 // Bugs37 #83(f): truncating 2.5 to 2 lost
                                 // information silently. Near-ints are
                                 // rounded (2.9999999999999 → 3), others
                                 // are a DOMAIN ERROR (use ⌊B to truncate).
                                 //
                                 if (!B.is_near_int(idx_B))
                                    {
                                      MORE_ERROR() << "B["
                                         << (idx_B + Workspace::get_IO())
                                         << "] (= " << fv
                                         << ") is not an integer, as"
                                            " required by format '%"
                                         << uni_1 << "' in " << funname;
                                      DOMAIN_ERROR;
                                    }
                                 int_val = B.get_near_int(idx_B);
                               }

                            // APL integers are 64 bits; drop any
                            // user-supplied length modifier (it would be
                            // the wrong size) and force a 64-bit one.
                            //
                            unsigned int fm2 = 1;   // keep the leading '%'
                            for (unsigned int f = 1; f < fm; ++f)
                                {
                                  switch (fmt[f])
                                     {
                                       case 'h': case 'l': case 'L':
                                       case 'q': case 'j': case 'z':
                                       case 't': continue;   // drop
                                       default:  fmt[fm2++] = fmt[f];
                                     }
                                }
                            fm = fm2;
                            if (uni_1 != 'p')   // %p takes no length mod.
                               { fmt[fm++] = 'l';   fmt[fm++] = 'l'; }
                            fmt[fm++] = uni_1;   fmt[fm] = 0;

                            // fmt's width/precision are user-controlled and
                            // can exceed numbuf's fixed size; size the
                            // buffer to what this conversion actually needs.
                            //
                            int need = snprintf(0, 0, fmt, int_val);
                            if (need < 0)
                               {
                                 // Bugs8 #5 (Blake McBride): glibc returns
                                 // -1/EOVERFLOW when the conversion would
                                 // exceed INT_MAX; clamping to 0 silently
                                 // formatted into a 1-byte buffer and
                                 // appended the resulting empty string.
                                 //
                                 MORE_ERROR() << "⎕FIO printf: format '"
                                    << fmt << "' rejected by snprintf() "
                                       "(width/precision too large)";
                                 DOMAIN_ERROR;
                               }

                            // Bugs8 #4 (Blake McBride): need is
                            // user-controlled via the format string's
                            // width/precision and was never bounded, so a
                            // single expression could allocate gigabytes
                            // and OOM-kill the interpreter instead of
                            // raising WS FULL.
                            //
                            if (Value::check_WS_FULL("⎕FIO printf", need, LOC))
                               WS_FULL;

                            vector<char> dynbuf(need + 1);
                            snprintf(&dynbuf[0], dynbuf.size(), fmt, int_val);
                            if (thousands)
                               group_thousands_width(UZ, fmt, fm, int_val,
                                                      false, char(uni_1));
                            else
                               UZ << &dynbuf[0];
                          }
                          goto field_done;

                     case 'e':   case 'E':   case 'f':   case 'F':
                     case 'g':   case 'G':   case 'a':   case 'A':
                          {
                            COUNT_ARG;
                            const ShapeItem idx_B = off_B++;

                            // Bugs28 #100(j): same fix as the integer
                            // conversion case above.
                            //
                            if (B.is_complex_cell(idx_B) &&
                                !B.is_near_real(idx_B))
                               {
                                 MORE_ERROR() << "argument " << idx_B
                                    << " is complex and cannot be used"
                                       " with format '%" << uni_1
                                    << "' in " << funname;
                                 DOMAIN_ERROR;
                               }

                            const APL_Float float_val =
                                  B.get_real_value(idx_B);

                            // APL floats are passed as a plain double;
                            // drop any user-supplied length modifier the
                            // same way the integer case above does. Left
                            // in place, e.g. '%Lf' tells snprintf() to
                            // read a long double off the vararg slot --
                            // undefined behaviour for a double argument
                            // (Blake McBride, Bugs25 #5, confirmed via
                            // ASan's printf interceptor). Unlike the
                            // integer case, no replacement modifier is
                            // added: no modifier is exactly right for a
                            // plain double.
                            //
                            unsigned int fm2 = 1;   // keep the leading '%'
                            for (unsigned int f = 1; f < fm; ++f)
                                {
                                  switch (fmt[f])
                                     {
                                       case 'h': case 'l': case 'L':
                                       case 'q': case 'j': case 'z':
                                       case 't': continue;   // drop
                                       default:  fmt[fm2++] = fmt[f];
                                     }
                                }
                            fm = fm2;
                            fmt[fm++] = uni_1;   fmt[fm] = 0;

                            int need = snprintf(0, 0, fmt, float_val);
                            if (need < 0)
                               {
                                 MORE_ERROR() << "⎕FIO printf: format '"
                                    << fmt << "' rejected by snprintf() "
                                       "(width/precision too large)";
                                 DOMAIN_ERROR;
                               }

                            if (Value::check_WS_FULL("⎕FIO printf", need, LOC))
                               WS_FULL;

                            vector<char> dynbuf(need + 1);
                            snprintf(&dynbuf[0], dynbuf.size(), fmt, float_val);
                            char * const numbuf = &dynbuf[0];
                            if (thousands)
                               {
                                 group_thousands_width(UZ, fmt, fm, float_val,
                                                        true, char(uni_1));
                               }
                            else if (char * const dot = strchr(numbuf, '.'))
                               {
                                 *dot = 0;
                                 UZ << numbuf;
                                 UZ << Workspace::get_FC(0) << (dot + 1);
                               }
                            else
                               {
                                 UZ << numbuf;
                               }
                          }
                          goto field_done;

                     case 's':   // string or char
                          {
                            COUNT_ARG;
                            UCS_string content;
                            if (B.is_character_cell(off_B))
                               {
                                 content << B.get_char_value(off_B++);
                               }
                            else
                               {
                                 Value_P str = B.get_pointer_value(off_B++);
                                 content = UCS_string(*str.get());
                               }
                            pad_and_append(UZ, content, fmt, fm);
                          }
                          goto field_done;

                     case 'c':   // single char
                          {
                            COUNT_ARG;
                            const UCS_string content(1, B.get_char_value(off_B++));
                            pad_and_append(UZ, content, fmt, fm);
                          }
                          goto field_done;

                     case 'm':
                          {
                            // %m (insert strerror(errno)) takes no argument
                            // of its own, so it must not be counted as a
                            // conversion that consumes one.
                            SPRINTF(numbuf, "%s", strerror(errno));
                            const UTF8_string utf(numbuf);
                            const UCS_string content(utf);
                            pad_and_append(UZ, content, fmt, fm);
                          }
                          goto field_done;

                     case '%':
                          if (fm == 1)   // %% is % (fm==1: only the
                                         // leading '%' copied so far)
                             {
                               UZ << UNI_PERCENT;
                               goto field_done;
                             }
                          /* no break */

                     default:
                          MORE_ERROR() << "invalid format character " << uni_1
                                       << " in " << funname;
                          DOMAIN_ERROR;   // bad format char
                  }
             }
         field_done: ;
       }

check_unused:
   if (B.element_count() > off_B)
      {
        const int unused = B.element_count() - off_B;
        MORE_ERROR() << unused << " unused argument(s) in argument B of: "
                     << funname << "\n" << "    " << conversion_count_A
                     << " argument(s) were used, but " << arg_count_B
                     << " were provided.\n"
                        "    The format string was: '" << A_format << "'";
        LENGTH_ERROR;
      }
      return;

missing_arg:
   MORE_ERROR() << "Too few arguments in argument B of: " << funname << "\n"
                   "    Only " << arg_count_B
                <<   " argument(s) were provided in B.\n"
                   "    The format string was: '" << A_format << "'";
    LENGTH_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AB___3(Value_P A)
{
APL_Integer probe = A->get_near_int(0);
   if (probe < 0)   probe = Probe::PROBE_COUNT + probe;
   if (probe >= Probe::PROBE_COUNT)
      return Token(TOK_APL_VALUE1, IntScalar(-1, LOC));

const int len = Probe::get_length(probe);
   if (len < 0)   return Token(TOK_APL_VALUE1, IntScalar(-1, LOC));

Value_P Z(len, LOC);
   loop(m, len)
       Z->next_ravel_Int(Probe::get_time(probe, m));

   Probe::init(probe);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AB___19(Value_P A)
{
const APL_Integer old_trigger = TM3_trigger;
   TM3_trigger = A->get_near_int(0);
   return Token(TOK_APL_VALUE1, IntScalar(old_trigger, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AB___21(Value_P A)
{
   // A is the signal name without SIG, e.g. 'BUS'
   if (!A->is_char_string())   DOMAIN_ERROR;

const UTF8_string name(A->get_UCS_ravel());
   if (strcmp(name.c_str(), "SEGV") && strcmp(name.c_str(), "BUS")  &&
       strcmp(name.c_str(), "FPE")  && strcmp(name.c_str(), "ILL")  &&
       strcmp(name.c_str(), "ABRT"))
      {
        MORE_ERROR() << "⎕FIO ¯21: A shall be one of 'SEGV', 'BUS', "
                        "'FPE', 'ILL', or 'ABRT'";
        DOMAIN_ERROR;
      }

   if (!TM3_trigger_armed())   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

   CERR << "NOTE: Provoking SIG" << name << "..." << endl;
   CrashDump::provoke(name.c_str());
   CERR << "NOTE: Provoking SIG" << name << " failed." << endl;
   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_FIO::eval_ALXB___1(Value_P A, Token & LO, Value_P B)
{
#if ! HAVE_RDTSC
   MORE_ERROR() << "Platform has no 'rtdsc' instruction.";
   DOMAIN_ERROR;
#endif

   // doit...
   //
cFunction_P fun = LO.get_function();
   Assert(fun);
   const uint64_t from = cycle_counter();
Token result = fun->eval_AB(*A, *B);
const uint64_t to = cycle_counter();
   if (result.get_tag() == TOK_SI_PUSHED)
      {
        Workspace::SI_top()->                     // pretend ⎕ES
                             set_safe_execution_depth();
        benchmark_cycles_from = from;
        return result;
      }

   // LO is a primitive
   //
   return Token(TOK_APL_VALUE1, IntScalar(to - from, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__13(Value_P A, Value_P B)
{
   // errno reset moved next to the actual syscall, and the return value
   // checked before consulting errno (Blake McBride, Bugs17 #7: same
   // idiom as Bugs15 #7's chdir() fix -- get_FILE()/get_near_int() sat
   // between the old errno=0 and the call and could disturb it).
FILE * file = get_FILE(*B);
const APL_Integer pos = A->get_near_int(0);
   errno = 0;
const int err = fseek(file, pos, SEEK_SET);
   return Token(TOK_APL_VALUE1, IntScalar(err ? -errno : 0, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__14(Value_P A, Value_P B)
{
FILE * file = get_FILE(*B);
const APL_Integer pos = A->get_near_int(0);
   errno = 0;
const int err = fseek(file, pos, SEEK_CUR);
   return Token(TOK_APL_VALUE1, IntScalar(err ? -errno : 0, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__15(Value_P A, Value_P B)
{
FILE * file = get_FILE(*B);
const APL_Integer pos = A->get_near_int(0);
   errno = 0;
const int err = fseek(file, pos, SEEK_END);
   return Token(TOK_APL_VALUE1, IntScalar(err ? -errno : 0, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__20(Value_P A, Value_P B)
{
const UCS_string path_ucs(*B.get());
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
#if MINGW_SRC
   errno = 0;
const int err = mkdir(path.c_str());
#else // ! MINGW_SRC
const int mask = A->get_near_int(0);
   errno = 0;
const int err = mkdir(path.c_str(), mask);
#endif // ! MINGW_SRC
   if (err && errno == EEXIST)   errno = 0;   // frequent non-error
   return Token(TOK_APL_VALUE1, IntScalar(err ? -errno : 0, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__202(Value_P A, Value_P B, APL_Integer function_number)
{
const APL_Integer threshold = A->get_near_int(0);
cFunction_P fun = 0;
   if (B->element_count() == 3)   // dyadic operator
      {
        const Unicode oper = B->get_char_value(1);
        if (oper != UNI_FULLSTOP)   DOMAIN_ERROR;
        fun = &Bif_OPER2_INNER::fun;
      }
   else
      {
        const Unicode prim = B->get_char_value(0);

        // tokenize_function(Unicode) is written for characters the
        // tokenizer has already classified as functions; its
        // fall-through is Assert(0 && "Missing Function"), reached for
        // exactly the invalid-character inputs the is_function() check
        // below was meant to catch (making that check unreachable dead
        // code). Pre-filter with the same test tokenize_function() uses
        // internally (Avec::uni_to_token()), which only needs the
        // token's tag -- not its (not yet initialised) function
        // pointer -- so is_function() is safe to call on it directly
        // (Blake McBride, Bugs25 #8).
        //
        Unicode prim_copy = prim;   // uni_to_token() may rewrite its arg
        if (!Avec::uni_to_token(prim_copy, LOC).is_function())
           DOMAIN_ERROR;
        const Token tok = Tokenizer::tokenize_function(prim);
        fun = tok.get_function();
      }
   if (fun == 0)   DOMAIN_ERROR;
APL_Integer old_threshold;
   if (function_number == 202)
      {
        old_threshold = fun->get_monadic_threshold();
        const_cast<Function *>(fun)->set_monadic_threshold(threshold);
      }
   else
      {
        old_threshold = fun->get_dyadic_threshold();
        const_cast<Function *>(fun)->set_dyadic_threshold(threshold);
      }
   return Token(TOK_APL_VALUE1, IntScalar(old_threshold, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__22(Value_P A, Value_P B)
{
   errno = 0;
FILE * file = get_FILE(*B);
   return do_fprintf(file, A);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__23(Value_P A, Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__write);
   errno = 0;
FILE * file = get_FILE(*B);
UCS_string text(*A.get());
UTF8_string utf(text);
const size_t len = fwrite(utf.c_str(), 1, utf.size(), file);
   return Token(TOK_APL_VALUE1, IntScalar(len, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__24(Value_P A, Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__exec);

   // CHECK_SECURITY() above is a no-op unless the interpreter was built
   // with SECURITY_LEVEL_WANTED=1 or 2 (Security.hh) -- i.e. it does
   // NOT depend on --safe at the DEFAULT build level. --safe (uprefs.
   // safe_mode) already blocks )HOST (Cmd_HOST.cc) from spawning a
   // subprocess; this ⎕FIO function can equally spawn/exec one
   // (popen()/fork()+execve()), so honour the SAME runtime flag here,
   // independent of the compile-time security level. See Bugs27 #54.
   //
   if (UserPreferences::uprefs.safe_mode)   not_allowed("--safe");
NOT_MINGW(
   {
     const UCS_string mode_ucs(*A.get());
     const UCS_string path_ucs(*B.get());
     const UTF8_string mode(mode_ucs);
     check_path(path_ucs);   // Bugs37 #83a
     const UTF8_string path(path_ucs);
     const char * m = mode.c_str();
     bool read = false;
     bool write = false;
     if      (!strncmp(m, "er", 2))     read = true;
     else if (!strncmp(m, "r" , 1))     read = true;
     else if (!strncmp(m, "ew", 2))     write = true;
     else if (!strncmp(m, "w" , 1))     write = true;
     else    DOMAIN_ERROR;
     errno = 0;
     if (FILE * f = sys_popen(path.c_str(), m))
        {
          file_entry fe(f, fileno(f));
          fe.fe_may_read = read;
          fe.fe_may_write = write;
          fe.fe_popen = true;
          open_files.push_back(fe);
          return Token(TOK_APL_VALUE1, IntScalar(fe.fe_fd, LOC));
        }
     if (errno)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
     return Token(TOK_APL_VALUE1, IntScalar(-1, LOC));
   }
) // NOT_MINGW
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__27(Value_P A, Value_P B)
{
const UCS_string old_name_ucs(*A.get());
const UCS_string new_name_ucs(*B.get());
UTF8_string old_name(old_name_ucs);
UTF8_string new_name(new_name_ucs);
   errno = 0;
const int result = rename(old_name.c_str(), new_name.c_str());
   if (result && errno)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
   return Token(TOK_APL_VALUE1, IntScalar(result, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__3(Value_P A, Value_P B)
{
const UCS_string mode_ucs(*A.get());
const UCS_string path_ucs(*B.get());
UTF8_string mode(mode_ucs);
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
const char * m = mode.c_str();
bool read = false;
bool write = false;
   if      (!strncmp(m, "r+", 2))     read = write = true;
   else if (!strncmp(m, "r" , 1))     read         = true;
   else if (!strncmp(m, "w+", 2))     read = write = true;
   else if (!strncmp(m, "w" , 1))            write = true;
   else if (!strncmp(m, "a+", 2))     read = write = true;
   else if (!strncmp(m, "a" , 1))            write = true;
   else    DOMAIN_ERROR;
   errno = 0;
FILE * f = fopen(path.c_str(), m);
   if (f == 0)   return Token(TOK_APL_VALUE1, IntScalar(-1, LOC));
file_entry fe(f, fileno(f));
   fe.path = path;
   fe.fe_may_read = read;
   fe.fe_may_write = write;
   open_files.push_back(fe);
   return Token(TOK_APL_VALUE1, IntScalar(fe.fe_fd, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__31(Value_P A, Value_P B)
{
const UCS_string mode_ucs(*A.get());
const UCS_string path_ucs(*B.get());
UTF8_string permissions(mode_ucs);
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
int perms = 0;
   loop(a, permissions.size())
       {
         int p = permissions[a] & 0xFF;
         if      (p == 'R')   perms |= R_OK;
         else if (p == 'r')   perms |= R_OK;
         else if (p == 'W')   perms |= W_OK;
         else if (p == 'w')   perms |= W_OK;
         else if (p == 'X')   perms |= X_OK;
         else if (p == 'x')   perms |= X_OK;
         else if (p == 'F')   perms |= F_OK;
         else if (p == 'f')   perms |= F_OK;
         else DOMAIN_ERROR;
       }
   errno = 0;
const int not_ok = access(path.c_str(), perms);
   if (not_ok)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__33(Value_P A, Value_P B)
{
const int fd = get_fd(*B.get());
SockAddr addr;
   memset(&addr, 0, sizeof(addr.inet));
   addr.inet.sin_family      = A->get_near_int(0);
   addr.inet.sin_addr.s_addr = htonl(A->get_near_int(1));
   addr.inet.sin_port        = htons(A->get_near_int(2));
   // return value now checked, not just inferred from errno being
   // nonzero (Blake McBride, Bugs17 #7): a successful call is explicitly
   // permitted to leave errno set from something earlier.
   errno = 0;
const int err = ::bind(fd, &addr.addr, sizeof(addr.inet));
   return Token(TOK_APL_VALUE1, IntScalar(err ? -errno : 0, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__34(Value_P A, Value_P B)
{
const int fd = get_fd(*B.get());
APL_Integer backlog = 10;
   if (A->element_count() > 0)
      backlog = A->get_near_int(0);
   errno = 0;
const int err = listen(fd, backlog);
   return Token(TOK_APL_VALUE1, IntScalar(err ? -errno : 0, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__36(Value_P A, Value_P B)
{
const int fd = get_fd(*B.get());
SockAddr addr;
   memset(&addr, 0, sizeof(addr.inet));
   addr.inet.sin_family      = A->get_near_int(0);
   addr.inet.sin_addr.s_addr = htonl(A->get_near_int(1));
   addr.inet.sin_port        = htons(A->get_near_int(2));
   errno = 0;
   // was 'sizeof(addr)': addr is a union also containing sockaddr_un
   // (much larger than sockaddr_in on this platform), so sizeof(addr)
   // overstates the real, populated sockaddr_in's length -- the extra
   // bytes were never written by memset() or the assignments above
   // either. bind() a few lines above already gets this right with
   // sizeof(addr.inet); match it here.
   //
   // Return value now checked, not just inferred from errno (Blake
   // McBride, Bugs17 #7).
const int err = connect(fd, &addr.addr, sizeof(addr.inet));
   return Token(TOK_APL_VALUE1, IntScalar(err ? -errno : 0, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__37(Value_P A, Value_P B)
{
   // A->get_near_int(0) is signed; copying it straight into a size_t
   // has no lower bound, so a negative A wraps to a huge size_t and
   // "new char[bytes]" attempts a huge allocation -- same bug already
   // fixed for the sibling ⎕FIO[8], see the comment there.
const APL_Integer bytes_signed = A->get_near_int(0);
   if (bytes_signed < 0)   LENGTH_ERROR;
const size_t bytes = bytes_signed;
const int fd = get_fd(*B.get());
vector<char> buffer(bytes);
   errno = 0;
const ssize_t len = recv(fd, buffer.data(), bytes, 0);
   if (len < 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
Value_P Z(len, LOC);
   loop(z, len)   Z->next_ravel_Int(buffer[z] & 0xFF);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__38(Value_P A, Value_P B)
{
   errno = 0;
const size_t bytes = A->element_count();
const int fd = get_fd(*B.get());
vector<char> buffer(bytes);
   loop(z, bytes)   buffer[z] = A->get_byte_value(z);
const ssize_t len = send(fd, buffer.data(), bytes, 0);
   if (len < 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
   return Token(TOK_APL_VALUE1, IntScalar(len, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__39(Value_P A, Value_P B)
{
UCS_string text(*A.get());
UTF8_string utf(text);
const int fd = get_fd(*B.get());
   errno = 0;
const ssize_t len = send(fd, utf.c_str(), utf.size(), 0);
   if (len < 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
   return Token(TOK_APL_VALUE1, IntScalar(len, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__41(Value_P A, Value_P B)
{
   // see eval_AXB__37 above: A->get_near_int(0) is signed and has no
   // lower bound when copied straight into a size_t.
const APL_Integer bytes_signed = A->get_near_int(0);
   if (bytes_signed < 0)   LENGTH_ERROR;
const size_t bytes = bytes_signed;
const int fd = get_fd(*B.get());
vector<char> buffer(bytes);
   errno = 0;
const ssize_t len = read(fd, buffer.data(), bytes);
   if (len < 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
Value_P Z(len, LOC);
   loop(z, len)   Z->next_ravel_Int(buffer[z] & 0xFF);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__42(Value_P A, Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__write);
const size_t bytes = A->element_count();
const int fd = get_fd(*B.get());
vector<char> buffer(bytes);
   loop(z, bytes)   buffer[z] = A->get_byte_value(z);
   errno = 0;
const ssize_t len = write(fd, buffer.data(), bytes);
   if (len < 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
   return Token(TOK_APL_VALUE1, IntScalar(len, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__43(Value_P A, Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__write);
const int fd = get_fd(*B.get());
UCS_string text(*A.get());
UTF8_string utf(text);
   errno = 0;
const ssize_t len = write(fd, utf.c_str(), utf.size());
   if (len < 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
   return Token(TOK_APL_VALUE1, IntScalar(len, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__46(Value_P A, Value_P B)
{
const APL_Integer level   = A->get_near_int(0);
const APL_Integer optname = A->get_near_int(1);
const int fd = get_fd(*B.get());
int optval = 0;
socklen_t olen = sizeof(optval);
   errno = 0;
const int ret = sys_getsockopt(fd, level, optname, &optval, &olen);
   if (ret < 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
   return Token(TOK_APL_VALUE1, IntScalar(optval, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__47(Value_P A, Value_P B)
{
const APL_Integer level   = A->get_near_int(0);
const APL_Integer optname = A->get_near_int(1);
const int optval          = A->get_near_int(2);
const int fd = get_fd(*B.get());
   errno = 0;
   sys_setsockopt(fd, level, optname, &optval, sizeof(optval));
   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__48(Value_P A, Value_P B)
{
FILE * file = get_FILE(*B);
const UCS_string format(*A.get());
File_or_String fos(file);
   errno = 0;
   return do_scanf(fos, format, 48);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__55(Value_P A, Value_P B)
{
const UCS_string format(*A.get());
const UCS_string data(*B.get());
File_or_String fos(&data);
   errno = 0;
   return do_scanf(fos, format, 55);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__56(Value_P A, Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__write);

   // 1. before opening the output file, check that A is valid.
   //
   errno = 0;
size_t items_written = 0;
const UCS_string path_ucs(*B.get());
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
   if (A->get_rank() > 1)   RANK_ERROR;
const ShapeItem line_count = A->element_count();
   loop(a, line_count)
       {
         Cell cache;
         const Cell & cA = A->get_cravel(a, cache);
         if (!cA.is_pointer_cell())
            {
               MORE_ERROR() <<
"The left argument of A ⎕FIO[56] B is not a nested vector of strings. The\n"
"first non-nested element is A[⎕IO+" << a << "].";
               DOMAIN_ERROR;
            }

         Value_P Ai = cA.get_pointer_value();
         if (!Ai->is_char_vector())
            {
               MORE_ERROR() <<
"The left argument of A ⎕FIO[56] B is not a nested vector of strings. The\n"
"first non-string element is A[⎕IO+" << a << "].";
               DOMAIN_ERROR;
            }
       }

   // 2. at this point As is OK. Write it to file Bs.
FILE * f = fopen(path.c_str(), "w");
   if (f == 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));

   loop(a, line_count)
       {
         Cell cache;
         const Cell & cA = A->get_cravel(a, cache);
         const Value & Ai = *cA.get_pointer_value();
         UCS_string line_ucs(Ai);
         UTF8_string line_utf(line_ucs);
         line_utf += '\n';
         const size_t len = line_utf.size();
         size_t written = fwrite(line_utf.c_str(), 1, len, f);
         if (len != written)
            {
              fclose(f);
              return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
            }
         items_written += len;
       }
   fclose(f);
   return Token(TOK_APL_VALUE1, IntScalar(items_written, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__58(Value_P A, Value_P B)
{
const UCS_string A_format(*A);
UCS_string UZ;
   do_snprintf(UZ, A_format, *B, 0, "A ⎕FIO.snprintf B");
Value_P Z(UZ, LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__59(Value_P A, Value_P B)
{
NOT_MINGW(
   {
     const int fd = get_fd(*B.get());
     errno = 0;
     int result = -1;
     switch(A->element_count())
        {
           case 1: result = fcntl(fd, A->get_near_int(0));
                   break;

           case 2: result = fcntl(fd, A->get_near_int(0),
                                      A->get_near_int(1));
                   break;

           default: LENGTH_ERROR;
        }

     if (result == -1 && errno)
        return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
     return Token(TOK_APL_VALUE1, IntScalar(result, LOC));
   }
) // NOT_MINGW
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__6(Value_P A, Value_P B)
{
   errno = 0;
   // see eval_AXB__37 above: A->get_near_int(0) is signed and has no
   // lower bound when copied straight into a size_t. Also bounded like
   // the sibling eval_AXB__8() (sized fgets) below: an oversized request
   // here isn't unsafe (vector<char>'s bad_alloc already turns it into a
   // controlled WS FULL, Blake McBride, Bugs16 minor), but there is no
   // reason to let a single ⎕FIO[6] ask for e.g. petabytes before finding
   // that out.
   //
const APL_Integer bytes_signed = A->get_near_int(0);
   if (bytes_signed < 0 || bytes_signed > 100000000)   LENGTH_ERROR;
const size_t bytes = bytes_signed;
FILE * file = get_FILE(*B);
   clearerr(file);
vector<char> buffer(bytes);
const size_t len = fread(buffer.data(), 1, bytes, file);
   if (len == 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
Value_P Z(len, LOC);
   loop(z, len)   Z->next_ravel_Int(buffer[z] & 0xFF);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__60(Value_P A, Value_P B)
{
   errno = 0;
   if (!A->is_scalar())   RANK_ERROR;
   if (!B->is_scalar())   RANK_ERROR;
const APL_Integer mode = A->get_near_int(0);
const APL_Integer len  = B->get_near_int(0);
   if (len < 1)    LENGTH_ERROR;
   if (len > 32)   LENGTH_ERROR;
   if (mode == 0 && len > 8)   LENGTH_ERROR;   // one int64 (Bugs37 #81)
Value_P Z = get_random(mode, len);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__7(Value_P A, Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__write);
   errno = 0;
const size_t bytes = A->element_count();
FILE * file = get_FILE(*B);
vector<char> buffer(bytes);
   loop(z, bytes)   buffer[z] = A->get_byte_value(z);
const size_t len = fwrite(buffer.data(), 1, bytes, file);
   return Token(TOK_APL_VALUE1, IntScalar(len, LOC));
}
//────────────────────────────────────────────────────────────────────────────
int
Quad_FIO::fgets_len(char * buffer, int size, FILE * file)
{
   // like fgets(buffer, size, file), but return the number of bytes read
   // (0 at EOF or on error). Unlike strlen() after fgets(), a NUL byte in
   // the line does not truncate it. Bugs37 #32.
   //
int len = 0;
   while (len < size - 1)
      {
        const int cc = getc(file);
        if (cc == EOF)   break;
        buffer[len++] = cc;
        if (cc == '\n')   break;
      }
   buffer[len] = 0;
   return len;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_AXB__8(Value_P A, Value_P B)
{
   errno = 0;
   // A->get_near_int(0) is signed; copying it straight into a size_t (as
   // was done before) has no lower bound, so Ai = ¯1 wrapped to SIZE_MAX,
   // making "new char[bytes + 1]" wrap to a 0-byte allocation while
   // "bytes" (truncated to fgets's int size parameter) became ¯1 --
   // a libc that accepts a non-positive size then writes a full line into
   // that 0-byte heap buffer. Validate the signed value first.
const APL_Integer bytes_signed = A->get_near_int(0);
   if (bytes_signed < 1 || bytes_signed > 100000000)   LENGTH_ERROR;
const size_t bytes = bytes_signed;
FILE * file = get_FILE(*B);
   clearerr(file);
vector<char> buffer(bytes + 1);
const int len = fgets_len(buffer.data(), int(bytes), file);
Value_P Z(len, LOC);
   loop(z, len)   Z->next_ravel_Int(buffer[z] & 0xFF);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B__0()
{
   errno = 0;
   Value_P Z(open_files.size(), LOC);
   loop(z, open_files.size())
       {
         Z->next_ravel_Int(open_files[z].fe_fd);
       }
   Z->set_proto_Int();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B__30(Value_P B)
{
   errno = 0;
   char buffer[APL_PATH_MAX + 1];
   char * success = getcwd(buffer, APL_PATH_MAX);
   if (!success)
      return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));

   buffer[APL_PATH_MAX] = 0;   // just in case
   UTF8_string buffer_utf(buffer);
   UCS_string cwd(buffer_utf);

   Value_P Z(cwd, LOC);
   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B___14()
{
   // SI_top is the ⎕FIO call, so dont show it.
   for (StateIndicator * si = Workspace::SI_top()->get_parent();
        si; si = si->get_parent())
       {
         CERR << "[" << si->get_level() << "]: ";
         si->get_prefix().print_stack(CERR, ".");
       }
    return Token(TOK_APL_VALUE1, IntScalar(0, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B___15()
{
const ShapeItem count = PFS_MAX3;
Shape sh(count, 2);
Value_P Z(sh, LOC);
#define perfo_4(id, b, name, thr) perfo_1(id, b, name, thr)
#define perfo_3(id, b, name, thr) perfo_1(id, b, name, thr)
#define perfo_2(id, b, name, thr) perfo_1(id, b, name, thr)
#define perfo_1(id, ab, name, _thr)         \
   { Z->next_ravel_Int(PFS_ ## id ## ab);   \
     UCS_string ucs(UTF8_string(#id #ab));  \
     Value_P uZ(ucs, LOC);                  \
     Z->next_ravel_Pointer(uZ.get());       }
#include "Performance.def"
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B___18(Value_P B)
{
const int64_t blocks = B->get_near_int(1);
const int64_t verbo  = B->get_near_int(2);
   // blocks * 512 below is computed in signed 64-bit and can itself wrap
   // to a small (or negative) value for a large blocks, in which case the
   // allocation SUCCEEDS at the wrapped-small size while probe_memory()
   // still writes using the original, huge blocks -- an OOB write that
   // the try/catch(bad_alloc) below does not catch, since no exception is
   // thrown in that case. Reject blocks that would overflow first.
   if (blocks < 1 || uint64_t(blocks) > SIZE_MAX / 512)   WS_FULL;
uint64_t * p = 0;
   try { p = new uint64_t[blocks * 512]; }
   catch (std::bad_alloc &) { WS_FULL; }
   catch (...)              { FIXME; }
   if (p == 0)
      {
         const APL_Integer K_bytes = blocks * 4;
         const APL_Integer M_bytes = K_bytes / 1024;
         CERR << "NOTE: " << blocks << " blocks = " << K_bytes
              << " Kbytes = " << M_bytes << " MBytes." << endl;
        WS_FULL;
      }

const APL_Integer errors =
      Sys::probe_memory(p, blocks, verbo);
   delete[] p;
   return Token(TOK_APL_VALUE1, IntScalar(errors, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B___2()
{
timeval tv = { 0, 100000 }; // 100 ms
const uint64_t from = cycle_counter();
   select(0, 0, 0, 0, &tv);
const uint64_t to = cycle_counter();

   return Token(TOK_APL_VALUE1, IntScalar(10*(to - from), LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B___5()
{
Value_P Z(256, LOC);
const Unicode * ibm = Avec::IBM_quad_AV();
   loop(c, 256)   Z->next_ravel_Char(ibm[c]);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_FIO::eval_B___6()
{
   if (!TM3_trigger_armed())   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

NOT_MINGW(
   {
     CERR << "NOTE: Resetting SIGSEGV handler and triggering "
             "a segfault..." << endl;

     // reset the SSEGV handler
     //
     struct sigaction action;
     memset(&action, 0, sizeof(struct sigaction));
     action.sa_handler = 0;
     sigaction(SIGSEGV, &action, 0);
     const APL_Integer result = *reinterpret_cast<char *>(4343);
     CERR << "NOTE: Throwing a segfault failed." << endl;
     return Token(TOK_APL_VALUE1, IntScalar(result, LOC));
   }
) // NOT_MINGW
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B___7()
{
   if (!TM3_trigger_armed())   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

   CERR << "NOTE: Triggering a segfault (keeping the current "
           "SIGSEGV handler)..." << endl;

const APL_Integer result = *reinterpret_cast<char *>(4343);
   CERR << "NOTE: Throwing a segfault failed." << endl;
   return Token(TOK_APL_VALUE1, IntScalar(result, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B___20()
{
   if (!TM3_trigger_armed())   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));

   CERR << "NOTE: Simulating a FIXME (internal-consistency-failure)..."
        << endl;
   FIXME;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B___8()
{
NOT_MINGW(
   {
     struct winsize ws;
     ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
     if (ws.ws_col)
        return Token(TOK_APL_VALUE1, IntScalar(ws.ws_col, LOC));
   }
) // NOT_MINGW
   return Token(TOK_APL_VALUE1, IntScalar(Workspace::get_PW(), LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_B___9()
{
NOT_MINGW(
   {
     struct winsize ws;
     ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
     return Token(TOK_APL_VALUE1, IntScalar(ws.ws_row, LOC));
   }
) // NOT_MINGW
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_LXB___1(Token & LO, Value_P B)
{
#if ! HAVE_RDTSC
   MORE_ERROR() << "Platform has no 'rtdsc' instruction.";
   DOMAIN_ERROR;
#endif

   // doit...
   //
cFunction_P fun = LO.get_function();
   Assert(fun);
const uint64_t from = cycle_counter();
   Workspace::SI_top()->set_safe_execution_depth();   // pretend ⎕ES
Token result = fun->eval_B(*B);
   if (result.get_tag() == TOK_SI_PUSHED)
      {
        benchmark_cycles_from = from | 1;
        return result;
      }

const uint64_t to = cycle_counter();
   Workspace::SI_top()->clear_safe_execution();
   return Token(TOK_APL_VALUE1, IntScalar(to - from, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__16(Value_P B)
{
   errno = 0;
FILE * file = get_FILE(*B);
   fflush(file);
   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__17(Value_P B)
{
NOT_MINGW(
   {
     errno = 0;
     const int fd = get_fd(*B.get());
     fsync(fd);
   }
   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
) // NOT_MINGW
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__18(Value_P B)
{
NOT_MINGW(
   {
     errno = 0;
     const int fd = get_fd(*B.get());
     struct stat s;
     const int result = fstat(fd, &s);
     if (result)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
     Value_P Z(Value_P(13, LOC));
     Z->next_ravel_Int(s.st_dev);
     Z->next_ravel_Int(s.st_ino);
     Z->next_ravel_Int(s.st_mode);
     Z->next_ravel_Int(s.st_nlink);
     Z->next_ravel_Int(s.st_uid);
     Z->next_ravel_Int(s.st_gid);
     Z->next_ravel_Int(s.st_rdev);
     Z->next_ravel_Int(s.st_size);
     Z->next_ravel_Int(s.st_blksize);
     Z->next_ravel_Int(s.st_blocks);
     Z->next_ravel_Int(s.st_atime);
     Z->next_ravel_Int(s.st_mtime);
     Z->next_ravel_Int(s.st_ctime);
     Z->check_value(LOC);
     return Token(TOK_APL_VALUE1, Z);
   }
) // NOT_MINGW
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__19(Value_P B)
{
   errno = 0;
const UCS_string path_ucs(*B.get());
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
   unlink(path.c_str());
   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__2(Value_P B)
{
// APL_Integer, not int: 2147483648 wrapped (Bugs37 #83g)
APL_Integer b = B->get_near_int(0);
   if (b < 0)   b = -b;
   if (b > 0x7FFFFFFF)
      {
        MORE_ERROR() << "⎕FIO[2] B: " << b << " is not an errno value";
        DOMAIN_ERROR;
      }
const char * text = strerror(int(b));
const int len = strlen(text);
Value_P Z(len, LOC);
   loop(t, len)   Z->next_ravel_Char(Unicode(text[t]));
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__20(Value_P B)
{
   errno = 0;
const UCS_string path_ucs(*B.get());
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
#if MINGW_SRC
   mkdir(path.c_str());
#else // ! MINGW_SRC
   mkdir(path.c_str(), 0777);
#endif // ! MINGW_SRC
   if (errno == EEXIST)   errno = 0;   // frequent non-error
   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__200(Value_P B, APL_Integer function_number)
{
const Pfstat_ID b = Pfstat_ID(B->get_near_int(0));
Statistics * stat = Performance::get_statistics(b);
   if (stat == 0)   DOMAIN_ERROR;   // bad statistics ID

   if (function_number == 200)   // reset statistics
      {
        stat->reset();
        return Token(TOK_APL_VALUE1, IntScalar(b, LOC));
      }

   // get statistics
   //
const int t = Performance::get_statistics_type(b);
UCS_string stat_name(UTF8_string(stat->get_name()));
Value_P Zsub(stat_name, LOC);
   if (t <= 2)   // cell function statistics
      {
        const Statistics_record * r1 = stat->get_first_record();
        const Statistics_record * rN = stat->get_record();
        Value_P Z(8, LOC);
        Z->next_ravel_Int(t);
        Z->next_ravel_Pointer(Zsub.get());
        Z->next_ravel_Int(r1->get_count());
        Z->next_ravel_Int(r1->get_sum());
        Z->next_ravel_Float(r1->get_sum2());
        Z->next_ravel_Int(rN->get_count());
        Z->next_ravel_Int(rN->get_sum());
        Z->next_ravel_Float(rN->get_sum2());
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }
   else           // function statistics
      {
        const Statistics_record * r = stat->get_record();
        Value_P Z(5, LOC);
        Z->next_ravel_Int(t);
        Z->next_ravel_Pointer(Zsub.get());
        Z->next_ravel_Int(r->get_count());
        Z->next_ravel_Int(r->get_sum());
        Z->next_ravel_Float(r->get_sum2());
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_FIO::eval_XB__202(Value_P B, APL_Integer function_number)
{
const Function * fun = 0;
   if (B->element_count() == 3)   // dyadic operator
      {
        const Unicode lfun = B->get_char_value(0);
        const Unicode oper = B->get_char_value(1);
        if (oper == UNI_FULLSTOP)
           {
             if (lfun == UNI_RING_OPERATOR)   // ∘.g
                fun = &Bif_OPER2_OUTER::fun;
             else                             // f.g
                fun = &Bif_OPER2_INNER::fun;
           }
      }
   else
      {
        const Unicode prim = B->get_char_value(0);

        // see the matching comment in eval_AXB__202() above (Blake
        // McBride, Bugs25 #8): tokenize_function()'s own is_function()
        // check on the next line is unreachable dead code -- its
        // fall-through asserts first. Pre-filter instead.
        //
        Unicode prim_copy = prim;   // uni_to_token() may rewrite its arg
        if (!Avec::uni_to_token(prim_copy, LOC).is_function())
           DOMAIN_ERROR;
        const Token tok = Tokenizer::tokenize_function(prim);
        fun = tok.get_function();
      }
   if (fun == 0)   DOMAIN_ERROR;
APL_Integer old_threshold;
   if (function_number == 202)
      {
        old_threshold = fun->get_monadic_threshold();
      }
   else
      {
        old_threshold = fun->get_dyadic_threshold();
      }
   return Token(TOK_APL_VALUE1, IntScalar(old_threshold, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__21(Value_P B)
{
   errno = 0;
const UCS_string path_ucs(*B.get());
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
   rmdir(path.c_str());
   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__24(Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__exec);

   // CHECK_SECURITY() above is a no-op unless the interpreter was built
   // with SECURITY_LEVEL_WANTED=1 or 2 (Security.hh) -- i.e. it does
   // NOT depend on --safe at the DEFAULT build level. --safe (uprefs.
   // safe_mode) already blocks )HOST (Cmd_HOST.cc) from spawning a
   // subprocess; this ⎕FIO function can equally spawn/exec one
   // (popen()/fork()+execve()), so honour the SAME runtime flag here,
   // independent of the compile-time security level. See Bugs27 #54.
   //
   if (UserPreferences::uprefs.safe_mode)   not_allowed("--safe");
const UCS_string path_ucs(*B.get());
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
   errno = 0;
FILE * f = sys_popen(path.c_str(), "r");
   if (f == 0)
      {
        if (errno)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
        return Token(TOK_APL_VALUE1, IntScalar(-1, LOC));
      }
file_entry fe(f, fileno(f));
   fe.fe_may_read = true;
   fe.fe_popen = true;
   open_files.push_back(fe);
   return Token(TOK_APL_VALUE1, IntScalar(fe.fe_fd, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__25(Value_P B)
{
   errno = 0;
file_entry & fe = get_file_entry(*B);

   // never pclose() stdin, stdout, or stderr (matches close_handle()'s
   // guard for fclose())
   //
   if (fe.fe_fd <= STDERR_FILENO)   // Bugs37 #83d
      {
        MORE_ERROR() << "Attempt to close fd " << fe.fe_fd
                     << " (stdin, stdout, or stderr)";
        DOMAIN_ERROR;
      }

   // Bugs31 #20 (Blake McBride): only a handle opened via sys_popen()
   // (⎕FIO[24]) may reach sys_pclose(); pclose() of any other FILE is
   // undefined behaviour.
   //
   // fe.fe_popen rather than an empty fe.path: a ⎕FIO[57] (fork/execve)
   // handle has no path either (Blake McBride, Bugs32 #28).
   //
   if (!fe.fe_popen)
      {
        MORE_ERROR() << "⎕FIO[25] (pclose): handle " << fe.fe_fd
                     << " was not opened by ⎕FIO[24] (popen)";
        DOMAIN_ERROR;
      }

int err = EBADF;   /* Bad file number */
   if (fe.fe_FILE)
      {
        err = sys_pclose(fe.fe_FILE);

        // erase fe's own entry (not necessarily the last one)
        //
        const size_t h = &fe - &open_files[0];
        open_files.erase(open_files.begin() + h);
      }
   if (err == -1)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
   return Token(TOK_APL_VALUE1, IntScalar(err, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__26(Value_P B)
{
NOT_MINGW(
   {
     errno = 0;
     const UCS_string path_ucs(*B.get());
     check_path(path_ucs);   // Bugs37 #83a
     const UTF8_string path(path_ucs);
     int fd = open(path.c_str(), O_RDONLY);
     if (fd == -1)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
     struct stat st;
     if (fstat(fd, &st))
        {
          close(fd);
          return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
        }
     if (!S_ISREG(st.st_mode))
        {
          close(fd);
          MORE_ERROR() << path << " is not a regular file";
          DOMAIN_ERROR;
        }
     ShapeItem len = st.st_size;

     // some regular files (e.g. in /proc) report st_size 0 but have
     // content: read them (instead of mmap()ing them), Bugs37 #83b
     //
     std::vector<UTF8> buffer;
     if (len == 0)
        {
          UTF8 chunk[4096];
          for (;;)
              {
                const ssize_t got = read(fd, chunk, sizeof(chunk));
                if (got <= 0)   break;
                buffer.insert(buffer.end(), chunk, chunk + got);
              }
          len = buffer.size();
        }
     const bool mapped = buffer.size() == 0;
     // mmap()ing 0 bytes is invalid (EINVAL) whenever HAVE_SYS_MMAN_H's
     // real mmap() is active -- an empty regular file is not an error,
     // so skip the call rather than surface -EINVAL for it; data is
     // never dereferenced below when len==0 (the fill loop is a no-op).
     const UTF8 * data = !mapped ? buffer.data()
                       : len ? Sys::mmap(fd, len) : 0;
     close(fd);
     if (len && data == 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));

     // Bugs10 #12 (Blake McBride): Value_P Z(len, LOC) can throw WS FULL;
     // without the try/catch that unwind never reaches Sys::munmap()
     // below and the mapping leaks.
     try
        {
          Value_P Z(len, LOC);
          Z->set_proto_Spc();
          loop(z, len)   Z->next_ravel_Char(Unicode(data[z]));
          if (mapped)   Sys::munmap(data, len);
          Z->set_proto_Spc();
          Z->check_value(LOC);
          return Token(TOK_APL_VALUE1, Z);
        }
     catch (...)
        {
          if (mapped)   Sys::munmap(data, len);
          throw;
        }
   }
) // NOT_MINGW
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__28(Value_P B, APL_Integer function_number)
{
   errno = 0;
const UCS_string path_ucs(*B.get());
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
DIR * dir = opendir(path.c_str());
   if (dir == 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));

vector<struct dirent> entries;
   for (;;)
       {
         dirent * entry = readdir(dir);
         if (entry == 0)   break;   // directory done

         // skip . and ..
         //
         const int entry_len = strlen(entry->d_name);
         if (entry->d_name[0] == '.')
            {
              if (entry_len == 1 ||
                  (entry_len == 2 && entry->d_name[1] == '.'))
                 continue;
            }

         entries.push_back(*entry);
       }
   closedir(dir);

Shape shape_Z(entries.size());
   if (function_number == 28)   // 5 by N matrix
      {
        shape_Z.add_shape_item(5);
      }

Value_P Z(shape_Z, LOC);

   loop(e, entries.size())
      {
        const dirent & dent = entries[e];
        if (function_number == 28)   // full dirent
           {
             // all platforms support inode number
             //
             Z->next_ravel_Int(dent.d_ino);

#ifdef _DIRENT_HAVE_D_OFF
             Z->next_ravel_Int(dent.d_off);
#else
             Z->next_ravel_1();
#endif

#ifdef _DIRENT_HAVE_D_RECLEN
             Z->next_ravel_Int(dent.d_reclen);
#else
             Z->next_ravel_1();
#endif

#ifdef _DIRENT_HAVE_D_TYPE
            Z->next_ravel_Int(dent.d_type);
#else
             Z->next_ravel_1();
#endif
           }   // function_number == 28

        UCS_string filename(UTF8_string(dent.d_name));
        Value_P Z_name(filename, LOC);
        Z->next_ravel_Pointer(Z_name.get());
      }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__3(Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__open);
const UCS_string path_ucs(*B.get());
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
   errno = 0;
FILE * f = fopen(path.c_str(), "r");
   if (f == 0)   return Token(TOK_APL_VALUE1, IntScalar(-1, LOC));
file_entry fe(f, fileno(f));
   fe.path = path;
   fe.fe_may_read = true;
   open_files.push_back(fe);
   return Token(TOK_APL_VALUE1, IntScalar(fe.fe_fd, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__32(Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__socket);
   UNSAFE("socket", 32);
   errno = 0;
APL_Integer domain = AF_INET;
APL_Integer type = SOCK_STREAM;
APL_Integer protocol = 0;
   if (B->element_count() > 0)   domain   = B->get_near_int(0);
   if (B->element_count() > 1)   type     = B->get_near_int(1);
   if (B->element_count() > 2)   protocol = B->get_near_int(2);
const int sock = socket(domain, type, protocol);
   if (sock == -1)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
file_entry fe(0, sock);
   fe.fe_may_read = true;
   fe.fe_may_write = true;
   open_files.push_back(fe);
   return Token(TOK_APL_VALUE1, IntScalar(fe.fe_fd, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__34(Value_P B)
{
const int fd = get_fd(*B.get());
   errno = 0;
   listen(fd, 10);
   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__35(Value_P B)
{
const int fd = get_fd(*B.get());
   errno = 0;
SockAddr addr;
socklen_t alen = sizeof(addr.inet);
const int sock = accept(fd, &addr.addr, &alen);
   if (sock == -1)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
file_entry nfe(0, sock);
   // file_entry(FILE*, int) default-initializes both flags to false;
   // an accepted TCP connection is always usable for both, same as a
   // freshly created socket() a few lines above (which does set both)
   // -- left unset here, so eval_XB__?? (the FIO[?] "may read/write"
   // reporting query, around line 3237) misreported every accepted
   // connection as neither readable nor writable.
   nfe.fe_may_read = true;
   nfe.fe_may_write = true;
   open_files.push_back(nfe);
Value_P Z(4, LOC);
   Z->next_ravel_Int(nfe.fe_fd);
   Z->next_ravel_Int(addr.inet.sin_family);
   Z->next_ravel_Int(ntohl(addr.inet.sin_addr.s_addr));
   Z->next_ravel_Int(ntohs(addr.inet.sin_port));
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__37(Value_P B)
{
   errno = 0;
const int fd = get_fd(*B.get());
char buffer[SMALL_BUF];
const ssize_t len = recv(fd, buffer, sizeof(buffer), 0);
   if (len < 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
Value_P Z(len, LOC);
   loop(z, len)   Z->next_ravel_Int(buffer[z] & 0xFF);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__4(Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__open);
   errno = 0;
file_entry & fe = get_file_entry(*B);
   if (fe.fe_fd <= STDERR_FILENO)   // Bugs37 #83d
      {
        MORE_ERROR() << "Attempt to close fd " << fe.fe_fd
                     << " (stdin, stdout, or stderr)";
        DOMAIN_ERROR;
      }

   // a popen() handle must be closed with sys_pclose(), which keeps its
   // SIGCHLD nesting counter balanced: fclose() left it incremented for
   // good, so that every later ⎕FIO[57] child became a zombie (Blake
   // McBride, Bugs32 #11).
   //
   if (fe.fe_popen)       sys_pclose(fe.fe_FILE);
   else if (fe.fe_FILE)   fclose(fe.fe_FILE);   // also closes fe.fe_fd
   else                   close(fe.fe_fd);
   fe = open_files.back();       // move last file to fe
   open_files.pop_back();        // erase last file
   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__40(Value_P B)
{
fd_set readfds;     FD_ZERO(&readfds);
fd_set writefds;    FD_ZERO(&writefds);
fd_set exceptfds;   FD_ZERO(&exceptfds);
timeval timeout = { 0, 0 };
fd_set * rd = 0;
fd_set * wr = 0;
fd_set * ex = 0;
timeval * to = 0;
APL_Integer max_fd = -1;

   if (B->element_count() > 4)   LENGTH_ERROR;
   if (B->element_count() < 1)   LENGTH_ERROR;

   if (B->element_count() >= 4)
      {
        const APL_Integer milli = B->get_near_int(3);
        if (milli < 0)   DOMAIN_ERROR;
        timeout.tv_sec = milli / 1000;
        timeout.tv_usec = (milli%1000) * 1000;
        to = &timeout;   // was missing: select() always blocked (NULL
                          // timeout) regardless of this argument.
      }

   if (B->element_count() >= 3)
      {
        Cell cache;
        const Cell & b2 = B->get_cravel(2, cache);
        if (!b2.is_pointer_cell())   DOMAIN_ERROR;
        Value_P vex = b2.get_pointer_value();
        loop(l, vex->element_count())
            {
              const int fd(vex->get_near_int(l));
              if (fd < 0)              DOMAIN_ERROR;
              if (fd >= FD_SETSIZE)    DOMAIN_ERROR;   // was >, off-by-one
              FD_SET(SOCKET(fd), &exceptfds);
              if (max_fd < fd)   max_fd = fd;
              ex = &exceptfds;
            }
      }

   if (B->element_count() >= 2)
      {
        Cell cache;
        const Cell & b1 = B->get_cravel(1, cache);
        if (!b1.is_pointer_cell())   DOMAIN_ERROR;
        Value_P vwr = b1.get_pointer_value();
        loop(l, vwr->element_count())
            {
              const APL_Integer fd = vwr->get_near_int(l);
              if (fd < 0)              DOMAIN_ERROR;
              if (fd >= FD_SETSIZE)    DOMAIN_ERROR;   // was >, off-by-one
              FD_SET(SOCKET(fd), &writefds);
              if (max_fd < fd)   max_fd = fd;
              wr = &writefds;
            }
      }

   if (B->element_count() >= 1)
      {
        Cell cache;
        const Cell & b0 = B->get_cfirst(cache);
        if (!b0.is_pointer_cell())   DOMAIN_ERROR;
        Value_P vrd = b0.get_pointer_value();
        loop(l, vrd->element_count())
            {
              const APL_Integer fd = vrd->get_near_int(l);
              if (fd < 0)              DOMAIN_ERROR;
              if (fd >= FD_SETSIZE)    DOMAIN_ERROR;   // was >, off-by-one
              FD_SET(SOCKET(fd), &readfds);
              if (max_fd < fd)   max_fd = fd;
              rd = &readfds;
            }
      }

const int count = select(max_fd + 1, rd, wr, ex, to);
   if (count < 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));

Value_P Z(5, LOC);
const APL_Integer milli_seconds = timeout.tv_sec*1000 + timeout.tv_usec/1000;
   Z->next_ravel_Int(count);
   // fds_to_val()'s loop is 'for (m=0; m<max_fd; ...)', an EXCLUSIVE
   // upper bound -- same convention as select()'s own nfds parameter
   // just above (which correctly used max_fd+1). Passing plain max_fd
   // (the highest fd VALUE, not value+1) here silently skipped
   // checking that highest fd itself, dropping it from the result
   // whenever it was actually ready.
   Z->next_ravel_Pointer(fds_to_val(rd, max_fd + 1).get());
   Z->next_ravel_Pointer(fds_to_val(wr, max_fd + 1).get());
   Z->next_ravel_Pointer(fds_to_val(ex, max_fd + 1).get());
   Z->next_ravel_Int(milli_seconds);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__41(Value_P B)
{
   errno = 0;
const int fd = get_fd(*B.get());
char buffer[SMALL_BUF];
const ssize_t len = read(fd, buffer, sizeof(buffer));
   if (len < 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
Value_P Z(len, LOC);
   loop(z, len)   Z->next_ravel_Int(buffer[z] & 0xFF);
   Z->set_proto_Int();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__44(Value_P B)
{
const int fd = get_fd(*B.get());
   errno = 0;
SockAddr addr;
socklen_t alen = sizeof(addr.inet);
const int ret = getsockname(fd, &addr.addr, &alen);
   if (ret == -1)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
Value_P Z(3, LOC);
   Z->next_ravel_Int(addr.inet.sin_family);
   Z->next_ravel_Int(ntohl(addr.inet.sin_addr.s_addr));
   Z->next_ravel_Int(ntohs(addr.inet.sin_port));
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__45(Value_P B)
{
const int fd = get_fd(*B.get());
   errno = 0;
SockAddr addr;
socklen_t alen = sizeof(addr.inet);
const int ret = getpeername(fd, &addr.addr, &alen);
   if (ret == -1)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
Value_P Z(3, LOC);
   Z->next_ravel_Int(addr.inet.sin_family);
   Z->next_ravel_Int(ntohl(addr.inet.sin_addr.s_addr));
   Z->next_ravel_Int(ntohs(addr.inet.sin_port));
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__49(Value_P B)
{
NOT_MINGW(
   {
     errno = 0;
     const UCS_string path_ucs(*B.get());
     check_path(path_ucs);   // Bugs37 #83a
     const UTF8_string path(path_ucs);
     const int fd = open(path.c_str(), O_RDONLY);
     if (fd == -1)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
     struct stat st;
     if (fstat(fd, &st))
        {
          close(fd);
          return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
        }
     if (!S_ISREG(st.st_mode))
        {
          close(fd);
          MORE_ERROR() << path << " is not a regular file";
          DOMAIN_ERROR;
        }
     ShapeItem len = st.st_size;

     // some regular files (e.g. in /proc) report st_size 0 but have
     // content: read them (instead of mmap()ing them), Bugs37 #83b
     //
     std::vector<UTF8> buffer;
     if (len == 0)
        {
          UTF8 chunk[4096];
          for (;;)
              {
                const ssize_t got = read(fd, chunk, sizeof(chunk));
                if (got <= 0)   break;
                buffer.insert(buffer.end(), chunk, chunk + got);
              }
          len = buffer.size();
        }
     const bool mapped = buffer.size() == 0;
     // mmap()ing 0 bytes is invalid (EINVAL) whenever HAVE_SYS_MMAN_H's
     // real mmap() is active -- an empty regular file is not an error
     // (0 lines), so skip the call rather than surface -EINVAL for it;
     // data is never dereferenced below when len==0 (both loops and the
     // "last line" check are gated on len).
     const UTF8 * data = !mapped ? buffer.data()
                       : len ? Sys::mmap(fd, len) : 0;
     close(fd);
     if (len && data == 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));

     // count number of LFs in the file
     //
     ShapeItem line_count = 0;
     loop(l, len)
         {
           if (data[l] == '\n')   ++line_count;
         }

     // if the last line does not end with \n then count it as well
     if (len && data[len - 1] != '\n')   ++line_count;

     // Bugs10 #12 (Blake McBride): Value_P Z()/ZZ() below can throw
     // WS FULL; without the try/catch that unwind never reaches
     // Sys::munmap() below and the mapping leaks.
     try
        {
          Value_P Z(line_count, LOC);
          Z->set_proto_Spc();

          const UTF8 * from = data;
          loop(l, len)
              {
                if (data[l] != '\n')   continue;
                const uint8_t * end = data + l;
               // discard CR before LF
                if (end > data && end[-1] == '\r')   --end;
                UTF8_string utf(from, end - from);
                UCS_string ucs(utf);
                Value_P ZZ(ucs, LOC);
                Z->next_ravel_Pointer(ZZ.get());
                from = data + l + 1;
              }

          if (len && data[len - 1] != '\n')   // incomplete final line
             {
                const uint8_t * end = data + len;
                if (end[-1] == '\r')   --end;   // discard trailing CR
                UTF8_string utf(from, end - from);
                UCS_string ucs(utf);
                Value_P ZZ(ucs, LOC);
                Z->next_ravel_Pointer(ZZ.get());
             }

          if (mapped)   Sys::munmap(data, len);
          Z->set_proto_Spc();
          Z->check_value(LOC);
          return Token(TOK_APL_VALUE1, Z);
        }
     catch (...)
        {
          if (mapped)   Sys::munmap(data, len);
          throw;
        }
   }
) // NOT_MINGW
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__50(Value_P B)
{
const APL_Integer unit = B->get_near_int(0);
timeval tv;
   gettimeofday(&tv, 0);
int64_t usec = tv.tv_sec;
   usec *= 1000000;
   usec += tv.tv_usec;
APL_Integer z = 0;
   if      (unit == 1)         z = usec/1000000;
   else if (unit == 1000)      z = usec/1000;
   else if (unit == 1000000)   z = usec;
   else
      {
        MORE_ERROR() <<
  "Invalid time unit (use 1 for seconds, 1000 for ms, or 1000000 for μs)";
        DOMAIN_ERROR;
      }
   return Token(TOK_APL_VALUE1, IntScalar(z, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__51(Value_P B)
{
   if (B->element_count() < 6 || B->element_count() > 9)   LENGTH_ERROR;
tm t;
   t.tm_year = B->get_near_int(0) - 1900;
   t.tm_mon  = B->get_near_int(1) - 1;
   t.tm_mday = B->get_near_int(2);
   t.tm_hour = B->get_near_int(3);
   t.tm_min  = B->get_near_int(4);
   t.tm_sec  = B->get_near_int(5);
   if (B->element_count() > 6)   // dst provided
      t.tm_isdst = B->get_near_int(6);
   else
      t.tm_isdst = -1;
const time_t seconds = mktime(&t);
   if (seconds == time_t(-1))   DOMAIN_ERROR;
Value_P Z(4, LOC);
   Z->next_ravel_Int(seconds);
   Z->next_ravel_Int(t.tm_wday);
   Z->next_ravel_Int(t.tm_yday);
   Z->next_ravel_Int(t.tm_isdst);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__52(Value_P B, APL_Integer function_number)
{
   if (B->element_count() != 1)   LENGTH_ERROR;
const time_t t = B->get_near_int(0);
const tm * tmp = (function_number == 52) ? localtime(&t) : gmtime(&t);
   if (tmp == 0)   DOMAIN_ERROR;
Value_P Z(9, LOC);
   Z->next_ravel_Int(tmp->tm_year + 1900);
   Z->next_ravel_Int(tmp->tm_mon + 1);
   Z->next_ravel_Int(tmp->tm_mday);
   Z->next_ravel_Int(tmp->tm_hour);
   Z->next_ravel_Int(tmp->tm_min);
   Z->next_ravel_Int(tmp->tm_sec);
   Z->next_ravel_Int(tmp->tm_wday);
   Z->next_ravel_Int(tmp->tm_yday);
   Z->next_ravel_Int(tmp->tm_isdst);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__54(Value_P B)
{
   // was "errno = chdir(...)": chdir() returns 0/-1, not an error number,
   // so this overwrote the real errno with -1 on every failure (Blake
   // McBride, Bugs15 #7) -- the function then always returned 1 instead
   // of the actual -errno, inverting the documented sign convention and
   // losing the real failure reason.
   //
const UCS_string path_ucs(*B.get());
check_path(path_ucs);   // Bugs37 #83a
const UTF8_string path(path_ucs);
   // errno = 0 immediately before the call, and the return value checked
   // before consulting errno at all: POSIX only guarantees errno is *set*
   // on failure, a successful call may still leave it non-zero, and the
   // two string conversions above (between an earlier errno reset and the
   // syscall) could otherwise leave something in errno that gets
   // misattributed to chdir() (Blake McBride, Bugs17 #7).
   errno = 0;
const int err = chdir(path.c_str());
   return Token(TOK_APL_VALUE1, IntScalar(err ? -errno : 0, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__57(Value_P B)
{
   CHECK_SECURITY(disable_Quad_FIO__exec);

   // CHECK_SECURITY() above is a no-op unless the interpreter was built
   // with SECURITY_LEVEL_WANTED=1 or 2 (Security.hh) -- i.e. it does
   // NOT depend on --safe at the DEFAULT build level. --safe (uprefs.
   // safe_mode) already blocks )HOST (Cmd_HOST.cc) from spawning a
   // subprocess; this ⎕FIO function can equally spawn/exec one
   // (popen()/fork()+execve()), so honour the SAME runtime flag here,
   // independent of the compile-time security level. See Bugs27 #54.
   //
   if (UserPreferences::uprefs.safe_mode)   not_allowed("--safe");
   errno = 0;
const int fd = do_FIO_57(*B.get(), 0);
   if (fd == -1)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
   return Token(TOK_APL_VALUE1, IntScalar(fd, LOC));
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__6(Value_P B)
{
   errno = 0;
FILE * file = get_FILE(*B);
   clearerr(file);
char buffer[SMALL_BUF];
const size_t len = fread(buffer, 1, SMALL_BUF, file);
   if (len == 0)   return Token(TOK_APL_VALUE1, IntScalar(-errno, LOC));
Value_P Z(len, LOC);
   loop(z, len)   Z->next_ravel_Int(buffer[z] & 0xFF);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__60(Value_P B)
{
   if (!B->is_scalar())   RANK_ERROR;
const APL_Integer len = B->get_near_int(0);
   if (len < 1)   LENGTH_ERROR;
   if (len > 8)   LENGTH_ERROR;
Value_P Z = get_random(0, len);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__62(Value_P B)
{
   // the socket constants of the platform on which GNU APL was compiled.
   // They differ between platforms (e.g. SOL_SOCKET is 1 on x86 Linux but
   // 0xFFFF on sparc64 Linux), so APL code should take them from here
   // rather than hard-coding them (Barak Pearlmutter).
   //
struct sock_const
   {
     const char * name;    ///< the name of the constant, e.g. "AF_INET"
     APL_Integer  value;   ///< the value of the constant on this platform
   };

#define SOCK_CONST(x) { #x, APL_Integer(x) },
static const sock_const constants[] =
   {
#ifdef AF_UNSPEC
     SOCK_CONST(AF_UNSPEC)
#endif
#ifdef AF_LOCAL
     SOCK_CONST(AF_LOCAL)
#endif
#ifdef AF_UNIX
     SOCK_CONST(AF_UNIX)
#endif
#ifdef AF_INET
     SOCK_CONST(AF_INET)
#endif
#ifdef AF_SNA
     SOCK_CONST(AF_SNA)
#endif
#ifdef AF_DECnet
     SOCK_CONST(AF_DECnet)
#endif
#ifdef AF_APPLETALK
     SOCK_CONST(AF_APPLETALK)
#endif
#ifdef AF_ROUTE
     SOCK_CONST(AF_ROUTE)
#endif
#ifdef AF_IPX
     SOCK_CONST(AF_IPX)
#endif
#ifdef AF_ISDN
     SOCK_CONST(AF_ISDN)
#endif
#ifdef AF_INET6
     SOCK_CONST(AF_INET6)
#endif
#ifdef AF_BLUETOOTH
     SOCK_CONST(AF_BLUETOOTH)
#endif
#ifdef AF_MAX
     SOCK_CONST(AF_MAX)
#endif
#ifdef PF_INET
     SOCK_CONST(PF_INET)
#endif
#ifdef PF_LOCAL
     SOCK_CONST(PF_LOCAL)
#endif
#ifdef PF_UNIX
     SOCK_CONST(PF_UNIX)
#endif
#ifdef SOCK_STREAM
     SOCK_CONST(SOCK_STREAM)
#endif
#ifdef SOCK_DGRAM
     SOCK_CONST(SOCK_DGRAM)
#endif
#ifdef SOCK_RAW
     SOCK_CONST(SOCK_RAW)
#endif
#ifdef SOCK_RDM
     SOCK_CONST(SOCK_RDM)
#endif
#ifdef SOCK_SEQPACKET
     SOCK_CONST(SOCK_SEQPACKET)
#endif
#ifdef SOCK_DCCP
     SOCK_CONST(SOCK_DCCP)
#endif
#ifdef SOCK_PACKET
     SOCK_CONST(SOCK_PACKET)
#endif
#ifdef SOCK_CLOEXEC
     SOCK_CONST(SOCK_CLOEXEC)
#endif
#ifdef SOCK_NONBLOCK
     SOCK_CONST(SOCK_NONBLOCK)
#endif
#ifdef IPPROTO_IP
     SOCK_CONST(IPPROTO_IP)
#endif
#ifdef IPPROTO_ICMP
     SOCK_CONST(IPPROTO_ICMP)
#endif
#ifdef IPPROTO_IPIP
     SOCK_CONST(IPPROTO_IPIP)
#endif
#ifdef IPPROTO_TCP
     SOCK_CONST(IPPROTO_TCP)
#endif
#ifdef IPPROTO_PUP
     SOCK_CONST(IPPROTO_PUP)
#endif
#ifdef IPPROTO_UDP
     SOCK_CONST(IPPROTO_UDP)
#endif
#ifdef IPPROTO_IDP
     SOCK_CONST(IPPROTO_IDP)
#endif
#ifdef IPPROTO_TP
     SOCK_CONST(IPPROTO_TP)
#endif
#ifdef IPPROTO_DCCP
     SOCK_CONST(IPPROTO_DCCP)
#endif
#ifdef IPPROTO_IPV6
     SOCK_CONST(IPPROTO_IPV6)
#endif
#ifdef IPPROTO_ROUTING
     SOCK_CONST(IPPROTO_ROUTING)
#endif
#ifdef IPPROTO_FRAGMENT
     SOCK_CONST(IPPROTO_FRAGMENT)
#endif
#ifdef IPPROTO_RSVP
     SOCK_CONST(IPPROTO_RSVP)
#endif
#ifdef IPPROTO_GRE
     SOCK_CONST(IPPROTO_GRE)
#endif
#ifdef IPPROTO_ESP
     SOCK_CONST(IPPROTO_ESP)
#endif
#ifdef IPPROTO_AH
     SOCK_CONST(IPPROTO_AH)
#endif
#ifdef IPPROTO_ICMPV6
     SOCK_CONST(IPPROTO_ICMPV6)
#endif
#ifdef IPPROTO_NONE
     SOCK_CONST(IPPROTO_NONE)
#endif
#ifdef IPPROTO_DSTOPTS
     SOCK_CONST(IPPROTO_DSTOPTS)
#endif
#ifdef IPPROTO_MTP
     SOCK_CONST(IPPROTO_MTP)
#endif
#ifdef IPPROTO_ENCAP
     SOCK_CONST(IPPROTO_ENCAP)
#endif
#ifdef IPPROTO_PIM
     SOCK_CONST(IPPROTO_PIM)
#endif
#ifdef IPPROTO_COMP
     SOCK_CONST(IPPROTO_COMP)
#endif
#ifdef IPPROTO_SCTP
     SOCK_CONST(IPPROTO_SCTP)
#endif
#ifdef IPPROTO_UDPLITE
     SOCK_CONST(IPPROTO_UDPLITE)
#endif
#ifdef IPPROTO_RAW
     SOCK_CONST(IPPROTO_RAW)
#endif
#ifdef SOL_SOCKET
     SOCK_CONST(SOL_SOCKET)
#endif
#ifdef SO_BINDTODEVICE
     SOCK_CONST(SO_BINDTODEVICE)
#endif
#ifdef SO_REUSEADDR
     SOCK_CONST(SO_REUSEADDR)
#endif
#ifdef SO_BROADCAST
     SOCK_CONST(SO_BROADCAST)
#endif
#ifdef SO_REUSEPORT
     SOCK_CONST(SO_REUSEPORT)
#endif
#ifdef SO_KEEPALIVE
     SOCK_CONST(SO_KEEPALIVE)
#endif
#ifdef SO_LINGER
     SOCK_CONST(SO_LINGER)
#endif
#ifdef SO_RCVBUF
     SOCK_CONST(SO_RCVBUF)
#endif
#ifdef SO_SNDBUF
     SOCK_CONST(SO_SNDBUF)
#endif
#ifdef SO_RCVTIMEO
     SOCK_CONST(SO_RCVTIMEO)
#endif
#ifdef SO_SNDTIMEO
     SOCK_CONST(SO_SNDTIMEO)
#endif
#ifdef SO_ERROR
     SOCK_CONST(SO_ERROR)
#endif
#ifdef SO_TYPE
     SOCK_CONST(SO_TYPE)
#endif
#ifdef INADDR_ANY
     SOCK_CONST(INADDR_ANY)
#endif
#ifdef INADDR_LOOPBACK
     SOCK_CONST(INADDR_LOOPBACK)
#endif
#ifdef INADDR_BROADCAST
     SOCK_CONST(INADDR_BROADCAST)
#endif
#ifdef TCP_NODELAY
     SOCK_CONST(TCP_NODELAY)
#endif
   };
#undef SOCK_CONST
enum { CONST_COUNT = sizeof(constants) / sizeof(*constants) };

   // B empty: return all constants as a CONST_COUNT×2 matrix (name value)
   //
   if (B->element_count() == 0)
      {
        Value_P Z(Shape(CONST_COUNT, 2), LOC);
        loop(c, CONST_COUNT)
           {
             Value_P Zname(UCS_string(UTF8_string(constants[c].name)), LOC);
             Z->next_ravel_Pointer(Zname.get());
             Z->next_ravel_Int(constants[c].value);
           }
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // return the value of the constant named name_val
   //
auto lookup = [](const Value & name_val) -> APL_Integer
   {
     if (!name_val.is_char_string())
        {
          MORE_ERROR() << "⎕FIO[62]: B shall be empty, a name, "
                          "or a vector of names";
          DOMAIN_ERROR;
        }

     const UCS_string name_ucs(name_val);
     const UTF8_string name(name_ucs);
     loop(c, CONST_COUNT)
        {
          if (!strcmp(name.c_str(), constants[c].name))
             return constants[c].value;
        }

     MORE_ERROR() << "⎕FIO[62]: unknown socket constant '" << name_ucs
                  << "' (⎕FIO[62] '' lists the known ones)";
     DOMAIN_ERROR;
   };

   // B is a name: return its value
   //
   if (B->is_char_string())
      return Token(TOK_APL_VALUE1, IntScalar(lookup(*B), LOC));

   // B is a vector of names: return their values
   //
   if (B->get_rank() > 1)   RANK_ERROR;

const ShapeItem len_B = B->element_count();
Value_P Z(len_B, LOC);
   loop(b, len_B)
       {
         Cell cache;
         const Cell & cB = B->get_cravel(b, cache);
         if (!cB.is_pointer_cell())
            {
              MORE_ERROR() << "⎕FIO[62]: B[" << (b + Workspace::get_IO())
                           << "] is not a name";
              DOMAIN_ERROR;
            }
         Z->next_ravel_Int(lookup(*cB.get_pointer_value()));
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_FIO::eval_XB__8(Value_P B)
{
   errno = 0;
FILE * file = get_FILE(*B);
   clearerr(file);
char buffer[SMALL_BUF];
const int len = fgets_len(buffer, SMALL_BUF, file);
Value_P Z(len, LOC);
   loop(z, len)   Z->next_ravel_Int(buffer[z] & 0xFF);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_FIO::fds_to_val(fd_set * fds, int max_fd)
{
int fd_count = 0;
   if (fds)
      {
        for (int m = 0; m < max_fd; ++m)   if (FD_ISSET(m, fds))   ++fd_count;
      }

Value_P Z(ShapeItem(fd_count), LOC);
   if (fds)
      {
        for (int m = 0; m < max_fd; ++m)
            if (FD_ISSET(m, fds))   Z->next_ravel_Int(m);
      }

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
FILE *
Quad_FIO::get_FILE(APL_Integer handle)
{
file_entry & fe = get_file_entry(handle);
   if (fe.fe_FILE == 0)
      {
        if (fe.fe_may_read && fe.fe_may_write)
           fe.fe_FILE = fdopen(fe.fe_fd, "a+");
        else if (fe.fe_may_read)
           fe.fe_FILE = fdopen(fe.fe_fd, "r");
        else if (fe.fe_may_write)
           fe.fe_FILE = fdopen(fe.fe_fd, "a");
        else
           {
             MORE_ERROR() << "bad (closed ?) file handle " << handle;
             DOMAIN_ERROR;   // internal error
           }
      }

   return fe.fe_FILE;
}
//────────────────────────────────────────────────────────────────────────────
Quad_FIO::file_entry &
Quad_FIO::get_file_entry(APL_Integer handle)
{
   // NOTE: handle is not narrowed to int (as before, Bugs34 #25), so that
   // H+2⋆32 is not taken as handle H

   loop(h, open_files.size())
      {
        if (open_files[h].fe_fd == handle)
           {
             last_handle = handle;   // see Handle_errno
             return open_files[h];
           }
      }

   MORE_ERROR() << handle
                << " is not an open file handle managed by ⎕FIO, see ⎕FIO 0.";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_FIO::get_random(APL_Integer mode, APL_Integer len)
{
const int device = open("/dev/urandom", O_RDONLY);
   if (device == -1)
      {
        MORE_ERROR() << "⎕FIO[60] failed when trying to open /dev/urandom: "
                     << strerror(errno);
        DOMAIN_ERROR;
      }

unsigned char buffer[32];   // caller has checked that len <= 32
const ssize_t bytes = read(device, buffer, len);
   close(device);

   if (bytes == -1)
      {
        MORE_ERROR() << "⎕FIO[60] failed when trying to read /dev/urandom: "
                     << strerror(errno);
        DOMAIN_ERROR;
      }

   // read() succeeding does not guarantee it filled the whole buffer --
   // a short read is legal per POSIX and was not checked for here, so
   // the mode==0/mode==1 loops below could read uninitialized stack
   // bytes (buffer[bytes..len-1]) as if they were real random data.
   if (bytes != len)
      {
        MORE_ERROR() << "⎕FIO[60]: short read from /dev/urandom ("
                     << ShapeItem(bytes) << " of " << len << " bytes)";
        DOMAIN_ERROR;
      }

   if (mode == 0)        // single integer result (len ≤ 8)
      {
         APL_Integer result = 0;
         loop(l, len)   result = result << 8 | (buffer[l] & 0xFF);
         return IntScalar(result, LOC);
      }
   else if (mode == 1)   // byte vector result
      {
        Value_P Z(len, LOC);
        loop(l, len)   Z->next_ravel_Int(buffer[l] & 0xFF);
        Z->check_value(LOC);
         return Z;
      }

   MORE_ERROR() << "Invalid mode A in A ⎕FIO[60] B";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_FIO::group_thousands(UCS_string & dest, char * buffer, bool flt,
                          char conv)
{
   // buffer is the 0-terminated and ASCII-only output of some snprintf("%..."),
   // and the user has requested thousands' separators in the integer part of
   // buffer.

   if (flt)   // buffer may or may not have a fractional part
      {
        if (char * dot = strchr(buffer, '.'))   // buffer has a  fractional part
           {
             *dot = 0;
             group_thousands(dest, buffer, false, conv);
             dest << Workspace::get_FC(0) << (dot + 1);
             return;
           }
      }

   // %p is a pointer, not a number: C does not group it even with the
   // ' flag, so pass it through unchanged (Blake McBride, Bugs23 #7).
   if (conv == 'p')
      {
        dest << buffer;
        return;
      }

   // at this point, buffer is integer only. For x/X, group hex digits
   // (a-f/A-F count as digits too, in groups of 3, same as C); for
   // everything else, group decimal digits only (Bugs23 #7).
   //
const bool hex = (conv == 'x' || conv == 'X');

   // a #-flag "0x"/"0X" prefix is not part of the number and must be
   // passed through without being counted or grouped.
   //
char * digits = buffer;
   if (hex && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
      {
        dest << Unicode(*digits++);
        dest << Unicode(*digits++);
      }

int digit_count = 0;
   for (const char * b = digits; *b; ++b)
       {
         const char cc = *b;
         if ((cc >= '0' && cc <= '9') ||
             (hex && ((cc >= 'a' && cc <= 'f') || (cc >= 'A' && cc <= 'F'))))
            ++digit_count;
       }

   for (const char * b = digits; *b;)
       {
         const char cc = *b++;
         dest << Unicode(cc);
         if ((cc >= '0' && cc <= '9') ||
             (hex && ((cc >= 'a' && cc <= 'f') || (cc >= 'A' && cc <= 'F'))))
            {
              if (--digit_count && !(digit_count % 3))
                 dest << Workspace::get_FC(1);
            }
         }
}
//────────────────────────────────────────────────────────────────────────────
APL_Integer
Quad_FIO::secs_epoch(const cValue & B)
{
   // B is a time/date like YYYY [MM [DD [HH [MM [SS]]]]]
   if (B.get_rank() > 1)   RANK_ERROR;
const int len_B = B.element_count();
   if (len_B < 1 || len_B > 6)   LENGTH_ERROR;

const struct {
               const char * name;
               int min_val;
               int max_val;
             } range[6] =
             {
               // year has no real upper bound of its own (the previous
               // 2037 cap was a 32-bit time_t artifact that the sibling
               // ⎕FIO[51] does not impose either, Blake McBride, Bugs16
               // #6); mktime()'s own time_t(-1) check below is what
               // actually bounds it.
               { "year",   0, INT_MAX },
               { "month",  1,      12 },   // was 1-11 (Bugs16 #6): this
                                            // table validates the APL value
                                            // (1-12, see tm_mon below), not
                                            // the tm range (0-11), so month
                                            // 12 (December) was refused
               { "day",    1,      31 },
               { "hour",   0,      23 },
               { "minute", 0,      59 },
               { "second", 0,      60 },
             };

// APL_Integer, not int: the range checks below must see the value as
// given, not narrowed to int (Bugs37 #55)
APL_Integer Bv[6];  // values as provided in B
   loop(b, 6)
       {
         if (len_B <= b)   Bv[b] = range[b].min_val;
         else              Bv[b] = B.get_near_int(b);
         if (Bv[b] < range[b].min_val)
            {
              MORE_ERROR() << "⎕FIO[61]: B["
                           << (b + Workspace::get_IO())
                           <<  "] (aka. " << range[b].name
                           << ") is too small (the valid range is "
                           << range[b].min_val << "-"
                           << range[b].max_val << ")";
              DOMAIN_ERROR;
            }
         else if (Bv[b] > range[b].max_val)
            {
              MORE_ERROR() << "⎕FIO[61]: B["
                           << (b + Workspace::get_IO())
                           <<  "] (aka. " << range[b].name
                           << ") is too large (the valid range is "
                           << range[b].min_val << "-"
                           << range[b].max_val << ")";
              DOMAIN_ERROR;
            }
       }

   if (Bv[0] < 100)   Bv[0] += 2000;   // interpret year xx as 20xx

tm tm;

                                  //                   APL B[]    mktime
   tm.tm_sec  = Bv[5];            // Seconds           0-60       (0-60)
   tm.tm_min  = Bv[4];            // Minutes           0-59       (0-59)
   tm.tm_hour = Bv[3];            // Hours             0-23       (0-23)
   tm.tm_mday = Bv[2];            // Day of the month  1-31       (1-31)
   tm.tm_mon  = Bv[1] - 1;        // Month             1-12       (0-11)
   tm.tm_year = Bv[0] - 1900;     // Year              2000...    1900...

   // B does not (and, unlike ⎕FIO[51], cannot) specify DST, so let
   // mktime() determine it -- tm.tm_isdst was uninitialized here before
   // (Blake McBride, Bugs16 #2): mktime() reads it to decide whether the
   // supplied fields are DST or standard time, so the result depended on
   // whatever indeterminate value was on the stack, e.g. the same call
   // evaluated twice in one statement could return two different answers.
   // matches the sibling eval_XB__51()'s no-DST-provided default.
   //
   tm.tm_isdst = -1;

const time_t z = mktime(&tm);
   if (z != time_t(-1))   return APL_Integer(z);

   MORE_ERROR() << "⎕FIO[61]: bad B";
   DOMAIN_ERROR;
}
//════════════════════════════════════════════════════════════════════════════

