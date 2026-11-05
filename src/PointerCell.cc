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

#include "Error.hh"
#include "PointerCell.hh"
#include "PrintOperator.hh"
#include "Value.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
/// every PointerCell nests sub_val one level deeper into cell_owner; this
/// is the single chokepoint all nesting-increasing constructions funnel
/// through (enclose, strand notation, indexed/selective/member
/// assignment, ...), so it is where the ≡ depth limit is enforced. Clones
/// of an already-valid sub_val (whose depth was checked when it was first
/// nested) re-check harmlessly since cloning does not increase sub_val's
/// own depth.
///
/// Some callers (e.g. Bif_F12_PARTITION::enclose_with_axes()) construct an
/// empty sub_val, wrap it in a PointerCell immediately, and only populate
/// its ravel afterwards through the same (now shared) Value object --
/// sub_val->more() is true while that population is still in progress.
/// compute_depth() is unsafe to call at that point (the ravel is only
/// partially, or not yet at all, initialized), so the check below is
/// skipped for an incomplete sub_val: no useful depth exists yet, and any
/// PointerCell later copied into sub_val (deepening it) goes back through
/// this same constructor, so the limit is still enforced by the time
/// sub_val (and hence cell_owner) is actually used.
void
PointerCell::check_nesting_depth(const Value * sub_val)
{
   if (sub_val->more())   return;   // still being populated, see above
   if (1 + sub_val->compute_depth() > MAX_DEPTH)   LIMIT_ERROR_NESTING;
}
//────────────────────────────────────────────────────────────────────────────
PointerCell::PointerCell(Value * sub_val, Value & cell_owner)
{
   Assert(!sub_val->is_simple_scalar());
   check_nesting_depth(sub_val);

   new (&value.pval.valp) Value_P(sub_val, LOC);
   value.pval.owner = &cell_owner;

   Assert(value.pval.owner != sub_val);   // typical cut-and-paste error
   cell_owner.increment_pointer_cell_count();
   cell_owner.add_subcount(sub_val->nz_element_count());
}
//────────────────────────────────────────────────────────────────────────────
PointerCell::PointerCell(Value * sub_val, Value & cell_owner, uint32_t magic)
{
   // DO NOT: Assert(!sub_val->is_simple_scalar()); This is a special
   // constructor only used in ScalarFunction.cc to create an un-initialized
   // PointerCell

   Assert(magic == 0x6B616769);

   new (&value.pval.valp) Value_P(sub_val, LOC);
   value.pval.owner = &cell_owner;

   Assert(value.pval.owner != sub_val);   // typical cut-and-paste error
   cell_owner.increment_pointer_cell_count();
   cell_owner.add_subcount(sub_val->nz_element_count());
}
//────────────────────────────────────────────────────────────────────────────
PrintBuffer
PointerCell::character_representation(const PrintContext & pctx) const
{
   return character_representation(pctx,
                                   PrintBuffer::get_frame_chars(pctx.get_style()));
}
//────────────────────────────────────────────────────────────────────────────
PrintBuffer
PointerCell::character_representation(const PrintContext & pctx,
                                      const UCS_string & frame_chars) const
{
Value_P val = get_pointer_value();
   Assert(val);

   if (pctx.get_style() & PST_QUOTE_CHARS)
      {
        if (val->is_char_vector())
           return PrintBuffer(*val, pctx, frame_chars, 0);
      }

   if (pctx.get_style() & PST_INPUT)   // APL input format, e.g. (1 2)
      {
        UCS_string ucs;
        PrintBuffer::append_input_item(ucs, *this);
        ColInfo ci;
        PrintBuffer ret(ucs, ci);
        ret.get_info().int_len  = ret.get_column_count();
        ret.get_info().real_len = ret.get_column_count();
        return ret;
      }

PrintBuffer ret(*val, pctx, frame_chars, 0);
   ret.get_info().flags &= ~CT_MASK;
   ret.get_info().flags |= CT_POINTER;

   if (PrintBuffer::has_frame(frame_chars))
      {
        const ShapeItem ec = val->element_count();
        if (ec == 0)   // empty value
           {
             int value_flags = 0;
             loop(r, val->get_rank())
                {
                  if (val->get_shape_item(r) == 0)   // an empty axis
                     {
                       if (r == val->get_rank()-1)
                          value_flags |= PST_EMPTY_LAST;
                       else
                          value_flags |= PST_EMPTY_NLAST;
                     }
                }

             // do A←(1⌈⍴A)⍴⊂↑A
             Value_P proto = val->prototype(LOC);

             // compute a shape like ⍴ val but 0 replaced with 1.
             //
             Shape sh(val->get_shape());
             loop(r, sh.get_rank())
                 {
                   if (sh.get_shape_item(r) == 0)   sh.set_shape_item(r, 1);
                 }

             Value_P proto_reshaped(sh, LOC);
             const ShapeItem len = proto_reshaped->nz_element_count();

             // store proto in the first ravel item, and copies of proto in
             // the subsequent ravel items
             //
             loop(rv, len)   proto_reshaped->next_ravel_Value(proto.get());

             proto_reshaped->check_value(LOC);
             ret = PrintBuffer(*proto_reshaped, pctx, frame_chars, 0);
             if (frame_chars[PrintBuffer::FC_PARENS] == UNI_L_PARENT)
                {
                  // as IBM APL2: blanks as wide as one row of prototypes
                  // (none if the last axis is 0) in parentheses
                  //
                  const int width = (value_flags & PST_EMPTY_LAST)
                                  ? 0 : ret.get_column_count();
                  ColInfo ci;
                  ret = PrintBuffer(UCS_string(width, UNI_SPACE), ci);
                }
             ret.add_frame(frame_chars, proto_reshaped->get_shape(),
                           proto_reshaped->compute_depth(), value_flags);
           }
       else
           {
             int value_flags = 0;
             bool is_simple   = true;
             bool has_chars   = false;
             bool has_numbers = false;
             loop(e, ec)
                 {
                   Cell cache;
                   const Cell & cell = val->get_cravel(e, cache);
                   if      (cell.is_pointer_cell())     is_simple = false;
                   else if (cell.is_character_cell())   has_chars = true;
                   else if (cell.is_numeric())          has_numbers = true;
                 }
              if (has_chars && has_numbers)
                 {
                   value_flags |= PST_SIMPLE_MIXED;
                 }
              else if (is_simple && has_numbers)
                 {
                   value_flags |= PST_SIMPLE_NUMER;
                 }

             if (!(pctx.get_style() & PST_QUOTE_CHARS && val->is_char_array()))
                {
                  ret.add_frame(frame_chars, val->get_shape(),
                                val->compute_depth(), value_flags);
                }
           }
      }

   ret.get_info().int_len  = ret.get_column_count();
   ret.get_info().real_len = ret.get_column_count();
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
Comp_result
PointerCell::compare(const Cell & other) const
{
   if (other.get_cell_type() & CT_SIMPLE)   // nested > numeric > char
      return COMP_GT;

   if (other.get_cell_type() != CT_POINTER)   DOMAIN_ERROR;

   // at this point both cells are pointer cells.
   //
Value_P v1 = get_pointer_value();
Value_P v2 = other.get_pointer_value();

   // compare ranks
   //
   if (v1->get_rank() != v2->get_rank())   // ranks differ
      return v1->get_rank() > v2->get_rank() ? COMP_GT : COMP_LT;

   // same rank, compare shapes
   //
   loop(r, v1->get_rank())
      {
        const ShapeItem axis1 = v1->get_shape_item(r);
        const ShapeItem axis2 = v2->get_shape_item(r);
        if (axis1 != axis2)   // axis r differs
           return axis1 > axis2 ? COMP_GT : COMP_LT;
      }

   // same rank and shape, compare ravel
   //
   loop(e, v1->nz_element_count())
      {
        Cell cache1, cache2;
        if (const Comp_result comp = v1->get_cravel(e, cache1)
                                        .compare(v2->get_cravel(e, cache2)))
           return  comp;
      }

   // everthing equal
   //
   return COMP_EQ;
}
//────────────────────────────────────────────────────────────────────────────
bool
PointerCell::equal(const Cell & other, double qct) const
{
   if (!other.is_pointer_cell())   return false;

Value_P A = get_pointer_value();
Value_P B = other.get_pointer_value();

   if (!A->same_shape(*B))                 return false;

const ShapeItem count = A->nz_element_count();
   loop(c, count)
       {
         Cell cache_A, cache_B;
         if (!A->get_cravel(c, cache_A).equal(B->get_cravel(c, cache_B), qct))
            return false;
       }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
PointerCell::get_pointer_value() const
{
Value * vp = static_cast<Value *>(const_cast<cValue *>(value.pval.valp.get()));
Value_P ret(vp, LOC);   // Value_P constructor increments owner_count
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
bool
PointerCell::greater(const Cell & other) const
{
   if (compare(other) == COMP_GT)   return true;
   if (compare(other) == COMP_LT)   return false;
   return this > &other;
}
//────────────────────────────────────────────────────────────────────────────
void
PointerCell::init_other(void * other, Value & other_owner,
                            const char * loc) const
{
   Assert(other);   // the new PointerCell to be created

Value_P sub;   // instantiate beforehand so that sub is 0 if clone() fails

   sub = CLONE_P(get_pointer_value(), loc);
   Assert(sub);
   new (other) PointerCell(sub.get(), other_owner);
}
//────────────────────────────────────────────────────────────────────────────
bool
PointerCell::is_member_anchor() const
{
   return value.pval.valp.value_p &&
          value.pval.valp.value_p->is_member();
}
//────────────────────────────────────────────────────────────────────────────
void
PointerCell::isolate_deep(const char * loc)
{
   isolate(loc);
Value * val = value.pval.valp.get();
   // depth-cache invalidation piggybacks on this walk -- see the matching
   // comment in Value_P_Base::isolate_deep() (Value_P.icc), which is where
   // this recursion is entered from at the top level.
   if (val)   val->invalidate_depth();
   if (val && !val->is_packed())
      loop(j, val->nz_element_count())
          {
            Cell & cell = val->get_wravel(j);
            if (cell.is_pointer_cell())
               {
                 cell.isolate_deep(loc);
               }
          }
}
//────────────────────────────────────────────────────────────────────────────
void
PointerCell::release(const char * loc)
{
   // 1. update the PointerCell related counters in the owner of this
   //    PointerCell (since this Cell is no longer a PointerCell).
   //
   value.pval.owner->decrement_pointer_cell_count();
   value.pval.owner->add_subcount(-get_pointer_value()->nz_element_count());

   // 2. decrement the owner_count of our sub-value via reset() (since this
   //    PointerCell releases the ownership of the sub-value.
   //
   value.pval.valp.reset();

   // 3. not needed but to be on the safe side: make this Cell an IntCell
   //    (in case someone still points to it).
   //
   IntCell::z0(this);
}
//────────────────────────────────────────────────────────────────────────────
CellType
PointerCell::deep_cell_subtypes() const
{
   return CellType(CT_POINTER | get_pointer_value()->deep_cell_subtypes());
}
//────────────────────────────────────────────────────────────────────────────
CellType
PointerCell::deep_cell_types() const
{
   return CellType(CT_POINTER | get_pointer_value()->deep_cell_types());
}
//════════════════════════════════════════════════════════════════════════════

