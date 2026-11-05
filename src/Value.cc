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

#include <memory>
#include <sys/types.h>

#include "Bif_F12_PARTITION_PICK.hh"
#include "CDR_string.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "Common.hh"
#include "ConstCell_P.hh"
#include "Error.hh"
#include "FloatCell.hh"
#include "Heapsort.hh"
#include "IndexExpr.hh"
#include "IndexIterator.hh"
#include "IntCell.hh"
#include "IO_Files.hh"
#include "LvalCell.hh"
#include "Macro.hh"
#include "Output.hh"
#include "PointerCell.hh"
#include "Parallel.hh"
#include "PrintOperator.hh"
#include "Quad_XML.hh"
#include "StateIndicator.hh"
#include "Symbol.hh"
#include "SystemVariable.hh"
#include "UCS_string.hh"
#include "UserFunction.hh"
#include "Value.hh"
#include "ValueHistory.hh"
#include "Workspace.hh"

#include "Workspace.icc"

extern uint64_t top_of_memory();

uint64_t Value::value_count = 0;
uint64_t Value::total_ravel_count = 0;

// most static members of class Value are defined in StaticObjects.cc

_deleted_value * Value::deleted_values = 0;

int Value::deleted_values_count = 0;

uint64_t Value::fast_new_count = 0;

uint64_t Value::slow_new_count = 0;

uint64_t Value::alloc_size = 0;

//════════════════════════════════════════════════════════════════════════════
Value::Value(const char * loc)
   : DynamicObject(loc, &all_values)
{
   ADD_EVENT(this, VHE_Create, 0, loc);
   init_ravel();
}
//────────────────────────────────────────────────────────────────────────────
Value::Value(const Cell & cell, const char * loc)
   : DynamicObject(loc, &all_values)
{
   ADD_EVENT(this, VHE_Create, 0, loc);
   init_ravel();

   set_ravel_Cell(0, cell);
   check_value(LOC);
}
//────────────────────────────────────────────────────────────────────────────
Value::Value(ShapeItem len, const char * loc)
   : DynamicObject(loc, &all_values)
{
   shape = Shape(len);
   ADD_EVENT(this, VHE_Create, 0, loc);
   init_ravel();
}
//────────────────────────────────────────────────────────────────────────────
Value::Value(ShapeItem rows, ShapeItem cols, const char * loc)
   : DynamicObject(loc, &all_values)
{
   shape = Shape(rows, cols);
   ADD_EVENT(this, VHE_Create, 0, loc);
   init_ravel();
}
//────────────────────────────────────────────────────────────────────────────
Value::Value(const Shape & sh, const char * loc)
   : DynamicObject(loc, &all_values)
{
   shape = sh;
   ADD_EVENT(this, VHE_Create, 0, loc);
   init_ravel();
}
//────────────────────────────────────────────────────────────────────────────
Value::Value(const Shape & sh, uint64_t * bits, const char * loc)
   : DynamicObject(loc, &all_values),
     owner_count(0)
{
   sh.checked_volume();   // throw WS_FULL if sh's true volume overflows
   shape = sh;
   pointer_cell_count = 0;
   ravel.fetcher = &Ravel::bool_fetcher;
   flags = {}; flags.ravel_type = RPT_BOOL;
   ravel.valid_ravel_items = sh.get_nz_volume();
   ravel.cells = reinterpret_cast<Cell *>(bits);
   ADD_EVENT(this, VHE_Create, 0, loc);
   check_ptr = charP(this) + 7;

   // this constructor bypasses init_ravel() entirely, but ~Value()'s
   // RPT_BOOL branch always does --value_count (unconditionally) and,
   // since ravel.cells here is always heap (this constructor never uses
   // ravel.short_value), always total_ravel_count -= nz_element_count()
   // too -- without mirroring init_ravel()'s ++value_count/
   // total_ravel_count here, both counters silently underflow over
   // repeated )LOADs of bit-packed values, corrupting WS-FULL decisions
   // that depend on them.
   ++value_count;
   total_ravel_count += sh.get_nz_volume();

   if (ravel.cells)   { set_complete();   return; }   // caller has
                                                       // allocated -- this
                                                       // value is already
                                                       // fully populated
                                                       // too, just like
                                                       // the self-
                                                       // allocated branch
                                                       // below, which
                                                       // already called
                                                       // set_complete().

   // round the size up to the next 64 bit boundary.
const size_t uint64_count = (sh.get_nz_volume() + 63) >> 6;
   bits = new uint64_t[uint64_count];
   loop(u, uint64_count)   bits[u] = 0;
   ravel.cells = reinterpret_cast<Cell *>(bits);
   set_complete();
}
//────────────────────────────────────────────────────────────────────────────
Value::Value(const UCS_string & ucs, const char * loc)
   : DynamicObject(loc, &all_values)
{
   shape = Shape(ucs.size());
   ADD_EVENT(this, VHE_Create, 0, loc);
   init_ravel();

   set_proto_Spc();                          // prototype
   loop(l, ucs.size())   next_ravel_Char(ucs[l]);
   set_complete();
   try_pack();
}
//────────────────────────────────────────────────────────────────────────────
Value::Value(const UTF8_string & utf, const char * loc)
   : DynamicObject(loc, &all_values)
{
   shape = Shape(utf.size());
   ADD_EVENT(this, VHE_Create, 0, loc);
   init_ravel();

   set_proto_Spc();                          // prototype
   loop(l, utf.size())   next_ravel_Char(Unicode(utf[l] & 0xFF));
   set_complete();
   try_pack();
}
//────────────────────────────────────────────────────────────────────────────
Value::Value(const CDR_string & ui8, const char * loc)
   : DynamicObject(loc, &all_values)
{
   shape = Shape(ui8.size());
   ADD_EVENT(this, VHE_Create, 0, loc);
   init_ravel();

   set_proto_Spc();                          // prototype
   loop(l, ui8.size())   next_ravel_Char(Unicode(ui8[l]));
   set_complete();
   try_pack();
}
//────────────────────────────────────────────────────────────────────────────
Value::Value(const PrintBuffer & pb, const char * loc)
   : DynamicObject(loc, &all_values)
{
   shape = Shape(pb.get_row_count(), pb.get_column_count());
   ADD_EVENT(this, VHE_Create, 0, loc);
   init_ravel();

   set_proto_Spc();                          // prototype

const ShapeItem height = pb.get_row_count();
const ShapeItem width = pb.get_column_count();

   loop(y, height)
   loop(x, width)   next_ravel_Char(pb.get_char(x, y));

   set_complete();
}
//────────────────────────────────────────────────────────────────────────────
Value::Value(const char * loc, const Shape * sh)
   : DynamicObject(loc, &all_values)
{
   shape = Shape(ShapeItem(sh->get_rank()));
   ADD_EVENT(this, VHE_Create, 0, loc);
   init_ravel();

   IntCell::z0(&get_wproto());   // prototype

   loop(r, sh->get_rank())   next_ravel_Int(sh->get_shape_item(r));

   set_complete();
}
//────────────────────────────────────────────────────────────────────────────
Value::~Value()
{
   ADD_EVENT(this, VHE_Destruct, 0, LOC);
   unlink();

   if (flags.ravel_type == (RPT_BOOL))
      {
        --value_count;
        if (ravel.cells != ravel.short_value)   // don't free embedded buffer
           {
             // mirror the value_count/total_ravel_count bookkeeping of the
             // long-value branch below, and (like that branch) deallocate
             // via std::allocator<Cell>, matching how init_ravel() actually
             // allocated the buffer via std::allocator<Cell>::allocate().
             // Uses ravel.alloc_cells (the count actually passed to
             // allocate()), not nz_element_count() (the current, possibly
             // shrunk-in-place, logical count) -- see ravel.alloc_cells's
             // own comment (Ravel.hh) and finding #3 of Blake McBride's
             // Bugs12.md.
             //
             total_ravel_count -= ravel.alloc_cells;
             std::allocator<Cell>{}.deallocate(ravel.cells, ravel.alloc_cells);
           }
        ravel.cells = 0;
        Assert(check_ptr == charP(this) + 7);
        check_ptr = 0;
        return;
      }

const ShapeItem length = nz_element_count();

#if APL_Float_is_class

   //  APL_Float is a class, therefore we need to release all Cells

#else

   // APL_Float is NOT a class, Therefore release only PointerCells
   if (get_pointer_cell_count() > 0)

#endif
      {
        Cell * cZ = &get_wfirst();
        loop(c, length)
            {
              if (get_pointer_cell_count() == 0)   break;
              cZ++->release(LOC);
            }
      }

   Assert1(get_pointer_cell_count() == 0);

   --value_count;

   if (ravel.cells == 0)   return;   // new() failed

   if (ravel.cells != ravel.short_value)   // long value
      {
        // ravel.alloc_cells (the count originally passed to allocate()),
        // NOT length (== nz_element_count(), the current logical count,
        // which several primitives shrink in place after allocating a
        // worst-case ravel -- see ravel.alloc_cells's own comment).
        //
        total_ravel_count -= ravel.alloc_cells;
        std::allocator<Cell>{}.deallocate(ravel.cells, ravel.alloc_cells);
      }

   Assert(check_ptr == charP(this) + 7);
   check_ptr = 0;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Value::get_cellrefs(const char * loc)
{
   // Defense in depth for depth-cache invalidation (see plan.txt's
   // 2026-09-01 analysis): every caller that reaches this point is about
   // to hand out direct write access into this value's ravel, so this is
   // always at least a *candidate* mutation site. The real invalidation
   // guarantee comes from isolate_deep() (called upstream of every left-
   // value construction, unconditionally, over the whole tree) -- this
   // call is deliberately redundant with that, not a replacement for it:
   // it costs one already-cache-resident byte write here, in exchange for
   // catching any future left-value mechanism that reaches a nested
   // sub-value via get_cellrefs() without having gone through
   // isolate_deep() first.
   //
   invalidate_depth();

   // LvalCell stores a Cell * address; that requires an unpacked (Cell-array)
   // ravel.  Explode here so that every get_wravel() below returns a valid
   // Cell reference regardless of how the value was constructed.
   //
   explode_to_Cells();

   /* Create a non-nested (!) (left-) value left_Z from this (right-) value.
      left_Z has the same shape and consists entirely of LvalCells that point
      to the corresponding cells of this value.

      NOTE that any PointerCell in this are NOT handled differently than
      simple cells (and get_cellrefs() will simply point to them)
      rather than recursing into its sub-value. For that reason:

      1. the result left_Z below is flat (not nested), and
      2. assign_cellrefs() below must (at its later point in time),call
         get_cellrefs() when it encounters an LvalCell whose target is a
         PointerCell.

      The reason for delaying the recursion into sub-values is that the
      subsequent processing of left_Z may erase those sub-values and therefore
      doing it now may render useless later.
   */

Value_P left_Z(get_shape(), loc);

   // next_ravel_Cell() is 0 for empty values.
   // Therefore empty values (with element_count == 0) must use
   // set_ravel_Cell() instead of next_ravel_Cell() !
   //
   if (const ShapeItem ec = element_count())
      {
        loop(e, ec)
            {
              Cell & right_cell = get_wravel(e);
              const LvalCell left_cell(&right_cell, this);
              left_Z->next_ravel_Cell(left_cell);
            }
      }
   else   // prototype
      {
        left_Z->set_ravel_Cell(0, LvalCell(&get_wproto(), this));
      }

   left_Z->set_left_value();
   left_Z->check_value(LOC);
   return left_Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Value::neutralize_lval_cells(const char * loc)
{
   // nz_element_count(), not element_count(): an empty value has its
   // LvalCell in its prototype, e.g. ⍳0↑V. And LvalCells can also sit in
   // nested items, e.g. under ⊂ in (⊂V)←9 9 (Blake McBride, Bugs32 #25).
   // Only LvalCells are changed, so recursing into the items of an
   // ordinary (right-) value has no effect.
   //
   // A packed ravel has neither LvalCells nor PointerCells (and no Cells
   // at all, so get_wravel() must not be used on it). Bugs37 #4.
   //
   if (is_packed())   return;

   loop(d, nz_element_count())
       {
         Cell & cell = get_wravel(d);
         if (cell.is_lval_cell())
            {
              cell.release(loc);
              new (&cell)   IntCell(0);
            }
         else if (cell.is_pointer_cell())
            {
              cell.get_pointer_value()->neutralize_lval_cells(loc);
            }
       }
}
//────────────────────────────────────────────────────────────────────────────
void
Value::assign_cellrefs(Value_P new_value)
{
   // assign new_value to this (left-) value

   // LRM: "Pick with an empty left argument ... causes the whole array
   // associated with the assigned name to be replaced", e.g.
   // ((⍳0)⊃A)←new_value ←→ A←new_value, with none of the shape/count
   // conformance that an ordinary selective specification requires. This
   // marker (see set_lval_whole_symbol()) only survives an unbroken chain
   // of empty-Pick applications straight from Symbol::resolve_lv() through
   // to this assignment; anything else in between returns a fresh Value
   // that never carries it, so the ordinary per-cell path below still
   // applies to every other selective specification.
   //
   if (Symbol * sym = get_lval_whole_symbol())
      {
        sym->assign(new_value, true, LOC);
        return;
      }

   // (A⊃B)←new_value where B[...A...] was already nested: replace the
   // whole item wholesale, ignoring the old item's own shape (Blake
   // McBride, Bugs28 #30) -- see set_lval_pick_slot(). Like the
   // lval_whole_symbol marker above, this only survives an unbroken chain
   // straight from Bif_F12_PICK::pick() through to this assignment; any
   // outer selector narrowing this value (e.g. (1 0/A⊃B)←new_value) built
   // a fresh Value for its own narrowed result and never copied the
   // marker, so the ordinary per-cell path below still applies there.
   //
   if (Cell * slot = get_lval_pick_slot())
      {
        // no owner: Pick from a value assembled across several nested
        // items, e.g. (1⊃∊⌽¨N)←9 for a depth-3 N (Blake McBride, Bugs32 #1)
        //
        if (get_lval_pick_owner() == 0)   LvalCell::throw_no_owner();

        Value & owner = *get_lval_pick_owner();
        owner.depth_update_for_overwrite(slot - owner.ravel.cells,
                                new_value->is_simple_scalar() ? -1 : 0);
        slot->release(LOC);
        // init_from_value() enclose-wraps new_value in a fresh PointerCell
        // unless it is itself a simple scalar, in which case it places a
        // plain copy of that scalar's cell instead (⊂ of a simple scalar
        // is a no-op) -- a bare PointerCell(new_value, owner) here would
        // fail Assert(!sub_val->is_simple_scalar()) for e.g. (2⊃A)←7.
        //
        slot->init_from_value(new_value.get(), owner, LOC);
        return;
      }

   // (S.a)←new_value with nothing selecting from S.a first, for either a
   // scalar or an array member (Bugs30 #14): a bare member reference
   // used directly as a selective-specification target looks exactly
   // like a broken/incomplete selective specification -- unlike a plain
   // symbol, where (X)←new_value is legitimately just X←new_value
   // (handled by the lval_whole_symbol marker above), member variables
   // are a GNU-APL-only construct free to reject this outright. Like the
   // other two markers above, this only survives an unbroken chain
   // straight from Prefix::reduce_D_V__()'s member-access reduction
   // through to this assignment; a genuinely selective form such as
   // (⌽S.a)←new_value or (2↑S.a)←new_value builds a fresh Value for its
   // own (narrowed) result and never copies the marker, so those are
   // unaffected.
   //
   if (get_lval_bare_member_ref())
      {
        MORE_ERROR() << "(S.member)←B: a bare member reference is not a "
                        "valid selective-specification target -- select "
                        "something from it first (e.g. (⌽S.member)←B), or "
                        "assign to the member directly (S.member←B)";
        neutralize_lval_cells(LOC);   // Bugs30 #14 (same as #28)
        SYNTAX_ERROR;
      }

const ShapeItem new_value_count = new_value->element_count();
const ShapeItem dest_count  = element_count();

   // check_lval_consistency() (see its own definition, cValue.cc) rejects
   // any ravel item that is neither an LvalCell nor a PointerCell -- the
   // very mechanism that already correctly SYNTAX_ERRORs a non-empty
   // plain constant used as an assignment target (e.g. 1 2 3←7). But
   // dest_count == 0 used to return right here, before ever reaching
   // that check below, so the empty-vector constant ⍬ (dest_count == 0,
   // and not an lvalue either) slipped through as a silent no-op instead
   // of being rejected the same way (Bugs30 #29: ⍬←2, (0⍴0)←2). Run the
   // check first -- it uses nz_element_count() ("at least 1, for the
   // prototype"), so it still sees ⍬'s own (non-lval) prototype cell
   // even though dest_count itself is 0, while a genuinely empty
   // *lvalue* selection (e.g. (0↑V)←⍬ for a real variable V, whose
   // prototype cell is itself an LvalCell) still passes and returns as
   // the legitimate no-op it always was.
   //
   check_lval_consistency();
   if (dest_count == 0)   return;   // nothing to assign; established
                                     // (Bugs28 #52) leniency for a
                                     // genuinely empty selection, e.g.
                                     // (V[⍬])←1 2 3 -- unlike dest_count
                                     // >= 1 below, this is NOT narrowed
                                     // to also require new_value_count
                                     // to be 0 or 1; see Bracket_Index.tc.
                                     // A[⍬]←B for a plain (unparenthesized)
                                     // variable still correctly LENGTH
                                     // ERRORs via the completely separate
                                     // Symbol::assign_indexed() path.

   // nz_element_count() (used here previously) is "at least 1, for the
   // prototype" -- a real, empty new_value made it look exactly like a
   // legitimate 1-element (scalar-extendable) one below, silently
   // zero-filling every selected cell with the prototype instead of
   // raising the LENGTH ERROR that plain indexed assignment (A[...]←⍬)
   // already correctly gives for the same shape mismatch (Bugs28 #31).
   //
   // Unconditionally now, not just when dest_count > 1 (Bugs30 #13/#16):
   // a dest_count of exactly 1 used to be treated as inherently ambiguous
   // between "select exactly one cell/item" (a 1-element ↑/⌷/..., or a
   // member-array bracket index like S.b[1]) and "replace this whole
   // slot wholesale" (Pick, or an ordinary, non-selective "V←B" lvalue)
   // -- but the latter is now always reached via one of the dedicated
   // markers above (lval_whole_symbol, lval_pick_slot) instead, which
   // return long before this point, so anything still here with
   // dest_count == 1 is a genuine 1-item *selection*, not a wholesale
   // replace, and must enforce the same conformance as any other count:
   // (1↑V)←⍬ was wrongly accepted (zero-filling the one selected item)
   // exactly like plain indexed A[1]←⍬ is correctly LENGTH ERROR.
   //
   if (new_value_count == 0)
      {
        MORE_ERROR() << "(f Z)←B: B is empty; expecting ⍴,B to be 1 or "
                     << dest_count << " (the number of selected items)";
        neutralize_lval_cells(LOC);   // Bugs30 #28
        LENGTH_ERROR;
      }

const int src_incr  = (new_value_count == 1)  ? 0 : 1;

   if (src_incr  &&   // new_value is not a scalar or 1-element value, and
       !this->conforms_to(*new_value))   // non-trivial shape mismatch
      {
        // NOTE:  M←3 2ρι6 ◊ (1 0/M) ← 'abc' succeeds in IBM APL2, even
        //        though the shapes of 1 0/M and 'abc' differ. We therefore
        //
        MORE_ERROR() << "(f Z)←B: expecting ⍴,B to be 1 or " << dest_count
                     << " (the number of selected items); ⍴,B is "
                     << new_value_count;
        neutralize_lval_cells(LOC);   // Bugs30 #28
        LENGTH_ERROR;
      }

   /* this: a value containing LvalCells and possibly PointerCells.

      test with:

      C←"001011010010010110010100100001000010001101" ◊ ((C='0')/C)←' ' ◊ C
      K←'RED' 'WHITE' 'BLUE'   ◊ (↑K)←'YELLOW' ◊ K
      V←'v' 'vw' 'vwx' 'vwxyz' ◊ (2↑¨V)←5      ◊ 4 ⎕CR V

    */

   // consistency check (all items are LvalCells or PointerCells) already
   // done above, before the dest_count == 0 check (Bugs30 #29).
   //
   // Bugs31 #18 (Blake McBride): LRM p.43 rule 2 ("if the right is a
   // scalar, or an array with empty shape when ones are removed") means
   // a 1-ELEMENT new_value (e.g. ,9, shape ,1) is exempt from rule 1's
   // enclosure just as much as a genuine scalar is -- checking
   // new_value->is_scalar() (rank == 0) instead of new_value_count == 1
   // wrongly enclosed a 1-item non-scalar RHS like ,9 into (V[1])←,9,
   // instead of scalar-extending it the same way (V[1])←9 already does.
   //
   if (is_scalar() && new_value_count != 1)
      {
        Cell cache;
        const Cell * C0 = &get_cscalar(cache);
        if (!C0->is_lval_cell())   LEFT_SYNTAX_ERROR;
        if (Cell * target = C0->try_lval_value())   // valid right Cell
           {
             Value & owner = *C0->get_cell_owner();
             owner.depth_update_for_overwrite(target - owner.ravel.cells, 0);
             target->release(LOC);   // free sub-values etc (if any)
             new (target)   PointerCell(new_value.get(), owner);
           }
        return;
      }

   loop(d, dest_count)
      {
        Cell src_cache;
        const Cell & src = new_value->get_cravel(d * src_incr, src_cache);
        Cell & dest = get_wravel(d);
        if (dest.is_pointer_cell())
           {
             /*
                NOTE: At this point dest is one of the left cells in a
                      selective assignment, say:

                      (f1 f2 ... fn VAR) ← new_value.

                The initial cellrefs (i.e. of VAR) contains no PointerCells
                (though possibly Lval cells pointing to PointerCells).

                Therefore dest must have been created by one of the fi and
                then all sub-values of dest must also be Lval Cells.
              */
              Value_P left_sub = dest.get_pointer_value();

              // the item is the result of A⊃ (e.g. (1⊃¨N)←B): replace the
              // picked item wholesale (as (A⊃N)←B does, LRM), not cell by
              // cell with the shape of the old item (Bugs34 #18)
              //
              if (left_sub->get_lval_pick_slot())
                 {
                   left_sub->assign_cellrefs(src.is_pointer_cell()
                                             ? src.get_pointer_value()
                                             : src.to_value(LOC));
                   continue;
                 }

              Value_P right_sub;
              if (src.is_pointer_cell())   right_sub = src.get_pointer_value();
              if (right_sub)
                 {
                   if (left_sub->get_rank() != right_sub->get_rank())
                      {
                        MORE_ERROR() << "(f Z)←B: expecting ⍴⍴B = "
                                     << left_sub->get_rank()
                                     << " (⍴⍴ of the selected item); ⍴⍴B is "
                                     << right_sub->get_rank();
                        RANK_ERROR;
                      }
                   if (left_sub->get_shape() != right_sub->get_shape())
                      {
                        MORE_ERROR() << "(f Z)←B: expecting ⍴B = "
                                     << left_sub->get_shape()
                                     << " (⍴ of the selected item); ⍴B is "
                                     << right_sub->get_shape();
                        LENGTH_ERROR;
                      }
                 }
              loop(s, left_sub->nz_element_count())
                  {
                    Cell * Csub = &left_sub->get_wravel(s);
                    if (!Csub->is_lval_cell())   LEFT_SYNTAX_ERROR;

                    if (Cell * target = Csub->try_lval_value())   // 0 possible!
                       {
                         Value & owner = *Csub->get_cell_owner();
                         // if src is simple, then scalar extend it.
                         // Otherwise use item s of src.
                         //
                         Cell right_cache;
                         const Cell & right_cell = right_sub ?
                                     right_sub->get_cravel(s, right_cache) : src;
                         owner.depth_update_for_overwrite(target - owner.ravel.cells,
                                    right_cell.is_pointer_cell() ? 0 : -1);
                         target->release(LOC);   // free sub-values etc.
                         right_cell.init_other(target, owner, LOC);
                       }
                  }
           }
        else if (dest.is_lval_cell())
           {
             if (Cell * target = dest.get_lval_value())   // target can be 0!
                {
                  Value & owner = *dest.get_cell_owner();
                  owner.depth_update_for_overwrite(target - owner.ravel.cells,
                                    src.is_pointer_cell() ? 0 : -1);
                  target->release(LOC);   // free sub-values etc (if any)
                  src.init_other(target, owner, LOC);
                }
           }
        else   LEFT_SYNTAX_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
Cell *
Value::get_member(const vector<const UCS_string *> & members,
                  Value * & owner, bool throw_error)
{
   owner = this;

   // members[members.size() - 1] == this
   //
   for (int m = members.size() - 2; m >= 0; --m)
       {
         if (!owner->is_member())
            {
              UCS_string & more = MORE_ERROR()
                  << "member access: non-structured variable "
                  << *members.back() << " has no member ";
              more.append_members(members, 0);
              if (throw_error)   DOMAIN_ERROR;
              else               return 0;
            }

         const UCS_string & member_ucs = *members[m];
         if (owner->get_rank() != 2)
            {
              if (!throw_error)   return 0;
              UCS_string & more = MORE_ERROR() << "member access: the rank of ";
              more.append_members(members, m);
              more << " is not 2.\n"
                      "Expecting an N×2 matrix of member,value pairs.";
              RANK_ERROR;
            }

         if (owner->get_cols() != 2)
            {
              if (!throw_error)   return 0;
              UCS_string & more = MORE_ERROR()
                 << "member access: the number of columns of ";
              more.append_members(members, m);
              more << " is not 2.\n"
                      "Expecting an N×2 matrix of member,value pairs.";
              LENGTH_ERROR;
            }

         Cell * member_cell = owner->get_member_data(member_ucs);
         if (member_cell)   // existing member
            {
              if (m == 0)   return member_cell; // final member

              // more members coming. Then member_cell should point to a
              // structured sub-member
              //
              if (!member_cell->is_pointer_cell() ||
                  !member_cell->get_pointer_value()->is_member())
                 {
                   if (!throw_error)   return 0;
                   UCS_string & more = MORE_ERROR()
                                << "member access: member " << member_ucs
                                << " exists in ";
                   more.append_members(members, m);
                   more << " but its (internal) value is not nested";
                   DOMAIN_ERROR;
                 }

              // next member
              //
              owner = member_cell->get_pointer_value().get();
            }
         else   // member does not exist
            {
              // create new member row...

              // find an unused slot in owner. get_new_member() is guaranteed
              // to find one (by possibly increasing the ravel of owner.
              //
              Cell * member_data = owner->get_new_member(member_ucs);
              if (m == 0)   return member_data;

              // more members coming
              //
              Value_P member_sub = EmptyStruct(LOC);
              new (member_data)   PointerCell(member_sub.get(), *owner);
              owner = member_sub.get();
            }   // if (member_cell == 0)
       }

   FIXME;   // not reached
}
//────────────────────────────────────────────────────────────────────────────
Cell *
Value::get_member_data(const UCS_string & member_name)
{
const ShapeItem rows = get_rows();

ShapeItem row = member_name.FNV_hash() % rows;

   // loop over the member rows. Since we hash the start position this will
   // typically return quickly and the slow case where all rows are tested
   // will throw a VALUE ERROR right after returning 0.
   //
   loop(r, rows)
       {
         if (++row >= rows)   row = 0;
         Cell cache;
         const Cell & name_cell = get_cravel(2*row, cache);

         if (name_cell.is_pointer_cell() &&
             name_cell.get_pointer_value()->equal_string(member_name))
            return &get_wravel(2*row + 1);
       }

   // member row not found
   //
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
Cell *
Value::get_new_member(const UCS_string & new_member_name)
{
   // find an unused slot in owner. Return its data cell (= its name cell + 1)
   //
   for (;;)   // until an unused row was found
       {
         const ShapeItem rows = get_rows();   // changed by double_ravel() !
         ShapeItem row = new_member_name.FNV_hash() % rows;
         loop(r, 14)
             {
               if (++row >= rows)   row = 0;
               Cell * cell = &get_wravel(2*row);
               if (cell->is_integer_cell() &&
                   cell->get_int_value() == 0)   // unused row
                  {
                    Value_P name_val(new_member_name, LOC);
                    // member names are read-only, always short, and only
                    // ever compared -- pack unconditionally (bypassing
                    // the general pack_min_length threshold, which exists
                    // to avoid packing overhead for values that might not
                    // stay short) and, for anything that ends up heap
                    // ravel'd, use the MemberNameRavel vtable instead of
                    // plain Char16Ravel so character-wise access is
                    // caught instead of silently allowed.
                    name_val->try_pack(true);
                    name_val->upgrade_member_name();
                    invalidate_depth();   // growing an existing value
                    new (cell) PointerCell(name_val.get(), *this);
                    return cell + 1;
                  }
             }

          /* at this point a cluster of 14 consecutive used rows was hit.
             Common wisdom (Knuth) has it, that a hash table should maintain
             fill level (i.e. used rows ÷ all rows) below 50%.

             For a hash table with N rows and an unknown fill level (like this
             one) we could either:

             A1. first determine the fill level (an O(N) operation), and
             A2. double the table size (another O(n) operation) if the fill
                 level is > 50 %,

                 or:

             B.   double the table size regardless of the fill level (and
                  an 1:16384 chance that the doubling was premature).


             We choose B. because it is simpler and because prematurely
             doubling the table could very well amortize itself later.
          */

         double_ravel(LOC);
       }
}
//────────────────────────────────────────────────────────────────────────────
void
Value::double_ravel(const char * loc)
{
Cell * const old_ravel = ravel.cells;

   Assert(is_member());
const char * del = 0;
   if (ravel.cells != ravel.short_value)   // heap ravel
      {
        del = reinterpret_cast<const char *>(ravel.cells);
      }

   Assert(get_rank() == 2);
   Assert(get_cols() == 2);

const ShapeItem old_rows  = get_rows();
const ShapeItem new_rows  = 2*old_rows;
const ShapeItem new_cells = 2*new_rows;
const ShapeItem old_alloc_cells = ravel.alloc_cells;   // for deallocate() below

   // std::allocator<Cell>, not new Cell[]: init_ravel()/~Value() use
   // std::allocator<Cell> throughout, and mixing allocator families is
   // alloc-dealloc-mismatch UB (Blake McBride, Bugs12.md #4). The
   // following loop placement-constructs every cell via IntCell::z0(),
   // matching allocate()'s raw/uninitialized-memory contract exactly --
   // no change needed there.
   //
Cell * doubled = std::allocator<Cell>{}.allocate(new_cells);
   loop(n, new_cells)   IntCell::z0(doubled + n);
   ravel.valid_ravel_items = new_cells;
   ravel.alloc_cells = new_cells;
   shape.set_shape_item(0, new_rows);
   ravel.cells = doubled;

   // keep total_ravel_count in sync with the grown ravel so that ~Value()
   // (which subtracts ravel.alloc_cells, just updated above) does not
   // under- or over-subtract and drift/underflow total_ravel_count.
   //
   if (del)   total_ravel_count += new_cells - old_alloc_cells; // was long already
   else       total_ravel_count += new_cells;                  // was short, now long

   loop(r, old_rows)
       {
         // transfer the member name. The old member_name_cell must be released
         // since get_new_member() below creates a new one in the new ravel.
         //
         Cell & member_name_cell = old_ravel[2*r];
         if (!member_name_cell.is_pointer_cell())   continue;   // unused
         Assert(member_name_cell.is_pointer_cell());
         UCS_string member_name(*member_name_cell.get_pointer_value());
         member_name_cell.release(LOC);

         // transfer the member value. memcpy() should work because ownership
         // remains the same
         //
         void * dest = get_new_member(member_name);
         void * old_member_data_cell = old_ravel + 2*r + 1;
         memcpy(dest, old_member_data_cell, sizeof(Cell));
       }

   // del is char *, so std::allocator<Cell>::deallocate() does not (and
   // must not -- these cells were memcpy()'d out or released() above,
   // not destructed) call any cell destructors.
   //
   if (del)
      std::allocator<Cell>{}.deallocate(
          reinterpret_cast<Cell *>(const_cast<char *>(del)), old_alloc_cells);
}
//────────────────────────────────────────────────────────────────────────────
bool
Value::is_or_contains(const cValue * value, const cValue * sub)
{
   if (value == 0)     return false;   // value is not a valid value
   if (value == sub)   return true;    // value is sub

   for (ConstRavel_P v(*value, true); +v; ++v)
      {
        if (v->is_pointer_cell() &&
            is_or_contains(v->get_pointer_value().get(), sub))   return true;
      }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
Value::init()
{
   Log(LOG_startup)
      CERR << "Max. Rank            is " << MAX_RANK << endl;
};
//────────────────────────────────────────────────────────────────────────────
void
Value::add_member(const UCS_string & member_name, Value * member_value)
{
   if (!is_structured())
      {
        MORE_ERROR() << "attempt to add member (" << member_name
                     << "to a non-structured value";
        DOMAIN_ERROR;
      }

vector<const UCS_string *> members;
   members.push_back(&member_name);
   members.push_back(&member_name);   // ignored

Value * member_owner = 0;
Cell * data = get_member(members, member_owner, false);
   Assert(member_owner);
   Assert(member_owner == this);

   // check before releasing/overwriting data: PointerCell's own
   // constructor does the equivalent check internally, but by then
   // data has already been release()'d below -- if the constructor
   // then throws (e.g. LIMIT_ERROR_NESTING), data is left released but
   // never reconstructed, a half-dead Cell that crashes the next code
   // to walk this value's ravel (its own destructor included). See
   // Bugs28 #9.
   //
   PointerCell::check_nesting_depth(member_value);

   // get_member() can return an EXISTING (already-populated) member's
   // cell, not just an unused slot -- placement-new over it without
   // releasing first leaked the old sub-tree's reference (never
   // decremented) and could double-count pointer_cell_count.
   data->release(LOC);
   invalidate_depth();   // (re-)setting an existing value's member
   new (data) PointerCell(member_value, *this);
}
//────────────────────────────────────────────────────────────────────────────
void
Value::mark_all_dynamic_values()
{
   for (DynamicObject * dob = DynamicObject::all_values.get_prev();
        dob != &all_values; dob = dob->get_prev())
       {
         dob->rValue().set_marked();
       }
}
//────────────────────────────────────────────────────────────────────────────
void
Value::rollback(ShapeItem items, const char * loc)
{
   ADD_EVENT(this, VHE_Unroll, 0, loc);

   // this value has only items < nz_element_count() valid items.
   // init the rest...
   //
   while (more())   next_ravel_0();

   alloc_loc = loc;
}
//────────────────────────────────────────────────────────────────────────────
void
Value::check_value(const char * loc)
{
#ifdef cfg_VALUE_CHECK_WANTED

   // if value was initialized by means of a next_ravel_XXX() mechanism,
   // then all cells are supposed to be OK.
   //
   if (ravel.valid_ravel_items && ravel.valid_ravel_items >= element_count())
      {
        set_complete();
        return;
      }

uint32_t error_count = 0;
// this walks the ravel as an array of real Cell objects via pointer
// arithmetic below, which is only valid for an unpacked (RPT_CELLS)
// ravel -- same as ravel.cells itself, so read it directly rather than
// through a fetcher/cache that may materialise a packed element.
const Cell * C = ravel.cells;

const ShapeItem ec = nz_element_count();
    loop(c, ec)
       {
         const CellType ctype = C->get_cell_type();
         switch(ctype)
            {
              case CT_CHAR:
              case CT_POINTER:
              case CT_CELLREF:
              case CT_INT:
              case CT_FLOAT:
              case CT_COMPLEX:   break;   // OK

              default:
                 CERR << endl
                      << "*** check_value(" << loc << ") detects:" << endl
                      << "   bad ravel[" << c << "] (CellType "
                      << ctype << ")" << endl;

                 ++error_count;
            }

         if (error_count >= 10)
            {
              CERR << endl << "..." << endl;
              break;
            }

         ++C;
       }

   if (error_count)
      {
        CERR << "Shape: " << get_shape() << endl;
        print(CERR) << endl
           << "************************************************"
           << endl;
        Assert(0 && "corrupt ravel ");
      }
#endif

   set_complete();
}
//────────────────────────────────────────────────────────────────────────────
int
Value::erase_stale(const char * loc)
{
int count = 0;

   Log(LOG_Value__erase_stale)
      CERR << endl << endl << "erase_stale() called from " << loc << endl;

   for (DynamicObject * dob = all_values.get_next();
        dob != &all_values; dob = dob->get_next())
       {
         Value & v = dob->rValue();
         if (dob == dob->get_next())   // a loop
            {
              CERR << "A loop in DynamicObject::all_values (detected in "
                      "function erase_stale() at object "
                   << voidP(dob) << "): " << endl;
              all_values.print_chain(CERR);
              CERR << endl;

              // use separate CERR << in case this crashes
              //
              CERR << " DynamicObject: " << dob << endl;
              CERR << " Value:         " << v   << endl;
              CERR << v                         << endl;
            }

         Assert(dob != dob->get_next());
         if (v.owner_count)   continue;

         ADD_EVENT(&v, VHE_Stale, v.owner_count, loc);

         Log(LOG_Value__erase_stale)
            {
              CERR << "Erasing stale Value "
                   << voidP(dob) << ":" << endl
                   << "  Allocated by " << v.where_allocated() << endl
                   << "  ";
              v.list_one(CERR, false);
            }

         // count it unless we know it is dirty
         //
         ++count;

         dob->unlink();

         // v->erase(loc) could mess up the chain, so we start over
         // rather than continuing
         //
         dob = &all_values;
       }

   return count;
}
//────────────────────────────────────────────────────────────────────────────
void
Value::erase_all(ostream & out)
{
   for (const DynamicObject * dob = DynamicObject::all_values.get_next();
        dob != &DynamicObject::all_values; dob = dob->get_next())
       {
         const Value & value = dob->rValue();
         out << "erase_all() sees Value:" << endl
             << "  Allocated by " << value.where_allocated() << endl
             << "  ";
         value.list_one(CERR, false);
       }
}
//────────────────────────────────────────────────────────────────────────────
ostream &
Value::list_all(ostream & out, bool show_owners)
{
int num = 0;
   for (const DynamicObject * dob = all_values.get_prev();
        dob != &all_values; dob = dob->get_prev())
       {
         out << "Value #" << num++ << ":";
         dob->rValue().list_one(out, show_owners);
       }

   return out << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Value::to_type(bool force_numeric)
{
   // set_ravel_Char()/set_ravel_Int() below placement-new a full 24-byte
   // Cell at Cell stride; a packed (non-RPT_CELLS) ravel's elements are
   // narrower and more closely spaced, so writing Cells directly into one
   // without unpacking first overruns adjacent elements and leaves the
   // (still-installed) packed fetcher reinterpreting Cell internals (e.g.
   // a vtable pointer) as packed data. explode_to_Cells() is a no-op if
   // the ravel is already RPT_CELLS.
   explode_to_Cells();

const ShapeItem ec = nz_element_count();

   loop(e, ec)
      {
        Cell & cell = get_wravel(e);
        if (cell.is_pointer_cell())
           {
             cell.isolate(LOC);
             cell.get_pointer_value()->to_type(force_numeric);
           }
        else if (cell.is_character_cell() && ! force_numeric)
           {
             set_ravel_Char(e, UNI_SPACE);
           }
        else
           {
             set_ravel_Int(e, 0);
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Value::explode_to_Cells()
{
   if (!is_packed())   return;

   /*
      Unpack in reverse index order so that reading packed[i] (at a low offset)
      never conflicts with the Cell[i] we are about to write (at a higher
      offset). sizeof(Cell)=24 >= sizeof(any packed element), so Cell[i]
      at i×24 is always placed at or beyond packed[i], and already-written
      Cells at i+1..N-1 are entirely above the packed data for 0..i-1.
    */

   // length 1 Cells are necer exploded: no need for nz_element_count()
const ShapeItem N = element_count();
Cell * dst = ravel.cells + N;

   switch (get_ravel_type())
      {
        case RPT_BOOL:
             rev_loop(i, N)
                 {
                   new (--dst) IntCell(Ravel::get_packed_bit(ravel.cells, i));
                 }
             break;

        case RPT_INT64:
             {
               const int64_t * src =
                     reinterpret_cast<const int64_t *>(ravel.cells) + N;
               loop(i, N)   new (--dst) IntCell(*--src);
             }
             break;

        case RPT_FLOAT64:
             {
               const double * src =
                     reinterpret_cast<const double *>(ravel.cells) + N;
             loop(i, N)   new (--dst) FloatCell(*--src);
             }
             break;

        case RPT_COMPLEX:
             {
               const double * src =
                     reinterpret_cast<const double *>(ravel.cells) + 2*N;
               loop(i, N)
                  {
                    const double imag = *--src;
                    const double real = *--src;
                    new (--dst) ComplexCell(real, imag);
                  }
             }
             break;

        case RPT_UNICODE16:
             {
               const uint16_t * src =
                     reinterpret_cast<const uint16_t *>(ravel.cells) + N;
               loop(i, N)   new (--dst) CharCell(Unicode(*--src));
             }
             break;

        case RPT_UNICODE32:
             {
               const uint32_t * src =
                     reinterpret_cast<const uint32_t *>(ravel.cells) + N;
               loop(i, N) new (--dst) CharCell(Unicode(*--src));
                 }
             break;

        default:   return;
      }

   flags.ravel_type        = RPT_CELLS;
   ravel.fetcher           = &Ravel::cell_fetcher;
   ravel.valid_ravel_items = N;
   // Packing installs a subclass vtable via placement-new (IntRavel(),
   // BoolRavel(), Char16Ravel(), ...) for heap ravels; reinstall the base
   // Ravel vtable here so indexed accessors (get_int_value(),
   // is_integer_cell(), apply_fast_dyadic(), ...) stop dispatching through
   // the stale subclass and decoding the old packed layout as Cells.
   if (ravel.cells != ravel.short_value)
      new (&ravel) Ravel(Ravel::upgrade_tag{});
}
//────────────────────────────────────────────────────────────────────────────
RavelType
Value::explode_to_UNICODE32()
{
   Assert(get_ravel_type() == RPT_UNICODE16);

   /*
      Unpack in reverse index order so that reading packed[i] (at a low offset)
      never conflicts with the Cell[i] we are about to write (at a higher
      offset). sizeof(Cell)=24 >= sizeof(any packed element), so Cell[i]
      at i×24 is always placed at or beyond packed[i], and already-written
      Cells at i+1..N-1 are entirely above the packed data for 0..i-1.
    */

const ShapeItem N = nz_element_count();
auto dst = reinterpret_cast<uint32_t *>(ravel.cells) + N;
auto src = reinterpret_cast<const uint16_t *>(ravel.cells) + N;
   loop(i, N)   *--dst = *--src;

   flags.ravel_type        = RPT_UNICODE32;
   ravel.fetcher           = &Ravel::char32_fetcher;
   ravel.valid_ravel_items = N;
   // see explode_to_Cells() for why this vtable reinstall is needed.
   if (ravel.cells != ravel.short_value)   new (&ravel) Char32Ravel();
   return RPT_UNICODE32;
}
//────────────────────────────────────────────────────────────────────────────
RavelType
Value::explode_to_COMPLEX()
{
   /*
      Unpack in reverse index order so that reading packed[i] (at a low offset)
      never conflicts with the Cell[i] we are about to write (at a higher
      offset). sizeof(Cell)=24 >= sizeof(any packed element), so Cell[i]
      at i×24 is always placed at or beyond packed[i], and already-written
      Cells at i+1..N-1 are entirely above the packed data for 0..i-1.
    */

const ShapeItem N = nz_element_count();
auto dst = reinterpret_cast<double *>(ravel.cells) + 2 * N;

   switch(get_ravel_type())
      {
        case RPT_BOOL:
             {
               rev_loop(i, N)
                   {
                     *--dst = 0.0;
                     *--dst = double(Ravel::get_packed_bit(ravel.cells, i));
                   }
             }
        break;

        case RPT_INT64:
             {
               auto src = reinterpret_cast<const int64_t *>(ravel.cells) + N;
               loop(i, N)
                   {
                     *--dst = 0.0;
                     *--dst = double(*--src);
                   }
             }
        break;

        case RPT_FLOAT64:
             {
               auto src = reinterpret_cast<const double *>(ravel.cells) + N;
               loop(i, N)
                   {
                     *--dst = 0.0;
                     *--dst = *--src;
                   }
             }
        break;

        default: FIXME;
      }

   flags.ravel_type        = RPT_COMPLEX;
   ravel.fetcher           = &Ravel::complex_fetcher;
   ravel.valid_ravel_items = N;
   // see explode_to_Cells() for why this vtable reinstall is needed.
   if (ravel.cells != ravel.short_value)   new (&ravel) ComplexRavel();
   return RPT_COMPLEX;
}
//────────────────────────────────────────────────────────────────────────────
RavelType
Value::explode_to_FLOAT64()
{
   /*
      Unpack in reverse index order so that reading packed[i] (at a low offset)
      never conflicts with the Cell[i] we are about to write (at a higher
      offset). sizeof(Cell)=24 >= sizeof(any packed element), so Cell[i]
      at i×24 is always placed at or beyond packed[i], and already-written
      Cells at i+1..N-1 are entirely above the packed data for 0..i-1.
    */

const ShapeItem N = nz_element_count();
auto dst = reinterpret_cast<double *>(ravel.cells) + N;

   switch(get_ravel_type())
      {
        case RPT_BOOL:
             {
               rev_loop(i, N)
                   {
                     *--dst = double(Ravel::get_packed_bit(ravel.cells, i));
                   }
             }
        break;

        case RPT_INT64:
             {
               auto src = reinterpret_cast<const int64_t *>(ravel.cells) + N;
               loop(i, N)   *--dst = double(*--src);
             }
        break;

        default: FIXME;
      }

   flags.ravel_type        = RPT_FLOAT64;
   ravel.fetcher           = &Ravel::float64_fetcher;
   ravel.valid_ravel_items = N;
   // see explode_to_Cells() for why this vtable reinstall is needed.
   if (ravel.cells != ravel.short_value)   new (&ravel) FloatRavel();
   return RPT_FLOAT64;
}
//────────────────────────────────────────────────────────────────────────────
RavelType
Value::explode_to_INT64()
{
   /*
      Unpack in reverse index order so that reading packed[i] (at a low offset)
      never conflicts with the Cell[i] we are about to write (at a higher
      offset). sizeof(Cell)=24 >= sizeof(any packed element), so Cell[i]
      at i×24 is always placed at or beyond packed[i], and already-written
      Cells at i+1..N-1 are entirely above the packed data for 0..i-1.
    */

const ShapeItem N = nz_element_count();
auto dst = reinterpret_cast<int64_t *>(ravel.cells) + N;

   Assert1(get_ravel_type() == RPT_BOOL);
   rev_loop(i, N)
      {
        *--dst = int64_t(Ravel::get_packed_bit(ravel.cells, i));
      }

   flags.ravel_type        = RPT_INT64;
   ravel.fetcher           = &Ravel::int64_fetcher;
   ravel.valid_ravel_items = N;
   // see explode_to_Cells() for why this vtable reinstall is needed.
   if (ravel.cells != ravel.short_value)   new (&ravel) IntRavel();
   return RPT_INT64;
}
//────────────────────────────────────────────────────────────────────────────
const char *
Value::try_implode()
{
   if (is_packed())          return "value already packed";
   if (pointer_cell_count)   return "value not simple";
   if (element_count() == 0) return "empty value — prototype only";

   /*
 *    Pack boolean bits in-place into the existing Cell buffer (front-to-back,
      64 bits per chunk).  The Cell buffer is always large enough: one uint64
      covers 64 cells × 24 bytes each.  We accumulate each chunk fully before
      writing, so the write at chunk c (byte c×8) never overlaps the cells we
      are still reading (which start at byte c×64×24 = c×1536).
    */
const ShapeItem N = element_count();
const ShapeItem Z_len = (N + 63) >> 6;   // number of uint64 chunks

   // validate before packing (two-pass): the packing loop below writes
   // each fully-accumulated 64-bit chunk into the Cell buffer *before* it
   // can discover a non-boolean cell in a *later* chunk, so a single-pass
   // version corrupts the ravel on its own failure path -- with ≥65
   // elements where the first 64 are boolean and a later one is not,
   // dst[0] would already have overwritten the first 8 bytes of
   // ravel.cells[0] by the time the non-boolean item is found, and the
   // function returns leaving flags.ravel_type unchanged, i.e. a
   // RPT_CELLS value whose first Cell has been destroyed (Bugs18 #11).
   //
   loop(b, N)
      if (!ravel.cells[b].is_near_bool())
         return "value has non-boolean items";

auto dst = reinterpret_cast<uint64_t *>(ravel.cells);

   loop(c, Z_len)
       {
         uint64_t chunk = 0;   // 64 packed bits
         const ShapeItem base = c << 6;
         const ShapeItem lim  = std::min(base + 64, N);
         for (ShapeItem b = base; b < lim; ++b)
             {
               if (ravel.cells[b].get_near_bool())
                  chunk |= uint64_t(1) << (b - base);
             }
         dst[c] = chunk;
       }

   ravel.fetcher           = &Ravel::bool_fetcher;
   ravel.valid_ravel_items = N;
   flags.ravel_type        = RPT_BOOL;
   if (ravel.cells != ravel.short_value)
      new (&ravel) BoolRavel();
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
void
Value::try_pack(bool force)
{
   if (is_packed())          return;   // already packed
   if (pointer_cell_count)   return;   // nested sub-values → cannot pack

const ShapeItem N = element_count();
   if (N < Quad_SYL::pack_min_length)
      {
        if (N == 0)   return;   // empty arrays have only a prototype — don't pack
        if (!force)   return;
        /* fall through */
      }

   // determine packing type from the first cell
   //
const Cell & c0 = ravel.cells[0];
RavelType t;
   if (c0.is_integer_cell())
      { const APL_Integer v = c0.get_int_value();
        t = (v == 0 || v == 1) ? RPT_BOOL : RPT_INT64;
      }
   else if (c0.is_float_cell())      t = RPT_FLOAT64;
   else if (c0.is_complex_cell())    t = RPT_COMPLEX;
   else if (c0.is_character_cell())
      t = c0.get_char_value() <= Unicode(0xFFFF) ? RPT_UNICODE16 : RPT_UNICODE32;
   else return;   // LvalCell or unknown → stay mixed

   // scan remaining cells; escalate t as needed
   //
   for (ShapeItem i = 1; i < N; ++i)
       {
         const Cell & c = ravel.cells[i];
         switch(c.get_cell_type())
            {
              case CT_INT:
                   { const APL_Integer v = c.get_int_value();
                     const RavelType item_t = (v == 0 || v == 1) ? RPT_BOOL : RPT_INT64;
                     if (t == RPT_BOOL && item_t == RPT_INT64)   t = RPT_INT64;
                     else if (t == RPT_FLOAT64)
                        // an INT mixed into an otherwise-all-float
                        // ravel would demote it to a packed double,
                        // silently losing its exact-integer type on
                        // readback for any later exact-integer
                        // primitive (!, *, ...) -- same class of bug
                        // as the CT_FLOAT case below, just the mirror
                        // direction (Blake McBride, Bugs22 #5).
                        return;
                     else if (t == RPT_COMPLEX)
                        // an INT cell mixed into an otherwise-all-
                        // complex ravel (this cell's own IntCell
                        // identity, not just precision, is what would
                        // be lost by packing it as a complex double
                        // pair) -- same class of bug as the CT_COMPLEX
                        // case below, just the mirror direction (Blake
                        // McBride, Bugs23 #2). Regardless of magnitude:
                        // leave the whole ravel unpacked.
                        return;
                     else if (t != RPT_BOOL && t != RPT_INT64)
                        return;   // numeric + char → mixed
                   }
                   break;

              case CT_FLOAT:
                   if (t == RPT_BOOL || t == RPT_INT64)
                      {
                        // don't escalate an int-only ravel to
                        // RPT_FLOAT64 just because one float cell
                        // showed up: that would silently demote every
                        // exact IntCell already seen to a packed
                        // double, losing exact-integer results from
                        // later primitives (!, *, ...) that stay exact
                        // only for a genuine IntCell -- observable by
                        // crossing ⎕SYL[34;2] (Blake McBride, Bugs22
                        // #5). Leave the whole ravel unpacked instead.
                        return;
                      }
                   else if (t == RPT_COMPLEX)
                        // FloatCell mixed into an otherwise-all-complex
                        // ravel -- same mirror-direction bug as the INT
                        // case above (Blake McBride, Bugs23 #2).
                        return;
                   else if (t != RPT_FLOAT64)   return;
                   break;

              case CT_COMPLEX:
                   // an INT or FLOAT cell mixed into an otherwise-all-
                   // complex ravel would pack as RPT_COMPLEX, silently
                   // losing its IntCell/FloatCell identity on readback:
                   // every element then reads back as a ComplexCell
                   // (26 ⎕CR reports it complex; ⌹/⍟/etc. take the
                   // complex code path even for a real value with a
                   // zero imaginary part), and that wrong type identity
                   // persists through )SAVE/)LOAD and ⎕TF. Same class of
                   // bug as the CT_INT/CT_FLOAT arms above, just the
                   // complex direction (Blake McBride, Bugs23 #2) --
                   // leave the whole ravel unpacked instead of
                   // escalating to RPT_COMPLEX.
                   if (t == RPT_BOOL || t == RPT_INT64 || t == RPT_FLOAT64)
                      return;
                   else if (t != RPT_COMPLEX)   return;   // char + complex → mixed
                   break;

              case CT_CHAR:
                   { const Unicode u = c.get_char_value();
                     if      (t == RPT_UNICODE16 && u > Unicode(0xFFFF))   t = RPT_UNICODE32;
                     else if (t != RPT_UNICODE16 && t != RPT_UNICODE32)     return;
                   }
                   break;

              default: return;   // LvalCell → stay mixed
            }
       }

   // pack in place (front-to-back is safe:
   // dst offset 8i < src offset 24i for all i≥0)
   //
const bool heap_ravel = ravel.cells != ravel.short_value;
   switch (t)
      {
        case RPT_BOOL:
             try_implode();   // packs in place, no reallocation; flags.ravel_type set inside
             return;

        case RPT_INT64:
             {
               auto dst = reinterpret_cast<int64_t *>(ravel.cells);
               loop(i, N)   dst[i] = ravel.cells[i].get_int_value();
               ravel.fetcher           = &Ravel::int64_fetcher;
               ravel.valid_ravel_items = N;
               flags.ravel_type = RPT_INT64;
               if (heap_ravel)   // heap ravel: Cell() doesn't touch packed data
                  new (&ravel) IntRavel();
             }
             return;

        case RPT_FLOAT64:
             {
               auto dst = reinterpret_cast<double *>(ravel.cells);
               loop(i, N)   dst[i] = ravel.cells[i].get_real_value();
               ravel.fetcher           = &Ravel::float64_fetcher;
               ravel.valid_ravel_items = N;
               flags.ravel_type = RPT_FLOAT64;
               if (heap_ravel)   new (&ravel) FloatRavel();
             }
             return;

        case RPT_COMPLEX:
             {
               auto dst = reinterpret_cast<double *>(ravel.cells);
               loop(i, N)
                  { const double r = ravel.cells[i].get_real_value();
                    const double m = ravel.cells[i].get_imag_value();
                    dst[2*i]   = r;
                    dst[2*i+1] = m;
                  }
               ravel.fetcher           = &Ravel::complex_fetcher;
               ravel.valid_ravel_items = N;
               flags.ravel_type = RPT_COMPLEX;
               if (heap_ravel)   new (&ravel) ComplexRavel();
             }
             return;

        case RPT_UNICODE32:
             {
               auto dst = reinterpret_cast<uint32_t *>(ravel.cells);
               loop(i, N)   dst[i] = ravel.cells[i].get_char_value();
               ravel.fetcher           = &Ravel::char32_fetcher;
               ravel.valid_ravel_items = N;
               flags.ravel_type = RPT_UNICODE32;
               if (heap_ravel)   new (&ravel) Char32Ravel();
             }
             return;

        case RPT_UNICODE16:
             {
               auto dst = reinterpret_cast<uint16_t *>(ravel.cells);
               loop(i, N)   dst[i] = uint16_t(ravel.cells[i].get_char_value());
               ravel.fetcher           = &Ravel::char16_fetcher;
               ravel.valid_ravel_items = N;
               flags.ravel_type = RPT_UNICODE16;
               if (heap_ravel)   new (&ravel) Char16Ravel();
             }
             return;

        default: return;
      }
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
cValue::packed_bytes_per_item() const
{
const RavelType rt = get_ravel_type();
   // NOTE: this used to be `if (rt & RPT_real) return 8;` -- but
   // RPT_BOOL == 0x0201 and RPT_real == 0x3200 (RPT_real's definition
   // ORs in RPT_integer, which ORs in RPT_BOOL, before masking to the
   // one-hot high byte), so RPT_BOOL & RPT_real == 0x0200 != 0: a
   // bit-packed boolean ravel matched this test and returned 8, even
   // though bpi==0 is documented (and intended) for RPT_BOOL just below.
   // Every caller treats bpi>0 as "byte-addressable packed elements" and
   // memcpy's raw bytes per element on that assumption -- for a bit-packed
   // value that reads 8-byte units per *bit*, i.e. stale pre-pack garbage.
   // Test the two intended types explicitly instead of a bitmask.
   if (rt == RPT_INT64 || rt == RPT_FLOAT64)   return 8;
   if (rt == RPT_COMPLEX)   return 16;
   if (rt == RPT_UNICODE32) return 4;
   if (rt == RPT_UNICODE16) return 2;
   return 0;   // RPT_BOOL (bit-packed) or RPT_CELLS (Cell* — not memcpy-safe)
}
//────────────────────────────────────────────────────────────────────────────
void
Value::commit_ravel_like(const cValue & B, ShapeItem n)
{
   Assert(ravel.valid_ravel_items == 0);
   switch (B.get_ravel_type())
      {
        case RPT_INT64:
             if (ravel.cells != ravel.short_value)
                new (&ravel) IntRavel();   // heap ravel: safe; short ravel: base Ravel handles RPT_INT64
             ravel.fetcher    = &Ravel::int64_fetcher;
             flags.ravel_type = RPT_INT64;
             break;
        case RPT_FLOAT64:
             if (ravel.cells != ravel.short_value)
                new (&ravel) FloatRavel();
             ravel.fetcher    = &Ravel::float64_fetcher;
             flags.ravel_type = RPT_FLOAT64;
             break;
        case RPT_COMPLEX:
             if (ravel.cells != ravel.short_value)
                new (&ravel) ComplexRavel();
             ravel.fetcher    = &Ravel::complex_fetcher;
             flags.ravel_type = RPT_COMPLEX;
             break;
        case RPT_UNICODE32:
             if (ravel.cells != ravel.short_value)
                new (&ravel) Char32Ravel();
             ravel.fetcher    = &Ravel::char32_fetcher;
             flags.ravel_type = RPT_UNICODE32;
             break;
        case RPT_UNICODE16:
             if (ravel.cells != ravel.short_value)
                new (&ravel) Char16Ravel();
             ravel.fetcher    = &Ravel::char16_fetcher;
             flags.ravel_type = RPT_UNICODE16;
             break;
        case RPT_BOOL:
             if (ravel.cells != ravel.short_value)
                new (&ravel) BoolRavel();
             ravel.fetcher    = &Ravel::bool_fetcher;
             flags.ravel_type = RPT_BOOL;
             break;
        default: return;
      }
   ravel.valid_ravel_items = n;
}
//────────────────────────────────────────────────────────────────────────────
void
Value::pack_like(const cValue & B)
{
   if (is_packed())          return;
   if (pointer_cell_count)   return;

const ShapeItem N = nz_element_count();
   if (N < Quad_SYL::pack_min_length)   return;

   switch (B.get_ravel_type())
      {
        case RPT_BOOL:
             try_implode();
             return;

        case RPT_INT64:
             {
               // validate before writing in place: a non-integer cell's
               // get_int_value() throws DOMAIN_ERROR, and this loop had
               // no such guard, so a throw mid-loop left the ravel half
               // packed / half (destroyed) Cells with flags.ravel_type
               // still RPT_CELLS (Bugs18 #11; every current caller is
               // safe since Z is always a permutation or copy of B, but
               // nothing enforced that).
               //
               loop(i, N)
                  if (!ravel.cells[i].is_integer_cell())   return;

               auto dst = reinterpret_cast<int64_t *>(ravel.cells);
               loop(i, N)   dst[i] = ravel.cells[i].get_int_value();
               ravel.fetcher           = &Ravel::int64_fetcher;
               ravel.valid_ravel_items = N;
               flags.ravel_type = RPT_INT64;
               if (ravel.cells != ravel.short_value)
                  new (&ravel) IntRavel();   // heap ravel: Cell() doesn't touch packed data
             }
             return;

        case RPT_FLOAT64:
             {
               // validate before writing in place -- see the RPT_INT64
               // case above (Bugs18 #11).
               //
               loop(i, N)
                  if (!ravel.cells[i].is_numeric())   return;

               auto dst = reinterpret_cast<double *>(ravel.cells);
               loop(i, N)   dst[i] = ravel.cells[i].get_real_value();
               ravel.fetcher           = &Ravel::float64_fetcher;
               ravel.valid_ravel_items = N;
               flags.ravel_type = RPT_FLOAT64;
               if (ravel.cells != ravel.short_value)
                  new (&ravel) FloatRavel();
             }
             return;

        case RPT_COMPLEX:
             {
               // validate before writing in place -- see the RPT_INT64
               // case above (Bugs18 #11).
               //
               loop(i, N)
                  if (!ravel.cells[i].is_numeric())   return;

               auto dst = reinterpret_cast<double *>(ravel.cells);
               // read both parts into locals BEFORE writing dst[2*i]:
               // dst aliases ravel.cells (in place), and dst[2*i]'s first
               // 8 bytes ARE cells[i]'s vptr (sizeof(double)==8, matching
               // Cell's leading vptr) -- writing dst[2*i] first, then
               // calling the virtual get_imag_value() on the same cell,
               // dispatches through the just-clobbered vptr. try_pack()
               // (which this is a copy of, with the temporaries removed)
               // already gets this right.
               loop(i, N)
                  { const APL_Float r = ravel.cells[i].get_real_value();
                    const APL_Float m = ravel.cells[i].get_imag_value();
                    dst[2*i]   = r;
                    dst[2*i+1] = m;
                  }
               ravel.fetcher           = &Ravel::complex_fetcher;
               ravel.valid_ravel_items = N;
               flags.ravel_type = RPT_COMPLEX;
               if (ravel.cells != ravel.short_value)
                  new (&ravel) ComplexRavel();
             }
             return;

        case RPT_UNICODE32:
             {
               // validate before writing in place -- see the RPT_INT64
               // case above (Bugs18 #11).
               //
               loop(i, N)
                  if (!ravel.cells[i].is_character_cell())   return;

               auto dst = reinterpret_cast<Unicode *>(ravel.cells);
               loop(i, N)   dst[i] = ravel.cells[i].get_char_value();
               ravel.fetcher           = &Ravel::char32_fetcher;
               ravel.valid_ravel_items = N;
               flags.ravel_type = RPT_UNICODE32;
               if (ravel.cells != ravel.short_value)
                  new (&ravel) Char32Ravel();
             }
             return;

        case RPT_UNICODE16:
             {
               // validate before writing in place -- see the RPT_INT64
               // case above (Bugs18 #11).
               //
               loop(i, N)
                  if (!ravel.cells[i].is_character_cell())   return;

               auto dst = reinterpret_cast<uint16_t *>(ravel.cells);
               loop(i, N)   dst[i] = uint16_t(ravel.cells[i].get_char_value());
               ravel.fetcher           = &Ravel::char16_fetcher;
               ravel.valid_ravel_items = N;
               flags.ravel_type = RPT_UNICODE16;
               if (ravel.cells != ravel.short_value)
                  new (&ravel) Char16Ravel();
             }
             return;

        default:   // RPT_CELLS: B not packed, fall back to type-scanning try_pack
             try_pack();
             return;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Value::assign_cell(ShapeItem offset, const Cell & C, const char * loc)
{
   /* For packed ravels: write directly into packed storage when the new cell
      type is compatible, so the ravel stays packed.  Only explode to Cell when
      the new type is incompatible with the current packing.

      Structure: the top-level switch is the new Cell. In total there are
                 6 Cell types and 7 packing types, thus 42 combinations.

                 The top-level switch handles the incoming Cell types.
                 There are always 3 possibilities:

                 A. exact match,           e.g.  RPT_FLOAT64 + CT_FLOAT
                 B. compatible match #1,   e.g.  RPT_INT64   + CT_FLOAT
                 C. compatible match #2,   e.g.  RPT_FLOAT64 + CT_INT
                 D. incompatible,          e.g.  RPT_INT64   + CT_CHAR

                 case B. calls the proper explode() function and proceeds
                         like case A.
    */

   if (is_packed())
      {
        RavelType rt = get_ravel_type();   // current packing
        switch(C.get_cell_type())                // next Cell
           {
             case CT_INT:
                  {
                    if (rt == RPT_BOOL)   rt = explode_to_INT64();   // B→A

                    const APL_Integer v = C.get_int_value();
                    if (rt == RPT_INT64)   // A.
                       {
                         reinterpret_cast<int64_t *>(ravel.cells)[offset] = v;
                         return;   // compatible
                       }
                    else if (rt == RPT_FLOAT64)   // C
                       {
                         reinterpret_cast<double *>
                                         (ravel.cells)[offset] = double(v);
                         return;   // compatible
                       }
                    else if (rt == RPT_COMPLEX)   // C
                       {
                         auto p = reinterpret_cast<double *>
                                                  (ravel.cells) + 2*offset;
                         *p++ = double(v);   // real
                         *p++ = 0.0;         // imag
                         return;   // compatible
                       }
                  }
                  // D.: explode_to_Cells() below
                  break;

             case CT_FLOAT:
                  {
                    if (rt & RPT_integer)   rt = explode_to_FLOAT64();   // B→A

                    const APL_Float f = C.get_real_value();
                    if (rt == RPT_FLOAT64)   // A.
                       {
                         reinterpret_cast<double *>(ravel.cells)[offset] = f;
                         return;
                       }

                    if (rt == RPT_COMPLEX)   // C.
                       {
                         auto p = reinterpret_cast<double *>
                                                (ravel.cells) + offset * 2;
                         *p++ = f;
                         *p++ = 0.0;
                         return;
                       }
                  }
                  // D.: explode_to_Cells() below
                  break;

             case CT_COMPLEX:
                  if (rt & RPT_real)   rt = explode_to_COMPLEX();   // B→A
                  if (rt == RPT_COMPLEX)   // A.
                     {
                       auto p = reinterpret_cast<double *>
                                                    (ravel.cells) + offset * 2;
                       *p++ = C.get_real_value();
                       *p++ = C.get_imag_value();
                       return;
                     }
                  // D.: explode_to_Cells() below
                  break;

             case CT_CHAR:
                  {
                    const Unicode u = C.get_char_value();
                    if (rt == RPT_UNICODE16 && u > Unicode(0xFFFF))
                       rt = explode_to_UNICODE32();   // B. → A.

                    if (rt == RPT_UNICODE16)   // most likely
                       {
                         reinterpret_cast<uint16_t *>(ravel.cells)[offset] = u;
                         return;
                       }
                    else if (rt == RPT_UNICODE32)   // rarely
                       {
                         reinterpret_cast<uint32_t *>(ravel.cells)[offset] = u;
                         return;
                       }
                  }
                  // D.: explode_to_Cells() below
                  break;

             default: break;   // PointerCell, LvalCell (always D.)
           }

        explode_to_Cells();   // incompatible type: convert back to Cell array
      }

   // Cell path. Either C can not be packed (as e.g. PointerCells)
   // or C does noy fit into the current packing (eg char in numeric packing
   //
   depth_update_for_overwrite(offset, C.is_pointer_cell() ? 0 : -1);
   ravel.cells[offset].release(loc);
   C.init_other(&ravel.cells[offset], *this, loc);
}
//────────────────────────────────────────────────────────────────────────────
int
Value::print_incomplete(ostream & out)
{
std::vector<const cValue *> incomplete;
bool goon = true;

   for (const DynamicObject * dob = all_values.get_prev();
        goon && (dob != &all_values); dob = dob->get_prev())
       {
         const Value & value = dob->rValue();
         goon = (dob != dob->get_prev());

         if (value.is_complete())   continue;

         out << "incomplete value at " << voidP(&value) << endl;
         incomplete.push_back(&value);

         if (!goon)
            {
              out << "Value::print_incomplete() : endless loop in "
                     "Value::all_values; stopping display." << endl;
            }
       }

   // then print more info...
   //
int count = 0;
   loop(s, incomplete.size())
      {
        { const Value * vp = static_cast<const Value *>(incomplete[s]);
          vp->print_stale_info(out, vp); }
        if (++count > 20)   // its getting boring
           {
             CERR << endl << " ... ( " << (incomplete.size() - count) 
                  << " more incomplete values)..." << endl;
             break;
           }
      }

   return incomplete.size();
}
//────────────────────────────────────────────────────────────────────────────
int
Value::print_stale(ostream & out)
{
std::vector<const cValue *> stale_vals;
std::vector<const DynamicObject *> stale_dobs;
bool goon = true;
int count = 0;

   // first print addresses and remember stale values
   //
   for (const DynamicObject * dob = all_values.get_prev();
        goon && (dob != &all_values); dob = dob->get_prev())
       {
         const Value & value = dob->rValue();
         goon = (dob != dob->get_prev());

         if (value.owner_count)   continue;

         out << "stale value at " << voidP(&value) << endl;
         stale_vals.push_back(&value);
         stale_dobs.push_back(dob);

         if (!goon)
            {
              out << "Value::print_stale() : endless loop in "
                     "Value::all_values; stopping display." << endl;
            }
       }

   // then print more info...
   //
   loop(s, stale_vals.size())
      {
        const DynamicObject * dob = stale_dobs[s];
        const cValue * val = stale_vals[s];
        val->print_stale_info(out, dob);
        if (++count > 20)   // its getting boring
           {
             CERR << endl << " ... ( " << (stale_vals.size() - count) 
                  << " more stale values)..." << endl;
             break;
           }
       }

   // mark all dynamic values, and then unmark those known in the workspace
   //
   mark_all_dynamic_values();
   Workspace::unmark_all_values();
   Macro::unmark_all_macros();

   // print all values that are still marked
   //
   for (const DynamicObject * dob = all_values.get_prev();
        dob != &all_values; dob = dob->get_prev())
       {
         const Value & value = dob->rValue();

         // don't print values found in the previous round.
         //
         bool known_stale = false;
         loop(s, stale_vals.size())
            {
              if (&value == stale_vals[s])
                 {
                   known_stale = true;
                   break;
                 }
            }
         if (known_stale)   continue;

         if (value.is_marked())
            {
              ++count;
              if (count < 20)   value.print_stale_info(out, dob);
              else if (count == 20)
              CERR << endl << " ... (more stale values)..." << endl;
              value.unmark();
            }
       }

   return count;
}
//────────────────────────────────────────────────────────────────────────────
int
Value::check_all_Cells(ostream & out)
{
int errors = 0;

   for (const DynamicObject * dob = all_values.get_prev();
        dob != &all_values; dob = dob->get_prev())
       {
         const Value & value = dob->rValue();
         if (value.check_Cells(out))   ++errors;
       }

   return errors;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Value::glue_item_item(Value & item_A, Value & item_B, const char * loc)
{
   // glue two items together, starting a strand
   //
   Log(LOG_glue)
      {
        CERR << "gluing two items " << endl << item_A
             << " and " << endl << item_B << endl;
      }

Value_P Z(2, LOC);
   Z->next_ravel_Value(&item_A);
   Z->next_ravel_Value(&item_B);

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Value::glue_item_strand(Value & item_A, const Value & strand_B,
                        const char * loc)
{
   // glue a new item A to the strand B
   //
const ShapeItem len_A = item_A.element_count();
const ShapeItem len_B = strand_B.element_count();
   Log(LOG_glue)
      {
        CERR << "gluing item[" << len_A << "] " << endl << item_A
             << " to strand[" << len_B << "] " << endl << strand_B << endl;
      }

   Assert(strand_B.is_scalar_or_vector());

Value_P Z(len_B + 1, LOC);
   Z->next_ravel_Value(&item_A);

   loop(b, len_B)
       { Cell cache; Z->next_ravel_Cell(strand_B.get_cravel(b, cache)); }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Value::glue_strand_item(const Value & strand_A, Value & item_B,
                        const char * loc)
{
   // glue a strand A to new item B
   //
   Log(LOG_glue)
      {
        CERR << "gluing strand " << endl << strand_A
             << " to item " << endl << item_B << endl;
      }

   Assert(strand_A.is_scalar_or_vector());

const ShapeItem len_A = strand_A.element_count();
Value_P Z(len_A + 1, LOC);

   loop(a, len_A)
       { Cell cache; Z->next_ravel_Cell(strand_A.get_cravel(a, cache)); }
   Z->next_ravel_Value(&item_B);

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Value::glue_strand_strand(const Value & strand_A, const Value & strand_B,
                          const char * loc)
{
   // glue two strands A and B together
   //
const ShapeItem len_A = strand_A.element_count();
const ShapeItem len_B = strand_B.element_count();

   Log(LOG_glue)
      {
        CERR << "gluing strands " << endl << strand_A
             << "with shape " << strand_A.get_shape() << endl
             << " and " << endl << strand_B << endl
             << "with shape " << strand_B.get_shape() << endl;
      }

   Assert(strand_A.is_scalar_or_vector());
   Assert(strand_B.is_scalar_or_vector());

Value_P Z(len_A + len_B, LOC);

   loop(a, len_A)
       { Cell cache; Z->next_ravel_Cell(strand_A.get_cravel(a, cache)); }
   loop(b, len_B)
       { Cell cache; Z->next_ravel_Cell(strand_B.get_cravel(b, cache)); }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
bool
Value::check_WS_FULL(const char * args, ShapeItem requested_cell_count,
                     const char * loc)
{
const int64_t used_memory
               = (total_ravel_count + requested_cell_count) * sizeof(Cell)
               + (value_count + 1) * sizeof(Value)
               + Workspace::SI_entry_count() * sizeof(StateIndicator);

   if (int64_t((Quad_WA::total_memory*Quad_WA::WA_scale/100)) >
       (used_memory + Quad_WA::WA_margin))   return false;   // OK

   Log(LOG_Value_alloc) CERR
   << "    value_count:       " << value_count             << endl
   << "    total_ravel_count: " << total_ravel_count       << " cells" << endl
   << "    new cell_count:    " << requested_cell_count    << " cells" << endl
   << "    total_memory:      " << Quad_WA::total_memory   << " bytes" << endl
   << "    used_memory:       " << used_memory             << " bytes" << endl
   << "    ⎕WA margin:        " << Quad_WA::WA_margin      << " bytes" << endl
   << "    ⎕WA scale:         " << Quad_WA::WA_scale       << "%" << endl

           << " at " << LOC << endl;
   return true;
}
//────────────────────────────────────────────────────────────────────────────
void
Value::catch_Error(const Error & error, const char * args, const char * loc)
{
   Log(LOG_Value_alloc)   CERR << "Ravel allocation failed" << endl;

   // keep a more specific )MORE text, e.g. the ⎕SYL limit (Bugs37 #49)
   //
   if (Workspace::more_error().size() == 0)
      MORE_ERROR() << "new Value(" << args
                   << ") failed (APL error in ravel allocation)";
   throw error;   // rethrow
}
//────────────────────────────────────────────────────────────────────────────
void
Value::catch_exception(const exception & ex, const char * args,
                      const char * caller,  const char * loc)
{
const int64_t used_memory
               = total_ravel_count * sizeof(Cell)
               + value_count * sizeof(Value)
               + Workspace::SI_entry_count() * sizeof(StateIndicator);

   Log(LOG_Value_alloc)
      CERR << "Value_P::Value_P(" << args << ") failed at " << loc
           << " (caller: "        << caller << ")" << endl
           << " what: "           << ex.what() << endl
           << " initial sbrk(): 0x" << hex << Quad_WA::initial_sbrk << endl
           << " current sbrk(): 0x" << top_of_memory() << endl
           << " alloc_size:     0x" << alloc_size << dec << " ("
                                    << alloc_size << ")" << endl
           << " used memory:    0x" << hex  << used_memory << dec
                                    << " (" << used_memory << ")" << endl;

   MORE_ERROR() << "new Value(" << args << ") failed (" << ex.what() << ")";
   WS_FULL;
}
//────────────────────────────────────────────────────────────────────────────
void
Value::catch_ANY(const char * args, const char * caller, const char * loc)
{
   Log(LOG_Value_alloc)
      CERR << "Value_P::Value_P(Shape " << args << " failed at " << loc
           << " (caller: " << caller << ")" << endl;
   MORE_ERROR() << "new Value(" << args << ") failed (ANY)";
   WS_FULL;
}
//────────────────────────────────────────────────────────────────────────────
void
Value::init_ravel()
{
   owner_count = 0;
   ravel.fetcher = &Ravel::cell_fetcher;
   pointer_cell_count = 0;
   ravel.nz_subcell_count = 0;
   check_ptr = 0;
   IntCell::z0(ravel.short_value);
   ravel.cells = ravel.short_value;

   ++value_count;
   if (Quad_SYL::value_count_limit &&
       Quad_SYL::value_count_limit < APL_Integer(value_count))
      {
        MORE_ERROR() <<
"the system limit on the APL value count (as set in ⎕SYL["
<< (Quad_SYL::SYL_VALUE_COUNT_LIMIT + Workspace::get_IO())
<< ";2]) was reached\n"
"(and to avoid lock-up, this system limit in ⎕SYL was automatically cleared).";

        // reset the limit so that we don't get stuck here.
        //
        Quad_SYL::value_count_limit = 0;
        InterruptContext::set_attention_raised(LOC);
        InterruptContext::set_interrupt_raised(LOC);
      }

const ShapeItem length = shape.checked_volume();

   // small values always succeed...
   //
   if (length <= cfg_SHORT_VALUE_LENGTH_WANTED)
      {
        check_ptr = charP(this) + 7;
        return;
      }

   // large Value. If the compiler uses 4-byte pointers, then do not allow APL
   // values that are too large. The reason is that new[] may return
   // a non-0 pointer (thus pretending everything is OK), but a subsequent
   // attempt to initialize the value might then throw a segfault.
   //
   enum { MAX_LEN = 2000000000 / sizeof(Cell) };
   if (length > MAX_LEN && sizeof(void *) < 6)
      throw_apl_error(E_WS_FULL, alloc_loc);

   if (Quad_SYL::ravel_count_limit &&
       Quad_SYL::ravel_count_limit < APL_Integer(total_ravel_count + length))
      {
        // throw WS FULL right away (like for MAX_LEN above) rather than
        // continuing with a fake value and an INTERRUPT later, which
        // computed a wrong result and leaked the value (Bugs37 #49).
        // The limit remains in place; ⎕SYL[…;2]←0 clears it.
        //
        MORE_ERROR() <<
"the system limit on the total ravel size (as set in ⎕SYL["
<< (Quad_SYL::SYL_RAVEL_BYTES_LIMIT + Workspace::get_IO())
<< ";2]) was reached";
        throw_apl_error(E_WS_FULL, alloc_loc);
      }

   alloc_size = length * sizeof(Cell);
   ravel.cells = std::allocator<Cell>{}.allocate(length);
   ravel.alloc_cells = length;

/*
   ravel = 0;   // assume new() fails
// try
//    {
        ravel = reinterpret_cast<Cell *>(new char[length * sizeof(Cell)]);
        if (ravel == 0)   // new failed (without trowing an exception)
           {
              Log(LOG_Value_alloc)
                 CERR << "new char[" << (length * sizeof(Cell))
                      << "] (aka. long ravel allocation) returned 0 at " LOC
                      << endl;

              MORE_ERROR() << "The instatiation of a Value object succeeded, "
                              "but allocation of its (large) ravel failed.";
              new (&shape) Shape();
              ravel = short_value;
              throw_apl_error(E_WS_FULL, alloc_loc);
           }
      }
   catch (...)
      {
        // for some unknown reason, this object gets cleared after
        // throwing the E_WS_FULL below (which destroys the prev and
        // next pointers. We therefore unlink() the object here (where
        // prev and next pointers are still intact).
        //
        unlink();

        Log(LOG_Value_alloc)
           {
             CERR << "new char[" << (length * sizeof(Cell))
                  << "] (aka. long ravel allocation) threw an exception at " LOC
                  << endl;
           }

        MORE_ERROR() << "The instatiation of a Value object succeeded, "
                        "but allocation of its (large) ravel failed.";
        new (&shape) Shape();
        ravel = short_value;
        throw_apl_error(E_WS_FULL, alloc_loc);
      }
*/

   // init the first ravel element to (prototype) 0 so that we can avoid
   // many empty checks all over the place
   //
   IntCell::z0(&get_wproto());
   check_ptr = charP(this) + 7;
   total_ravel_count += length;
}
//════════════════════════════════════════════════════════════════════════════
ostream &
operator <<(ostream & out, const Value & v)
{
   v.print(out);
   return out;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
IntScalar(APL_Integer val, const char * loc)
{
Value_P Z(loc);
   Z->next_ravel_Int(val);
   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
FloatScalar(APL_Float val, const char * loc)
{
Value_P Z(loc);
   Z->next_ravel_Float(val);
   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
ComplexScalar(APL_Complex val, const char * loc)
{
Value_P Z(loc);
   Z->next_ravel_Complex(val);
   Z->check_value(loc);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
ComplexScalar(APL_Float real, APL_Float imag, const char * loc)
{
Value_P Z(loc);
   Z->next_ravel_Complex(real, imag);
   Z->check_value(loc);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
CharScalar(Unicode uni, const char * loc)
{
Value_P Z(loc);
   Z->next_ravel_Char(uni);
   Z->check_value(loc);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Idx0(const char * loc)
{
Value_P Z(ShapeItem(0), loc);
   // default proto is 0, so no need to set it here
   Z->check_value(loc);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Str0(const char * loc)
{
Value_P Z(ShapeItem(0), loc);
   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Str0_0(const char * loc)
{
Shape sh(ShapeItem(0), ShapeItem(0));
Value_P Z(sh, loc);
   Z->set_proto_Spc();
   Z->check_value(loc);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Idx0_0(const char * loc)
{
Shape sh(ShapeItem(0), ShapeItem(0));
Value_P Z(sh, loc);
   Z->check_value(loc);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
EmptyStruct(const char * loc)
{
   enum {
          rows = 8,
          cols = 2,
          items = rows * cols
        };

const Shape shape_Z(rows, cols);
Value_P Z(shape_Z, loc);
   loop(z, items)  Z->next_ravel_0();
   Z->check_value(LOC);
   Z->set_member();
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
/// declared in PrintOperator.hh
ostream &
operator << (ostream & out, const AP_num3 & ap3)
{
   return out << ap3.proc << "." << ap3.parent << "." << ap3.grand;
}
//════════════════════════════════════════════════════════════════════════════

