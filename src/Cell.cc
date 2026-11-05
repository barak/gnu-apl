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

#include "CharCell.hh"
#include "ComplexCell.hh"
#include "Error.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "LvalCell.hh"
#include "Output.hh"
#include "PointerCell.hh"
#include "PrintOperator.hh"
#include "Value.hh"
#include "SystemLimits.hh"
#include "Workspace.hh"

#include "Cell.icc"

//════════════════════════════════════════════════════════════════════════════
ErrorCode
Cell::bif_equal(Cell * Z, const Cell * A) const
{
   // incompatible types ?
   //
   if (is_character_cell() != A->is_character_cell())   return IntCell::z0(Z);

   return IntCell::zI(Z, equal(*A, Workspace::get_CT()));
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Cell::bif_greater_eq(Cell * Z, const Cell * A) const
{
   return IntCell::zI(Z, (A->compare(*this) != COMP_LT) ? 1 : 0);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Cell::bif_greater_than(Cell * Z, const Cell * A) const
{
   return IntCell::zI(Z, (A->compare(*this) == COMP_GT) ? 1 : 0);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Cell::bif_less_eq(Cell * Z, const Cell * A) const
{
   return IntCell::zI(Z, (A->compare(*this) != COMP_GT) ? 1 : 0);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Cell::bif_less_than(Cell * Z, const Cell * A) const
{
   return IntCell::zI(Z, (A->compare(*this) == COMP_LT) ? 1 : 0);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Cell::bif_not_equal(Cell * Z, const Cell * A) const
{
   // incompatible types ?
   //
   if (is_character_cell() != A->is_character_cell())   return IntCell::z1(Z);

   return IntCell::zI(Z, !equal(*A, Workspace::get_CT()));
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::equal(const Cell & other, double qct) const
{
   MORE_ERROR() << "Cell::equal() : Objects of class " << get_classname()
                << " cannot be compared with objects of class"
                << other.get_classname();
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Unicode
Cell::get_char_value() const
{
   MORE_ERROR() << "Bad cell type " << int(get_cell_type())
                << " aka. " << get_cell_type_name(get_cell_type())
                << " when expecting a CHARACTER cell";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Cell::get_pointer_value() const
{
   MORE_ERROR() << "Bad cell type " << int(get_cell_type())
                << " aka. " << get_cell_type_name(get_cell_type())
                << " when expecting a nested cell";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Cell::try_pointer_value() const
{
   return is_pointer_cell() ? get_pointer_value() : Value_P();
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::greater(const Cell & other) const
{
   MORE_ERROR() << "Cell::greater() : Objects of class " << get_classname()
                << " cannot be compared with objects of class"
                << other.get_classname();
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Cell::to_value(const char * loc) const
{
   if (is_pointer_cell())
      {
        Value_P Z = get_pointer_value();   //->clone(LOC);
        return Z;
      }
   else
      {
        Value_P Z(loc);
        Z->set_ravel_Cell(0, *this);
        Z->check_value(LOC);
        return Z;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Cell::init_from_value(Value * value, Value & cell_owner, const char * loc)
{
   // this may overwrite a Cell of a value that already has a cached
   // ≡ depth (e.g. Symbol::assign_indexed()'s member-assignment path);
   // invalidate unconditionally since callers building a fresh Value
   // (whose depth cache already starts dirty) pay only a harmless no-op.
   cell_owner.invalidate_depth();

   // is_scalar(), not is_simple_scalar() (Bugs30 #13/#16 sibling): value
   // being scalar-SHAPED, regardless of what its one cell actually
   // contains, already means it is exactly one item -- copying that one
   // cell directly (via the polymorphic init_other(), which already
   // handles a PointerCell source correctly by cloning the pointed-to
   // value) both places a plain cell verbatim (the original,
   // is_simple_scalar()-only behavior) AND avoids wrapping an ALREADY
   // scalar-shaped enclosure in yet another PointerCell -- e.g.
   // (2⊃A)←⊂9 9 used to store ⊂⊂9 9 (double nested) instead of the ⊂9 9
   // that "the right array replaces it" (LRM p.43) calls for. Only a
   // genuinely non-scalar value (e.g. (2⊃A)←3 4 5, several items that
   // must be gathered into the one target slot) still needs the
   // single enclosing wrap in the else branch below.
   //
   if (value->is_scalar())
      {
        Cell cache;
        value->get_cfirst(cache).init_other(this, cell_owner, loc);
      }
   else
      {
        new (this) PointerCell(value, cell_owner);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Cell::init_type_at(void * dest, const Cell & other, Value & cell_owner,
                   const char * loc)
{
   if (other.is_pointer_cell())
      {
        // Somewhat tricky! We must 'Value_P proto' so that proto owns
        // the cloned value until PointerCell() takes over ownership of it.
        //
        Value_P proto = other.get_pointer_value()->clone(loc);
        Assert(!proto->is_simple_scalar());
        proto->to_type(false);
        new (dest) PointerCell(proto.get(), cell_owner);
      }
   else if (other.is_lval_cell())      new (dest) LvalCell(0, 0);
   else if (other.is_character_cell()) new (dest) CharCell(UNI_SPACE);
   else                                new (dest) IntCell(0);
}
//────────────────────────────────────────────────────────────────────────────
void *
Cell::operator new(std::size_t s, void * pos)
{
   return pos;
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::A_greater_B(const Cell * const & A, const Cell * const & B,
                  const void * /* comp_arg not used */)
{
   return A->greater(*B);
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::compare_ptr(const Cell * const & A, const Cell * const & B,
                  const void * unused_comp_arg)
{
   return A > B;
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::compare_stable(const Cell * const & A, const Cell * const & B,
                  const void * unused_comp_arg)
{
   if (const Comp_result cr = A->compare(*B))   return cr == COMP_GT;
   return A > B;
}
//────────────────────────────────────────────────────────────────────────────
void
Cell::copy(Value & val, const cValue & src, ShapeItem & idx, ShapeItem count)
{
   loop(c, count)
      {
        Assert1(val.more());
        Cell cache;
        val.next_ravel_Cell(src.get_cravel(idx++, cache));
      }
}
//────────────────────────────────────────────────────────────────────────────
const char *
Cell::get_cell_type_name(CellType ct)
{
   switch(ct & CT_MASK)
      {
        case CT_NONE:    return "NONE";
        case CT_BASE:    return "BASE";
        case CT_CHAR:    return "CHARACTER";
        case CT_POINTER: return "NESTED";
        case CT_CELLREF: return "LEFTVAL";
        case CT_INT:     return "INTEGER";
        case CT_FLOAT:   return "FLOAT";
        case CT_COMPLEX: return "COMPLEX";
      }

   return "UNKNOWN";
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::greater_cp(const ShapeItem &  A, const ShapeItem & B, const void * ctx)
{
const ravel_comp_len * rcl = reinterpret_cast<const ravel_comp_len *>(ctx);
const ShapeItem comp_len = rcl->comp_len;
ShapeItem idxA = A * comp_len;
ShapeItem idxB = B * comp_len;

   loop(l, comp_len)
       {
         Cell cA, cB;
         if (const Comp_result cr = rcl->value->get_cravel(idxA++, cA)
                                        .compare(rcl->value->get_cravel(idxB++, cB)))
            return cr == COMP_GT;
       }

   // at this point all cells were equal
   //
   return A > B;
}
//────────────────────────────────────────────────────────────────────────────
int
Cell::compare_int_float(APL_Integer i, APL_Float f)
{
   // Bugs30 #4 / Bugs31 #8/#9 (Blake McBride): comparing an IntCell
   // against a FloatCell (or a real-valued ComplexCell) by converting
   // *both* operands via get_real_value() loses precision once the
   // IntCell's exact value exceeds 2⋆53, silently misordering or
   // misequating (e.g. 9007199254740993 vs 9007199254740992.0).
   // Compare i's exact int64 value against f's double without ever
   // converting i to double: floor() of a double never loses precision
   // beyond what the double already has, so casting that floor back to
   // int64 (once known to be in int64 range) exactly recovers the
   // integer value the double represents, whatever its magnitude.
   //
   if (f >= 9223372036854775808.0)    return -1;   // f ≥ 2⋆63
   if (f < -9223372036854775808.0)    return 1;    // f < -2⋆63

const APL_Float f_floor = floor(f);
const APL_Integer i_floor = APL_Integer(f_floor);
   if (i < i_floor)   return -1;
   if (i > i_floor)   return 1;
   if (f > f_floor)   return -1;   // i == ⌊f⌋ < f
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
/// EXACT (non-⎕CT) numeric comparison, shared by greater_cp_exact() and
/// smaller_cp_exact(): 0 if equal, negative if ca < cb, positive if
/// ca > cb. Non-numeric or complex cells fall back to the ordinary,
/// possibly ⎕CT-tolerant Cell::compare() -- ⍋/⍒'s ISO-mandated
/// exactness (10.1.2: "⎕CT is not an implicit argument" to grading) is
/// specifically about real numeric magnitude; nothing in Bugs27 #30
/// exercises complex or mixed-type grading, and compare()'s existing
/// char/nested/complex ordering rules are unaffected either way.
static int
exact_numeric_compare(const Cell & ca, const Cell & cb)
{
   if (ca.is_integer_cell() && cb.is_integer_cell())
      {
        // compare as int64, not via get_real_value()'s round-trip
        // through double: an IntCell's exact value can exceed 2⋆53,
        // beyond which double can no longer distinguish adjacent
        // integers, silently misordering ⍋/⍒ (Bugs28 #28, Bugs27 #30
        // residual).
        //
        const APL_Integer ia = ca.get_int_value();
        const APL_Integer ib = cb.get_int_value();
        if (ia < ib)   return -1;
        if (ia > ib)   return 1;
        return 0;
      }

   if (ca.is_numeric() && cb.is_numeric() &&
       !ca.is_complex_cell() && !cb.is_complex_cell())
      {
        // Bugs30 #4 (Blake McBride): an IntCell/FloatCell pair (the
        // IntCell/IntCell pair above is already exact) still converted
        // *both* operands via get_real_value(), so a large IntCell
        // (beyond 2⋆53) rounded to the same double as a nearby
        // FloatCell and compared equal, misordering ⍋/⍒ (e.g.
        // 9007199254740993 vs 9007199254740992.0). Compare the
        // IntCell's exact int64 against the FloatCell's double without
        // ever converting the int64 side to double: floor() of a
        // double never loses precision beyond what the double already
        // has, so casting that floor back to int64 (once known to be
        // in int64 range) exactly recovers the integer value the
        // double represents, whatever its magnitude.
        //
        if (ca.is_integer_cell() != cb.is_integer_cell())
           {
             const bool a_is_int = ca.is_integer_cell();
             const APL_Integer i  = a_is_int ? ca.get_int_value()
                                              : cb.get_int_value();
             const APL_Float   f  = a_is_int ? cb.get_real_value()
                                              : ca.get_real_value();
             const int cr = Cell::compare_int_float(i, f);
             return a_is_int ? cr : -cr;
           }

        const APL_Float fa = ca.get_real_value();
        const APL_Float fb = cb.get_real_value();
        if (fa < fb)   return -1;
        if (fa > fb)   return 1;
        return 0;
      }

   return ca.compare(cb);   // COMP_LT(-1)/COMP_EQ(0)/COMP_GT(1) compatible
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::greater_cp_exact(const ShapeItem & A, const ShapeItem & B,
                       const void * ctx)
{
const ravel_comp_len * rcl = reinterpret_cast<const ravel_comp_len *>(ctx);
const ShapeItem comp_len = rcl->comp_len;
ShapeItem idxA = A * comp_len;
ShapeItem idxB = B * comp_len;

   loop(l, comp_len)
       {
         Cell cA, cB;
         if (const int cr = exact_numeric_compare(
                                rcl->value->get_cravel(idxA++, cA),
                                rcl->value->get_cravel(idxB++, cB)))
            return cr > 0;
       }

   return A > B;
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::smaller_cp_exact(const ShapeItem & A, const ShapeItem & B,
                       const void * ctx)
{
const ravel_comp_len * rcl = reinterpret_cast<const ravel_comp_len *>(ctx);
const ShapeItem comp_len = rcl->comp_len;
ShapeItem idxA = A * comp_len;
ShapeItem idxB = B * comp_len;

   loop(l, comp_len)
       {
         Cell cA, cB;
         if (const int cr = exact_numeric_compare(
                                rcl->value->get_cravel(idxA++, cA),
                                rcl->value->get_cravel(idxB++, cB)))
            return cr < 0;
       }

   return A > B;
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::is_near_int(APL_Float value)
{
   // all large values are considered int because their fractional part
   // has been rounded off. However, they may not fit into an int64_t,
   // and if that is the concern then use is_near_int64_t() below.

   if (value > LARGE_INT)   return true;
   if (value < SMALL_INT)   return true;

const APL_Float result = nearbyint(value);
const APL_Float diff = value - result;
   if (diff >= INTEGER_TOLERANCE)    return false;
   if (diff <= -INTEGER_TOLERANCE)   return false;

   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::is_near_int64_t(APL_Float value)
{
   if (value > LARGE_INT)   return false;
   if (value < SMALL_INT)   return false;

const APL_Float result = nearbyint(value);
const APL_Float diff = value - result;
   if (diff >=  INTEGER_TOLERANCE)   return false;
   if (diff <= -INTEGER_TOLERANCE)   return false;

   return true;
}
//────────────────────────────────────────────────────────────────────────────
APL_Integer
Cell::near_int(APL_Float value)
{
   if (value >= LARGE_INT)   DOMAIN_ERROR;
   if (value <= SMALL_INT)   DOMAIN_ERROR;

const APL_Float result = nearbyint(value);
const APL_Float diff = value - result;
   if (diff >  INTEGER_TOLERANCE)   DOMAIN_ERROR;
   if (diff < -INTEGER_TOLERANCE)   DOMAIN_ERROR;

   if (result > 0.0)   return   APL_Integer(0.3 + result);
   else                return - APL_Integer(0.3 - result);
}
//────────────────────────────────────────────────────────────────────────────
bool
Cell::smaller_cp(const ShapeItem &  A, const ShapeItem & B, const void * ctx)
{
const ravel_comp_len * rcl = reinterpret_cast<const ravel_comp_len *>(ctx);
const ShapeItem comp_len = rcl->comp_len;
ShapeItem idxA = A * comp_len;
ShapeItem idxB = B * comp_len;

   loop(l, comp_len)
       {
         Cell cA, cB;
         if (const Comp_result cr = rcl->value->get_cravel(idxA++, cA)
                                        .compare(rcl->value->get_cravel(idxB++, cB)))
            return cr == COMP_LT;
       }

   // at this point all cells were equal
   //
   return A > B;
}
//════════════════════════════════════════════════════════════════════════════
ostream &
operator <<(ostream & out, const Cell & cell)
{
PrintBuffer pb = cell.character_representation(PR_BOXED_GRAPHIC);
UCS_string ucs(pb, 0, Workspace::get_PW());
   return out << ucs;
}
//════════════════════════════════════════════════════════════════════════════
ErrorCode
Cell::sorted_indices(vector<ShapeItem> & indices, const cValue & value,
                     Sort_order order, ShapeItem comp_len, bool exact)
{
   Assert(indices.size() == 0);   // initially empty, filled by this function

   // get_shape_item(0) (the length of the first axis) is only the
   // correct "number of comparison units" when value's rank matches
   // comp_len's own meaning -- e.g. a proper R×comp_len matrix, or a
   // plain vector with comp_len==1. The sorted-A fast path in
   // Bif_F12_INDEX_OF.cc calls this with comp_len==1 (scalar-by-scalar
   // comparison) even when value itself is rank ≥ 2 (e.g. 8 8⍴1): there
   // get_shape_item(0) is 8, not the 64 actual scalar items, so indices
   // ends up undersized and find_B_in_sorted_A()'s own
   // Assert(len_A == Idx_A.size()) fires (or, with that Assert compiled
   // out at the default assert level, the search reads out of bounds).
   // element_count()/comp_len is correct in both cases.
   //
const ShapeItem rows = value.element_count() / (comp_len ? comp_len : 1);

   // initialize indices with 0, 1, ... length-1
   //
   try           { indices.reserve(rows); }
   catch (std::bad_alloc &) { return E_WS_FULL; }
   catch (...)              { FIXME; }
   loop(r, rows)   indices.push_back(r);

   // the cpmparison context ctx for the sorting is the cells of the APL
   // values and the number of consecutive cells to be compared. The APL
   // value can be a vector (and then comp_len = 1) or a matrix with
   // comp_len columns.
   //
const ravel_comp_len ctx = { &value, comp_len};
   if (order == SORT_ASCENDING)
      Heapsort<ShapeItem>::sort(indices,
                    exact ? &Cell::greater_cp_exact : &Cell::greater_cp, &ctx);
   else
      Heapsort<ShapeItem>::sort(indices,
                    exact ? &Cell::smaller_cp_exact : &Cell::smaller_cp, &ctx);
   return E_NO_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
