/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2016  Dr. Jürgen Sauermann

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

#ifndef __QUAD_FIO_HH_DEFINED__
#define __QUAD_FIO_HH_DEFINED__

#include <vector>

#include "Error_macros.hh"
#include "PrimitiveOperator.hh"
#include "QuadFunction.hh"
#include "UserPreferences.hh"

class File_or_String;

//-----------------------------------------------------------------------------

/**
   The system function Quad-FIO (File I/O)
 */
/// The class implementing ⎕FIO
class Quad_FIO : public QuadFunction
{
public:
   /// overloaded Function::is_operator()
   virtual bool is_operator() const   { return true; }

   /// the default buffer size if the user does not provide one
   enum { SMALL_BUF = 5000 };

   /// Constructor.
   Quad_FIO();

   /// overloaded Function::eval_B().
   virtual Token eval_B(Value_P B);

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(Value_P A, Value_P B);

   /// overloaded Function::eval_AXB().
   virtual Token eval_XB(Value_P X, Value_P B);

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B);

   /// overloaded Function::eval_AXB().
   virtual Token eval_LXB(Token & LO, Value_P X, Value_P B);

   /// close all open files
   void clear();

   /// fork(), execve(), and return a connection to fd 3 of forked process
   int do_FIO_57(const UCS_string & B);

   /// close a file descriptor (and its FILE * if any)
   int close_handle(int handle);

   /// get one Unicode from file
   static Unicode fget_utf8(FILE * file, ShapeItem & fget_count);

   /// return one mor more random values
   static Value_P get_random(APL_Integer mode, APL_Integer len);

   /// return the open FILE * for (APL integer) \b handle
   FILE * get_FILE(int handle);

   static Quad_FIO * fun;   ///< Built-in function.
   static Quad_FIO  _fun;   ///< Built-in function.

protected:
   /// one file (openend with open(), fopen(), or fdopen()).
   /// : handle == fd, and FILE * may or may not exist for fd
   struct file_entry
      {
        /// constructor
        file_entry() {}

        /// constructor
        file_entry(FILE * fp, int fd)
        : fe_FILE(fp),
          fe_fd(fd),
          fe_errno(0),
          fe_may_read(false),
          fe_may_write(false)
        {}

        /// assignment
        void operator =(const file_entry & other)
           {
             path = other.path;
             fe_FILE = other.fe_FILE;
             fe_fd = other.fe_fd;
             fe_errno = other.fe_errno;
             fe_may_read = other.fe_may_read;
             fe_may_write = other.fe_may_write;
           }

        UTF8_string path;      ///< filename
        FILE * fe_FILE;        ///< FILE * returned by fopen()
        int    fe_fd;          ///< file desriptor == fileno(file)
        int    fe_errno;       ///< errno for last operation
        bool   fe_may_read;    ///< file open for reading
        bool   fe_may_write;   ///< file open for writing
      };

   /// return the open file for (APL integer) \b handle
   file_entry & get_file_entry(int handle);

   /// return the open file for (APL integer) \b handle
   file_entry & get_file_entry(const Value & handle)
      { return get_file_entry(handle.get_ravel(0).get_near_int()); }

   /// return the open FILE * (APL integer) \b handle
   FILE * get_FILE(const Value & handle)
      { return get_FILE(handle.get_ravel(0).get_near_int()); }

   /// return the open file descriptor for (APL integer) \b handle
   int get_fd(const Value & value)
       {
         file_entry & fe = get_file_entry(value);   // may throw DOMAIN ERROR
         return fe.fe_fd;
       }

   /// throw a DOMAIN error if the interpreter runs in safe mode.
   void UNSAFE(const char * funname, int funnum)
      {
        if (uprefs.safe_mode)
           {
             MORE_ERROR() << "⎕FIO[" << funnum
                          << " is not permitted in safe mode (see ⎕ARGi)";
             DOMAIN_ERROR;
           }
      }

   /// list all ⎕IO functions to \b out
   Token list_functions(ostream & out);

   /// convert bits set in \b fds to an APL integer vector
   Value_P fds_to_val(fd_set * fds, int max_fd);

   /// printf A to \b out
   Token do_printf(FILE * out, Value_P A);

   /// sprintf with format string A and Data items B
   void do_sprintf(UCS_string & UZ, const UCS_string & A_format,
                   const Value * B, int B_start);

   /// perform an fscanf() from file
   Token do_scanf(File_or_String & input, const UCS_string & format);

   /// the open files
   std::vector<file_entry> open_files;
};
//-----------------------------------------------------------------------------
#endif //  __QUAD_FIO_HH_DEFINED__

