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
#include "Bif_F12_PARTITION_PICK.hh"
#include "Bif_OPER1_EACH.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Workspace.hh"

// primitive function instances
//
Bif_F12_TAKE      Bif_F12_TAKE     ::fun;    // ↑
Bif_F12_DROP      Bif_F12_DROP     ::fun;    // ↓

//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_TAKE::eval_AB(cValue_R A, cValue_R B) const
{
   ArgCheck::require_scalar_or_vector("A↑B", "A", A);
   if (A.element_count() > MAX_RANK)
      {
        MORE_ERROR() << "A↑B: A has " << A.element_count()
                     << " items; ⍴⍴Z cannot exceed " << MAX_RANK;
        LIMIT_ERROR_RANK;
      }

Shape ravel_A1(A, /* ⎕IO */ 0);   // checks 1 ≤ ⍴⍴A and ⍴A ≤ MAX_RANK

   if (B.is_scalar())
      {
        Shape shape_B1;
        loop(a, ravel_A1.get_rank())   shape_B1.add_shape_item(1);
        // B.clone(), NOT the CLONE() macro: under NEW_CLONE, CLONE(&B, LOC)
        // is just another Value_P to the *same* Value (see Value.hh), so
        // set_shape() below would permanently reshape the caller's B.
        Value_P B1 = B.clone(LOC);
        B1->set_shape(shape_B1);

        // the clone must remain a left value, like B: otherwise e.g.
        // (∊¨0↑V)←9 for a scalar V was a SYNTAX ERROR, while the same
        // for a vector V is a no-op (Blake McBride, Bugs32 #17).
        //
        if (B.is_left_value())   B1->set_left_value();
        return Token(TOK_APL_VALUE1, do_take(ravel_A1, *B1, false));
      }
   else
      {
        if (ravel_A1.get_rank() != B.get_rank())
           {
             MORE_ERROR() << "A↑B: expecting ⍴⍴B = " << ravel_A1.get_rank()
                          << " (⍴A); ⍴⍴B is " << B.get_rank();
             LENGTH_ERROR;
           }
        return Token(TOK_APL_VALUE1, do_take(ravel_A1, B, false));
      }
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_TAKE::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   ArgCheck::require_scalar_or_vector("A↑[X]B", "A", A);
   ArgCheck::require_scalar_or_vector("A↑[X]B", "X", X);

const ShapeItem len_A = A.element_count();
const ShapeItem len_X = X.element_count();
   if (len_A != len_X)
      {
        UCS_string & more = MORE_ERROR();
        more << "A↑[X]B: expecting ⍴A = ⍴X; ⍴A is ";
        ArgCheck::append_shape(more, A);
        more << ", ⍴X is ";
        ArgCheck::append_shape(more, X);
        LENGTH_ERROR;
      }

   if (len_X == 0)   // no axes
      {
        // Bugs31 #19/#23 (Blake McBride): CLONE() aliases B rather than
        // copying it, so any lvalue pass-through marker on B would
        // otherwise survive this identity application unbroken -- see
        // Value::clear_lval_markers().
        //
        Value_P Z = CLONE(&B, LOC);
        Z->clear_lval_markers();
        Token result(TOK_APL_VALUE1, Z);
        return result;
      }

   // construct the left argument of Bif_F12_TAKE::fill(). Start with ⍴B and
   // then replace corresponding shape items with A[X[x]].
   //
const APL_Integer qio = Workspace::get_IO();
const AxesBitmap axes_X = X.to_bitmap("A↑[X]B", B.get_rank());
Shape sh_take = B.get_shape();   // start with ⍴B
   loop(x, len_X)                 // for exery axis X[x] in X
      {
        const APL_Integer axis = X.get_near_int(x) - qio;
        const APL_Integer alen = A.get_near_int(x);
        sh_take.set_shape_item(axis, alen);
      }

   return Token(TOK_APL_VALUE1, do_take(sh_take, B, axes_X));
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_TAKE::eval_XB(cValue_R X, cValue_R B) const
{
   // ↑[X] B    ←→ ((⍴X)⍴1)↑[X] B   (GNU APL only)
   if (X.get_rank() > 1)
      {
        MORE_ERROR() << "↑[X]B: X must be a scalar or vector; ⍴⍴X is "
                     << X.get_rank();
        AXIS_ERROR;
      }

const AxesBitmap axes_X = X.to_bitmap("↑[X]B", B.get_rank());

   // construct the left argument of Bif_F12_TAKE::fill(). Start with ⍴B and
   // then replace corresponding shape items with 1.
   //
Shape sh_take = B.get_shape();   // start with ⍴B
   loop(b, B.get_rank())         // for exery axis X[x] in X
       {
         if (axes_X & 1 << b) sh_take.set_shape_item(b, 1);
       }

   return Token(TOK_APL_VALUE1, do_take(sh_take, B, axes_X));
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_F12_TAKE::do_take(const Shape & ravel_A1, const cValue & B,
                      AxesBitmap axes)
{
   // ravel_A1 can have negative items (for take from the end).
   //
Value_P Z(ravel_A1.abs(), LOC);

   if (ravel_A1.is_empty())
      {
        Z->set_default(B, LOC);
      }
   else
      {
        // 1D packed fast path for numeric types (fill element is 0)
        const ShapeItem bpi   = B.packed_bytes_per_item();
        const RavelType rtype = B.get_ravel_type();
        const bool zero_fill  = rtype == RPT_INT64
                             || rtype == RPT_FLOAT64
                             || rtype == RPT_COMPLEX;
        if (ravel_A1.get_rank() == 1 && B.get_rank() == 1
            && bpi > 0 && zero_fill && axes == 0)
           {
             const ShapeItem take_n = ravel_A1.get_shape_item(0);  // signed
             const ShapeItem len_Z  = Z->element_count();           // |take_n|
             const ShapeItem len_B  = B.element_count();
             const ShapeItem copy_n = len_Z < len_B ? len_Z : len_B;
             const ShapeItem fill_n = len_Z - copy_n;
             const char * pB = static_cast<const char *>(B.cravel_packed());
             char * pZ = reinterpret_cast<char *>(&Z->get_wfirst());
             if (take_n >= 0)
                {
                  if (copy_n > 0) memcpy(pZ,                   pB, copy_n * bpi);
                  if (fill_n > 0) memset(pZ + copy_n * bpi, 0, fill_n * bpi);
                }
             else
                {
                  const ShapeItem start = len_B - copy_n;
                  if (fill_n > 0) memset(pZ,                   0,  fill_n * bpi);
                  if (copy_n > 0) memcpy(pZ + fill_n * bpi, pB + start * bpi,
                                         copy_n * bpi);
                }
             Z->commit_ravel_like(B, len_Z);
           }
        else
           {
             fill(ravel_A1, *Z, B, axes);
           }
      }
   if (B.is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_F12_TAKE::fill(const Shape & shape_Zi, Value & Z,
                   const cValue & B, AxesBitmap axes)
{
   for (TakeDropIterator i(true, shape_Zi, B.get_shape()); i.has_more(); ++i)
       {
         const ShapeItem offset = i();
         if (offset != -1)                          // valid cell
            {
              Cell cache;
              Z.next_ravel_Cell(B.get_cravel(offset, cache));
            }
         else if (axes && B.element_count())         // overtake from axis
            {
              // (only when B actually has data -- if B is completely
              // empty then axis_proto()'s offset does not point to any
              // real item and falls through to the ↑B case below)
              const ShapeItem offset = i.axis_proto(axes);
              Cell cache;
              Z.next_ravel_Proto(B.get_cravel(offset, cache));
            }
         else                                       // overtake from ↑B
            {
              Cell cache;
              Z.next_ravel_Proto(B.get_cproto(cache));
            }
       }
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_F12_TAKE::first(const cValue & B)
{
   /*
      lrm p. 131: ⍴ Z ← ↑ B depends on the shape of the first item.

      IBM APL2:
          A.  ↑'DO' 'RE' 'MI'   is: 'DO' with shape 2
          B.  ↑⊂'DO'       also is: 'DO' with shape 2

      Therefore: S ≡ ↑S ≡ ↑⊃S for simple scalars S,
            but: V ≢ ↑V ≡ ⊃V[⎕IO]

      We handle the nested first_B case first because all others return scalars
    */
Cell cache;
const Cell & first_B = B.get_cfirst(cache);
   if (Value_P sub = first_B.try_pointer_value())   // first item is nested
      {
        return CLONE_P(sub, LOC);
      }

   if (B.element_count() ||      // the normal case,         e.g. Z ← ↑ 1 2 3
       first_B.is_lval_cell())   // selective specification, e.g. (↑ B) ← V
      {
        Value_P Z(LOC);   // the (always) scalar result of ↑B
        Z->next_ravel_Cell(first_B);
        if (B.is_left_value())   Z->set_left_value();
        Z->check_value(LOC);

        // (↑B)←V replaces B's first item wholesale, like Pick, not a
        // shape-conforming per-cell copy -- IBM APL2's own example
        // (↑'DO' 'RE' 'MI') is 'DO' with shape 2, an item that may well
        // differ in shape/length from the replacement V (Bugs30 #16
        // sibling: (↑K)←'YELLOW' for K←'RED' 'WHITE' 'BLUE' must keep
        // succeeding even though 'YELLOW' is longer than 'RED'). Mark
        // it the same way Bif_F12_PICK::pick() marks its own direct,
        // unbroken selection, so Value::assign_cellrefs() gives it the
        // wholesale-replace treatment instead of ordinary conformance
        // checking now that dest_count == 1 alone no longer implies
        // that (see Bugs30 #13/#16's own removal of that blanket
        // exception).
        //
        // Only when B.get_lval_cellowner() actually finds one (i.e. B is
        // one symbol's own uniform lvalue array): B built by ⊃/∊/¨ (Blake
        // McBride, Bugs31 #1, e.g. V←(1 2)(3 4) ⋄ (↑⊃V)←9) has no single
        // such owner, but first_B is still a perfectly good LvalCell with
        // its own, individually correct owner -- leaving the pick-slot
        // marker unset here just falls through to Value::assign_cellrefs()'s
        // ordinary per-cell path below, which already handles that
        // correctly (same as the plain control V←1 2 3 ⋄ (↑V)←9).
        //
        if (first_B.is_lval_cell())
           if (Cell * target = first_B.get_lval_value())
              if (Value * owner = B.get_lval_cellowner())
                 Z->set_lval_pick_slot(target, owner);

        return Z;
      }

   // ↑ '' or ↑ ⍬ : return prototype
   //
   if (first_B.is_character_cell())   return CharScalar(UNI_SPACE, LOC);
   else                               return IntScalar(0, LOC);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_DROP::eval_AB(cValue_R A, cValue_R B) const
{
   ArgCheck::require_scalar_or_vector("A↓B", "A", A);
   if (A.element_count() > MAX_RANK)
      {
        MORE_ERROR() << "A↓B: A has " << A.element_count()
                     << " items; ⍴⍴Z cannot exceed " << MAX_RANK;
        LIMIT_ERROR_RANK;
      }

const Shape ravel_A(A, /* ⎕IO */ 0);

   if (B.is_scalar())
      {
        /*
           A scalar B is taken as ((⍴⍴B)⍴1)⍴B.

           Z←A↓B with scalar B has an element count() of either 0 or 1:

           If all items of A are are 0 then nothing is dropped) and therefore
           the result is the scalar B.

            Otherwise at least one axis A[j] is ≠ 0 dropped and the result
            is empty.

           ⍴⍴Z is ⍴,A and ⍴Z[j] ←→  0 ⌈ (⍴B)[j] - ∣ A[j]
         */
        Shape shape_Z;
        loop(r, ravel_A.get_rank())
            {
               // (⍴Zi)[r] r'th either 1 (if nothing is dropped)
               // or else 1 (possibly overdrop).
               if (ravel_A.get_shape_item(r))   shape_Z.add_shape_item(0);
               else                             shape_Z.add_shape_item(1);
            }

        Value_P Z(shape_Z, LOC);

        Cell cache;
        Z->set_ravel_Cell(0, B.get_cfirst(cache));
        if (shape_Z.get_volume() == 0)
           {
             if (B.is_left_value())
                {
                  // Bugs31 #25 (Blake McBride): to_type(false) below
                  // would rewrite Z's just-copied LvalCell (B's own, via
                  // get_cfirst()) into a plain type-appropriate cell,
                  // leaving Z marked set_left_value() (a claimed
                  // selective-assignment target, set just below) with a
                  // prototype that is no longer an LvalCell at all --
                  // check_lval_consistency() (now called before
                  // assign_cellrefs()'s dest_count==0 early return,
                  // Bugs30 #29) correctly rejects that contradiction as
                  // a malformed specification. (1↓V)←9 on a scalar V
                  // over-drops to nothing, exactly like the already-
                  // working vector over-drop (5↓V)←9 -- a clean empty-
                  // selection no-op, not an error. Use the same
                  // invalid/placeholder LvalCell(0,0) pattern used
                  // elsewhere for this "no real cell to select" case.
                  //
                  new (&Z->get_wproto()) LvalCell(0, 0);
                }
             else
                {
                  Z->to_type(false);
                }
           }
        if (B.is_left_value())   Z->set_left_value();
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (ravel_A.get_rank() != B.get_rank())
      {
        MORE_ERROR() << "A↓B: expecting ⍴⍴B = " << ravel_A.get_rank()
                     << " (⍴A); ⍴⍴B is " << B.get_rank();
        LENGTH_ERROR;
      }

Shape shape_Z;
   loop(r, ravel_A.get_rank())
       {
         const ShapeItem sA = ravel_A.get_shape_item(r);    // A[r]
         const ShapeItem sB = B.get_shape_item(r);         // (⍴B[r]
         // over-drop if |sA| ≥ sB. Not -sA, which overflows for the
         // int64 minimum (Bugs35 #35).
         if (sA >= sB || sA <= -sB)   shape_Z.add_shape_item(0);
         else   shape_Z.add_shape_item(sB - (sA < 0 ? -sA : sA));
       }

Value_P Z(shape_Z, LOC);
   if (shape_Z.is_empty())   // empty Z, e.g. from overdrop
      {
        // was a second, inner 'Value_P Z(shape_Z, LOC);' here, shadowing
        // (and silently discarding) the outer Z just constructed above
        // with the identical shape_Z -- redundant allocation, not a
        // correctness bug (the inner Z was used consistently within its
        // own scope), but pointless. Reuse the outer Z.
        Z->set_default(B, LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // 1D packed fast path: result is a contiguous slice of B's ravel
   const ShapeItem bpi = B.packed_bytes_per_item();
   if (B.get_rank() == 1 && bpi > 0)
      {
        const ShapeItem sA    = ravel_A.get_shape_item(0);
        const ShapeItem start = sA > 0 ? sA : 0;
        const ShapeItem len_Z = shape_Z.get_shape_item(0);
        const char * pB = static_cast<const char *>(B.cravel_packed());
        char * pZ = reinterpret_cast<char *>(&Z->get_wfirst());
        memcpy(pZ, pB + start * bpi, len_Z * bpi);
        Z->commit_ravel_like(B, len_Z);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   for (TakeDropIterator i(false, ravel_A, B.get_shape()); i.has_more(); ++i)
      {
        const ShapeItem offset = i();
        Cell cache;
        Z->next_ravel_Cell(B.get_cravel(offset, cache));
      }

   if (B.is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_DROP::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   // Bugs28 #67: A must be type- and length-checked against X even when X
   // is empty -- the "no axes" shortcut used to run first, so e.g.
   // 'abc'↓[⍬]M (a non-scalar-or-vector, wrongly typed A) or 2↓[⍬]M (⍴A ≠
   // ⍴X) silently returned B unchanged instead of failing, unlike
   // Bif_F12_TAKE::eval_AXB just above, which checks first.
   //
   ArgCheck::require_scalar_or_vector("A↓[X]B", "A", A);
   ArgCheck::require_scalar_or_vector("A↓[X]B", "X", X);

const ShapeItem len_X = X.element_count();
const ShapeItem len_A = A.element_count();
   if (len_A != len_X)
      {
        UCS_string & more = MORE_ERROR();
        more << "A↓[X]B: expecting ⍴A = ⍴X; ⍴A is ";
        ArgCheck::append_shape(more, A);
        more << ", ⍴X is ";
        ArgCheck::append_shape(more, X);
        LENGTH_ERROR;
      }

   if (len_X == 0)   // no axes
      {
        // Bugs31 #19/#23: see the matching A↑[X]B case above.
        //
        Value_P Z = CLONE(&B, LOC);
        Z->clear_lval_markers();
        Token result(TOK_APL_VALUE1, Z);
        return result;
      }

   // to_bitmap() also (redundantly but harmlessly) re-checks X's rank
   // and validates every axis is integral, in range, and unique -- so
   // the loop below no longer needs its own range/duplicate tracking.
   //
   X.to_bitmap("A↓[X]B", B.get_rank());

const APL_Integer qio = Workspace::get_IO();

   // init ravel_A = shape_B.
   //
Shape ravel_A(B.get_shape());

   loop(r, len_X)
       {
         const APL_Integer a = A.get_near_int(r);
         const APL_Integer x = X.get_near_int(r) - qio;

         const ShapeItem amax = B.get_shape_item(x);
         if      (a >= amax)   ravel_A.set_shape_item(x, 0);
         else if (a >= 0)      ravel_A.set_shape_item(x, a - amax);
         else if (a > -amax)   ravel_A.set_shape_item(x, amax + a);
         else                  ravel_A.set_shape_item(x, 0);
       }

   return Token(TOK_APL_VALUE1,
                Bif_F12_TAKE::do_take(ravel_A, B, 0));
}
//════════════════════════════════════════════════════════════════════════════
ShapeItem
TakeDropIterator::axis_proto(AxesBitmap axes) const
{
   /* compute the offset for the prototype of current_offset. The APL2
      language reference somehow suggests that for A↑[X] B the axes of B
      shall be enclosed (to give ⊂[X] B) and then the prototype of the
      enclosed sub-array shall be used as the fill item for overtake.

      This would mean that the offsets of the prototype is the sum of the
      weighted offsets of the axes in X. Interestingly we get the output
      shown in the language reference only if we use the sum of the weighted
      offsets of the axes NOT in X.

      The code below implements exactly that (axes NOT in X) -- it used to
      sum the axes IN X instead, contradicting this comment. For any axis
      a in X, ftwc[a].current walks 0..sA-1 as the overtake proceeds, i.e.
      unboundedly past B's real extent along that axis; summing it in still
      produced *a* number (so simple/homogeneous B silently got the right
      answer regardless, since their prototype is a constant 0/space that
      doesn't depend on which offset was read), but that offset was then
      used to index B's own ravel (B.get_cravel(offset) in fill(), a few
      lines above) -- valid while it happened to fall inside whatever
      memory backs B (e.g. its short_value[] inline buffer), and a
      use-after-the-end read (crash observed via the fuzzer, confirmed with
      SHORT_VALUE_LENGTH_WANTED-sized short values) once it didn't.

      Second, independent bug found alongside the above: axes (the bitmap
      passed in) numbers axes the ordinary/natural way (bit 0 = first/
      highest axis, per cValue::to_bitmap()), but ftwc[] is indexed the
      *transposed* way used throughout this class (index 0 = last/lowest
      axis, per Shape::get_transposed_shape_item()) -- e.g. for a rank-2 B,
      natural axis 0 (rows) is ftwc[1], and natural axis 1 (columns) is
      ftwc[0]. Using the loop variable as both a bit position into axes
      and an index into ftwc[] without converting between the two
      conventions reads the wrong axis's current/weight entirely (verified
      directly: for a 2x3 B overtaken along axis 1/rows past row 2, this
      produced offsets 6, 9, 12 -- multiples of 3, i.e. ftwc[1]'s (the
      *row* axis's, the one being overtaken, not excluded) weight -- while
      B only has 6 elements, so every one of those was already an
      out-of-bounds read into B's ravel).
    */
ShapeItem ret = 0;
   loop(nat, ref_B.get_rank())
      {
        if (!(axes & 1 << nat))   // natural axis nat NOT in X
           {
             const sAxis transposed = ref_B.get_rank() - 1 - nat;
             const _ftwc & ftwc_a = ftwc[transposed];
             ret += ftwc_a.current * ftwc_a.weight;
           }
      }

   return ret;
}
//════════════════════════════════════════════════════════════════════════════

