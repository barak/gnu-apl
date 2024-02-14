/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2024  Dr. Jürgen Sauermann,
    Copyright ©      2024  Paul Rockwell (Apple)

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

#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Common.hh"   // for HAVE_EXECINFO_H et al.
#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
# include <cxxabi.h>
#define EXEC(x) x
#else
#define EXEC(x)
#endif

#if defined(HAVE_LIBDWARF) && defined(HAVE_LIBDWARF_LIBDWARF_H) && \
    defined(HAVE_LIBELF)  && defined(HAVE_LIBELF_H)
# define HAVE_DWARF /* ongoing work, set to 1 if it works */ 0
#endif

#include "Backtrace.hh"

#if HAVE_DWARF
# include <libelf.h>
# include <libdwarf/libdwarf.h>
# include <libdwarf/dwarf.h>

Dwarf_Error error = 0;
Dwarf_Debug dbg;
void
init_DWARF(const char * binary_filename)
{
   get_CERR() << " Initializing DWARF for " << binary_filename << "..." << endl;

   if (const int rc = dwarf_init_path(
       binary_filename,
       0,            // char *            true_path_out_buffer
       0,            // unsigned int      true_path_bufferlen
       0,            // Dwarf_Unsigned    access
       0,            // unsigned int      groupnumber
       0,            // Dwarf_Handler     errhand
       0,            // Dwarf_Ptr         errarg
       &dbg,         // Dwarf_Debug*      dbg
       0, 0, 0,      // const char *       reserved1..3
       &error))      // Dwarf_Error*       error
      {
         get_CERR() << "init_DWARF(): rc=" << rc << ", error-" << error << endl
              << dwarf_errmsg(error) << endl;
      }
   else get_CERR() << "OK." << endl;
}


#else   // no dwarf

void init_DWARF(const char * binary_filename)
{
   cerr << "init_DWARF(): NO dwarf" << endl;
}

#endif

#define NO_PC (-1LL)

using namespace std;

Backtrace::APL_lines_status Backtrace::lines_status = LINES_not_checked;

std::vector<Backtrace::PC_src> Backtrace::pc_2_src;

static int64_t main_offset = 0;

//----------------------------------------------------------------------------
int
Backtrace::pc_cmp(const int64_t & key, const PC_src & pc_src, const void *)
{
   return key - pc_src.pc;
}
//----------------------------------------------------------------------------
const char *
Backtrace::find_src(int64_t pc)
{
   if (pc == NO_PC)   return 0;   // no pc

   if (pc_2_src.size())   // database was properly set up.
      {
        typedef Heapsort<Backtrace::PC_src> HS;
        if (const PC_src * posp = HS::search<const int64_t &>
                                            (pc,                // key
                                             pc_2_src.data(),   // array
                                             pc_2_src.size(),   // array size
                                             &pc_cmp,           // compare fun
                                             0))                // compare arg
        return posp->src_loc;   // found
      }

   (0) && cerr << "PC=" << hex << pc << dec << " not found in apl.lines" << endl;
   return 0;   // not found
}
//----------------------------------------------------------------------------
void
Backtrace::open_lines_file()
{
   if (lines_status != LINES_not_checked)   return;   // already called

   // line numbers work only if ./configure was called with CXXFLAGS set
   // to (at least) -rdynamic -gdwarf-2. Give up if this was not the case.
   //
   if (0 == strstr(cfg_CONFIGURE_ARGS, "-rdynamic") ||
       0 == strstr(cfg_CONFIGURE_ARGS, "-gdwarf-2"))
      {
        cerr << "*** useless apl.lines "
                "(no CXXFLAGS=-rdynamic -gdwarf-2)" << endl;
         lines_status = LINES_unusable;
         return;
      }
   // we are here for the first time.
   // Assume that apl.lines is outdated until proven otherwise.
   //
   lines_status = LINES_outdated;

struct stat st;
   if (stat("apl", &st))   return;   // stat apl failed

const time_t apl_time = st.st_mtime;

const int fd = open("apl.lines", O_RDONLY);
   if (fd == -1)   return;

   if (fstat(fd, &st))   // stat apl.lines failed
      {
        close(fd);
        return;
      }

const time_t apl_lines_time = st.st_mtime;

   if (apl_lines_time < apl_time)
      {
        close(fd);
        get_CERR() << endl
                   << "Cannot show line numbers, since apl.lines is older "
                      "than apl." << endl
                   << "Consider running 'make apl.lines' in directory " << endl
                   << apl_DIR__src " to fix this" << endl;
        return;
      }

   pc_2_src.reserve(100000);
char buffer[1000];
FILE * file = fdopen(fd, "r");
size_t file_lines = 0;
size_t asm_lines = 0;

   assert(file);

const char * src_line = 0;
bool new_line = false;
int64_t prev_pc = NO_PC;

   for (;;)
       {
         const char * s = fgets(buffer, sizeof(buffer) - 1, file);
         if (s == 0)   break;   // end of file.
         ++file_lines;

         buffer[sizeof(buffer) - 1] = '\0';   // just in case
         int slen = strlen(buffer);
         if (slen && buffer[slen - 1] == '\n')   buffer[--slen] = '\0';
         if (slen && buffer[slen - 1] == '\r')   buffer[--slen] = '\0';
         if (slen == 0)   continue;   // empty line

         /* the file supposedly contains only 3 types of non-empty lines:

            case 1: the (single) <main>: line,
            case 2: absolute source file path lines, and
            case 3: code lines.

            For example:

000000000014c7a9 <main>:                                   [<main> line]
main():
/home/eedjsa/apl-1.8/src/main.cc:611                       [source file path]
  14c7a9:»······55                   »··push   %rbp        [code line]
  14c7aa:»······48 89 e5             »··mov    %rsp,%rbp   [code line]
  ...

            NOTE: case 2 only works with:

            CXXFLAGS='-rdynamic -gdwarf-2' ./configure ...

            since newer g++ compiler versions strip location information
            by default.
          */

         if (strstr(s, "<main>:"))   // case 1: <main line>
            {
              const int64_t main_funct = get_main();
              const int64_t main_loc = strtoll(s, 0, 16);
              main_offset = main_funct - main_loc;

              cerr << hex << setfill(' ')
                   << "main() in apl.lines  " << setw(16) << main_loc    << endl
                   << "main() offset:      +" << setw(16) << main_offset << endl
                   << "main() start:       =" << setw(16) << main_funct  << endl
                   << dec;
            }

         if (s[0] == '/')            // case 2: source file path
            {
              src_line = strdup(strrchr(s, '/') + 1);
              new_line = true;
              continue;
            }

         if (!new_line)   continue;

         if (s[8] == ':')   // case 3: code line
            {
              ++asm_lines;
              errno = 0;
              long long pc = strtoll(s, 0, 16);   // opcode address
              if (errno)   // strtoll() failed
                 {
                   pc = NO_PC;
                 }
              else         // strtoll() succeeded
                 {
                   if (pc < prev_pc)
                      {
                         cerr << endl
                              << "in file apl.lines:" << file_lines << endl
                              << lhex << "prev_pc was: " << prev_pc << endl
                              << "pc is:       " << pc << nohex     << endl
                              << "line is:     '" << s << "'"       << endl
                              << endl;
                         assert(0 && "file apl.lines is not ordered by PC");
                         break;
                      }

                   PC_src pcs = { pc, src_line };
                   (0) && cerr << "adding " << uhex << pcs.pc << dec
                             << " aka. " << pcs.src_loc << endl;
                   pc_2_src.push_back(pcs);
                   prev_pc = pc;

                   // new_line = false;   causes some lines to not be found
                 }
              continue;
            }
       }

   fclose(file);   // also closes fd

   cerr << "total_lines in apl.lines:     " << file_lines      << endl
        << "assembler lines in apl.lines: " << asm_lines       << endl
        << "source line numbers found:    " << pc_2_src.size() << endl;
   lines_status = LINES_valid;
}
//----------------------------------------------------------------------------
void
Backtrace::show_item(int idx, char * s)
{
#ifdef HAVE_EXECINFO_H
    
// Change this to #define DISPLAY_ASM_OFFSET if you would like the asm_offset
// displayed in the backtrace the default is not to display
//
#undef  DISPLAY_ASM_OFFSET
    
int64_t abs_addr = NO_PC;
char * fun = nullptr;
long long asm_offset = 0;
   (void)asm_offset;   // avoid warning if not used
    
#ifdef __APPLE__
/*
    on macOS/Darwin, string s looks like this:

    0x00000001000a93f0 _ZN9Workspace19immediate_executionEb + 68
    ││││││││││││││││││ ││││││││││││││││││││││││││││││││││││   ││
    ││││││││││││││││││ ││││││││││││││││││││││││││││││││││││   └┴─── asm_offset
    ││││││││││││││││││ └┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴──────── fun
    └┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴───────────────────────────────────────────── abs_addr
 */

   // Find the abs_addr by skipping to the first ' 0x'

   if (char * a_a = strstr(s, " 0x") )   // the space before abs_addr
      {
          // extract abs_addr now that we found it
          *a_a = '\0';
          a_a++;
          if (char * e = strchr(a_a,' ')) {
              *e = '\0';
              fun = e + 1;
              abs_addr = strtoll(a_a, nullptr, 16);
              abs_addr -= main_offset;
          }
      }

    if (fun)
    {
        if (char * e = strchr(fun , ' '))
        {
            *e = '\0';
            e++ ;                                 // skip and look for "+ " ' '
            if (char * a_o = strstr(e,"+ "))
            {
                a_o += 2;
                asm_offset = strtoll(a_o,nullptr,10);
            }
        }
    }
    
#else /* __APPLE__ not defined,  a.k.a. other Linux, etc. platforms */

/*
    string s looks like this:
    
     ./apl(_ZN10APL_parser9nextTokenEv+0x1dc) [0x80778dc]
           │││││││││││││││││││││││││││ │││││   │││││││││
           │││││││││││││││││││││││││││ │││││   └┴┴┴┴┴┴┴┴───── abs_addr
           │││││││││││││││││││││││││││ └┴┴┴┴───────────────── asm_offset
           └┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴─────────────────────── fun
   */

   // split off abs_addr from s.
   //

   if (char * space = strchr(s, ' '))   // the space before [abs_addr]
      {
        *space = '\0';
        space += 2;
        if (char * e = strchr(space, ']'))   *e = 0;
        abs_addr = strtoll(space, 0, 16);
        abs_addr -= main_offset;
      }

   // split off function from s.
   //
   {
    char * opar = strchr(s, '(');
    if (opar)
       {
         *opar = '\0';
         fun = opar + 1;
         char * e = strchr(opar + 1, ')');
         if (e)   *e = '\0';
       }
   }

   // split off +asm_offset from fun.
   //
   if (fun)
      {
       if (char * plus = strchr(fun, '+'))
          {
            *plus++ = '\0';
            asm_offset = strtoll(plus, nullptr, 16);
          }
      }

# endif /* __APPLE__ */

const char * src_loc = find_src(abs_addr);

char obuf[200] = "@@@@";
   if (fun)
      {
       strncpy(obuf, fun, sizeof(obuf) - 1);
       obuf[sizeof(obuf) - 1] = '\0';

       size_t obuflen = sizeof(obuf) - 1;
       int status = 0;
//     cerr << "mangled fun is: " << fun << endl;
       __cxxabiv1::__cxa_demangle(fun, obuf, &obuflen, &status);
       switch(status)
          {
            case 0: // demangling succeeded
                 break;

            case -2: // not a valid name under the C++ ABI mangling rules.
                 break;

            default:
                 cerr << "__cxa_demangle() returned " << status << endl;
                 break;
          }
      }

// cerr << setw(2) << idx << ": ";

   // we normally prefer uppercase hex, but 'objcopy' and friends produce
   // lowercase hex and we follow suit as to simplify searching in their files.
   //
   cerr << "0x" << lhex << abs_addr << nohex;

// cerr << left << setw(20) << s << right << " ";

   // indent.
   //
   for (int i = -1; i < idx; ++i)   cerr << " ";

   cerr << obuf;

# ifdef DISPLAY_ASM_OFFSET
   if (asm_offset > 0)   cerr << " + " << asm_offset;
# endif

   if (src_loc)
      {
        char cc[200];
        SPRINTF(cc, "%s", src_loc);
        if (char * disc = strstr(cc, "discriminator"))   disc[-1] = '\0';
        cerr << " at " << cc;
      }
   cerr << endl;
#endif   // _APPLE_
}
//----------------------------------------------------------------------------
void
Backtrace::show_dwarf(int idx, const char * s)
{
#if HAVE_DWARF

   // loop over CUs
   //
   {
    Dwarf_Unsigned abbrev_offset  = 0;
    Dwarf_Half     address_size   = 0;
    Dwarf_Half     version_stamp  = 0;
    Dwarf_Half     offset_size    = 0;
    Dwarf_Half     extension_size = 0;
    Dwarf_Sig8     signature;
    Dwarf_Unsigned typeoffset     = 0;
    Dwarf_Unsigned next_cu_header = 0;
    Dwarf_Half     header_cu_type = 0;
    Dwarf_Bool     is_info        = true;   // inside .debug_info section
                                            // otherwise: in .debug_types
    int            res            = 0;

   for (;;)
       {
         Dwarf_Die no_die = 0;
         Dwarf_Die cu_die = 0;
         Dwarf_Unsigned cu_header_length = 0;
         memset(&signature, 0, sizeof(signature));
         Dwarf_Die in_die;
         int res = dwarf_next_cu_header_d(dbg, is_info, &cu_header_length,
                                          &version_stamp, &abbrev_offset,
                                          &address_size, &offset_size,
                                          &extension_size, &signature,
                                          &typeoffset, &next_cu_header,
                                          &header_cu_type, &error);
         if (res == DW_DLV_ERROR)   break; // needed ???
         if (res == DW_DLV_NO_ENTRY)
            {
              if (is_info)   // end of .debug_info reached
                 {
                   // Done with .debug_info, now in section .debug_types
                   is_info = false;
                   continue;
                 }
            }
         res = dwarf_siblingof_b(dbg, no_die, is_info, &cu_die, &error);
         if (res == DW_DLV_ERROR) {
            break;
        }

         if (res == DW_DLV_ERROR)   break;

         /*
            0x03 = DW_AT_name     = source file name
            0x1B = DW_AT_comp_dir = source file directory
            0x25 = DW_AT_producer = compiler and its command line flags
          */

         char * src_file = 0;
         char * src_dir  = 0;
         Dwarf_Attribute att_PC_low = 0;
         dwarf_die_text(cu_die, DW_AT_name, &src_file, &error);
         dwarf_die_text(cu_die, DW_AT_comp_dir, &src_dir,  &error);
         dwarf_attr(cu_die, DW_AT_low_pc, &att_PC_low,  &error);
         if (src_file && src_dir && att_PC_low)   // got them
            {
              Dwarf_Addr low = 0;
              dwarf_formaddr(att_PC_low, &low, &error);
              cerr << "See CU " << src_dir
                   << "/" << src_file
                   << " PC " << HEX(low) << endl;
            }
       }
   }

int64_t PC = 0;
#ifdef __APPLE__
/*
    on macOS/Darwin, string s looks like this:

    0x00000001000a93f0 _ZN9Workspace19immediate_executionEb + 68
    ││││││││││││││││││ ││││││││││││││││││││││││││││││││││││   ││
    ││││││││││││││││││ ││││││││││││││││││││││││││││││││││││   └┴─── asm_offset
    ││││││││││││││││││ └┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴──────── fun
    └┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴───────────────────────────────────────────── abs_addr
 */

   PC = strtoll(s);

#else   // not APPLE
/*
    string s looks like this:
    
     ./apl(_ZN10APL_parser9nextTokenEv+0x1dc) [0x80778dc]
           │││││││││││││││││││││││││││ │││││   │││││││││
           │││││││││││││││││││││││││││ │││││   └┴┴┴┴┴┴┴┴───── abs_addr
           │││││││││││││││││││││││││││ └┴┴┴┴───────────────── asm_offset
           └┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴─────────────────────── fun
   */
   if (const char * bracket = strrchr(s, '['))   PC = strtoll(bracket + 3, 0, 16);

#endif

   cerr << "0x" << lhex << PC << dec << "    ";
   for (int ind = 0; ind < idx; ++ind)   cerr << "  ";   // print indentation
   cerr << "xxx" << endl;

#endif   // HAVE_LIBELFIN_ELF_ELF___HH
}
//----------------------------------------------------------------------------
void
Backtrace::show(const char * file, int line)
{
   // CYGWIN, for example, has no execinfo.h and the functions declared there
   //
#ifndef HAVE_EXECINFO_H
   cerr << "Cannot show function call stack since execinfo.h seems not"
           " to exist on this OS (WINDOWs ?)." << endl;
   return;

#else

   open_lines_file();

void * buffer[200];
const int size = backtrace(buffer, sizeof(buffer)/sizeof(*buffer));

char ** strings = backtrace_symbols(buffer, size);

   cerr << endl
        << "----------------------------------------"  << endl
        << "-- Stack trace at " << file << ":" << line << endl
        << "----------------------------------------"  << endl;

   if (strings == nullptr)
      {
        cerr << "backtrace_symbols() failed. Using backtrace_symbols_fd()"
                " instead..." << endl << endl;
        // backtrace_symbols_fd(buffer, size, STDERR_FILENO);
        for (int b = size - 1; b > 0; --b)
            {
              for (int s = b + 1; s < size; ++s)   cerr << " ";
                  backtrace_symbols_fd(buffer + b, 1, STDERR_FILENO);
            }
        cerr << "========================================" << endl;
        return;
      }

   for (int i = 1; i < size - 1; ++i)   // loop over stacktrace lines
       {
         // make a copy si of strings[i] that show_item() can mess up
         //
         const char * const_si = strings[size - i - 1];
         char si[strlen(const_si) + 1];
         strcpy(si, const_si);
         si[sizeof(si) - 1] = 0;   // 0-terminate (just in case)
         show_item(i - 1, si);
       }

   cerr << "========================================" << endl;

   for (int i = 1; i < size - 1; ++i)   // loop over stacktrace lines
       {
         // make a copy si of strings[i] that show_item() can mess up
         //
         const char * si = strings[size - i - 1];
         show_dwarf(i - 1, si);
       }

   cerr << "========================================" << endl;

   // crashes at times
   free(strings);   // but not strings[x] !
   
#endif
}
//----------------------------------------------------------------------------
#ifdef HAVE_EXECINFO_H
int
Backtrace::demangle_line(char * result, size_t result_max, const char * buf)
{
std::string tmp;
   tmp.reserve(result_max + 1);
   for (const char * b = buf; *b &&  b < (buf + result_max); ++b)
       tmp += *b;
   tmp.append(0);

char * e = 0;
int status = 3;
char * p = strchr(&tmp[0], '(');
   if (p == 0)   goto error;
   else          ++p;

   e = strchr(p, '+');
   if (e == 0)   goto error;
   else *e = 0;

// cerr << "mangled fun is: " << p << endl;
   __cxxabiv1::__cxa_demangle(p, result, &result_max, &status);
   if (status)   goto error;
   return 0;

error:
   strncpy(result, buf, result_max);
   result[result_max - 1] = 0;
   return status;
}
//----------------------------------------------------------------------------
const char *
Backtrace::caller(int offset)
{
void * buffer[200];
const int size = backtrace(buffer, sizeof(buffer)/sizeof(*buffer));
char ** strings = backtrace_symbols(buffer, size);

char * demangled = static_cast<char *>(malloc(200));
   if (demangled)
       {
         *demangled = 0;
         demangle_line(demangled, 200, strings[offset]);
         return demangled;
       }
   return strings[offset];
}
//----------------------------------------------------------------------------
#endif // HAVE_EXECINFO_H
