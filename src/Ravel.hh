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

#ifndef __RAVEL_HH_DEFINED__
#define __RAVEL_HH_DEFINED__

#include "CharCell.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "ScalarOps.hh"

class ScalarFunction;
class ValueBase;
class cValue;
typedef const cValue & cValue_R;
class Value;

//════════════════════════════════════════════════════════════════════════════
/// The ravel of an APL value: the flat array of Cell objects together with
/// the short-value inline buffer, the fill/fetch function pointer, and the
/// initialization bookkeeping counters.  Factored out of ValueBase so that
/// a future class Ravel can own and manage the storage independently.
class Ravel
{
   friend class ValueBase;
   friend class cValue;
   friend class Value;

public:
   /// Tag for vtable-only upgrade via placement-new.
   /// IntRavel()/FloatRavel() constructors use this to leave all data
   /// members untouched while installing the subclass vtable.
   struct upgrade_tag {};

   Ravel()
   : fetcher(0),
     valid_ravel_items(0),
     nz_subcell_count(0),
     alloc_cells(0),
     cells(0)
   {}

   /// No-op constructor used by subclass placement-new upgrades.
   /// Preserves every data member; only the vtable pointer changes.
   explicit Ravel(upgrade_tag) {}

   //══════════════════════════════════════════════════════════════════════════
   // Read-only (const) methods
   //══════════════════════════════════════════════════════════════════════════

   /// Indexed scalar accessors — return native C++ values without
   /// materialising a temporary Cell that outlives the call.  The base
   /// class falls back through fetch_transient() (which does materialise
   /// into a call-local Cell for a packed ravel) so short packed ravels
   /// (which keep the base Ravel vtable) and RPT_CELLS ravels work
   /// correctly.  IntRavel, BoolRavel, FloatRavel, ComplexRavel,
   /// Char16Ravel and Char32Ravel each override every one of these with
   /// a direct read of the raw packed array (no Cell, no cache, no
   /// aliasing hazard) -- prefer calling these over
   /// fetch_transient(idx).xxx(), which always pays for a Cell
   /// materialisation even where a packed-type override could have
   /// avoided it entirely. A mismatched accessor for the ravel's real
   /// type raises DOMAIN_ERROR, same as the equivalent call on the
   /// underlying Cell subclass would.

   virtual APL_Integer get_int_value(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_int_value(); }

   virtual APL_Integer get_near_int(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_near_int(); }

   virtual bool get_near_bool(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_near_bool(); }

   virtual Unicode get_char_value(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_char_value(); }

   virtual APL_Float get_real_value(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_real_value(); }

   virtual APL_Float get_imag_value(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_imag_value(); }

   virtual int get_byte_value(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_byte_value(); }

   virtual APL_Complex get_complex_value(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_complex_value(); }

   virtual CellType get_cell_type(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_cell_type(); }

   virtual CellType get_cell_subtype(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_cell_subtype(); }

   virtual Value_P get_pointer_value(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).get_pointer_value(); }

   virtual bool is_pointer_cell(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_pointer_cell(); }

   /// like get_pointer_value(idx), but returns an empty (null) Value_P
   /// instead of throwing DOMAIN_ERROR when the cell at idx is not a
   /// pointer cell -- see Cell::try_pointer_value(). Non-virtual: composes
   /// the two already-virtual calls above, so packed-ravel subclasses
   /// (whose is_pointer_cell(idx) override already returns false in O(1)
   /// with no Cell materialisation) get the same zero-cost short-circuit
   /// here for free, without needing their own override.
   Value_P try_pointer_value(ShapeItem idx) const
      { return is_pointer_cell(idx) ? get_pointer_value(idx) : Value_P(); }

   virtual bool is_lval_cell(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_lval_cell(); }

   virtual bool is_simple_cell(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_simple_cell(); }

   virtual bool is_character_cell(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_character_cell(); }

   virtual bool is_integer_cell(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_integer_cell(); }

   virtual bool is_numeric(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_numeric(); }

   virtual bool is_complex_cell(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_complex_cell(); }

   virtual bool is_near_int(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_near_int(); }

   virtual bool is_near_bool(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_near_bool(); }

   virtual bool is_near_real(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_near_real(); }
   virtual bool is_real_cell(ShapeItem idx) const
      { Cell cache; return fetch_transient(idx, cache).is_real_cell(); }

protected:
   /// fetch the idx'th cell of the ravel into a call-local \b cache
   /// (no bounds check). Only for the base-class fallback accessors
   /// above, whose result is consumed and discarded within the same
   /// statement -- never store the returned reference beyond that.
   /// @param idx ravel index (0-based)
   /// @param cache caller-owned Cell to materialise a packed element into
   const Cell & fetch_transient(ShapeItem idx, Cell & cache) const
      {
        const Cell & result = fetcher(idx, cells, cache);

        // every fetcher except cell_fetcher() (whose cache parameter is
        // intentionally unused -- an unpacked ravel already holds real
        // Cells, cells[offset] IS the cell) must materialise into, and
        // return a reference to, *this* cache -- not some other object
        // (e.g. a shared static). A fetcher that silently violates this
        // is exactly Blake McBride's Bugs12.md #1 (bool_fetcher used to
        // return IntCell::boolean_TRUE/FALSE instead of writing cache).
        //
        Assert(fetcher == &Ravel::cell_fetcher || &result == &cache);
        return result;
      }
public:

   /// fetch function for unpacked (Cell-array) ravels.
   /// @param offset ravel index (0-based)
   /// @param cells pointer to the Cell array
   /// @param cache unused (required by fetcher signature)
   static const Cell & cell_fetcher(ShapeItem offset, const Cell * cells,
                                    Cell & not_used)
      { return cells[offset]; }

   /// return bit \b idx of a bool-packed ravel. The ravel is written and
   /// read (e.g. by the ScalarFunction fast paths) as uint64_t words, bit b
   /// being bit b&63 of word b>>6. Accessing it bytewise instead (bit b&7 of
   /// byte b>>3) gives the same result on little-endian machines only.
   static APL_Integer get_packed_bit(const Cell * cells, ShapeItem idx)
      { return (reinterpret_cast<const uint64_t *>(cells)[idx >> 6]
                >> (idx & 63)) & 1; }

   /// fetch function for packed (bit-array) boolean ravels.
   /// @param offset ravel index (0-based)
   /// @param cells pointer to the packed bit array cast to Cell *
   /// @param cache receives the materialised IntCell(0 or 1)
   static const Cell & bool_fetcher(ShapeItem offset, const Cell * cells,
                                    Cell & cache)
      {
        new (&cache) IntCell(get_packed_bit(cells, offset));
        return cache;
      }

   /// fetch function for packed int64_t ravels.
   static const Cell & int64_fetcher(ShapeItem offset, const Cell * cells,
                                     Cell & cache)
      { new (&cache) IntCell(reinterpret_cast<const int64_t *>(cells)[offset]);
        return cache;
      }

   /// fetch function for packed double ravels.
   static const Cell & float64_fetcher(ShapeItem offset, const Cell * cells,
                                       Cell & cache)
      {
        new (&cache) FloatCell(reinterpret_cast<const double *>
                                               (cells)[offset]);
        return cache;
      }

   /// fetch function for packed complex (2×double) ravels.
   static const Cell & complex_fetcher(ShapeItem offset, const Cell * cells,
                                       Cell & cache)
      {
        const double * p = reinterpret_cast<const double *>(cells) + 2*offset;
        new (&cache) ComplexCell(p[0], p[1]);
        return cache;
      }

   /// fetch function for packed 16-bit Unicode ravels.
   static const Cell & char16_fetcher(ShapeItem offset, const Cell * cells,
                                      Cell & cache)
      {
        new (&cache) CharCell(Unicode(reinterpret_cast<const uint16_t *>
                                                      (cells)[offset]));
        return cache;
      }

   /// fetch function for packed 32-bit Unicode ravels.
   static const Cell & char32_fetcher(ShapeItem offset, const Cell * cells,
                                      Cell & cache)
      {
        new (&cache) CharCell(reinterpret_cast<const Unicode *>
                                              (cells)[offset]);
        return cache;
      }

   //══════════════════════════════════════════════════════════════════════════
   // Read-write methods
   //══════════════════════════════════════════════════════════════════════════

   /// return the idx'th cell of the ravel (no bounds check).
   /// @param idx ravel index (0-based)
   Cell & get_wravel(ShapeItem idx)   { return cells[idx]; }

   /// return the first ravel cell (for non-empty ravels).
   Cell & get_wfirst()    { return get_wravel(0); }

   /// return the single ravel cell of a scalar.
   Cell & get_wscalar()   { return get_wravel(0); }

   /// return the first ravel cell (the prototype of an empty value).
   Cell & get_wproto()    { return get_wravel(0); }

   // ── Packed fast-path dispatch ─────────────────────────────────────────────

   /// Apply dyadic packed fast path if A's ravel type allows it.
   /// Returns true (and writes result into Z) if a fast path ran.
   /// Returns false when cell-by-cell fallback is needed.
   virtual bool apply_fast_dyadic(const ScalarFunction & sf,
                                   cValue_R A, int inc_A,
                                   cValue_R B, int inc_B,
                                   Value & Z, ShapeItem len_Z) const;

   /// Apply monadic packed fast path if B's ravel type allows it.
   virtual bool apply_fast_monadic(const ScalarFunction & sf,
                                    cValue_R B,
                                    Value & Z, ShapeItem len_Z) const;

protected:
   //══════════════════════════════════════════════════════════════════════════
   // Data members
   //══════════════════════════════════════════════════════════════════════════

   /// function that fetches one cell; differs for packed vs. unpacked ravels.
   const Cell & (*fetcher)(ShapeItem offset, const Cell * cells, Cell & cache);

   /// number of initialized cells in the ravel (excluding prototype).
   ShapeItem valid_ravel_items;

   /// number of cells in nested sub-values.
   ShapeItem nz_subcell_count;

   /// the number of Cells actually passed to std::allocator<Cell>::allocate()
   /// for the current heap buffer (0 if cells == short_value, i.e. no heap
   /// buffer at all). Distinct from valid_ravel_items/nz_element_count():
   /// several primitives allocate a worst-case ravel and then shrink the
   /// *logical* shape/count in place (an explicitly supported operation,
   /// see Value::set_shape_item()) without ever touching the underlying
   /// allocation. std::allocator<Cell>::deallocate() requires the exact
   /// size originally passed to allocate() -- unlike new[]/delete[], it has
   /// no self-tracking size cookie -- so ~Value() must use this field, not
   /// nz_element_count(), when freeing ravel.cells. Set in init_ravel() and
   /// kept current by double_ravel().
   ShapeItem alloc_cells;

   /// pointer to the ravel cells (points to short_value for short ravels,
   /// or to a heap-allocated array for longer ones).
   Cell * cells;

   /// inline storage for short (≤ cfg_SHORT_VALUE_LENGTH_WANTED) ravels.
   Cell short_value[cfg_SHORT_VALUE_LENGTH_WANTED];
};
//════════════════════════════════════════════════════════════════════════════
/// A Ravel whose storage holds a packed int64_t array (RPT_INT64).
/// Installed via placement-new only for heap ravels (ravel.cells != ravel.short_value).
/// Short ravels (N == cfg_SHORT_VALUE_LENGTH_WANTED) stay as base Ravel
/// because Ravel(upgrade_tag{}) calls Cell() on short_value[], which aliases
/// the packed storage. Base Ravel::apply_fast_dyadic/monadic handle
/// RPT_INT64 for those short ravels.
class IntRavel : public Ravel
{
public:
   IntRavel() : Ravel(upgrade_tag{}) {}

   virtual APL_Integer get_int_value(ShapeItem idx) const override
      { return reinterpret_cast<const int64_t *>(cells)[idx]; }

   virtual APL_Integer get_near_int(ShapeItem idx) const override
      { return reinterpret_cast<const int64_t *>(cells)[idx]; }

   virtual bool get_near_bool(ShapeItem idx) const override
      { const int64_t v = reinterpret_cast<const int64_t *>(cells)[idx];
        if (v == 0)   return false;
        if (v == 1)   return true;
        DOMAIN_ERROR; }

   virtual APL_Float get_real_value(ShapeItem idx) const override
      { return APL_Float(reinterpret_cast<const int64_t *>(cells)[idx]); }

   virtual APL_Float get_imag_value(ShapeItem idx) const override
      { return 0.0; }

   virtual int get_byte_value(ShapeItem idx) const override
      { // must match IntCell::get_byte_value()'s -128..255 range (the
        // unpacked/Cell-array path this packed one substitutes for), not
        // just 0..255 -- otherwise the same value (e.g. -1) is accepted
        // or rejected purely depending on whether the array happened to
        // pack (>= pack_min_length elements). ⎕FIO byte write of -1
        // failed at >= 12 elements while working below that threshold.
        const int64_t v = reinterpret_cast<const int64_t *>(cells)[idx];
        if (v >= -128 && v <= 255)   return int(v);
        DOMAIN_ERROR; }

   virtual APL_Complex get_complex_value(ShapeItem idx) const override
      { return APL_Complex(reinterpret_cast<const int64_t *>(cells)[idx], 0.0); }

   virtual CellType get_cell_type(ShapeItem idx) const override
      { return CT_INT; }

   virtual bool is_pointer_cell(ShapeItem idx) const override { return false; }
   virtual bool is_lval_cell(ShapeItem idx) const override    { return false; }
   virtual bool is_simple_cell(ShapeItem idx) const override  { return true; }
   virtual bool is_character_cell(ShapeItem idx) const override { return false; }
   virtual bool is_integer_cell(ShapeItem idx) const override { return true; }
   virtual bool is_numeric(ShapeItem idx) const override      { return true; }
   virtual bool is_complex_cell(ShapeItem idx) const override { return false; }
   virtual bool is_near_int(ShapeItem idx) const override     { return true; }
   virtual bool is_near_bool(ShapeItem idx) const override
      { const int64_t v = reinterpret_cast<const int64_t *>(cells)[idx];
        return v == 0 || v == 1; }
   virtual bool is_near_real(ShapeItem idx) const override    { return true; }
   virtual bool is_real_cell(ShapeItem idx) const override    { return true; }

   virtual bool apply_fast_dyadic(const ScalarFunction & sf,
                                   cValue_R A, int inc_A,
                                   cValue_R B, int inc_B,
                                   Value & Z, ShapeItem len_Z) const override;

   virtual bool apply_fast_monadic(const ScalarFunction & sf,
                                    cValue_R B,
                                    Value & Z, ShapeItem len_Z) const override;
};
//════════════════════════════════════════════════════════════════════════════
/// A Ravel whose storage holds a packed double array (RPT_FLOAT64).
/// Same placement-new guard as IntRavel: only installed for heap ravels.
class FloatRavel : public Ravel
{
public:
   FloatRavel() : Ravel(upgrade_tag{}) {}

   virtual APL_Float get_real_value(ShapeItem idx) const override
      { return reinterpret_cast<const double *>(cells)[idx]; }

   virtual APL_Float get_imag_value(ShapeItem idx) const override
      { return 0.0; }

   virtual APL_Complex get_complex_value(ShapeItem idx) const override
      { return APL_Complex(reinterpret_cast<const double *>(cells)[idx], 0.0); }

   virtual CellType get_cell_type(ShapeItem idx) const override
      { return CT_FLOAT; }

   virtual bool is_pointer_cell(ShapeItem idx) const override { return false; }
   virtual bool is_lval_cell(ShapeItem idx) const override    { return false; }
   virtual bool is_simple_cell(ShapeItem idx) const override  { return true; }
   virtual bool is_character_cell(ShapeItem idx) const override { return false; }
   virtual bool is_integer_cell(ShapeItem idx) const override { return false; }
   virtual bool is_numeric(ShapeItem idx) const override      { return true; }
   virtual bool is_complex_cell(ShapeItem idx) const override { return false; }
   virtual bool is_near_int(ShapeItem idx) const override
      { return Cell::is_near_int(reinterpret_cast<const double *>(cells)[idx]); }
   virtual bool is_near_bool(ShapeItem idx) const override
      { const APL_Float v = reinterpret_cast<const double *>(cells)[idx];
        return Cell::is_near_zero(v)
            || (v >= (1.0 - INTEGER_TOLERANCE) && v < (1.0 + INTEGER_TOLERANCE)); }
   virtual bool is_near_real(ShapeItem idx) const override    { return true; }
   virtual bool is_real_cell(ShapeItem idx) const override    { return true; }

   virtual bool apply_fast_dyadic(const ScalarFunction & sf,
                                   cValue_R A, int inc_A,
                                   cValue_R B, int inc_B,
                                   Value & Z, ShapeItem len_Z) const override;

   virtual bool apply_fast_monadic(const ScalarFunction & sf,
                                    cValue_R B,
                                    Value & Z, ShapeItem len_Z) const override;
};
//════════════════════════════════════════════════════════════════════════════
/// A Ravel whose storage holds a packed uint16_t array (RPT_UNICODE16).
/// Same placement-new guard: only installed for heap ravels.
class Char16Ravel : public Ravel
{
public:
   Char16Ravel() : Ravel(upgrade_tag{}) {}

   virtual Unicode get_char_value(ShapeItem idx) const override
      { return Unicode(reinterpret_cast<const uint16_t *>(cells)[idx]); }

   virtual CellType get_cell_type(ShapeItem idx) const override
      { return CT_CHAR; }

   virtual bool is_pointer_cell(ShapeItem idx) const override  { return false; }
   virtual bool is_lval_cell(ShapeItem idx) const override     { return false; }
   virtual bool is_simple_cell(ShapeItem idx) const override   { return true; }
   virtual bool is_character_cell(ShapeItem idx) const override { return true; }
   virtual bool is_integer_cell(ShapeItem idx) const override  { return false; }
   virtual bool is_numeric(ShapeItem idx) const override       { return false; }
   virtual bool is_complex_cell(ShapeItem idx) const override  { return false; }
   virtual bool is_near_int(ShapeItem idx) const override      { return false; }
   virtual bool is_near_bool(ShapeItem idx) const override     { return false; }
   virtual bool is_near_real(ShapeItem idx) const override     { return false; }
   virtual bool is_real_cell(ShapeItem idx) const override     { return false; }

   virtual bool apply_fast_dyadic(const ScalarFunction & sf,
                                   cValue_R A, int inc_A,
                                   cValue_R B, int inc_B,
                                   Value & Z, ShapeItem len_Z) const override;

   virtual bool apply_fast_monadic(const ScalarFunction & sf,
                                    cValue_R B,
                                    Value & Z, ShapeItem len_Z) const override;
};
//════════════════════════════════════════════════════════════════════════════
/// A Ravel for structured-variable member names (see Value::get_new_member()
/// and cValue::is_structured()). Member names are read-only after creation,
/// always short plain character strings (never 4-byte Unicode), and only
/// ever compared as a whole -- never indexed character-wise. Reuses
/// Char16Ravel's packed 16-bit storage and fetcher unchanged (so generic
/// printing/display/iteration via get_cravel() keeps working), but
/// disables the single-character accessor: any code that forgets a member
/// name isn't a general indexable string and tries per-character access
/// anyway hits this instead of silently working. (cValue::equal_string()'s
/// RPT_UNICODE16 fast path is the intended way to compare a member name;
/// it never calls get_char_value().)
/// Same placement-new guard as every other packed-type Ravel subclass:
/// only installed for heap ravels (see Value::upgrade_member_name()) --
/// a short member name keeps the base Ravel vtable, same as any other
/// short packed value, so get_char_value() is not disabled for those.
class MemberNameRavel : public Char16Ravel
{
public:
   MemberNameRavel() : Char16Ravel() {}

   virtual Unicode get_char_value(ShapeItem idx) const override
      { FIXME; }
};
//════════════════════════════════════════════════════════════════════════════
/// A Ravel whose storage holds a packed Unicode array (RPT_UNICODE32).
/// Same placement-new guard: only installed for heap ravels.
class Char32Ravel : public Ravel
{
public:
   Char32Ravel() : Ravel(upgrade_tag{}) {}

   virtual Unicode get_char_value(ShapeItem idx) const override
      { return reinterpret_cast<const Unicode *>(cells)[idx]; }

   virtual CellType get_cell_type(ShapeItem idx) const override
      { return CT_CHAR; }

   virtual bool is_pointer_cell(ShapeItem idx) const override  { return false; }
   virtual bool is_lval_cell(ShapeItem idx) const override     { return false; }
   virtual bool is_simple_cell(ShapeItem idx) const override   { return true; }
   virtual bool is_character_cell(ShapeItem idx) const override { return true; }
   virtual bool is_integer_cell(ShapeItem idx) const override  { return false; }
   virtual bool is_numeric(ShapeItem idx) const override       { return false; }
   virtual bool is_complex_cell(ShapeItem idx) const override  { return false; }
   virtual bool is_near_int(ShapeItem idx) const override      { return false; }
   virtual bool is_near_bool(ShapeItem idx) const override     { return false; }
   virtual bool is_near_real(ShapeItem idx) const override     { return false; }
   virtual bool is_real_cell(ShapeItem idx) const override     { return false; }

   virtual bool apply_fast_dyadic(const ScalarFunction & sf,
                                   cValue_R A, int inc_A,
                                   cValue_R B, int inc_B,
                                   Value & Z, ShapeItem len_Z) const override;

   virtual bool apply_fast_monadic(const ScalarFunction & sf,
                                    cValue_R B,
                                    Value & Z, ShapeItem len_Z) const override;
};
//════════════════════════════════════════════════════════════════════════════
/// A Ravel whose storage holds a bit-packed boolean array (RPT_BOOL).
/// Same placement-new guard: only installed for heap ravels.
class BoolRavel : public Ravel
{
public:
   BoolRavel() : Ravel(upgrade_tag{}) {}

   virtual APL_Integer get_int_value(ShapeItem idx) const override
      { return get_packed_bit(cells, idx); }

   virtual APL_Integer get_near_int(ShapeItem idx) const override
      { return get_int_value(idx); }

   virtual bool get_near_bool(ShapeItem idx) const override
      { return get_packed_bit(cells, idx); }

   virtual APL_Float get_real_value(ShapeItem idx) const override
      { return APL_Float(get_int_value(idx)); }

   virtual APL_Float get_imag_value(ShapeItem idx) const override
      { return 0.0; }

   virtual int get_byte_value(ShapeItem idx) const override
      { return int(get_int_value(idx)); }

   virtual APL_Complex get_complex_value(ShapeItem idx) const override
      { return APL_Complex(APL_Float(get_int_value(idx)), 0.0); }

   virtual CellType get_cell_type(ShapeItem idx) const override
      { return CT_INT; }

   virtual bool is_pointer_cell(ShapeItem idx) const override { return false; }
   virtual bool is_lval_cell(ShapeItem idx) const override    { return false; }
   virtual bool is_simple_cell(ShapeItem idx) const override  { return true; }
   virtual bool is_character_cell(ShapeItem idx) const override { return false; }
   virtual bool is_integer_cell(ShapeItem idx) const override { return true; }
   virtual bool is_numeric(ShapeItem idx) const override      { return true; }
   virtual bool is_complex_cell(ShapeItem idx) const override { return false; }
   virtual bool is_near_int(ShapeItem idx) const override     { return true; }
   virtual bool is_near_bool(ShapeItem idx) const override    { return true; }
   virtual bool is_near_real(ShapeItem idx) const override    { return true; }
   virtual bool is_real_cell(ShapeItem idx) const override    { return true; }

   virtual bool apply_fast_dyadic(const ScalarFunction & sf,
                                   cValue_R A, int inc_A,
                                   cValue_R B, int inc_B,
                                   Value & Z, ShapeItem len_Z) const override;

   virtual bool apply_fast_monadic(const ScalarFunction & sf,
                                    cValue_R B,
                                    Value & Z, ShapeItem len_Z) const override;
};
//════════════════════════════════════════════════════════════════════════════
/// A Ravel whose storage holds a packed complex array (RPT_COMPLEX): pairs of
/// doubles [re, im, re, im, ...].  Same placement-new guard: heap ravels only.
class ComplexRavel : public Ravel
{
public:
   ComplexRavel() : Ravel(upgrade_tag{}) {}

   virtual APL_Float get_real_value(ShapeItem idx) const override
      { return reinterpret_cast<const double *>(cells)[2*idx]; }

   virtual APL_Float get_imag_value(ShapeItem idx) const override
      { return reinterpret_cast<const double *>(cells)[2*idx + 1]; }

   virtual APL_Complex get_complex_value(ShapeItem idx) const override
      { const double * p = reinterpret_cast<const double *>(cells) + 2*idx;
        return APL_Complex(p[0], p[1]); }

   virtual CellType get_cell_type(ShapeItem idx) const override
      { return CT_COMPLEX; }

   virtual bool is_pointer_cell(ShapeItem idx) const override { return false; }
   virtual bool is_lval_cell(ShapeItem idx) const override    { return false; }
   virtual bool is_simple_cell(ShapeItem idx) const override  { return true; }
   virtual bool is_character_cell(ShapeItem idx) const override { return false; }
   virtual bool is_integer_cell(ShapeItem idx) const override { return false; }
   virtual bool is_numeric(ShapeItem idx) const override      { return true; }
   virtual bool is_complex_cell(ShapeItem idx) const override { return true; }
   virtual bool is_near_int(ShapeItem idx) const override
      { const double * p = reinterpret_cast<const double *>(cells) + 2*idx;
        return Cell::is_near_int(p[0]) && Cell::is_near_int(p[1]); }
   virtual bool is_near_bool(ShapeItem idx) const override
      { const double * p = reinterpret_cast<const double *>(cells) + 2*idx;
        return (Cell::is_near_zero(p[0]) || (p[0] >= (1.0 - INTEGER_TOLERANCE)
                                          && p[0] <  (1.0 + INTEGER_TOLERANCE)))
            && Cell::is_near_zero(p[1]); }
   virtual bool is_near_real(ShapeItem idx) const override
      { // fabs(), not I2=p[1]², squared-then-compared (Bugs30 #6
        // sibling, same fix as ComplexCell::is_near_real()): squaring
        // first underflows to 0 for a tiny but comparable-magnitude
        // imag/real pair, wrongly judging it near-real.
        //
        const double * p = reinterpret_cast<const double *>(cells) + 2*idx;
        const APL_Float i = fabs(p[1]);
        return i < REAL_TOLERANCE || i < fabs(p[0])*REAL_TOLERANCE; }
   virtual bool is_real_cell(ShapeItem idx) const override    { return false; }

   virtual bool apply_fast_dyadic(const ScalarFunction & sf,
                                   cValue_R A, int inc_A,
                                   cValue_R B, int inc_B,
                                   Value & Z, ShapeItem len_Z) const override;

   virtual bool apply_fast_monadic(const ScalarFunction & sf,
                                    cValue_R B,
                                    Value & Z, ShapeItem len_Z) const override;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __RAVEL_HH_DEFINED__
