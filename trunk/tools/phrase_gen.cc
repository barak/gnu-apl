/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2022  Dr. Jürgen Sauermann

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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "../src/Id.hh"

using namespace std;

enum { MAX_PHRASE_LEN = 4 };

struct _phrase
{
   TokenClass phrase[MAX_PHRASE_LEN];
   const char * rn0;   // real name of first token for reduce function
   const char * names[MAX_PHRASE_LEN];
   int          prio;
   int          misc;
   int          len;
   int          hash;
   const char * alias;
} phrase_table[] =
{
#define sn(x) SN_ ## x
#define tn(x) # x
#define phrase(prio, a, b, c, d, alias)          \
   { { sn(a), sn(b), sn(c), sn(d)  }, tn(a),  \
     { tn(a), tn(b), tn(c), tn(d), }, prio, 0, 0, 0, alias },

// phrase1 is a helper for phrase_MISC() to keep it short
//
#define phrase1(prio, a, b, c, d, alias)         \
   { { sn(a), sn(b), sn(c), sn(d) }, "MISC",   \
     { tn(a), tn(b), tn(c), tn(d) }, prio, 1, 0, 0, alias },

// phrase_MISC is a shortcut for a collection of phrases that cause a
// nomadic function to be called monadically.
#define phrase_MISC(prio, b, c, d, alias)   \
   phrase1(prio, ASS,   b, c, d, alias)     \
   phrase1(prio, GOTO,  b, c, d, alias)     \
   phrase1(prio, F,     b, c, d, alias)     \
   phrase1(prio, LBRA,  b, c, d, alias)     \
   phrase1(prio, END,   b, c, d, alias)     \
   phrase1(prio, LPAR,  b, c, d, alias)     \
   phrase1(prio, C,     b, c, d, alias)     \
   phrase1(prio, RETC,  b, c, d, alias)     \
   phrase1(prio, M,     b, c, d, alias)

phrase(0,                     ,      ,      ,    , "")

#include "phrase_gen.def"
};

enum { PHRASE_COUNT = sizeof(phrase_table) / sizeof(*phrase_table) };

//-----------------------------------------------------------------------------
ostream & get_CERR() { return std::cerr; }
//-----------------------------------------------------------------------------
/// a function that prints one phrase in the large comment at the start of
/// the output file (i.e. Prefix.def)
void
print_phrase(FILE * out, int ph)
{
const _phrase & e = phrase_table[ph];
int len = fprintf(out, "   phrase %2d: ", ph);
int hash_len = 0;

   for (int ll = 0; ll < MAX_PHRASE_LEN; ++ll)
       {
         if (e.phrase[ll] == TC_INVALID /* == SN_ */)   break;

         ++hash_len;
         switch(e.phrase[ll])
            {
              case TC_ASSIGN:   len += fprintf(out, " ←   ") - 2;   break;
              case TC_R_ARROW:  len += fprintf(out, " →   ") - 2;   break;
              case TC_FUN12:    len += fprintf(out, " F  ");        break;
              case TC_OPER1:    len += fprintf(out, " OP1 ");       break;
              case TC_OPER2:    len += fprintf(out, " OP2 ");       break;
              case TC_L_BRACK:  len += fprintf(out, " [ ; ");       break;
              case TC_END:      len += fprintf(out, " END ");       break;
              case TC_L_PARENT: len += fprintf(out, " (   ");       break;
              case TC_R_BRACK:  len += fprintf(out, " ]   ");       break;

              case TC_RETURN:   len += fprintf(out, " RET ");       break;
              case TC_VALUE:    len += fprintf(out, " VAL ");       break;
              case TC_INDEX:    len += fprintf(out, " [X] ");       break;
              case TC_SYMBOL:   len += fprintf(out, " SYM ");       break;
              case TC_FUN0:     len += fprintf(out, " F0  ");       break;
              case TC_R_PARENT: len += fprintf(out, " )   ");       break;

              case TC_PINDEX:   len += fprintf(out, " PIDX");       break;
              case TC_VOID:     len += fprintf(out, " VOID");       break;

              default: assert(0);
            }
       }

   while (len < 40)   len += fprintf(out, " ");
   fprintf(out, "%d", hash_len);
   if (*e.alias)   fprintf(out, " %s", e.alias);
   fprintf(out, "\n");
}
//-----------------------------------------------------------------------------
/// a function that prints the entire large comment at the start of
/// the output file (i.e. Prefix.def)
void
print_phrases(FILE * out)
{
   fprintf(out, "\n   /*** phrase table ***\n"
                "\n"
                "   phrase ##:  phrase              length alias\n"
                "   ---------------------------------------------\n");

   for (int ph = 0; ph < PHRASE_COUNT; ++ph)   print_phrase(out, ph);

   fprintf(out, "   ---------------------------------------------\n"
                "\n"
                "   *** phrase table ***/\n\n");
}
//-----------------------------------------------------------------------------
int
test_modu(int modu)
{
int * tab = new int[modu];

   for (int m = 0; m < modu; ++m)   tab[m] = -1;

   for (int ph = 0; ph < PHRASE_COUNT; ++ph)
       {
         const _phrase & e = phrase_table[ph];
         const int idx = e.hash % modu;
         if (tab[idx] == -1)   // free entry
            {
              tab[idx] = ph;
            }
         else                  // collision
            {
              delete [] tab;
              return false;
            }
       }

   delete [] tab;
   return true;
}
//-----------------------------------------------------------------------------
/// write the declaration of one reduce_ function to file \b out
void
print_entry_decl(FILE * out, const _phrase & e)
{
   // if phrase \b e is an alias then do nothing. For example, A D G is an alias
   // for F D B because reduce_F_D_G_() would do the same as reduce_A_D_G_()).
   //
   if (*e.alias)   return;

const char * rn0   = e.rn0;        // e.g. A F ... MISC MISC
const char * name0 = e.names[0];   // e.g. A F ... ASS  GOTO
   assert(rn0);
   assert(name0);

   // all MISC_xxx phrases with the same suffix xxx have the same reduce function
   // reduce_MISC_xxx(). We declare reduce_MISC_xxx() only once when we are
   // called for the first MISC item (ASS).
   //
   if (!strcmp(rn0, "MISC"))                   // e is a MISC_xxx phrase
      if (e.phrase[0] != SN_ASS)   return;     // but is not MISC_ASS

   // phrase_name is the first argument of the emitted PH() macro. It is a
   // blank separated list of abbreviated token classes, for example:
   // "F D G" or "V ASS B"
   //
char phrase_name[100];
int name_len = snprintf(phrase_name, sizeof(phrase_name), "%s %s %s %s", 
                        name0, e.names[1], e.names[2], e.names[3]);

   // remove trailing whitespace from unused token classes
   while (name_len && phrase_name[name_len - 1] == ' ')
         phrase_name[--name_len] = 0;

   // suffix is the suffix of the reduce function. Similar to phrase_name, but
   // the token classes are separated by '_' rather than ' '.
   //
char suffix[100];   snprintf(suffix, sizeof(suffix), "%s_%s_%s_%s();", 
                             rn0, e.names[1], e.names[2], e.names[3]);

   fprintf(out, "   void reduce_%-20s  ///< reduce phrase %s\n",
           suffix, phrase_name);
}
//-----------------------------------------------------------------------------
void
print_entry_macro(FILE * out, const _phrase & e)
{
   // the first argument (phrase name) of the PH() macro in Prefix.def
   //
char phrase_name[100];
   snprintf(phrase_name, sizeof(phrase_name), "%s %s %s %s", 
                 e.names[0], e.names[1], e.names[2], e.names[3]);

   // the last argument (reduce_XXX) of the PH() macro in Prefix.def
   //
char suffix[100];   snprintf(suffix, sizeof(suffix), "%s_%s_%s_%s", 
                          e.rn0, e.names[1], e.names[2], e.names[3]);

   if (*e.alias)   snprintf(suffix, sizeof(suffix), "%s", e.alias);

   fprintf(out, "  PH( %-14s , 0x%5.5X,   %2d,   %d,  %d, %-12s )\n",
                phrase_name, e.hash, e.prio, e.misc, e.len, suffix);
}
//-----------------------------------------------------------------------------
void
print_table(FILE * out)
{
   // find smallest modulus for wrapping hash_table.
   //
int MODU = TC_MAX_PHRASE;
   for (;; ++MODU)
       {
         if (test_modu(MODU))   break;
       }

   fprintf(out,
"\n"
"#ifndef PH\n"
"\n");

   for (int ph = 0; ph < PHRASE_COUNT; ++ph)
       {
         const _phrase & e = phrase_table[ph];
         print_entry_decl(out, e);
       }

   fprintf(out,
"\n"
"   enum { PHRASE_COUNT   = %d,      ///< number of phrases\n"
"          PHRASE_MODU    = %d,     ///< hash table size\n"
"          MAX_PHRASE_LEN = %d };     ///< max. number of token in a phrase\n"
           , PHRASE_COUNT, MODU, MAX_PHRASE_LEN);

   fprintf(out,
"\n"
"   /// one phrase in the phrase table\n"
"   struct Phrase\n"
"      {\n"
"        const char *   phrase_name;     ///< phrase name\n"
"        int            phrase_hash;     ///< phrase hash\n"
"        int            prio;            ///< phrase priority\n"
"        int            misc;            ///< 1 if MISC phrase\n"
"        int            phrase_len;      ///< phrase length\n"
"        void (Prefix::*reduce_fun)();   ///< reduce function\n"
"        const char *   reduce_name;     ///< reduce function name\n"
"      };\n"
"\n"
"      /// a hash table with all valid phrases (and many invalid entries)\n"
"      static const Phrase hash_table[PHRASE_MODU];\n"
"\n"
"#else  // PH defined\n"
"\n"
"const Prefix::Phrase Prefix::hash_table[PHRASE_MODU] =\n"
"{\n"
"  //  phrase_name      hash     prio misc len  reduce_XXX()\n"
"  //  -----------------------------------------------------\n");

   {
     const _phrase ** table = new const _phrase *[MODU];
     for (int i = 0; i < MODU; ++i)   table[i] = phrase_table;

     for (int i = 0; i < PHRASE_COUNT; ++i)
         {
           _phrase & e = phrase_table[i];
           table[e.hash % MODU] = phrase_table + i;
         }

     for (int i = 0; i < MODU; ++i)
         {
           print_entry_macro(out, *table[i]);
         }
      delete [] table;
   }


   fprintf(out,
"};\n"
"\n"
"#undef PH\n"
"\n"
"#endif   // PH defined\n"
"\n");
}
//-----------------------------------------------------------------------------
void
check_phrases()
{
   // check that all phrases are different
   //
   for (int ph = 0; ph < PHRASE_COUNT; ++ph)
   for (int ph1 = ph + 1; ph1 < PHRASE_COUNT; ++ph1)
       {
         bool same = true;
         for (int l = 0; l < MAX_PHRASE_LEN; ++l)
             {
               if (phrase_table[ph].phrase[l] != phrase_table[ph1].phrase[l])
                  {
                    same = false;
                    break;
                  }
             }

         if (same)
            {
              fprintf(stderr, "phrases %d and %d are the same\n", ph, ph1);
              assert(0);

            }
       }

   for (int ph = 0; ph < PHRASE_COUNT; ++ph)
       {
         const _phrase & e = phrase_table[ph];

         // checks on all token classes
         //
         for (int l = 0; l < MAX_PHRASE_LEN; ++l)
             {
               const TokenClass tc = e.phrase[l];
               if (tc == TC_INVALID)   continue;

               if (tc >= TC_MAX_PHRASE)
                  {
                    fprintf(stderr, "bad token %X in phrase %d token %d\n",
                            tc, ph, l);
                    assert(0);
                  }
             }
       }
}
//-----------------------------------------------------------------------------
int
main(int argc, char * argv[])
{
   for (int ph = 0; ph < PHRASE_COUNT; ++ph)
       {
         _phrase & e = phrase_table[ph];

         assert(e.len == 0);
         assert(e.hash == 0);
         int power = 0;

         for (int l = 0; l < MAX_PHRASE_LEN; ++l)
             {
               if (e.phrase[l] == SN_)   break;
               ++e.len;
               e.hash += e.phrase[l] << power;
               power += 5;
             }
       }

   check_phrases();
   print_phrases(stdout);
   print_table(stdout);

   return 0;
}
//-----------------------------------------------------------------------------
