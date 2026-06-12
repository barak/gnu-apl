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

   objdump --line-numbers --dwarf=decodedline ./apl

   and produces a more compact format soch as

     PC filename:line_number

   The typical usage is:

   objdump --line-numbers --dwarf=decodedline ./apl | sort -g

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

   enum { BUFLEN = 1000, BUFLAST = BUFLEN - 1 };
char buffer[BUFLEN];

   for (;;)
       {
          const char * s = fgets(buffer, sizeof(buffer), stdin);
          if (s == 0)   break;
          buffer[BUFLAST] = 0;   // just in case buffer is not 0-terminated.

          char filename[BUFLEN];
          int line_num;
          char addr_str[BUFLEN];
          const int count = sscanf(buffer, "%s %d %s",
                                   filename, &line_num, addr_str);

          if (3 != count)   continue;

          if (strcmp(addr_str, "0") && strncmp(addr_str, "0x", 2))   continue;

          const int addr = strtol(addr_str, 0, 16);

          printf("%8.8X %s:%u\n", addr, filename, line_num);
       }

   return 0;   // OK
}
//═══════════════════════════════════════════════════════════════════════════
