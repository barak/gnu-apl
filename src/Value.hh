/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2015  Dr. Jürgen Sauermann

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

#ifndef __VALUE_HH_DEFINED__
#define __VALUE_HH_DEFINED__

#include "CharCell.hh"
#include "DynamicObject.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "LvalCell.hh"
#include "PointerCell.hh"
#include "Shape.hh"

using namespace std;

class CDR_string;
class Error;
class IndexExpr;
class PrintBuffer;
class Value_P;
class Thread_context;

/// a linked list of deleted values
struct _deleted_value
{
   /// the next deleted value
  _deleted_value * next;
};

//=============================================================================
/**
    An APL value. It consists of a fixed header (rank, shape) and
    and a ravel (a sequence of cells). If the ravel is short, then it
    is contained in the value itself; otherwise the value uses a pointer
    to a loner ravel.
 */
/// An APL Value (essentially a Shape and a ravel)
class Value : public DynamicObject
{
   friend class Value_P;
   friend class Value_P_Base;
   friend class PointerCell;   // needs & for &cell_owner

protected:
   // constructors. Values should not be constructed directly but via their
   // counterparts in class Value_P.
   //
   /// constructor: scalar value (i.e. a value with rank 0).
   Value(const char * loc);

   /// constructor: a scalar value (i.e. a value with rank 0) from a cell
   Value(const Cell & cell, const char * loc);

   /// constructor: a true vector (i.e. a value with rank 1) with shape \b sh
   Value(ShapeItem sh, const char * loc);

   /// constructor: a general array with shape \b sh
   Value(const Shape & sh, const char * loc);

   /// constructor: a simple character vector from a UCS string
   Value(const UCS_string & ucs, const char * loc);

   /// constructor: a simple character vector from a UTF8 string
   Value(const UTF8_string & utf, const char * loc);

   /// constructor: a simple character vector from a CDR string
   Value(const CDR_string & cdr, const char * loc);

   /// constructor: a character matrix from a PrintBuffer
   Value(const PrintBuffer & pb, const char * loc);

   /// constructor: a integer vector containing the items of a shape 
   Value(const char * loc, const Shape * sh);

public:
   /// destructor
   virtual ~Value();

   /// return \b true iff \b this value is a scalar.
   bool is_scalar() const
      { return shape.get_rank() == 0; }

   /// return \b true iff \b this value is a simple (i.e. depth 0) scalar.
   bool is_simple_scalar() const
      { return is_scalar() &&
              !(get_ravel(0).is_pointer_cell() || get_lval_cellowner()); }

   /// return \b true iff \b this value is empty (some dimension is 0).
   bool is_empty() const
      { return shape.is_empty(); }

   /// return \b true iff \b this value is a numeric scalar.
   bool is_numeric_scalar() const
      { return  is_scalar() && get_ravel(0).is_numeric(); }

   /// return \b true iff \b this value is a character scalar
   bool is_character_scalar() const
      { return  is_scalar() && get_ravel(0).is_character_cell(); }

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

   /// return \b true iff \b this value is a simple character scalar or vector.
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
      { return get_rank() == 0 && get_ravel(0).is_character_cell(); }

   /// return \b true iff \b this value is a simple integer scalar.
   bool is_int_scalar() const
      { return get_rank() == 0 && get_ravel(0).is_near_int(); }

   /// return the number of elements (the product of the shapes).
   ShapeItem element_count() const
      { return shape.get_volume(); }

   /// return the number of elements, but at least 1 (for the prototype).
   ShapeItem nz_element_count() const
      { return shape.get_volume() ? shape.get_volume() : 1; }

   /// return the rank of \b this value
   uRank get_rank() const
     { return shape.get_rank(); }

   /// return the shape of \b this value
   const Shape & get_shape() const
     { return shape; }

   /// return the r'th shape element of \b this value
   ShapeItem get_shape_item(Rank r) const
      { return shape.get_shape_item(r); }

   /// return the length of the last dimension of \b this value
   ShapeItem get_last_shape_item() const
      { return shape.get_last_shape_item(); }

   /// return the length of the last dimension, or 1 for scalars
   ShapeItem get_cols() const
      { return shape.get_cols(); }

   /// return the product of all but the the last dimension, or 1 for scalars
   ShapeItem get_rows() const
      { return shape.get_rows(); }

   /// set the length of dimension \b r to \b sh.
   void set_shape_item(Rank r, ShapeItem sh)
      { shape.set_shape_item(r, sh); }

   /// reshape this value in place. The element count must not increase
   void set_shape(const Shape & sh)
      { Assert(sh.get_volume() <= shape.get_volume());   shape = sh; }

   /// return the position of cell in the ravel of \b this value.
   ShapeItem get_offset(const Cell * cell) const
      { return cell - &get_ravel(0); }

   /// return the next byte after the ravel
   const Cell * get_ravel_end() const
      { return &ravel[nz_element_count()]; }

   /// return the integer of a value that is supposed to have (exactly) one
   APL_Integer get_sole_integer() const
      { if (element_count() == 1)   return get_ravel(0).get_near_int();
        if (get_rank() > 1)   RANK_ERROR;
        else                  LENGTH_ERROR;
      }

   /// return the first integer of a value (the line number of →Value).
   Function_Line get_line_number() const
      { const APL_Integer line(ravel[0].get_near_int());
        Log(LOG_execute_goto)   CERR << "goto line " << line << endl;
        return Function_Line(line); }

   /// return \b true iff \b this value is simple (i.e. not nested).
   bool is_simple() const;

   /// return \b true iff \b this value and its ravel items have rank < 2
   bool is_one_dimensional() const;

   /// return \b true iff \b this value is a simple integer vector.
   bool is_int_vector() const;

   /// return true, if this value has complex cells, false iff it has only
   /// real cells. Throw domain error for other cells (char, nested etc.)
   /// if check_numeric is \b true.
   bool is_complex(bool check_numeric) const;

   /// return true if this value can be compared. This is the case when all
   /// cells (including nested ones) are not complex
   bool can_be_compared() const;

   /// return a value containing pointers to all ravel cells of this value.
   Value_P get_cellrefs(const char * loc);

   /// assign \b val to the cell references in this value.
   void assign_cellrefs(Value_P val);

   /// return the idx'th element of the ravel.
   Cell & get_ravel(ShapeItem idx)
      { Assert1(idx < nz_element_count());   return ravel[idx]; }

   /// return the idx'th element of the ravel.
   const Cell & get_ravel(ShapeItem idx) const
      { Assert1(idx < nz_element_count());   return ravel[idx]; }

   /// set the prototype (according to B) if this value is empty.
   inline void set_default(const Value & B, const char * loc);

   /// set the prototype to ' '
   inline void set_proto_Spc();

   /// set the prototype to ' ' if this value is empty.
   inline void set_default_Spc();

   /// set the prototype to 0 if this value is empty.
   inline void set_default_Int();

   /// Return the number of scalars in this value (enlist).
   ShapeItem get_enlist_count() const;

   /// compute the depth of this value.
   Depth compute_depth() const;

   /// store the scalars in this value into dest...
   void enlist(Cell * & dest, Value & dest_owner, bool left) const;

   /// compute the cell types contained in the top level of \b this value
   CellType flat_cell_types() const;

   /// compute the cell subtypes contained in the top level of \b this value
   CellType flat_cell_subtypes() const;

   /// compute the CellType contained in \b this value (recursively)
   CellType deep_cell_types() const;

   /// recursive set of Cell types in this value
   CellType deep_cell_subtypes() const;

   /// print \b this value (line break at Workspace::get_PW())
   ostream & print(ostream & out) const;

   /// print \b this value (line break at print_width)
   ostream & print1(ostream & out, PrintContext pctx) const;

   /// print the properties (shape, flags etc) of \b this value
   ostream & print_properties(ostream & out, int indent, bool help) const;

   /// debug-print \b this value
   void debug(const char * info) const;

   /// print this value in 4 ⎕CR style
   ostream & print_boxed(ostream & out, const char * info = 0) const;

   /// return \b this indexed by (multi-dimensional) \b IDX.
   Value_P index(const IndexExpr & IDX) const;

   /// return \b this indexed by (one-dimensional) \b IDX.
   Value_P index(Value_P IDX) const;

   /// If this value is a single axis between ⎕IO and ⎕IO + max_axis then
   /// return that axis. Otherwise throw AXIS_ERROR.
   static Rank get_single_axis(const Value * val, Rank max_axis);

   /// convert the ravel of \b val to a shape
   static Shape to_shape(const Value * val);

   /// glue two values.
   static void glue(Token & token, Token & token_A, Token & token_B,
                    const char * loc);

   /// glue strands A and B
   static void glue_strand_strand(Token & result, Value_P A, Value_P B,
                                  const char * loc);

   /// glue strands A and strand B
   static void glue_strand_closed(Token & result, Value_P A, Value_P B,
                                  const char * loc);

   /// glue closed A and closed B
   static void glue_closed_strand(Token & result, Value_P A, Value_P B,
                                  const char * loc);

   /// glue closed A and closed B
   static void glue_closed_closed(Token & result, Value_P A, Value_P B,
                                  const char * loc);

   /// return the number of Value_P pointing to \b this value
   int get_owner_count() const
      { return owner_count; }

   /// return \b true iff this value is an lval (selective assignment)
   /// i.e. return true if at least one leaf value is an lval.
   Value * get_lval_cellowner() const;

   /// return true iff more ravel items (as per shape) need to be initialized.
   /// (the prototype of empty values may still be missing)
   bool more()
      { return valid_ravel_items < element_count(); }

   /// return the next ravel cell to be initialized (excluding prototype)
   Cell * next_ravel()
      { return more() ? &ravel[valid_ravel_items++] : 0; }

   /// initialize the next ravel cell with a character value
   inline void next_ravel_Char(Unicode u);

   /// initialize the next ravel cell with an integer value
   inline void next_ravel_Int(APL_Integer i);

   /// initialize the next ravel cell with an floating-point value
   inline void next_ravel_Float(APL_Float f);

   /// initialize the next ravel cell with an APL sub-value
   inline void next_ravel_Pointer(Value * val);

   /// return the NOTCHAR property of the value. NOTCHAR is false for simple
   /// char arrays and true if any element is numeric or nested. The NOTCHAR
   /// property of empty arrays is the NOTCHAR property of its prototype.
   /// see also lrm p. 138.
   bool NOTCHAR() const;

   /// convert chars to ints and ints to chars (recursively).
   /// return the number of cells that are neither char nor int.
   int toggle_UCS();

   /// return \b true iff \b this value has the same rank as \b other.
   bool same_rank(const Value & other) const
      { return get_rank() == other.get_rank(); }

   /// return \b true iff \b this value has the same shape as \b other.
   bool same_shape(const Value & other) const
      { if (get_rank() != other.get_rank())   return false;
        loop (r, get_rank())
            if (get_shape_item(r) != other.get_shape_item(r))   return false;
        return true;
      }

   /// return \b true iff \b this value has the same shape as \b other or one
   /// of the values is a scalar
   bool scalar_matching_shape(const Value & other) const
      { return is_scalar_extensible()
            || other.is_scalar_extensible()
            || same_shape(other);
      }

   /// returen true if \b sub == \b val or sub is contained in \b val
   static bool is_or_contains(const Value * val, const Value & sub);

   /// print debug info about setting or clearing of flags to CERR
   void flag_info(const char * loc, ValueFlags flag, const char * flag_name,
                  bool set) const;

   /// initialize value related variables and print some statistics.
   static void init();

/// maybe enable LOC for set/clear of flags
#if defined(VF_TRACING_WANTED) || defined(VALUE_HISTORY_WANTED)   // enable LOC
# define _LOC LOC
# define _loc loc
# define _loc_type const char *
#else                                                           // disable LOC
# define _LOC
# define _loc
# define _loc_type
#endif

#ifdef VF_TRACING_WANTED
 # define FLAG_TRACE(f, b) flag_info(loc, VF_ ## f, #f, b);
#else
 # define FLAG_TRACE(_f, _b)
#endif

   /// set the Value flag \b complete
   void SET_complete(_loc_type _loc) const
      { FLAG_TRACE(complete, true)   flags |=  VF_complete;
        ADD_EVENT(this, VHE_SetFlag, VF_complete, _loc); }

   /// true if Value flag \b complete is set
   bool is_complete() const      { return (flags & VF_complete) != 0; }

# define set_complete() SET_complete(_LOC)

   /// set the Value flag \b marked
   void SET_marked(_loc_type _loc) const
      { FLAG_TRACE(marked, true)   flags |=  VF_marked;
        ADD_EVENT(this, VHE_SetFlag, VF_marked, _loc); }

   /// clear the Value flag \b marked
   void CLEAR_marked(_loc_type _loc) const
      { FLAG_TRACE(marked, false)   flags &=  ~VF_marked;
        ADD_EVENT(this, VHE_ClearFlag, VF_marked, _loc); }

   /// true if Value flag \b marked is set
   bool is_marked() const
      { return (flags & VF_marked) != 0; }

# define set_marked()   SET_marked(_LOC)
# define clear_marked() CLEAR_marked(_LOC)

   /// mark all values, except static values
   static void mark_all_dynamic_values();

   /// clear marked flag on this value and its nested sub-values
   void unmark() const;

   /// rollback initialization of this value
   void rollback(ShapeItem items, const char * loc);

   /// the prototype of this value
   Value_P prototype(const char * loc) const;

   /// return a deep copy of \b this value
   Value_P clone(const char * loc) const;

   /// get the min spacing for this column and set/clear if there
   /// is/isn't a numeric item in the column.
   /// are/ain't numeric items in col.
   int32_t get_col_spacing(bool & numeric, ShapeItem col, bool framed) const;

   /// list a value
   ostream & list_one(ostream & out, bool show_owners) const;

   /// check \b that this value is completely initialized, and set complete flag
   void check_value(const char * loc);

   /// return the total CDR size (header + data + padding) for \b this value.
   int total_size_brutto(CDR_type cdr_type) const
      { return (total_size_netto(cdr_type) + 15) & ~15; }

   /// return the total CDR size in bytes (header + data),
   ///  not including any except padding for \b this value.
   int total_size_netto(CDR_type cdr_type) const;

   /// return the CDR size in bytes for the data of \b value,
   /// not including the CDR header and padding
   int data_size(CDR_type cdr_type) const;

   /// return the CDR type for \b this value
   CDR_type get_CDR_type() const;

   /// erase stale values
   static int erase_stale(const char * loc);

   /// re-initialize incomplete values
   static int finish_incomplete(const char * loc);

   /// erase all values (clean-up after )CLEAR)
   static void erase_all(ostream & out);

   /// list all values
   static ostream & list_all(ostream & out, bool show_owners);

   /// return the ravel of \b this value as UCS string, or throw DOMAIN error
   /// if the ravel contains non-char or nested cells.
   UCS_string get_UCS_ravel() const;

   /// recursively replace all ravel elements with 0
   void to_proto();

   /// print address, shape, and flags of this value
   void print_structure(ostream & out, int indent, ShapeItem idx) const;

   /// return the current flags
   ValueFlags get_flags() const   { return ValueFlags(flags); }

   /// print info related to a stale value
   void print_stale_info(ostream & out, const DynamicObject * dob) const;

   /// number of Value_P objects pointing to this value
   int owner_count;

   /// print incomplete Values, and return the number of incomplete Values.
   static int print_incomplete(ostream & out);

   /// print stale Values, and return the number of stale Values.
   static int print_stale(ostream & out);

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

   /// return the PointerCell count
   ShapeItem get_pointer_cell_count() const
      { return pointer_cell_count; }

   /// increase \b nz_subcell_count by \b count
   void add_subcount(ShapeItem count)
      { nz_subcell_count += count; }

      /// increment the number of (smart-) pointers to this value
   void increment_owner_count(const char * loc)
      {
        Assert1(reinterpret_cast<void *>(this) != 0);
        if (check_ptr == charP(this) + 7)
           ++owner_count;
      }

      /// decrement the number of (smart-) pointers to this value and delete
      /// this value if no more pointers exist
      void decrement_owner_count(const char * loc)
         {
           Assert1(reinterpret_cast<void *>(this) != 0);
           if (check_ptr == charP(this) + 7)
              {
                Assert1(owner_count > 0);
                --owner_count;

                if (owner_count == 0) delete this;
              }
         }

   /// check if WS is FULL after allocating value with \b cell_count items
   static bool check_WS_FULL(const char * args, ShapeItem cell_count,
                             const char * loc);

   /// handler for catch(Error) in init_ravel() (never called)
   static void catch_Error(const Error & error, const char * args,
                           const char * loc);

   /// handler for catch(exception) in init_ravel() (never called)
   static void catch_exception(const exception & ex, const char * args,
                        const char * caller,  const char * loc);

   /// handler for catch(...) in init_ravel() (never called)
   static void catch_ANY(const char * args, const char * caller,
                         const char * loc);

   /// the number of fast (recycled) new() calls
   static uint64_t fast_new;

   /// the number of slow (malloc() based) new() calls
   static uint64_t slow_new;

protected:
   /// init the ravel of an APL value, return the ravel length
   inline void init_ravel();

   /// the shape of \b this value (only the first \b rank values are valid.
   Shape shape;

   /// the value that has a PointerCell pointing to \b this value (if any)
   ShapeItem pointer_cell_count;

   /// valueFlags for this value.
   mutable uint16_t flags;

   /// number of initialized cells in the ravel
   ShapeItem valid_ravel_items;

   /// the number of cells in nested sub-values
   ShapeItem nz_subcell_count;

   /// The ravel of \b this value.
   Cell * ravel;

   /// the cells of a short (i.e. ⍴,value ≤ SHORT_VALUE_LENGTH_WANTED) value
   Cell short_value[SHORT_VALUE_LENGTH_WANTED];

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
             ++fast_new;
             return ret;
           }

        ++slow_new;
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
             free(ptr);
           }
      }

#endif

private:
   /// prevent new[] of Value
   static void * operator new[](size_t sz);

   /// prevent delete[] of Value
   static void operator delete[](void* ptr);

   /// restrict use of & (which is frequently a mistake)
   Value * operator &()   { return this; }
};
// ----------------------------------------------------------------------------

extern void print_history(ostream & out, const Value * val, const char * loc);

// shortcuts for frequently used APL values...

/// integer scalar
Value_P IntScalar(APL_Integer val, const char * loc);

/// floating-point scalar
Value_P FloatScalar(APL_Float val, const char * loc);

/// character scalar
Value_P CharScalar(Unicode uni, const char * loc);

/// complex scalar
Value_P ComplexScalar(APL_Complex cpx, const char * loc);

/// ⍳0 (aka. ⍬)
Value_P Idx0(const char * loc);

/// ''
Value_P Str0(const char * loc);

/// 0 0⍴''
Value_P Str0_0(const char * loc);

/// 0 0⍴0
Value_P Idx0_0(const char * loc);

// ----------------------------------------------------------------------------

// NOTE: there exist cross-dependencies between Value.hh and Value_P.hh on
// one hand and Value.icc and Value_P.icc on the other. It is therefore
// important that the declarations in both .hh files (i.e. this file and
// Value_P.hh occur before any of the .icc files.
//
#include "Value_P.hh"

#include "Value.icc"
#include "Value_P.icc"

#endif // __VALUE_HH_DEFINED__

