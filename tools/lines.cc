
#include <cstdint>
#include <cstdio>

using namespace std;

//----------------------------------------------------------------------------
int
print_UCS(const char * utf)
{
int b0 = *utf++;
int UCS = 0;

   if      ((b0 & 0xE0) == 0xC0)   UCS = b0 & 0x1F;
   else if ((b0 & 0xF0) == 0xE0)   UCS = b0 & 0x0F;
   else if ((b0 & 0xF8) == 0xF0)   UCS = b0 & 0x0E;
 
   while (*utf & 0x80)   UCS = UCS << 6 | *utf++ & 0x3F;

   return printf(" U+%4.4X =", UCS);
}
//----------------------------------------------------------------------------
#define U   "⁰ ¹ ² ³ ⁴ ⁵ ⁶ ⁷ ⁸ ⁹ ⁺ ⁻ ⁼ ⁽ ⁾ ⁱ ʲ ᵏ ᵐ ⁿ ᵗ "
#define L   "₀ ₁ ₂ ₃ ₄ ₅ ₆ ₇ ₈ ₉ ₊ ₋ ₌ ₍ ₎ ᵢ ⱼ ₖ ₘ ₙ "
#define GR  "⍺ λ μ χ ⍵ ⇄ ° • ⎡ ⎢ ⎣ ⎤ ⎥ ⎦ ⎛ ⎜ ⎝ ⎞ ⎟ ⎠ ⎲ ⎳ ⌠ ⌡ "

#define L11 "┌ ┬ ┐ ├ ┼ ┤ └ ┴ ┘ "
#define L12 "╒ ╤ ╕ ╞ ╪ ╡ ╘ ╧ ╛ "
#define L21 "╓ ╥ ╖ ╟ ╫ ╢ ╙ ╨ ╜ "
#define L22 "╔ ╦ ╗ ╟ ╫ ╢ ╚ ╩ ╝ "
#define LHV "│ ─ ║ ═ "
#define LINES L11 L12 L21 L22 LHV

#define UNIC LINES GR

int
main(int, char * [])
{
   printf(
"\n"
"    ┌───┬───┐    ╒═══╤═══╕    ╓───╥───╖    ╔═══╦═══╗\n"
"    │   │   │    │   │   │    ║   ║   ║    ║   ║   ║\n"
"    ├───┼───┤    ╞═══╪═══╡    ╟───╫───╢    ╟───╫───╢\n"
"    │   │   │    │   │   │    ║   ║   ║    ║   ║   ║\n"
"    └───┴───┘    ╘═══╧═══╛    ╙───╨───╜    ╚═══╩═══╝\n"
"\n"
"    U" U  "\n"
"    L" L  "\n"
"       " GR "\n"
"\n"
         );

const char * indent = "   ";
int len = printf("%s", indent);
   for (const char * cp = UNIC; *cp;)
       {
         while ((*cp & 0x80) == 0)   // while ASCII
               {
                 len += printf(" %c", *cp++);
                 continue;
               }

         // UTF8 encoding... first byte is ≥ 0xC0, subsequent 0x80 ... 0xC0
         //
         char utf[10];
         char * utfp = utf;
         do { *utfp++ = *cp++; } while (bool(*cp & 0x80) != bool(*cp & 0x40));
         *utfp = 0;   ++cp;

         printf(" %s", utf);   len += 2;
         len += print_UCS(utf);

         int byte_count = 0;
         while (utf[byte_count])
             len += printf(" %2.2X", 0xFF & utf[byte_count++]);
         for (; byte_count < 4; ++byte_count)   { printf("   ");   len += 3; }

         if (len > 60)   { len = printf("\n%s", indent); }
       }
   printf("\n");
}
//----------------------------------------------------------------------------

