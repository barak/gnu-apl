/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2022  Dr. Jürgen Sauermann

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

#include "buildtag.hh"
#include "Command.hh"
#include "Common.hh"
#include "IO_Files.hh"
#include "LibPaths.hh"
#include "Macro.hh"
#include "Output.hh"
#include "NativeFunction.hh"
#include "Workspace.hh"
#include "UserPreferences.hh"

/** @file **/

/** \mainpage GNU APL

   GNU APL is a free interpreter for the programming language APL.

   GNU APL tries to be compatible with both the \b ISO \b standard \b 13751
   (aka. Programming Language APL, Extended) and to \b IBM \b APL2.

   It is \b NOT meant to be a vehicle for adding new features to the
   APL language. Its promary focus is on compatibility with APL2 and the
   ISO standard and portability to different platforms.
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
   // this fact and repeat fgets() once after a WINCH signal
   //
   got_WINCH = true;

struct winsize wsize;
   // TIOCGWINSZ is 0x5413 on GNU/Linux. We use 0x5413 instead of TIOCGWINSZ
   // because TIOCGWINSZ may not exist on all platforms
   //
   if (0 != ioctl(STDIN_FILENO, 0x5413, &wsize))   return;
   if (wsize.ws_col < MIN_Quad_PW)   return;
   if (wsize.ws_col > MAX_Quad_PW)   return;

   Workspace::set_PW(wsize.ws_col, LOC);
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
/// print argc and argv[]
static void
show_argv(int argc, const char ** argv)
{
   CERR << "argc: " << argc << endl;
   loop(a, argc)   CERR << "  argv[" << a << "]: '" << argv[a] << "'" << endl;

   // tell if stdin is open or closed
   //
   if (fcntl(STDIN_FILENO, F_GETFD))
      CERR << "stdin is: CLOSED" << endl;
   else
      CERR << "stdin is: OPEN" << endl;

   // tell if fd 3 is open or closed
   //
   if (fcntl(3, F_GETFD))
      CERR << "fd 3 is:  CLOSED" << endl;
   else
      CERR << "fd 3 is:  OPEN" << endl;
}
//----------------------------------------------------------------------------
/// print a welcome message (copyright notice)
static void
show_welcome(ostream & out, const char * argv0)
{
char c1[200];
char c2[200];
   snprintf(c1, sizeof(c1), "Welcome to GNU APL version %s", build_tag[1]);
   snprintf(c2, sizeof(c2), "for details run: %s --gpl.", argv0);

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
  "Copyright (C) 2008-2021  Dr. Jürgen Sauermann"                         ,
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
/// port uprefs.tcp_port on localhost
void
remap_stdio()
{
   if (uprefs.tcp_port <= 0)   return;

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
   local.sin_port = htons(uprefs.tcp_port);

   // fix bind() error when listening socket is openend too quickly
   {
     const int yes = 1;
     if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR,
                     &yes, sizeof(yes)) < 0)
        {
          perror("setsockopt(SO_REUSEADDR) failed");
        }

      // continue, since a failed setsockopt() is sort of OK here.
   }

   if (::bind(listen_socket, (const sockaddr *)&local, sizeof(local)))
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
             << uprefs.tcp_port << endl;

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
init_apl(int argc, const char * argv[])
{
   {
     // make curses happy
     //
     const char * term = getenv("TERM");
     if (term == 0 || *term == 0)   setenv("TERM", "dumb", 1);
   }

const bool log_startup0 = uprefs.parse_argv_0(argc, argv);
   if (LOG_argc_argv || log_startup0)
      {
         CERR << "argc/argv before expansion:\n";
         show_argv(argc, argv);
      }

   uprefs.expand_argv(argc, argv);

const bool log_startup = uprefs.parse_argv_1() || log_startup0;
   if (LOG_argc_argv || log_startup)
      {
         CERR << "argc/argv after expansion:\n";
         show_argv(uprefs.expanded_argv.size(), &uprefs.expanded_argv[0]);
      }


#ifdef DYNAMIC_LOG_WANTED
   if (log_startup)   Log_control(LID_startup, true);
#endif // DYNAMIC_LOG_WANTED

   init_1(argv[0], log_startup);

   uprefs.read_config_file(true,  log_startup);   // in /etc/gnu-apl.d/
   uprefs.read_config_file(false, log_startup);   // in $HOME/.config/gnu_apl/
   uprefs.read_threshold_file(true,  log_startup);  // dito parallel_thresholds
   uprefs.read_threshold_file(false, log_startup);  // dito parallel_thresholds

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
   if (uprefs.WINCH_sets_pw)
      {
        sigaction(SIGWINCH, &new_WINCH_action, &old_WINCH_action);
        signal_WINCH_handler(0);   // pretend window size change
      }
   else
      {
        Workspace::set_PW(uprefs.initial_pw, LOC);
      }

#if PARALLEL_ENABLED
   memset(&new_control_BSL_action, 0, sizeof(struct sigaction));
   new_control_BSL_action.sa_handler = &control_BSL;
   sigaction(SIGQUIT, &new_control_BSL_action, &old_control_BSL_action);
#endif

   uprefs.parse_argv_2(log_startup);

   // maybe use TCP connection instead of stdin/stderr. This function blocks
   // until a TCP connections was received.
   //
   remap_stdio();

   if (uprefs.CPU_limit_secs)
      {
        rlimit rl;
        getrlimit(RLIMIT_CPU, &rl);
        rl.rlim_cur = uprefs.CPU_limit_secs;
        setrlimit(RLIMIT_CPU, &rl);
      }

   if (uprefs.emacs_mode)
      {
        UCS_string info;
        if (uprefs.emacs_arg)
           {
             info = NativeFunction::load_emacs_library(uprefs.emacs_arg);
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

   if (uprefs.wait_ms)   usleep(1000*uprefs.wait_ms);

   init_2(log_startup);

   if (!uprefs.silent)   show_welcome(cout, argv[0]);

   if (log_startup)   CERR << "PID is " << getpid() << endl;
   Log(LOG_argc_argv || log_startup)   show_argv(argc, argv);

   if (ProcessorID::init(log_startup))
      {
        // error message printed in ProcessorID::init()
        return 8;
      }

   if (uprefs.do_Color)   Output::toggle_color("ON");

   if (uprefs.latent_expression.size())
      {
        // there was a --LX expression on the command line
        //
        UCS_string lx(uprefs.latent_expression);

        if (log_startup)
           CERR << "executing --LX '" << lx << "'" << endl;

        Command::process_line(lx);
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
         UCS_string cont("CONTINUE");
         UTF8_string filename =
            LibPaths::get_lib_filename(LIB0, cont, true, ".xml", ".apl");

         if (access(filename.c_str(), F_OK) == 0)
            {
              // CONTINUE workspace exists and was not inhibited by --noCONT
              //
              UCS_string load_cmd(")LOAD CONTINUE");
              Command::process_line(load_cmd);
              return 0;
            }

         // no CONTINUE workspace but maybe SETUP
         //
         cont = UCS_string("SETUP");
         filename =
            LibPaths::get_lib_filename(LIB0, cont, true, ".xml", ".apl");

         if (access(filename.c_str(), F_OK) == 0)
            {
              // SETUP workspace exists and was not inhibited by --noCONT
              //
              UCS_string load_cmd(")LOAD SETUP");
              Command::process_line(load_cmd);
              return 0;
            }
      }

   if (uprefs.initial_workspace.size())
      {
         // the user has provided a workspace name via -L
         //
         UCS_string init_ws(uprefs.initial_workspace);
         const char * cmd = uprefs.silent ? ")QLOAD " : ")LOAD ";
         UCS_string load_cmd(cmd);
         load_cmd.append(init_ws);
         Command::process_line(load_cmd);
      }

   Quad_TZ::compute_offset();
   return 0;
}
//----------------------------------------------------------------------------
/// dito.
int
main(int argc, const char *argv[])
{
   if (const int ret = init_apl(argc, argv))   return ret;

   if (uprefs.eval_exprs.size())
      {
         loop(e, uprefs.eval_exprs.size())
            {
              const char * expr = uprefs.eval_exprs[e];
              const UTF8_string expr_utf(expr);
              UCS_string expr_ucs(expr_utf);
              Command::process_line(expr_ucs);
            }
        Command::cmd_OFF(0);
        return 0;
      }

#if HAVE_PTHREAD_SETNAME_NP
         pthread_setname_np(pthread_self(), "apl/main");
#endif

   for (;;)
       {
         Token t = Workspace::immediate_execution(
                   IO_Files::test_mode == IO_Files::TM_EXIT_AFTER_FILE_ERROR);
         if (t.get_tag() == TOK_OFF)   Command::cmd_OFF(0);
       }

   return 0;
}
//----------------------------------------------------------------------------
