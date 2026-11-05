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
#include "Bif_F12_PARTITION_PICK.hh"
#include "CDR_string.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "Common.hh"
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
#include "SystemVariable.hh"
#include "UCS_string.hh"
#include "UserFunction.hh"
#include "UserPreferences.hh"
#include "Value.hh"
#include "ValueHistory.hh"
#include "Workspace.hh"

#include "Workspace.icc"

//────────────────────────────────────────────────────────────────────────────
bool
cValue::is_apl_char_vector() const
{
   if (get_rank() != 1)   return false;

   loop(c, get_shape_item(0))
      {
        if (!is_character_cell(c))   return false;

        const Unicode uni = get_char_value(c);
        if (Avec::find_char(uni) == Avec::Invalid_CHT)   // not in ⎕AV
           return false;
      }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
cValue::is_char_array() const
{
   loop(c, nz_element_count())   // also check prototype
      if (!is_character_cell(c))   return false;   // not char
   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
cValue::get_sole_bool() const
{
   if (element_count() != 1)
      {
        if (element_count())
           MORE_ERROR() << "Boolean array has length " << element_count()
                        << " (expecting length 1).";
        else
           MORE_ERROR() << "Boolean array is empty (expecting length 1).";
        LENGTH_ERROR;
      }

Cell cache;
const Cell & c0 = get_cfirst(cache);
   if (c0.is_near_one())    return true;
   if (c0.is_near_zero())   return false;

   MORE_ERROR() << "array is not Boolean (expecting 0 or 1).";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
bool
cValue::is_simple() const
{
   if (flags.ravel_type)   return true;   // packed ravel → no PointerCells → simple

const ShapeItem count = element_count();

   loop(c, count)
       {
         if (is_pointer_cell(c))   return false;
         if (is_lval_cell(c))      return false;
       }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
cValue::is_one_dimensional() const
{
   // lrm would not return false if the value itself is, e.g. a matrix.
   // That is wrong, however
   //
   if (get_rank() > 1)   return false;

const ShapeItem count = nz_element_count();

   loop(c, count)
       {
         if (is_pointer_cell(c))
            {
             Value_P sub_val = get_pointer_value(c);
             if (sub_val->get_rank() > 1)                return false;
             if (!sub_val->is_one_dimensional())         return false;
            }
       }

   return true;   // all items are scalars or vectors
}
//────────────────────────────────────────────────────────────────────────────
bool
cValue::is_int_array() const
{
const ShapeItem ec = nz_element_count();
   loop(c, ec)
       {
         if (!is_near_int(c))   return false;
       }

   return true;   // all ravel items are near-int
}
//────────────────────────────────────────────────────────────────────────────
bool
cValue::is_complex(bool check_numeric) const
{
const ShapeItem ec = nz_element_count();

   loop(e, ec)
      {
        Cell cache;
        const Cell & cell = get_cravel(e, cache);
        if (!cell.is_numeric())
           {
             if (check_numeric)    DOMAIN_ERROR;
             else                  continue;
           }
        if (!cell.is_near_real())   return true;
      }

   return false;   // all cells numeric and not complex
}
//────────────────────────────────────────────────────────────────────────────
bool
cValue::is_structured() const
{
   if (!is_member())      return false;
   if (get_rank() != 2)   return false;
   if (get_cols() != 2)   return false;

const ShapeItem rows = get_rows();
   loop(r, rows)
       {
         Cell cache;
         const Cell & key = get_cravel(2*r, cache);
         if (key.is_integer_cell() &&
             key.get_int_value() == 0)                    ;   // OK: unused row
         else if (key.is_pointer_cell() &&
             key.get_pointer_value()->is_char_vector())   ;   // OK: key
         else return false;
       }

   return true;    // OK: all rows were OK
}
//────────────────────────────────────────────────────────────────────────────
bool
cValue::can_be_compared() const
{
const ShapeItem count = nz_element_count();
   loop(c, count)
      {
       Cell cache;
       const Cell & cell = get_cravel(c, cache);
       const CellType ctype = cell.get_cell_type();
       if (ctype & (CT_CHAR | CT_INT | CT_FLOAT))   continue;
       if (cell.is_near_real())                     continue;
       if (cell.is_pointer_cell() &&
           cell.get_pointer_value()->can_be_compared())   continue;
       return false;
      }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
cValue::equal_string(const UCS_string & ucs) const
{
   if (get_rank() == 1)        // most likely: char vector
      {
        const ShapeItem len = element_count();
        if (ucs.ssize() != len)   return false;   // wrong length

        if (get_ravel_type() == RPT_UNICODE16)
           {
             // bulk-compare directly against the packed buffer instead of
             // len virtual get_char_value() calls -- also the reason
             // this doesn't crash on a MemberNameRavel-backed member
             // name, whose get_char_value() is deliberately disabled
             // (see Ravel.hh): this path never calls it.
             const uint16_t * packed = cravel_unicode16();
             loop(u, len)
                 if (ucs[u] != Unicode(packed[u]))   return false;
             return true;
           }

        loop(u, len)
            if (ucs[u] != get_char_value(u))   // mismatch
               return false;
        return true;
      }
   else if (get_rank() == 0)   // rarely: char scalar
      {
        if (ucs.size() != 1)   return false;   // wrong length
        return ucs[0] == get_char_value(0);
      }

   RANK_ERROR;            // never
}
//────────────────────────────────────────────────────────────────────────────
const Cell *
cValue::get_existing_member(const vector<const UCS_string *> & members) const
{
const cValue * parent = static_cast<const cValue *>(this);

   // members[members.size() - 1] == this
   //
   for (int m = members.size() - 2; m >= 0; --m)
       {
         if (!parent->is_member())
            {
              UCS_string & more = MORE_ERROR()
                  << "member access: non-structured variable "
                  << *members.back() << " has no member ";
              more.append_members(members, 0);
              DOMAIN_ERROR;
            }

         const UCS_string & member_ucs = *members[m];
         if (parent->get_rank() != 2)
            {
              UCS_string & more = MORE_ERROR() << "member access: the rank of ";
              more.append_members(members, m);
              more << " is not 2.\n"
                      "Expecting an N×2 matrix of member,value pairs.";
              RANK_ERROR;
            }

         if (parent->get_cols() != 2)
            {
              UCS_string & more = MORE_ERROR()
                 << "member access: the number of columns of ";
              more.append_members(members, m);
              more << " is not 2.\n"
                      "Expecting an N×2 matrix of member,value pairs.";
              LENGTH_ERROR;
            }

         const Cell * member_cell = parent->get_member_data(member_ucs);
         if (member_cell)   // existing member
            {
              if (m == 0)   return member_cell; // final member

              // more members coming. Then member_cell should point to a
              // structured sub-member
              //
              if (!member_cell->is_pointer_cell() ||
                  !member_cell->get_pointer_value()->is_member())
                 {
                   UCS_string & more = MORE_ERROR()
                                << "member access: member " << member_ucs
                                << " exists in ";
                   more.append_members(members, m);
                   more << " but its (internal) value is not nested";
                   DOMAIN_ERROR;
                 }

              // next member
              //
              parent = member_cell->get_pointer_value().get();
            }
         else   // member does not exist
            {
              UCS_string & more = MORE_ERROR()
                                       << "member access: structure ";
              more.append_members(members, m + 1);
              more << " has no member '" << member_ucs << "'.";
              VALUE_ERROR;
            }   // if (member_cell == 0)
       }

   FIXME;   // not reached
}
//────────────────────────────────────────────────────────────────────────────
const Cell *
cValue::try_existing_member(const vector<const UCS_string *> & members) const
{
   // Bugs28 #53: a tolerant sibling of get_existing_member(), for
   // callers (⎕EX, )ERASE) that only want to know whether there is
   // something there to remove, not to actually dereference the
   // member -- get_existing_member() throws VALUE_ERROR/DOMAIN_ERROR/
   // RANK_ERROR/LENGTH_ERROR for every one of these "no such member"
   // conditions (correct for an actual member *reference*, e.g. plain
   // S.zz), and constructing/throwing an Error has an observable
   // console side effect even when the caller immediately catches it
   // (Error::update_error_info()), so a try/catch around
   // get_existing_member() is not enough -- this mirrors its
   // traversal without ever throwing, returning 0 for every "not
   // there" case instead.
   //
const cValue * parent = static_cast<const cValue *>(this);

   for (int m = members.size() - 2; m >= 0; --m)
       {
         if (!parent->is_member())     return 0;
         if (parent->get_rank() != 2)   return 0;
         if (parent->get_cols() != 2)   return 0;

         const UCS_string & member_ucs = *members[m];
         const Cell * member_cell = parent->get_member_data(member_ucs);
         if (!member_cell)   return 0;   // member does not exist

         if (m == 0)   return member_cell;   // final member

         // more members coming: member_cell must point to a
         // structured sub-member
         //
         if (!member_cell->is_pointer_cell() ||
             !member_cell->get_pointer_value()->is_member())
            return 0;

         parent = member_cell->get_pointer_value().get();
       }

   return 0;   // not reached (members.size() >= 2 always, per caller)
}
//────────────────────────────────────────────────────────────────────────────
const Cell *
cValue::get_member_data(const UCS_string & member) const
{
const ShapeItem rows = get_rows();

ShapeItem row = member.FNV_hash() % rows;
   loop(r, rows)   // loop over member rows
       {
         if (++row >= rows)   row = 0;
         Cell cache;
         const Cell & name_cell = get_cravel(2*row, cache);

         if (name_cell.is_pointer_cell() &&
             name_cell.get_pointer_value()->equal_string(member))
            {
              // this value is a member table (name_cell is a PointerCell),
              // which can never be packed (try_pack() bails as soon as
              // pointer_cell_count > 0) -- so the fetch below never touches
              // data_cache and the returned address is always the real
              // ravel slot, safe to return past this function's return.
              Cell data_cache;
              return &get_cravel(2*row + 1, data_cache);
            }
       }

   // member row not found
   //
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
void
cValue::sorted_members(std::vector<ShapeItem> & result,
                      const Unicode * filters) const
{
   // we take advantage of the fact that the positions in the member
   // prefixes are limited, so we can sort them in linear time.
   //
   // CAUTION: This function works only for members with a _NNN prefix
   // indication the sorting order (as produced by ⎕XML) @@@
   //
   Assert(is_structured());

const Unicode filters_all[] = { UNI_DELTA_UNDERBAR, UNI_DELTA,
                                UNI_UNDERSCORE,     Unicode_0 };

   if (filters == 0)   filters = filters_all;

const ShapeItem rows = get_rows();
const ShapeItem max_member_count = get_all_members_count() + 1;   // ⎕IO←0 or 1
const ShapeItem alloc = 2*max_member_count; // one for ⍙, one for ∆ and _
ShapeItem * sorted = new ShapeItem[alloc];
   if (sorted == 0)   WS_FULL;

ShapeItem * sorted_attributes = sorted;      // ⍙
ShapeItem * sorted_nodes = sorted + max_member_count;   // ∆ and _

   loop(a, alloc)   sorted[a] = -1;

   loop(r, rows)
       {
         Cell cache;
         const Cell & member_name_cell = get_cravel(2*r, cache);
         if (!member_name_cell.is_pointer_cell())   continue;   // unused

         const Value & val = *member_name_cell.get_pointer_value();
         ShapeItem member_pos;     // the position in the XML file
         Unicode category;
         UCS_string name;
         Quad_XML::split_name(&category, &member_pos, &name, val);

         if (member_pos < 0 || member_pos >= max_member_count)
            {
              MORE_ERROR() << "invalid position " << member_pos
                           << " in member name '" << name << "'";
              delete [] sorted;
              DOMAIN_ERROR;
            }
         loop(f, 4)   // at most filters ⍙, ∆, and _
             {
               const Unicode filter = filters[f];
               if (filter == Unicode_0)   break;
               if (filter == category)
                  {
                    if (category == UNI_DELTA_UNDERBAR)
                       sorted_attributes[member_pos] = r;
                    else
                       sorted_nodes[member_pos] = r;
                    break;
                  }
             }
       }

   loop(a, alloc)
       {
         if (sorted[a] != -1)   result.push_back(sorted[a]);
       }

   delete [] sorted;
}
//────────────────────────────────────────────────────────────────────────────
static bool
member_name_greater(const ShapeItem & a, const ShapeItem & b, const void * ctx)
{
const cValue * val = reinterpret_cast<const cValue *>(ctx);
Cell cache_a, cache_b;
Value_P name_a = val->get_cravel(2*a, cache_a).get_pointer_value();
Value_P name_b = val->get_cravel(2*b, cache_b).get_pointer_value();

   // Plain dictionary (lexicographic) string comparison -- NOT
   // Cell::greater()/PointerCell::compare(), which compares nested
   // values shape-first (rank, then per-axis length) and only falls
   // back to content when the shapes match exactly. That's the right
   // rule for general ⍋/⍒ grading of mixed/nested arrays, but wrong
   // here: it sorted member names by LENGTH before content (e.g.
   // "kiwi" before "apple"/"mango"/"zebra", since kiwi is shorter),
   // not by name as a dictionary would.
   //
const ShapeItem len_a = name_a->element_count();
const ShapeItem len_b = name_b->element_count();
const ShapeItem min_len = len_a < len_b ? len_a : len_b;

   loop(c, min_len)
       {
         const Unicode ca = name_a->get_char_value(c);
         const Unicode cb = name_b->get_char_value(c);
         if (ca != cb)   return ca > cb;
       }

   // one name is a prefix of the other (or they are equal length):
   // the shorter one sorts first, e.g. "apple" before "apples".
   return len_a > len_b;
}
//────────────────────────────────────────────────────────────────────────────
void
cValue::used_members(std::vector<ShapeItem> & result, bool sorted) const
{
   Assert(is_structured());

   // 1. colllect the used columns...
   //
const ShapeItem rows = get_rows();
   result.reserve(rows);
   loop(r, rows)
       {
         Cell cache;
         const Cell & member_name_cell = get_cravel(2*r, cache);
         if (!member_name_cell.is_pointer_cell())   continue;   // unused

         result.push_back(r);
       }

   if (!sorted)   return;

   Heapsort<ShapeItem>::sort(result, &member_name_greater,
                             static_cast<const cValue *>(this));
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
cValue::get_member_count() const
{
   // this function shall only be called for values that have the same
   // layout as structured variables.
   //
   // it checks if the first column of this value looks OK and returns
   // the number of used rows.
   //
   Assert(get_cols() == 2);

const ShapeItem rows = get_rows();
ShapeItem valid_rows = 0;

   loop(r, rows)
      {
        Cell cache;
        const Cell & name_cell = get_cravel(2*r, cache);

        /*
           name_cell must  be:

           i.   integer 0       for unused rows, or
           ii.  any character   for 1-character member names, or else
           iii. any string      for other member names

         */
        if (name_cell.is_integer_cell())   // maybe i.
           {
             if (name_cell.get_int_value() != 0)   // invalid unused
                {
                   MORE_ERROR() << "invalid member name in row "
                                << r << " (integer)";
                   DOMAIN_ERROR;
                }
           }
        else
           {
             ++valid_rows;    // assume ii. or iii.

             if (Value_P v = name_cell.try_pointer_value())   // maybe iii ?
                {
                  if (!v->is_char_string())   // no.
                     {
                       MORE_ERROR() << "invalid member name in row "
                                    << r << " (nested non-string)";
                      DOMAIN_ERROR;
                     }
                }
             else if (!name_cell.is_character_cell())   // unexpected type
                     {
                       MORE_ERROR() << "invalid member name in row "
                                    << r << " (type)";
                       DOMAIN_ERROR;
                     }
           }
      }

   return valid_rows;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
cValue::get_enlist_count() const
{
const ShapeItem ec = element_count();
ShapeItem count = ec;

   // take care of non-simple cells...
   //
   loop(c, ec)
       {
         Cell cache;
         const Cell & cell = get_cravel(c, cache);
         if (cell.is_pointer_cell())
            {
               count--;   // deduct the pointer cell
               count += cell.get_pointer_value()->get_enlist_count();
            }
         else if (cell.is_lval_cell())
            {
              if (Cell * cp = cell.get_lval_value())   // valid Lval cell
                 {
                   if (cp->is_pointer_cell())
                      {
                        count--;   // deduct the pointer cell
                        count += cp->get_pointer_value()->get_enlist_count();
                      }

                   // otherwise the Lval cell points to a cell and was
                   // counted correctly
                 }
              else   // invalid Lval cel
                 {
                   count--;   // deduct the Lval cell
                 }
            }
       }

   return count;
}
//────────────────────────────────────────────────────────────────────────────
APL_types::Depth
cValue::compute_depth() const
{
   // The cache is reliable for the full 0..254 range, not just depth ≤ 1:
   // every mutation that could change ANY value's depth -- its own ravel
   // being overwritten, or a nested sub-value reachable from it being
   // mutated in place (selective/indexed/member assignment, left-value ⊃,
   // ...) -- invalidates the whole chain from wherever the mutating
   // assignment started down to the actual write, not just the immediate
   // owner. See plan.txt's 2026-09-01 analysis for why a naive "only
   // invalidate the direct owner" scheme is NOT sufficient (that was tried
   // once before and reverted: ZZ49_Regression.tc's "selective
   // specification with dyadic ¨" case silently kept the pre-mutation
   // depth once caching was extended to depth ≥ 2 without whole-chain
   // invalidation) -- and why piggybacking full-chain invalidation onto
   // the mandatory isolate_deep() COW walk (Value_P_Base::isolate_deep(),
   // PointerCell::isolate_deep()) fixes that, at no extra traversal cost.
   //
   if (flags.value_depth != VF_DEPTH_DIRTY)   return flags.value_depth;

   // Packed ravels store cells in a compact non-Cell representation (bool/
   // int/float/unicode arrays, see Ravel.hh) and can therefore never
   // contain a PointerCell -- every packed subtype overrides
   // is_pointer_cell(idx) to unconditionally return false. So a packed
   // value's depth is decided by its rank alone, with no need to scan the
   // (possibly large) ravel at all: 0 for a scalar, 1 otherwise. This
   // matters because compute_depth() gets called from hot, frequently-
   // repeated paths (PointerCell's MAX_DEPTH check on every ⊂/strand/
   // selective assignment, PrintBuffer, Bif_F2_INDEX, ...), not just ≡.
   //
   if ((pointer_cell_count == 0) || is_packed())
      {
        flags.value_depth = is_scalar() ? 0 : 1;
        return flags.value_depth;
      }

APL_types::Depth depth;
   if (is_scalar())   // any scalar
      {
        if (is_pointer_cell(0))   // nested scalar
           {
             depth = 1 + get_pointer_value(0)->compute_depth();
           }
        else                      // simple scalar: cache 0
           {
             flags.value_depth = 0;
             return 0;
           }
      }
   else               // not scalar
      {
        APL_types::Depth sub_depth = 0;
        const ShapeItem count = nz_element_count();
        loop(c, count)
            {
              if (is_pointer_cell(c))
                 {
                   const APL_types::Depth d =
                      get_pointer_value(c)->compute_depth();
                   if (sub_depth < d)   sub_depth = d;
                 }
            }
        depth = sub_depth + 1;
      }

   flags.value_depth = uint8_t(depth);   // depth is always ≤ MAX_DEPTH (254)
   return depth;
}
//────────────────────────────────────────────────────────────────────────────
void
cValue::enlist_left(Value & Z) const
{
   // Z is a new (un-initialized) Value that shall be (recursively)
   // initialized with the (non-pointer) items of this (left-) value
   //
   // Packed ravels store elements in compact form (uint16_t, int64_t, …) rather
   // than as Cell objects.  LvalCells must point to actual Cell objects so that
   // the selective-assignment writer can reach the storage.  Explode to RPT_CELLS
   // so that get_cravel() returns a reference into the real ravel, not the cache.
   if (is_packed())
      const_cast<Value *>(static_cast<const Value *>(this))->explode_to_Cells();

   Cell cache;   // exploded above, so never touched -- see comment above
   loop(c, element_count())
       {
         const Cell & cell = get_cravel(c, cache);
         if (Value_P v = cell.try_pointer_value())
            {
              v->enlist_left(Z);
            }
         else if (cell.is_lval_cell())
            {
              Cell * target = cell.get_lval_value();
              if (target == 0)
                 {
                   CERR << "0-pointer at " LOC << endl;
                 }
              else if (target->is_pointer_cell())
                 {
                   target->isolate(LOC);
                   target->get_pointer_value()->enlist_left(Z);
                 }
              else 
                 {
                   Z.next_ravel_Cell(cell);
                 }
            }
         else   // neither PointerCell nor LvalCell
            {
              /* cell is a right hand cell in a left expression.
                 This happens when assign_cellrefs() is flat (i.e. always)
                 and this left-value contains pointer-cells (supposedly
                 created AFTER get_celrefs() was called).

                 test with:

                 A←'ZIPPITY' 'DOO' 'DAH' ◊ (ϵA[2])←0 ◊ A
                 A←(10 20 30) 'AB'       ◊ (ϵA)←⍳5   ◊ A
                 A←''                    ◊ (↑A)←33   ◊ ↑A


              */

              // cannot use Z->next_ravel_Cell() since then the cell owner
              // would be Z and not this!
              //
              LvalCell z(const_cast<Cell *>(&cell),
                         static_cast<Value *>(const_cast<cValue *>(this)));
              Z.next_ravel_Cell(z);
            }
       }
}
//────────────────────────────────────────────────────────────────────────────
void
cValue::enlist_right(Value & Z) const
{
   // Z is a new (un-initialized) Value that shall be (recursively)
   // initialized with the (non-pointer) items of this (right-) value
   //
   // An LvalCell can legitimately reach here too: this path is also
   // taken (via Bif_F12_ELEMENT::do_eval_B()) for a strand whose
   // get_lval_cellowner() rejected it as not purely derived from one
   // left-value name (e.g. ∊2 V, mixing a literal with a variable) --
   // in that case some cells genuinely are LvalCells (V's) and some are
   // not (the literal's). Copying the LvalCell through unchanged, same
   // as an ordinary cell, is what lets Value::assign_cellrefs()'s own
   // per-cell is_lval_cell() check correctly reject the mixed result
   // later, the same way it already does for e.g. (⊃2 V)←C -- rather
   // than asserting here as if this could never happen.
   //
   // An LvalCell with a 0 (invalid) target can reach here too -- e.g.
   // Blake McBride, Bugs31 #2: (∊⊃V)←9 for V←(1 2 3)(4 5)(6), where ⊃V
   // mixes V's differently-shaped items into a rectangular array, padding
   // the shorter rows with target-less placeholder LvalCells so the whole
   // result stays uniformly "left value" (is_left_value()). Our own
   // get_enlist_count() (this file, above) already EXCLUDES exactly these
   // cells from Z's size -- enlist_left() already skips them too (its own
   // target==0 branch just CERR-logs and writes nothing) -- but this
   // function used to copy them anyway via next_ravel_Cell(), writing
   // past the end of Z's (correctly, smaller) allocation and corrupting
   // memory. Skip them here the same way enlist_left() does, so the
   // number of cells actually written always matches get_enlist_count().
   //
   loop(c, element_count())
       {
         Cell cache;
         const Cell & cell = get_cravel(c, cache);
         if (Value_P v = cell.try_pointer_value())
            {
              v->enlist_right(Z);
            }
         else if (cell.is_lval_cell() && cell.get_lval_value() == 0)
            {
              // invalid/placeholder LvalCell -- deducted from
              // get_enlist_count() already, so not written here either.
            }
         else if (cell.is_lval_cell() &&
                  cell.get_lval_value()->is_pointer_cell())
            {
              // an LvalCell pointing to a nested item: get_enlist_count()
              // counts the items of that nested item (not the LvalCell),
              // so write them too. Copying the LvalCell instead left the
              // rest of Z uninitialized (Blake McBride, Bugs32 #3, e.g.
              // (⊂∊⊃N)←9 for N←((1 2)(3 4))((5 6)(7 8))).
              //
              cell.get_lval_value()->get_pointer_value()->enlist_right(Z);
            }
         else
            {
              Z.next_ravel_Cell(cell);
            }
       }
}
//────────────────────────────────────────────────────────────────────────────
CellType
cValue::flat_cell_types() const
{
int32_t ctypes = 0;

const ShapeItem count = nz_element_count();
   loop(c, count)   ctypes |= get_cell_type(c);

   return CellType(ctypes);
}
//────────────────────────────────────────────────────────────────────────────
CellType
cValue::flat_cell_subtypes() const
{
int32_t ctypes = 0;

const ShapeItem count = nz_element_count();
   loop(c, count)   ctypes |= get_cell_subtype(c);

   return CellType(ctypes);
}
//────────────────────────────────────────────────────────────────────────────
CellType
cValue::deep_cell_types() const
{
int32_t ctypes = 0;

const ShapeItem count = nz_element_count();
   loop(c, count)
      {
       Cell cache;
       const Cell & cell = get_cravel(c, cache);
        ctypes |= cell.get_cell_type();
        if (Value_P v = cell.try_pointer_value())
            ctypes |= v->deep_cell_types();
      }

   return CellType(ctypes);
}
//────────────────────────────────────────────────────────────────────────────
CellType
cValue::deep_cell_subtypes() const
{
int32_t ctypes = 0;

const ShapeItem count = nz_element_count();
   loop(c, count)
      {
       Cell cache;
       const Cell & cell = get_cravel(c, cache);
        ctypes |= cell.get_cell_subtype();
        if (Value_P v = cell.try_pointer_value())
           ctypes |= v->deep_cell_subtypes();
      }

   return CellType(ctypes);
}
//────────────────────────────────────────────────────────────────────────────
ostream &
cValue::print(ostream & out) const
{
   if (is_member())   return print_members(out, UCS_string());

PrintContext pctx = Workspace::get_PrintContext(PR_APL);
   if (get_rank() == 0)   // scalar
      {
        pctx.set_style(PR_APL_MIN);
      }
   else if (get_rank() == 1)   // vector
      {
        // an empty vector is one empty line, whatever its prototype (GNU
        // APL rule 4; an empty nested vector printed nothing, Bugs34 #39)
        //
        if (element_count() == 0)   return out << endl;

        pctx.set_style(PR_APL_MIN);
      }
   else                  // matrix or higher
      {
        // an empty value: as IBM APL2 with STRICT_IBM_APL2_FORMATTING
        // (empty_lines()), otherwise no lines at all (GNU APL's traditional
        // output, e.g. for the common idiom 0 0⍴X)
        if (element_count() == 0 &&
            UserPreferences::uprefs.strict_IBM_APL2_formatting)
           return print_empty(out);
        pctx.set_style(PrintStyle(pctx.get_style() | PST_NO_FRACT_0));
      }

   // STRICT_IBM_APL2_FORMATTING parentheses: nested items in parentheses
   if (UserPreferences::uprefs.nested_parentheses)
      pctx.set_style(PrintStyle(pctx.get_style() | PST_CS_PARENS));

PrintBuffer pb(static_cast<const Value &>(*this), pctx, &out);   // constructor prints it
   return out;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
cValue::print_empty(ostream & out) const
{
const ShapeItem lines = empty_lines();
   loop(l, lines)   out << endl;
   return out;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
cValue::empty_lines() const
{
   // The number of (empty) lines in the display of an empty value of rank
   // ≥ 2, as IBM APL2 does it (verified with APL2 itself, e.g. 2 0⍴0: 2 empty
   // lines, 0 2⍴0: none, 2 3 0⍴0: 6 empty lines, 2 0 3⍴0: 1 empty line,
   // 0 2 3⍴0 and 0 0 0⍴1: none):
   //
   // 1. if the width (the last axis) is 0, then every row (×/¯1↓⍴B) is
   //    displayed as an empty line, without the blank lines between planes.
   //
   // 2. otherwise there are no rows (some other axis is 0) and only the
   //    blank lines between planes remain: as for a non-empty value, j blank
   //    lines between two consecutive items along the j-th axis counted
   //    from the rows (1 between matrices, 2 between rank-3 blocks, ...).
   //
   // A huge number of lines (e.g. for 1E9 1E9 0⍴0) is not displayed at all
   // (as before) rather than flooding the output (or exhausting memory).
   //
   enum { MAX_EMPTY_LINES = 100000 };
const sRank rank = get_rank();
   if (rank < 2)   return 0;

ShapeItem lines = 0;
   if (shape.get_last_shape_item() == 0)   // case 1.
      {
        lines = 1;
        loop(r, rank - 1)
           {
             const ShapeItem len = shape.get_shape_item(r);
             if (len > MAX_EMPTY_LINES)   return 0;
             lines *= len;
             if (lines > MAX_EMPTY_LINES)   return 0;
           }
        return lines;
      }

   // case 2. Axes 0 ... rank-3 are the plane axes; axis a separates its
   // items by rank-2-a blank lines, and there are (×/⍴[⍳a]) × (⍴[a] - 1)
   // such separators (none after an axis that is 0).
   //
ShapeItem outer = 1;   // ×/ of the plane axes before axis a
   loop(a, rank - 2)
      {
        const ShapeItem len = shape.get_shape_item(a);
        if (len == 0)   break;
        if (len > MAX_EMPTY_LINES)   return 0;
        lines += outer * (len - 1) * (rank - 2 - a);
        outer *= len;
        if (lines > MAX_EMPTY_LINES || outer > MAX_EMPTY_LINES)   return 0;
      }
   return lines;
}
//════════════════════════════════════════════════════════════════════════════
ostream &
cValue::print_brief(ostream & out) const
{
   // shape
   //
   out << "⍴";
   loop(r, shape.get_rank())
       {
         r && out << ";";
         out << shape.get_shape_item(r);
       }

   // depth
   //
   out << "≡" << compute_depth() << " ";

   // ravel
   //
const int count = min(3, int(element_count()));
   if (count == 0)   out << "⌽";   // empty
   loop(c, count)
      {
        if (c)    out << " ";
        Cell cache;
        const Cell & cell = get_cravel(c, cache);
        if (cell.is_integer_cell())        out << cell.get_int_value();
        else if (cell.is_float_cell())     out << "FLT";
        else if (cell.is_complex_cell())   out << "CPLX";
        else if (cell.is_pointer_cell())   out << "⊂";
        if (element_count() > count)       out << "...";
      }

   return out;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
cValue::print_members(ostream & out, UCS_string member_prefix) const
{
const ShapeItem rows = get_rows();

   // figure the longest member name, and collect the row numbers of
   // the used (member-name-bearing) rows.
   //
ShapeItem longest_name = 0;
std::vector<ShapeItem> order;
   loop(r, rows)
      {
        Cell cache;
        const Cell & cell = get_cravel(2*r, cache);   // (nested) member-name or 0
        if (Value_P member_name = cell.try_pointer_value())
           {
              const ShapeItem name_len = member_name->element_count();
              if (longest_name < name_len)  longest_name = name_len;
              order.push_back(r);
           }
      }

   // sort order[] by member name so members print alphabetically. The
   // member count is normally small (a handful of struct fields), so
   // a simple O(N×N) selection sort is enough -- no need for Heapsort.
   //
   loop(i, order.size())
       {
         ShapeItem min_at = i;
         Cell cache;
         UCS_string min_name =
            get_cravel(2*order[min_at], cache).get_pointer_value()->get_UCS_ravel();
         for (ShapeItem j = i + 1; j < ShapeItem(order.size()); ++j)
             {
               Cell cache_j;
               const UCS_string name_j =
                  get_cravel(2*order[j], cache_j).get_pointer_value()->get_UCS_ravel();
               if (name_j.compare(min_name) == COMP_LT)
                  {
                    min_at = j;
                    min_name = name_j;
                  }
             }

         if (min_at != i)
            {
              const ShapeItem tmp = order[i];
              order[i] = order[min_at];
              order[min_at] = tmp;
            }
       }

const size_t indent = member_prefix.size() + longest_name + 3;

   loop(oi, order.size())
       {
         const ShapeItem r = order[oi];
         Cell cache;
         const Cell & cell_name = get_cravel(2*r, cache);  // member-name

         Value_P cell_sub = cell_name.get_pointer_value();
         Assert(cell_sub->is_char_string());

         // print the member name
         //
         const UCS_string member_name = cell_sub->get_UCS_ravel();
         const size_t pad = longest_name - member_name.size();
         UCS_string member = member_prefix;   // ancestor.ancestor ...
         member << UNI_FULLSTOP << member_name;
         out << member << ": ";
         out << UCS_string(pad, UNI_SPACE);

         // print the member value (at 2*r+1)
         //
         Cell cache_val;
         const Cell & cell_val = get_cravel(2*r + 1, cache_val);
         if (cell_val.is_pointer_cell())   // sub-member or leaf
            {
              bool printed = false;
              Value_P sub = cell_val.get_pointer_value();
              if (sub->is_member())
                 {
                   out << "□" << endl;
                   sub->print_members(out, member);
                   printed = true;
                 }
              else if (sub->is_char_vector())   // maybe multi-line with \n
                 {
                   Value_P sub1 = Quad_CR::do_CR35(*sub);
                   Value_P sub2 = Bif_F12_PICK::disclose(*sub1, false);
                   if (sub2->get_rows() > 1)
                      {
                        sub2->print_boxed(out, indent);
                        printed = true;
                      }

                   // for obscure reasons we need to release the lines in sub1
                   //
                   loop(c, sub1->nz_element_count())
                       {
                         Cell & cell = sub1->get_wravel(c);
                         cell.release(LOC);
                         new (&cell) IntCell(0);   // was IntCell::z0(&cell)
                       }
                 }

              if (!printed)
                 {
                   sub->print_boxed(out, indent);
                 }
            }
         else                           // simple member value
            {
              Value_P sub(cell_val, LOC);
              sub->print_boxed(out, indent);
            }
       }

   return out;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
cValue::print1(ostream & out, PrintContext pctx) const
{
int style = pctx.get_style();
   if (get_rank() < 2)   // scalar or vector
      {
        style = PR_APL_MIN;
      }
   else                  // matrix or higher
      {
        style |= PST_NO_FRACT_0;
      }

   pctx.set_style(PrintStyle(style));

PrintBuffer pb(static_cast<const Value &>(*this), pctx, &out);
   return out;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
cValue::print_properties(ostream & out, int indent, bool help) const
{
UCS_string ind(indent, UNI_SPACE);
   if (help)
      {
        out << ind << "Rank:  " << get_rank()  << endl
            << ind << "Shape:";
        loop(r, get_rank())   out << " " << get_shape_item(r);
        out << endl
            << ind << "Depth: " << compute_depth()   << endl
            << ind << "Type:  ";

       const CellType types = deep_cell_types();
       if (!(types & CT_CHAR))           // no chars in this value
          out << "numeric";
       else if (!(types & CT_NUMERIC))   // no numbers in this value
          out << "character";
       else                              // chars and numbers in this value
          out << "mixed";
      }
   else
      {
        out << ind << "Addr:    " << voidP(this) << endl
            << ind << "Rank:    " << get_rank()  << endl
            << ind << "Shape:   " << get_shape() << endl
            << ind << "Flags:   " << get_flags();
        if (is_complete())    out << " VF_complete";
        if (is_marked())      out << " VF_marked";
        if (is_member())      out << " VF_member";
        if (is_packed())      out << " VF_packed";
        Cell cache;
        out << endl
             << ind << "First:   " << get_cfirst(cache)  << endl
             << ind << "Dynamic: ";

        static_cast<const Value *>(this)->DynamicObject::print(out);
      }
   return out;
}
//────────────────────────────────────────────────────────────────────────────
void
cValue::debug(const char * info) const
{
const PrintContext pctx = Workspace::get_PrintContext(PR_APL);
PrintBuffer pb(static_cast<const Value &>(*this), pctx, 0);
   pb.debug(CERR, info);
}
//════════════════════════════════════════════════════════════════════════════
ostream &
cValue::print_boxed(ostream & out, int indent) const
{
const PrintContext pctx(PST_NONE);

Value_P Z = Quad_CR::do_CR(8, *this, pctx);
   Assert(Z->get_rank() == 2);
const ShapeItem rows = Z->get_rows();
const ShapeItem cols = Z->get_cols();
   loop(r, rows)
       {
         if (indent && r)   out << UCS_string(indent, UNI_SPACE);
         loop(c, cols)   out << Z->get_char_value(c + r*cols);
         out << endl;
       }
   out << endl;
   return out;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
cValue::index(const IndexExpr & IX) const
{
   // if IX.value_count() were 1 (as in A[X] as opposed to A[X1;...]) then
   // it should have been parsed as an axis and Value::index(Value_P X) should
   // have been called instead of this one. This is an internal error.

   if (is_member())
      {
         // for a structured variable VAR, we only allow VAR[1;] to obtain
         // the valid member names...
         //
         if (IX.get_rank() != 2)
            {
              MORE_ERROR() << "A[B]: expecting 2 indices (VAR[row;col]) for"
                              " member access; got " << IX.get_rank();
              RANK_ERROR;
            }
         if (IX.values[1])   // row not elided
            {
              MORE_ERROR() << "A[B]: member access: row (VAR[row;col]'s"
                              " first index) must be elided; use VAR[;col]";
              LENGTH_ERROR;   // not elided
            }

         if (!IX.values[0])   // col also elided, e.g. VAR[;]
            {
              MORE_ERROR() << "A[B]: member access: col (VAR[row;col]'s"
                              " second index) is elided "
                              "(expecting VAR[;1] to obtain the member names)";
              LENGTH_ERROR;
            }

         if (IX.values[0]->element_count() != 1)
            {
              MORE_ERROR() << "A[B]: member access: col (VAR[row;col]'s"
                              " second index) has "
                           << IX.values[0]->element_count()
                           << " items (expecting 1)";
              LENGTH_ERROR;   // not 1 columns
            }

         const APL_Integer col = IX.values[0]->get_int_value(0);
         if (col != Workspace::get_IO())
            {
              MORE_ERROR() << "A[B]: member access: col (VAR[row;col]'s"
                              " second index) is " << col
                           << " (expecting ⎕IO, i.e. "
                           << Workspace::get_IO() << ")";
              INDEX_ERROR;
            }

         // VAR is a dictionary: VAR[;1] returns its keys sorted by name,
         // not in raw (insertion-order-independent, resize-dependent)
         // hash-bucket order.
         //
         std::vector<ShapeItem> used_rows;
         used_members(used_rows, /* sorted */ true);

         Value_P Z(used_rows.size(), LOC);
         loop(r, used_rows.size())
             {
               Cell cache;
               Z->next_ravel_Cell(get_cravel(2*used_rows[r], cache));
             }

         if (used_rows.size() == 0)   // no valid members (last member )ERASEd)
            new (&Z->get_wfirst()) PointerCell(Idx0(LOC).get(), *Z);

         Z->check_value(LOC);
         return Z;
      }

   Assert(!IX.is_axis());   // should have called index(Value_P X)

   if (get_rank() != IX.get_rank())   // ISO p. 158
      {
        MORE_ERROR() << "A[B]: expecting " << get_rank()
                     << " semicolon-separated indices (⍴⍴A); got "
                     << IX.get_rank();
        RANK_ERROR;
      }

   // Notes:
   //
   // 1.  IX is parsed from right to left:    B[I2;I1;I0]  --> I0 I1 I2
   //     the shapes of this and IX are then related as follows:
   //
   //     this     IX
   //     ---------------
   //     0        rank-1   (rank = IX->value_count())
   //     1        rank-2
   //     ...      ...
   //     rank-2   1
   //     rank-1   0
   //     ---------------
   //
   // 2.  shape Z is the concatenation of all shapes in IX
   // 3.  rank Z is the sum of all ranks in IX

   // construct result rank_Z and shape_Z.
   // We go from higher indices of IX to lower indices (Note 1.)
   //
Shape shape_Z;
   loop(this_r, get_rank())
       {
         const ShapeItem idx_r = get_rank() - this_r - 1;

         Value_P I = IX.values[idx_r];
         if (!I)
            {
              shape_Z.add_shape_item(this->get_shape_item(this_r));
            }
         else
            {
              loop(s, I->get_rank())
                shape_Z.add_shape_item(I->get_shape_item(s));
            }
       }

   // check that all indices are valid
   //
   IX.check_index_range(get_shape());

MultiIndexIterator mult(get_shape(), IX);

Value_P Z(shape_Z, LOC);
const ShapeItem ec_z = Z->element_count();

   if (ec_z == 0)   // empty result
      {
        Z->set_default(static_cast<const Value &>(*this), LOC);
        if (is_left_value())   Z->set_left_value();
        Z->check_value(LOC);
        return Z;
      }

   // construct iterators.
   // We go from lower indices to higher indices in IX, which
   // means from higher indices to lower indices in this and Z
   //
   loop(z, ec_z)
       {
         Cell cache;
         Z->next_ravel_Cell(get_cravel(mult++, cache));
       }

   Assert(!mult.has_more());
   if (is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
AxesBitmap
cValue::to_bitmap(const char * where, uRank rank_B) const
{
const APL_Integer qio = Workspace::get_IO();
   // ⎕IO=0 is unusual enough that axis numbers below can look like
   // off-by-one mistakes if the reader assumes the (far more common)
   // ⎕IO=1; call that out explicitly rather than silently.
const char * io_note = qio == 0 ? " Note: ⎕IO=0." : "";
AxesBitmap ret = 0;

   if (get_rank() > 1)
      {
         MORE_ERROR() << where << ": Bad rank " << get_rank()
                      << " of X (expecting ⍴⍴X ≤ 1)";
         AXIS_ERROR;
      }

   // note: length error on X will either produce a range error or a
   // duplicte error below.
   //
   loop(e, element_count())
       {
         Cell cache;
         const Cell & cX = get_cravel(e, cache);
         if (!cX.is_near_int())
            {
              MORE_ERROR() << where << ": X[" << (e + qio)
                           << "] is not integral.";
              AXIS_ERROR;
            }

         const APL_Integer axis = cX.get_near_int() - qio;
         if (axis < 0)
            {
              MORE_ERROR() << where << ": X[" << (e + qio)
                           << "] = " << (axis + qio)
                           << " is not a valid axis (expecting "
                           << qio << "≤X[" << (e + qio) << "]<"
                           << (rank_B + qio) << ")." << io_note;
              AXIS_ERROR;
            }

         if (axis >= rank_B)
            {
              MORE_ERROR() << where << ": X[" << (e + qio)
                           << "] = " << (axis + qio)
                           << " is not a valid axis (expecting "
                           << qio << "≤X[" << (e + qio) << "]<"
                           << (rank_B + qio) << ")." << io_note;
              AXIS_ERROR;
            }

         if (ret & 1 << axis)   // aready set
            {
              MORE_ERROR() << where << ": X[" << (e + qio)
                           << "] = " << (axis + qio)
                           << " is a duplicate axis.";
              AXIS_ERROR;
            }

         ret |= 1 << axis;
       }

   return ret;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
cValue::index(cValue_R X) const
{
   if (false)   // A[] (1-item elided index)
      {
        return CLONE(this, LOC);
      }

   if (is_member())
      {

        const UCS_string name(X);

        if (const Cell * data = get_member_data(name))   // member exists
           {
             if (data->is_pointer_cell())
                {
                  return CLONE_P(data->get_pointer_value(), LOC);
                }
             Value_P Z(LOC);
             Z->next_ravel_Cell(*data);
             Z->check_value(LOC);
             return Z;
           }

        // construct a )MORE error...
        //
        UCS_string & more = MORE_ERROR();
        more << "A[B]: member access: member " << name
             << " not found. The valid members are:";
        const ShapeItem rows = get_rows();
        loop(r, rows)
            {
              Cell cache;
              const Cell & cell_r = get_cravel(2*r, cache);
              if (cell_r.is_pointer_cell())
                 {
                   more << "\n      "
                        << UCS_string(*cell_r.get_pointer_value());
                 }
            }
        INDEX_ERROR;
      }

const ShapeItem max_idx = element_count();
const APL_Integer qio = Workspace::get_IO();

   // important (since frequent) special case: A[X] with scalar X and vector A
   //
   if (get_rank() == 1 && X.is_scalar())
      {
        Cell x_cache;
        const Cell & xcell = X.get_cscalar(x_cache);

        // Bugs28 #100(t): get_near_int() throws (a bare, uncontextualized
        // DOMAIN ERROR) for a value too large to fit an int64_t index --
        // an index is by definition a ShapeItem (int64_t), so check that
        // safely first (is_near_int64_t() cannot itself throw) and skip
        // this fast path instead of calling it; the general loop below
        // has the same check and reports it as the well-formed INDEX
        // ERROR it actually is.
        //
        if (xcell.is_near_int64_t())
           {
             // check against qio (i.e. idx0 < 0) BEFORE subtracting it,
             // not after: computing idx0 = raw - qio first and then
             // testing idx0 < 0 needlessly risks underflowing raw when
             // raw is already at the (is_near_int64_t()-permitted)
             // extreme end of the range.
             //
             const APL_Integer raw = xcell.get_near_int();
             if (raw >= qio)
                {
                  const APL_Integer idx0 = raw - qio;
                  if (idx0 < max_idx)
                     {
                       Value_P Z(LOC);
                       Cell cache;
                       Z->next_ravel_Cell(get_cravel(idx0, cache));
                       if (is_left_value())   Z->set_left_value();
                       Z->check_value(LOC);
                       return Z;
                     }
                }
           }

        // fall through re-using the verbose INDEX_ERROR below
      }

   if (get_rank() != 1)
      {
        MORE_ERROR() << "A[B]: A is not a vector (single-index A[B] only"
                        " applies to vector A); ⍴⍴A is " << get_rank();
        RANK_ERROR;
      }

   // ⍴A[X] = ⍴X
   //
Value_P Z(X.get_shape(), LOC);

ShapeItem xI = 0;

   while (Z->more())
      {
         const ShapeItem this_xI = xI;

         // Bugs28 #100(t): get_near_int() below throws (a bare,
         // uncontextualized DOMAIN ERROR) for a value too large to fit
         // an int64_t index -- and that same value then hazards a
         // stack overflow when the error-display code later tries to
         // format it for the caret line (Token.cc's own "?" fallback
         // exists specifically to dodge that crash). An index is by
         // definition a ShapeItem (int64_t); check that safely first
         // (is_near_int64_t() cannot itself throw) instead of calling
         // get_near_int() unconditionally, and report it as the
         // ordinary, well-formed INDEX ERROR it actually is, with the
         // same context the in-range check just below already gives
         // for an ordinary out-of-bounds index.
         //
         Cell x_cache;
         const Cell & xcell = X.get_cravel(this_xI, x_cache);
         if (!xcell.is_near_int64_t())
            {
              // a character, nested, or non-integer index is outside the
              // domain of indices (LRM p. 70) like for M[I;J] or V[I]←B,
              // while an integer too large for an index is out of range.
              // Bugs37 #40.
              //
              Z->rollback(Z->get_valid_item_count(), LOC);
              if (xcell.is_near_int())
                 {
                   MORE_ERROR() << "A[B]: B is not a valid index for A (an"
                                   " integer too large for a (signed) 63-bit"
                                   " index)";
                   INDEX_ERROR;
                 }
              MORE_ERROR() << "A[B]: B is not a valid index for A (not an"
                              " integer)";
              DOMAIN_ERROR;
            }

         ++xI;
         const APL_Integer raw = xcell.get_near_int();

         // check against qio (i.e. idx0 < 0) BEFORE subtracting it, not
         // after: computing idx0 = raw - qio first and then testing
         // idx0 < 0 needlessly risks underflowing raw when raw is
         // already at the (is_near_int64_t()-permitted) extreme end of
         // the range.
         //
         if (raw < qio)
            {
              MORE_ERROR() << "A[B]: B[" << (this_xI + qio) << "] = "
                           << raw
                           << " is not a valid index for A (expecting "
                           << qio << "≤index<" << (max_idx + qio) << ")"
                           << ArgCheck::index_io0_note(raw - qio, max_idx);
              Z->rollback(Z->get_valid_item_count(), LOC);
              INDEX_ERROR;
            }

         const ShapeItem idx0 = raw - qio;
         if (idx0 >= max_idx)
            {
              MORE_ERROR() << "A[B]: B[" << (this_xI + qio) << "] = "
                           << raw
                           << " is not a valid index for A (expecting "
                           << qio << "≤index<" << (max_idx + qio) << ")"
                           << ArgCheck::index_io0_note(idx0, max_idx);
              Z->rollback(Z->get_valid_item_count(), LOC);
              INDEX_ERROR;
            }

         Cell cache;
         Z->next_ravel_Cell(get_cravel(idx0, cache));
      }

   Z->set_default(static_cast<const Value &>(*this), LOC);
   if (is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
sRank
Value::get_single_axis(const cValue * val, sRank max_axis, const char * where)
{
const APL_Integer qio = Workspace::get_IO();

   if (val == 0)
      {
        MORE_ERROR() << where << ": X is missing (elided axis [])";
        AXIS_ERROR;
      }

   if (!val->is_scalar_or_len1_vector())
      {
        MORE_ERROR() << where << ": Bad rank/length of X (expecting a"
                        " single axis, i.e. a scalar or 1-item vector)";
        AXIS_ERROR;
      }

   if (!val->is_near_int(0))
      {
        MORE_ERROR() << where << ": X is not integral.";
        AXIS_ERROR;
      }

// APL_Integer (64-bit), not a plain (32-bit) int: get_near_int(0) is a
// full APL_Integer, and a plain int here wrapped an axis >= 2^32 back
// into the valid range instead of being rejected (confirmed: ⌽[4294967298]M
// silently reversed axis 2 -- 4294967298 truncated to 32 bits -- instead
// of raising AXIS_ERROR).
const APL_Integer axis = val->get_near_int(0) - qio;

   // axis is a plain (32-bit) signed int here while max_axis is the
   // (16-bit) signed sRank, so "axis >= max_axis" alone is a signed
   // comparison and does NOT catch a negative axis (confirmed: ⌽[0]
   // with ⎕IO←1 silently returned B unreversed instead of raising an
   // error). Test both bounds explicitly.
   //
   if (axis < 0 || axis >= APL_Integer(max_axis))
      {
        // ⎕IO=0 is unusual enough that the axis number below can look
        // like an off-by-one mistake if the reader assumes the (far
        // more common) ⎕IO=1; call that out explicitly rather than
        // silently -- but only when it actually explains the error.
        const APL_Integer axis_at_1 = val->get_near_int(0) - 1;
        const char * io_note = (qio == 0 && axis_at_1 >= 0
                                && axis_at_1 < APL_Integer(max_axis))
                              ? " Note: ⎕IO=0." : "";
        MORE_ERROR() << where << ": X = " << (axis + qio)
                     << " is not a valid axis (expecting " << qio
                     << "≤X<" << (max_axis + qio) << ")." << io_note;
        AXIS_ERROR;
      }

   return axis;
}
//════════════════════════════════════════════════════════════════════════════
Shape
Value::to_shape(const cValue * val)
{
   if (val == 0)
      {
        MORE_ERROR() << "illegal elided index [].";
        INDEX_ERROR;   // elided index ?
      }

const ShapeItem xlen = val->element_count();
const APL_Integer qio = Workspace::get_IO();

Shape shape;
     loop(x, xlen)
        shape.add_shape_item(val->get_near_int(x) - qio);

   return shape;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Value::glue(const Token & token_A, const Token & token_B, const char * loc)
{
Value_P A = token_A.get_apl_val();
Value_P B = token_B.get_apl_val();

const bool strand_A = token_A.get_tag() == TOK_APL_VALUE3;
const bool strand_B = token_B.get_tag() == TOK_APL_VALUE3;

   // in this context, a strand is an (open) strand, i.e. a strand to which
   // another item can be glued. The result is always an open strand (so
   // the caller must put it into an TOK_APL_VALUE3).
   //
Value_P Z = strand_A ? (strand_B ? glue_strand_strand(*A, *B, loc)
                                  : glue_strand_item(*A, *B, loc))
                     : (strand_B ? glue_item_strand(*A, *B, loc)
                                  : glue_item_item(*A, *B, loc));

   // a strand glued from a left value (e.g. Q S.b in (Q S.b)←7 8) is a left
   // value as well: otherwise a rejected selective specification of it left
   // its LvalCells alive in the suspended statement until )SIC, since the
   // cleanup (Prefix::neutralize_lval_values()) only visits left values
   // (Bugs34 #36)
   //
   if (A->is_left_value() || B->is_left_value())   Z->set_left_value();
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value *
cValue::get_lval_cellowner() const
{
   /* this Value may or may not be a left value. If it is, then its
      top-level (!) ravel is a mix of:

      1.  PointerCells pointing to (not yet get_cellrefs()ed) right Values,
      2a. valid LvalCells (from get_cellrefs()), or
      2b. invalid LvalCells(0, 0) (e.g. from ↑ overtake).

      A cell of any other kind -- a plain, non-pointer, non-lval cell,
      e.g. a literal stranded alongside a variable as in ∊2 V -- means
      this Value is not *purely* derived from one left-value name and
      must not be treated as a left value at all: silently returning
      whichever owner was found first (the original behaviour here)
      let enlist_left() wrap that plain cell in a synthetic LvalCell
      owned by the strand temporary itself, and assign_cellrefs() would
      later write through it into a Value already destroyed by the time
      the assignment ran (Blake McBride, Bugs25 #1, `(∊2 V)←¯1`). The
      same reasoning requires every found owner to agree with the
      others: a strand of lval cells from two *different* variables is
      likewise not derived from a single name.
    */
Value * owner = 0;
   loop(e, nz_element_count())
      {
        Cell cache;
        const Cell & cell = get_cravel(e, cache);
        if (Value_P v = cell.try_pointer_value())   // case 1.
           {
             Value * sub_owner = v->get_lval_cellowner();
             if (sub_owner == 0)                 return 0;
             if (owner && owner != sub_owner)    return 0;
             owner = sub_owner;
             continue;
           }

        if (cell.is_lval_cell())      // case 2a. or 2b.
           {
             if (Value * cell_owner = cell.get_cell_owner())
                {
                  if (owner && owner != cell_owner)   return 0;
                  owner = cell_owner;
                }
             continue;
           }

        return 0;   // plain cell: not purely a left-value
      }

   return owner;   // 0 if nothing (lval-ish) was found
}
//────────────────────────────────────────────────────────────────────────────
bool
cValue::NOTCHAR() const
{
   // always test element 0.
   if (!is_character_cell(0))   return true;

const ShapeItem ec = element_count();
   for (ShapeItem e = 1; e < ec; ++e)
       if (!is_character_cell(e))   return true;

   // all ravel items are (single) characters.
   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
cValue::flag_info(const char * loc, uint32_t flag_bit, const char * flag_name,
                 bool set) const
{
const char * sc = set ? " SET " : " CLEAR ";
const uint32_t cur_flags = get_flags();
const uint32_t new_flags = set ? cur_flags | flag_bit : cur_flags & ~flag_bit;
const char * chg = cur_flags == new_flags ? " (no change)" : " (changed)";

   CERR << "Value " << voidP(this)
        << sc << flag_name << " (" << HEX(flag_bit) << ")"
        << " at " << loc << " now = " << HEX(new_flags)
        << chg << endl;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
cValue::get_all_members_count() const
{
   Assert(is_structured());

ShapeItem count = 0;
const ShapeItem rows = get_rows();
   loop(r, rows)
       {
         Cell cache_name;
         const Cell & member_name_cell = get_cravel(2*r, cache_name);
         if (!member_name_cell.is_pointer_cell())   continue;  // unused row
            {
              ++count;
              Cell cache_data;
              const Cell & member_data_cell = get_cravel(2*r + 1, cache_data);
              if (Value_P v = member_data_cell.try_pointer_value())   // nested value
                 {
                   if (!v->is_structured())   continue;
                   count += v->get_all_members_count();
                 }
            }
       }

   return count;
}
//────────────────────────────────────────────────────────────────────────────
void
cValue::unmark() const
{
   clear_marked();
   if (is_packed())   return;

const ShapeItem ec = nz_element_count();
   loop(e, ec)
      {
        if (is_pointer_cell(e))
           get_pointer_value(e)->unmark();
      }
}
//────────────────────────────────────────────────────────────────────────────
Value_P
cValue::prototype(const char * loc) const
{
   /** lrm p.46:

       The type of an array is an array with the same structure, but with
      all numbers replaced with 0 and all characters replaced with ' '.

      The prototype of an array is the type of the first element of the array.

      type B        ←→   ↑0⍴⊂B
      prototype B   ←→   ↑0⍴⊂↑B

      Since ↑B is a scalar the prototype of B is also a scalar.
    **/

Cell cache;
const Cell & first = get_cfirst(cache);
   if (first.is_numeric())          return IntScalar(0, LOC);
   if (first.is_character_cell())   return CharScalar(UNI_SPACE, LOC);
   if (Value_P sub = first.try_pointer_value())
      {
        Value_P Z = CLONE(sub.get(), LOC);
        Z->to_type(false);
        return Z;
      }

   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
/* lrp p.138: S←⍴⍴A + NOTCHAR (per column)

   lrm defines NOTCHAR as (comments by jsa)

   ∇ Z←NOTCHAR R
   [1] Z←1              ⍝ assume NOTCHAR
   [2] →(1<≡R)/0        ⍝ NOTCHAR if R is nested
   [3] Z←' '∨.≠,↑0⍴⊂R   ⍝ all characters ?
   ∇

  IOW: NOTCHAR returns 1 if R is not a simple character
       array and 0 otherwise.
 */
int32_t
cValue::get_col_spacing(bool & non_char_col, ShapeItem col, bool framed) const
{
int32_t max_spacing = 0;
   non_char_col = false;

const ShapeItem ec = element_count();
const ShapeItem cols = get_last_shape_item();
const ShapeItem rows = ec/cols;

   loop(row, rows)   // for all items of column col
      {
        /* compute the S, which is the spacing demanded by this item.
           The spacing is defined by the rank of the item (lrm p.138):
         
           S←((ρρA)+NOTCHAR A)⌈(ρρB)+NOTCHAR B

           Instead of computing S for every pair A, B of adjacent columns
           (which would compute it twice for every column) we only compute it
           once for every column and reuse the result when B becomes A.
         */ 
        Cell cache;
        const Cell & cell = get_cravel(col + row*cols, cache);
        int32_t S = 1;   // assume simple numeric

        if (cell.is_pointer_cell())   // nested: NOTCHAR[2]
           {
             non_char_col = true;
             if (framed)
                {
                  S = 1;
                }
             else
                {
                  const Value & sub = *cell.get_pointer_value();
                  S = sub.get_rank();
                  if (sub.NOTCHAR())   ++S;
                  // GNU APL (unless STRICT_IBM_APL2_FORMATTING): an empty
                  // item is displayed as a single blank (see PrintBuffer)
                  // and is spaced like a simple scalar, whatever its shape
                  // and prototype
                  if (sub.element_count() == 0 &&
                      !UserPreferences::uprefs.strict_IBM_APL2_formatting)
                     S = 1;
                }
           }
        else if (cell.is_character_cell())   // simple char
           {
             S = 0;
           }
        else                                 // simple numeric
           {
             non_char_col = true;
           }

        if (max_spacing < S)   max_spacing = S;
      }

   return max_spacing;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
cValue::list_one(ostream & out, bool show_owners) const
{
   if (is_member() || is_complete() || is_marked() || is_packed())
      {
        out << "   Flags =";
        char sep = ' ';
        if (is_member())     { out << sep << "MEMBER";     sep = '+'; }
        if (is_complete())   { out << sep << "COMPLETE";   sep = '+'; }
        if (is_marked())     { out << sep << "MARKED";     sep = '+'; }
        if (is_packed())     { out << sep << "PACKED";     sep = '+'; }
      }
   else
      {
        out << "   Flags = NONE";
      }

   out << ", ⍴" << get_shape() << " ≡" << compute_depth() << ":" << endl;
   print(out);
   out << endl;

   if (!show_owners)   return out;

   // print owners...
   //
   out << "Owners of " << voidP(this) << ":" << endl;

   Workspace::show_owners(out, static_cast<const Value &>(*this));

   out << "---------------------------" << endl << endl;
   return out;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
cValue::total_CDR_size_netto(CDR_type cdr_type) const
{
   if (cdr_type != 7)   // not nested
      return 16 + 4*get_rank() + CDR_data_size(cdr_type);

   // nested: header + offset-array + sub-values.
   //
const ShapeItem ec = nz_element_count();
ShapeItem size = 16 + 4*get_rank() + 4*ec;   // top_level size
   size = (size + 15) & ~15;           // rounded up to 16 bytes

   loop(e, ec)
      {
        Cell cache;
        const Cell & cell = get_cravel(e, cache);
        if (cell.is_simple_cell())
           {
             // a non-pointer sub value consisting of its own 16 byte header,
             // and 1-16 data bytes, padded up to 16 bytes,
             //
             size += 32;
           }
        else if (cell.is_pointer_cell())
           {
             Value_P sub_val = cell.get_pointer_value();
             const CDR_type sub_type = sub_val->get_CDR_type();
             size += sub_val->total_CDR_size_brutto(sub_type);
           }
         else
           DOMAIN_ERROR;
      }

   return size;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
cValue::CDR_data_size(CDR_type cdr_type) const
{
const ShapeItem ec = nz_element_count();

   switch (cdr_type)
      {
        case 0: return (ec + 7) / 8;   // 1/8 byte bit, rounded up
        case 1: return 4*ec;           //   4 byte integer
        case 2: return 8*ec;           //   8 byte float
        case 3: return 16*ec;          // two 8 byte floats
        case 4: return ec;             // 1 byte char
        case 5: return 4*ec;           // 4 byte Unicode char
        case 7: break;                 // nested: continue below.

        default: FIXME;
      }

   // compute size of a nested CDR.
   // The top level consists of structural offsets that do not count as data.
   // We therefore simly add up the data sizes for the sub-values.
   //
ShapeItem size = 0;
   loop(e, ec)
      {
        Cell cache;
        const Cell & cell = get_cravel(e, cache);
        if (cell.is_simple_cell())
           {
             size += cell.CDR_size();
           }
        else if (cell.is_pointer_cell())
           {
             Value_P sub_val = cell.get_pointer_value();
             const CDR_type sub_type = sub_val->get_CDR_type();
             size += sub_val->CDR_data_size(sub_type);
           }
         else
           DOMAIN_ERROR;
      }

   return size;
}
//────────────────────────────────────────────────────────────────────────────
CDR_type
cValue::get_CDR_type() const
{
   // if all cells are characters (8 or 32 bit), then return 4 or 5.
   // if all cells are numeric (1, 32, 64, or 128 bit, then return  0 ... 3
   // otherwise return 7 (nested)

const ShapeItem nzec = nz_element_count();   // for Cell-ravel loops
const ShapeItem ec   = element_count();       // for packed loops (packed[0] not
                                              // valid for empty arrays)
Cell cache_0;
const Cell & cell_0 = get_cfirst(cache_0);
const RavelType rt = get_ravel_type();

   if (rt & RPT_char)   // RPT_UNICODE16 or RPT_UNICODE32: all cells are chars
      {
        bool has_big = false;
        loop(e, ec)   // ec=0 for empty: no packed element exists
           {
             const Unicode uni = get_char_value(e);
             if (uni < 0)      has_big = true;
             if (uni >= 256)   has_big = true;
           }
        return has_big ? CDR_CHAR32 : CDR_CHAR8;
      }

   if (cell_0.is_character_cell())   // 8 or 32 bit characters (Cell ravel)
      {
        bool has_big = false;   // assume 8-bit char
        loop(e, nzec)
           {
             Cell cache;
             const Cell & cell = get_cravel(e, cache);
             if (!cell.is_character_cell())   return CDR_NEST32;
             const Unicode uni = cell.get_char_value();
             if (uni < 0)      has_big = true;
             if (uni >= 256)   has_big = true;
           }

        return has_big ? CDR_CHAR32 : CDR_CHAR8;   // 8-bit or 32-bit char
      }

   if (rt == RPT_COMPLEX)
      return CDR_CPLX128;

   if (rt & RPT_integer)   // RPT_INT64 or RPT_BOOL
      {
        bool has_int = false;
        bool has_float = false;
        loop(e, ec)   // ec=0 for empty: prototype is 0, so CDR_BOOL1 is correct
           {
             const APL_Integer i = get_int_value(e);
             if (i == 0)                   ;
             else if (i == 1)              ;
             else if (i >  0x7FFFFFFFLL)   has_float = true;
             else if (i < -0x80000000LL)   has_float = true;
             else                          has_int   = true;
           }
        if (has_float)   return CDR_FLT64;
        if (has_int)     return CDR_INT32;
        return CDR_BOOL1;
      }

   if (rt == RPT_FLOAT64)
      return CDR_FLT64;

   if (cell_0.is_numeric())
      {
        bool has_int     = false;
        bool has_float   = false;
        bool has_complex = false;

        loop(e, nzec)
           {
             Cell cache;
             const Cell & cell = get_cravel(e, cache);
             if (cell.is_integer_cell())
                {
                  const APL_Integer i = cell.get_int_value();
                  if (i == 0)                   ;
                  else if (i == 1)              ;
                  else if (i >  0x7FFFFFFFLL)   has_float = true;
                  else if (i < -0x80000000LL)   has_float = true;
                  else                          has_int   = true;
                }
             else if (cell.is_float_cell())
                {
                  has_float  = true;
                }
             else if (cell.is_complex_cell())
                {
                  has_complex  = true;
                }
             else return CDR_NEST32;   // mixed: return 7
           }

        if (has_complex)   return CDR_CPLX128;
        if (has_float)     return CDR_FLT64;
        if (has_int)       return CDR_INT32;
        return CDR_BOOL1;
      }

   return CDR_NEST32;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
cValue::get_UCS_ravel() const
{
UCS_string ucs;

const ShapeItem ec = element_count();
   loop(e, ec)   ucs << get_char_value(e);

   return ucs;
}
//────────────────────────────────────────────────────────────────────────────
void
cValue::print_structure(ostream & out, int indent, ShapeItem idx) const
{
   loop(i, indent)   out << "    ";
   if (indent)   out << "[" << idx << "] ";
   out << "addr=" << voidP(this)
       << " ≡" << compute_depth()
       << " ⍴" << get_shape()
       << " flags: " << HEX4(get_flags()) << "   "
       << get_flags()
       << " " << static_cast<const Value *>(this)->where_allocated()
       << endl;

const ShapeItem ec = nz_element_count();
   loop(e, ec)
      {
        if (is_pointer_cell(e))
           get_pointer_value(e)->print_structure(out, indent + 1, e);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
cValue::print_stale_info(ostream & out, const DynamicObject * dob) const
{
   out << "print_stale_info():   alloc(" << dob->where_allocated()
       << ") flags(" << get_flags() << ")" << endl;

   VH_entry::print_history(out, dob->rValue(), LOC);

   try
      {
        print_structure(out, 0, 0);
        const PrintContext pctx(PST_NONE);
        Value_P Z = Quad_CR::do_CR(7, *this, pctx);
        Z->print(out);
        out << endl;
      }
   catch (Error &)          { out << " *** corrupt stale ***"; }
   catch (std::bad_alloc &) { out << " *** corrupt stale ***"; WS_FULL; }
   catch (...)              { FIXME; }

   out << endl;
}
//────────────────────────────────────────────────────────────────────────────
int
cValue::check_Cells(ostream & out) const
{
size_t errors = 0;
ShapeItem pointer_count = 0;
ShapeItem lval_count = 0;

   loop(c, nz_element_count())
       {
         Cell cache;
         const Cell & cell = get_cravel(c, cache);
         if (cell.is_pointer_cell())     ++pointer_count;
         else if (cell.is_lval_cell())   ++lval_count;
       }

   if (lval_count)
      {
        out << "*** Warning: value " << voidP(this)
            << " has " << lval_count << " Lval Cells" << endl;
        VH_entry::print_history(cerr, static_cast<const Value &>(*this), LOC);

        ++errors;
      }

   if (pointer_count != static_cast<const Value *>(this)->get_pointer_cell_count())
      {
        out << "*** Error: value " << voidP(this)
            << " has pointer_cell_count="
            << static_cast<const Value *>(this)->get_pointer_cell_count()
            << " but " << pointer_count << " PointerCells" << endl;
        ++errors;
      }

   return errors;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
cValue::clone(const char * loc) const
{
#ifdef cfg_PERFORMANCE_COUNTERS_WANTED
const uint64_t start_1 = cycle_counter();
#endif

Value_P Z(get_shape(), loc);
   if (flags.member)   Z->flags.member = 1;   // propagate member flag

   // NOTE: if something fails in the allocation of Z (⎕SYL etc.) then
   // Z->get_shape() may differ from this->get_shape(). We therefore use
   // the (in that case smaller) Z->element_count() here.
   //
   if (const ShapeItem count = Z->element_count())   // non-empty
      {
        if (is_packed())
           {
             // Fast path: packed ravels (RPT_BOOL/INT64/FLOAT64/COMPLEX/
             // UNICODE16/UNICODE32) hold homogeneous, non-nested data with
             // no PointerCells, so the per-element ownership bookkeeping
             // that next_ravel_Cell()/init_other() performs is unneeded.
             // Give Z the same packed layout as this and bulk-memcpy the
             // raw bytes instead of materialising and re-storing one Cell
             // at a time.
             Z->commit_ravel_like(*this, count);
             // bool: copy whole 64-bit words, not just ceil(count/8) bytes
             // -- see the identical fix/rationale in Bif_F12_ELEMENT.cc's
             // ∊B packed fast path (Blake McBride, Bugs21 #5): Z is fresh,
             // non-zeroed memory, and a RPT_BOOL ravel's "tail bits are
             // always zero" invariant (ScalarFunction.hh) must hold for
             // the whole last word, not just its first ceil(count/8)
             // bytes. The source ravel is always allocated to a whole-
             // word size, so the wider read stays in bounds.
             const size_t nbytes = (get_ravel_type() == RPT_BOOL)
                                  ? (size_t(count) + 63) / 64 * 8
                                  : size_t(count) * packed_bytes_per_item();
             // cast to void*: this is a bulk raw-byte copy of packed
             // storage, not construction of Cell objects (see the packed
             // fast-path comment above) -- Cell itself is not trivially
             // copyable (it has virtual functions), so memcpy'ing through
             // a bare Cell* triggers -Wclass-memaccess; the explicit cast
             // says so instead of suppressing the diagnostic wholesale.
             memcpy(reinterpret_cast<void *>(&Z->get_wfirst()),
                    cravel_packed(), nbytes);
           }
        else
           {
             loop(c, count)
                 {
                   Cell cache;
                   Z->next_ravel_Cell(get_cravel(c, cache));
                 }
           }
      }
   else                                              // empty
      {
        Cell cache;
        get_cproto(cache).init_other(&Z->get_wproto(), *Z, LOC);
      }

   Z->check_value(LOC);

   // Propagate cached depth: a clone has the same depth as the source.
   if (flags.value_depth != VF_DEPTH_DIRTY)
      Z->flags.value_depth = flags.value_depth;

#ifdef cfg_PERFORMANCE_COUNTERS_WANTED
   const uint64_t end_1 = cycle_counter();
   const uint64_t count1 = nz_element_count();
   Performance::fs_clone_B.add_sample(end_1 - start_1, count1);
#endif

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
cValue::check_lval_consistency() const
{
const ShapeItem ec = nz_element_count();

   /* check the following left value rules:
     
      1. every ravel item is either an LvalCell, or a PointerCell

      2. for every LvalCells:
      2a. either cell is 0 and owner is 0,
      2b. or cell is ≠0 and owner is ≠0 and cell lies in the ravel of owner
    */
   loop(e, ec)
       {
         Cell cache;
         const Cell * cell = &get_cravel(e, cache);   // rarely
         if (cell->is_pointer_cell())
            {
              cell->get_pointer_value()->check_lval_consistency();
            }
         else if (cell->is_lval_cell())
            {
              cell->check_consistency();
            }
         else       // error, e.g. 3{⍺+2←⍵}4
            {
              MORE_ERROR() << "mal-formed selective specification";
              SYNTAX_ERROR;
            }
       }
}
//════════════════════════════════════════════════════════════════════════════
ostream &
operator<<(ostream & out, const cValue & x)
{
   return x.print(out);
}
