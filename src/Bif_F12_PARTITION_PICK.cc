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

#include "ArgCheck.hh"
#include "ArrayIterator.hh"
#include "Bif_F12_PARTITION_PICK.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Bif_F12_TRANSPOSE.hh"
#include "Bif_OPER1_EACH.hh"
#include "Workspace.hh"

Bif_F12_PARTITION Bif_F12_PARTITION::fun;    // ⊂
Bif_F12_PICK      Bif_F12_PICK     ::fun;    // ⊃

//════════════════════════════════════════════════════════════════════════════
Value_P
Bif_F12_PARTITION::do_eval_B(cValue_R B)
{
Value_P Z(LOC);   // Z ← ⊂B is always a scalar

   // Bugs28 #100(s): B is not "simple" (per is_simple_scalar()) when its
   // one cell is an LvalCell -- deliberately, since an LvalCell is not a
   // plain data cell. But that also meant a selective specification
   // like (⊂A)←1 2 3, where B here is A resolved to its own lvalue (a
   // scalar LvalCell pointing back at A), fell into the "nested: clone
   // and copy" branch below, which clones the pointed-to VALUE and
   // discards the live reference -- so Z's own cell became a plain
   // PointerCell, not an LvalCell, and the later selective-assignment
   // code (Value.cc's is_scalar() branch, which requires the target
   // cell to itself be an lvalue) had nothing to write through and
   // rejected it. Enclosing a scalar is defined to be a no-op (⊂ of a
   // scalar returns that scalar unchanged) regardless of whether that
   // scalar happens to be an ordinary value or a live lvalue reference,
   // so copy the cell itself (preserving its lvalue-ness, the same way
   // the "simple" case already does) instead of cloning-and-wrapping.
   //
Cell cache;
const bool is_lval_scalar = B.is_scalar() && B.get_cscalar(cache).is_lval_cell();
   if (B.is_simple_scalar() || is_lval_scalar)   // B is not nested: copy ↑B
      {
        Z->next_ravel_Cell(B.get_cscalar(cache));
        if (B.is_left_value())   Z->set_left_value();

        // (⊂B)←C replaces B's slot wholesale, like Pick/First, not a
        // shape-conforming copy -- ⊂ of a scalar is defined as a no-op
        // regardless of C's shape, e.g. (⊂B)←1 2 3 for a plain scalar B
        // (Bugs30 #13/#16 sibling, same as Bif_F12_TAKE::first()'s own
        // marking just above/below in this file's sibling class): mark
        // it the same way Bif_F12_PICK::pick() marks its own direct,
        // unbroken selection, now that dest_count == 1 alone no longer
        // implies that in Value::assign_cellrefs().
        //
        if (is_lval_scalar)
           if (Cell * target = B.get_cscalar(cache).get_lval_value())
              Z->set_lval_pick_slot(target, B.get_lval_cellowner());
      }
   else                         // B is nested: clone and copy
      {
        Value_P Z0 = B.clone(LOC);
        // clone() copies each cell's own semantics (an LvalCell's clone
        // is another LvalCell pointing at the same live target -- see
        // the Bugs28 #100(s) comment above), so Z0 is just as valid a
        // selective-assignment target as B was; propagate the flag,
        // which clone() does not do on its own (it is not itself
        // selective-spec aware -- see also (⊂V)←5, where V is a
        // multi-element lvalue array and this is the branch taken).
        //
        if (B.is_left_value())   Z0->set_left_value();
        Z->next_ravel_Pointer(Z0.get());
      }
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_PARTITION::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "A⊂[X]B");
   return Token(TOK_APL_VALUE1, partition(A, B, axis));
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Bif_F12_PARTITION::enclose_with_axes(const Shape & sh_X, Value_P B)
{
   // Note: the caller has checked that sh_X contains only valid
   // axes of B, so we do not need to check it again here.

Shape item_shape;
Shape it_weights;
Shape shape_Z;
Shape weights_Z;

   // split ⍴B into two shapes: shape_Z and item_shape. Axes of B that are
   // contained in X go into item_shape (and their order in X matters) while
   // the other axes go into shape_Z.
   //
const Shape weights_B = B->get_shape().get_weights();

AxesBitmap axes_X = 0;   // axes in axes_X with ⎕IO←0
   loop(r, sh_X.get_rank())   // the axes in axes_X
       {
         const ShapeItem ax = sh_X.get_shape_item(r);
         axes_X |= 1 << ax;

         item_shape.add_shape_item(B->get_shape_item(ax));
         it_weights.add_shape_item(weights_B.get_shape_item(ax));
       }

   loop(ax, B->get_rank())        // the axes not in axes_X
       {
         if (axes_X & 1 << ax)   continue;   // ax is in X

         shape_Z.add_shape_item(B->get_shape_item(ax));
         weights_Z.add_shape_item(weights_B.get_shape_item(ax));
       }

   if (item_shape.get_rank() == 0)   // empty axes
      {
        //  ⊂[⍳0]B   ←→   ⊂¨B
        Token part(TOK_FUN1, &Bif_F12_PARTITION::fun);
        return Bif_OPER1_EACH::do_eval_LB(part, *B).get_apl_val();
      }

Value_P Z(shape_Z, LOC);
   if (Z->is_empty())
      {
        // Z's prototype item must have shape item_shape (Blake McBride,
        // Bugs28 #62), not a simple prototype cell copied from B: with
        // e.g. Z←⊂[2]0 3⍴0, item_shape is 3 (non-empty) even though Z
        // itself (shape_Z 0) is empty, so ≡Z must be 2 and ⍴↑Z must be 3.
        //
        Value_P vZ(item_shape, LOC);
        if (item_shape.is_empty())
           {
             vZ->set_default(*B, LOC);
           }
        else
           {
             Cell cache;
             const Cell & B_proto = B->get_cproto(cache);
             loop(i, item_shape.get_volume())
                 vZ->next_ravel_Proto(B_proto);
           }
        vZ->check_value(LOC);
        new (&Z->get_wproto()) PointerCell(vZ.get(), *Z);
        Z->check_value(LOC);
        return Z;
      }

   for (ArrayIterator it_Z(shape_Z); it_Z.has_more(); ++it_Z)
      {
        const Shape it_Z_sh = it_Z.get_shape_offsets();
        ShapeItem off_Z = 0;
        loop(z, it_Z_sh.get_rank())
            off_Z += it_Z_sh.get_shape_item(z)
                   * weights_Z.get_shape_item(z);

        Value_P vZ(item_shape, LOC);
        Z->next_ravel_Pointer(vZ.get());

        if (item_shape.is_empty())
           {
             vZ->set_default(*B, LOC);
           }
        else
           {
             for (ArrayIterator it_it(item_shape); it_it.has_more(); ++it_it)
                 {
                    const Shape it_sh = it_it.get_shape_offsets();
                    ShapeItem off_B = 0;
                    loop(i, item_shape.get_rank())
                        off_B += it_sh.get_shape_item(i)
                               * it_weights.get_shape_item(i);
                   Cell cache;
                   vZ->next_ravel_Cell(B->get_cravel(off_Z + off_B, cache));
                 }
           }
        vZ->check_value(LOC);
      }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_F12_PARTITION::partition(cValue_R A, cValue_R B, sAxis axis)
{
   // A must be a scalar or vector (of non-negative integers), or any
   // 1-element value (Bugs37 #66 scalar-extension rule).
   // B must be non-scalar
   //
   if (!A.is_scalar_extensible())
      ArgCheck::require_scalar_or_vector("A⊂B", "A", A);

   if (B.get_rank() == 0)
      {
        MORE_ERROR() << "A⊂B: B must have rank ≥ 1; B is a scalar"
                        " (⍴⍴B is 0)";
        RANK_ERROR;
      }

const ShapeItem len_A = A.element_count();

   // the length of A shall be 1 (which is then extended to the length of the
   // B axis) or else the length of the B-axis along which the partitioning
   //  is performed.
   //
   // Unlike IBM APL2 we not only extend scalars and one-item vectors but
   // also one-element arrays of rank ≥ 2.
   //
   if (len_A != 1 && len_A != B.get_shape_item(axis))
      {
        MORE_ERROR() << "A⊂B: expecting ⍴A to be 1 or "
                     << B.get_shape_item(axis) << " (the length of B along"
                        " axis " << (axis + Workspace::get_IO())
                     << "); ⍴A is " << A.get_shape() << " (" << len_A
                     << " items), ⍴B is " << B.get_shape();
        LENGTH_ERROR;
      }

   ArgCheck::require_non_negative_ints("A⊂B", "A", A);

   // construct a vector of partitions from A...
vector<Partition> partitions;   // all partitions on the B-axis
   {
     ShapeItem prev_A = 0;
     bool in_partition = false;
     loop(apos, len_A)
         {
           const APL_Integer aval = A.get_near_int(apos);

           if (aval > prev_A)   // new partition starting at apos
              {
                if (in_partition)   partitions.back().end = apos;
                const Partition part = { apos, -1 };
                partitions.push_back(part);
                in_partition = true;
              }
           else if (in_partition && aval == 0)
              {
                partitions.back().end = apos;
                in_partition = false;
              }
           prev_A = aval;
         }

     if (in_partition)   partitions.back().end = len_A;
   }

   // ⍴⍴Z ←→ ⍴⍴B
   // ⍴Z  ←→ (¯1↓⍴B), bm            ( for A ⊂ B )
   // ⍴Z  ←→ (⍴B) ⊢[axis=⍳⍴⍴B] bm   ( for A ⊂[axis] B )
   //
const ShapeItem Zm = partitions.size();   // number of non-0 partitions
Shape shape_Z(B.get_shape());
   shape_Z.set_shape_item(axis, Zm);

Value_P Z(shape_Z, LOC);

   if (Z->is_empty())
      {
        // the length of an item depends only on A (not on the other, empty
        // axes of B): the prototype is the first partition of a B with one
        // row, i.e. (length of partition 0) prototypes of B (Bugs35 #30).
        //
        ShapeItem len = 0;
        if (Zm)
           {
             len = partitions[0].length();
             if (len_A == 1)   len *= B.get_shape_item(axis);
           }
        Value_P ZZ(len, LOC);
        Cell cache;
        const Cell & proto_B = B.get_cproto(cache);
        loop(l, len)   ZZ->next_ravel_Proto(proto_B);
        ZZ->set_default(B, LOC);
        ZZ->check_value(LOC);
        new (&Z->get_wproto()) PointerCell(ZZ.get(), *Z);
        Z->check_value(LOC);
        return Z;
      }

const Shape & shape_B = B.get_shape();
const Shape3 shape_B3(shape_B, axis);
const ShapeItem B3_lm = shape_B3.l() * shape_B3.m();

   // Extend scalars and one-item arrays A to the length of the B-axis
   if (len_A == 1)   partitions[0].end *= shape_B3.m();

   loop(h, shape_B3.h())
   loop(m, Zm)
   loop(l, shape_B3.l())
       {
         const ShapeItem partition_start = partitions[m].start;
         const ShapeItem partition_len   = partitions[m].length();
         const ShapeItem start_B =
                         l + partition_start * shape_B3.l() + h * B3_lm;
         Value_P ZZ(partition_len, LOC);   // the m'th partition
         loop(p, partition_len)
             {
               Cell cache;
               ZZ->next_ravel_Cell(B.get_cravel(start_B + p * shape_B3.l(),
                                                 cache));
             }
         ZZ->check_value(LOC);
         Z->next_ravel_Pointer(ZZ.get());
       }

   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_PICK::eval_AB(cValue_R A, cValue_R B) const
{
   ArgCheck::require_scalar_or_vector("A⊃B", "A", A);

const ShapeItem ec_A = A.element_count();

   // if A is empty, return B
   //
   if (ec_A == 0)   return Token(TOK_APL_VALUE1, CLONE(&B, LOC));

const APL_Integer qio = Workspace::get_IO();

Value_P Z = pick(A, 0, ec_A, B, qio);

   if (B.is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Bif_F12_PICK::disclose(cValue_R B, bool rank_tolerant)
{
   // for simple scalars B: B ≡ ⊂ B and therefore B ≡ ⊃ B
   //
   if (B.is_simple_scalar())   return CLONE(&B, LOC);

   // (⊃Z)←C for a scalar Z: ⊃Z ≡ ↑Z, and ↑ marks the item as replaced
   // wholesale. The per-cell path wrote a simple C into the only cell of a
   // nested scalar item, leaving a PointerCell to a simple scalar in Z
   // (Bugs35 #76).
   //
   if (B.is_scalar() && B.is_left_value())
      {
        Cell cache;
        if (B.get_cscalar(cache).is_lval_cell())
           return Bif_F12_TAKE::first(B);
      }

   // compute item_shape, which is the smallest shape into which each
   // item of B fits, and then ⍴Z ←→ (⍴B), item_shape

const Shape item_shape = compute_item_shape(B, rank_tolerant);
const Shape shape_Z = B.get_shape() + item_shape;

const ShapeItem len_B = B.element_count();
   if (len_B == 0)
      {
        // the cells of a mixed value are the cells of the items of B, so
        // the prototype of Z is the (type of the) first cell of B's
        // prototype item. Disclosing that item once more (as before) lost
        // one level of nesting, and set_shape() on the non-empty
        // disclosure left its surplus cells unreleased (Bugs34 #5).
        //
        Value_P first = Bif_F12_TAKE::first(B);
        Value_P Z(shape_Z, LOC);
        Cell cache;
        Z->set_default(first->get_cproto(cache), LOC);
        if (B.is_left_value())   Z->set_left_value();
        Z->check_value(LOC);
        return Z;
      }

Value_P Z(shape_Z, LOC);

const ShapeItem item_len = item_shape.get_volume();

   if (item_len == 0)   // empty enclosed value
      {
        Cell cache_B0;
        const Cell & B0 = B.get_cproto(cache_B0);
        if (B0.is_pointer_cell())
           {
             // B0's own value has shape item_shape (item_len == 0 can only
             // happen if every nested cell examined by compute_item_shape()
             // has exactly item_shape -- any smaller/larger shape would
             // have changed the running max). Returning it unchanged here
             // used to silently drop B's own (possibly non-scalar) shape:
             // ⍴Z should be (⍴B),item_shape (see the invariant documented
             // above), not just item_shape -- confirmed directly (⊃5⍴⊂⍬
             // gave shape 0 instead of the correct shape 5 0). Clone
             // (B0's value may be shared with other cells) and reshape to
             // shape_Z, which -- like B0's own value -- always has volume
             // 0 here, so this is a safe, allocation-free reshape.
             //
             // NOTE: CLONE_P(x, L) is NOT a clone under NEW_CLONE (see
             // Value.hh) -- it expands to (x), the same Value_P. Using it
             // here left every PointerCell sharing B0's sub-value aliased,
             // so set_shape() below corrupted all of them. vB->clone(LOC)
             // is an actual copy.
             Value_P vB = B0.get_pointer_value();
             Value_P result = vB->clone(LOC);
             if (B.is_left_value())   result->set_left_value();
             result->set_shape(shape_Z);
             return result;
           }
        else   // simple B0
           {
             Z->set_default(B0, LOC);
           }

        if (B.is_left_value())   Z->set_left_value();
        Z->check_value(LOC);
        return Z;
      }

   loop(b, len_B)   // for all items in B...
       {
         Cell cache;
         const Cell & B_item = B.get_cravel(b, cache);
         disclose_item(*Z, b, item_shape, item_len, B_item);
       }

   Z->set_default(B, LOC);
   if (B.is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_F12_PICK::disclose_item(Value & Z, ShapeItem b,
                            const Shape & item_shape, ShapeItem item_len,
                             const Cell & B_item)
{
   if (B_item.is_pointer_cell())
      {
         // B_item points to a nested APL value. Take that value, expand
         // it to item_shape, and store it in Z.
         //
        const Value & vB = *B_item.get_pointer_value();
        Bif_F12_TAKE::fill(item_shape, Z, vB, 0);
        if (vB.is_member())   Z.set_member();
      }
   else if (B_item.is_lval_cell())
      {
        /* B_item is a left-value Cell (pointing to a Cell (target) that
           shall be assigned at a later point in time. This is a litte
           tricky since there are 3 cases:

           1. target == 0, which means that target was padded at an earlier
              point in time (and then nothing shall be done), or

           2. target is a right-value (even though it is left of →. The
              convention for cellrefs has it, that left hand PointerCells are
              not expanded when their parents are (because they are subject to
              be deleted by other primitives, and then the early conversion to
              left-values would not pay of). We expand the target now, expand
              it to item_shape, and store it in Z.

           3. target is a simple Cell, so B_item points a simple Cell. We
              expand B_item to item_shape using LvalCells that point nowhere.

           In all 3 cases, since we are left of ←, the expansion needs to be
           made with LvalCell(0, 0) pointing nowhere rather than with UNI_SPACE
           or with 0.
        */
        Cell * target = B_item.get_lval_value();
        if (target == 0)                           // case 1.
           {
             loop(c, item_len)   Z.next_ravel_Lval(0, 0);
           }
        else if (target->is_pointer_cell())        // case 2.
           {
             Value_P subval = target->get_pointer_value();
             Value_P subrefs = subval->get_cellrefs(LOC);
             Bif_F12_TAKE::fill(item_shape, Z, *subrefs, 0);
           }
        else                                       // case 3.
           {
             Z.next_ravel_Cell(B_item);
             for (ShapeItem c = 1; c < item_len; ++c)
             Z.next_ravel_Lval(0, 0);
           }
      }
   else if (B_item.is_character_cell())   // simple char scalar
      {
        // B_item is a character, so expand it to item_shape with UNI_SPACE,
        //  and store it in Z
        //
        Z.next_ravel_Cell(B_item);
        for (ShapeItem c = 1; c < item_len; ++c)   // the remaining items.
            Z.next_ravel_Char(UNI_SPACE);
      }
   else                                   // simple numeric scalar
      {
        // B_item is a number, so expand it to item_shape with 0, and store
        //  it in Z
        //
        Z.next_ravel_Cell(B_item);
        for (ShapeItem c = 1; c < item_len; ++c)
            Z.next_ravel_0();
      }
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_F12_PICK::disclose_with_axis(cValue_R X, cValue_R B)
{
   // disclose with axis: Z←⊃[X] B
   // implemented as: cB ← ⊃ B ◊ cX ← ((⍳⍴⍴cB)∼X),X ◊ Z←cX ⍉ B

Value_P cB = disclose(B, true);   // cB ← ⊃ B

   // X names axes of cB (not of B -- disclose() can raise cB's rank above
   // B's own, e.g. when B's items are themselves non-scalar), so X can only
   // be validated once cB is known. to_bitmap() checks that X is a scalar
   // or vector of distinct, in-range, ⎕IO-adjusted axes and reports a
   // ⎕IO-aware )MORE text on its own -- reused here instead of an ad-hoc
   // check, which used to (a) invoke undefined behaviour (1 << negative)
   // on an axis too small for the current ⎕IO, and (b) mislabel any
   // out-of-range axis as a generic "Bad length of X" instead of naming
   // the offending axis.
const AxesBitmap axes_X = X.to_bitmap("⊃[X]B", cB->get_rank());
const Shape sh_X = Value::to_shape(&X);

   // ISO 13751 / APL2 (Disclose with axis): the number of items in X must
   // equal the rank of the items of B (Blake McBride, Bugs28 #63). Without
   // this check, to_bitmap() above only rejects an axis that is out of
   // range for cB (B's own rank plus the items' rank), so e.g. too few or
   // too many axes in X -- as long as they are themselves in that wider
   // range -- silently picked the wrong axes of cB instead of failing.
   //
const Shape item_shape = compute_item_shape(B, true);
   if (sh_X.get_rank() != item_shape.get_rank())
      {
        MORE_ERROR() << "⊃[X]B: ⍴,X (" << sh_X.get_rank()
                     << ") does not match the rank of the items of B ("
                     << item_shape.get_rank() << ")";
        AXIS_ERROR;
      }

   /* ⍴Z is the axes of B that are not in X, followed by those which are.
      That is, shape_Z is a permutation of ⍳⍴⍴B defined by X.

      We prepend the permutation axes_X (of each disclosed item) with the
      axes not in X to obtain the entire permutation perm_cB of cB.
  */
Shape perm_cB;   // perm_cB is the permutation of cB, constructed from X
   loop(x, cB->get_rank())      // axes not in X
       {
         if (axes_X & 1 << x)   continue;   // axis x is in X
         perm_cB.add_shape_item(x);       //  axis x is not in X
       }

   loop(x, sh_X.get_rank())   // axes in X
       {
         perm_cB.add_shape_item(sh_X.get_shape_item(x));
       }

   // perm_cB.get_rank() == cB->get_rank() always holds here: to_bitmap()
   // above guarantees that sh_X.get_rank() axes, all distinct and each in
   // [0, cB->get_rank()), were excluded from the first loop, so the two
   // loops together contribute exactly cB->get_rank() items.
   //
   return Bif_F12_TRANSPOSE::transpose(perm_cB, *cB);
}
//────────────────────────────────────────────────────────────────────────────
Shape
Bif_F12_PICK::compute_item_shape(cValue_R B, bool rank_tolerant)
{
   /* The ravel cells of B are either simple or else PointerCells of nested
      values with possibly different shapes.

      Return the smallest shape that is large enough to contain each value.

      If rank_tolerant is false, then all nested values have the same rank
      (which is also the rank of the result). Otherwise: RANK_ERROR.

      If rank_tolerant is true, then the smaller ranks are extended to the
      largest rank of all shapes by prepending 1s.
    */

   // we use ret_rank and ret[MAX_RANK] instead of Shape to avoid unnecessary
   // volume recomputations in class Shape.
   //
ShapeItem ret_rank = 0;
ShapeItem ret[MAX_RANK];
   loop(r, MAX_RANK)   ret[r] = 0;

   loop(b, B.nz_element_count())
       {
         const cValue * val;
         {
           Cell cache;
           const Cell & cB = B.get_cravel(b, cache);
           if (Value_P v = cB.try_pointer_value())
              {
                val = v.get();
              }
           else if (cB.is_lval_cell())
              {
                const Cell * target = cB.get_lval_value();

                /* pointee was created by a selective assignment and
                   there are 3 cases:

                   1. pointee == 0 (e.g from over-take): do nothing
                   2. pointee is simple.                 do nothing
                   3. pointee is sub-value:              use its value
                 */
                if (!target)   continue;                      // case 1.
                if (!target->is_pointer_cell())   continue;   // case 2.
                val = target->get_pointer_value().get();      // case 3.
              }
           else
              {
                continue;   // cB is simple, so it fits anywhere.
              }
         }

         // at this point val is a non-scalar value.
         //
         const sRank val_rank = val->get_rank();
         if (ret_rank == 0)   // val is the first non-scalar
            {
              ret_rank = val_rank;
              loop(r, ret_rank)   ret[r] = val->get_shape_item(r);
              continue;
            }

         // the items of B must have the same rank, unless we are rank_tolerant
         //
         if (ret_rank != val_rank)
            {
              if (!rank_tolerant)   RANK_ERROR;

              // the smaller rank is extended by PREPENDING 1s, i.e. the
              // shapes are aligned at their last axes (like the item copy
              // does). Aligning them at their first axes lost item data
              // (Bugs35 #31).
              //
              if (ret_rank < val_rank)   // shift ret right
                 {
                   const ShapeItem diff = val_rank - ret_rank;
                   for (ShapeItem r = ret_rank - 1; r >= 0; --r)
                       ret[r + diff] = ret[r];
                   loop(r, diff)   ret[r] = 1;
                   ret_rank = val_rank;
                 }
            }

         const ShapeItem offset = ret_rank - val_rank;
         loop(r, val_rank)
             {
               if (ret[offset + r] < val->get_shape_item(r))
                  {
                    ret[offset + r] = val->get_shape_item(r);
                  }
             }
       }

   return Shape(ret_rank, ret);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_F12_PICK::pick(cValue_R A, ShapeItem idx_A, ShapeItem len_A,
                   cValue_R B, APL_Integer qio)
{
   // A[idx_A] is the current index of B.
   //
const ShapeItem offset = pick_offset(A, idx_A, len_A, B, qio);
Cell cache;
const Cell & cB = B.get_cravel(offset, cache);

   if (len_A > 1)   // more levels coming.
      {
        if (cB.is_pointer_cell())
           {
             return pick(A, idx_A+1, len_A-1,
                         *cB.get_pointer_value(), qio);
           }

        if (cB.is_lval_cell())
           {
             // Note: this is a little tricky...

             // first of all, we need a pointer cell. Therefore the target
             // of cB should be a PointerCell.
             //
             Cell & target = *cB.get_lval_value();
             if (!target.is_pointer_cell())
                {
                  MORE_ERROR() << "A⊃B: the target selected by A["
                               << (idx_A + qio) << "] is not nested";
                  DOMAIN_ERROR;
                }

             // secondly, get_cellrefs() is not recursive, therefore target has
             // not (yet) been converted to a left-value. We do that now.
             //
             Value_P subval = target.get_pointer_value();   // right-value
             Value_P subrefs = subval->get_cellrefs(LOC);   // left-value
             return pick(A, idx_A + 1, len_A - 1, *subrefs, qio);
           }

        // simple cell. This means that the depth of B does not suffice to
        // pick the item selected by A or, in other words, A is too long).
        //
        MORE_ERROR() << "A⊃B: B is only " << (idx_A + 1)
                     << " levels deep, but A has " << (idx_A + len_A)
                     << " items";
        RANK_ERROR;   // ISO p.166 wants a RANK ERROR here, event though LENGTH
                      // ERROR would be more intuitive since A is too long.
      }

   // len_A == 1, means that the end of the iteration over A has been reached,
   // and that cB is the cell in B that was pick'ed by A⊃B.
   //
   if (Value_P v = cB.try_pointer_value())
      {
        return CLONE_P(v, LOC);
      }

   if (cB.is_lval_cell())   // selective assignment, e.g. (A⊃B) ← C
      {
        Cell * target = cB.get_lval_value();
        Assert(target);

        if (target->is_pointer_cell())
           {
             /* (A⊃B)←C must REPLACE the whole (already-nested) item with
                C, not assign into the old item's own cells -- the shape
                of the OLD item must not constrain the new value C at all
                (neither IBM APL2 nor Dyalog assign into a nested target,
                per the LRM's own Pick example; Blake McBride, Bugs28
                #30). But (SEL A⊃B)←C -- Pick composed with an OUTER
                selector, e.g. (1 0/2⊃V)←C -- needs cB's cellrefs at the
                OLD item's own shape so that outer selector can narrow
                them in the ordinary way; whether this A⊃B is that outer
                selector's own target (needs the drill-down) or is itself
                the outermost, final target (needs the wholesale replace)
                is not something pick() can tell from A and B alone -- it
                depends on what the CALLER does with the value returned
                here, which pick() cannot see.
                So: always return the drill-down (subrefs, below), but
                also mark it (set_lval_pick_slot()) as being pick()'s own
                direct, unbroken selection of *target -- exactly like
                Symbol::resolve_lv()'s lval_whole_symbol marker, an outer
                selector that narrows subrefs into its own fresh Value
                (e.g. compress selecting a subset of rows) never copies
                this marker, so it only survives when nothing narrows
                subrefs before Value::assign_cellrefs() sees it -- which
                is exactly when the wholesale replace is correct.
                //
                // cB was created by get_cellrefs() which is flat (non-
                // recursive). That means that the required conversion to
                // a left-side PointerCell (which does not exist) was
                // deferred until this point in time and needs to be done
                // now.
              */
             Value_P subval = target->get_pointer_value();
             Value_P subrefs = subval->get_cellrefs(LOC);
             Value * cell_owner = B.get_lval_cellowner();
             subrefs->set_lval_pick_slot(target, cell_owner);
             return subrefs;
           }

        Value_P Z(LOC);
        Value * cell_owner = B.get_lval_cellowner();
        Z->next_ravel_Lval(target, cell_owner);

        // mark this as pick()'s own direct, unbroken selection of
        // *target too, exactly like the is_pointer_cell() branch above
        // (Bugs30 #13/#16): without this, (3⊃N)←C for a simple (non-
        // nested) N relied on Value::assign_cellrefs()'s own blanket
        // "a 1-element destination skips conformance" special case to
        // accept a non-conforming C -- the same overly broad exception
        // that let a genuinely selective, merely-1-element target (e.g.
        // (1↑V)←9 9 9, a Take/Drop narrowing to one item, nothing to do
        // with Pick) wrongly accept a non-conforming C too. Marking
        // every Pick target explicitly, and having assign_cellrefs()
        // rely only on that marker rather than on dest_count==1, keeps
        // Pick's own wholesale-replace semantics while letting Take/Drop
        // and friends enforce ordinary conformance again.
        //
        Z->set_lval_pick_slot(target, cell_owner);
        return Z;
      }
   else   // simple cell
      {
        Value_P Z(LOC);
        Z->next_ravel_Cell(cB);
        return Z;
      }
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
Bif_F12_PICK::pick_offset(cValue_R A, ShapeItem idx_A,
                          ShapeItem len_A, cValue_R B, APL_Integer qio)
{
Cell cache;
const Cell & cA = A.get_cravel(idx_A, cache);

   if (cA.is_pointer_cell())   // then B shall be a 1-dimensional array
      {
        /*
           cA = A[idx_A] is nested, e.g.

           case i.    (⊂"b") ⊃ A.b.c ← 'leaf-A.b.c'   (structured variable A)

           case ii.   (⊂1 1) ⊃ A←3 3⍴⍳9               (normal A)
         */

        const Value & A = *cA.get_pointer_value();

        if (B.is_member())   // case i. (structured B)
           {
             if (!A.is_char_string())
                {
                  UCS_string & more = MORE_ERROR();
                  more << "A⊃B: A[" << (idx_A + qio)
                       << "] must be a member name (a character vector);"
                          " A[" << (idx_A + qio) << "] is ";
                  ArgCheck::append_shape(more, A);
                  DOMAIN_ERROR;
                }

             const UCS_string top_level(UNI_B);
             const UCS_string member(A);
             vector<const UCS_string *> members;
             members.push_back(&member);
             members.push_back(&top_level);   // dummy, must be last
             const Cell * Bsub = B.get_existing_member(members);  // may throw
             return B.get_offset(Bsub);
           }
        else                  // case ii. (normal B)
           {
             if (A.get_rank() > 1)
                {
                  MORE_ERROR() << "A⊃B: expecting ⍴⍴A[" << (idx_A + qio)
                               << "] ≤ 1; ⍴⍴A[" << (idx_A + qio) << "] is "
                               << A.get_rank();
                  RANK_ERROR;
                }

             const ShapeItem len_A = A.element_count();
             if (B.get_rank() != len_A)
                {
                  MORE_ERROR() << "A⊃B: expecting ⍴⍴B = ⍴,A[" << (idx_A + qio)
                               << "] = " << len_A << "; ⍴⍴B is "
                               << B.get_rank();
                  RANK_ERROR;
                }

             const Shape weights_B = B.get_shape().get_weights();
             const Shape A_as_shape(A, qio);
             ShapeItem offset = 0;

             loop(r, A.element_count())
                 {
                   const ShapeItem ar = A_as_shape.get_shape_item(r);
                   const ShapeItem dim = B.get_shape_item(r);
                   if (ar < 0 || ar >= dim)
                      {
                        MORE_ERROR() << "A⊃B: A[" << (idx_A + qio) << "]["
                                     << (r + qio) << "] = " << (ar + qio)
                                     << " is not a valid index for axis "
                                     << (r + qio) << " of B (expecting "
                                     << qio << "≤index<" << (dim + qio)
                                     << ")" << ArgCheck::index_io0_note(ar, dim);
                        INDEX_ERROR;
                      }
                   offset += weights_B.get_shape_item(r) * ar;
                 }
             return offset;
           }
      }
   else   // A is a scalar, so B must be a vector.
      {
        if (B.get_rank() != 1)
           {
             MORE_ERROR() << "A⊃B: expecting ⍴⍴B = 1 for scalar A; ⍴⍴B is "
                          << B.get_rank();
             RANK_ERROR;
           }
        const APL_Integer a = cA.get_near_int() - qio;
        const ShapeItem dim = B.get_shape_item(0);
        if (a < 0 || a >= dim)
           {
             MORE_ERROR() << "A⊃B: A = " << (a + qio)
                          << " is not a valid index for B (expecting "
                          << qio << "≤A<" << (dim + qio) << ")"
                          << ArgCheck::index_io0_note(a, dim);
             INDEX_ERROR;
           }
        return a;
      }
}
//════════════════════════════════════════════════════════════════════════════




