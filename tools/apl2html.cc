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

// This program converts a GNU APL .apl file to a .apl.html file (or back)

#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

using namespace std;

//-----------------------------------------------------------------------------
int usage(const char * prog)
{
const char * prog1 = strrchr(prog, '/');
   if (prog1)   ++prog1;
   else         prog1 = prog;

   cerr <<
"usage: " << prog1 << " [options] file\n"
"    options: \n"
"    -h, --help           print this help\n"
"    -a author            set meta tag author\n"
"    -c copyright         set meta tag copyright (default: author)\n"
"    -d description       set meta tag description\n"
"    -k keyword           set meta tag keywords\n"
"\n";

   return 0;
}
//-----------------------------------------------------------------------------

const char * author = "??????";
const char * desc = "??????";
const char * keyw = "APL, GNU";
const char * copyr = 0;

void
print_header(const char * filename, int day, int mon, int year)
{
   cout <<
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n"
"                      \"http://www.w3.org/TR/html4/strict.dtd\">\n"
"<html>\n"
"  <head>\n"
"    <title>" << filename<< "</title>\n"
"    <meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n"
"    <meta name=\"author\" content=\"" << author << "\">\n"
"    <meta name=\"copyright\" content=\"&copy; " <<year<< " by " << copyr << ">\n"
"    <meta name=\"date\" content=\"" << year<<"-"<<mon<<"-"<<day << "\">\n"
"    <meta name=\"description\" content=\"" << desc << "\">\n"
"    <meta name=\"keywords\" lang=\"en\" content=\"" << keyw << "\">\n"
"  </head>\n"
"  <body><pre>\n"
"⍝\n"
"⍝ Author:      " << author << "\n"
"⍝ Date:        " << day << "." << mon << "." << year << "\n"
"⍝ Copyright:   Copyright (C) " << year << " by " << copyr << "\n"
"⍝ License:     GPL see http://www.gnu.org/licenses/gpl-3.0.en.html\n"
"⍝ email:       ??????@??????\n"
"⍝ Portability: ??????\n"
"⍝\n"
"⍝ Purpose:\n"
"⍝ ??????\n"
"⍝\n"
"⍝ Description:\n"
"⍝ ??????\n"
"⍝\n"
;
}
//-----------------------------------------------------------------------------
int
main(int argc, char * argv[])
{
const char * filename = 0;
bool apl_to_html = true;
int year, mon, day;

   {
     timeval tv;
     gettimeofday(&tv, 0);
     tm * t = localtime(&tv.tv_sec);
     year = 1900 + t->tm_year;
     mon = 1 + t->tm_mon;
     day = t->tm_mday;
   }

   for (int a = 1; a < argc; )
       {
         const char * opt = argv[a++];
         const char * val = (a < argc) ? argv[a] : 0;

         if (!strcmp(opt, "-h"))        { return usage(argv[0]); }
         if (!strcmp(opt, "--help"))    { return usage(argv[0]); }
         if (!strcmp(opt, "-a"))        { author = val; ++a;     }
         else if (!strcmp(opt, "-c"))   { copyr = val;  ++a;     }
         else if (!strcmp(opt, "-d"))   { desc = val;   ++a;     }
         else if (!strcmp(opt, "-k"))   { keyw = val;   ++a;     }
         else   // filename ?
            {
              if (val == 0 && access(opt, R_OK) == 0)   // last argv: filename
                 {
                   filename = opt;
                   if (!strcmp(filename + strlen(filename) -4, ".apl"))
                      {
                        apl_to_html = true;
                      }
                   else if (!strcmp(filename + strlen(filename) -5, ".html"))
                      {
                        apl_to_html = false;
                      }
                 }
              else
                 {
                   cerr << "Bad option: " << opt << endl;
                   usage(argv[0]);
                   return 1;
                 }
            }
       }

   if (filename == 0)
      {
        cerr << "missing filename" << endl;
        usage(argv[0]);
        return 1;
      }

FILE * fin = fopen(filename, "r");
   if (fin == 0)   // file exists
      {
        cerr << "cannot open " << filename << strerror(errno) << endl;
        usage(argv[0]);
        return 1;
      }

   if (copyr == 0)   copyr = author;

   print_header(filename, day, mon, year);
   for (;;)
       {
         const int cc = fgetc(fin);
         switch (cc)
            {
              case EOF: break;
              case '&': cout << "&amp;";   continue;
              case '<': cout << "&lt;";    continue;
              case '>': cout << "&gt;";    continue;
              default: cout << (char)cc;   continue;
            }
         break;
       }
   fclose(fin);

   cout << "  </pre></body>\n</html>\n";
   return 0;
}
//-----------------------------------------------------------------------------

