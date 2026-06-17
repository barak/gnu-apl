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

#ifndef __PYTHONPIPE_HH_DEFINED__
#define __PYTHONPIPE_HH_DEFINED__

#include <stdio.h>
#include <sys/types.h>

#include "UCS_string.hh"
#include "UTF8_string.hh"

/// a pipe to a process running python
class PythonPipe
{
public:
   PythonPipe(const char * script);
   ~PythonPipe()   { close(); }

   /// read one line from python
   UCS_string read(bool keep_LF = false);

   /// write to python
   void write(UTF8_string cmd, bool no_LF = false);

   /// write one line and get one line back
   UCS_string wr_rd(const char * cmd);

protected:
   /// close the connection to python
   void close();

   /// name of the python script
   const char * python_script;

   /// PID of the python process
   pid_t pid;

   /// socket to the python process
   int fd;

};

#endif // __PYTHONPIPE_HH_DEFINED__
// EOF
