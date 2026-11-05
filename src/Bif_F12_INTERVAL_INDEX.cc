/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2020-2026  Dr. Jürgen Sauermann

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
#include "ArgCheck.hh"
#include "Bif_F12_INTERVAL_INDEX.hh"
#include "Workspace.hh"

Bif_F12_INTERVAL_INDEX Bif_F12_INTERVAL_INDEX::fun;    // ⍳

//════════════════════════════════════════════════════════════════════════════
/// search elements of B in ntervals defined by A. ⍴Z is ⍴B, and elements of Z
/// are indices of A so that A[Z[N]] ≤ B[N] < A[Z[N] + 1]
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_INTERVAL_INDEX::eval_AB(cValue_R A, cValue_R B) const
{
  // A must be a non-empty sorted vector
  //
   ArgCheck::require_scalar_or_vector("A⍸B", "A", A);
const ShapeItem ec_A = A.element_count();
   if (ec_A < 1)
      {
        MORE_ERROR() << "A⍸B: A is empty (expecting ⍴A > 0)";
        LENGTH_ERROR;
      }

   // Bugs28 #100(r): A⍸B used to order genuinely complex numbers
   // silently via Cell::compare()'s arbitrary real-then-imaginary
   // fallback (e.g. 1J1 2⍸1 gave 0), unlike ⍋/⍒ (Bif_F12_SORT::sort()),
   // which already rejects exactly this with the same can_be_compared()
   // check used here.
   //
   if (!A.can_be_compared())
      {
        MORE_ERROR() << "A⍸B: A contains items that cannot be compared"
                        " with each other";
        DOMAIN_ERROR;
      }
   if (!B.can_be_compared())
      {
        MORE_ERROR() << "A⍸B: B contains items that cannot be compared"
                        " with each other";
        DOMAIN_ERROR;
      }

   // from here on nothing can fail.
   //
const ShapeItem ec_B = B.element_count();
const APL_Integer qio = Workspace::get_IO();

   // INT64 × INT64 fast path: std::upper_bound avoids Cell dispatch.
   // Sort check uses cravel_int64() to avoid exploding a packed ravel.
   if (A.get_ravel_type() == RPT_INT64 && B.get_ravel_type() == RPT_INT64)
      {
        const int64_t * pA = A.cravel_int64();
        for (ShapeItem a = 1; a < ec_A; ++a)
            {
              if (pA[a-1] >= pA[a])
                 {
                   MORE_ERROR() << "A⍸B: Bad value of argument A"
                                   " (expecting ascending order)";
                   DOMAIN_ERROR;
                 }
            }
        const int64_t * pB = B.cravel_int64();
        Value_P Z(B.get_shape(), LOC);
        int64_t * pZ = reinterpret_cast<int64_t *>(&Z->get_wfirst());
        loop(b, ec_B)
            {
              const int64_t * pos = std::upper_bound(pA, pA + ec_A, pB[b]);
              pZ[b] = (pos - pA) - 1 + qio;
            }
        Z->commit_ravel_Int64(ec_B);
        Z->set_proto_Int();
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // generic sort check
   for (ShapeItem a = 1; a < ec_A; ++a)
       {
         Cell c1_cache;
         const Cell & c1 = A.get_cravel(a-1, c1_cache);
         Cell c2_cache;
         const Cell & c2 = A.get_cravel(a, c2_cache);
         const Comp_result c1_c2 = c1.compare(c2);
         if (c1_c2 != COMP_LT)
            {
              MORE_ERROR() << "A⍸B: Bad value of argument A"
                              " (expecting ascending order)";
              DOMAIN_ERROR;
            }
       }

Value_P Z(B.get_shape(), LOC);

   // find_range() below fetches from A while its cell argument (fetched
   // from B here) is still live; for V⍸V, A and B are the same Value, so
   // a raw reference from the shared no-cache fetch (plain B.get_cravel(b))
   // used to get clobbered by the first A.get_cravel() inside find_range()
   // -- same aliasing bug as A⍳B above, and the same fix: materialize into
   // caller-owned storage (the generic sort check a few lines above this
   // function already does this with its own c1_cache).
   loop(b, ec_B)
      {
        Cell key_B_cache;
        const Cell & key_B = B.get_cravel(b, key_B_cache);
        const ShapeItem z = find_range(key_B, A, ec_A);
        Z->next_ravel_Int(z + qio);
      }

   Z->set_proto_Int();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
/** return { (,⍵) / ,⍳⍴⍵ }. ⍸B is similar to ⍳B except that:
    * ⍸Z is a vector ⍴,B instead of an array with shape ⍴B, and'
    * the n_th index ist repeated (,B)[n] times instead of once (and
      therefore vanish if (,B)[n] is 0).

   (⍸S⍴1) ≡ ,⍳S  for all shapes S = ⍴ S1
 **/

Token
Bif_F12_INTERVAL_INDEX::eval_B(cValue_R B) const
{
   // B must be boolean. Che that and count the number of 1s in B.
   //
const APL_Integer qio = Workspace::get_IO();
const ShapeItem ec_B = B.element_count();

   // BOOL 1D fast path: __builtin_popcountll + __builtin_ctzll per word
   if (B.get_ravel_type() == RPT_BOOL && B.get_rank() == 1 && ec_B > 0)
      {
        const uint64_t * pB = B.cravel_bool();
        const ShapeItem words = (ec_B + 63) / 64;

        // count set bits; mask last word to ignore tail bits beyond ec_B
        ShapeItem count = 0;
        loop(w, words - 1)   count += __builtin_popcountll(pB[w]);
        { const uint64_t _tm = (ec_B & 63) ? ((uint64_t(1) << (ec_B & 63)) - 1)
                                           : UINT64_MAX;
          count += __builtin_popcountll(pB[words - 1] & _tm); }

        Value_P Z(count, LOC);
        int64_t * pZ = reinterpret_cast<int64_t *>(&Z->get_wfirst());
        ShapeItem z = 0;

        // extract positions of set bits
        for (ShapeItem w = 0; w < words; ++w)
            {
              uint64_t word = pB[w];
              const ShapeItem base = w * 64;
              while (word)
                  {
                    const int bit = __builtin_ctzll(word);
                    const ShapeItem pos = base + bit;
                    if (pos < ec_B)   pZ[z++] = pos + qio;
                    word &= word - 1;   // clear lowest set bit
                  }
            }

        Z->commit_ravel_Int64(count);
        Z->set_proto_Int();
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

ShapeItem count = 0;
   loop(b, ec_B)
       {
         if (!B.is_near_int(b))
            {
              MORE_ERROR() << "⍸B: Bad type of argument B"
                              " (expecting integers)";
              DOMAIN_ERROR;
            }

          const APL_Integer Bi = B.get_near_int(b);
          if (Bi < 0)
            {
              MORE_ERROR() << "⍸B: Bad value of argument B (expecting B ≥ 0)";
              DOMAIN_ERROR;
            }

         // Bugs9 #2 (Blake McBride): count += Bi with no overflow check let
         // e.g. 4×4611686018427387904 (= 2^64 ≡ 0) allocate Z with zero
         // cells while the fill loop below still performs 2^62
         // next_ravel_Int() calls -- heap overflow, SIGSEGV.
         //
         // NOTE: the obvious fix -- form count+Bi first and check the
         // result via Cell::sum_overflow(), the pattern
         // Bif_REDUCE::replicate() uses -- does NOT reliably work here:
         // forming an overflowing signed sum is undefined behaviour, and
         // at -O2 the compiler silently optimizes the subsequent check
         // away based on the assumption that the overflow it just
         // computed cannot have happened (confirmed empirically: the
         // exact same Cell::sum_overflow() call, given the exact same
         // already-overflowed inputs, returns true in an isolated test
         // program but false here). Bi and count are both already known
         // >= 0 at this point (Bi < 0 was rejected above), so check
         // *before* adding, using only well-defined subtraction/compare.
         //
         if (Bi > LARGE_INT - count)   WS_FULL;
         count += Bi;
       }

Value_P Z(count, LOC);

const uRank rank = B.get_rank();
   loop(b, ec_B)
       {
         const APL_Integer Bi = B.get_near_int(b);   // number of repetitions
         if (!Bi)   continue;   // nothing to do

        const Shape sh_b = B.get_shape().offset_to_index(b, qio);
        Assert(sh_b.get_rank() == rank);
         loop(rep, Bi)
             {
               if (rank == 1)   // simple shape. NOT a scalar B: the index
                                // of its only item is ⍬ (Bugs35 #28)
                  {
                    Z->next_ravel_Int(b + qio);
                  }
               else             // nested shape
                  {
                     Value_P ZZ(rank, LOC);
                     loop(r, rank)
                         {
                           const ShapeItem sh_r = sh_b.get_shape_item(r);
                           ZZ->next_ravel_Int(sh_r);
                         }
                     ZZ->set_proto_Int();
                     ZZ->check_value(LOC);
                     Z->next_ravel_Pointer(ZZ.get());
                  }
             }
       }

   if (rank != 1 && Z->is_empty())
      {
        // Z's prototype item must be a pointer to a rank-item zero vector
        // (Blake McBride, Bugs28 #64), matching a real item (see the
        // "nested shape" branch above) instead of Z->set_proto_Int()'s
        // plain IntCell 0 -- e.g. Z←⍸2 2⍴0 must have ≡Z 2 and ⍴↑Z (2 2),
        // not ≡Z 1 and ⍴↑Z ⍬.
        //
        Value_P ZZ(rank, LOC);
        loop(r, rank)   ZZ->next_ravel_Int(0);
        ZZ->set_proto_Int();
        ZZ->check_value(LOC);
        new (&Z->get_wproto()) PointerCell(ZZ.get(), *Z);
      }
   else
      {
        Z->set_proto_Int();
      }
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
ShapeItem
Bif_F12_INTERVAL_INDEX::find_range(const Cell & cell, cValue_R A,
                                   ShapeItem range_count)
{
   // first check if cell is below or above it
   //
   {
     Cell cache;
     const Comp_result c0 = cell.compare(A.get_cfirst(cache));
     if (c0 == COMP_LT)   return -1;   // == 0 with ⎕IO = 1

     const Comp_result cN = cell.compare(A.get_cravel(range_count - 1, cache));
     if (cN != COMP_LT)   return range_count-1;
   }

   // divide and conquer the ranges
   //
ShapeItem ret = 0;
   while (range_count > 1)
         {
           const ShapeItem middle = range_count >> 1;
           Cell cache;
           const Comp_result cm = cell.compare(A.get_cravel(ret + middle, cache));
           if (cm == COMP_LT)   // cell is below the middle
              {
                range_count = middle;
              }
           else                 // cell is above the middle
              {
                ret         += middle;
                range_count -= middle;
              }
         }


   return ret;
}
//════════════════════════════════════════════════════════════════════════════

