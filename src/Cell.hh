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

#ifndef __CELL_HH_DEFINED__
#define __CELL_HH_DEFINED__

#include <complex>

#include "Common.hh"
#include "ErrorCode.hh"
#include "PrintBuffer.hh"
#include "Value_P.hh"

class CharCell;
class ComplexCell;
class FloatCell;
class IntCell;
class LvalCell;
class PointerCell;

//════════════════════════════════════════════════════════════════════════════
/**
 **   Base class for one item of an APL ravel. The item is one of the following:
 **
 **  A character scalar
 **  An integer scalar
 **  A floating point scalar
 **  A complex number scalar
 **  An APL array
 **
 **   The kind of item is defined by its class, which is derived from Cell.
 **/
/// The base class for all Cells
class Cell
{
public:
   /// Construct an un-initialized Cell
   Cell() {}

   /// A union containing all possible cell values for the different Cell types
   union SomeValue
      {
        Unicode        aval;      ///< for CharCell
        APL_Float_Base cval[2];   ///< for ComplexCell
        ErrorCode      eval;      ///< an error code
        APL_Integer    ival;      ///< for IntCell

        struct _fval              ///< for FloatCell
           {
             /// either a floating point value, or the denominator of a quotient
             union _flt_num
                {
                  APL_Float_Base flt;   ///< the non-rational value
                  APL_Integer    num;   ///< the numerator of the quotient
                } u1;                   ///< primary value, 27 ⎕CR

             /// 0 for non-rational valued, or the denominator of a quotient
             APL_Integer denominator;
           }           fval;        ///< a rational or floating point value
        Cell          *lval;        ///< for LvalCell (selective assignment)

        /// a pointer to, and the owner of, a nested APL (sub-) value
        struct _pval
           {
             Value_P_Base valp;     ///< smart pointer to a value
             Value       *owner;    ///< the value that contains valp
           } pval;                  ///< for PointerCell
      };

   /// store the sum of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_add(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// the inverse of bif_add
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_add_inverse(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the logical and of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_and(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the bitwise and of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_and_bitwise(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// round the value of this Cell up and store the result in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_ceiling(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the binomial (n over k) of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_binomial(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store a circle function (according to A) of \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell (the circle function selector)
   virtual ErrorCode bif_circle_fun(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store a circle function (according to A) of \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell (the circle function selector)
   virtual ErrorCode bif_circle_fun_inverse(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// conjugate the value of this Cell and store the result in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_conjugate(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the direction (sign) of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_direction(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the quotient between A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_divide(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// bitwise comparison of *this and A
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_equal_bitwise(Cell * Z, const Cell * A) const
      { DOMAIN_ERROR; }

   /// store e to the power of the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_exponential(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the factorial (N!) of the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_factorial(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// Round the value of this Cell down and store the result in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_floor(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the base A logarithm of \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell (the base)
   virtual ErrorCode bif_logarithm(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the absolute value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_magnitude(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the larger of A and of \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_maximum(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the smaller of A and of \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_minimum(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the product of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_multiply(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// the inverse of bif_multiply
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_multiply_inverse(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the logical nand of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_nand(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the bitwise nand of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_nand_bitwise(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the base e logarithm of the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_nat_log(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the nearby integer of the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_near_int64_t(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the negative of the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_negative(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the logical complement of the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_not(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the bitwise complement of the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_not_bitwise(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// bitwise comparison of *this and A
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_not_equal_bitwise(Cell * Z, const Cell * A) const
      { DOMAIN_ERROR; }

   /// store the logical NOR of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_nor(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the bitwise NOR of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_nor_bitwise(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the logical OR of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_or(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the bitwise or of A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_or_bitwise(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store pi times the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_pi_times(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store pi times the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_pi_times_inverse(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store A to the power of \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell (the base)
   virtual ErrorCode bif_power(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the reciprocal of the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_reciprocal(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store A modulo \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell (the value to reduce)
   virtual ErrorCode bif_residue(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store a random number between 1 and the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_roll(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// store the difference between A and \b this cell in Z
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   virtual ErrorCode bif_subtract(Cell * Z, const Cell * A) const
      { return E_DOMAIN_ERROR; }

   /// store the nearby integer of the value of this Cell in Z
   /// @param Z  destination cell for the result
   virtual ErrorCode bif_within_quad_CT(Cell * Z) const
      { return E_DOMAIN_ERROR; }

   /// return the minimum number of data bytes to store this cell in
   /// CDR format. The actual number of data bytes can be bigger if other
   /// cells need more bytes,
   virtual int CDR_size() const
      { NeverReach("CDR_size called on Cell base class"); return 0; }

   /// the Quad_CR representation of this cell
   /// @param pctx  print format context
   virtual PrintBuffer character_representation(const PrintContext &pctx) const
      { DOMAIN_ERROR; }

   /// Return the byte value (-128..255 incl) of a cell
   virtual int get_byte_value() const   { DOMAIN_ERROR; }

   /// return the subtype of \b this cell. The subtype is the set of containers
   /// that can store the cell (only for Int and Char cells).
   virtual CellType get_cell_subtype() const
      { return get_cell_type(); }

   /// return the type of \b this cell
   virtual CellType get_cell_type() const
      { return CT_BASE; }

   /// Return value if it is (known to be) close to int, or else Assert()
   virtual APL_Integer get_checked_near_int()  const
      { NeverReach("Value is not an integer"); return 0; }

   /// return the name of the class
   virtual const char * get_classname()  const   { return "Cell"; }

   /// Return the complex value of a cell
   virtual APL_Complex get_complex_value() const   { DOMAIN_ERROR; }

   /// Return the imaginary part of a cell
   virtual APL_Float get_imag_value() const   { DOMAIN_ERROR; }

   /// Return the integer value of a cell
   virtual APL_Integer get_int_value() const   { DOMAIN_ERROR; }

   /// Return the APL value of a cell (Asserts for non-lval cells)
   virtual Cell * get_lval_value() const   { LEFT_SYNTAX_ERROR; }

   /// like get_lval_value(), but returns 0 instead of throwing
   /// LEFT_SYNTAX_ERROR when this cell is not an lval cell -- lets callers
   /// write if (Cell * target = cell.try_lval_value()) { ... } instead of
   /// a separate is_lval_cell() check first. A 0 result is therefore
   /// ambiguous between "not an lval cell" and "a genuine lval cell whose
   /// target is itself 0" (e.g. an invalid LvalCell(0, 0) left behind by
   /// an overtake ↑) -- both cases mean "nothing to write through" to the
   /// caller, so the ambiguity doesn't matter in practice. Non-virtual:
   /// composes the two already-virtual calls above/below, no Cell
   /// subclass needs its own override. Unlike try_pointer_value() (in
   /// Cell.cc), safe to define inline here: Cell * needs no complete type.
   Cell * try_lval_value() const
      { return is_lval_cell() ? get_lval_value() : 0; }

   /// return the Value that owns the cell this points to (PointerCell) or
   /// the cell this refers to (LvalCell); throws DOMAIN_ERROR for any
   /// other cell type.
   virtual Value * get_cell_owner() const   { DOMAIN_ERROR; }

   /// verify this cell's internal invariants (currently meaningful only
   /// for LvalCell -- see LvalCell::check_consistency()); a no-op for
   /// every other cell type.
   virtual void check_consistency() const   {}

   /// make the nested Value this cell points to (if any) uniquely owned
   /// (copy-on-write clone if shared); a no-op for every cell type other
   /// than PointerCell -- see PointerCell::isolate().
   virtual void isolate(const char * loc)   {}

   /// like isolate(), but also isolates every sub-value transitively; a
   /// no-op for every cell type other than PointerCell -- see
   /// PointerCell::isolate_deep().
   virtual void isolate_deep(const char * loc)   {}

   /// Return value if it is close to boolean, or else throw DOMAIN_ERROR
   virtual bool get_near_bool()  const
      { DOMAIN_ERROR; }

   /// Return value if it is close to int, or else throw DOMAIN_ERROR
   virtual APL_Integer get_near_int()  const
      { DOMAIN_ERROR; }

   /// Return the real part of a cell
   virtual APL_Float get_real_value() const   { DOMAIN_ERROR; }

   /// raw pointer to the primary value (for 27 ⎕CR)
   const void * get_u0() const   { return &value.cval[0]; }

   /// raw pointer to the secondary value (for 28 ⎕CR)
   const void * get_u1() const   { return &value.cval[1]; }

   /// init (uninitialized) Cell \b other from \b this initialized cell
   /// @param other  destination cell (uninitialized memory)
   /// @param other_owner  value that owns the destination cell
   /// @param loc  caller location for diagnostics
   virtual void init_other(void * other, Value & other_owner,
                           const char * loc) const
      { Assert(0 && "Cell::init_other() called on base class"); }

   /// Return \b true iff \b this cell is a character cell
   virtual bool is_character_cell() const
      { return false; }

   /// Return \b true iff \b this cell is a complex number cell
   virtual bool is_complex_cell() const
      { return false; }

   /// Return \b true iff \b this cell is an example field character cell
   virtual bool is_example_field() const
      { return false; }

   /// Return \b true unless \b this cell contains infinity or NaN
   virtual bool is_finite() const
      { return true; }

   /// Return \b true iff \b this cell is a floating point cell
   virtual bool is_float_cell() const
      { return false; }

   /// Return \b true iff \b this cell is an integer cell
   virtual bool is_integer_cell() const
      { return false; }

   /// Return \b true iff \b this cell is a pointer to a cell
   virtual bool is_lval_cell() const
      { return false; }

   /// Return true iff this is a PointerCell which points to a member value.
   virtual bool is_member_anchor() const
      { return false; }

   /// return true iff value is numeric and close to 0 or 1
   bool is_near_bool() const
      { return is_near_zero() || is_near_one(); }

   /// True iff value is numeric and close to an int
   virtual bool is_near_int() const
      { return false; }

   /// True iff value is numeric and close to an int
   virtual bool is_near_int64_t() const
      { return false; }

   /// True iff value is near the integer 0 but not 0, e.g. 1E¯11. Such a
   /// value must not be snapped to 0 where a 0 changes the result (e.g.
   /// A!B, A∨B, A∧B; see Bugs31 #15 and Bugs37 #11).
   bool is_snapped_to_0() const
      { return is_near_int() && get_real_value() != 0.0 &&
               nearbyint(get_real_value()) == 0.0; }

   /// return true iff value is numeric and close to 1
   virtual bool is_near_one() const
      { return false; }

   /// return true iff value is numeric and close to a real number
   virtual bool is_near_real() const
      { return false; }

   /// return true iff value is numeric and close to 0
   virtual bool is_near_zero() const
      { return false; }

   /// Return \b true iff \b this cell is an integer, float, or complex cell
   virtual bool is_numeric() const
      { return false; }

   /// Return \b true iff \b this cell is a pointer to a cell from pick()
   virtual bool is_picked_lval_cell() const
      { return false; }

   /// Return \b true iff \b this cell is a pointer (to a sub-array) cell
   virtual bool is_pointer_cell() const
      { return false; }

   /// Return \b true iff \b this cell is an integer or float cell
   virtual bool is_real_cell() const   // int or flt
      { return false; }

   /// Return \b true iff \b this cell is a numeric cor character cell
   bool is_simple_cell() const
      { return (get_cell_type() & CT_SIMPLE) != 0; }

   /// return true if this cell needs scaling (exponential format) in pctx
   /// @param pctx  print format context
   virtual bool need_scaling(const PrintContext &pctx) const
      { return false; }

#ifdef cfg_RATIONAL_NUMBERS_WANTED
   /// return the numerator of a quotient
   virtual APL_Integer get_numerator() const   { FIXME }

   /// return the denominator of a quotient, or 0 for non-quotients
   virtual APL_Integer get_denominator() const { FIXME }
#endif

   /// Release content pointed to (complex, APL value)
   virtual void release(const char * loc) {}

   /// true iff value is close to 0 (within +- qct)
   static bool is_near_zero(APL_Float value)
      { return (value < INTEGER_TOLERANCE) && (value > -INTEGER_TOLERANCE); }

   /// return \b true if z = a - b had an overflow.
   /// @param z  the computed difference
   /// @param a  minuend
   /// @param b  subtrahend
   static bool diff_overflow(APL_Integer z, APL_Integer a, APL_Integer b)
      {
        if (z < 0)   return ((a | ~b) & 0x8000000000000000) == 0;
        else         return ((a & ~b) & 0x8000000000000000) != 0;
      }

   /// return \b true if multiplying a and b overflows int64_t.
   /// @param a  first factor
   /// @param b  second factor
   static bool prod_overflow(int64_t a, int64_t b)
      {
        // exact (Bugs28 #82): this used to compare a double-precision
        // approximation of the product against the (conservative)
        // LARGE_INT/SMALL_INT boundary, roughly 2×10^16 below the
        // actual int64_t limits -- so callers demoted results to float
        // (or, in bif_multiply_ii's own inline copy of this check,
        // FloatCell) for the top ~0.4% of the int64_t range even when
        // the exact product fit perfectly well, e.g.
        // 9223372036854775807×1 gave a float instead of the exact
        // integer. __builtin_mul_overflow computes the exact int64_t
        // product (or reports overflow) directly, in hardware, with no
        // double intermediate and hence no margin needed.
        //
        int64_t prod;
        return __builtin_mul_overflow(a, b, &prod);
      }

   /// return \b true if z = a + b had an overflow.
   /// @param z  the computed sum
   /// @param a  first addend
   /// @param b  second addend
   static bool sum_overflow(APL_Integer z, APL_Integer a, APL_Integer b)
      {
        if (z < 0)   return ((a | b) & 0x8000000000000000) == 0;
        else         return ((a & b) & 0x8000000000000000) != 0;
      }

   /// store 1 in Z if A == the value of \b this cell in Z, else 0
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   ErrorCode bif_equal(Cell * Z, const Cell * A) const;

   /// store 1 in Z if A is >= the value of \b this cell in Z, else 0
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   ErrorCode bif_greater_eq(Cell * Z, const Cell * A) const;

   /// store 1 in Z if A > the value of \b this cell in Z, else 0
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   ErrorCode bif_greater_than(Cell * Z, const Cell * A) const;

   /// store 1 in Z if A <= the value of \b this cell in Z, else 0
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   ErrorCode bif_less_eq(Cell * Z, const Cell * A) const;

   /// store 1 in Z if A < the value of \b this cell in Z, else 0
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   ErrorCode bif_less_than(Cell * Z, const Cell * A) const;

   /// store 1 in Z if A != the value of \b this cell in Z, else 0
   /// @param Z  destination cell for the result
   /// @param A  left operand cell
   ErrorCode bif_not_equal(Cell * Z, const Cell * A) const;

   /// compare this with other, throw DOMAIN ERROR on illegal comparisons
   /// @param other  cell to compare against
   virtual Comp_result compare(const Cell & other) const
      { DOMAIN_ERROR; }

   /// return \b true if \b this cell is equal to \b other
   /// @param other  cell to compare against
   /// @param qct  comparison tolerance (⎕CT)
   virtual bool equal(const Cell & other, double qct) const;

   /// Return the character value of a cell
   virtual Unicode get_char_value() const;

   /// Return the APL value of a cell (Asserts for non-pointer cells)
   virtual Value_P get_pointer_value()  const;

   /// like get_pointer_value(), but returns an empty (null) Value_P
   /// instead of throwing DOMAIN_ERROR when this cell is not a pointer
   /// cell -- lets callers write if (Value_P v = cell.try_pointer_value())
   /// instead of a separate is_pointer_cell() check first. Non-virtual:
   /// composes the two already-virtual calls below, so Cell subclasses
   /// need no override of their own. Defined out-of-line in Cell.cc (not
   /// inline here), same as get_pointer_value() below: Value_P's own
   /// destructor isn't visible yet at this point in Cell.hh, and an
   /// inline body would need it to destroy the empty Value_P temporary.
   Value_P try_pointer_value() const;

   /// Return \b true if \b this cell is greater than \b other, with:
   /// 1. PointerCell > NumericCell > CharCell
   /// 2a. NumericCells sorted by get_real_value(). then by get_imag_value()
   /// 2b. CharCells sorted by get_char_value()
   /// 2c. PointerCells sorted by rank, then by shape, then by ravel
   /// @param other  cell to compare against
   virtual bool greater(const Cell & other) const;

   /// return pointer value of a PointerCell or create a scalar with a
   /// copy of this cell.
   /// @param loc  caller location for diagnostics
   Value_P to_value(const char * loc) const;

   /// init this Cell from value. If value is a scalar then its first element
   /// is used (and value is erased). Otherwise a PointerCell is created.
   /// @param value  APL value to initialize from
   /// @param cell_owner  value that owns this cell
   /// @param loc  caller location for diagnostics
   void init_from_value(Value * value, Value & cell_owner, const char * loc);

   /// placement-construct, at \b dest, a cell of the type of \b other.
   /// Static (unlike init_other(), which this mirrors) because \b dest is
   /// raw ravel storage with no Cell object yet: the previous non-static
   /// init_type(), called as "next_ravel()->init_type(...)"/"get_wproto()
   /// .init_type(...)", called a non-static member function on a glvalue
   /// that did not yet designate an object of the (polymorphic) Cell
   /// type -- UB per [basic.life], UBSan-confirmed (Blake McBride, Bugs15
   /// #12) even though today's non-virtual, immediately-placement-new
   /// implementation happens to generate correct code regardless.
   /// @param dest  target memory address (raw storage, no live Cell yet)
   /// @param other  cell whose type to replicate
   /// @param cell_owner  value that owns this cell
   /// @param loc  caller location for diagnostics
   static void init_type_at(void * dest, const Cell & other,
                            Value & cell_owner, const char * loc);

   /// placement new
   /// @param s  required allocation size (ignored for placement)
   /// @param p  target memory address
   void * operator new(std::size_t s, void * p);

   /// like greater() but static
   /// @param A  left operand cell
   /// @param B  right operand cell
   /// @param unused_comp_arg  unused comparison context
   static bool A_greater_B(const Cell * const & A, const Cell * const & B,
                           const void * unused_comp_arg);

   /// a stable compare function to be used with Heapsort. The cell contents
   /// are ignored and only the cell addresses are being compared. Return
   /// true iff &A > &B.
   static bool compare_ptr(const Cell * const & A, const Cell * const & B,
                          const void * comp_arg);

   /// a stable compare function to be used with Heapsort. Equal cells
   /// are compared by their address. Return true iff A > B or A = B
   /// and &A > &B.
   static bool compare_stable(const Cell * const & A, const Cell * const & B,
                       const void * comp_arg);

   /// copy (deep) count cells from src[idx] to val (val is under construction);
   /// idx is advanced by count
   static void copy(Value & val, const cValue & src, ShapeItem & idx,
                    ShapeItem count);

   /// the name of \b ct
   static const char * get_cell_type_name(CellType ct);

   /// compare cells[a] and cells[b] ascendingly
   static bool greater_cp(const ShapeItem & a, const ShapeItem & b,
                           const void * cells);

   /// like greater_cp(), but exact (not ⎕CT-tolerant) for numeric
   /// cells -- used by ⍋/⍒ (Bif_F12_SORT.cc), which ISO 10.1.2 requires
   /// to grade exactly regardless of ⎕CT. See Bugs27 #30.
   static bool greater_cp_exact(const ShapeItem & a, const ShapeItem & b,
                                const void * cells);

   /// like smaller_cp(), but exact -- see greater_cp_exact() above.
   static bool smaller_cp_exact(const ShapeItem & a, const ShapeItem & b,
                                const void * cells);

   /// compare an exact int64 value \b i against a double \b f WITHOUT
   /// ever converting \b i to double first (which, unlike converting f
   /// to an int64 candidate and back, can silently lose precision for
   /// |i| > 2⋆53). Used at ⎕CT=0 wherever an IntCell is compared
   /// against a FloatCell/ComplexCell (relationals, ⍳, ∊, and the
   /// already-exact ⍋/⍒ grading). See Bugs31 #8/#9.
   /// @param i  exact integer value (left-hand side)
   /// @param f  double value (right-hand side)
   /// @return -1 if i < f, 0 if i == f, 1 if i > f
   static int compare_int_float(APL_Integer i, APL_Float f);

   /// ISO p. 19: A is integral (close to a Gaussian Integer) within qct
   /// @param A  real value to test
   /// @param qct  comparison tolerance (⎕CT)
   static bool integral_within(APL_Float A, double qct);

   /// true iff value is close to an int (within +- qct)
   static bool is_near_int(APL_Float value);

   /// true iff value is close to an int64_t (i.e. fots into an int64_t
   static bool is_near_int64_t(APL_Float value);

   /// return value if it is close to int, or else throw DOMAIN_ERROR
   static APL_Integer near_int(APL_Float value);

   /// ISO p.15: return \b true if A and B are on the same half-plane
   /// @param A  first complex value
   /// @param B  second complex value
   static bool same_half_plane(APL_Complex A, APL_Complex B);

   /// compare cells[a] and cells[b] descendingly
   static bool smaller_cp(const ShapeItem & a, const ShapeItem & b,
                          const void * cells);

   /// return 0-based indices i1, i2, ... iN so that
   /// value[i1] < value[i2] < ... < value[iN].
   /// @param exact  if true, use greater_cp_exact()/smaller_cp_exact()
   ///               (no ⎕CT tolerance) instead of the default,
   ///               ⎕CT-tolerant greater_cp()/smaller_cp() -- ⍋/⍒ need
   ///               this (Bugs27 #30); other callers (e.g. dyadic ⍳'s
   ///               binary-search setup) keep the tolerant default.
   static ErrorCode sorted_indices(vector<ShapeItem> & indices,
                                     const cValue & value, Sort_order order,
                                     ShapeItem comp_len, bool exact = false);

   /// ISO p.19: return \b true if real A is tolerantly equal to real B within C
   /// @param A  first real value
   /// @param B  second real value
   /// @param qct  comparison tolerance (⎕CT)
   static bool tolerantly_equal(APL_Float A, APL_Float B, double qct);

   /// ISO p. 19: return \b true if complex A and B are tolerantly equal
   /// within real qct
   /// @param A  first complex value
   /// @param B  second complex value
   /// @param qct  comparison tolerance (⎕CT)
   static bool tolerantly_equal(APL_Complex A, APL_Complex B, double qct);

protected:
   /// the primary value of \b this cell
   SomeValue value;

private:
   /** Cells that are allocated with new() shall always be contained in
       APL values (using placement new() on the ravel of the value.
       We prevent the accidental use of non-placement new() by deleting it.
    **/
   void * operator new(std::size_t) = delete;

   /** Cells shalle never be overriden with other Cells since they may
       require their desctuctor to be called (e.g. PointerCell)
       We prevent the accidental use of operator =() by deleting it.
    **/

   Cell & operator =(const Cell & other) = delete;
};
//════════════════════════════════════════════════════════════════════════════

typedef ErrorCode (Cell::*prim_f1)(Cell *) const;
typedef ErrorCode (Cell::*prim_f2)(Cell *, const Cell *) const;

#endif // __CELL_HH_DEFINED__
