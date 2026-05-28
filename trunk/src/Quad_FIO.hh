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

   /// return the open FILE * for (APL integer value) \b handle
   /// @param handle APL value holding the integer file handle
   static FILE * get_FILE(const Value & handle)
      { return get_FILE(handle.get_cscalar().get_near_int()); }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(Value_P A, Value_P X, Value_P B) const;

   /// overloaded Function::eval_B().
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_AXB().
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// close all open files
   static void clear();

   /// close a file descriptor (and its FILE * if any)
   static int close_handle(int handle);

   /// fork(), execve(), and return a connection to fd 3 of forked process
   /// @param B command string to execute in the forked process
   /// @param envp environment variables for the forked process
   static int do_FIO_57(const UCS_string & B, char * const * envp);

   /// get one Unicode from file
   /// @param file open file to read from
   /// @param fget_count running count of bytes consumed
   static Unicode fget_utf8(FILE * file, ShapeItem & fget_count);

   /// cycle counter at start of a benchmark (⎕FIO[-1])
   static uint64_t benchmark_cycles_from;

   static Quad_FIO  fun;   ///< Built-in function.

protected:
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
          fe_errno(0),
          fe_fd(fd),
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

        FILE * fe_FILE;        ///< FILE * returned by fopen()
        int    fe_errno;       ///< errno for last operation
        int    fe_fd;          ///< file desriptor == fileno(file)
        bool   fe_may_read;    ///< file open for reading
        bool   fe_may_write;   ///< file open for writing
        UTF8_string path;      ///< filename
      };

   /// overloaded Function::get_fun_valence()
   virtual int get_fun_valence() const   { return 2; }

   /// overloaded Function::get_oper_valence().
   virtual int get_oper_valence() const   { return 1; }

   /// overloaded Function::is_operator()
   virtual bool is_operator() const   { return true; }

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

   /// return the open file descriptor for (APL integer) \b handle
   /// @param value APL value holding the integer file handle
   static int get_fd(const Value & value)
       {
         const file_entry & fe = get_file_entry(value);   // may throw DOMAIN ERROR
         return fe.fe_fd;
       }

   /// return the open file for (APL integer) \b handle
   /// @param handle APL value holding the integer file handle
   static file_entry & get_file_entry(const Value & handle)
      { return get_file_entry(handle.get_cscalar().get_near_int()); }

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

   /// list all ⎕IO functions to \b out
   Token list_functions(ostream & out) const;

   /// helper function to print the subfunction syntax mapping
   virtual void print_map_syntax(ostream & out,
                                 const function_info & info) const;

   /// fprintf A to file \b out
   /// @param out file to write formatted output to
   /// @param A APL value containing format string and data
   static Token do_fprintf(FILE * out, Value_P A);

   /// perform an fscanf() from file
   /// @param input file or string to scan from
   /// @param format scanf-style format string
   /// @param function_number ⎕FIO subfunction number for error reporting
   static Token do_scanf(File_or_String & input, const UCS_string & format,
                         int function_number);

   /// snprintf with format string A and Data items B
   /// @param UZ output string to append formatted result to
   /// @param A_format printf-style format string
   /// @param B APL value containing data items
   /// @param B_start starting index into B for data items
   /// @param funname calling function name for error messages
   static void do_snprintf(UCS_string & UZ, const UCS_string & A_format,
                   const Value * B, int B_start, const char * funname);

   /// eval_AB case -3: read probe A and clear it
   static Token eval_AB___3(Value_P A);

   /// eval_ALXB case -1: benchmark dyadic LO with arguments A and B
   static Token eval_ALXB___1(Value_P A, Token & LO, Value_P B);

   /// eval_AXB case 13: fseek(Bh, Ai, SEEK_SET)
   static Token eval_AXB__13(Value_P A, Value_P B);

   /// eval_AXB case 14: fseek(Bh, Ai, SEEK_CUR)
   static Token eval_AXB__14(Value_P A, Value_P B);

   /// eval_AXB case 15: fseek(Bh, Ai, SEEK_END)
   static Token eval_AXB__15(Value_P A, Value_P B);

   /// eval_AXB case 20: mkdir(Bc, Ai)
   static Token eval_AXB__20(Value_P A, Value_P B);

   /// eval_AXB cases 202+203: set parallel threshold
   static Token eval_AXB__202(Value_P A, Value_P B, APL_Integer function_number);

   /// eval_AXB case 22: fprintf(Bh, A)
   static Token eval_AXB__22(Value_P A, Value_P B);

   /// eval_AXB case 23: fwrite(Ac, 1, ⍴Ac, Bh) Unicode
   static Token eval_AXB__23(Value_P A, Value_P B);

   /// eval_AXB case 24: popen(Bs, As)
   static Token eval_AXB__24(Value_P A, Value_P B);

   /// eval_AXB case 27: rename(As, Bs)
   static Token eval_AXB__27(Value_P A, Value_P B);

   /// eval_AXB case 3: fopen(Bs, As)
   static Token eval_AXB__3(Value_P A, Value_P B);

   /// eval_AXB case 31: access(As, Bp)
   static Token eval_AXB__31(Value_P A, Value_P B);

   /// eval_AXB case 33: bind(Bh, Aa)
   static Token eval_AXB__33(Value_P A, Value_P B);

   /// eval_AXB case 34: listen(Bh, Ai)
   static Token eval_AXB__34(Value_P A, Value_P B);

   /// eval_AXB case 36: connect(Bh, Aa)
   static Token eval_AXB__36(Value_P A, Value_P B);

   /// eval_AXB case 37: recv(Bh, Zi, Ai, 0)
   static Token eval_AXB__37(Value_P A, Value_P B);

   /// eval_AXB case 38: send(Bh, Ai, ⍴Ai, 0)
   static Token eval_AXB__38(Value_P A, Value_P B);

   /// eval_AXB case 39: send(Bh, Ac, ⍴Ac, 0) Unicode
   static Token eval_AXB__39(Value_P A, Value_P B);

   /// eval_AXB case 41: read(Bh, Zi, Ai)
   static Token eval_AXB__41(Value_P A, Value_P B);

   /// eval_AXB case 42: write(Bh, Ai, ⍴Ai)
   static Token eval_AXB__42(Value_P A, Value_P B);

   /// eval_AXB case 43: write(Bh, Ac, ⍴Ac) Unicode
   static Token eval_AXB__43(Value_P A, Value_P B);

   /// eval_AXB case 46: getsockopt(Bh, ...)
   static Token eval_AXB__46(Value_P A, Value_P B);

   /// eval_AXB case 47: setsockopt(Bh, ...)
   static Token eval_AXB__47(Value_P A, Value_P B);

   /// eval_AXB case 48: fscanf(Bh, A_format)
   static Token eval_AXB__48(Value_P A, Value_P B);

   /// eval_AXB case 55: sscanf(Bs, Af)
   static Token eval_AXB__55(Value_P A, Value_P B);

   /// eval_AXB case 56: write nested lines As to file Bs
   static Token eval_AXB__56(Value_P A, Value_P B);

   /// eval_AXB case 58: snprintf(Af, B...)
   static Token eval_AXB__58(Value_P A, Value_P B);

   /// eval_AXB case 59: fcntl(Bh, Ai...)
   static Token eval_AXB__59(Value_P A, Value_P B);

   /// eval_AXB case 6: fread(Zi, 1, Ai, Bh)
   static Token eval_AXB__6(Value_P A, Value_P B);

   /// eval_AXB case 60: random value(s)
   static Token eval_AXB__60(Value_P A, Value_P B);

   /// eval_AXB case 7: fwrite(Ai, 1, ⍴Ai, Bh)
   static Token eval_AXB__7(Value_P A, Value_P B);

   /// eval_AXB case 8: fgets(Zi, Ai, Bh)
   static Token eval_AXB__8(Value_P A, Value_P B);

   /// eval_B case 0: list of open file descriptors
   static Token eval_B__0();

   /// eval_B case 30: getcwd()
   static Token eval_B__30(Value_P B);

   /// eval_B case -14: print stacks
   static Token eval_B___14();

   /// eval_B case -15: print performance IDs and names
   static Token eval_B___15();

   /// eval_B case -18: memory test
   static Token eval_B___18(Value_P B);

   /// eval_B case -2: return CPU frequency
   static Token eval_B___2();

   /// eval_B case -5: return ⎕AV of IBM APL2
   static Token eval_B___5();

   /// eval_B case -6: reset SIGSEGV handler and throw segfault
   static Token eval_B___6();

   /// eval_B case -7: throw a segfault (keep handler)
   static Token eval_B___7();

   /// eval_B case -8: screen width
   static Token eval_B___8();

   /// eval_B case -9: screen height
   static Token eval_B___9();

   /// eval_LXB case -1: benchmark monadic LO with argument B
   static Token eval_LXB___1(Token & LO, Value_P B);

   /// eval_XB case 16: fflush(Bh)
   static Token eval_XB__16(Value_P B);

   /// eval_XB case 17: fsync(Bh)
   static Token eval_XB__17(Value_P B);

   /// eval_XB case 18: fstat(Bh)
   static Token eval_XB__18(Value_P B);

   /// eval_XB case 19: unlink(Bc)
   static Token eval_XB__19(Value_P B);

   /// eval_XB case 2: strerror(B)
   static Token eval_XB__2(Value_P B);

   /// eval_XB case 20: mkdir(Bc, 0777)
   static Token eval_XB__20(Value_P B);

   /// eval_XB cases 200+201: clear/get statistics
   static Token eval_XB__200(Value_P B, APL_Integer function_number);

   /// eval_XB cases 202+203: get parallel threshold
   static Token eval_XB__202(Value_P B, APL_Integer function_number);

   /// eval_XB case 21: rmdir(Bc)
   static Token eval_XB__21(Value_P B);

   /// eval_XB case 24: popen(Bs, "r")
   static Token eval_XB__24(Value_P B);

   /// eval_XB case 25: pclose(Bh)
   static Token eval_XB__25(Value_P B);

   /// eval_XB case 26: read entire file as byte vector
   static Token eval_XB__26(Value_P B);

   /// eval_XB cases 28+29: read directory / file names
   static Token eval_XB__28(Value_P B, APL_Integer function_number);

   /// eval_XB case 3: fopen(Bs, "r")
   static Token eval_XB__3(Value_P B);

   /// eval_XB case 32: socket(...)
   static Token eval_XB__32(Value_P B);

   /// eval_XB case 34: listen(Bh, 10)
   static Token eval_XB__34(Value_P B);

   /// eval_XB case 35: accept(Bh)
   static Token eval_XB__35(Value_P B);

   /// eval_XB case 37: recv(Bh, Zi, SMALL_BUF, 0)
   static Token eval_XB__37(Value_P B);

   /// eval_XB case 4: fclose(Bh)
   static Token eval_XB__4(Value_P B);

   /// eval_XB case 40: select(...)
   static Token eval_XB__40(Value_P B);

   /// eval_XB case 41: read(Bh, Zi, SMALL_BUF)
   static Token eval_XB__41(Value_P B);

   /// eval_XB case 44: getsockname(Bh)
   static Token eval_XB__44(Value_P B);

   /// eval_XB case 45: getpeername(Bh)
   static Token eval_XB__45(Value_P B);

   /// eval_XB case 49: read entire file as nested lines
   static Token eval_XB__49(Value_P B);

   /// eval_XB case 50: gettimeofday
   static Token eval_XB__50(Value_P B);

   /// eval_XB case 51: mktime
   static Token eval_XB__51(Value_P B);

   /// eval_XB cases 52+53: localtime / gmtime
   static Token eval_XB__52(Value_P B, APL_Integer function_number);

   /// eval_XB case 54: chdir(Bs)
   static Token eval_XB__54(Value_P B);

   /// eval_XB case 57: fork() + execve()
   static Token eval_XB__57(Value_P B);

   /// eval_XB case 6: fread(Zi, 1, SMALL_BUF, Bh)
   static Token eval_XB__6(Value_P B);

   /// eval_XB case 60: random value
   static Token eval_XB__60(Value_P B);

   /// eval_XB case 8: fgets(Zi, SMALL_BUF, Bh)
   static Token eval_XB__8(Value_P B);

   /// convert bits set in \b fds to an APL integer vector
   /// @param fds file-descriptor bit set from select()
   /// @param max_fd highest file descriptor to inspect
   static Value_P fds_to_val(fd_set * fds, int max_fd);

   /// return the open FILE * for (APL integer) \b handle
   /// @param handle integer file handle index
   static FILE * get_FILE(int handle);

   /// return the open file for (APL integer) \b handle
   /// @param handle integer file handle index
   static file_entry & get_file_entry(int handle);

   /// return one or more random values
   /// @param mode distribution/type selector for random generation
   /// @param len number of random values to generate
   static Value_P get_random(APL_Integer mode, APL_Integer len);

   /// append ASCII-buffer \b buffer to dest, inserting thousands' separators.
   /// Note: \b buffer may be modified.
   /// @param dest destination string to append to
   /// @param buffer ASCII numeric buffer (may be modified in place)
   /// @param flt true if the number is floating-point
   static void group_thousands(UCS_string & dest, char * buffer, bool flt);

   /// for Date/Time Bv. return its seconds since midnight Jan 1, 1970
   /// @param B APL value containing a date/time vector
   static APL_Integer secs_epoch(const Value & B);

   /// the open files
   static std::vector<file_entry> open_files;

   /// a mapping between function names and function numbers
   static const FunctionGroup::function_info subfunction_infos[];
};
//----------------------------------------------------------------------------
#endif //  __QUAD_FIO_HH_DEFINED__

