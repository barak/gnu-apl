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
#include <unordered_set>

#include "Bif_F12_ELEMENT.hh"
#include "ConstCell_P.hh"
#include "LvalCell.hh"
#include "Value.hh"
#include "Workspace.hh"

// primitive function instance
//
Bif_F12_ELEMENT   Bif_F12_ELEMENT  ::fun;    // ϵ

//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_ELEMENT::eval_AB(cValue_R A, cValue_R B) const
{
   // return Z←A ϵ B. Z[i] is 1 iff A[i] = B[j] for some j
   //
const double qct = Workspace::get_CT();
Value_P Z(A.get_shape(), LOC);

const ShapeItem len_A = A.element_count();
   if (len_A == 0)
      {
        Z->set_default(B, LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // INT64 × INT64 fast path: hash set lookup (exact), BOOL result. Only
   // if exact and tolerant comparison agree (Bugs35 #34).
   if (A.get_ravel_type() == RPT_INT64 && B.get_ravel_type() == RPT_INT64 &&
       IntCell::ints_compare_exactly(A.cravel_int64(), len_A, qct) &&
       IntCell::ints_compare_exactly(B.cravel_int64(), B.element_count(),
                                     qct))
      {
        const int64_t * pA = A.cravel_int64();
        const int64_t * pB = B.cravel_int64();
        const ShapeItem len_B = B.element_count();

        std::unordered_set<int64_t> set_B;
        // matching Bif_F2_INTER::eval_AB()'s guard: a bare reserve() on a
        // very large B raises a C++ std::bad_alloc instead of the APL
        // WS FULL its sibling produces (Bugs18 #16, same class here).
        // catch (...) rather than catch (std::bad_alloc &) alone: an
        // over-large reserve() may also throw std::length_error, which
        // is likewise an allocation-site failure and should also mean
        // WS FULL, not FIXME's internal-error abort (Blake McBride,
        // Bugs20 #8c).
        //
        try { set_B.reserve(len_B * 2); }
        catch (...) { WS_FULL; }
        loop(b, len_B)   set_B.insert(pB[b]);

        uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z->get_wfirst());
        const ShapeItem words = (len_A + 63) / 64;
        loop(w, words)   pZ[w] = 0;

        loop(a, len_A)
            {
              if (set_B.count(pA[a]))
                 pZ[a >> 6] |= uint64_t(1) << (a & 63);
            }

        Z->commit_ravel_Bool(len_A);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   for (ConstRavel_P a(A, true); +a; ++a)
       {
         APL_Integer found_a_in_B = 0;
         for (ConstRavel_P b(B, true); +b; ++b)
             {
               if (a->equal(*b, qct))   { found_a_in_B = 1;   break; }
             }

         Z->next_ravel_Int(found_a_in_B);
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Bif_F12_ELEMENT::do_eval_B(cValue_R B)
{
   // enlist
   //
   // lrm p. 118, ⍴⍴Z = 1, ⍴Z = number of simple scalars in B
   //
   if (B.element_count() == 0)   // empty argument
      {
        // ⍴⍴Z = 1, ⍴Z = number of simple scalars in B -- 0 for an empty
        // B (ISO 13751 §8.2.6: 0⍴∊↑,B), not 1: this used to build a
        // ONE-element vector holding the prototype value instead of a
        // genuinely EMPTY vector carrying that same value only as its
        // (unraveled) prototype -- matching the LvalCell branch below,
        // which already got this right. See Bugs27 #29.
        //
        Cell cache_C0;
        const Cell & C0 = B.get_cproto(cache_C0);
        if (C0.is_numeric())
           {
             Value_P Z(ShapeItem(0), LOC);
             new (&Z->get_wproto()) IntCell(0);
             Z->check_value(LOC);
             return Z;
           }

        if (C0.is_character_cell())
           {
             Value_P Z(ShapeItem(0), LOC);
             new (&Z->get_wproto()) CharCell(UNI_SPACE);
             Z->check_value(LOC);
             return Z;
           }

        if (C0.is_lval_cell())
           {
             // (∈⍬)←value is a noop
             //
             Value_P Z(ShapeItem(0), LOC);
             new (&Z->get_wproto()) LvalCell(0, 0);
             Z->check_value(LOC);
             return Z;
           }

        if (Value_P v = C0.try_pointer_value())
            {
             // v (the prototype's own pointee, e.g. the 2 3⍴'a' inside
             // 0⍴⊂2 3⍴'a') is generally NOT itself empty, so recursing
             // via do_eval_B(*v) unconditionally enlisted v's real
             // content (e.g. 6 characters) -- right prototype TYPE, but
             // wrong: the OUTER B was empty, so the result must still
             // have 0 elements. Recurse only to learn what a nested
             // enlist's own prototype cell would be, then build a
             // genuinely empty vector carrying that as its prototype.
             //
             Value_P Z1 = do_eval_B(*v);
             Cell cache;
             const Cell & proto = Z1->get_cproto(cache);
             Value_P Z(ShapeItem(0), LOC);
             proto.init_other(&Z->get_wproto(), *Z, LOC);
             Z->check_value(LOC);
             return Z;
            }


        // not reached
        //
        FIXME;
      }

   // fast path: B is already packed (no PointerCells); ∊B is a packed vector
   // with the same packing as B — just memcpy the raw packed bytes.
   //
   if (B.is_packed())
      {
        const ShapeItem count = B.element_count();
        Value_P Z(count, LOC);
        uint8_t * pZ = reinterpret_cast<uint8_t *>(&Z->get_wfirst());
        const ShapeItem ebytes = B.packed_bytes_per_item();
        // bool: copy whole 64-bit words, not just ceil(count/8) bytes --
        // a RPT_BOOL ravel is addressed (and its "tail bits are always
        // zero" invariant maintained, see ScalarFunction.hh) as uint64_t
        // words, but the destination here is freshly allocated, non-
        // zeroed memory; copying only the partial last byte would leave
        // bits [count..63] of the last word as uninitialised garbage
        // instead of the required zero (Blake McBride, Bugs21 #5). The
        // source ravel is always allocated to a whole-word size, so the
        // wider read stays in bounds.
        const ShapeItem nbytes = ebytes > 0 ? count * ebytes
                                            : (count + 63) / 64 * 8;
        memcpy(pZ, B.cravel_packed(), nbytes);
        Z->commit_ravel_like(B, count);
        Z->check_value(LOC);
        return Z;
      }

const ShapeItem len_Z = B.get_enlist_count();

   // B contains no simple scalars (e.g. ∊⊂''): recurse into the first
   // enclosed value to determine the prototype and return an empty result.
   //
   if (len_Z == 0)
      {
        // B holds LvalCells (selective assignment, e.g. (∊⊂'')←5): there is
        // no enclosed Value_P to recurse into (try_pointer_value() is 0 for
        // every LvalCell), so mirror the empty-B lval branch above -- 0
        // items selected is a no-op, not an internal error.
        //
        if (B.get_lval_cellowner())
           {
             Value_P Z(ShapeItem(0), LOC);
             new (&Z->get_wproto()) LvalCell(0, 0);
             Z->set_left_value();
             Z->check_value(LOC);
             return Z;
           }

        loop(c, B.element_count())
           {
             if (Value_P v = B.try_pointer_value(c))
                return do_eval_B(*v);
           }

        // Neither an lvalue nor any enclosed item to recurse into. Not
        // reachable via a plain ∊⍬ / ∊(0⍴0) (those take len_Z==0 through a
        // different path before ever getting here), but reachable via e.g.
        // (∊¨0↑V)←9 (Blake McBride, Bugs31 #4): EACH's own empty-B handling
        // substitutes a synthetic single-item prototype (built by
        // Bif_F12_TAKE::first()) for its cellrefs before calling LO here.
        // An empty top-level B -- however it got here -- selects 0 items,
        // full stop; that is always a legitimate (if degenerate) selective-
        // assignment target, not a distinct error case, so return the exact
        // same empty-lval no-op as the B.get_lval_cellowner() branch above
        // rather than treating "not individually recognizable as one
        // uniform lvalue array" as if it meant something went wrong.
        //
        Value_P Z(ShapeItem(0), LOC);
        new (&Z->get_wproto()) LvalCell(0, 0);
        Z->set_left_value();
        Z->check_value(LOC);
        return Z;
      }

Value_P Z(len_Z, LOC);

   if (B.get_lval_cellowner())
      {
        B.enlist_left(*Z);
        Z->set_left_value();
      }
   else
      {
        B.enlist_right(*Z);
        Z->try_pack(true);   // result has no PointerCells; pack regardless of size
      }

   Assert(len_Z);   // cannot be empty
   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
