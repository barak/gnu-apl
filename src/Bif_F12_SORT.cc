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

#include "Assert.hh"
#include "Bif_F12_SORT.hh"
#include "Cell.hh"
#include "Heapsort.hh"
#include "Macro.hh"
#include "Value.hh"
#include "Workspace.hh"

Bif_F12_SORT_ASC  Bif_F12_SORT_ASC::_fun;     // ⍋
Bif_F12_SORT_DES  Bif_F12_SORT_DES::_fun;     // ⍒

Bif_F12_SORT_ASC * Bif_F12_SORT_ASC::fun = &Bif_F12_SORT_ASC::_fun;
Bif_F12_SORT_DES * Bif_F12_SORT_DES::fun = &Bif_F12_SORT_DES::_fun;

//-----------------------------------------------------------------------------
CollatingCache::CollatingCache(const Value & A, const Cell * base,
                               ShapeItem clen)
   : rank(A.get_rank()),
     base_B1(base),
     comp_len(clen)
{
const ShapeItem ec_A = A.element_count();
UCS_string UA;
   UA.reserve(ec_A);
   loop(a, ec_A)   UA.append(A.get_ravel(a).get_char_value());

UCS_string UA1 = UA.unique();

   reserve(UA1.size());

   // create CollatingCacheEntry for every char in UA1. At this point, all
   // entries are located at the end of A.
   //
   loop(a, UA1.size())
       {
         const Unicode uni = UA1[a];
         const CollatingCacheEntry entry(uni, A.get_shape());
         push_back(entry);
       }

   // move entries back
   //
   loop(a, ec_A)
      {
        const Unicode uni = A.get_ravel(a).get_char_value();
        CollatingCacheEntry & entry = at(find_entry(uni));

        ShapeItem aq = a;
        loop(r, A.get_rank())
           {
             const Rank axis = entry.ce_shape.get_rank() - r - 1;
             const ShapeItem ar = aq % A.get_shape_item(axis);
             Assert(ar <= A.get_shape_item(axis));
             if (entry.ce_shape.get_shape_item(axis) > ar)
                entry.ce_shape.set_shape_item(axis, ar);
             aq /= A.get_shape_item(axis);
           }
      }

   // add one entry for all characters in B that are not in A
   //
CollatingCacheEntry others(Invalid_Unicode, A.get_shape());
   push_back(others);
}
//-----------------------------------------------------------------------------
bool
CollatingCache::greater_vec(const IntCell & Za, const IntCell & Zb,
                            const void * comp_arg)
{
const CollatingCache & cache =
                      *reinterpret_cast<const CollatingCache *>(comp_arg);
const Cell * ca = cache.base_B1 + cache.comp_len * Za.get_int_value();
const Cell * cb = cache.base_B1 + cache.comp_len * Zb.get_int_value();

const Rank rank = cache.get_rank();

   loop(r, rank)
      {
        loop(c, cache.get_comp_len())
           {
             const APL_Integer a = ca[c].get_int_value();
             const APL_Integer b = cb[c].get_int_value();
             const int diff = cache[a].compare_axis(cache[b], rank - r - 1);

              if (diff)   return diff > 0;
           }
      }

   return ca > cb;
}
//-----------------------------------------------------------------------------
bool
CollatingCache::smaller_vec(const IntCell & Za, const IntCell & Zb,
                            const void * comp_arg)
{
const CollatingCache & cache =
                      *reinterpret_cast<const CollatingCache *>(comp_arg);
const Cell * ca = cache.base_B1 + cache.comp_len * Za.get_int_value();
const Cell * cb = cache.base_B1 + cache.comp_len * Zb.get_int_value();
const Rank rank = cache.get_rank();

   loop(r, rank)
      {
        loop(c, cache.get_comp_len())
           {
             const APL_Integer a = ca[c].get_int_value();
             const APL_Integer b = cb[c].get_int_value();
             const int diff = cache[a].compare_axis(cache[b], rank - r - 1);

              if (diff)   return diff < 0;
           }
      }

   return ca > cb;
}
//-----------------------------------------------------------------------------
ShapeItem
CollatingCache::find_entry(Unicode uni) const
{
const CollatingCacheEntry * entries = &at(0);
const CollatingCacheEntry * entry =

   Heapsort<CollatingCacheEntry>:: search<Unicode>(uni, entries, size(),
                                           CollatingCacheEntry::compare_chars);

   if (entry)   return entry - entries;
   return size() - 1;   // the entry for characters not in A
}
//=============================================================================
Token
Bif_F12_SORT::sort(Value_P B, Sort_order order)
{
   if (B->is_scalar())          return Token(TOK_ERROR, E_RANK_ERROR);
   if (!B->can_be_compared())   return Token(TOK_ERROR, E_DOMAIN_ERROR);

const ShapeItem len_BZ = B->get_shape_item(0);
   if (len_BZ == 0)   return Token(TOK_APL_VALUE1, Idx0(LOC));

const ShapeItem comp_len = B->element_count()/len_BZ;

   // first set Z←⍳len_BZ
   //
const int qio = Workspace::get_IO();
Value_P Z(len_BZ, LOC);
   loop(l, len_BZ)   new (Z->next_ravel())   IntCell(l + qio);
   Z->check_value(LOC);
   if (len_BZ == 1)   return Token(TOK_APL_VALUE1, Z);

   // then sort Z (actually re-arrange Z so that B[Z] is sorted)
   //
const Cell * base = &B->get_ravel(0) - qio*comp_len;
const struct { const Cell * base; ShapeItem comp_len; } ctx = { base, comp_len};

   if (order == SORT_ASCENDING)
      Heapsort<IntCell>::sort(&Z->get_ravel(0).vIntCell(),
                              len_BZ, &ctx, &Cell::greater_vec);
   else
      Heapsort<IntCell>::sort(&Z->get_ravel(0).vIntCell(),
                               len_BZ, &ctx, &Cell::smaller_vec);

   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------
Token
Bif_F12_SORT::sort_collating(Value_P A, Value_P B, Sort_order order)
{
   if (A->is_scalar())   RANK_ERROR;
   if (A->NOTCHAR())     DOMAIN_ERROR;

const APL_Integer qio = Workspace::get_IO();
   if (B->NOTCHAR())     DOMAIN_ERROR;
   if (B->is_scalar())   return Token(TOK_APL_VALUE1, IntScalar(qio, LOC));

const ShapeItem len_BZ = B->get_shape_item(0);
   if (len_BZ == 0)   return Token(TOK_APL_VALUE1, Idx0(LOC));

   // first set Z←⍳len_BZ
   //
Value_P Z(len_BZ, LOC);
   loop(l, len_BZ)   new (Z->next_ravel())   IntCell(l + qio);
   Z->check_value(LOC);
   if (len_BZ == 1)   return Token(TOK_APL_VALUE1, Z);

const ShapeItem ec_B = B->element_count();
const ShapeItem comp_len = ec_B/len_BZ;

   // create a vector B1 which has the same shape as B, but instead of
   // B's characters, it has the index of the corresponding CollatingCache
   // index for each character in B.
   //
Value_P B1(B->get_shape(), LOC);
const Cell * base_B1 = &B1->get_ravel(0) - qio*comp_len;
CollatingCache cache(A.getref(), base_B1, comp_len);
   loop(b, ec_B)
      {
        const Unicode uni = B->get_ravel(b).get_char_value();
        const APL_Integer b1 = cache.find_entry(uni);
        new (B1->next_ravel()) IntCell(b1);
      }
   B1->check_value(LOC);

   // then sort Z (actually re-arrange Z so that B[Z] is sorted)
   //
IntCell * z0 = &Z->get_ravel(0).vIntCell();
   if (order == SORT_ASCENDING)
      Heapsort<IntCell>::sort(z0, len_BZ, &cache, &CollatingCache::greater_vec);
   else
      Heapsort<IntCell>::sort(z0, len_BZ, &cache, &CollatingCache::smaller_vec);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//-----------------------------------------------------------------------------

