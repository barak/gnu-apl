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

#ifndef __QUAD_FIO_HH_DEFINED__
#define __QUAD_FIO_HH_DEFINED__

#include <config.h>
#include <signal.h>

#if HAVE_WINSOCK2_H
#  include <winsock2.h>
#endif // HAVE_WINSOCK2_H

#include "Error_macros.hh"
#include "PrimitiveOperator.hh"
#include "QuadFunction.hh"
#include "UserPreferences.hh"

class File_or_String;

//----------------------------------------------------------------------------

/**
   The system function Quad-FIO (File I/O)
 */
/// The class implementing ⎕FIO
class Quad_FIO : public QuadFunction
{
public:
   /// Constructor.
   Quad_FIO();

   /// the default buffer size if the user does not provide one
   enum { SMALL_BUF = 5000 };

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_AXB().
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// overloaded Function::eval_B().
   virtual Token eval_B(Value_P B) const;

   /// close a file descriptor (and its FILE * if any)
   static int close_handle(int handle);

   /// fork(), execve(), and return a connection to fd 3 of forked process
   /// @param B command string to execute in the forked process
   /// @param envp environment variables for the forked process
   static int do_FIO_57(const UCS_string & B, char * const * envp);

   static Quad_FIO  fun;   ///< Built-in function.

   /// get one Unicode from file
   /// @param file open file to read from
   /// @param fget_count running count of bytes consumed
   static Unicode fget_utf8(FILE * file, ShapeItem & fget_count);

   /// close all open files
   static void clear();

   /// cycle counter at start of a benchmark (⎕FIO[-1])
   static uint64_t benchmark_cycles_from;

   /// return the open FILE * for (APL integer value) \b handle
   /// @param handle APL value holding the integer file handle
   static FILE * get_FILE(const Value & handle)
      { return get_FILE(handle.get_cscalar().get_near_int()); }

protected:
   /// a mapping between function names and function numbers
   static const FunctionGroup::function_info subfunction_infos[];

   /// overloaded Function::is_operator()
   virtual bool is_operator() const   { return true; }

   /// overloaded Function::get_fun_valence()
   virtual int get_fun_valence() const   { return 2; }

   /// overloaded Function::get_oper_valence().
   virtual int get_oper_valence() const   { return 1; }

   /// overloaded Function::eval_ALXB().
   /// @param A left APL value argument
   /// @param LO left operator function token
   /// @param X axis value
   /// @param B right APL value argument
   virtual Token eval_ALXB(Value_P A, Token & LO, Value_P X, Value_P B) const;

   /// overloaded Function::eval_LXB().
   /// @param LO left operator function token
   /// @param X axis value
   /// @param B right APL value argument
   virtual Token eval_LXB(Token & LO, Value_P X, Value_P B) const;

   /// return one or more random values
   /// @param mode distribution/type selector for random generation
   /// @param len number of random values to generate
   static Value_P get_random(APL_Integer mode, APL_Integer len);

   /// return the open FILE * for (APL integer) \b handle
   /// @param handle integer file handle index
   static FILE * get_FILE(int handle);

   /// one file (openend with open(), fopen(), or fdopen()).
   /// : handle == fd, and FILE * may or may not exist for fd
   struct file_entry
      {
        /// constructor
        file_entry() {}

        /// constructor
        /// @param fp FILE pointer returned by fopen()
        /// @param fd file descriptor number
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
   /// @param handle integer file handle index
   static file_entry & get_file_entry(int handle);

   /// return the open file for (APL integer) \b handle
   /// @param handle APL value holding the integer file handle
   static file_entry & get_file_entry(const Value & handle)
      { return get_file_entry(handle.get_cscalar().get_near_int()); }

   /// return the open file descriptor for (APL integer) \b handle
   /// @param value APL value holding the integer file handle
   static int get_fd(const Value & value)
       {
         const file_entry & fe = get_file_entry(value);   // may throw DOMAIN ERROR
         return fe.fe_fd;
       }

   /// append ASCII-buffer \b buffer to dest, inserting thousands' separators.
   /// Note: \b buffer may be modified.
   /// @param dest destination string to append to
   /// @param buffer ASCII numeric buffer (may be modified in place)
   /// @param flt true if the number is floating-point
   static void group_thousands(UCS_string & dest, char * buffer, bool flt);

   /// throw a DOMAIN error if the interpreter runs in safe mode.
   /// @param funname name of the function being guarded
   /// @param funnum ⎕FIO subfunction number
   static void UNSAFE(const char * funname, int funnum)
      {
        if (UserPreferences::uprefs.safe_mode)
           {
             MORE_ERROR() << "⎕FIO[" << funnum
                          << " is not permitted in safe mode (see ⎕ARG)";
             DOMAIN_ERROR;
           }
      }

   /// list all ⎕IO functions to \b out
   Token list_functions(ostream & out) const;

   /// helper function to print the subfunction syntax mapping
   virtual void print_map_syntax(ostream & out,
                                 const function_info & info) const;

   /// convert bits set in \b fds to an APL integer vector
   /// @param fds file-descriptor bit set from select()
   /// @param max_fd highest file descriptor to inspect
   static Value_P fds_to_val(fd_set * fds, int max_fd);

   /// fprintf A to file \b out
   /// @param out file to write formatted output to
   /// @param A APL value containing format string and data
   static Token do_fprintf(FILE * out, Value_P A);

   /// snprintf with format string A and Data items B
   /// @param UZ output string to append formatted result to
   /// @param A_format printf-style format string
   /// @param B APL value containing data items
   /// @param B_start starting index into B for data items
   /// @param funname calling function name for error messages
   static void do_snprintf(UCS_string & UZ, const UCS_string & A_format,
                   const Value * B, int B_start, const char * funname);

   /// perform an fscanf() from file
   /// @param input file or string to scan from
   /// @param format scanf-style format string
   /// @param function_number ⎕FIO subfunction number for error reporting
   static Token do_scanf(File_or_String & input, const UCS_string & format,
                         int function_number);

   /// for Date/Time Bv. return its seconds since midnight Jan 1, 1970
   /// @param B APL value containing a date/time vector
   static APL_Integer secs_epoch(const Value & B);

   /// the open files
   static std::vector<file_entry> open_files;
};
//----------------------------------------------------------------------------
#endif //  __QUAD_FIO_HH_DEFINED__

