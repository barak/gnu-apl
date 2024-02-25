/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2024-2024  Dr. Jürgen Sauermann

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

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iomanip>

using namespace std;

/*
 This program filters the output of

 objdump --section=.text --line-numbers --disassemble ./apl

   and produces a more compact format soch as

   [PC_from - PC_to] src_line

   where  [PC_from - PC_to] is a PC range in the binary file i(apl) and
   src_line is the line number in the source file that has produced the range.

   PC_to is excluding, i.e. the first address after the line. The typical
   usage is:

   objdump --section=.text --line-numbers --disassemble ./apl > apl.lines

   (in the Makefile) in directory src.
 */
//═══════════════════════════════════════════════════════════════════════════
inline bool
is_prefix(const char * buffer, const char * prefix)
{
   return !strncmp(buffer, prefix, strlen(prefix));
}
//───────────────────────────────────────────────────────────────────────────
int
main(int argc, char *argv[])
{

bool expect_Disassembly = true;
bool in_ASM = false;

uint64_t from = 0;
uint64_t to   = 0;
int src_line  = -1;

   enum { BUFLEN = 1000, BUFLAST = BUFLEN - 1 };
char buffer[BUFLEN];

   for (;;)
       {
          const char * s = fgets(buffer, sizeof(buffer), stdin);
          if (s == 0)   break;
          buffer[BUFLAST] = 0;   // just in case buffer is not 0-termonated.

          size_t last = strlen(buffer) - 1;
          if (last >= 0 && buffer[last] == '\n')   buffer[last--] = 0;
          if (last >= 0 && buffer[last] == '\r')   buffer[last--] = 0;
          // cerr << "buffer[" << (last + 1) << "]='" << buffer << "'" << endl;

          if (last == -1)   // empty line
             {
               continue;
             }

           if (expect_Disassembly)
             {
               // do not do anything before the Disassembly line,
               //
               const char * expect = "Disassembly of section .text";
               if (is_prefix(buffer, expect))   expect_Disassembly = false;
               continue;
             }

           // at this point we have a mix of assembler and non-assembler lines
           //
           if (is_prefix(buffer, "  "))   // assembler
             {
               uint64_t addr = 0;
               char colon = 0;
               const int count = sscanf(buffer, "  %lX%c", &addr, &colon);
               if ((count != 2) || (addr== 0)  || (colon != ':'))
                  {
                     cerr << endl
                          << "count: " << count << endl
                          << "addr:  " << addr  << endl
                          << "colon: " << colon << endl
                          << "Bad asm line: '"
                          << buffer << endl << endl << endl;
                     assert(0 && "Bad asm line");
                  }

               // supposedly addresses oncrease from line to line
               //
               assert(addr >= to);
               if (!in_ASM)   from = addr;   // non-asm -> asm
               to = addr + 1;
               in_ASM = true;
               continue;
             }

            if (in_ASM)   // asm → next line
               {
                 printf("%lX %d %d\n", from, int(to - from), src_line);
                 in_ASM = false;   // next line or file
                  // proceed below
               }

            if (is_prefix(buffer, "000") && buffer[last] == ':')
               {
                 // e.g. "000000000014b430 <_start>:"
                 continue;
               }

            if (buffer[0] == '/')   // file name and line number
               {
                 const char * colon = strchr(buffer, ':');
                 assert(colon);
                 src_line = strtoll(colon+1, 0, 10);
                continue;
               }

           if (!strcmp(buffer + last - 2, "():"))
              {
                // e.g. "'_start():'"
                continue;
              }

           goto bad_buffer;
       }
   return 0;   // OK

bad_buffer:
   cerr << "bad buffer: '" << buffer << "'" << endl; buffer;
   return 1;
}
//═══════════════════════════════════════════════════════════════════════════
