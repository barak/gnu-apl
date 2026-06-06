
/* this tool reads stdin and remembers the UTF8-encoded Unicodes read.
 */

#include <cstdio>
#include <cstring>
#include <vector>

using namespace std;

char used[0x10000];
vector<int>big_used;
int main()
{
   memset(used, 0, sizeof(used));
   int echo = 0;
   for (;;)
       {
         const int cc = getchar();
         if (echo)
            {
              putchar(cc);
              if (cc == '\n')   --echo;
            }

         if (cc == EOF)   break;

         if ((cc & 0x80) == 0)   continue;   // ASCII
         if ((cc & 0xC0) == 0x80)
            {
              printf("*** Unexpected UTF8-continuation %2.2X\n", cc & 0xFF);
              continue;
            }

          int len, val = cc;
          if      ((cc & 0xE0) == 0xC0)   { len = 2;   val &= 0x1F; }
          else if ((cc & 0xF0) == 0xE0)   { len = 3;   val &= 0x0F; }
          else if ((cc & 0xF8) == 0xF0)   { len = 4;   val &= 0x07; }
          else if ((cc & 0xFC) == 0xF8)   { len = 5;   val &= 0x03; }
          else if ((cc & 0xFE) == 0xFC)   { len = 6;   val &= 0x01; }

          for (int l = 1; l < len; ++l)
              {
                const int cc = getchar();
                if ((cc & 0xC0) != 0x80)
                   {
                     printf("*** Unexpected UTF8-start %2.2X\n", cc & 0xFF);
                     continue;
                   }
                val = (val << 6) | (cc & 0x3F);
              }
          if (val < sizeof(used))   used[val] = 1;
          else   // rarely
             {
               bool found = false;
               for (int b = 0; b < big_used.size(); ++b)
                   {
                     if (big_used[b] == val)
                        {
                          found = true;
                          break;
                        }
                   }
               if (!found)   big_used.push_back(val);
               echo = 5;   // display context
             }

       }

   // print result
   //
   int count = 0;
   for (size_t uni = 0x80; uni < sizeof(used); ++uni)
       {
         if (!used[uni])   continue;

         ++count;
         // printf("%X : ", int(uni));
         if (uni < 0x800)   // 2-byte unicode
            {
              putchar(0xC0 | uni >> 6);
              putchar(0x80 | uni & 0x3F);
              putchar(' ');
            }
         else               // 3-byte unicode
            {
              putchar(0xE0 | (       uni >> 12));
              putchar(0x80 | (0x3F & uni >>  6));
              putchar(0x80 | (0x3F & uni));
              putchar(' ');
            }
         if ((count % 30) == 0)   printf("\n");
       }

   for (size_t b = 0; b < big_used.size(); ++b)
       {
         ++count;
         const int uni = big_used[b];
        if (uni < 0x200000)   // 4-byte unicode
           {
             putchar(0xF0 | (       uni >> 18));
             putchar(0x80 | (0x3F & uni >> 12));
             putchar(0x80 | (0x3F & uni >>  6));
             putchar(0x80 | (0x3F & uni));
             putchar(' ');   // printf("= %X\n", uni);
           }
        else printf(" %X", uni);

         if ((count % 30) == 0)   printf("\n");

       }

   printf("\n\nTotal number of Unicodes: %d\n\n", count);
   return 0;
}
