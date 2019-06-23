/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2019  Dr. Jürgen Sauermann

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

/*
    The code in this file is a TLV server template that can be used as
    a starting point for developing TLV servers with more functionality.

   TLV is the abbreviation for Tag/Length/Value.

   A TLV buffer is a sequence of bytes that consist of (in that order_:

   * a four-byte tag (in big endian byte order, i.e. MSB first),
   * a four-byte value length (in big endian byte order, i.e. MSB first), and
   * a sequence of 0 or more value bytes; the length of the sequence being
     the value length above.

   A TLV with tag T and value V (of length L) is very simple to encode into
   and to decode out of a TLV buffer. GNU APL provides functions for the
   decoding (34 ⎕CR) and encoding (33 ⎕CR) of TLV buffers and a function for
   starting a a TLV server that is connected with GNU APL and can exchange TLV
   buffers with GNU APL.

   The TLV server template below receives arbitrary TLV buffers and sends them
   back with the tag T negated and the value bytes V mirrored. The code at the
   APL side for using the template server is this (provided that the TLV_server
   was installed in /usr/lib/apl/ which is NOT DONE AUTOMATICALLY by
   'make install' !).

      ⍝ one time
      Path ← '/usr/lib/apl/TLV_server'    ⍝ whereever TLV_server was installed
      Handle ← ⎕FIO[57] Path              ⍝ start & connect to the TLV_server

      ⍝ typically in a loop,,,
      TLV ← 33 ⎕CR 42,'Forty-Two'         ⍝ encode a TLV buffer, Tag 42
      ⊣TLV ⎕FIO[43] Handle                ⍝ send TLV buffer to TLV_server
      TL ← 8 ⎕FIO[6] Handle               ⍝ read tag/length from TLV_server
      Value ← (256⊥4↓TL) ⎕FIO[6] Handle   ⍝ read value  from TLV_server
      34 ⎕CR TL,Value                     ⍝ display response tag and value

      ⍝ cleanup (one time)
      TLV ← 33 ⎕CR 99,'quit'              ⍝ encode stop command, Tag 99
      ⊣TLV ⎕FIO[43] Handle                ⍝ send TLV buffer to TLV_server
      ⊣(⎕FIO[4] Handle)                   ⍝ close connection (stops TLV_server)

 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

//-----------------------------------------------------------------------------
int
main(int argc, char * argv[])
{
enum { TLV_socket = 3 };
char TLV[1000];       // the entire TLV buffer
char * V = TLV + 8;   // the value part of the TLV buffer
ssize_t rx_len, tx_len;
int32_t TLV_tag, TLV_len, j;

FILE * f = fdopen(TLV_socket, "r");
   if (f == 0)
      {
        fprintf(stderr,
"fdopen() failed: %s\n"
"That typically happens if this program is started directly,\n"
"more precisely: without opening file descriptor 3 first. The anticipated\n"
"usage is to open this program from GNU APL using ⎕FIO[57] and then to send\n"
"TLV buffers encoded with 33 ⎕CR and send to this program with ⎕FIO[43]\n"
        , strerror(errno));
        return 1;
      }

   for (;;)
       {
          // read the fixed size TL
          //
          rx_len = fread(TLV, 1, 4 + 4, f);
          if (rx_len != 4 + 4)
             {
               fprintf(stderr, "TLV socked closed (1): %s\n", strerror(errno));
               break;
             }

          TLV_tag = (TLV[0] & 0xFF) << 24
                  | (TLV[1] & 0xFF) << 16
                  | (TLV[2] & 0xFF) << 8
                  | (TLV[3] & 0xFF);
          TLV_len = TLV[4] << 24 | TLV[5] << 16 | TLV[6] << 8 | TLV[7];

          // read the variable-sized V
          //
          if (TLV_len)
             {
               rx_len = fread(V, 1, TLV_len, f);
               if (rx_len != TLV_len)
                  {
                    fprintf(stderr, "TLV socked closed (2): %s\n",
                            strerror(errno));
                    break;
                  }
               V[TLV_len] = 0;
             }

          // check for 'quit' Tag from the TLV client
          //
          const int done = (TLV_tag == 99);
          if (done)
             {
               TLV_len = 4;
               V[0] = 'd';
               V[1] = 'o';
               V[2] = 'n';
               V[3] = 'e';
             }
          else
             {
               // negate the tag
               //
               TLV_tag = -TLV_tag;
               TLV[0] = TLV_tag >>  24;
               TLV[1] = TLV_tag >>  16;
               TLV[2] = TLV_tag >>   8;
               TLV[3] = TLV_tag;

               // mirror the value
               //
               for (j = 0; j < TLV_len/2; ++j)
                   {
                     const char tmp = V[j];
                     V[j] = V[TLV_len - j - 1];
                     V[TLV_len - j - 1] = tmp;
                   }
             }

          // send the response
          //
          tx_len = write(TLV_socket, TLV, 4 + 4 + TLV_len);
          if (tx_len != (4 + 4 + TLV_len))
             {
               fprintf(stderr, "TLV socked closed (3): %s\n", strerror(errno));
               break;
             }

          if (done)   break;
       }

   fclose(f);
   return 0;
}

