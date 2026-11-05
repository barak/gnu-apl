/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2026  Dr. Jürgen Sauermann,
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
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Common.hh"   // for HAVE_EXECINFO_H et al.
#include "Sys.hh"

#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
# include <cxxabi.h>
# ifdef __ELF__   // function names and line numbers from the ELF files
#  define HAVE_ELF_SYMBOLS 1
#  include <dlfcn.h>
#  include <link.h>       // for ElfW()
#  include <sys/mman.h>
#  include <algorithm>
#  include <map>
# endif
#define EXEC(x) x
#else
#define EXEC(x)
#endif

#include "Backtrace.hh"

#define NO_PC (-1LL)

using namespace std;

//════════════════════════════════════════════════════════════════════════════
void
Backtrace::show_signal_safe(int extra_fd, bool to_stderr)
{
#ifndef HAVE_EXECINFO_H
const char msg[] = "Cannot show function call stack: no execinfo.h\n";
   if (to_stderr && write(STDERR_FILENO, msg, sizeof(msg) - 1)) { /* nothing to do */ }
   if (extra_fd >= 0 && write(extra_fd, msg, sizeof(msg) - 1)) { }
   return;

#else

   // backtrace() itself may allocate (unwind tables etc.) on its very
   // first-ever call; main() calls it once during normal startup (see
   // main.cc) specifically so that first allocation never happens here.
   //
void * buffer[200];
const int size = backtrace(buffer, sizeof(buffer)/sizeof(*buffer));

const char banner[] =
      "-- raw backtrace (addresses only; not demangled -- see\n"
      "   Backtrace::show_signal_safe() for why) --\n";
   // return value deliberately ignored: this runs from a signal handler
   // (or right before one, see main.cc's warm-up call) with nothing
   // sensible to do about a failed/partial write while already crashing.
   if (to_stderr && write(STDERR_FILENO, banner, sizeof(banner) - 1)) { /* nothing to do */ }
   if (extra_fd >= 0 && write(extra_fd, banner, sizeof(banner) - 1)) { }

   // backtrace_symbols_fd(), unlike backtrace_symbols(), does not call
   // malloc() -- it is the one part of this API glibc itself documents
   // as safe to call from a signal handler.
   //
   if (to_stderr)   backtrace_symbols_fd(buffer, size, STDERR_FILENO);
   if (extra_fd >= 0)   backtrace_symbols_fd(buffer, size, extra_fd);

const char footer[] = "====================================================\n";
   if (to_stderr && write(STDERR_FILENO, footer, sizeof(footer) - 1)) { /* nothing to do */ }
   if (extra_fd >= 0 && write(extra_fd, footer, sizeof(footer) - 1)) { }

#endif
}
//────────────────────────────────────────────────────────────────────────────
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

void * buffer[200];
const int size = backtrace(buffer, sizeof(buffer)/sizeof(*buffer));

char ** strings = backtrace_symbols(buffer, size);

   cerr << endl
        << "----------------------------------------"  << endl
        << "-- Stack trace at " << file << ":" << line << endl
        << "----------------------------------------"  << endl;

   if (strings == 0)
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

   // the function names and source locations from the binaries (also for
   // the functions that backtrace_symbols() does not know).
   //
std::vector<Frame_info> frames;
   resolve_frames(buffer, size, frames);

   // in a crash: skip the frames of the signal handler (and of this
   // function), i.e. the frames newer than the one that crashed.
   //
int end = size - 1;
   if (signal_PC)
      {
        loop(b, size)
            {
              if (buffer[b] == signal_PC)   { end = size - b;   break; }
            }
      }

   for (int item = 1; item < end; ++item)   // loop over stacktrace lines
       {
         /* make a copy mutable_si of strings[item] which show_item() may mess
            up.
            variable item is the number passed to show_item() and counts
            from oldest to latest, while 'strings' runs from latest to
            oldest. We want the lines to be displayed from oldest to latest.
          */
         const char * const_si = strings[size - item - 1];
         std::vector<char> mutable_si(const_si, const_si + strlen(const_si) + 1);
         show_item(item - 1, mutable_si.data(), frames[size - item - 1]);
       }

   cerr << "========================================" << endl;

   // crashes at times
   free(strings);   // but not strings[x] !

#endif
}
//────────────────────────────────────────────────────────────────────────────
#if HAVE_ELF_SYMBOLS
namespace
{
/// the function symbols in the symbol tables (.symtab and .dynsym) of one
/// ELF file (the interpreter, libapl.so, or a shared library)
struct ELF_symbols
{
   /// one function symbol
   struct Sym
      {
        uint64_t addr;      ///< the start address (in the ELF file)
        uint64_t size;      ///< the size (0 if unknown)
        std::string name;   ///< the (mangled) name

        /// compare by address (for sorting)
        bool operator <(const Sym & other) const
           { return addr < other.addr; }
      };

   /// read the symbols of ELF file \b path
   ELF_symbols(const char * path);

   /// return the symbol that contains \b addr, or 0 if none
   const Sym * find(uint64_t addr) const;

   /// return the (cached) symbols of ELF file \b path
   static const ELF_symbols & get(const std::string & path);

   /// true if the file is position-independent (ET_DYN), i.e. if its
   /// addresses are relative to the address where it was loaded
   bool is_PIE;

   /// the function symbols, sorted by address
   std::vector<Sym> syms;
};
//────────────────────────────────────────────────────────────────────────────
ELF_symbols::ELF_symbols(const char * path)
   : is_PIE(false)
{
const int fd = open(path, O_RDONLY);
   if (fd == -1)   return;

struct stat st;
   if (fstat(fd, &st) || size_t(st.st_size) < sizeof(ElfW(Ehdr)))
      {
        close(fd);
        return;
      }

const size_t file_size = st.st_size;
void * map = mmap(0, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
   close(fd);
   if (map == MAP_FAILED)   return;

const char * base = reinterpret_cast<const char *>(map);
const ElfW(Ehdr) & ehdr = *reinterpret_cast<const ElfW(Ehdr) *>(base);

   // check that the file is an ELF file of the same class (32 or 64 bit)
   // as the interpreter, and that its section headers are within the file
   //
const bool ok = memcmp(ehdr.e_ident, ELFMAG, SELFMAG) == 0 &&
                ehdr.e_ident[EI_CLASS] == (sizeof(void *) == 8 ? ELFCLASS64
                                                               : ELFCLASS32) &&
                ehdr.e_shentsize == sizeof(ElfW(Shdr)) &&
                ehdr.e_shoff < file_size &&
                ehdr.e_shnum <= (file_size - ehdr.e_shoff)
                                / sizeof(ElfW(Shdr));
   if (ok)
      {
        is_PIE = ehdr.e_type == ET_DYN;
        const ElfW(Shdr) * sections =
              reinterpret_cast<const ElfW(Shdr) *>(base + ehdr.e_shoff);
        loop(sec, ehdr.e_shnum)
           {
             const ElfW(Shdr) & symtab = sections[sec];
             if (symtab.sh_type != SHT_SYMTAB &&
                 symtab.sh_type != SHT_DYNSYM)            continue;
             if (symtab.sh_link >= ehdr.e_shnum)          continue;
             if (symtab.sh_offset > file_size ||
                 symtab.sh_size > file_size - symtab.sh_offset)   continue;

             const ElfW(Shdr) & strtab = sections[symtab.sh_link];
             if (strtab.sh_offset > file_size ||
                 strtab.sh_size > file_size - strtab.sh_offset)   continue;

             const ElfW(Sym) * elf_syms =
                   reinterpret_cast<const ElfW(Sym) *>(base + symtab.sh_offset);
             const char * names = base + strtab.sh_offset;
             const size_t count = symtab.sh_size / sizeof(ElfW(Sym));
             loop(e, count)
                {
                  const ElfW(Sym) & es = elf_syms[e];
                  const int type = ELF64_ST_TYPE(es.st_info);
                  if (type != STT_FUNC && type != STT_GNU_IFUNC)   continue;
                  if (es.st_value == 0 || es.st_shndx == SHN_UNDEF)   continue;
                  if (es.st_name == 0 || es.st_name >= strtab.sh_size)
                     continue;

                  // the name must end within the string table
                  const char * name = names + es.st_name;
                  const size_t max_len = strtab.sh_size - es.st_name;
                  const size_t len = strnlen(name, max_len);
                  if (len == max_len)   continue;

                  const Sym sym = { es.st_value, es.st_size,
                                    std::string(name, len) };
                  syms.push_back(sym);
                }
           }

        // .symtab and .dynsym contain the same (exported) functions
        std::stable_sort(syms.begin(), syms.end());
        syms.erase(std::unique(syms.begin(), syms.end(),
                               [](const Sym & a, const Sym & b)
                                 { return a.addr == b.addr; }),
                   syms.end());
      }

   munmap(map, file_size);
}
//────────────────────────────────────────────────────────────────────────────
const ELF_symbols::Sym *
ELF_symbols::find(uint64_t addr) const
{
const Sym key = { addr, 0, std::string() };
auto next = std::upper_bound(syms.begin(), syms.end(), key);
   if (next == syms.begin())   return 0;   // addr is before the first symbol

const Sym & sym = *--next;
   if (sym.size && addr >= sym.addr + sym.size)   return 0;   // in a gap
   return &sym;
}
//────────────────────────────────────────────────────────────────────────────
const ELF_symbols &
ELF_symbols::get(const std::string & path)
{
static std::map<std::string, ELF_symbols *> cache;
ELF_symbols * & elf = cache[path];
   if (elf == 0)   elf = new ELF_symbols(path.c_str());
   return *elf;
}
}   // namespace
//────────────────────────────────────────────────────────────────────────────
/// demangle \b name (if it is a mangled C++ name)
static std::string
demangle(const std::string & name)
{
int status = 0;
char * dm = __cxxabiv1::__cxa_demangle(name.c_str(), 0, 0, &status);
   if (dm == 0)   return name;
const std::string ret(dm);
   free(dm);
   return ret;
}
#endif // HAVE_ELF_SYMBOLS
//────────────────────────────────────────────────────────────────────────────
const void * Backtrace::signal_PC = 0;
//────────────────────────────────────────────────────────────────────────────
void
Backtrace::resolve_frames(void * const * buffer, int size,
                          std::vector<Frame_info> & frames)
{
   frames.clear();
   frames.resize(size);

#if HAVE_ELF_SYMBOLS
   // the frames, grouped by the file (interpreter, libraries) they are in
std::map<std::string, std::vector<std::pair<int, uint64_t>>> per_file;

   loop(f, size)
      {
        Dl_info info;
        if (!dladdr(buffer[f], &info))   continue;
        if (!info.dli_fname || !info.dli_fbase)   continue;

        // the interpreter itself has the name it was started with (argv[0],
        // e.g. 'apl' or './apl'), the libraries have absolute paths.
        //
        std::string path = info.dli_fname;
# ifdef __linux__
        if (path[0] != '/')   // the real path (also for addr2line below)
           {
             char exe[PATH_MAX + 1];
             const ssize_t len = readlink("/proc/self/exe", exe, PATH_MAX);
             if (len > 0)   path.assign(exe, len);
           }
# endif

        const ELF_symbols & elf = ELF_symbols::get(path);

        // the address in the file. buffer[f] is a return address, i.e. it
        // points behind the call (which may be the end of the function).
        //
        uint64_t addr = reinterpret_cast<uint64_t>(buffer[f]);
        if (elf.is_PIE)   addr -= reinterpret_cast<uint64_t>(info.dli_fbase);
        if (addr && buffer[f] != signal_PC)   --addr;

        if (const ELF_symbols::Sym * sym = elf.find(addr))
           frames[f].fun = demangle(sym->name);
        else if (info.dli_sname)
           frames[f].fun = demangle(info.dli_sname);

        per_file[path].push_back(std::pair<int, uint64_t>(f, addr));
      }

   // the source locations
   for (const auto & file : per_file)
       run_addr2line(file.first, file.second, frames);
#endif // HAVE_ELF_SYMBOLS
}
//────────────────────────────────────────────────────────────────────────────
void
Backtrace::run_addr2line(const std::string & file,
                         const std::vector<std::pair<int, uint64_t>> & addrs,
                         std::vector<Frame_info> & frames)
{
#if HAVE_ELF_SYMBOLS
   // addr2line reads the line numbers (DWARF, from the -g option of g++)
   // directly from the binary. Without addr2line (binutils) only the function names are
   // shown.
   //
   // DEBUGINFOD_URLS= : never download debug information from the
   // internet (slow, and a backtrace should not access the network).
   //
std::string cmd = "DEBUGINFOD_URLS= addr2line -C -f -i -p -e '";
   for (const char cc : file)
       {
         if (cc == '\'')   cmd += "'\\''";   // quote ' in the file name
         else              cmd += cc;
       }
   cmd += "'";

   for (const auto & fa : addrs)
       {
         char hex_addr[40];
         snprintf(hex_addr, sizeof(hex_addr), " 0x%llx",
                  static_cast<unsigned long long>(fa.second));
         cmd += hex_addr;
       }
   cmd += " 2>/dev/null";

FILE * pipe = popen(cmd.c_str(), "r");
   if (pipe == 0)   return;

   // with -p, the output for one address is one line "fun at file:line",
   // followed by one line " (inlined by) fun at file:line" for every
   // function into which fun was inlined.
   //
std::vector<std::pair<std::string, std::string>> entries;   // fun, file:line
int idx = -1;   // index into addrs

auto flush = [&]()
   {
     if (idx < 0 || idx >= int(addrs.size()) || entries.empty())   return;
     Frame_info & frame = frames[addrs[idx].first];

     // entries is innermost first; the last one is the function of the
     // frame. addr2line reports nested scopes (blocks) of the same function
     // like inlined functions; merge them (keeping the innermost location).
     //
     for (size_t e = 1; e < entries.size();)
         {
           if (entries[e].first == entries[e - 1].first)
              entries.erase(entries.begin() + e);
           else
              ++e;
         }

     const auto & outer = entries.back();
     if (outer.first != "??")   frame.fun = outer.first;
     if (outer.second.size())   frame.src_loc = outer.second;
     for (int e = int(entries.size()) - 2; e >= 0; --e)
         {
           std::string inl = entries[e].first;
           if (entries[e].second.size())   inl += " at " + entries[e].second;
           frame.inlined.push_back(inl);
         }
     entries.clear();
   };

char line[2000];
   while (fgets(line, sizeof(line), pipe))
      {
        std::string str(line);
        while (str.size() && (str.back() == '\n' || str.back() == '\r'))
           str.pop_back();

        const char * inlined_by = " (inlined by) ";
        if (str.compare(0, strlen(inlined_by), inlined_by) == 0)
           str.erase(0, strlen(inlined_by));
        else   // the next address
           {
             flush();
             ++idx;
           }

        // remove " (discriminator N)"
        const size_t disc = str.find(" (discriminator");
        if (disc != std::string::npos)   str.erase(disc);

        if (str.compare(0, 2, "??") == 0)   str = "??";   // unknown

        // split "fun at /path/file:line" into fun and file:line
        std::string fun = str;
        std::string loc;
        const size_t at = str.rfind(" at ");
        if (at != std::string::npos)
           {
             fun = str.substr(0, at);
             loc = str.substr(at + 4);
             const size_t slash = loc.rfind('/');
             if (slash != std::string::npos)   loc.erase(0, slash + 1);
             if (loc.compare(0, 2, "??") == 0 || loc[0] == ':')   loc.clear();
             if (loc.size() > 2 && loc.compare(loc.size() - 2, 2, ":?") == 0)
                loc.erase(loc.size() - 2);   // file known, line unknown
           }
        entries.push_back(std::make_pair(fun, loc));
      }
   flush();
   pclose(pipe);
#endif // HAVE_ELF_SYMBOLS
}
//────────────────────────────────────────────────────────────────────────────
void
Backtrace::show_item(int idx, char * s, const Frame_info & frame)
{
#ifdef HAVE_EXECINFO_H

// Change this to #define DISPLAY_ASM_OFFSET if you would like the asm_offset
// displayed in the backtrace the default is not to display
//
#undef  DISPLAY_ASM_OFFSET

int64_t abs_addr = NO_PC;
char * fun = 0;
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
              abs_addr = strtoll(a_a, 0, 16);
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
                asm_offset = strtoll(a_o, 0, 10);
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
            asm_offset = strtoll(plus, 0, 16);
          }
      }

# endif /* __APPLE__ */

char obuf[200] = "@@@@";
   if (fun)
      {
       strncpy(obuf, fun, sizeof(obuf) - 1);
       obuf[sizeof(obuf) - 1] = '\0';

       int status = 0;
//     cerr << "mangled fun is: " << fun << endl;
       // __cxa_demangle's ABI contract requires the output buffer to have
       // come from malloc() -- it may realloc() it when undersized, and
       // realloc() on a stack array (obuf) is UB (typically a "realloc():
       // invalid pointer" abort, or worse). Any demangled name over 199
       // chars hits this, which is routine for this codebase's templated/
       // namespaced symbols (Heapsort<...>::sort, std::__...). Let
       // __cxa_demangle allocate instead, then copy into obuf (used below
       // and sized for the common case) and free the malloc'd result.
       char * dm = __cxxabiv1::__cxa_demangle(fun, 0, 0, &status);
       switch(status)
          {
            case 0: // demangling succeeded
                 strncpy(obuf, dm, sizeof(obuf) - 1);
                 obuf[sizeof(obuf) - 1] = '\0';
                 break;

            case -2: // not a valid name under the C++ ABI mangling rules.
                 break;

            default:
                 cerr << "__cxa_demangle() returned " << status << endl;
                 break;
          }
       free(dm);
      }

   // prefer the names found in the binaries: backtrace_symbols() only
   // knows the exported functions.
   //
   if (frame.fun.size())
      {
        strncpy(obuf, frame.fun.c_str(), sizeof(obuf) - 1);
        obuf[sizeof(obuf) - 1] = '\0';
      }

// cerr << setw(2) << idx << ": ";

   // we normally prefer uppercase hex, but 'objcopy' and friends produce
   // lowercase hex and we follow suit as to simplify searching in their files.
   //
   cerr << "0x" << lhex << abs_addr << reset_format;

// cerr << left << setw(20) << s << right << " ";

   // indent.
   //
   for (int i = -1; i < idx; ++i)   cerr << " ";

   cerr << obuf;

# ifdef DISPLAY_ASM_OFFSET
   if (asm_offset > 0)   cerr << " + " << asm_offset;
# endif

   if (frame.src_loc.size())   cerr << " at " << frame.src_loc;
   cerr << endl;

   for (const std::string & inl : frame.inlined)
       {
         cerr << "                ";
         for (int i = -1; i < idx; ++i)   cerr << " ";
         cerr << "(inlined: " << inl << ")" << endl;
       }
#endif   // _APPLE_
}
//────────────────────────────────────────────────────────────────────────────
#ifdef HAVE_EXECINFO_H
const char *
Backtrace::caller(int offset)
{
void * buffer[200];
const int size = backtrace(buffer, sizeof(buffer)/sizeof(*buffer));
char ** strings = backtrace_symbols(buffer, size);

   // Bugs9 #12 (Blake McBride): strings[offset] was read without checking
   // offset against size (callers pass a literal 3, so a shallower stack
   // read out of bounds) nor against backtrace_symbols() returning 0 (its
   // documented allocation-failure return).
   if (strings == 0 || offset < 0 || offset >= size)
      {
        free(strings);
        return "???";
      }

   // demangled is deliberately leaked: this is a one-shot diagnostic path
   // (only reached under LOG_error_throw-style logging), and its callers
   // (UCS_string.cc) use the returned string once, immediately, in a
   // stream expression with no way to free it afterwards.
char * demangled = static_cast<char *>(malloc(1024));
   if (demangled)
       {
         *demangled = 0;
         demangle_line(demangled, 1024, strings[offset]);
         free(strings);
         return demangled;
       }

static char fallback[256];
   SPRINTF(fallback, "%.255s", strings[offset]);
   free(strings);
   return fallback;
}
//────────────────────────────────────────────────────────────────────────────
int
Backtrace::demangle_line(char * result, size_t result_max, const char * buf)
{
std::string tmp;
   tmp.reserve(result_max + 1);
   for (const char * b = buf; *b &&  b < (buf + result_max); ++b)
       tmp += *b;
   tmp += char(0);

char * e = 0;
int status = 3;
char * p = strchr(&tmp[0], '(');
   if (p == 0)   goto error;
   else          ++p;

   e = strchr(p, '+');
   if (e == 0)   goto error;
   else *e = 0;

// cerr << "mangled fun is: " << p << endl;
   {
   size_t len = 0;
   char * dm = __cxxabiv1::__cxa_demangle(p, NULL, &len, &status);
   if (!dm || status)   goto error;
   strncpy(result, dm, result_max - 1);
   result[result_max - 1] = 0;
   free(dm);
   }
   return 0;

error:
   strncpy(result, buf, result_max);
   result[result_max - 1] = 0;
   return status;
}
//════════════════════════════════════════════════════════════════════════════
#endif // HAVE_EXECINFO_H
