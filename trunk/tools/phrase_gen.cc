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

typedef int TableIndex;

using namespace std;

enum { MAX_PHRASE_LEN = 4 };

struct _phrase
{
   TokenClass classes[MAX_PHRASE_LEN];
   const char * rn0;   // real name of first token for reduce function
   const char * names[MAX_PHRASE_LEN];
   int          prio;
   int          misc;
   int          len;
   int          hash;
   const char * alias;

   string get_suffix() const
      {
        char buffer[40];
        char * b = buffer;
        loop(l, len)   b += sprintf(b, "_%s", names[l]);
        sprintf(b, "[%d]", len);
        return string(buffer);
      }

   /// return the table index of the child with class tc, or 0 if none.
   TableIndex get_child_idx(TokenClass tc) const;

   /// return the table index of the parent, or 0 if none.
   TableIndex get_parent_idx() const;

   /// return true if \b child is a child of \b this
   bool is_child(const _phrase & child) const;

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

// like phrase_table, but including internal nodes (sub_phrases).
vector<_phrase> expanded_table;


enum { PHRASE_COUNT = sizeof(phrase_table) / sizeof(*phrase_table) };

/// return the index of child this→tc (if any), otherwise 0.
TableIndex
_phrase::get_child_idx(TokenClass tc) const
{
   loop(idx, expanded_table.size())
       {
         const _phrase & child = expanded_table[idx];
         if (is_child(child) && child.classes[len] == tc)   return idx;
       }

   return 0;   // none
}

/// return the index of the parent of \b this.
// Fails if the parent is an internal node that does not (yet) exist.

TableIndex
_phrase::get_parent_idx() const
{
   loop(parent_idx, expanded_table.size())
       {
         const _phrase & parent = expanded_table[parent_idx];
         if (parent.is_child(*this))    return parent_idx;
       }

   return 0;   // none
}

bool
_phrase::is_child(const _phrase & child) const
{
   // this is the parent, child may be a child of it.
   //
   if (child.len != (len + 1))                          return false;
   loop(t, len) { if (child.classes[t] != classes[t])   return false; }
   return true;
}
//-----------------------------------------------------------------------------
ostream & get_CERR() { return std::cerr; }
//-----------------------------------------------------------------------------
/// a function that prints one phrase in the large comment at the start of
/// the output file (i.e. Prefix.def)
void
print_phrase(FILE * out, int ph)
{
const _phrase & e = phrase_table[ph];
int len = fprintf(out, "   ║  %2d  │ ", ph);
int hash_len = 0;

   for (int ll = 0; ll < MAX_PHRASE_LEN; ++ll)
       {
         if (e.classes[ll] == TC_INVALID /* == SN_ */)   break;

         ++hash_len;
         switch(e.classes[ll])
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

   while (len < 36)   len += fprintf(out, " ");
   fprintf(out, " │ %d │", hash_len);
   fprintf(out, " %-14s", e.alias);
   fprintf(out, "║\n");
}
//-----------------------------------------------------------------------------
/// a function that prints the entire large comment at the start of
/// the output file (i.e. Prefix.def)
void
print_phrases(FILE * out)
{
   fprintf(out,
"\n"
"/* WARNING: this file is generated:\n"
"\n"
"   Generator:       tools/phrase_gen\n"
"   Generator input: tools/phrase_gen.def\n"
"\n"
"   USAGE: cd tools ; make gen    # hash table\n"
"      OR: cd tools ; make gen2   # search tree\n"
"\n"
"   ╔══════╤═════ PHRASE TABLE ═══╤═══╤═══════════════╗\n"
"   ║number│ phrase               │len│ alias         ║\n"
"   ╟──────┼──────────────────────┼───┼───────────────╢\n"
          );

   for (int ph = 0; ph < PHRASE_COUNT; ++ph)   print_phrase(out, ph);

   fprintf(out,
"   ╚══════╧══════════════════════╧═══╧═══════════════╝\n"
" */\n"
"\n"      );
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
print_entry_decl(FILE * out, const _phrase & e, TableIndex ph)
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
      if (e.classes[0] != SN_ASS)   return;     // but is not MISC_ASS

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

   // suffix is the suffix XXX of the reduce_XXX() function. Similar to
   // phrase_name, but the token classes are separated by '_' rather than ' '.
   //
char suffix[100];   snprintf(suffix, sizeof(suffix), "%s_%s_%s_%s();", 
                             rn0, e.names[1], e.names[2], e.names[3]);

   // member function declaration (Prefix.hh)
   fprintf(out, "   void reduce_%-20s  ///< reduce phrase %s\n",
           suffix, phrase_name);
}
//-----------------------------------------------------------------------------
static void
print_PH_macro(FILE * out, TableIndex e_idx, const _phrase & e, bool hash)
{
   // the first argument (phrase name) of the PH() macro in Prefix.def
   //
char phrase_name[100];
   snprintf(phrase_name, sizeof(phrase_name), "%s %s %s %s", 
                 e.names[0], e.names[1], e.names[2], e.names[3]);

   // the reduce_XXX argument of the PH() macro in Prefix.def
   //
char suffix[100];   snprintf(suffix, sizeof(suffix), "%s_%s_%s_%s", 
                             e.rn0, e.names[1], e.names[2], e.names[3]);

   if (*e.alias)   snprintf(suffix, sizeof(suffix), "%s", e.alias);

const char * macro = "PH";
   if (!hash)   // parse tree
      {
        if (e_idx == 0)              macro = "P0";
        if (e_idx >= PHRASE_COUNT)   macro = "P0";
      }

      if (strcmp(e.rn0, "none"))   // node with reduce function
         fprintf(out, "  %s( %-14s , %-14s, 0x%5.5X ,  %2d  ,  %2d  ,  %1d",
                   macro, phrase_name, suffix, e.hash, e.prio, e.misc, e.len);
      else                         // internal node without reduce function
         fprintf(out, "  %s( %-14s , %-14s, 0x%5.5X ,  %2d  ,  %2d  ,  %1d",
                   macro, phrase_name, "none", e.hash, e.prio, e.misc, e.len);

   if (!hash)   // tree: print sub tree indices
      {
        loop(tc, TC_MAX_PHRASE)   // for every phrase class
            {
              // set child_idx to the phrase_table index for which
              // phrase_table[child_idx] has a final token tc and the
              // other token are the same as the parent e (if any), or
              // 0 if no such child exists.
              
              int child_idx = 0;
              if (e.len < 4)    // since level 4 can not have children
                 {
                   child_idx = e.get_child_idx(TokenClass(tc));
                 }
              if (tc)   fprintf(out, ",%2.2X", child_idx);
              else      fprintf(out, ", %2.2X", child_idx);
            }
      }

   fprintf(out, "),  // [%2.2X]\n", e_idx);
}
//-----------------------------------------------------------------------------
void
expand_table()
{
bool V = false;  // verbosity

   // copy phrase_table into expanded_table
   //
   loop(ph, PHRASE_COUNT)   // for all parents
       {
         _phrase phrase = phrase_table[ph];
         expanded_table.push_back(phrase);
       }

   // add internal nodes
   //
   loop(ph, expanded_table.size())   // for all existing phrases
       {
         if (ph == 0)   continue;   // the tree root has no parent
         const _phrase * child = &phrase_table[ph];
V && fprintf(stderr, "visit #%2.2X child %s\n", int(ph),
            child->get_suffix().c_str());

         for (int ch_len = child->len; ch_len; --ch_len)
            {
              if (const TableIndex parent = child->get_parent_idx())
                 {
                   // child already has a parent

V && fprintf(stderr, " child %s already has parent %s\n",
     child->get_suffix().c_str(),
     expanded_table[parent].get_suffix().c_str());
                   continue;
                 }
              assert(child->len <= 4);

V && fprintf(stderr, "  ├── %s has no parent\n", child->get_suffix().c_str());

              _phrase new_parent = *child;
              --new_parent.len;
              new_parent.names[new_parent.len] = "";   // clear last child token
              new_parent.hash = 0;                 // not used (since tree)
              new_parent.rn0 = "none";
V && fprintf(stderr, "  ├── inserted new %s at [%2.2X]\n",
     new_parent.get_suffix().c_str(), int(expanded_table.size()));

              expanded_table.push_back(new_parent);
              child = &expanded_table.back();

V && fprintf(stderr, "  └── add new parent %s of child %s\n",
     new_parent.get_suffix().c_str(),
     child->get_suffix().c_str());
            }
       }
}
//-----------------------------------------------------------------------------
static void
print_table(FILE * out, bool hash)
{
   // find the smallest modulus for a collision-free hash_table of all prefixes
   //
int MODU = TC_MAX_PHRASE;
   for (;; ++MODU)
       {
         if (test_modu(MODU))   break;
       }

   fprintf(out,
"\n"
"#ifndef PH   // declarations (in Prefix.hh)\n");

   if (hash)   fprintf(out, "\n# define PREFIX_HASH\n\n");
   else        fprintf(out, "\n# define PREFIX_TREE\n\n");

   for (int ph = 0; ph < PHRASE_COUNT; ++ph)
       {
         const _phrase & e = phrase_table[ph];
         print_entry_decl(out, e, ph);
       }

   fprintf(out,
"\n"
"   enum { PHRASE_COUNT   = %d,      ///< number of phrases\n"
"          PHRASE_MODU    = %d,     ///< hash table size\n"
"          MAX_PHRASE_LEN = %d };     ///< max. number of token in a phrase\n"
           , PHRASE_COUNT, MODU, MAX_PHRASE_LEN);

   fprintf(out,
"\n"
"      /// a hash table with all valid phrases (and many invalid entries)\n"
"      static const Phrase hash_table[PHRASE_MODU];\n"
"\n"
"#else  // PH(...) defined: table instantiation (in Prefix.cc)\n\n");

      if (hash)   fprintf(out,

"//PH( phrase_name    , reduce_XXX()  ,   hash  , prio , misc , len)\n"
"//═════════════════════════════════════════════════════════════════\n");

      else        fprintf(out,

"//                                                                  "
"                    F  I  O  O        R  S  V  P  V\n"
"//                                                                  "
"                 F  1  D  P  P        E  Y  A  [  O\n"
"//PH( phrase_name    , reduce_XXX()  ,   hash  , prio , misc , len) "
"  ←  →  [  ]  ◊  0  2  X  1  2  (  )  T  M  L  ]  I\n"
"//═════════════════════════════════════════════════════════════════"
  "00══════════04══════════08══════════0C══════════10═════\n");

   if (hash)   // hash table
      {
        const _phrase ** table = new const _phrase *[MODU];
        for (int i = 0; i < MODU; ++i)   table[i] = phrase_table;
     
        for (int i = 0; i < PHRASE_COUNT; ++i)
            {
              _phrase & e = phrase_table[i];
              table[e.hash % MODU] = phrase_table + i;
            }
     
        loop(ph, MODU)
            {
              print_PH_macro(out, ph, *table[ph], true);
            }
         delete [] table;
      }
   else        // search tree
      {
        expand_table();   // add internal nodes
        loop (ph, expanded_table.size())   // for all parents
            {
              const _phrase & e_par = expanded_table[ph];
              print_PH_macro(out, ph, e_par, false);
            }
      }

   if (!hash)   fprintf(out, "#undef P0\n");  // parse tree
   fprintf(out,
"#undef PH\n"
"\n"
"#endif   // PH(...) defined/not defined\n"
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
               if (phrase_table[ph].classes[l] != phrase_table[ph1].classes[l])
                  {
                    same = false;
                    break;
                  }
             }

         if (same)
            {
              fprintf(stderr, "phrases %d (%s) and %d (%s) are the same\n",
              ph, phrase_table[ph].get_suffix().c_str(),
              ph1, phrase_table[ph1].get_suffix().c_str());
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
               const TokenClass tc = e.classes[l];
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
FILE * out = stdout;
bool hash = argc != 2 || strcmp(argv[1], "-t");

// hash = false;   // OVERRIDE !!!!!!!!!!!!!!!!!!!!!

   for (int ph = 0; ph < PHRASE_COUNT; ++ph)
       {
         _phrase & e = phrase_table[ph];

         assert(e.len == 0);
         assert(e.hash == 0);
         int power = 0;

         for (int l = 0; l < MAX_PHRASE_LEN; ++l)
             {
               if (e.classes[l] == SN_)   break;
               ++e.len;
               e.hash += e.classes[l] << power;
               power += 5;
             }
       }

   check_phrases();
   print_phrases(out);
   print_table(out, hash);

   return 0;
}
//-----------------------------------------------------------------------------
