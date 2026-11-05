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

#ifndef __ARGCHECK_HH_DEFINED__
#define __ARGCHECK_HH_DEFINED__

#include "Error_macros.hh"
#include "Value.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
/**
   Reusable argument-validation building blocks for primitive eval_XXX()
   functions.

   Many primitives share the same handful of argument-shape invariants
   (e.g. "A is a scalar or vector", "A and B are conformable"). Rather than
   hand-writing the check plus its )MORE text at every call site, each
   function here does both: if the invariant does not hold, it sets )MORE
   text (following the convention in the ")MORE error convention" project:
   RANK_ERROR shows ranks, LENGTH_ERROR shows shapes, DOMAIN_ERROR shows
   the offending value) and throws the matching APL error. If a call
   returns normally, the invariant holds and the caller may rely on it
   without re-checking.

   \b where is the arity-prefix for the primitive, e.g. "A⍴B" -- see the
   MORE_ERROR() convention (project_apl_more_error_convention memory).
   \b argname is the argument's own name as used in \b where, e.g. "A".
*/
class ArgCheck
{
public:
   /// require \b val to be a scalar or vector (⍴⍴val ≤ 1).
   /// @param where arity-prefix, e.g. "A⍴B"
   /// @param argname name of \b val as used in \b where, e.g. "A"
   /// @param val the value to check
   static inline void require_scalar_or_vector(const char * where,
                                       const char * argname, cValue_R val)
      {
        if (val.get_rank() > 1)
           {
             MORE_ERROR() << where << ": " << argname
                          << " must be a scalar or vector; ⍴⍴" << argname
                          << " is " << val.get_rank();
             RANK_ERROR;
           }
      }

   /// require every item of \b val to be a non-negative near-integer.
   /// \b val is typically already known to be require_scalar_or_vector().
   /// @param where arity-prefix, e.g. "A⍴B"
   /// @param argname name of \b val as used in \b where, e.g. "A"
   /// @param val the value to check
   static inline void require_non_negative_ints(const char * where,
                                        const char * argname, cValue_R val)
      {
        const APL_Integer qio = Workspace::get_IO();
        loop(i, val.element_count())
            {
              const APL_Integer v = val.get_near_int(i);
              if (v < 0)
                 {
                   MORE_ERROR() << where << ": " << argname << "["
                                << (i + qio) << "] = " << v
                                << " is negative; " << argname
                                << " must contain only non-negative"
                                   " integers";
                   DOMAIN_ERROR;
                 }
            }
      }

   /// require \b shape_A and \b shape_B to be the same shape, or one of
   /// them scalar-extensible (a scalar or a 1-element shape) -- the
   /// specific conformability rule used by scalar dyadic primitives (+,
   /// -, ×, ..., see ScalarFunction::conforming_shape()). Do NOT reuse
   /// this for primitives with a *different* notion of conformability
   /// (e.g. "scalar or vector" is a different, unrelated check -- see
   /// require_scalar_or_vector() -- and some primitives do not allow
   /// scalar extension at all); name any other conformability rule
   /// explicitly rather than overloading this one, since "conformable"
   /// on its own is ambiguous across primitives.
   ///
   /// Non-throwing core: sets \b ec (E_RANK_ERROR or E_LENGTH_ERROR) and
   /// )MORE text and returns 0 on mismatch, so that it is also safe to
   /// call from a worker thread during parallel evaluation (where the
   /// actual throw must happen later, back on the interpreter thread).
   /// @param ec receives the error code if shape_A and shape_B mismatch
   /// @param where arity-prefix, e.g. "A+B"
   /// @param shape_A shape of the left argument
   /// @param shape_B shape of the right argument
   static inline const Shape * check_same_shape_or_scalar_extensible(
                                          ErrorCode & ec, const char * where,
                                          const Shape & shape_A,
                                          const Shape & shape_B)
      {
        if (shape_A.get_rank() == 0)     return &shape_B;
        if (shape_B.get_rank() == 0)     return &shape_A;

        // Both singleton extension rules (volume 1) can apply at once
        // when A and B are both one-element arrays of different rank;
        // testing them in a fixed order made the result shape (and
        // therefore e.g. ⍴A+B vs ⍴B+A for commutative functions like +)
        // depend on argument order. Per APL2/ISO 13751 singleton
        // extension, the result takes the shape of the higher-rank
        // argument (Blake McBride, Bugs20 #5).
        //
        if (shape_A.get_volume() == 1 && shape_B.get_volume() == 1)
           return shape_A.get_rank() >= shape_B.get_rank() ? &shape_A
                                                             : &shape_B;
        if (shape_A.get_volume() == 1)   return &shape_B;
        if (shape_B.get_volume() == 1)   return &shape_A;
        if (shape_A == shape_B)          return &shape_A;

        if (shape_A.get_rank() != shape_B.get_rank())
           {
             MORE_ERROR() << where << ": expecting ⍴⍴A = ⍴⍴B (or A or B a"
                             " scalar); ⍴⍴A is " << shape_A.get_rank()
                          << ", ⍴⍴B is " << shape_B.get_rank();
             ec = E_RANK_ERROR;
             return 0;
           }

        MORE_ERROR() << where << ": expecting ⍴A = ⍴B (or A or B a"
                        " scalar); ⍴A is " << shape_A << ", ⍴B is "
                     << shape_B;
        ec = E_LENGTH_ERROR;
        return 0;
      }

   /// throwing wrapper around check_same_shape_or_scalar_extensible():
   /// returns the result shape, or throws RANK_ERROR/LENGTH_ERROR (with
   /// )MORE text already set) if shape_A and shape_B do not conform.
   /// @param where arity-prefix, e.g. "A+B"
   /// @param shape_A shape of the left argument
   /// @param shape_B shape of the right argument
   static inline const Shape & require_same_shape_or_scalar_extensible(
                                          const char * where,
                                          const Shape & shape_A,
                                          const Shape & shape_B)
      {
        ErrorCode ec = E_NO_ERROR;
        const Shape * result =
              check_same_shape_or_scalar_extensible(ec, where,
                                                     shape_A, shape_B);
        if (ec == E_RANK_ERROR)     RANK_ERROR;
        if (ec == E_LENGTH_ERROR)   LENGTH_ERROR;
        return *result;
      }

   /// NOT a check (never throws) -- a )MORE text formatting helper. Return
   /// " Note: ⎕IO=0." iff the current ⎕IO is 0 and the already
   /// ⎕IO-adjusted index \b idx (i.e. the raw index minus the current
   /// ⎕IO) would have been valid (0 <= idx < dim) had ⎕IO instead been 1.
   /// Append to an out-of-range-index )MORE text so the reader isn't
   /// left wondering whether the index would work under the far more
   /// common ⎕IO=1 -- e.g. (2⊃1 2 3) at ⎕IO←0 is only an error because
   /// ⎕IO=0 makes "2" mean the 3rd item.
   /// @param idx the failed index, already adjusted for the current ⎕IO
   /// @param dim the valid length along the relevant axis
   static inline const char * index_io0_note(APL_Integer idx, ShapeItem dim)
      {
        if (Workspace::get_IO() != 0)   return "";
        const APL_Integer idx_at_io1 = idx - 1;
        if (idx_at_io1 >= 0 && idx_at_io1 < dim)   return " Note: ⎕IO=0.";
        return "";
      }

   /// NOT a check (never throws) -- a )MORE text formatting helper.
   /// Append "shape S T ..." resp. "a scalar" to \b more, describing
   /// \b val's shape. Plain "⍴val is " << val.get_shape() reads fine for
   /// a vector or higher-rank val, but silently prints nothing at all for
   /// a scalar (⍴ of a scalar is empty) -- misleading in running prose,
   /// since e.g. "...; A is " with nothing after it looks like a cut-off
   /// sentence rather than the (accurate but easy to misread) statement
   /// that A is a scalar.
   /// @param more the )MORE text (e.g. MORE_ERROR()) to append to
   /// @param val the value whose shape to describe
   static inline void append_shape(UCS_string & more, const cValue & val)
      {
        if (val.get_rank() == 0)   more << "a scalar";
        else                       more << "shape " << val.get_shape();
      }
};
//════════════════════════════════════════════════════════════════════════════
#endif // __ARGCHECK_HH_DEFINED__
