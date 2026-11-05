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

#include <string.h>

#include "ArgCheck.hh"
#include "Bif_F12_RHO.hh"
#include "StateIndicator.hh"
#include "Value.hh"
#include "Workspace.hh"

// primitive function instance
//
Bif_F12_RHO       Bif_F12_RHO      ::fun;    // ⍴

//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_RHO::eval_AB(cValue_R A, cValue_R B) const
{
#ifdef cfg_PERFORMANCE_COUNTERS_WANTED
const uint64_t start_1 = cycle_counter();
#endif

   // argument check: A (the new shape) must be a scalar or vector of
   // non-negative integers, and short enough that the result does not
   // exceed MAX_RANK axes. Checked here (with a specific message) rather
   // than left to Shape's own constructor below, so that the )MORE text
   // can name A specifically rather than being generic across Shape's
   // several unrelated callers.
   //
   ArgCheck::require_scalar_or_vector("A⍴B", "A", A);

   if (A.element_count() > MAX_RANK)
      {
        MORE_ERROR() << "A⍴B: the result rank (≡⍴A) cannot exceed " << MAX_RANK
                     << "; ⍴A is " << A.get_shape() << " (" << A.element_count()
                     << " items)";
        LIMIT_ERROR_RANK;
      }

   ArgCheck::require_non_negative_ints("A⍴B", "A", A);

const Shape shape_Z(A, 0);

   // NOTE: this function used to have an "optimization of Z<-A/B" here that
   // reshaped B in place and returned it as Z whenever B was a temporary
   // (owner_count==1) value and Z was not longer than B -- avoiding a copy.
   // Removed: it could reshape (shrink) B's *logical* shape even when B was
   // already packed to RPT_BOOL, which is the one case where ~Value()'s
   // RPT_BOOL destructor branch cannot safely deallocate using the value's
   // current (post-shrink) element count -- see the destructor's own
   // comment. Every other call site that packs a value does so as the very
   // last step before returning it, with no further shrink, so removing
   // this one in-place optimization lets the destructor use exactly the
   // same std::allocator<Cell>::deallocate() as the non-packed case,
   // instead of the less clean delete[] override it needed before.
   //
#ifdef cfg_PERFORMANCE_COUNTERS_WANTED
Token ret = do_reshape(shape_Z, B);
const uint64_t end_1 = cycle_counter();
   Performance::fs_F12_RHO_AB.add_sample(end_1 - start_1,
                                         shape_Z.get_volume());
   return ret;
#else
   return do_reshape(shape_Z, B);
#endif
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_RHO::eval_B(cValue_R B) const
{
const sRank rank = B.get_rank();
Value_P Z(rank, LOC);

   {
     int64_t * pZ = reinterpret_cast<int64_t *>(&Z->get_wfirst());
     loop(r, rank)   pZ[r] = B.get_shape_item(r);
     Z->commit_ravel_Int64(rank);
   }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_RHO::do_reshape(const Shape & shape_Z, const cValue & B)
{
const ShapeItem len_B = B.element_count();

Value_P Z(shape_Z, LOC);
const ShapeItem len_Z = Z->element_count();

   if (len_B == 0)   // empty B: use prototype of B for all cells of Z
      {
        Cell cache;
        loop(z, len_Z)   Z->next_ravel_Proto(B.get_cproto(cache));
      }
   else
      {
        const ShapeItem bpi = B.packed_bytes_per_item();   // 0 for CELLS or BOOL
        if (bpi > 0 && len_Z > 0)
           {
             // packed ravel: copy B cycling over Z with memcpy
             const char * pB = static_cast<const char *>(B.cravel_packed());
             char * pZ = reinterpret_cast<char *>(&Z->get_wfirst());
             ShapeItem done = 0;
             while (done < len_Z)
                 {
                   const ShapeItem chunk = len_Z - done < len_B ? len_Z - done : len_B;
                   memcpy(pZ + done * bpi, pB, chunk * bpi);
                   done += chunk;
                 }
             Z->commit_ravel_like(B, len_Z);
           }
        else if (B.is_bool_packed() && len_Z > 0)
           {
             // BOOL fast path: cyclic bit copy without exploding B
             const uint64_t * pB = B.cravel_bool();
             uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z->get_wfirst());
             const ShapeItem words_Z = (len_Z + 63) / 64;
             memset(pZ, 0, words_Z * sizeof(uint64_t));
             ShapeItem sb = 0;
             for (ShapeItem i = 0; i < len_Z; ++i)
                {
                  pZ[i >> 6] |= ((pB[sb >> 6] >> (sb & 63)) & 1) << (i & 63);
                  if (++sb == len_B) sb = 0;
                }
             Z->commit_ravel_Bool(len_Z);
           }
        else
           {
             Cell cache;
             loop(z, len_Z)
               {
                 Z->next_ravel_Cell(B.get_cravel(z % len_B, cache));
               }
           }
      }

   Z->set_default(B, LOC);
   if (B.is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   Z->try_pack();
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
