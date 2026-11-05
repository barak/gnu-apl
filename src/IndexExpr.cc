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
#include "IndexExpr.hh"
#include "PrintBuffer.hh"
#include "PrintOperator.hh"
#include "Value.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
IndexExpr::IndexExpr(Assign_state astate, const char * loc)
   // Ring membership (Bugs6 #5) was tried and reverted TWICE before this
   // (see r2053/r2054 history), both times because erase_stale() *deleted*
   // whatever it found via the ring -- and it had no way to tell a stale
   // IndexExpr from a live one (no owner_count; a live one is the normal
   // case while an index is on some SI's suspended prefix stack). Deleting
   // a live IndexExpr is a segfault waiting to happen: the dangling
   // pointer is still sitting in a TOK_PINDEX/TOK_INDEX token, scheduled
   // for a second delete at the next )SIC/)CLEAR/)RESET (Blake McBride,
   // Bugs8 #1 -- almost certainly also Bill Heagy's original crash).
   //
   // Fix: IndexExpr::erase_stale() is gone. Actual freeing stays exactly
   // where it already correctly happened all along (Prefix::clean_up(),
   // the individual reduce_*() delete sites). Ring membership now exists
   // purely so that print_stale() (used by )CHECK and by IO_Files.cc at
   // the end of every .tc file) can independently re-derive reachability
   // -- mark every IndexExpr in the ring, then unmark those still
   // reachable from a live TOK_PINDEX/TOK_INDEX token on some SI's prefix
   // stack (see Prefix::unmark_all_values()) -- and report, never delete,
   // whatever is still marked afterwards. This mirrors how Value's own
   // )CHECK diagnostic works: a real bug still leaks rather than crashes,
   // but is now visible and testcase-reproducible instead of silent.
   : DynamicObject(loc, &all_index_exprs),
     quad_io(Workspace::get_IO()),
     assign_state(astate),
     rank(0),
     value_count(0),
     marked(false)
{
}
//────────────────────────────────────────────────────────────────────────────
IndexExpr::~IndexExpr()
{
   if (value_count)
      {
        loop(r, rank)   if (values[r])   values[r].reset();
      }
}
//────────────────────────────────────────────────────────────────────────────
void
IndexExpr::check_index_range(const Shape & shape) const
{
   loop(r, rank)
      {
        if (const cValue * ival = get_axis_value(r))   // unless elided index
           {
             const ShapeItem dim = shape.get_shape_item(r);
             loop(i, ival->element_count())
                {
                  const APL_Integer idx = ival->get_near_int(i) - quad_io;
                  if (idx < 0 || idx >= dim)   // invalid index
                     {
                       MORE_ERROR() << "A[B]: index " << (idx + quad_io)
                                    << " is not a valid index for axis "
                                    << (r + quad_io) << " of A (expecting "
                                    << quad_io << "≤index<" << (dim + quad_io)
                                    << "); ⍴A is " << shape
                                    << ArgCheck::index_io0_note(idx, dim);
                       INDEX_ERROR;
                     }
                }
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
sRank
IndexExpr::get_axis(sRank max_axis) const
{
   if (rank != 1)   INDEX_ERROR;

const APL_Integer qio = Workspace::get_IO();

Value_P I = values[0];
   if (!I->is_scalar_or_len1_vector())     INDEX_ERROR;

   if (!I->is_near_int(0))   INDEX_ERROR;

const APL_Integer wide_axis = I->get_near_int(0) - qio;

   // axis and max_axis are both signed, so "axis >= max_axis" alone does
   // not catch a negative axis (same class of bug as cValue.cc's
   // get_single_axis, see H17). This function currently has no callers,
   // but fix it to match the validated pattern used elsewhere. Bounds
   // are checked on the wide (APL_Integer) value *before* narrowing to
   // sRank (int16_t), so a huge out-of-range index cannot wrap into a
   // small in-range sRank first.
   //
   if (wide_axis < 0 || wide_axis >= max_axis)   INDEX_ERROR;

const sRank axis = sRank(wide_axis);

   return axis;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
IndexExpr::extract_axis()
{
   if (rank == 0 || !values[0])    return Value_P();   // no index or [ ]

   Assert1(rank == 1);   // must only be called for [ axis ]
   --value_count;
Value_P ret = values[0];
   values[0].reset();   // decrement owner count
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
void
IndexExpr::mark_all_dynamic_index_exprs()
{
   for (DynamicObject * dob = all_index_exprs.get_next();
        dob != &all_index_exprs; dob = dob->get_next())
       {
         dob->pIndexExpr()->mark();
       }
}
//────────────────────────────────────────────────────────────────────────────
int
IndexExpr::print_stale(ostream & out)
{
   // independently re-derive reachability rather than trusting any
   // bookkeeping (IndexExpr has no owner_count): mark everything, then
   // unmark whatever Prefix::unmark_all_values() finds still reachable
   // from a live TOK_PINDEX/TOK_INDEX token on some SI's prefix stack.
   // See the constructor comment for why this never deletes anything.
   //
   mark_all_dynamic_index_exprs();
   Workspace::unmark_all_values();

int count = 0;

   for (const DynamicObject * dob = all_index_exprs.get_next();
        dob != &all_index_exprs; dob = dob->get_next())
       {
         const IndexExpr * idx = dob->pIndexExpr();
         if (!idx->is_marked())   continue;   // reachable, i.e. not stale

         out << dob->where_allocated();

         try           { out << *idx; }
         catch (Error &)      { out << " *** corrupt ***"; }
         catch (std::bad_alloc &) { out << " *** corrupt ***"; WS_FULL; }
         catch (...)          { FIXME; }

         out << endl;

         idx->unmark();
         ++count;
       }

   return count;
}
//════════════════════════════════════════════════════════════════════════════
ostream &
operator <<(ostream & out, const IndexExpr & idx)
{
   out << "[";
   loop(i, idx.get_rank())
      {
        if (i)   out << ";";
        if (const cValue * ival = idx.get_axis_value(i))
           {
             // value::print() may print a trailing LF that we don't want here.
             // We therefore print the index values ourselves.
             const PrintContext pctx = Workspace::get_PrintContext(PR_APL_MIN);
             PrintBuffer pb(*ival, pctx, 0);

             UCS_string ucs(pb, ival->get_rank(), Workspace::get_PW());
             out << ucs;
           }
      }
   return out << "]";
}
//════════════════════════════════════════════════════════════════════════════
