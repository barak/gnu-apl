/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2026  Dr. Jürgen Sauermann

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

#include <algorithm>
#include "Assert.hh"
#include "Bif_F12_SORT.hh"
#include "Cell.hh"
#include "Heapsort.hh"
#include "Macro.hh"
#include "Value.hh"
#include "Workspace.hh"

Bif_F12_SORT_ASC  Bif_F12_SORT_ASC::fun;     // ⍋
Bif_F12_SORT_DES  Bif_F12_SORT_DES::fun;     // ⍒

//════════════════════════════════════════════════════════════════════════════
/** class CollatingCache is a helper that efficiently maps characters in
    vector B to an integer vector B1 (the significances of B) according to
    the collating sequence A.

    Sorting with collation has to first search every item B[j] in A where
    the first match in A defines the significance of B[j]. For that reason
    subsequent copies A[a+k] of some A[a] will never match and are therefore
    not stored in the CollatingCache.
 **/
//════════════════════════════════════════════════════════════════════════════
CollatingCache::CollatingCache(const cValue & A,
                               const vector<ShapeItem> & signif,
                               ShapeItem clen)
   : rank(A.get_rank()),
     significances_B(signif),
     comp_len(clen)
{
const ShapeItem ec_A = A.element_count();
UCS_string UA;
   UA.reserve(ec_A);
   loop(a, ec_A)   UA << A.get_char_value(a);

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

   // move entries back. ISO 13751 10.2.21: "A character's lowest index
   // along an axis defines its position in the order described by that
   // axis", i.e. the minimum of every axis separately (e.g. A and B are
   // equivalent in 2 2⍴'ABBA'). Not the position where it is first found
   // (APL2 LRM p.155, Bugs35 #32).
   //
   loop(a, ec_A)
      {
        const Unicode uni = A.get_char_value(a);
        CollatingCacheEntry & entry = at(get_significance(uni));

        ShapeItem aq = a;
        loop(r, A.get_rank())
           {
             const sAxis axis = entry.ce_shape.get_rank() - r - 1;
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
//────────────────────────────────────────────────────────────────────────────
ShapeItem
CollatingCache::get_significance(Unicode uni) const
{
const CollatingCacheEntry * entries = &at(0);
const CollatingCacheEntry * entry =

   Heapsort<CollatingCacheEntry>::search<Unicode>(uni, *this,
                                         CollatingCacheEntry::compare_chars, 0);

   if (entry)   return entry - entries;
   return size() - 1;   // the entry for characters not in A
}
//────────────────────────────────────────────────────────────────────────────
bool
CollatingCache::greater_vec(const ShapeItem & Za, const ShapeItem & Zb,
                            const void * comp_arg)
{
const CollatingCache & cache =
                      *reinterpret_cast<const CollatingCache *>(comp_arg);
const ShapeItem * significance_a = &cache.significances_B[cache.comp_len * Za];
const ShapeItem * significance_b = &cache.significances_B[cache.comp_len * Zb];

const sRank rank = cache.get_rank();

   rev_loop(r, rank)
   loop(c, cache.get_comp_len())
       {
         const CollatingCacheEntry & a = cache[significance_a[c]];
         const CollatingCacheEntry & b = cache[significance_b[c]];
         if (const int diff = a.compare_axis(b, r))   return diff > 0;
       }

   return significance_a > significance_b;
}
//────────────────────────────────────────────────────────────────────────────
bool
CollatingCache::smaller_vec(const ShapeItem & Za, const ShapeItem & Zb,
                            const void * comp_arg)
{
const CollatingCache & cache =
                      *reinterpret_cast<const CollatingCache *>(comp_arg);
const ShapeItem * significance_a = &cache.significances_B[cache.comp_len * Za];
const ShapeItem * significance_b = &cache.significances_B[cache.comp_len * Zb];

const sRank rank = cache.get_rank();

   rev_loop(r, rank)
   loop(c, cache.get_comp_len())
       {
         const CollatingCacheEntry & a = cache[significance_a[c]];
         const CollatingCacheEntry & b = cache[significance_b[c]];
         if (const int diff = a.compare_axis(b, r))   return diff < 0;
       }

   // Bugs9 #9 (Blake McBride): heapsort is unstable, so this tiebreak is
   // the only thing holding equal items in index order. Inverting the
   // *value* comparison above for a descending sort (⍒) is correct, but
   // inverting the tiebreak along with it is not -- ties must always break
   // ascending by index, matching greater_vec() (used by ⍋) just above.
   return significance_a > significance_b;
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_SORT::sort(cValue_R B, Sort_order order)
{
const char * where = order == SORT_ASCENDING ? "⍋B" : "⍒B";
   if (B.is_scalar())
      {
        MORE_ERROR() << where << ": B is a scalar; B must have rank ≥ 1";
        return Token(TOK_ERROR, E_RANK_ERROR);
      }
   if (!B.can_be_compared())
      {
        MORE_ERROR() << where << ": B contains items that cannot be"
                        " compared with each other";
        return Token(TOK_ERROR, E_DOMAIN_ERROR);
      }

const ShapeItem len_BZ = B.get_shape_item(0);
   if (len_BZ == 0)   return Token(TOK_APL_VALUE1, Idx0(LOC));
const ShapeItem comp_len = B.element_count()/len_BZ;

const int qio = Workspace::get_IO();
Value_P Z(len_BZ, LOC);
int64_t * dst = reinterpret_cast<int64_t *>(&Z->get_wfirst());

   // fast path for comp_len == 0 (e.g. 3 0⍴0): every "row" is empty, so
   // all rows compare equal and the (stable) result is their original
   // index order. Cell::sorted_indices() sizes its vector by
   // element_count()/comp_len, i.e. 0/0, leaving it empty while len_BZ
   // (here 3) is not -- the caller below then reads past it. See
   // Bugs29 #1 (Blake McBride), a regression from the Bugs28 #25 fix.
   if (comp_len == 0)
      {
        loop(b, len_BZ)   dst[b] = b + qio;
        Z->commit_ravel_Int64(len_BZ);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // fast path for 1D INT64/FLOAT64: std::stable_sort avoids Cell dispatch
   if (comp_len == 1)
      {
        loop(b, len_BZ)   dst[b] = b;

        if (B.get_ravel_type() == RPT_INT64)
           {
             const int64_t * pB = B.cravel_int64();
             if (order == SORT_ASCENDING)
                std::stable_sort(dst, dst + len_BZ,
                                 [pB](ShapeItem a, ShapeItem b)
                                 { return pB[a] < pB[b]; });
             else
                std::stable_sort(dst, dst + len_BZ,
                                 [pB](ShapeItem a, ShapeItem b)
                                 { return pB[a] > pB[b]; });
             loop(b, len_BZ)   dst[b] += qio;
             Z->commit_ravel_Int64(len_BZ);
             Z->check_value(LOC);
             return Token(TOK_APL_VALUE1, Z);
           }

        if (B.get_ravel_type() == RPT_FLOAT64)
           {
             const double * pB = B.cravel_float64();
             if (order == SORT_ASCENDING)
                std::stable_sort(dst, dst + len_BZ,
                                 [pB](ShapeItem a, ShapeItem b)
                                 { return pB[a] < pB[b]; });
             else
                std::stable_sort(dst, dst + len_BZ,
                                 [pB](ShapeItem a, ShapeItem b)
                                 { return pB[a] > pB[b]; });
             loop(b, len_BZ)   dst[b] += qio;
             Z->commit_ravel_Int64(len_BZ);
             Z->check_value(LOC);
             return Token(TOK_APL_VALUE1, Z);
           }
      }

vector<ShapeItem> ordered_indices_B;
   // exact = true: ISO 10.1.2 "⎕CT is not an implicit argument" to
   // grading -- the RPT_INT64/RPT_FLOAT64 fast path above (std::stable_
   // sort with plain </>) was already exact; this general path used
   // Cell::compare(), which IS ⎕CT-tolerant (correct for </>/⌈/⌊ etc,
   // wrong here), giving ⍋/⍒ a result that changed with the packing
   // threshold. See Bugs27 #30.
   Cell::sorted_indices(ordered_indices_B, B, order, comp_len, true);

   loop(b, len_BZ)   dst[b] = ordered_indices_B[b] + qio;
   Z->commit_ravel_Int64(len_BZ);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_SORT::sort_collating(cValue_R A, cValue_R B, Sort_order order)
{
const APL_Integer qio = Workspace::get_IO();
const char * where = order == SORT_ASCENDING ? "A⍋B" : "A⍒B";
   if (A.is_scalar())
      {
        MORE_ERROR() << where << ": A is a scalar; A (the collating"
                        " sequence) must have rank ≥ 1";
        RANK_ERROR;
      }
   if (A.NOTCHAR())
      {
        MORE_ERROR() << where << ": A (the collating sequence) must be"
                        " characters";
        DOMAIN_ERROR;
      }
   if (B.NOTCHAR())
      {
        MORE_ERROR() << where << ": B must be characters";
        DOMAIN_ERROR;
      }
   // APL2 LRM p.155: "R: Simple nonscalar character array" (Bugs35 #33).
   // An empty A is allowed (ISO 13751 10.2.21: "If A is empty, return
   // IO+⍳1↑⍴B").
   //
   if (B.is_scalar())
      {
        MORE_ERROR() << where << ": B is a scalar; B must have rank ≥ 1";
        RANK_ERROR;
      }

const ShapeItem len_BZ = B.get_shape_item(0);
   if (len_BZ == 0)   return Token(TOK_APL_VALUE1, Idx0(LOC));   // return ⍬
   if (len_BZ == 1)
      {
        Value_P Z(1, LOC);
        Z->next_ravel_Int(qio);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

const ShapeItem ec_B = B.element_count();
const ShapeItem comp_len = ec_B/len_BZ;

   // create integer vector B1 which is a 1:1 mapping of the characters
   // in the ravel of B to their integer significance.
   //
vector<ShapeItem> B1;
   B1.reserve(ec_B);
CollatingCache cache(A, B1, comp_len);
   loop(b, ec_B)
      {
        const Unicode uni = B.get_char_value(b);
        const ShapeItem b1 = cache.get_significance(uni);
        B1.push_back(b1);
      }

vector<ShapeItem> vZ;
   vZ.reserve(len_BZ);
   loop(z, len_BZ)   vZ.push_back(z);

   if (order == SORT_ASCENDING)
      Heapsort<ShapeItem>::sort(vZ, &CollatingCache::greater_vec, &cache);
   else
      Heapsort<ShapeItem>::sort(vZ, &CollatingCache::smaller_vec, &cache);

Value_P Z(len_BZ, LOC);
int64_t * dst = reinterpret_cast<int64_t *>(&Z->get_wfirst());
   loop(z, len_BZ)   dst[z] = vZ[z] + qio;
   Z->commit_ravel_Int64(len_BZ);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════

