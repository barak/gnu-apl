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

#include <unistd.h>
#include <signal.h>

#include "config.h"   // for HAVE_MMAN_H

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

#if HAVE_MMAN_H

#  include <sys/mman.h>

   /// @param fd open file descriptor to map
   /// @param len number of bytes to map
   static inline const uint8_t * mmap(int fd, int len)
      {
      const void * vp = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
         if (vp == MAP_FAILED)   return 0;
         return reinterpret_cast<const uint8_t *>(vp);
      }

   /// @param data pointer returned by mmap()
   /// @param len number of bytes originally mapped
   static inline void munmap(const uint8_t * data, int len)
      { munmap(data, len); }

#else // ! HAVE_MMAN_H =======================================================

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

#endif // ! MINGW_SRC

   /// invasive memory test
   /// @param base start address of the memory region to probe
   /// @param blocks number of 64-bit blocks to probe
   /// @param verbosity level of diagnostic output
   static int64_t probe_memory(uint64_t * base, uint64_t blocks, int verbosity);
};
//────────────────────────────────────────────────────────────────────────────
/// open a pipe for reading or writing
/// @param command shell command string to execute
/// @param mode open mode passed to popen() (e.g. "r" or "w")
inline FILE * sys_popen(const char * command, const char * mode)
{
FILE * file = popen(command, mode);

#if ! MINGW_SRC
   signal(SIGCHLD, SIG_DFL);
#endif // ! MINGW_SRC

   return file;
}
//────────────────────────────────────────────────────────────────────────────
/// close a pipe opened with sys_popen()
/// @param stream FILE pointer returned by sys_popen()
inline int sys_pclose(FILE *stream)
{
const int ret = pclose(stream);

#if ! MINGW_SRC
   signal(SIGCHLD, SIG_IGN);
#endif // ! MINGW_SRC

   return ret;
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

#endif // SYS_HH_DEFINED

