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

#include "config.h"

#include <fcntl.h>
#include <unistd.h>

#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif // HAVE_SYS_SOCKET_H

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif // HAVE_NETINET_IN_H

#if HAVE_WINSOCK2_H
# include <winsock2.h>
#endif

#if MINGW_SRC
# define socketpair(a, b, c, d) 1
#define fork() 1
#endif // MINGW_SRC

#include "Error_macros.hh"
#include "LibPaths.hh"
#include "PythonPipe.hh"

//════════════════════════════════════════════════════════════════════════════
PythonPipe::PythonPipe(const char * script)
  : fd(-1),
    pid(-1),
    python_script(script)

{
   // check that script is readable. We expect in in the current directory, or
   // else in APL_bin_path.
   //
UTF8_string argv1_utf;
   {
     const char * bin_dir = LibPaths::get_APL_bin_path();
     UTF8_string path1("./");      // current directory
     UTF8_string path2(bin_dir);   // where apl shll be installed
     path1 << script;
     path2 << script;
     if      (access(path1.c_str(), R_OK) == 0)   argv1_utf = path1;
     else if (access(path2.c_str(), R_OK) == 0)   argv1_utf = path2;
     else
        {
          MORE_ERROR() << "No python script '" << script
                       << "' in the current directory or in " << bin_dir;
          DOMAIN_ERROR;
        }
   }

int spair[2];   // spair[0]: apl,  spair[1]: python

   if (socketpair(AF_UNIX, SOCK_STREAM, 0, spair))
      {
        MORE_ERROR() << "PythonPipe(): socketpair() failed: "
                     << strerror(errno);
        DOMAIN_ERROR;
      }

const int & apl_sock    = spair[0];
const int & python_sock = spair[1];

   pid = fork();

   if (pid == -1)   // fork() failed
      {
        MORE_ERROR() << "PythonPipe(): fork() failed: " << strerror(errno);
        DOMAIN_ERROR;
      }

   if (pid)   // parent (caller (APL))
      {
        // ::close(python_sock);   // child end of the pipe
        //
        fd = apl_sock;
        ::close(python_sock);
        return;
      }

   // child (python)
   //
   dup2(python_sock, STDIN_FILENO);
   dup2(python_sock, STDOUT_FILENO);
   dup2(python_sock, STDERR_FILENO);
   ::close(apl_sock);   // parent end of the pipe
   ::close(python_sock);   // parent end of the pipe

// const char * program = "/bin/sh";
char * program = const_cast<char *>( "/usr/bin/python3" );
char * argv1  = const_cast<char *>(argv1_utf.c_str());
char * argv[] = { program, argv1, 0 };
char * env[] = { 0 };
   execve(argv[0], argv, env);

   // only reached if execve() has failed
   //
   cout << "\n\n*** child: execve() failed: " << strerror(errno) << "\n\n";
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
PythonPipe::read(bool keep_LF)
{

UTF8_string ret;
   ret.reserve(80);

   if (fd == -1)
      {
         ret << "<!END-OF-FILE!>.";
         return UCS_string(ret);   // connection was closed
      }

   for (;;)
       {
        UTF8 cc;
        const ssize_t len = ::read(fd, &cc, sizeof(cc));
        if (len != sizeof(cc))
           {
             CERR << endl << "<!PYTHON PIPE CLOSED!>." << endl;
             close();
             break;
           }

        ret.push_back(cc);
        if (cc == '\n')   break;
       }

   if (!keep_LF)
      {
        if (ret.size() && ret.back() == '\n')   ret.pop_back();
      }

   return UCS_string(ret);
}
//────────────────────────────────────────────────────────────────────────────
void
PythonPipe::write(UTF8_string cmd, bool no_LF )
{
   if (!no_LF)   cmd.push_back('\n');
const ssize_t len = ::write(fd, cmd.c_str(), cmd.size());
   Assert(len == int(cmd.size()));
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
PythonPipe::wr_rd(const char * cmd)
{
UTF8_string cmd_utf(cmd);
   write(cmd_utf);
   return read();
}
//────────────────────────────────────────────────────────────────────────────
void
PythonPipe::close()
{
   if (fd != -1)   ::close(fd);
   fd = -1;
}
//════════════════════════════════════════════════════════════════════════════

