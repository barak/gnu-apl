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

#ifndef __SCALAR_FUNCTION_HH_DEFINED__
#define __SCALAR_FUNCTION_HH_DEFINED__

#include "Id.hh"
#include "Parallel.hh"
#include "PrimitiveFunction.hh"
#include "ScalarOps.hh"
#include "Thread_context.hh"

#include "Value.hh"

//════════════════════════════════════════════════════════════════════════════
/// Base class for all scalar functions
class ScalarFunction : public PrimitiveFunction
{
   // ScalarFunction is only a helper class whose function shall not be
   // called directly but only via their virtual counterparts in base
   // class Function. We therefore delare all functions protected:
   //
   friend class Ravel;        // apply_fast_dyadic/monadic call get_vv_XXX() lazily
   friend class IntRavel;     // subclass, friendship not inherited in C++
   friend class FloatRavel;   // subclass, friendship not inherited in C++
   friend class Char16Ravel;  // subclass, friendship not inherited in C++
   friend class Char32Ravel;  // subclass, friendship not inherited in C++
   friend class BoolRavel;    // subclass, friendship not inherited in C++
   friend class ComplexRavel; // subclass, friendship not inherited in C++

protected:
   /// Construct a ScalarFunction with \b Id \b id
   /// @param tag token tag identifying this scalar function
   /// @param stat_AB dyadic performance statistics collector (may be null)
   /// @param stat_B monadic performance statistics collector (may be null)
   /// @param thresh_AB element-count threshold for parallel dyadic evaluation
   /// @param thresh_B element-count threshold for parallel monadic evaluation
   ScalarFunction(TokenTag tag, CellFunctionStatistics * stat_AB,
                                CellFunctionStatistics * stat_B,
                                ShapeItem thresh_AB,
                                ShapeItem thresh_B)
   : PrimitiveFunction(tag, stat_AB, stat_B)
   {
     set_dyadic_threshold(thresh_AB);
     set_monadic_threshold(thresh_B);
   }

   /// overloaded Function::eval_fill_AB()
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_fill_AB(cValue_R A, cValue_R B) const
      { return do_eval_fill_AB(A, B); }

   /// overloaded Function::eval_fill_B()
   /// @param B right argument APL value
   virtual Token eval_fill_B(cValue_R B) const
      { return do_eval_fill_B(B); }

   /// overloaded Function::has_result()
   virtual bool has_result() const   { return true; }

   /// overloaded Function::is_scalar_function
   virtual bool is_scalar_function() const   { return true; }

   /// return true if this function can be parallelized
   virtual bool may_parallel() const   { return true; }

   /// A helper function for eval_fill_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   Token do_eval_fill_AB(cValue_R A, cValue_R B) const;

   /// A helper function for eval_fill_B().
   /// @param B right argument APL value
   Token do_eval_fill_B(cValue_R B) const;

   /// compute the dyadic scalar function \b fun along one ravel
   /// @param ec receives error code on failure
   /// @param A left argument APL value
   /// @param B right argument APL value
   /// @param fun cell-level dyadic function to apply
   /// A, B are read-only throughout do_scalar_AB() (only the freshly
   /// allocated result Z is ever written to), so they are taken by
   /// const reference rather than by owning Value_P -- see PJob_scalar_AB
   /// (PJob.hh) for why the worklist built underneath does not need to
   /// own them either.
   Value_P do_scalar_AB(ErrorCode & ec, cValue_R A,
                                        cValue_R B, prim_f2 fun) const;

   /// compute the monadic scalar function \b fun along one ravel
   /// @param ec receives error code on failure
   /// @param B right argument APL value
   /// @param fun cell-level monadic function to apply
   Value_P do_scalar_B(ErrorCode & ec, cValue_R B, prim_f1 fun) const;

   /// Evaluate a scalar function dyadically.
   /// @param A left argument APL value
   /// @param B right argument APL value
   /// @param fun cell-level dyadic function to apply
   Token eval_scalar_AB(cValue_R A, cValue_R B, prim_f2 fun) const;

   /// Evaluate a scalar function dyadically with axis.
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   /// @param fun cell-level dyadic function to apply
   Token eval_scalar_AXB(cValue_R A, cValue_R X, cValue_R B, prim_f2 fun) const;

   /// A helper function for eval_scalar_AXB().
   /// @param A left argument APL value
   /// @param axes_in_X bitmap of axes along which to apply the function
   /// @param B right argument APL value
   /// @param fun cell-level dyadic function to apply
   /// @param reversed true if the axis order should be reversed
   Value_P eval_scalar_AXB(cValue_R A, AxesBitmap axes_in_X,
                           cValue_R B, prim_f2 fun, bool reversed) const;

   /// Evaluate a scalar function monadically.
   /// @param B right argument APL value
   /// @param fun cell-level monadic function to apply
   Token eval_scalar_B(cValue_R B, prim_f1 fun) const;

   /// compute cell_A fun cell_B, scalar-extending PointerCells
   /// @param Z result value to write into
   /// @param cell_A left ravel cell (scalar-extended if PointerCell)
   /// @param cell_B right ravel cell (scalar-extended if PointerCell)
   /// @param fun cell-level dyadic function to apply
   void expand_nested(Value * Z, const Cell & cell_A,
                      const Cell & cell_B, prim_f2 fun) const;

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const = 0;

   /// return the conforming shape of sh_A and sh_B, or set ec (and a
   /// )MORE text on \b where) and return 0 if shapes cannot be conformed.
   /// RANK ERROR or LENGTH ERROR if they dont. Does not throw (safe to
   /// call from a worker thread during parallel evaluation) -- see
   /// ArgCheck::check_conformable(), which does the actual work.
   /// @param ec receives the error code if shapes cannot be conformed
   /// @param where arity-prefix for the )MORE text, e.g. "A+B"
   /// @param sh_A shape of the left argument
   /// @param sh_B shape of the right argument
   static const Shape * conforming_shape(ErrorCode & ec, const char * where,
                                         const Shape & sh_A,
                                         const Shape & sh_B);

   /// Evaluate \b the identity function.
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   /// @param FI0 fill identity cell value
   static Token eval_scalar_identity_fun(cValue_R B, sAxis axis,
                                         const Cell & FI0);

   /// BIG_FLOAT (for eval_scalar_identity_fun)
   static const FloatCell float_max;

   /// -BIG_FLOAT (for eval_scalar_identity_fun)
   static const FloatCell float_min;

   /// 0 (for eval_scalar_identity_fun)
   static const IntCell integer_0;

   /// 1 (for eval_scalar_identity_fun)
   static const IntCell integer_1;

   /// parallel eval_scalar_AB
   static Thread_context::PoolFunction PF_scalar_AB;

   /// parallel eval_scalar_B
   static Thread_context::PoolFunction PF_scalar_B;

   // ── Packed fast-path hooks ──────────────────────────────────────────────
   // Worker function types are defined in ScalarOps.hh.

   /// return dyadic float64→float64 worker, or 0 if not implemented
   virtual vv_f2f_t get_vv_f2f() const { return 0; }

   /// return monadic float64→float64 worker, or 0 if not implemented
   virtual v_f2f_t  get_v_f2f()  const { return 0; }

   /// return dyadic int64→int64 worker, or 0 if not implemented
   virtual vv_i2i_t get_vv_i2i() const { return 0; }

   /// return monadic int64→int64 worker, or 0 if not implemented
   virtual v_i2i_t  get_v_i2i()  const { return 0; }

   /// return dyadic int64→bool worker, or 0 if not implemented
   virtual vv_i2b_t get_vv_i2b() const { return 0; }

   /// return dyadic float64→bool worker, or 0 if not implemented
   virtual vv_f2b_t get_vv_f2b() const { return 0; }

   /// return dyadic unicode16→bool worker, or 0 if not implemented
   virtual vv_c16_2b_t get_vv_c16_2b() const { return 0; }

   /// return dyadic unicode32→bool worker, or 0 if not implemented
   virtual vv_c32_2b_t get_vv_c32_2b() const { return 0; }

   /// return dyadic bit×bit→bit worker, or 0 if not implemented
   virtual vv_b2b_t get_vv_b2b() const { return 0; }

   /// return monadic bit→bit worker, or 0 if not implemented
   virtual v_b2b_t  get_v_b2b()  const { return 0; }

   /// return dyadic complex→complex worker, or 0 if not implemented
   virtual vv_z2z_t get_vv_z2z() const { return 0; }

   /// return monadic complex→complex worker, or 0 if not implemented
   virtual v_z2z_t  get_v_z2z()  const { return 0; }

   /// ISO tolerance comparison (mirrors Cell::tolerantly_equal).
   /// Kept here so DEF_VV_F2B workers are independent of Cell.icc linkage.
   static bool tol_eq(double A, double B, double ct)
      { if (A == B)                   return true;
        if (A < 0.0 && B > 0.0)       return false;
        if (A > 0.0 && B < 0.0)       return false;
        const double ma = A < 0.0 ? -A : A;
        const double mb = B < 0.0 ? -B : B;
        const double mm = ma > mb ? ma : mb;
        const double d  = A > B  ? A - B : B - A;
        return d < ct * mm; }
};

#define PERF_A(x)   TOK_F2_ ## x,                           \
                  & Performance::cfs_F2_    ## x ## _AB, 0, \
                    Performance::thresh_F2_ ## x ## _AB, -1

#define PERF_AB(x)  TOK_F12_ ## x,                    \
                  & Performance::cfs_F12_ ## x ## _AB,    \
                  & Performance::cfs_F12_ ## x ## _B,     \
                    Performance::thresh_F12_ ## x ## _AB, \
                    Performance::thresh_F12_ ## x ## _B

#define PERF_B(x)   TOK_F12_ ## x,                          \
                  0, & Performance::cfs_F12_    ## x ## _B, \
                  -1,  Performance::thresh_F12_ ## x ## _B

//════════════════════════════════════════════════════════════════════════════
// Helpers for bit-packed BOOL worker functions used by comparison primitives.
// DEF_VV_I2B: int64×int64 → BOOL, OP is a C++ comparison operator.
// DEF_VV_F2B: double×double → BOOL, PRED uses _av/_bv (values) and _ct (⎕CT).
#define DEF_VV_I2B(NAME, OP)                                               \
   static void NAME(uint64_t * pZ, const int64_t * pA, int incA,           \
                    const int64_t * pB, int incB, ShapeItem N)             \
   { const ShapeItem _nz = (N + 63) >> 6;                                  \
     loop(_c, _nz)                                                          \
        { uint64_t _w = 0;                                                  \
          const ShapeItem _b0 = _c << 6,                                    \
                          _b1 = _b0 + 64 > N ? N : _b0 + 64;              \
          for (ShapeItem _j = _b0; _j < _b1; ++_j)                         \
              if (pA[_j * incA] OP pB[_j * incB])                          \
                 _w |= uint64_t(1) << (_j - _b0);                          \
          pZ[_c] = _w; } }

#define DEF_VV_F2B(NAME, PRED)                                             \
   static void NAME(uint64_t * pZ, const double * pA, int incA,            \
                    const double * pB, int incB, ShapeItem N, double _ct)  \
   { const ShapeItem _nz = (N + 63) >> 6;                                  \
     loop(_c, _nz)                                                          \
        { uint64_t _w = 0;                                                  \
          const ShapeItem _b0 = _c << 6,                                    \
                          _b1 = _b0 + 64 > N ? N : _b0 + 64;              \
          for (ShapeItem _j = _b0; _j < _b1; ++_j)                         \
              { const double _av = pA[_j * incA], _bv = pB[_j * incB];    \
                if (PRED)   _w |= uint64_t(1) << (_j - _b0); }             \
          pZ[_c] = _w; } }

// DEF_VV_C16_2B: uint16×uint16 → BOOL, OP is a C++ comparison operator.
#define DEF_VV_C16_2B(NAME, OP)                                            \
   static void NAME(uint64_t * pZ, const uint16_t * pA, int incA,          \
                    const uint16_t * pB, int incB, ShapeItem N)            \
   { const ShapeItem _nz = (N + 63) >> 6;                                  \
     loop(_c, _nz)                                                          \
        { uint64_t _w = 0;                                                  \
          const ShapeItem _b0 = _c << 6,                                    \
                          _b1 = _b0 + 64 > N ? N : _b0 + 64;              \
          for (ShapeItem _j = _b0; _j < _b1; ++_j)                         \
              if (pA[_j * incA] OP pB[_j * incB])                          \
                 _w |= uint64_t(1) << (_j - _b0);                          \
          pZ[_c] = _w; } }

// DEF_VV_C32_2B: Unicode×Unicode → BOOL, OP is a C++ comparison operator.
#define DEF_VV_C32_2B(NAME, OP)                                            \
   static void NAME(uint64_t * pZ, const Unicode * pA, int incA,           \
                    const Unicode * pB, int incB, ShapeItem N)             \
   { const ShapeItem _nz = (N + 63) >> 6;                                  \
     loop(_c, _nz)                                                          \
        { uint64_t _w = 0;                                                  \
          const ShapeItem _b0 = _c << 6,                                    \
                          _b1 = _b0 + 64 > N ? N : _b0 + 64;              \
          for (ShapeItem _j = _b0; _j < _b1; ++_j)                         \
              if (pA[_j * incA] OP pB[_j * incB])                          \
                 _w |= uint64_t(1) << (_j - _b0);                          \
          pZ[_c] = _w; } }

// DEF_VV_B2B: bit×bit → bit, word-level op.  WEXPR uses uint64_t _av and _bv.
// inc=0: scalar (single bit in bit 0 of word 0, expanded to all-0/all-1 word).
// Tail bits beyond N are always zeroed so callers can use popcountll safely.
#define DEF_VV_B2B(NAME, WEXPR)                                            \
   static void NAME(uint64_t * pZ, const uint64_t * pA, int incA,          \
                    const uint64_t * pB, int incB, ShapeItem N)            \
   { const ShapeItem _nw = (N + 63) >> 6;                                  \
     const uint64_t _a0 = incA ? 0 : ((pA[0] & 1) ? UINT64_MAX : 0ULL);  \
     const uint64_t _b0 = incB ? 0 : ((pB[0] & 1) ? UINT64_MAX : 0ULL);  \
     loop(_w, _nw)                                                          \
        { const uint64_t _av = incA ? pA[_w] : _a0;                       \
          const uint64_t _bv = incB ? pB[_w] : _b0;                       \
          pZ[_w] = (WEXPR); }                                              \
     if (N & 63) pZ[_nw - 1] &= (uint64_t(1) << (N & 63)) - 1; }

// DEF_V_B2B: bit → bit, WEXPR uses uint64_t _bv.
// Tail bits beyond N are always zeroed so callers can use popcountll safely.
#define DEF_V_B2B(NAME, WEXPR)                                             \
   static void NAME(uint64_t * pZ, const uint64_t * pB, ShapeItem N)      \
   { const ShapeItem _nw = (N + 63) >> 6;                                  \
     loop(_w, _nw) { const uint64_t _bv = pB[_w]; pZ[_w] = (WEXPR); }   \
     if (N & 63) pZ[_nw - 1] &= (uint64_t(1) << (N & 63)) - 1; }

// DEF_VV_Z2Z: complex×complex → complex.
// inc=0: scalar (pA[0]=re, pA[1]=im); inc=1: N-element array.
// RE_EXPR/IM_EXPR use doubles _ar,_ai (A re/im) and _br,_bi (B re/im).
#define DEF_VV_Z2Z(NAME, RE_EXPR, IM_EXPR)                                \
   static void NAME(double * pZ, const double * pA, int incA,             \
                    const double * pB, int incB, ShapeItem N)             \
   { for (ShapeItem _i = 0; _i < N; ++_i)                                 \
        { const double _ar = pA[2*_i*incA], _ai = pA[2*_i*incA + 1];     \
          const double _br = pB[2*_i*incB], _bi = pB[2*_i*incB + 1];     \
          pZ[2*_i] = (RE_EXPR); pZ[2*_i + 1] = (IM_EXPR); } }

// DEF_V_Z2Z: complex → complex. RE_EXPR/IM_EXPR use doubles _br,_bi.
#define DEF_V_Z2Z(NAME, RE_EXPR, IM_EXPR)                                 \
   static void NAME(double * pZ, const double * pB, ShapeItem N)          \
   { for (ShapeItem _i = 0; _i < N; ++_i)                                 \
        { const double _br = pB[2*_i], _bi = pB[2*_i + 1];               \
          pZ[2*_i] = (RE_EXPR); pZ[2*_i + 1] = (IM_EXPR); } }

/** Scalar functions binomial and factorial.
 */
/// The class implementing !
class Bif_F12_BINOM : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_BINOM()
   : ScalarFunction(PERF_AB(BINOM))
   {}

   static Bif_F12_BINOM fun;      ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   /// @param B right argument APL value
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_factorial); }

   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_binomial); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_binomial; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_binomial); }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function less than.
 */
/// The class implementing <
class Bif_F2_LESS : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_LESS()
   : ScalarFunction(PERF_A(LESS))
   {}

   static Bif_F2_LESS  fun;         ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_less_than); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_less_than; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_0); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_less_than); }

   DEF_VV_I2B(vv_lt_i, <)
   DEF_VV_F2B(vv_lt_f, !tol_eq(_av, _bv, _ct) && _av < _bv)
   DEF_VV_C16_2B(vv_lt_c16, <)
   DEF_VV_C32_2B(vv_lt_c32, <)
   DEF_VV_B2B(vv_lt_b, ~_av & _bv)
   virtual vv_i2b_t    get_vv_i2b()    const { return &vv_lt_i; }
   virtual vv_f2b_t    get_vv_f2b()    const { return &vv_lt_f; }
   virtual vv_c16_2b_t get_vv_c16_2b() const { return &vv_lt_c16; }
   virtual vv_c32_2b_t get_vv_c32_2b() const { return &vv_lt_c32; }
   virtual vv_b2b_t    get_vv_b2b()    const { return &vv_lt_b; }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function equal.
 */
/// The class implementing =
class Bif_F2_EQUAL : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_EQUAL()
   : ScalarFunction(PERF_A(EQUAL))
   {}

   static Bif_F2_EQUAL  fun;        ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_equal); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_equal; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_equal); }

   DEF_VV_I2B(vv_eq_i, ==)
   DEF_VV_F2B(vv_eq_f, tol_eq(_av, _bv, _ct))
   DEF_VV_C16_2B(vv_eq_c16, ==)
   DEF_VV_C32_2B(vv_eq_c32, ==)
   DEF_VV_B2B(vv_eq_b, ~(_av ^ _bv))
   virtual vv_i2b_t    get_vv_i2b()    const { return &vv_eq_i; }
   virtual vv_f2b_t    get_vv_f2b()    const { return &vv_eq_f; }
   virtual vv_c16_2b_t get_vv_c16_2b() const { return &vv_eq_c16; }
   virtual vv_c32_2b_t get_vv_c32_2b() const { return &vv_eq_c32; }
   virtual vv_b2b_t    get_vv_b2b()    const { return &vv_eq_b; }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function EQ bitwise (i.e. bitwise not A xor B)
 */
/// The class implementing ⊤=
class Bif_F2_EQUAL_B : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_EQUAL_B()
   : ScalarFunction(PERF_A(EQUAL_B))
   {}

   static Bif_F2_EQUAL_B  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B,
                              &Cell::bif_equal_bitwise); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_equal_bitwise; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_equal_bitwise); }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function NE bitwise (i.e. bitwise A xor B)
 */
/// The class implementing ⊤≠
class Bif_F2_UNEQ_B : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_UNEQ_B()
   : ScalarFunction(PERF_A(UNEQ_B))
   {}

   static Bif_F2_UNEQ_B  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_not_equal_bitwise); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_not_equal_bitwise; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_not_equal_bitwise); }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function greater than.
 */
/// The class implementing >
class Bif_F2_GREATER : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_GREATER()
   : ScalarFunction(PERF_A(GREATER))
   {}

   static Bif_F2_GREATER  fun;      ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_greater_than); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_greater_than; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_0); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_greater_than); }

   DEF_VV_I2B(vv_gt_i, >)
   DEF_VV_F2B(vv_gt_f, !tol_eq(_av, _bv, _ct) && _av > _bv)
   DEF_VV_C16_2B(vv_gt_c16, >)
   DEF_VV_C32_2B(vv_gt_c32, >)
   DEF_VV_B2B(vv_gt_b, _av & ~_bv)
   virtual vv_i2b_t    get_vv_i2b()    const { return &vv_gt_i; }
   virtual vv_f2b_t    get_vv_f2b()    const { return &vv_gt_f; }
   virtual vv_c16_2b_t get_vv_c16_2b() const { return &vv_gt_c16; }
   virtual vv_c32_2b_t get_vv_c32_2b() const { return &vv_gt_c32; }
   virtual vv_b2b_t    get_vv_b2b()    const { return &vv_gt_b; }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function AND/LCM
 */
/// The class implementing ∧
class Bif_F2_AND : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_AND()
   : ScalarFunction(PERF_A(AND))
   {}

   static Bif_F2_AND  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_and); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_and; }

   /// return the associative cell function of this function
   virtual assoc_f2 get_assoc() const { return &Cell::bif_and; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_and); }

   DEF_VV_B2B(vv_and_b, _av & _bv)
   virtual vv_b2b_t get_vv_b2b() const { return &vv_and_b; }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function AND bitwise
 */
/// The class implementing ⊤∧
class Bif_F2_AND_B : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_AND_B()
   : ScalarFunction(PERF_A(AND_B))
   {}

   static Bif_F2_AND_B  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   /// @param B right argument APL value
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_within_quad_CT); }

   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B,
                              &Cell::bif_and_bitwise); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_and_bitwise; }

   /// return the associative cell function of this function
   virtual assoc_f2 get_assoc() const { return &Cell::bif_and_bitwise; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_and_bitwise); }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function OR/GCD
 */
/// The class implementing ∨
class Bif_F2_OR : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_OR()
   : ScalarFunction(PERF_A(OR))
   {}

   static Bif_F2_OR  fun;           ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_or); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_or; }

   /// return the associative cell function of this function
   virtual assoc_f2 get_assoc() const { return &Cell::bif_or; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_0); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_or); }

   DEF_VV_B2B(vv_or_b, _av | _bv)
   virtual vv_b2b_t get_vv_b2b() const { return &vv_or_b; }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function OR bitwise
 */
/// The class implementing ⊤∨
class Bif_F2_OR_B : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_OR_B()
   : ScalarFunction(PERF_A(OR_B))
   {}

   static Bif_F2_OR_B  fun;           ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   /// @param B right argument APL value
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_near_int64_t); }

   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_or_bitwise); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_or_bitwise; }

   /// return the associative cell function of this function
   virtual assoc_f2 get_assoc() const { return &Cell::bif_or_bitwise; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_0); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_or_bitwise); }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function less or equal.
 */
/// The class implementing ≤
class Bif_F2_LEQU : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_LEQU()
   : ScalarFunction(PERF_A(LEQU))
   {}

   static Bif_F2_LEQU  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_less_eq); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_less_eq; }

   /// overloaded Function::eval_identity_fun();
   /// @param B right argument APL value
   /// @param axis axis along which the identity is applied
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   /// @param A left argument APL value
   /// @param X axis specification value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_less_eq); }

   DEF_VV_I2B(vv_le_i, <=)
   DEF_VV_F2B(vv_le_f, tol_eq(_av, _bv, _ct) || _av <= _bv)
   DEF_VV_C16_2B(vv_le_c16, <=)
   DEF_VV_C32_2B(vv_le_c32, <=)
   DEF_VV_B2B(vv_le_b, ~_av | _bv)
   virtual vv_i2b_t    get_vv_i2b()    const { return &vv_le_i; }
   virtual vv_f2b_t    get_vv_f2b()    const { return &vv_le_f; }
   virtual vv_c16_2b_t get_vv_c16_2b() const { return &vv_le_c16; }
   virtual vv_c32_2b_t get_vv_c32_2b() const { return &vv_le_c32; }
   virtual vv_b2b_t    get_vv_b2b()    const { return &vv_le_b; }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function greater or equal.
 */
/// The class implementing ≥
class Bif_F2_MEQU : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_MEQU()
   : ScalarFunction(PERF_A(MEQU))
   {}

   static Bif_F2_MEQU  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_greater_eq); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_greater_eq; }

   /// overloaded Function::eval_identity_fun();
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_greater_eq); }

   DEF_VV_I2B(vv_ge_i, >=)
   DEF_VV_F2B(vv_ge_f, tol_eq(_av, _bv, _ct) || _av >= _bv)
   DEF_VV_C16_2B(vv_ge_c16, >=)
   DEF_VV_C32_2B(vv_ge_c32, >=)
   DEF_VV_B2B(vv_ge_b, _av | ~_bv)
   virtual vv_i2b_t    get_vv_i2b()    const { return &vv_ge_i; }
   virtual vv_f2b_t    get_vv_f2b()    const { return &vv_ge_f; }
   virtual vv_c16_2b_t get_vv_c16_2b() const { return &vv_ge_c16; }
   virtual vv_c32_2b_t get_vv_c32_2b() const { return &vv_ge_c32; }
   virtual vv_b2b_t    get_vv_b2b()    const { return &vv_ge_b; }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function not equal
 */
/// The class implementing ≠
class Bif_F2_UNEQU : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_UNEQU()
   : ScalarFunction(PERF_A(UNEQU))
   {}

   static Bif_F2_UNEQU  fun;         ///< Built-in function.

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_not_equal); }

   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const;

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_not_equal; }

protected:
   /// overloaded Function::eval_identity_fun();
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_0); }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_not_equal); }

   DEF_VV_I2B(vv_ne_i, !=)
   DEF_VV_F2B(vv_ne_f, !tol_eq(_av, _bv, _ct))
   DEF_VV_C16_2B(vv_ne_c16, !=)
   DEF_VV_C32_2B(vv_ne_c32, !=)
   DEF_VV_B2B(vv_ne_b, _av ^ _bv)
   virtual vv_i2b_t    get_vv_i2b()    const { return &vv_ne_i; }
   virtual vv_f2b_t    get_vv_f2b()    const { return &vv_ne_f; }
   virtual vv_c16_2b_t get_vv_c16_2b() const { return &vv_ne_c16; }
   virtual vv_c32_2b_t get_vv_c32_2b() const { return &vv_ne_c32; }
   virtual vv_b2b_t    get_vv_b2b()    const { return &vv_ne_b; }
};
#undef DEF_VV_I2B
#undef DEF_VV_F2B
#undef DEF_VV_C16_2B
#undef DEF_VV_C32_2B
//────────────────────────────────────────────────────────────────────────────
/** Scalar function find.
 */
/// The class implementing ⋸
class Bif_F2_FIND : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_FIND()
   : ScalarFunction(PERF_A(FIND))
   {}

   static Bif_F2_FIND  fun;         ///< Built-in function.

   /// overloaded ScalarFunction::may_parallel()
   virtual bool may_parallel() const   { return false; }

protected:
   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return 0; }

   /// Return true iff A is contained in B.
   static bool contained(const Shape & shape_A, cValue_R A,
                         cValue_R B, const Shape & idx_B, double qct);
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function NOR
 */
/// The class implementing ⍱
class Bif_F2_NOR : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_NOR()
   : ScalarFunction(PERF_A(NOR))
   {}

   static Bif_F2_NOR  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_nor); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_nor; }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_nor); }

   DEF_VV_B2B(vv_nor_b, ~(_av | _bv))
   virtual vv_b2b_t get_vv_b2b() const { return &vv_nor_b; }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function NOR bitwise
 */
/// The class implementing ⊤⍱
class Bif_F2_NOR_B : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_NOR_B()
   : ScalarFunction(PERF_A(NOR_B))
   {}

   static Bif_F2_NOR_B  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_not_bitwise); }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B,
                              &Cell::bif_nor_bitwise); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_nor_bitwise; }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_nor_bitwise); }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function nand.
 */
/// The class implementing ⍲
class Bif_F2_NAND : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_NAND()
   : ScalarFunction(PERF_A(NAND))
   {}

   static Bif_F2_NAND  fun;         ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_nand); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_nand; }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_nand); }

   DEF_VV_B2B(vv_nand_b, ~(_av & _bv))
   virtual vv_b2b_t get_vv_b2b() const { return &vv_nand_b; }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function NAND bitwise
 */
/// The class implementing ⊤⍲
class Bif_F2_NAND_B : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F2_NAND_B()
   : ScalarFunction(PERF_A(NAND_B))
   {}

   static Bif_F2_NAND_B  fun;         ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_nand_bitwise); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_nand_bitwise; }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_nand_bitwise); }
};
#undef DEF_VV_B2B
#undef DEF_V_B2B
//────────────────────────────────────────────────────────────────────────────
/** Scalar functions power and exponential.
 */
/// The class implementing ⋆
class Bif_F12_POWER : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_POWER()
   : ScalarFunction(PERF_AB(POWER))
   {}

   static Bif_F12_POWER  fun;       ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_exponential); }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_power); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_power; }

   /// overloaded Function::eval_identity_fun();
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_power); }

   /// overloaded Function::get_monadic_inverse()
   virtual cFunction_P get_monadic_inverse() const;

   /// overloaded Function::get_dyadic_inverse()
   virtual cFunction_P get_dyadic_inverse() const;
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar functions add and conjugate.
 */
/// The class implementing +
class Bif_F12_PLUS : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_PLUS(bool inv)
   : ScalarFunction(PERF_AB(PLUS)),
     inverse(inv)
   {}

   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_conjugate); }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B,
               inverse ? &Cell::bif_add_inverse : &Cell::bif_add); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return inverse ? &Cell::bif_add_inverse : &Cell::bif_add; }

   /// return the associative cell function of this function
   virtual assoc_f2 get_assoc() const { return &Cell::bif_add; }

   static Bif_F12_PLUS  fun;           ///< Built-in function.
   static Bif_F12_PLUS  fun_inverse;   ///< Built-in function.

   static void vv_plus(double * pZ, const double * pA, int incA,
                                    const double * pB, int incB, ShapeItem N)
      { loop(i, N)
           { pZ[i] = pA[i * incA] + pB[i * incB];
             // the per-cell (non-packed) path enforces this; this packed
             // fast path silently stored raw IEEE overflow/NaN instead --
             // (12⍴1E308)+(12⍴1E308) gave ∞ while the 2-element (non-
             // packed) case correctly gave DOMAIN_ERROR.
             if (!isfinite(pZ[i]))   DOMAIN_ERROR;
           } }
   static void v_conjugate(double * pZ, const double * pB, ShapeItem N)
      { loop(i, N) pZ[i] = pB[i]; }

   static void v_conj_i(int64_t * pZ, const int64_t * pB, ShapeItem N)
      { loop(i, N) pZ[i] = pB[i]; }

   DEF_VV_Z2Z(vv_add_z, _ar + _br, _ai + _bi)
   DEF_V_Z2Z(v_conj_z, _br, -_bi)

   virtual vv_f2f_t get_vv_f2f() const { return &vv_plus; }
   virtual v_f2f_t  get_v_f2f()  const { return &v_conjugate; }
   virtual v_i2i_t  get_v_i2i()  const { return &v_conj_i; }
   virtual vv_z2z_t get_vv_z2z() const { return &vv_add_z; }
   virtual v_z2z_t  get_v_z2z()  const { return &v_conj_z; }

protected:
   /// overloaded Function::eval_identity_fun();
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_0); }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_add); }

   /// overloaded Function::get_dyadic_inverse()
   virtual cFunction_P get_dyadic_inverse() const;

   /// true if the inverse shall be computed. This allows Bif_F12_CIRCLE to
   /// be instantiated twice: once for non-inverted operation and once for
   /// inverted operation, and both instances can share some code in this class
   const bool inverse;
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar functions subtract and negative.
 */
/// The class implementing -
class Bif_F12_MINUS : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_MINUS()
   : ScalarFunction(PERF_AB(MINUS))
   {}

   static Bif_F12_MINUS  fun;       ///< Built-in function.

   static void vv_minus(double * pZ, const double * pA, int incA,
                                     const double * pB, int incB, ShapeItem N)
      { loop(i, N)
           { pZ[i] = pA[i * incA] - pB[i * incB];
             if (!isfinite(pZ[i]))   DOMAIN_ERROR;
           } }
   static void v_negate(double * pZ, const double * pB, ShapeItem N)
      { loop(i, N) pZ[i] = -pB[i]; }

   // no get_v_i2i() override: -pB[i] on an INT64_MIN element overflows
   // and (being outside the worker's int64-only signature) cannot promote
   // to float the way the scalar Cell path (IntCell::bif_negative_i)
   // correctly does -- confirmed to silently return a wrong (still
   // negative) result for a packed int64 ravel containing INT64_MIN.
   // Falling back to the Cell path is the correct, if slower, choice.

   DEF_VV_Z2Z(vv_sub_z, _ar - _br, _ai - _bi)
   DEF_V_Z2Z(v_neg_z, -_br, -_bi)

   virtual vv_f2f_t get_vv_f2f() const { return &vv_minus; }
   virtual v_f2f_t  get_v_f2f()  const { return &v_negate; }
   virtual vv_z2z_t get_vv_z2z() const { return &vv_sub_z; }
   virtual v_z2z_t  get_v_z2z()  const { return &v_neg_z; }

protected:
   /// overloaded Function::eval_AB()
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_subtract); }

   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_negative); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_subtract; }

   /// overloaded Function::eval_identity_fun();
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_0); }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_subtract); }

   /// overloaded Function::get_monadic_inverse()
   virtual cFunction_P get_monadic_inverse() const;

   /// overloaded Function::get_dyadic_inverse()
   virtual cFunction_P get_dyadic_inverse() const;
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function roll and non-scalar function dial.
 */
/// The class implementing ?
class Bif_F12_ROLL : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_ROLL()
   : ScalarFunction(PERF_B(ROLL))
   {}

   static Bif_F12_ROLL  fun;        ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const;

   /// dial A from B.
   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return 0; }

   /// overloaded ScalarFunction::may_parallel()
   virtual bool may_parallel() const   { return false; }

   /// recursively check that all ravel elements of B are integers ≥ 0 and
   /// return \b true iff not.
   static bool check_B(const cValue & B, double qct);
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar function not and non-scalar function without.
 */
/// The class implementing ∼
class Bif_F12_WITHOUT : public ScalarFunction // (almost)
{
public:
   /// Constructor.
   Bif_F12_WITHOUT()
   : ScalarFunction(PERF_B(WITHOUT))
   {}

   static Bif_F12_WITHOUT  fun;     ///< Built-in function.

   /// Compute A without B.
   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// Overloaded Function::eval_identity_fun. Dyadic ∼ is not scalar, therefore
   /// its eval_identity_fun() differs from ScalarFunction::eval_identity_fun().
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const;

   /// overloaded ScalarFunction::may_parallel()
   virtual bool may_parallel() const   { return false; }

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_not); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return 0; }

   /// eval_AB for large A and/or B
   static Value_P large_eval_AB(const cValue & A, const cValue & B);
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar functions times and direction.
 */
/// The class implementing ×
class Bif_F12_TIMES : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_TIMES(bool inv)
   : ScalarFunction(PERF_AB(TIMES)),
     inverse(inv)
   {}

   static Bif_F12_TIMES  fun;           ///< Built-in function.
   static Bif_F12_TIMES  fun_inverse;   ///< Built-in function.

   static void vv_times(double * pZ, const double * pA, int incA,
                                     const double * pB, int incB, ShapeItem N)
      { loop(i, N)
           { pZ[i] = pA[i * incA] * pB[i * incB];
             if (!isfinite(pZ[i]))   DOMAIN_ERROR;
           } }

   static void v_signum_i(int64_t * pZ, const int64_t * pB, ShapeItem N)
      { loop(i, N) pZ[i] = (0 < pB[i]) - (pB[i] < 0); }

   DEF_VV_Z2Z(vv_mul_z, _ar*_br - _ai*_bi, _ar*_bi + _ai*_br)

   virtual vv_f2f_t get_vv_f2f() const { return &vv_times; }
   virtual v_i2i_t  get_v_i2i()  const { return &v_signum_i; }
   virtual vv_z2z_t get_vv_z2z() const { return &vv_mul_z; }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B,
               inverse ? &Cell::bif_multiply_inverse : &Cell::bif_multiply); }

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_direction); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return inverse ? &Cell::bif_multiply_inverse : &Cell::bif_multiply; }

   /// return the associative cell function of this function
   virtual assoc_f2 get_assoc() const { return &Cell::bif_multiply; }

   /// overloaded Function::eval_identity_fun();
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_multiply); }

   /// overloaded Function::get_dyadic_inverse()
   virtual cFunction_P get_dyadic_inverse() const;

   /// true if the inverse shall be computed. This allows Bif_F12_CIRCLE to
   /// be instantiated twice: once for non-inverted operation and once for
   /// inverted operation, and both instances can share some code in this class
   const bool inverse;
};
#undef DEF_VV_Z2Z
#undef DEF_V_Z2Z
//────────────────────────────────────────────────────────────────────────────
/** Scalar functions divide and reciprocal.
 */
/// The class implementing ÷
class Bif_F12_DIVIDE : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_DIVIDE()
   : ScalarFunction(PERF_AB(DIVIDE))
   {}

   static Bif_F12_DIVIDE  fun;      ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_reciprocal); }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_divide); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_divide; }

   /// overloaded Function::eval_identity_fun();
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, integer_1); }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_divide); }

   /// overloaded Function::get_monadic_inverse()
   virtual cFunction_P get_monadic_inverse() const;

   /// overloaded Function::get_dyadic_inverse()
   virtual cFunction_P get_dyadic_inverse() const;
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar functions circle functions and pi times.
 */
/// The class implementing ○
class Bif_F12_CIRCLE : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_CIRCLE(bool inv)
   : ScalarFunction(PERF_AB(CIRCLE)),
     inverse(inv)
   {}

   static Bif_F12_CIRCLE  fun;              ///< Built-in function.
   static Bif_F12_CIRCLE  fun_inverse;      ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B,
               inverse ? &Cell::bif_pi_times_inverse : &Cell::bif_pi_times); }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, inverse ? &Cell::bif_circle_fun_inverse
                                            : &Cell::bif_circle_fun); }

   /// overloaded Function::get_scalar_f2
   virtual prim_f2 get_scalar_f2() const
      { return inverse ? &Cell::bif_circle_fun_inverse
                       : &Cell::bif_circle_fun; }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, inverse ? &Cell::bif_circle_fun_inverse
                                                 : &Cell::bif_circle_fun); }

   /// overloaded Function::get_monadic_inverse()
   virtual cFunction_P get_monadic_inverse() const;

   /// overloaded Function::get_dyadic_inverse()
   virtual cFunction_P get_dyadic_inverse() const;

   /// true if the inverse shall be computed. This allows Bif_F12_CIRCLE to
   /// be instantiated twice: once for non-inverted operation and once for
   /// inverted operation, and both instances can share some code in this class
   const bool inverse;
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar functions maximum and round up.
 */
/// The class implementing ⌈
class Bif_F12_RND_UP : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_RND_UP()
   : ScalarFunction(PERF_AB(RND_UP))
   {}

   static Bif_F12_RND_UP  fun;      ///< Built-in function.

   static void vv_max_i(int64_t * pZ, const int64_t * pA, int incA,
                                      const int64_t * pB, int incB, ShapeItem N)
      { loop(i, N) pZ[i] = pA[i*incA] > pB[i*incB] ? pA[i*incA] : pB[i*incB]; }
   static void vv_max_f(double * pZ, const double * pA, int incA,
                                     const double * pB, int incB, ShapeItem N)
      { loop(i, N) pZ[i] = pA[i*incA] >= pB[i*incB] ? pA[i*incA] : pB[i*incB]; }
   static void v_ceil_i(int64_t * pZ, const int64_t * pB, ShapeItem N)
      { loop(i, N) pZ[i] = pB[i]; }

   virtual vv_f2f_t get_vv_f2f() const { return &vv_max_f; }
   virtual vv_i2i_t get_vv_i2i() const { return &vv_max_i; }
   virtual v_i2i_t  get_v_i2i()  const { return &v_ceil_i; }

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_ceiling); }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_maximum); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_maximum; }

   /// return the associative cell function of this function
   virtual assoc_f2 get_assoc() const { return &Cell::bif_maximum; }

   /// overloaded Function::eval_identity_fun();
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, float_min); }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_maximum); }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar functions minimum and round down.
 */
/// The class implementing ⌊
class Bif_F12_RND_DN : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_RND_DN()
   : ScalarFunction(PERF_AB(RND_DN))
   {}

   static Bif_F12_RND_DN  fun;      ///< Built-in function.

   static void vv_min_i(int64_t * pZ, const int64_t * pA, int incA,
                                      const int64_t * pB, int incB, ShapeItem N)
      { loop(i, N) pZ[i] = pA[i*incA] < pB[i*incB] ? pA[i*incA] : pB[i*incB]; }
   static void vv_min_f(double * pZ, const double * pA, int incA,
                                     const double * pB, int incB, ShapeItem N)
      { loop(i, N) pZ[i] = pA[i*incA] <= pB[i*incB] ? pA[i*incA] : pB[i*incB]; }
   static void v_floor_i(int64_t * pZ, const int64_t * pB, ShapeItem N)
      { loop(i, N) pZ[i] = pB[i]; }

   virtual vv_f2f_t get_vv_f2f() const { return &vv_min_f; }
   virtual vv_i2i_t get_vv_i2i() const { return &vv_min_i; }
   virtual v_i2i_t  get_v_i2i()  const { return &v_floor_i; }

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_floor); }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_minimum); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_minimum; }

   /// return the associative cell function of this function
   virtual assoc_f2 get_assoc() const { return &Cell::bif_minimum; }

   /// overloaded Function::eval_identity_fun();
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
      { return eval_scalar_identity_fun(B, axis, float_max); }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_minimum); }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar functions residue and magnitude.
 */
/// The class implementing ∣
class Bif_F12_STILE : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_STILE()
   : ScalarFunction(PERF_AB(STILE))
   {}

   static Bif_F12_STILE  fun;       ///< Built-in function.

   static void v_abs(double * pZ, const double * pB, ShapeItem N)
      { loop(i, N) pZ[i] = pB[i] < 0 ? -pB[i] : pB[i]; }

   // no get_v_i2i() override: -pB[i] on an INT64_MIN element overflows
   // and cannot promote to float from within this int64-only worker --
   // see Bif_F12_MINUS's identical comment above; same confirmed bug,
   // same fix.

   virtual v_f2f_t get_v_f2f() const { return &v_abs; }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_residue); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_residue; }

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_magnitude); }

   /// overloaded Function::eval_identity_fun();
   virtual Token eval_identity_fun(cValue_R B, sAxis axis) const
   { return eval_scalar_identity_fun(B, axis, integer_0); }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_residue); }
};
//────────────────────────────────────────────────────────────────────────────
/** Scalar functions logarithms.
 */
/// The class implementing ⍟
class Bif_F12_LOGA : public ScalarFunction
{
public:
   /// Constructor.
   Bif_F12_LOGA()
   : ScalarFunction(PERF_AB(LOGA))
   {}

   static Bif_F12_LOGA  fun;        ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   virtual Token eval_B(cValue_R B) const
      { return eval_scalar_B(B, &Cell::bif_nat_log); }

   /// overloaded Function::eval_AB().
   virtual Token eval_AB(cValue_R A, cValue_R B) const
      { return eval_scalar_AB(A,
                       B, &Cell::bif_logarithm); }

   /// overloaded Function::get_scalar_f2()
   virtual prim_f2 get_scalar_f2() const
      { return &Cell::bif_logarithm; }

   /// overloaded Function::eval_AXB().
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
      { return eval_scalar_AXB(A, X, B, &Cell::bif_logarithm); }

   /// overloaded Function::get_monadic_inverse()
   virtual cFunction_P get_monadic_inverse() const;

   /// overloaded Function::get_dyadic_inverse()
   virtual cFunction_P get_dyadic_inverse() const;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __SCALAR_FUNCTION_HH_DEFINED__
