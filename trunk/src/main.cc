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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <pthread.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <termios.h>

#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "Backtrace.hh"   // for init_DWARF()
#include "Command.hh"
#include "Common.hh"      // #includes config.h
#include "IO_Files.hh"
#include "LibPaths.hh"
#include "LineInput.hh"
#include "Macro.hh"
#include "NativeFunction.hh"
#include "Output.hh"
#include "Workspace.hh"
#include "UserPreferences.hh"

#if HAVE_IOCTL_TIOCGWINSZ   // platform has ioctl(fd, TIOCGWINSZ. ...)
# include <sys/ioctl.h>
#endif

#if HAVE_LIBELFIN_ELF_ELF___HH
# include <libelfin/elf/elf++.hh>
#endif

/** \mainpage GNU APL

   GNU APL is a free interpreter for the programming language APL.

   GNU APL tries to be compatible with both the \b ISO \b standard \b 13751
   (aka. Programming Language APL, Extended) and to \b IBM \b APL2.

   It is \b NOT meant to be a vehicle for adding new features to the
   APL language. Its primary focus is on compatibility with APL2 and the
   ISO standard and on the portability to different platforms.
 **/

/// when this file  was built
static const char * build_tag[] = { BUILDTAG, 0 };

//----------------------------------------------------------------------------

/// old sigaction argument for ^C
static struct sigaction old_control_C_action;

/// new sigaction argument for ^C
static struct sigaction new_control_C_action;

//----------------------------------------------------------------------------
/// old sigaction argument for segfaults
static struct sigaction old_SEGV_action;

/// new sigaction argument for segfaults
static struct sigaction new_SEGV_action;

/// signal handler for segfaults
static void
signal_SEGV_handler(int)
{
   CERR << "\n\n===================================================\n"
           "SEGMENTATION FAULT" << endl;

#if PARALLEL_ENABLED
   CERR << "thread: " << reinterpret_cast<const void *>(pthread_self()) << endl;
   Thread_context::print_all(CERR);
#endif // PARALLEL_ENABLED

   BACKTRACE

   CERR << "====================================================\n";

   // count errors
   IO_Files::assert_error();

   // restore terminal setting and
   LineInput::restore_termios();

   Command::cmd_OFF(3);
}
//----------------------------------------------------------------------------
/// old sigaction argument for SIGWINCH
static struct sigaction old_WINCH_action;

/// new sigaction argument for SIGWINCH
static struct sigaction new_WINCH_action;

/// signal handler for SIGWINCH
static void
signal_WINCH_handler(int)
{
   // fgets() returns EOF when the WINCH signal is received. We remember
   // this fact and repeat fgets() once after a WINCH signal. got_WINCH
   // is used in LineInput::safe_fgetc()
   //
   got_WINCH = true;


#if HAVE_IOCTL_TIOCGWINSZ


   // query the window size and set ⎕PW if we have ioctl TIOCGWINSZ
   //
    
   // MAX_Quad_PW is 10,000 or so, which is certainly larger than
   // the number of screen columns. We trust TIOCGWINSZ only if
   // it is resonably small. We allow it to be too small (and then
   // increase it). but ignore it if it is too large.
   //
struct winsize wsize;
   wsize.ws_col = MAX_Quad_PW;   // invalidate the column count
   if (0 == ioctl(STDIN_FILENO, TIOCGWINSZ, &wsize))
      {
        if (wsize.ws_col < MIN_Quad_PW)   // increase too small ws_col
           wsize.ws_col = MIN_Quad_PW;
        if (wsize.ws_col > MAX_Quad_PW)   // limit ws_col
           wsize.ws_col = MAX_Quad_PW;
        Workspace::set_PW(wsize.ws_col, LOC);
      }
#endif
   // just return if we don't have ioctl TIOCGWINSZ
   return;
}
//----------------------------------------------------------------------------
/// old sigaction argument for SIGUSR1
static struct sigaction old_USR1_action;

/// new sigaction argument for SIGUSR1
static struct sigaction new_USR1_action;

/// signal handler for SIGUSR1
static void
signal_USR1_handler(int)
{
   CERR << "Got signal USR1" << endl;
}
//----------------------------------------------------------------------------
/// old sigaction argument for SIGTERM
static struct sigaction old_TERM_action;

/// new sigaction argument for SIGTERM
static struct sigaction new_TERM_action;

/// signal handler for SIGTERM
static void
signal_TERM_handler(int)
{
   cleanup(true);
   sigaction(SIGTERM, &old_TERM_action, 0);
   raise(SIGTERM);
}
//----------------------------------------------------------------------------
#if PARALLEL_ENABLED
/// old sigaction argument for ^\,
static struct sigaction old_control_BSL_action;

/// new sigaction argument for ^\.
static struct sigaction new_control_BSL_action;

/// signal handler for ^\.
static void
control_BSL(int sig)
{
   CERR << endl << "^\\" << endl;
   Thread_context::print_all(CERR);
}
#endif // PARALLEL_ENABLED
//----------------------------------------------------------------------------
/// old sigaction argument for SIGHUP
static struct sigaction old_HUP_action;

/// new sigaction argument for SIGHUP
static struct sigaction new_HUP_action;

/// new signal handler for SIGHUP
static void
signal_HUP_handler(int)
{
   cleanup(true);
   sigaction(SIGHUP, &old_HUP_action, 0);
   raise(SIGHUP);
}
//----------------------------------------------------------------------------
/// print a welcome message (copyright notice)
static void
show_welcome(ostream & out, const char * argv0)
{
char c1[200];
char c2[200];
   SPRINTF(c1, "Welcome to GNU APL version %s", build_tag[1]);
   SPRINTF(c2, "for details run: %s --gpl.", argv0);

const char * lines[] =
{
  ""                                                                      ,
  "   ______ _   __ __  __    ___     ____   __ "                         ,
  "  / ____// | / // / / /   /   |   / __ \\ / / "                        ,
  " / / __ /  |/ // / / /   / /| |  / /_/ // /  "                         ,
  "/ /_/ // /|  // /_/ /   / ___ | / ____// /___"                         ,
  "\\____//_/ |_/ \\____/   /_/  |_|/_/    /_____/"                       ,
  ""                                                                      ,
  c1                                                                      ,
  ""                                                                      ,
  "Copyright © 2008-2025  Dr. Jürgen Sauermann"                         ,
  "Banner by FIGlet: www.figlet.org"                                      ,
  ""                                                                      ,
  "This program comes with ABSOLUTELY NO WARRANTY;"                       ,
  c2                                                                      ,
  ""                                                                      ,
  "This program is free software, and you are welcome to redistribute it" ,
  "according to the GNU Public License (GPL) version 3 or later."         ,
  ""                                                                      ,
  0
};

   // compute max. length
   //
int len = 0;
   for (const char ** l = lines; *l; ++l)
       {
         const char * cl = *l;
         const int clen = strlen(cl);
         if (len < clen)   len = clen;
       }
 
const int left_pad = (80 - len)/2;

   for (const char ** l = lines; *l; ++l)
       {
         const char * cl = *l;
         if (const int clen = strlen(cl))   // unless empty line
            {
              const int pad = left_pad + (len - clen)/2;
              loop(p, pad)   out << " ";
              out << cl;
            }
         out<< endl;
       }
}
//----------------------------------------------------------------------------
/// maybe remap stdin, stdout, and stderr to an incoming TCP connection to
/// port UserPreferences::uprefs.tcp_port on localhost
void
remap_stdio()
{
   if (UserPreferences::uprefs.tcp_port <= 0)   return;

const int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
   if (listen_socket == -1)
      {
        perror("socket() failed");
        exit(1);
      }

sockaddr_in local;
   memset(&local, 0, sizeof(local));
   local.sin_family = AF_INET;
   local.sin_addr.s_addr = htonl(0x7F000001);   // localhost (127.0.0.1)
   local.sin_port = htons(UserPreferences::uprefs.tcp_port);

   // fix bind() error when listening socket is openend too quickly
   {
     const int yes = 1;
     if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR,
                     &yes, sizeof(yes)) < 0)
        {
          perror("setsockopt(SO_REUSEADDR) failed");
        }

      // continue, since a failed setsockopt(SO_REUSEADDR) is sort of OK here.
   }

   if (::bind(listen_socket, reinterpret_cast<const sockaddr *>(&local),
              sizeof(local)))
      {
        perror("bind() failed");
        exit(1);
      }

   if (listen(listen_socket, 10))
      {
        perror("listen() failed");
        exit(1);
      }

   0 && CERR << "The GNU APL server is listening on TCP port "
             << UserPreferences::uprefs.tcp_port << endl;

   for (;;)   // connection server loop
       {
         sockaddr_in remote;
         socklen_t remote_len = sizeof(remote);
         const int connection = ::accept(listen_socket,
                                       reinterpret_cast<sockaddr *>(&remote),
                                       &remote_len);
         if (connection == -1)
            {
              perror("accept() failed");
              exit(1);
            }

         0 && CERR << "GNU APL server got TCP connction from "
                   << (ntohl(remote.sin_addr.s_addr) >> 24 & 0xFF) << "."
                   << (ntohl(remote.sin_addr.s_addr) >> 16 & 0xFF) << "."
                   << (ntohl(remote.sin_addr.s_addr) >>  8 & 0xFF) << "."
                   << (ntohl(remote.sin_addr.s_addr) >>  0 & 0xFF) << " port "
                   << ntohs(remote.sin_port)                       << endl;

         // fork() and let the client return while the server remains in this
         // server loop for the next connection.
         //
         const pid_t fork_result = fork();
         if (fork_result == -1)   // fork() failed
            {
              close(connection);
              perror("fork() failed");
              exit(1);
            }

         if (fork_result)   // parent (server)
            {
              close(connection);
              continue;
            }

         // child (client)
         //
         close(listen_socket);
         dup2(connection, STDIN_FILENO);
         dup2(connection, STDOUT_FILENO);
         dup2(connection, STDERR_FILENO);
         close(connection);
         return;
       }
}
//----------------------------------------------------------------------------
/// initialize the interpreter
int
init_apl(const std::vector<const char *> & args)
{
   {
     // make curses happy
     //
     const char * term = getenv("TERM");
     if (term == 0 || *term == 0)   setenv("TERM", "dumb", 1);
   }

  // collect all user preferences.
  //
const bool log_startup =
      UserPreferences::uprefs.collect_preferences(args);

   // NOTE: struct sigaction differs between GNU/Linux and other systems,
   // which causes compile errors for direct curly bracket assignment on
   // some systems.
   //
   // We therefore memset everything to 0 and then set the handler (which
   // should compile on GNU/Linux and also on other systems.
   //
   memset(&new_control_C_action, 0, sizeof(struct sigaction));
   memset(&new_WINCH_action,     0, sizeof(struct sigaction));
   memset(&new_USR1_action,      0, sizeof(struct sigaction));
   memset(&new_SEGV_action,      0, sizeof(struct sigaction));
   memset(&new_TERM_action,      0, sizeof(struct sigaction));
   memset(&new_HUP_action,       0, sizeof(struct sigaction));

   new_control_C_action.sa_handler = &control_C;
   new_WINCH_action    .sa_handler = &signal_WINCH_handler;
   new_USR1_action     .sa_handler = &signal_USR1_handler;
   new_SEGV_action     .sa_handler = &signal_SEGV_handler;
   new_TERM_action     .sa_handler = &signal_TERM_handler;
   new_HUP_action      .sa_handler = &signal_HUP_handler;

   sigaction(SIGINT,   &new_control_C_action, &old_control_C_action);
   sigaction(SIGUSR1,  &new_USR1_action,      &old_USR1_action);
   sigaction(SIGSEGV,  &new_SEGV_action,      &old_SEGV_action);
   sigaction(SIGTERM,  &new_TERM_action,      &old_TERM_action);
   sigaction(SIGHUP,   &new_HUP_action,       &old_HUP_action);
   signal(SIGCHLD, SIG_IGN);   // do not create zombies

#if HAVE_IOCTL_TIOCGWINSZ
   // Enable the ability to change ⎕PW on window resize only if the
   // platform supports ioctl TIOCGWINSZ
   //
const UserPreferences & uprefs = UserPreferences::uprefs;    
   if (uprefs.WINCH_sets_pw)
      {
        // IF WINCH_sets_pw preference is enabled, set up a handler for the
        // SIGWINCH signal.
        sigaction(SIGWINCH, &new_WINCH_action, &old_WINCH_action);
         
        // The platform supports reading back of the window size. If the user
        // wants ⎕PW to be controlled by the window size, then she most likely
        // also wants ⎕PW to be controlled by the window size at start-up.
        // We do this by by using the WINCH signal handler to pretend that
        // there's a window size change
        
        signal_WINCH_handler(0);   // pretend window size change
        got_WINCH = false;
      }
#endif

    // the final word should be the user preferences (if any).
    //
    if (uprefs.initial_PW_by_user)
       {
         Workspace::set_PW(uprefs.initial_PW, LOC);
       }

#if PARALLEL_ENABLED
   memset(&new_control_BSL_action, 0, sizeof(struct sigaction));
   new_control_BSL_action.sa_handler = &control_BSL;
   sigaction(SIGQUIT, &new_control_BSL_action, &old_control_BSL_action);
#endif

   // maybe use TCP connection instead of stdin/stderr. This function blocks
   // until a TCP connections was received.
   //
   remap_stdio();

   if (uprefs.CPU_limit_secs)
      {
        rlimit rl;
        getrlimit(RLIMIT_CPU, &rl);
        rl.rlim_cur = UserPreferences::uprefs.CPU_limit_secs;
        setrlimit(RLIMIT_CPU, &rl);
      }

   if (UserPreferences::uprefs.emacs_mode)
      {
        UCS_string info;
        if (const char * emacs_arg = uprefs.emacs_arg)
           {
             info = NativeFunction::load_emacs_library(emacs_arg);
           }

        if (info.size())   // problems loading library
           {
             CERR << info << endl;
           }
        else
           {
             // CIN = U+F00C0 = UTF8 F3 B0 83 80 ...
             Output::color_CIN[0] = 0xF3;
             Output::color_CIN[1] = 0xB0;
             Output::color_CIN[2] = 0x83;
             Output::color_CIN[3] = 0x80;
             Output::color_CIN[4] = 0;

             // COUT = U+F00C1 = UTF8 F3 B0 83 81 ...
             Output::color_COUT[0] = 0xF3;
             Output::color_COUT[1] = 0xB0;
             Output::color_COUT[2] = 0x83;
             Output::color_COUT[3] = 0x81;
             Output::color_COUT[4] = 0;

             // CERR = U+F00C2 = UTF8 F3 B0 83 82 ...
             Output::color_CERR[0] = 0xF3;
             Output::color_CERR[1] = 0xB0;
             Output::color_CERR[2] = 0x83;
             Output::color_CERR[3] = 0x82;
             Output::color_CERR[4] = 0;

             // UERR = U+F00C3 = UTF8 F3 B0 83 83 ...
             Output::color_UERR[0] = 0xF3;
             Output::color_UERR[1] = 0xB0;
             Output::color_UERR[2] = 0x83;
             Output::color_UERR[3] = 0x83;
             Output::color_UERR[4] = 0;

             // no clear_EOL
             Output::clear_EOL[0] = 0;
           }
      }

   if (uprefs.daemon)
      {
        const pid_t pid = fork();
        if (pid)   // parent
           {
             Log(LOG_startup)
                CERR << "parent pid = " << getpid()
                     << " child pid = " << pid << endl;

             exit(0);   // parent returns
           }

        Log(LOG_startup)
           CERR << "child forked (pid" << getpid() << ")" << endl;
      }

   if (const int wait = uprefs.wait_ms)   usleep(1000*wait);

   init_modules2(log_startup);

   if (!uprefs.silent)   show_welcome(cout, args[0]);

   if (log_startup)   CERR << "PID is " << getpid() << endl;

   if (ProcessorID::init(log_startup))
      {
        // error message printed in ProcessorID::init()
        return 8;
      }

   if (uprefs.do_Color)   Output::toggle_color(UTF8_string("ON"));

   if (uprefs.latent_expression.size())
      {
        // there was a --LX expression on the command line
        //
        UCS_string lx(uprefs.latent_expression);
        if (log_startup)   CERR << "executing --LX '" << lx << "'" << endl;
        Command::process_line(lx, 0);
      }

   // maybe )LOAD the CONTINUE or SETUP workspace. Do that unless the user 
   // has given
   //
   // (1) --noCONT, or
   // (2) --script (which implies --noCONT), or
   // (3)  -L wsname
   //
   if (uprefs.do_CONT && !uprefs.initial_workspace.size())
      {
         UCS_string cont(UTF8_string("CONTINUE"));
         UTF8_string filename =
            LibPaths::get_lib_filename(LIB0, cont, true, ".xml", ".apl");

         if (access(filename.c_str(), F_OK) == 0)
            {
              // CONTINUE workspace exists and was not inhibited by --noCONT
              //
              UCS_string load_cmd(UTF8_string(")LOAD CONTINUE"));
              Command::process_line(load_cmd, 0);
              return 0;
            }

         // no CONTINUE workspace but maybe SETUP
         //
         cont = UCS_ASCII_string("SETUP");
         filename =
            LibPaths::get_lib_filename(LIB0, cont, true, ".xml", ".apl");

         if (access(filename.c_str(), F_OK) == 0)
            {
              // SETUP workspace exists and was not inhibited by --noCONT
              //
              UCS_string load_cmd(UTF8_string(")LOAD SETUP"));
              Command::process_line(load_cmd, 0);
              return 0;
            }
      }

   if (uprefs.initial_workspace.size())
      {
         // the user has provided a workspace name via -L
         //
         UCS_string init_ws(uprefs.initial_workspace);
         const char * cmd = uprefs.silent ? ")QLOAD " : ")LOAD ";
         const UTF8_string utf(cmd);
         UCS_string load_cmd(utf);
         load_cmd.append(init_ws);
         Command::process_line(load_cmd, 0);
      }

   Quad_TZ::compute_offset();

   // we allocate a mmap'ed dwarf object beforhand so that we do not need
   // to create one when, as often, a WS_FULL is thrown.
   //
   init_DWARF(LibPaths::get_APL_bin_path(), LibPaths::get_APL_bin_name());

   return 0;   // OK.
}
//----------------------------------------------------------------------------
/// dito.
int
main(int argc, const char *argv[])
{
std::vector<const char *> args(argc);
   loop(a, argc)   args[a] = argv[a];
   if (const int ret = init_apl(args))   return ret;

   if (UserPreferences::uprefs.eval_exprs.size())
      {
         loop(e, UserPreferences::uprefs.eval_exprs.size())
            {
              const char * expr = UserPreferences::uprefs.eval_exprs[e];
              const UTF8_string expr_utf(expr);
              UCS_string expr_ucs(expr_utf);
              Command::process_line(expr_ucs, 0);
            }
        Command::cmd_OFF(0);
        return 0;
      }

#if HAVE_PTHREAD_SETNAME_NP
         pthread_setname_np(pthread_self(), "apl/main");
#endif

   for (const bool exit_on_error = IO_Files::exit_on_error();;)
       {
         const Token tok = Workspace::immediate_execution(exit_on_error);
         if (tok.get_tag() == TOK_OFF)   Command::cmd_OFF(0);
       }

   return 0;
}

const int libapl_version = 0;
int64_t get_main()
{
   return reinterpret_cast<int64_t>(&main);
}
//----------------------------------------------------------------------------
