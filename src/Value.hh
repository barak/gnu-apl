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

#ifndef __VALUE_HH_DEFINED__
#define __VALUE_HH_DEFINED__

#include "CharCell.hh"
#include "ComplexCell.hh"
#include "DynamicObject.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "LvalCell.hh"
#include "NumericCell.hh"
#include "PointerCell.hh"
#include "Ravel.hh"
#include "Shape.hh"

using namespace std;

class CDR_string;
class Error;
class IndexExpr;
class PrintBuffer;
class ScalarFunction;
class Symbol;
class Value_P;
class Thread_context;
class Value;   // forward declaration for cValue parameter types

//════════════════════════════════════════════════════════════════════════════
/// a linked list of deleted values
struct _deleted_value
{
   /// the next deleted value
  _deleted_value * next;
};
//════════════════════════════════════════════════════════════════════════════
/// APL value data members, factored out so that a future cValue can provide
/// const-only access without the ownership overhead of class Value.
class ValueBase
{
protected:
   ValueBase()
   : flags{}
   { flags.value_depth = VF_DEPTH_DIRTY; }

   /// the shape of this value (only the first rank items are valid)
   Shape shape;

   /// flags and cached depth for this value
   mutable VF_Flags flags;

   /// the ravel of this value (cells, fetch function, and bookkeeping)
   Ravel ravel;

   /// the number of PointerCells pointing to sub-values of \b this value
   ShapeItem pointer_cell_count;
};
//════════════════════════════════════════════════════════════════════════════
class cValue;
typedef const cValue & cValue_R;

/// const-only view of an APL value, providing all read-only operations.
/// Inserted between ValueBase and Value so that code needing only
/// read access can use cValue without incurring ownership overhead.
class cValue : public ValueBase
{
   friend class Value_P;
   friend class Value_P_Base;
   friend class PointerCell;   // needs & for &cell_owner
   friend class PJob_scalar_AB;
   friend class PJob_scalar_B;

public:
   /// return \b true iff \b this value is a scalar.
   bool is_scalar() const
      { return shape.get_rank() == 0; }

   /// return \b true iff \b this value is a simple (i.e. depth 0) scalar.
   bool is_simple_scalar() const
      { return is_scalar() &&
              !(is_pointer_cell(0) || get_lval_cellowner()); }

   /// return \b true iff \b this value is a numeric scalar.
   bool is_numeric_scalar() const
      { return  is_scalar() && is_numeric(0); }

   /// return \b true iff \b this value is a character scalar.
   bool is_character_scalar() const
      { return  is_scalar() && is_character_cell(0); }

   /// return \b true iff \b this value is empty (some dimension is 0).
   bool is_empty() const
      { return shape.is_empty(); }

   /// return \b true iff \b * \b this \b ≡ \b ⍬ (aka. \b ⍳0).
   bool is_zilde() const
      { return get_rank() == 1 &&
               get_cols() == 0 &&
               is_integer_cell(0);
      }

   /// return \b true iff \b * \b this \b ≡ \b ''
   bool is_str0() const
      { return get_rank() == 1 &&
               get_cols() == 0 &&
               is_character_cell(0);
      }

   /// return \b true iff \b this value is a scalar or vector
   bool is_scalar_or_vector() const
      { return  get_rank() < 2; }

   /// return \b true iff \b this value is a vector.
   bool is_vector() const
      { return  get_rank() == 1; }

   /// return \b true iff \b this value is a scalar or vector of length 1.
   bool is_scalar_or_len1_vector() const
      { return is_scalar() || (is_vector() && (get_shape_item(0) == 1)); }

   /// return \b true iff \b this value can be scalar-extended
   bool is_scalar_extensible() const
      { return element_count() == 1; }

   /// return the increment for iterators of this value. The increment is used
   /// for scalar-extension of 1-element values
   int get_increment() const
      { return element_count() == 1 ? 0 : 1; }

   /// return \b true iff \b this value is a simple character scalar
   /// or vector.
   bool is_char_string() const
      { return get_rank() <= 1 && is_char_array(); }

   /// return \b true iff \b this value is a simple character vector.
   bool is_char_vector() const
      { return get_rank() == 1 && is_char_array(); }

   /// return \b true iff \b this value is a simple character vector
   /// containing only APL characters (from ⎕AV)
   bool is_apl_char_vector() const;

   /// return \b true iff \b all ravel elements of this value are characters
   bool is_char_array() const;

   /// return \b true iff \b this value is a simple character scalar.
   bool is_char_scalar() const
      { return get_rank() == 0 && is_character_cell(0); }

   /// return \b true iff \b this value is a simple integer scalar.
   bool is_int_scalar() const
      { return get_rank() == 0 && is_near_int(0); }

   /// return the number of elements (the product of the shapes).
   ShapeItem element_count() const
      { return shape.get_volume(); }

   /// return the number of elements, but at least 1 (for the prototype).
   ShapeItem nz_element_count() const
      { return shape.get_nz_volume(); }

   /// return the rank of \b this value
   uRank get_rank() const
     { return shape.get_rank(); }

   /// return the shape of \b this value
   const Shape & get_shape() const
     { return shape; }

   /// return the r'th shape element of \b this value
   /// @param r axis index (0-based)
   ShapeItem get_shape_item(sRank r) const
      { return shape.get_shape_item(r); }

   /// return the length of the first dimension of \b this value, i.e. ↑⍴this
   ShapeItem get_first_shape_item() const
      { return shape.get_first_shape_item(); }

   /// return the length of the last dimension of \b this value, i.e. ¯1↑⍴this
   ShapeItem get_last_shape_item() const
      { return shape.get_last_shape_item(); }

   /// return the length of the last axis, or 1 for scalars
   ShapeItem get_cols() const
      { return shape.get_cols(); }

   /// return the product of all but the the last axis, or 1 for scalars
   ShapeItem get_rows() const
      { return shape.get_rows(); }

   /// return the position of cell in the ravel of \b this value. Only
   /// meaningful for an unpacked (RPT_CELLS) ravel, where \b cell is a
   /// genuine address inside ravel.cells (e.g. an LvalCell/PointerCell
   /// target); packed ravels have no per-Cell addresses to compare against.
   /// @param cell pointer to the ravel cell whose offset is required
   ShapeItem get_offset(const Cell * cell) const
      { return cell - ravel.cells; }

   /// return the next byte after the ravel
   const Cell * get_ravel_end() const
      { return ravel.cells + nz_element_count(); }

   /// return the integer of a value that is supposed to have (exactly) one
   APL_Integer get_sole_integer() const
      { if (element_count() == 1)   return get_near_int(0);
        if (get_rank() > 1)   RANK_ERROR;
        else                  LENGTH_ERROR;
      }

   /// return the first integer of a value (the line number of →Value).
   Function_Line get_line_number() const
      { const APL_Integer line(ravel.get_near_int(0));
        Log(LOG_execute_goto)   CERR << "goto line " << line << endl;
        return Function_Line(line); }

   /// return the boolean value of a value that is supposed to have
   /// (exactly) one
   bool get_sole_bool() const;

   /// return \b true iff \b this value is simple (i.e. not nested).
   bool is_simple() const;

   /// return \b true iff \b this value and its ravel items have rank < 2
   bool is_one_dimensional() const;

   /// return \b true iff \b this value is a (simple) integer array, allowing
   /// near-int values (but NOT necessatily values within ⎕CT of an integers).
   bool is_int_array() const;

   /// return \b true iff \b this value is a (simple) integer vector.
   bool is_int_vector() const
      { return (get_rank() == 1) && is_int_array(); }

   /// return true, if this value has complex cells, false iff it has only
   /// real cells. Throw domain error for other cells (char, nested, etc.)
   /// if check_numeric is \b true.
   /// @param check_numeric if true, throw DOMAIN_ERROR for non-numeric cells
   bool is_complex(bool check_numeric) const;

   /// true if Value flag \b member is set
   bool is_member() const
      { return flags.member; }

   /// true if this value has a homogeneous (non-Cell) ravel
   bool is_packed() const
      { return flags.ravel_type != 0; }

   /// true if this value has a bool-packed (bit-array) ravel
   bool is_bool_packed() const
      { return flags.ravel_type == RPT_BOOL; }

   /// true if Value flag \b marked is set
   bool is_marked() const
      { return flags.marked; }

   /// true if Value flag \b complete is set
   bool is_complete() const
      { return flags.complete; }

   /// true if Value flag \b left_value is set, i.e. this value's ravel
   /// consists entirely of LvalCells (set by Value::get_cellrefs()); a
   /// selective-assignment target. See VF_Flags::left_value.
   bool is_left_value() const
      { return flags.left_value; }

   /// mark the cached ≡ depth as invalid. Needed whenever a Cell owned by
   /// this value is replaced (in-place, i.e. after this value may already
   /// have a cached depth) by something that could change the depth of
   /// this value, e.g. indexed/selective/member assignment. Front-door
   /// ravel mutators (next_ravel_Pointer(), set_ravel_Pointer(), ...) and
   /// assign_cellrefs() already call this internally; only call it
   /// directly when adding a new PointerCell to an existing value outside
   /// of those paths (see Value::add_member(), Value::get_new_member(),
   /// Cell::init_from_value()).
   void invalidate_depth() const
      { flags.value_depth = VF_DEPTH_DIRTY; }

   /// return true if this variable is is_member() and a N×2 matrix
   /// with proper keys
   bool is_structured() const;

   /// return true if this value can be compared. This is the case when all
   /// cells (including nested ones) are not complex
   bool can_be_compared() const;

   /// optimized comparison function for strings
   /// @param ucs Unicode string to compare with this value's ravel
   bool equal_string(const UCS_string & ucs) const;

   /** return the existing member of this value, defined by \b members. Thei
       first name in members is the deepest, while the last name is the name
       of the variable containing the members (and is only used in error
       printouts). Return 0 if member was not found.
    **/
   /// @param members path of member names from deepest to outermost
   const Cell * get_existing_member(const vector<const UCS_string *>
                                    & members)const;

   /// like get_existing_member(), but returns 0 instead of throwing
   /// for every "no such member" condition along the way (missing
   /// member, non-structured parent, wrong rank/cols) -- for callers
   /// (⎕EX, )ERASE) that only need to know whether something is there
   /// to remove, not to reference it. See Bugs28 #53.
   /// @param members path of member names from deepest to outermost
   const Cell * try_existing_member(const vector<const UCS_string *>
                                    & members) const;

   /// return the Cell (if any) containing the data of structured value member
   /// \b member, or 0 if member not found.
   /// @param member name of the structured member to look up
   const Cell * get_member_data(const UCS_string & member) const;

   /// store the row numbers (starting at 0) so that this[rows] is sorted
   /// in result. Only used rows are returned.
   /// @param result output vector receiving sorted row indices
   /// @param filter optional Unicode key to restrict which members are included
   void sorted_members(std::vector<ShapeItem> & result,
                       const Unicode * filter) const;

   /// store the row numbers (starting at 0) in result so that this[rows]
   /// is a (used) row, maybe sorted.
   /// in result. Only used rows are returned.
   /// @param result output vector receiving row indices
   /// @param sorted if true, return indices in sorted order
   void used_members(std::vector<ShapeItem> & result, bool sorted) const;

   /// return the number of (valid) members, as per this[1;]
   ShapeItem get_member_count() const;

   /// current ravel packing type (RPT_CELLS = unpacked Cell array)
   RavelType get_ravel_type() const
      { return RavelType(flags.ravel_type); }

   /// raw read pointer for RPT_INT64 ravels; call only when ravel_type==RPT_INT64
   const int64_t * cravel_int64() const
      { return reinterpret_cast<const int64_t *>(ravel.cells); }

   /// raw read pointer for RPT_FLOAT64 ravels; call only when ravel_type==RPT_FLOAT64
   const double * cravel_float64() const
      { return reinterpret_cast<const double *>(ravel.cells); }

   /// raw read pointer for RPT_UNICODE16 ravels; call only when ravel_type==RPT_UNICODE16
   const uint16_t * cravel_unicode16() const
      { return reinterpret_cast<const uint16_t *>(ravel.cells); }

   /// raw read pointer for RPT_UNICODE32 ravels; call only when ravel_type==RPT_UNICODE32
   const Unicode * cravel_unicode32() const
      { return reinterpret_cast<const Unicode *>(ravel.cells); }

   /// raw read pointer for RPT_BOOL ravels; call only when ravel_type==RPT_BOOL
   const uint64_t * cravel_bool() const
      { return reinterpret_cast<const uint64_t *>(ravel.cells); }

   /// raw read pointer for RPT_COMPLEX ravels (re,im pairs); call only when ravel_type==RPT_COMPLEX
   const double * cravel_complex() const
      { return reinterpret_cast<const double *>(ravel.cells); }

   /// raw read pointer to packed data for any non-BOOL packed ravel
   const void * cravel_packed() const
      { return ravel.cells; }

   /// packed element size in bytes: 8/8/16/4/2 for INT64/FLOAT64/COMPLEX/U32/U16;
   /// returns 0 for RPT_CELLS (not packed) and RPT_BOOL (bit-packed, needs special handling)
   ShapeItem packed_bytes_per_item() const;

   /// Indexed scalar accessors — return native C++ values without
   /// materialising a temporary Cell (no cell_fetch_cache aliasing hazard).
   APL_Integer get_int_value(ShapeItem idx) const
      { return ravel.get_int_value(idx); }

   APL_Integer get_near_int(ShapeItem idx) const
      { return ravel.get_near_int(idx); }

   bool get_near_bool(ShapeItem idx) const
      { return ravel.get_near_bool(idx); }

   Unicode get_char_value(ShapeItem idx) const
      { return ravel.get_char_value(idx); }

   APL_Float get_real_value(ShapeItem idx) const
      { return ravel.get_real_value(idx); }

   APL_Float get_imag_value(ShapeItem idx) const
      { return ravel.get_imag_value(idx); }

   int get_byte_value(ShapeItem idx) const
      { return ravel.get_byte_value(idx); }

   APL_Complex get_complex_value(ShapeItem idx) const
      { return ravel.get_complex_value(idx); }

   CellType get_cell_type(ShapeItem idx) const
      { return ravel.get_cell_type(idx); }

   CellType get_cell_subtype(ShapeItem idx) const
      { return ravel.get_cell_subtype(idx); }

   Value_P get_pointer_value(ShapeItem idx) const
      { return ravel.get_pointer_value(idx); }

   /// like get_pointer_value(idx), but returns an empty (null) Value_P
   /// instead of throwing DOMAIN_ERROR when the cell at idx is not a
   /// pointer cell -- see Ravel::try_pointer_value() and Cell::try_pointer_value().
   /// Lets callers write if (Value_P v = X.try_pointer_value(idx)) { ... }
   /// instead of a separate is_pointer_cell(idx) check first.
   Value_P try_pointer_value(ShapeItem idx) const
      { return ravel.try_pointer_value(idx); }

   bool is_pointer_cell(ShapeItem idx) const
      { return ravel.is_pointer_cell(idx); }
   bool is_lval_cell(ShapeItem idx) const
      { return ravel.is_lval_cell(idx); }
   bool is_simple_cell(ShapeItem idx) const
      { return ravel.is_simple_cell(idx); }
   bool is_character_cell(ShapeItem idx) const
      { return ravel.is_character_cell(idx); }
   bool is_integer_cell(ShapeItem idx) const
      { return ravel.is_integer_cell(idx); }
   bool is_numeric(ShapeItem idx) const
      { return ravel.is_numeric(idx); }
   bool is_complex_cell(ShapeItem idx) const
      { return ravel.is_complex_cell(idx); }
   bool is_near_int(ShapeItem idx) const
      { return ravel.is_near_int(idx); }
   bool is_near_bool(ShapeItem idx) const
      { return ravel.is_near_bool(idx); }
   bool is_near_real(ShapeItem idx) const
      { return ravel.is_near_real(idx); }
   bool is_real_cell(ShapeItem idx) const
      { return ravel.is_real_cell(idx); }

   /// return the (constant) idx'th element of the ravel, materialising a
   /// packed cell into caller-supplied \b cache instead of a shared,
   /// per-Value cache -- safe even when two references from the same (or
   /// an aliased) Value must be live simultaneously (e.g. inside a
   /// comparator).
   /// @param idx ravel index (0-based)
   /// @param cache caller-owned Cell to materialise a packed element into
   const Cell & get_cravel(ShapeItem idx, Cell & cache) const
      {
        Assert1(idx < nz_element_count());

        // packed-empty-value special case: for packed empty values
        // cells[0] holds Cell vtable bits, not valid packed data -- the
        // prototype was never written in the packed format. Materialise 0.
        // (Quad_CR10.tc regression: do_CR10_level's counting loop hit
        // exactly this, idx 0 on a packed empty value, via nz_element_count()
        // being "at least 1" for the prototype.)
        //
        if (idx == 0 && is_packed() && is_empty())
           { new (&cache) IntCell(0); return cache; }

        const Cell & result = ravel.fetcher(idx, ravel.cells, cache);

        // every fetcher except cell_fetcher() (whose cache parameter is
        // intentionally unused -- an unpacked ravel already holds real
        // Cells, cells[offset] IS the cell) must materialise into, and
        // return a reference to, *this* cache -- not some other object.
        //
        Assert(ravel.fetcher == &Ravel::cell_fetcher || &result == &cache);
        return result;
      }

   /// like get_cravel(idx, cache), but for the first element of the ravel
   /// (which is always present); same as get_cproto(cache), but named
   /// differently to indicate its context.
   const Cell & get_cfirst(Cell & cache) const
      {
        if (is_packed() && is_empty())
           { new (&cache) IntCell(0); return cache; }
        return get_cravel(0, cache);
      }

   /// like get_cfirst(Cell&), but for the first element of the ravel (the
   /// prototype of an empty value); same as get_cfirst(cache) above.
   const Cell & get_cproto(Cell & cache) const
      {
        if (is_packed() && is_empty())
           { new (&cache) IntCell(0); return cache; }
        return get_cravel(0, cache);
      }

   /// like get_cfirst(Cell&), but for the single ravel element of a scalar.
   const Cell & get_cscalar(Cell & cache) const
      { return get_cravel(0, cache); }

   /// return \b true iff more ravel items (as per shape) need to be initialized.
   /// (the prototype of empty values may still be missing)
   bool more() const
      { return ravel.valid_ravel_items < element_count(); }

   /// return \b true iff \b this value has the same rank as \b other.
   /// @param other value to compare rank against
   bool same_rank(const cValue & other) const
      { return get_rank() == other.get_rank(); }

   /// return \b true iff \b this value has the same shape as \b other.
   /// @param other value to compare shape against
   bool same_shape(const cValue & other) const
      { if (get_rank() != other.get_rank())   return false;
        loop (r, get_rank())
            if (get_shape_item(r) != other.get_shape_item(r))   return false;
        return true;
      }

   /// return \b true iff \b this value has the same shape as \b other or one
   /// of the values is a scalar
   /// @param other value to compare shape against
   bool scalar_matching_shape(const cValue & other) const
      { return is_scalar_extensible()
            || other.is_scalar_extensible()
            || same_shape(other);
      }

   /// return \b true if shapes of this and other differ only by axes of
   /// length 1
   /// @param other value whose shape is checked for conformance
   bool conforms_to(const cValue & other) const
      { return get_shape().conforms_to(other.get_shape()); }

   /// Return the number of scalars in this value (enlist).
   ShapeItem get_enlist_count() const;

   /// compute the depth of this value.
   APL_types::Depth compute_depth() const;

   /// store the scalars in this (left-)value into dest...
   /// @param Z destination value to receive the enlisted cells
   void enlist_left(Value & Z) const;

   /// store the scalars in this value into dest...
   /// @param Z destination value to receive the enlisted cells
   void enlist_right(Value & Z) const;

   /// compute the cell types contained in the top level of \b this value
   CellType flat_cell_types() const;

   /// compute the cell subtypes contained in the top level of \b this value
   CellType flat_cell_subtypes() const;

   /// compute the CellType contained in \b this value (recursively)
   CellType deep_cell_types() const;

   /// recursive set of Cell types in this value
   CellType deep_cell_subtypes() const;

   /// print \b this value (line break at Workspace::get_PW())
   /// @param out output stream to write to
   ostream & print(ostream & out) const;

   /// print \b this empty value of rank ≥ 2 (empty lines as IBM APL2 does)
   /// @param out output stream to write to
   ostream & print_empty(ostream & out) const;

   /// the number of (empty) lines in the display of \b this empty value of
   /// rank ≥ 2 (as IBM APL2 displays it), 0 for rank < 2
   ShapeItem empty_lines() const;

   /// print some information to help identifying \b this value (shape, depth,
   /// and the first few ravel items)
   /// @param out output stream to write to
   ostream & print_brief(ostream & out) const;

   /// print \b this member value
   /// @param out output stream to write to
   /// @param member name of the member to display
   ostream & print_members(ostream & out, UCS_string member) const;

   /// print \b this value (line break at print_width)
   /// @param out output stream to write to
   /// @param pctx print formatting context
   ostream & print1(ostream & out, PrintContext pctx) const;

   /// print the properties (shape, flags etc) of \b this value
   /// @param out output stream to write to
   /// @param indent indentation level in spaces
   /// @param help true to include additional help text
   ostream & print_properties(ostream & out, int indent, bool help) const;

   /// debug-print \b this value
   /// @param info descriptive label shown before the value
   void debug(const char * info) const;

   /// print this value in 4 ⎕CR style
   /// @param out output stream to write to
   /// @param indent indentation level in spaces
   ostream & print_boxed(ostream & out, int indent) const;

   /// return \b this indexed by (multi-dimensional) \b IDX.
   /// @param IDX multi-dimensional index expression
   Value_P index(const IndexExpr & IDX) const;

   /// return a bitmap of integers in \b this value. Normalized to ⎕IO←0.
   /// @param where caller description used in error messages
   /// @param rank_B rank of the array being indexed (for validation)
   AxesBitmap to_bitmap(const char * where, uRank rank_B) const;

   /// return \b this indexed by (one-dimensional) \b IDX.
   /// @param X one-dimensional index value
   Value_P index(cValue_R X) const;

   /// return \b true iff this value is an lval (selective assignment)
   /// i.e. return true if at least one leaf value is an lval.
   Value * get_lval_cellowner() const;

   /// return the NOTCHAR property of the value. NOTCHAR is false for simple
   /// char arrays and true if any element is numeric or nested. The NOTCHAR
   /// property of empty arrays is the NOTCHAR property of its prototype.
   /// see also lrm p. 138.
   bool NOTCHAR() const;

   /// print debug info about setting or clearing of flags to CERR
   /// @param loc caller location for diagnostics
   /// @param flag_bit the flag being modified, flat-encoded the same way
   ///                 get_flags() packs it (see the SET_/CLEAR_ functions
   ///                 below) -- NOT a VF_Flags bitfield
   /// @param flag_name name of the flag as a C string
   /// @param set true if the flag is being set, false if cleared
   void flag_info(const char * loc, uint32_t flag_bit, const char * flag_name,
                  bool set) const;

/// maybe enable LOC for set/clear of flags
#if defined(cfg_VF_TRACING_WANTED) || \
    defined(cfg_VALUE_HISTORY_WANTED)  // enable LOC
#  define _LOC LOC
#  define _loc loc
#  define _loc_type const char *
#else                                                           // disable LOC
#  define _LOC
#  define _loc
#  define _loc_type
#endif

#ifdef cfg_VF_TRACING_WANTED
 # define FLAG_TRACE(f, bit, b) flag_info(loc, bit, #f, b);
#else
 # define FLAG_TRACE(_f, _bit, _b)
#endif

   /// set the Value flag \b member
   void SET_member(_loc_type _loc) const
      { FLAG_TRACE(member, 0x08, true)   flags.member = 1;
        ADD_EVENT(this, VHE_SetFlag, 0x08, _loc); }

#define set_member() SET_member(_LOC)


   /// set the Value flag \b complete
   void SET_complete(_loc_type _loc) const
      { FLAG_TRACE(complete, 0x01, true)   flags.complete = 1;
        ADD_EVENT(this, VHE_SetFlag, 0x01, _loc); }

#define set_complete() SET_complete(_LOC)

   /// set the Value flag \b left_value
   void SET_left_value(_loc_type _loc) const
      { FLAG_TRACE(left_value, 0x10, true)   flags.left_value = 1;
        ADD_EVENT(this, VHE_SetFlag, 0x10, _loc); }

#define set_left_value() SET_left_value(_LOC)

   /// set the Value flag \b marked
   void SET_marked(_loc_type _loc) const
      { FLAG_TRACE(marked, 0x02, true)   flags.marked = 1;
        ADD_EVENT(this, VHE_SetFlag, 0x02, _loc); }

   /// clear the Value flag \b marked
   void CLEAR_marked(_loc_type _loc) const
      { FLAG_TRACE(marked, 0x02, false)   flags.marked = 0;
        ADD_EVENT(this, VHE_ClearFlag, 0x02, _loc); }

#define set_marked()   SET_marked(_LOC)
#define clear_marked() CLEAR_marked(_LOC)

   /// return the (recursive) number of members in this value
   ShapeItem get_all_members_count() const;

   /// clear marked flag on this value and its nested sub-values
   void unmark() const;

   /// the prototype of this value
   /// @param loc caller location for diagnostics
   Value_P prototype(const char * loc) const;

/** macro NEW_CLONE selects one of two clone() schemes:

   1. the old scheme (with # undef NEW_CLONE) clones values early, so that:
   1a. Different PointerCells always point to different Sub-Values, and
   1b. Arguments of defined functions are different in the caller and in
       the callee, and
   1c. Values may be cloned without need.

   2. the new scheme (with # define NEW_CLONE) clones Values late, so that:
   2a. Different PointerCells (of the same or even of different Values)
       may point to the same Sub-Value, and
   2b. Arguments of defined functions remain the same as long as they are
       not modified (aka. COW (Copy On Write)), and
   2c. A Value is only cloned before it is being modified and only if it
       has more than one owner.

   !!! NOTE that Scheme2 was reported to fail in certain cases of
            selective specification.
 **/
#define NEW_CLONE

#ifdef NEW_CLONE   /* new clone() scheme */

/// clone, given a Value_P. Result is a Value_P.
#  define CLONE_P(B_P, L)   (B_P)

/// clone, given a const cValue *, const Value *, or Value *. Result is a Value_P.
#  define CLONE(pB, L)      Value_P(const_cast<Value *>(static_cast<const Value *>(pB)), L)

#else   /* old clone() scheme */

/// clone, given a Value_P. Result is a Value_P.
#  define CLONE_P(B_P, L)   (B_P).get()->clone(L)

/// clone, given a Value *. Result is a Value_P.
#  define CLONE(pB, L)      (pB)->clone(L)

#endif

   /// return the min spacing (S of lrm p. 138) for column \b col and
   /// set \b non_char_col if the column contains a numeric or nested item.
   /// @param non_char_col set to true if a non-character item is found in
   ///        the column (not the NOTCHAR function of lrm p. 138, see NOTCHAR())
   /// @param col column index to examine (0-based)
   /// @param framed true if the value is being printed in a framed box
   int32_t get_col_spacing(bool & non_char_col, ShapeItem col,
                           bool framed) const;

   /// list a value
   /// @param out output stream to write to
   /// @param show_owners true to include owner-count information
   ostream & list_one(ostream & out, bool show_owners) const;

   /// return the total CDR size (header + data + padding) for \b this value.
   /// @param cdr_type CDR encoding type selector
   ShapeItem total_CDR_size_brutto(CDR_type cdr_type) const
      { return (total_CDR_size_netto(cdr_type) + 15) & ~15; }

   /// return the total CDR size in bytes (header + data),
   /// not including any padding for \b this value.
   /// @param cdr_type CDR encoding type selector
   ShapeItem total_CDR_size_netto(CDR_type cdr_type) const;

   /// return the CDR size in bytes for the data of \b value,
   /// not including the CDR header and padding
   /// @param cdr_type CDR encoding type selector
   ShapeItem CDR_data_size(CDR_type cdr_type) const;

   /// return the CDR type for \b this value
   CDR_type get_CDR_type() const;

   /// return the ravel of \b this value as UCS string, or throw DOMAIN error
   /// if the ravel contains non-char or nested cells.
   UCS_string get_UCS_ravel() const;

   /// print address, shape, and flags of this value
   /// @param out output stream to write to
   /// @param indent indentation level in spaces
   /// @param idx ravel index shown for nested context
   void print_structure(ostream & out, int indent, ShapeItem idx) const;

   /// return the current flags flat-encoded into a single integer (bit 0
   /// complete, bit 1 marked, bit 2 temp, bit 3 member) -- used only for
   /// diagnostics and the workspace archive format (Archive.cc's "flg="
   /// attribute), both of which need a flat integer rather than a
   /// VF_Flags bitfield; testing a Value's LIVE flags should instead use
   /// is_complete()/is_marked()/is_member() (i.e. direct VF_Flags member
   /// access), not this encoding. left_value is deliberately NOT included
   /// here: it is a purely transient, intra-statement marker on a
   /// selective-assignment cellrefs array (raw LvalCells, meaningless
   /// outside this process) that never becomes a named symbol's stored
   /// value, so it has no business in a persisted workspace.
   uint32_t get_flags() const
      { return (flags.complete ? 0x01 : 0) |
               (flags.marked   ? 0x02 : 0) |
               (flags.temp     ? 0x04 : 0) |
               (flags.member   ? 0x08 : 0); }

   /// print info related to a stale value
   /// @param out output stream to write to
   /// @param dob dynamic object associated with the stale value
   void print_stale_info(ostream & out, const DynamicObject * dob) const;

   /// check the cells of \b this value, return the number of errors.
   /// @param out output stream for diagnostic messages
   int check_Cells(ostream & out) const;

   /// return a deep copy of \b this value
   /// @param loc caller location for diagnostics
   Value_P clone(const char * loc) const;

   /// return the number of Value_P pointing to \b this value.
   inline int get_owner_count() const;

   /// return the PointerCell count
   ShapeItem get_pointer_cell_count() const
      { return pointer_cell_count; }

   /// Apply dyadic packed fast path: dispatches on A's ravel type (this).
   /// Returns true (result written into Z) if a fast path ran. A and B
   /// are read-only; only Z is mutated.
   bool apply_fast_dyadic(const ScalarFunction & sf,
                           cValue_R B, int inc_A, int inc_B,
                           Value & Z, ShapeItem len_Z) const
      { return ravel.apply_fast_dyadic(sf, *this, inc_A, B, inc_B, Z, len_Z); }

   /// Apply monadic packed fast path: dispatches on B's ravel type (this).
   /// Returns true (result written into Z) if a fast path ran. B is
   /// read-only; only Z is mutated.
   bool apply_fast_monadic(const ScalarFunction & sf,
                            Value & Z, ShapeItem len_Z) const
      { return ravel.apply_fast_monadic(sf, *this, Z, len_Z); }

protected:
   /// check that this left-value is consistent.
   void check_lval_consistency() const;
};

//════════════════════════════════════════════════════════════════════════════
/**
    An APL value. It consists of a fixed header (rank, shape) and
    and a ravel (a sequence of cells). If the ravel is short, then it
    is contained in the value itself; otherwise the value uses a pointer
    to a loner ravel.
 */
/// An APL Value (essentially a Shape and a ravel)
class Value : public DynamicObject, public cValue
{
   friend class cValue;
   friend class Value_P;
   friend class Value_P_Base;
   friend class PointerCell;   // needs & for &cell_owner
   friend class PJob_scalar_AB;
   friend class PJob_scalar_B;

protected:
   // constructors. Values shall not be constructed directly but only via
   // their counterparts in class Value_P
   //
   /// constructor: scalar value (i.e. a value with rank 0).
   /// @param loc caller location for diagnostics
   Value(const char * loc);

   /// constructor: a scalar value (i.e. a value with rank 0) from a cell
   /// @param cell the single ravel cell for the scalar
   /// @param loc caller location for diagnostics
   Value(const Cell & cell, const char * loc);

   /// constructor: a true vector (i.e. a value with rank 1) with len items
   /// @param len number of elements in the vector
   /// @param loc caller location for diagnostics
   Value(ShapeItem len, const char * loc);

   /// constructor: a matrix (i.e. a value with rank 2)
   /// @param rows number of rows
   /// @param cols number of columns
   /// @param loc caller location for diagnostics
   Value(ShapeItem rows, ShapeItem cols, const char * loc);

   /// constructor: a general array with shape \b sh
   /// @param sh shape of the new array
   /// @param loc caller location for diagnostics
   Value(const Shape & sh, const char * loc);

   /// constructor: a packed array with shape \b sh. The caller has allocated
   /// the ravel (bits)
   /// @param sh shape of the new array
   /// @param bits caller-allocated packed bit ravel
   /// @param loc caller location for diagnostics
   Value(const Shape & sh, uint64_t * bits, const char * loc);

   /// constructor: a simple character vector from a UCS string.
   /// Rank is always 1, so that is_char_vector() will be true.
   /// @param ucs source Unicode string
   /// @param loc caller location for diagnostics
   Value(const UCS_string & ucs, const char * loc);

   /// constructor: a simple character vector from a UTF8 string
   /// @param utf source UTF-8 string
   /// @param loc caller location for diagnostics
   Value(const UTF8_string & utf, const char * loc);

   /// constructor: a simple character vector from a CDR string
   /// @param cdr source CDR-encoded string
   /// @param loc caller location for diagnostics
   Value(const CDR_string & cdr, const char * loc);

   /// constructor: a character matrix from a PrintBuffer
   /// @param pb print buffer providing the character rows
   /// @param loc caller location for diagnostics
   Value(const PrintBuffer & pb, const char * loc);

   /// constructor: a integer vector containing the items of a shape
   /// @param loc caller location for diagnostics
   /// @param sh shape whose items are copied into the ravel
   Value(const char * loc, const Shape * sh);

public:
   /// destructor
   virtual ~Value();

   /// compile-time default; runtime threshold is Quad_SYL::pack_min_length
   enum { PACKED_MINIMUM_LENGHT = cfg_PACKED_MINIMUM_LENGTH_WANTED };

   // DynamicObject also has print(ostream&) const; select the cValue one
   // for unqualified ->print() calls.  DynamicObject::print() remains
   // reachable via explicit static_cast in print_properties().
   using cValue::print;

   /// set the length of axis \b r to \b sh.
   /// @param r axis index to update (0-based)
   /// @param sh new length for that axis
   void set_shape_item(sAxis r, ShapeItem sh)
      { shape.set_shape_item(r, sh); }

   /** reshape this value in place. This is generally dangerous and only
       permitted if:

       1. \b this value has been initialized completely, and
       2. the new shape has not more items than \b this value.

       If \b this value is empty, then (due to its prototype) reshaping
       it to volume 1 is permitted.

       The caller is responsible for ensuring that \b this value does not have
       multiple owners.
   **/
   /// @param new_shape replacement shape (must not exceed current volume)
   void set_shape(const Shape & new_shape)
      { Assert(new_shape.get_volume() <= nz_element_count() && is_complete());
        shape = new_shape; }

   /// return a value containing pointers to all ravel cells of this value.
   /// the result is non-nested and has the same shape as \b this
   /// @param loc caller location for diagnostics
   Value_P get_cellrefs(const char * loc);

   /// mark \b this (an lvalue returned by Symbol::resolve_lv(), i.e. the
   /// entire, still unmodified, current value of \b sym) so that
   /// Bif_F12_PICK::eval_AB() can recognize an empty-left-argument Pick
   /// applied directly to it (LRM: "Pick with an empty left argument ...
   /// causes the whole array associated with the assigned name to be
   /// replaced") and Value::assign_cellrefs() can special-case it as a
   /// plain Symbol::assign() instead of a shape-conforming per-cell copy.
   /// Every other lvalue-producing function returns a freshly built Value
   /// that never carries this marker, so it only ever survives an
   /// unbroken chain of empty-Pick applications straight through to the
   /// assignment -- any intervening function drops it automatically.
   /// @param sym the symbol whose entire current value \b this represents
   void set_lval_whole_symbol(Symbol * sym)
      { lval_whole_symbol = sym; }

   /// see set_lval_whole_symbol()
   Symbol * get_lval_whole_symbol() const
      { return lval_whole_symbol; }

   /// mark \b this (the cellrefs of an already-nested item, e.g. from
   /// (2⊃A)←X where A[2] is itself nested) as Bif_F12_PICK's DIRECT,
   /// unbroken selection of that one item, so that Value::assign_cellrefs()
   /// can special-case it as a wholesale replacement of the PointerCell at
   /// \b slot (in \b owner) -- neither IBM APL2 nor Dyalog assign INTO an
   /// already-nested picked item (Blake McBride, Bugs28 #30); the shape of
   /// the OLD item must not constrain the new value at all.
   ///
   /// \b this still has the OLD item's own shape (needed so that an outer
   /// selector, e.g. (1 0/2⊃A)←X, can narrow it in the ordinary way), but
   /// that outer selector always builds a fresh Value for its own (narrowed)
   /// result -- exactly like lval_whole_symbol above, this marker is never
   /// copied into such a fresh Value, so it only survives an unbroken chain
   /// of pass-throughs straight to the assignment.
   /// @param slot the PointerCell being replaced (a ravel cell of \b owner)
   /// @param owner the value owning \b slot
   void set_lval_pick_slot(Cell * slot, Value * owner)
      { lval_pick_slot = slot;   lval_pick_owner = owner; }

   /// see set_lval_pick_slot()
   Cell * get_lval_pick_slot() const    { return lval_pick_slot; }

   /// see set_lval_pick_slot()
   Value * get_lval_pick_owner() const  { return lval_pick_owner; }

   /// mark \b this as a BARE member reference (e.g. S.a, with no
   /// selecting function applied) built for a possible selective
   /// specification/assignment -- so that Value::assign_cellrefs() can
   /// reject it with SYNTAX ERROR if it survives unbroken all the way to
   /// the assignment, i.e. was never actually selected from (Bugs30
   /// #14). Unlike set_lval_whole_symbol()/set_lval_pick_slot() (which
   /// mark a legitimate "replace the whole thing" target), a bare member
   /// reference used directly as a selective-specification target --
   /// with nothing to its left to select with -- looks exactly like a
   /// broken/incomplete selective specification, and member variables
   /// are a GNU-APL-only construct free to reject that outright rather
   /// than special-case it into a wholesale replace the way a plain
   /// symbol's (X)←B already legitimately does.
   ///
   /// Exactly like the other two markers, this is never copied into a
   /// freshly built Value, so it only survives an unbroken chain of
   /// pass-throughs straight to the assignment -- any intervening
   /// selecting function (e.g. (⌽S.a)←B, (2↑S.a)←B) builds its own fresh
   /// result Value that does not carry it, and is unaffected.
   void set_lval_bare_member_ref()
      { lval_bare_member_ref = true; }

   /// see set_lval_bare_member_ref()
   bool get_lval_bare_member_ref() const
      { return lval_bare_member_ref; }

   /// clear all three "unbroken pass-through" lvalue markers above
   /// (lval_whole_symbol, lval_pick_slot/owner, lval_bare_member_ref).
   /// Needed by an identity fast path (e.g. scalar ⌽, 0⌽/0⊖, identity
   /// ⍉, ⍬↑[⍬]/⍬↓[⍬], ,[n]) that returns its argument ALIASED (CLONE()
   /// under the active NEW_CLONE scheme is not a real copy, just a new
   /// Value_P handle on the SAME Value object) instead of building a
   /// genuinely fresh Value the way every other selecting function is
   /// documented to -- these markers are only ever meant to survive an
   /// UNBROKEN chain straight from their origin (Symbol::resolve_lv(),
   /// Bif_F12_PICK::pick(), Prefix.cc's member-access reduction) through
   /// to Value::assign_cellrefs(); a real function application in
   /// between, even an identity one, must break that chain the same way
   /// a genuinely fresh Value automatically would. Bugs28 #52 fixed this
   /// ad-hoc for one marker/one call site (elided-index A[]); Bugs31
   /// #19/#23 generalizes it to every aliasing identity fast path and to
   /// all three markers -- harmless to call on a Value that never had
   /// any of them set (the common, non-lvalue-context case).
   void clear_lval_markers()
      {
        lval_whole_symbol = 0;
        lval_pick_slot = 0;
        lval_pick_owner = 0;
        lval_bare_member_ref = false;
      }

   /// assign \b val to the cell references in this value.
   /// @param val value whose elements are assigned to the cell references
   void assign_cellrefs(Value_P val);

   /// overwrite every LvalCell in this value's ravel with a harmless
   /// IntCell 0, in place (shape and cell count unchanged). Used by
   /// assign_cellrefs() right before it throws LENGTH_ERROR for a
   /// malformed selective specification/assignment (Bugs30 #28): the
   /// errored SI frame's Prefix keeps this lvalue-cellref array alive on
   /// its (unpopped) operand stack until the next )SIC, and nothing will
   /// ever consume it for its original purpose now that the assignment
   /// has definitively failed -- left as LvalCells, it is both a latent
   /// dangling-pointer risk (each one is a raw, non-owning pointer into
   /// another value's ravel, which could in principle be freed before
   /// this array is) and a false-positive for )CHECK's "value has N Lval
   /// Cells" diagnostic (cValue::check_Cells()).
   void neutralize_lval_cells(const char * loc);

   /** return member of this value, defined by \b members. The first name in
       members is the deepest, while the last name is the name of the
       variable containing the members (and is only used in error printouts).
   **/
   /// @param members path of member names from deepest to outermost
   /// @param owner set to the value that owns the returned cell
   /// @param throw_error if true, throw an error when member is not found
   Cell * get_member(const vector<const UCS_string *> & members,
                     Value * & owner, bool throw_error);

   /// return the Cell (if any) containing the data of structured value member
   /// \b member, or 0 if member not found.
   /// @param member name of the structured member to look up
   Cell * get_member_data(const UCS_string & member);

   // unhide the const overload from cValue (hidden by the non-const above)
   using cValue::get_member_data;

   /// create a new member and return the Cell containing the data for it
   /// @param new_member name of the member to create
   Cell * get_new_member(const UCS_string & new_member);

   /// double the ravel length of \b this value (by appending integer 0s).
   /// @param loc caller location for diagnostics
   void double_ravel(const char * loc);

   /// return the (writable) idx'th element of the ravel.
   /// @param idx ravel index (0-based)
   Cell & get_wravel(ShapeItem idx)
      { Assert1(idx < nz_element_count());   return ravel.get_wravel(idx); }

   /// return the first element of the ravel (which is always present)
   /// Same as get_wproto() and get_wscalar(), but named differently
   /// to indicate its context.
   Cell & get_wfirst()
      { return ravel.get_wfirst(); }

   /// return the first element of the ravel of a scalar value.
   /// Same as get_wfirst(), but named differently to indicate its context.
   Cell & get_wscalar()
      { return ravel.get_wscalar(); }

   /// return the first element of the ravel (which is always present)
   /// same as wfirst(), but named differently to indicate its context.
   Cell & get_wproto()
      { return ravel.get_wproto(); }

   /// finalize a ravel that was written directly as int64_t* via get_wfirst().
   /// Call after writing N int64_t values into
   ///   reinterpret_cast<int64_t *>(&get_wfirst()).
   void commit_ravel_Int64(ShapeItem n)
      { Assert(ravel.valid_ravel_items == 0);
        // Install IntRavel vtable only for heap ravels: Cell() called by Ravel(upgrade_tag{})
        // writes vtable pointers into short_value[], which aliases the packed int64 storage
        // for short ravels (ravel.cells == ravel.short_value). Heap ravels are unaffected.
        if (ravel.cells != ravel.short_value)
           new (&ravel) IntRavel();
        ravel.valid_ravel_items = n;
        ravel.fetcher    = &Ravel::int64_fetcher;
        flags.ravel_type = RPT_INT64; }

   /// finalize a ravel that was written directly as double* via get_wfirst().
   /// Call after writing N double values into
   ///   reinterpret_cast<double *>(&get_wfirst()).
   void commit_ravel_Float64(ShapeItem n)
      { Assert(ravel.valid_ravel_items == 0);
        if (ravel.cells != ravel.short_value)
           new (&ravel) FloatRavel();
        ravel.valid_ravel_items = n;
        ravel.fetcher    = &Ravel::float64_fetcher;
        flags.ravel_type = RPT_FLOAT64; }

   /// finalize Z as a bit-packed BOOL ravel after writing uint64_t chunks via
   ///   reinterpret_cast<uint64_t *>(&get_wfirst()).
   void commit_ravel_Bool(ShapeItem n)
      { Assert(ravel.valid_ravel_items == 0);
        if (ravel.cells != ravel.short_value)
           new (&ravel) BoolRavel();
        ravel.valid_ravel_items = n;
        ravel.fetcher    = &Ravel::bool_fetcher;
        flags.ravel_type = RPT_BOOL; }

   /// finalize Z as a packed complex ravel after writing N double pairs via
   ///   reinterpret_cast<double *>(&get_wfirst()).
   void commit_ravel_Complex(ShapeItem n)
      { Assert(ravel.valid_ravel_items == 0);
        if (ravel.cells != ravel.short_value)
           new (&ravel) ComplexRavel();
        ravel.valid_ravel_items = n;
        ravel.fetcher    = &Ravel::complex_fetcher;
        flags.ravel_type = RPT_COMPLEX; }

   /// finalize Z as the same packed type as B (for permutation functions)
   void commit_ravel_like(const cValue & B, ShapeItem n);

   /// pack Z using B's ravel_type directly, without scanning Z[0] (for permutations)
   void pack_like(const cValue & B);

   /// return the number of Value_P pointing to \b this value
   int get_owner_count() const
      { return owner_count; }

   /// return the number of (so far) initialized items
   ShapeItem get_valid_item_count()
      { return ravel.valid_ravel_items; }

   /// return the current ravel cell to be initialized (excluding prototype)
   Cell * current_ravel()
      { return more() ? ravel.cells + ravel.valid_ravel_items : 0; }

   /// set the prototype (according to B) if this value is empty.
   /// @param B value whose prototype is copied
   /// @param loc caller location for diagnostics
   inline void set_default(const Value & B, const char * loc);

   /// set the prototype (according to B) if this value is empty (const cValue variant).
   /// @param B value whose prototype is copied
   /// @param loc caller location for diagnostics
   inline void set_default(const cValue & B, const char * loc);

   /// set the prototype (according to B) if this value is empty.
   /// @param cB cell whose type determines the prototype
   /// @param loc caller location for diagnostics
   inline void set_default(const Cell & cB, const char * loc);

   /// set the prototype to ' ' if this value is empty.
   inline void set_proto_Spc();

   /// set the prototype to 0 if this value is empty.
   inline void set_proto_Int();

   /// release ravel Cell z
   /// @param offset index of the ravel cell to release
   /// @param loc caller location for diagnostics
   inline void release(ShapeItem offset, const char * loc);

   /// initialize the next ravel cell with a character value
   /// @param u Unicode character to store
   inline void next_ravel_Char(Unicode u);

   /// initialize the next ravel cell with a floating point value
   /// @param f floating-point value to store
   inline void next_ravel_Float(APL_Float f);

#ifdef cfg_RATIONAL_NUMBERS_WANTED
   /// initialize the next ravel cell with a floating point value
   /// @param numer numerator of the rational number
   /// @param denom denominator of the rational number
   inline void next_ravel_Float(APL_Integer numer, APL_Integer denom);

   /// initialize the next ravel cell with a floating point value, converting
   /// it to integer if possible
   /// @param numer numerator of the rational number
   /// @param denom denominator of the rational number
   inline void next_ravel_Number(APL_Integer numer, APL_Integer denom)
      {
         if      (denom == 1)    next_ravel_Int(numer);
         else if (denom == -1 && uint64_t(numer) != 0x8000000000000000)
                                 next_ravel_Int(-numer);
         else                    next_ravel_Float(numer, denom);
      }
#endif // cfg_RATIONAL_NUMBERS_WANTED

   /// initialize the next ravel cell with a floating point value, converting
   /// it to integer if possible
   /// @param f floating-point value to store (converted to int if near-int)
   inline void next_ravel_Number(APL_Float f);

   /// initialize the next ravel cell with a complex value if needed,
   ///  or with (near-real) floating point value if possible.
   /// @param real real part of the number
   /// @param imag imaginary part of the number
   inline void next_ravel_Number(APL_Float real, APL_Float imag);

   /// initialize the next ravel cell with a complex value if needed,
   ///  or with (near-real) floating point value if possible.
   /// @param cpx complex value to store
   inline void next_ravel_Number(APL_Complex cpx);

   /// initialize the next ravel cell from another Cell
   /// @param other source cell to copy
   inline void next_ravel_Cell(const Cell & other);

   /// initialize the next ravel cell from the type of another Cell
   /// @param other cell whose type determines the prototype cell
   inline void next_ravel_Proto(const Cell & other);

   /// initialize the next ravel cell with a complex value
   /// @param cpx complex value to store
   inline void next_ravel_Complex(APL_Complex cpx);

   /// initialize the next ravel cell with a complex value
   /// @param real real part of the complex value
   /// @param imag imaginary part of the complex value
   inline void next_ravel_Complex(APL_Float real, APL_Float imag);

   /// initialize the next ravel cell with an integer value
   /// @param i integer value to store
   inline void next_ravel_Int(APL_Integer i);

   /// initialize the next ravel cell with integer 0
   inline void next_ravel_0();

   /// initialize the next ravel cell with integer 1
   inline void next_ravel_1();

   /// initialize the next ravel cell with a pointer to another Cell
   /// @param target pointer to the target cell being referenced
   /// @param target_owner value that owns the target cell
   inline void next_ravel_Lval(Cell * target, Value * target_owner);

   /// initialize the next ravel cell with an APL sub-value
   /// @param val pointer to the nested sub-value
   inline void next_ravel_Pointer(Value * val);

   /// initialize the next ravel cell with an APL sub-value
   /// @param val pointer to the nested sub-value
   /// @param magic magic number tag stored alongside the pointer
   inline void next_ravel_Pointer(Value * val, uint32_t magic);

   /// initialize the next ravel cell from either a APL scalar (if val is one)
   /// or from a non-scalar (producing as PointerCell
   /// @param val pointer to the APL value to store
   inline void next_ravel_Value(Value * val);

   /// set ravel[offset] to \b uni
   /// @param offset ravel index (0-based)
   /// @param uni Unicode character to store
   inline void set_ravel_Char(ShapeItem offset, Unicode uni);

   /// set ravel[offset] to \b aint
   /// @param offset ravel index (0-based)
   /// @param aint integer value to store
   inline void set_ravel_Int(ShapeItem offset, APL_Integer aint);

   /// set ravel[offset] to \b flt
   /// @param offset ravel index (0-based)
   /// @param flt floating-point value to store
   inline void set_ravel_Float(ShapeItem offset, APL_Float flt);

   /// set ravel[offset] to \b Complex(real, imag)
   /// @param offset ravel index (0-based)
   /// @param real real part of the complex value
   /// @param imag imaginary part of the complex value
   inline void set_ravel_Complex(ShapeItem offset, APL_Float real,
                                                   APL_Float imag);

   /// set ravel[offset] to \b cell.
   /// @param offset ravel index (0-based)
   /// @param cell source cell to copy
   inline void set_ravel_Cell(ShapeItem offset, const Cell & cell);

   /// set ravel[offset] to b \b PointerCell(val), where val ≠ simple scalar.
   /// @param offset ravel index (0-based)
   /// @param val pointer to the nested sub-value
   inline void set_ravel_Pointer(ShapeItem offset, Value * val);

   /// set ravel[offset] to \b value
   /// @param offset ravel index (0-based)
   /// @param value pointer to the APL value to store
   inline void set_ravel_Value(ShapeItem offset, Value * value);

   /// update the depth cache when ravel[offset] is about to be overwritten.
   /// new_sub_depth: -1 = new cell is simple; ≥ 0 = new is PointerCell to a
   /// sub-value of that depth.  Call BEFORE the actual write.
   inline void depth_update_for_overwrite(ShapeItem offset, int new_sub_depth);

   /// returen \b true if \b sub == \b val or sub is contained in \b val
   /// @param val outer value to search within
   /// @param sub candidate sub-value to look for
   static bool is_or_contains(const cValue * val, const cValue * sub);

   /// initialize value related variables and print some statistics.
   static void init();

   /// add a member to a structured variable
   /// @param member_name name of the new member
   /// @param member_value value to associate with the member
   void add_member(const UCS_string & member_name, Value * member_value);

   /// mark all values, except static values
   static void mark_all_dynamic_values();

   /// rollback initialization of this value
   /// @param items number of ravel items to roll back
   /// @param loc caller location for diagnostics
   void rollback(ShapeItem items, const char * loc);

   /// check \b that this value was completely initialized, and set
   /// VF_complete if so.
   /// @param loc caller location for diagnostics
   void check_value(const char * loc);

   /// If this value is a single axis between ⎕IO and ⎕IO + max_axis then
   /// return that axis. Otherwise throw AXIS_ERROR.
   /// @param val value expected to contain a single axis number
   /// @param max_axis exclusive upper bound for valid axis values
   /// @param where arity prefix (e.g. "A⌽[X]B") for the )MORE error text
   static sRank get_single_axis(const cValue * val, sRank max_axis,
                                const char * where);

   /// convert the ravel of Value \b val to a shape (normalized to ⎕IO←0)
   /// An elided index, for example B[], throws an INDEX_ERROR.
   /// @param val value whose ravel is interpreted as a shape vector
   static Shape to_shape(const cValue * val);

   /// glue two values.
   /// @param token_A token holding the left value to glue
   /// @param token_B token holding the right value to glue
   /// @param loc caller location for diagnostics
   static Value_P glue(const Token & token_A, const Token & token_B,
                       const char * loc);

   /// erase stale values
   /// @param loc caller location for diagnostics
   static int erase_stale(const char * loc);

   /// erase all values (clean-up after )CLEAR)
   /// @param out output stream for diagnostic messages
   static void erase_all(ostream & out);

   /// list all values
   /// @param out output stream to write to
   /// @param show_owners true to include owner-count information
   static ostream & list_all(ostream & out, bool show_owners);

   /// recursively replace all numeric ravel elements with 0 and all
   /// characters with blank. See lrm p. 46.
   /// @param force_numeric if true, prototype is forced to numeric even for char arrays
   void to_type(bool force_numeric);

   /// expand this (packed) ravel to Cell format (in place). Valid for all
   /// packed ravels.
   void explode_to_Cells();

   /// expand this UNICODE16 ravel to Cell format (in place). Only valid
   /// for packed UNICODE16 ravels. Called before appending a uint32_t..
   inline RavelType explode_to_UNICODE32();

   /// expand this ravel to packed complex format (in place). Only valid
   /// for packed bool, int, and double ravels. Called before appending
   /// a complex<double>.
   inline RavelType explode_to_COMPLEX();

   /// expand this ravel to packed double format (in place). Only valid
   /// for packed bool and int ravels. Called before appending a double.
   inline RavelType explode_to_FLOAT64();

   /// expand this ravel to packed int64_t format (in place). Only valid
   /// for packed bool ravels. Called before appending an int64_t.
   inline RavelType explode_to_INT64();

   /// assign cell C to packed or unpacked ravel at offset; explodes only when
   /// the cell type is incompatible with the current packing
   void assign_cell(ShapeItem offset, const Cell & C, const char * loc);

   /// try to implode (pack) this unpacked value. Return 0 on success or
   /// reason on error;
   const char * try_implode();

   /// try to pack this value into the tightest homogeneous ravel format
   /// (RPT_BOOL, RPT_INT64, RPT_FLOAT64, RPT_COMPLEX, RPT_UNICODE16, or RPT_UNICODE32).
   /// Does nothing if the value is already packed, has PointerCells, or has
   /// fewer than PACKED_MINIMUM_LENGHT elements.  Safe to call multiple times.
   void try_pack(bool force = false);

   /// upgrade an already RPT_UNICODE16-packed, heap-allocated ravel to the
   /// MemberNameRavel vtable (see its class comment in Ravel.hh). Intended
   /// to be called right after try_pack(true) when constructing a
   /// structured-variable member name (Value::get_new_member()). A no-op
   /// for anything not RPT_UNICODE16, and for short (short_value-backed)
   /// ravels -- those keep the base Ravel vtable, same as every other
   /// packed type, so get_char_value() is not disabled for a short name.
   void upgrade_member_name()
      { if (get_ravel_type() != RPT_UNICODE16)   return;
        if (ravel.cells == ravel.short_value)    return;
        new (&ravel) MemberNameRavel(); }

   /// print incomplete Values, and return the number of incomplete Values.
   /// @param out output stream to write to
   static int print_incomplete(ostream & out);

   /// print stale Values, and return the number of stale Values.
   /// @param out output stream to write to
   static int print_stale(ostream & out);

   /// check the cells of all values, return the number of bad Values.
   /// @param out output stream for diagnostic messages
   static int check_all_Cells(ostream & out);

   /// total nz_element_counts of all non-short values
   static uint64_t total_ravel_count;

   /// the number of values created
   static uint64_t value_count;

   /// a "checksum" to detect deleted values
   const void * check_ptr;

   /// increment the PointerCell count
   void increment_pointer_cell_count()
      { ++pointer_cell_count; }

   /// decrement the PointerCell count
   void decrement_pointer_cell_count()
      { --pointer_cell_count; }

   /// increase \b nz_subcell_count by \b count
   /// @param count number of sub-cells to add
   void add_subcount(ShapeItem count)
      { ravel.nz_subcell_count += count; }

   /// increment the number of (smart-) pointers to this value
   /// @param loc caller location for diagnostics
   void increment_owner_count(const char * loc)
      {
        const char * cp_this = charP(this);
        Assert1(cp_this);
        Assert1(check_ptr == (cp_this + 7));
        ++owner_count;
      }

   /// decrement the number of (smart-) pointers to this value and delete
   /// this value if no more pointers exist
   /// @param loc caller location for diagnostics
   void decrement_owner_count(const char * loc)
      {
        const char * cp_this = charP(this);
        Assert1(cp_this);
        Assert1(check_ptr == (cp_this + 7));
        Assert1(owner_count > 0);

        // NOTE: the destructor (triggered by 'delete this' below) will
        // check 'check_ptr' and then set 'check_ptr' = 0. We therefore
        // don't clear 'check_ptr' here.
        //
        if (--owner_count == 0)   delete this;   // mo more owners
      }

   /// check if WS is FULL after allocating value with \b cell_count items
   /// @param args description of the allocation site for error messages
   /// @param cell_count number of cells being allocated
   /// @param loc caller location for diagnostics
   static bool check_WS_FULL(const char * args, ShapeItem cell_count,
                             const char * loc);

   /// handler for catch(Error) in init_ravel() (never called)
   /// @param error the caught Error object
   /// @param args description of the allocation site
   /// @param loc caller location for diagnostics
   static void catch_Error(const Error & error, const char * args,
                           const char * loc);

   /// handler for catch(exception) in init_ravel() (never called)
   /// @param ex the caught standard exception
   /// @param args description of the allocation site
   /// @param caller description of the calling function
   /// @param loc caller location for diagnostics
   static void catch_exception(const exception & ex, const char * args,
                        const char * caller,  const char * loc);

   /// handler for catch(...) in init_ravel() (never called)
   /// @param args description of the allocation site
   /// @param caller description of the calling function
   /// @param loc caller location for diagnostics
   static void catch_ANY(const char * args, const char * caller,
                         const char * loc);

   /// the number of fast (recycled) new() calls
   static uint64_t fast_new_count;

   /// the number of slow (malloc() based) new() calls
   static uint64_t slow_new_count;

protected:
   /// glue items A and B; both non-const to increase their owner count.
   /// @param item_A left item value to glue
   /// @param item_B right item value to glue
   /// @param loc caller location for diagnostics
   static Value_P glue_item_item(Value & item_A, Value & item_B,
                                 const char * loc);

   /// glue item A and strand B; A non-const to increase its owner count.
   /// @param item_A left item value to glue
   /// @param strand_B right strand value to glue
   /// @param loc caller location for diagnostics
   static Value_P glue_item_strand(Value & item_A, const Value & strand_B,
                                   const char * loc);

   /// glue strand A and item B; B non-const to increase its owner count.
   /// @param strand_A left strand value to glue
   /// @param item_B right item value to glue
   /// @param loc caller location for diagnostics
   static Value_P glue_strand_item(const Value & strand_A, Value & item_B,
                                   const char * loc);

   /// glue strands A and B
   /// @param strand_A left strand value to glue
   /// @param strand_B right strand value to glue
   /// @param loc caller location for diagnostics
   static Value_P glue_strand_strand(const Value & strand_A,
                                     const Value & strand_B, const char * loc);

   /// return the next ravel cell to be initialized (excluding prototype)
   Cell * next_ravel()
      { return more() ? ravel.cells + ravel.valid_ravel_items++ : 0; }

   /// init the ravel of an APL value, return the ravel length
   inline void init_ravel();

   /// number of Value_P objects pointing to this value
   int owner_count;

   /// see set_lval_whole_symbol(); 0 for every Value except a top-level
   /// Symbol::resolve_lv() result that has not (yet) been narrowed by any
   /// other lvalue-producing function.
   Symbol * lval_whole_symbol = 0;

   /// see set_lval_pick_slot(); 0 unless this is Bif_F12_PICK's direct,
   /// unbroken selection of an already-nested item.
   Cell * lval_pick_slot = 0;

   /// see set_lval_pick_slot()
   Value * lval_pick_owner = 0;

   /// see set_lval_bare_member_ref(); false unless this is a bare member
   /// reference (e.g. S.a) built for a possible selective specification
   /// that has not (yet) been narrowed by any selecting function.
   bool lval_bare_member_ref = false;

   /// a linked list of values that have been deleted
   static _deleted_value * deleted_values;

   /// number values that have been deleted
   static int deleted_values_count;

   /// the size of the next allocation
   static uint64_t alloc_size;

   /// max. number values that have been deleted
   enum { deleted_values_MAX = 10000 };

#if 1 // enable/disable deleted values chain for faster memory allocation

   /// allocate space for a new Value. For performance reasons, a pool of
   /// deleted_values_MAX is kept and Value objects in that pool are reused
   /// before calling new().
   static void * operator new(size_t sz)
      {
        if (deleted_values)   // we have deleted values: recycle one
           {
             --deleted_values_count;
             void * ret = deleted_values;
             deleted_values = deleted_values->next;
             ++fast_new_count;
             return ret;
           }

        ++slow_new_count;
        return ::operator new(sz);
      }

   /// free space for a new Value
   static void operator delete(void * ptr)
      {
        if (deleted_values_count < deleted_values_MAX)   // we have space
           {
             ++deleted_values_count;
             reinterpret_cast<_deleted_value *>(ptr)->next = deleted_values;
             deleted_values = reinterpret_cast<_deleted_value *>(ptr);
           }
        else                                             // no more space
           {
             // operator new() above (the slow-path branch) allocates via
             // ::operator new(), so the matching release is ::operator
             // delete(), not free() -- pairing them is UB (only benign as
             // long as nobody replaces the global operator new, which this
             // project itself does elsewhere, and as long as the allocator
             // doesn't keep separate books for the two families -- ASan,
             // tcmalloc's debug mode etc. all do). Blake McBride, Bugs15
             // #2: only reached once the recycle pool (deleted_values_MAX
             // entries) is full, so it never fires in the ordinary
             // testcase suite but does under any real workload that frees
             // a large nested value in one go.
             //
             ::operator delete(ptr);
           }
      }

#endif

   /// explicit cast from Value & to Value *. Use with care
   Value * get_pointer()   { return this; }

private:
   /// prevent new[] of Value
   static void * operator new[](size_t sz) = delete;

   /// prevent delete[] of Value
   static void operator delete[](void* ptr) = delete;

   /// limit the use of & (which is frequently a mistake)
   Value * operator &()   { return this; }
};
//════════════════════════════════════════════════════════════════════════════

// shortcuts for frequently used APL values...

/// integer scalar
/// @param val integer value for the scalar
/// @param loc caller location for diagnostics
Value_P IntScalar(APL_Integer val, const char * loc);

/// floating-point scalar
/// @param val floating-point value for the scalar
/// @param loc caller location for diagnostics
Value_P FloatScalar(APL_Float val, const char * loc);

/// character scalar
/// @param uni Unicode character for the scalar
/// @param loc caller location for diagnostics
Value_P CharScalar(Unicode uni, const char * loc);

/// complex scalars (from APL_Complex)
/// @param cpx complex value for the scalar
/// @param loc caller location for diagnostics
Value_P ComplexScalar(APL_Complex cpx, const char * loc);

/// complex scalars (from real and imag parts)
/// @param real real part of the complex scalar
/// @param imag imaginary part of the complex scalar
/// @param loc caller location for diagnostics
Value_P ComplexScalar(APL_Float real, APL_Float imag, const char * loc);

/// ⍳0 (aka. ⍬)
/// @param loc caller location for diagnostics
Value_P Idx0(const char * loc);

/// ''
/// @param loc caller location for diagnostics
Value_P Str0(const char * loc);

/// 0 0⍴''
/// @param loc caller location for diagnostics
Value_P Str0_0(const char * loc);

/// 0 0⍴0
/// @param loc caller location for diagnostics
Value_P Idx0_0(const char * loc);

/// empty struct
/// @param loc caller location for diagnostics
Value_P EmptyStruct(const char * loc);

//────────────────────────────────────────────────────────────────────────────

// NOTE: there exist cross-dependencies between Value.hh and Value_P.hh on
// one hand and Value.icc and Value_P.icc on the other. It is therefore
// important that the declarations in both .hh files (i.e. this file and
// Value_P.hh occur before any of the .icc files.
//
#include "Value_P.hh"

#include "Value.icc"
#include "Value_P.icc"

#endif // __VALUE_HH_DEFINED__
