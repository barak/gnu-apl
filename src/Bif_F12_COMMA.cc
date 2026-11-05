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

#include <string.h>
#include "ArgCheck.hh"
#include "Bif_F12_COMMA.hh"
#include "StateIndicator.hh"
#include "Workspace.hh"

Bif_F12_COMMA  Bif_F12_COMMA ::fun;    // ,
Bif_F12_COMMA1 Bif_F12_COMMA1::fun;    // ⍪

//════════════════════════════════════════════════════════════════════════════
Token
Bif_COMMA::ravel_axis(const char * where, cValue_R X, cValue_R B, uAxis axis)
{
const APL_Integer qio = Workspace::get_IO();

   if (false)   SYNTAX_ERROR;

   // I must be a (integer or real) scalar or simple integer vector
   //
   if (X.get_rank() > 1)
      {
        MORE_ERROR() << where << ": X must be a scalar or vector; ⍴⍴X is "
                     << X.get_rank();
        AXIS_ERROR;
      }

   // There are 3 variants, determined by I:
   //
   // 1) I is empty:             append a new last axis of length 1
   // 2) I is a fraction:        append a new axis before I
   // 3a) I is integer scalar:   return B
   // 3b) I is integer vector:   combine axes

   // case 1:   ,['']B or ,[⍳0]B : append new first or last axis of length 1.
   //
   if (X.element_count() == 0)
      {
        if (B.get_rank() == MAX_RANK)
           {
             MORE_ERROR() << where << ": ⍴⍴B (=" << B.get_rank()
                          << ") already is the system limit " << MAX_RANK
                          << "; cannot insert another axis";
             AXIS_ERROR;
           }

        const Shape shape_Z = B.get_shape().insert_axis(axis, 1);
        return ravel(shape_Z, B);
      }

   // case 2:   ,[x.y]B : insert axis before axis x+1
   //
   if (!X.is_near_int(0))  // fraction: insert an axis
      {
        if (B.get_rank() == MAX_RANK)
           {
             MORE_ERROR() << where << ": ⍴⍴B (=" << B.get_rank()
                          << ") already is the system limit " << MAX_RANK
                          << "; cannot insert another axis";
             AXIS_ERROR;
           }

        const APL_Float new_axis = X.get_real_value(0) - qio;
        // reject before narrowing to sAxis (int16_t): an unchecked
        // float->int16 conversion for a magnitude beyond int16 range is
        // UB, and silently landed the new axis at the front instead of
        // raising an error (⍴,[100000.5]M gave 1 2 3 with no error).
        if (new_axis > 32000.0 || new_axis < -32000.0)
           {
             MORE_ERROR() << where << ": X = " << (new_axis + qio)
                          << " is too far from a valid axis of B";
             AXIS_ERROR;
           }
        // a fractional axis must fall strictly between two of B's
        // existing (0-based) axis positions, i.e. in the OPEN interval
        // (-1, ⍴⍴B) -- valid insertion points range from "before axis
        // 0" (new_axis just above -1) to "after axis ⍴⍴B-1" (new_axis
        // just below ⍴⍴B). Anything outside that used to be silently
        // clamped to the first/last axis instead of AXIS ERROR (both
        // the >32000 guard above and the negative special-case below
        // only bounded the MAGNITUDE, never checked it was actually a
        // valid axis of THIS B). See Bugs27 #32.
        //
        if (new_axis <= -1.0 || new_axis >= B.get_rank())
           {
             MORE_ERROR() << where << ": X = " << (new_axis + qio)
                          << " is not a valid axis of B (expecting "
                          << (qio - 1) << "<X<" << (B.get_rank() + qio)
                          << ")";
             AXIS_ERROR;
           }
        sAxis axis = new_axis;   if (new_axis < 0.0)   axis = -1;
        const Shape shape_Z = B.get_shape().insert_axis(axis + 1, 1);
        return ravel(shape_Z, B);
      }
   // case 3a: ,[n]B : return B (combine single axis doesn't change anything)
   //
   if (X.is_scalar_or_len1_vector())   // single int: return B.
      {
        // this shortcut had no range check on n at all: ,[99]M and
        // ,[¯5]M both silently returned M unchanged instead of
        // AXIS_ERROR.
        const APL_Integer n = X.get_near_int(0) - qio;
        if (n < 0 || n >= B.get_rank())
           {
             MORE_ERROR() << where << ": X = " << (n + qio)
                          << " is not a valid axis of B (expecting " << qio
                          << "≤X<" << (B.get_rank() + qio) << ")"
                          << ArgCheck::index_io0_note(n, B.get_rank());
             AXIS_ERROR;
           }

        // Bugs31 #19/#23 (Blake McBride): CLONE() aliases B rather than
        // copying it, so any lvalue pass-through marker on B would
        // otherwise survive this identity application unbroken -- see
        // Value::clear_lval_markers().
        //
        Value_P Z = CLONE(&B, LOC);
        Z->clear_lval_markers();
        Token result(TOK_APL_VALUE1, Z);
        return result;
      }

   // case 3b: ,[n1 ... nk]B : combine axes.
   //
const Shape axes(X, qio);

const ShapeItem from = axes.get_first_shape_item();
   if (from < 0)
      {
        MORE_ERROR() << where << ": X = " << (from + qio)
                     << " is not a valid axis of B (expecting " << qio
                     << "≤X<" << (B.get_rank() + qio) << ")"
                     << ArgCheck::index_io0_note(from, B.get_rank());
        AXIS_ERROR;
      }

const ShapeItem to   = axes.get_last_shape_item();
   if (to >= B.get_rank())
      {
        MORE_ERROR() << where << ": X = " << (to + qio)
                     << " is not a valid axis of B (expecting " << qio
                     << "≤X<" << (B.get_rank() + qio) << ")"
                     << ArgCheck::index_io0_note(to, B.get_rank());
        AXIS_ERROR;
      }

   // from is only checked against the LOWER bound above (>= 0) and to only
   // against the UPPER bound (< B.get_rank()); the contiguity loop below
   // then reads B.get_shape_item(from + a) trusting from <= to (i.e. the
   // axes were given in ascending order) to keep from + a inside
   // [0, B.get_rank()). A non-ascending X, e.g. ,[2 1]1 2, has from (X's
   // first item) numerically bigger than to (X's last item) -- from alone
   // can still individually pass both the >= 0 and < B.get_rank() checks
   // (they were never cross-checked against each other), so the very
   // first read below (a=0, using from itself) can still be out of range.
   //
   if (from > to)
      {
        MORE_ERROR() << where << ": X = " << axes
                     << " is not ascending (expecting consecutive axes "
                     << (to + qio) << ".." << (from + qio)
                     << " listed low to high)";
        AXIS_ERROR;
      }

   // check that the axes are contiguous and compute the number of elements
   // in the combined axes
   //
ShapeItem count = 1;
   loop(a, axes.get_rank())
      {
        if (axes.get_shape_item(a) != (from + a))
           {
             MORE_ERROR() << where << ": X = " << axes
                          << " is not contiguous (expecting consecutive"
                             " axes " << (from + qio) << ".."
                          << (to + qio) << ")";
             AXIS_ERROR;
           }
        count *= B.get_shape_item(from + a);
      }

Shape shape_Z;
   loop (r, from)   shape_Z.add_shape_item(B.get_shape_item(r));
   shape_Z.add_shape_item(count);
   for (uRank r = to + 1; r < B.get_rank(); ++r)
       shape_Z.add_shape_item(B.get_shape_item(r));

   return ravel(shape_Z, B);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_COMMA::ravel(const Shape & new_shape, cValue_R B)
{
Value_P Z(new_shape, LOC);

const ShapeItem count = B.element_count();
   Assert(count == Z->element_count());

const ShapeItem ebytes = B.packed_bytes_per_item();
   if (ebytes > 0)   // packed non-bool: ravel order is preserved, copy directly
      {
        uint8_t * pZ = reinterpret_cast<uint8_t *>(&Z->get_wfirst());
        memcpy(pZ, B.cravel_packed(), count * ebytes);
        Z->commit_ravel_like(B, count);
      }
   else
      {
        loop(c, count)
            {
              Cell cache;
              Z->next_ravel_Cell(B.get_cravel(c, cache));
            }
        Z->pack_like(B);
      }

   Z->set_default(B, LOC);
   if (B.is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_COMMA::catenate(const char * where, const cValue & A, sAxis axis,
                    const cValue & B)
{
   // NOTE: the case A.is_scalar() && B.is_scalar() was supposedly ruled out
   //       before calling catenate()

   if (A.is_scalar())
      {
        Cell cache;
        const Cell & cell_A = A.get_cfirst(cache);
        Value_P Z = prepend_scalar(cell_A, axis, B);
        Z->check_value(LOC);
        return Z;
      }

   if (B.is_scalar())
      {
        Cell cache;
        const Cell & cell_B = B.get_cfirst(cache);
        Value_P Z = append_scalar(A, axis, cell_B);
        Z->check_value(LOC);
        return Z;
      }

   if ((A.get_rank() + 1) == B.get_rank())
      {
        Shape shape_Z;

        // check shape conformance.
        {
          ShapeItem ra = 0;
          loop(rb, B.get_rank())
             {
               if (rb != axis)
                  {
                    shape_Z.add_shape_item(B.get_shape_item(rb));
                    if (A.get_shape_item(ra) != B.get_shape_item(rb))
                       {
                         MORE_ERROR() << where << ": expecting ⍴A = ⍴B (with"
                                         " axis " << (axis + Workspace::get_IO())
                                      << " of B removed); ⍴A is "
                                      << A.get_shape() << ", ⍴B is "
                                      << B.get_shape();
                         LENGTH_ERROR;
                       }
                    ++ra;
                  }
               else
                  {
                    shape_Z.add_shape_item(B.get_shape_item(rb) + 1);
                  }
             }
        }

        Value_P Z(shape_Z, LOC);

        Z->set_default(B, LOC);

        const Shape3 shape_B3(B.get_shape(), axis);
        const ShapeItem slice_a = shape_B3.l();
        const ShapeItem slice_b = shape_B3.l() * B.get_shape_item(axis);

        ShapeItem idxA = 0;
        ShapeItem idxB = 0;

        loop(hz, shape_B3.h())
            {
              Cell::copy(*Z.get(), A, idxA, slice_a);
              Cell::copy(*Z.get(), B, idxB, slice_b);
            }

        Z->check_value(LOC);
        return Z;
      }

   if (A.get_rank() == (B.get_rank() + 1))
      {
        // e.g.	        ∆∆∆ , 3
        //              ∆∆∆   4
        //
        Shape shape_Z;

        // check shape conformance. We step ranks ra and rb, except for the
        // axis where only ra is incremented.
        {
          uint32_t rb = 0;
          loop(ra, A.get_rank())
             {
               if (ra != axis)
                  {
                    if (A.get_shape_item(ra) != B.get_shape_item(rb))
                       {
                         MORE_ERROR() << where << ": expecting ⍴A = ⍴B (with"
                                         " axis " << (axis + Workspace::get_IO())
                                      << " of A removed); ⍴A is "
                                      << A.get_shape() << ", ⍴B is "
                                      << B.get_shape();
                         LENGTH_ERROR;
                       }

                    shape_Z.add_shape_item(A.get_shape_item(ra));
                    ++rb;
                  }
               else
                  {
                    shape_Z.add_shape_item(A.get_shape_item(ra) + 1);
                  }
             }
        }

        Value_P Z(shape_Z, LOC);

        const Shape3 shape_A3(A.get_shape(), axis);
        const ShapeItem slice_a = shape_A3.l() * A.get_shape_item(axis);
        const ShapeItem slice_b = shape_A3.l();

        ShapeItem idxA = 0;
        ShapeItem idxB = 0;

        loop (hz, shape_A3.h())
            {
              Cell::copy(*Z.get(), A, idxA, slice_a);
              Cell::copy(*Z.get(), B, idxB, slice_b);
            }

        Z->set_default(B, LOC);

        Z->check_value(LOC);
        return Z;
      }
   if (A.get_rank() != B.get_rank())
      {
        MORE_ERROR() << where << ": expecting ⍴⍴A = ⍴⍴B (or A or B a"
                        " scalar); ⍴⍴A is " << A.get_rank() << ", ⍴⍴B is "
                     << B.get_rank();
        RANK_ERROR;
      }

   // A and B have the same rank. The shapes need to agree, except for the
   // dimension corresponding to the axis.
   //
Shape shape_Z;

   loop(r, A.get_rank())
       {
         if (r != axis)
            {
              if (A.get_shape_item(r) != B.get_shape_item(r))
                 {
                   MORE_ERROR() << where << ": expecting ⍴A = ⍴B (except at"
                                   " axis " << (axis + Workspace::get_IO())
                                << "); ⍴A is " << A.get_shape()
                                << ", ⍴B is " << B.get_shape();
                   LENGTH_ERROR;
                 }

              shape_Z.add_shape_item(A.get_shape_item(r));
            }
         else
            {
              shape_Z.add_shape_item(A.get_shape_item(r) +
                                   + B.get_shape_item(r));
            }
       }

const Shape3 shape_A3(A.get_shape(), axis);

const ShapeItem slice_a = shape_A3.l() * A.get_shape_item(axis);
const ShapeItem slice_b = shape_A3.l() * B.get_shape_item(axis);

const RavelType rtype = A.get_ravel_type();
const ShapeItem bpi   = A.packed_bytes_per_item();   // 0 for CELLS or BOOL
if (bpi > 0 && rtype == B.get_ravel_type())
   {
     // same packed type: replace Cell::copy loop with memcpy slices
     const char * pA = static_cast<const char *>(A.cravel_packed());
     const char * pB = static_cast<const char *>(B.cravel_packed());
     Value_P Z(shape_Z, LOC);
     char * pZ = reinterpret_cast<char *>(&Z->get_wfirst());
     ShapeItem offZ = 0;
     const ShapeItem bytes_a = slice_a * bpi;
     const ShapeItem bytes_b = slice_b * bpi;
     loop(hz, shape_A3.h())
         {
           memcpy(pZ + offZ, pA + hz * bytes_a, bytes_a);   offZ += bytes_a;
           memcpy(pZ + offZ, pB + hz * bytes_b, bytes_b);   offZ += bytes_b;
         }
     Z->commit_ravel_like(A, shape_A3.h() * (slice_a + slice_b));
     // ISO §10.2.1: if B is empty, Z's fill/prototype comes from A
     // instead, at rank >= 2 -- the vector case (rank 1) is
     // implementation-defined (ISO) and deliberately left as-is here
     // (testcases/Catenate.tc's '',⍳0 relies on the OLD B-wins
     // behavior). See Bugs27 #48.
     if (B.is_empty() && A.get_rank() >= 2)   Z->set_default(A, LOC);
     else                                     Z->set_default(B, LOC);
     Z->check_value(LOC);
     return Z;
   }

ShapeItem idxA = 0;
ShapeItem idxB = 0;

Value_P Z(shape_Z, LOC);
   loop(hz, shape_A3.h())
       {
         Cell::copy(*Z.get(), A, idxA, slice_a);
         Cell::copy(*Z.get(), B, idxB, slice_b);
       }

   if (B.is_empty() && A.get_rank() >= 2)   Z->set_default(A, LOC);
   else                                     Z->set_default(B, LOC);
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_COMMA::laminate(const char * where, const cValue & A, sAxis axis,
                    const cValue & B)
{
   // shapes of A and B must be the same, unless one of them is a scalar.
   //
   if (!A.is_scalar() && !B.is_scalar())
      {
        if (A.get_rank() != B.get_rank())
           {
             MORE_ERROR() << where << ": expecting ⍴⍴A = ⍴⍴B (or A or B a"
                             " scalar); ⍴⍴A is " << A.get_rank()
                          << ", ⍴⍴B is " << B.get_rank();
             RANK_ERROR;
           }
        if (A.get_shape() != B.get_shape())
           {
             MORE_ERROR() << where << ": expecting ⍴A = ⍴B (or A or B a"
                             " scalar); ⍴A is " << A.get_shape()
                          << ", ⍴B is " << B.get_shape();
             LENGTH_ERROR;
           }
      }

const Shape shape_Z = A.is_scalar() ? B.get_shape().insert_axis(axis, 2)
                                    : A.get_shape().insert_axis(axis, 2);

Value_P Z(shape_Z, LOC);

const Shape3 shape_Z3(shape_Z, axis);
   if (shape_Z3.m() != 2)
      {
        MORE_ERROR() << where << ": X is not a valid axis to laminate at";
        AXIS_ERROR;
      }

ShapeItem idxA = 0;
ShapeItem idxB = 0;
   if (A.is_scalar())
      {
        if (B.is_scalar())
           {
             Cell cache_A, cache_B;
             Z->next_ravel_Cell(A.get_cfirst(cache_A));
             Z->next_ravel_Cell(B.get_cfirst(cache_B));
           }
        else
           {
             loop(h, shape_Z3.h())
                 {
                   loop(l, shape_Z3.l())
                       {
                         Cell cache;
                         Z->next_ravel_Cell(A.get_cfirst(cache));
                       }
                   Cell::copy(*Z.get(), B, idxB, shape_Z3.l());
                }
           }
      }
   else
      {
        if (B.is_scalar())
           {
             loop(h, shape_Z3.h())
                 {
                   Cell::copy(*Z.get(), A, idxA, shape_Z3.l());
                   loop(l, shape_Z3.l())
                       {
                         Cell cache;
                         Z->next_ravel_Cell(B.get_cfirst(cache));
                       }
                }
           }
        else
           {
             loop(h, shape_Z3.h())
                 {
                   Cell::copy(*Z.get(), A, idxA, shape_Z3.l());
                   Cell::copy(*Z.get(), B, idxB, shape_Z3.l());
                }
           }
      }

   // ISO §10.2.1: if B is empty, Z's fill/prototype comes from A instead,
   // at rank >= 2 -- same rule (and same rank 1 carve-out) as catenate's
   // own fix above, applied here since laminate had it missing entirely
   // (Blake McBride, Bugs28 #66, Bugs27 #48 residual).
   //
   if (B.is_empty() && A.get_rank() >= 2)   Z->set_default(A, LOC);
   else                                     Z->set_default(B, LOC);

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_COMMA::catenate_or_laminate(const char * where, const cValue & A,
                                const cValue & X, const cValue & B)
{
   // catenate or laminate
   //
   if (!X.is_scalar_or_len1_vector())
      {
        MORE_ERROR() << where << ": X must be a scalar or 1-item vector";
        AXIS_ERROR;
      }

Cell cache;
const Cell & cX = X.get_cfirst(cache);
const APL_Integer qio = Workspace::get_IO();

   if (cX.is_near_int())   // catenate along existing axis
      {
        // catenate genuinely needs two conformable arrays sharing an
        // existing axis, unlike laminate (below), which forms a new
        // length-2 axis and legitimately accepts two scalars (APL2
        // LRM p.169 "If both arguments are scalars, L,[X]R ←→ L,R";
        // ISO §10.2.1). catenate() itself (see its own leading
        // comment) relies on this having already been ruled out.
        // Moved here, out of the shared code above, since it used to
        // reject two-scalar LAMINATE too (e.g. 1,[0.5]2). See Bugs27
        // #47.
        //
        if (A.is_scalar() && B.is_scalar())
           {
             MORE_ERROR() << where << ": A and B are both scalars; at least"
                             " one must be a non-scalar (catenate, not"
                             " laminate, since X is an existing/integral"
                             " axis)";
             RANK_ERROR;
           }

        // validate in the full-width APL_Integer before narrowing to
        // sAxis (int16_t): narrowing first let a huge axis wrap into
        // the valid range instead of being rejected (M,[65537]M
        // silently catenated along axis 0 -- 65537 wrapped to 1, minus
        // ⎕IO -- instead of raising AXIS_ERROR).
        const APL_Integer wide_axis = cX.get_checked_near_int() - qio;
        const ShapeItem max_rank = A.get_rank() > B.get_rank()
                                  ? A.get_rank() : B.get_rank();
        if (wide_axis < 0 || wide_axis >= max_rank)
           {
             MORE_ERROR() << where << ": X = " << (wide_axis + qio)
                          << " is not a valid axis (expecting " << qio
                          << "≤X<" << (max_rank + qio) << ")"
                          << ArgCheck::index_io0_note(wide_axis, max_rank);
             AXIS_ERROR;
           }
        const sAxis axis = wide_axis;
        return catenate(where, A, axis, B);
      }

const APL_Float axis = cX.get_real_value() - qio;
   {
     const APL_Float max_rank = A.get_rank() > B.get_rank()
                               ? A.get_rank() : B.get_rank();
     if (axis <= -1.0 || axis >= (max_rank + 1.0))
        {
          MORE_ERROR() << where << ": X = " << (axis + qio)
                       << " is not a valid (fractional) axis (expecting "
                       << (qio - 1) << "<X<" << (max_rank + qio + 1.0) << ")";
          AXIS_ERROR;
        }
   }
   return laminate(where, A, sAxis(axis + 1.0), B);
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Bif_COMMA::prepend_scalar(const Cell & cell_A, uAxis axis, const cValue & B)
{
   if (B.is_empty())
      {
        Shape shape_Z = B.get_shape();
        shape_Z.set_shape_item(axis, shape_Z.get_shape_item(axis) + 1);
        Value_P Z(shape_Z, LOC);
        if (Z->is_empty())
           {
              Z->set_default(cell_A, LOC);   // prototype
           }
        else
           {
             loop(z, Z->element_count())   Z->next_ravel_Cell(cell_A);
           }
        return Z;
      }

   if (B.is_scalar())
      {
        Value_P Z(2, LOC);
        Z->next_ravel_Cell(cell_A);
        Cell cache;
        Z->next_ravel_Cell(B.get_cfirst(cache));
        Z->check_value(LOC);
        return Z;
      }

   if (axis >= B.get_rank())   INDEX_ERROR;

Shape shape_Z(B.get_shape());
   shape_Z.set_shape_item(axis, shape_Z.get_shape_item(axis) + 1);

Value_P Z(shape_Z, LOC);

const Shape3 shape_B3(B.get_shape(), axis);
   const ShapeItem slice_a = shape_B3.l();
   const ShapeItem slice_b = shape_B3.l() * B.get_shape_item(axis);

ShapeItem idxB = 0;

   loop(hz, shape_B3.h())
       {
         loop(lz, slice_a)   Z->next_ravel_Cell(cell_A);

         Cell::copy(*Z.get(), B, idxB, slice_b);
       }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_COMMA::append_scalar(const cValue & A, uAxis axis, const Cell & cell_B)
{
   if (A.is_empty())
      {
        Shape shape_Z = A.get_shape();
        shape_Z.set_shape_item(axis, shape_Z.get_shape_item(axis) + 1);
        Value_P Z(shape_Z, LOC);
        if (Z->is_empty())
           {
              Z->set_default(cell_B, LOC);   // prototype
           }
        else
           {
             loop(z, Z->element_count())   Z->next_ravel_Cell(cell_B);
           }
        return Z;
      }
   // A.is_scalar() is handled by prepend_scalar()

   if (axis >= A.get_rank())   INDEX_ERROR;

Shape shape_Z(A.get_shape());
   shape_Z.set_shape_item(axis, shape_Z.get_shape_item(axis) + 1);

Value_P Z(shape_Z, LOC);

const Shape3 shape_A3(A.get_shape(), axis);
const ShapeItem slice_a = shape_A3.l() * A.get_shape_item(axis);
const ShapeItem slice_b = shape_A3.l();

ShapeItem idxA = 0;

   loop(hz, shape_A3.h())
       {
         Cell::copy(*Z.get(), A, idxA, slice_a);
         loop(lz, slice_b)   Z->next_ravel_Cell(cell_B);
       }

   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_COMMA::eval_AB(cValue_R A, cValue_R B) const
{
  if (A.is_scalar() && B.is_scalar())
     {
       Value_P Z(2, LOC);
       Cell cache_A, cache_B;
       Z->next_ravel_Cell(A.get_cscalar(cache_A));
       Z->next_ravel_Cell(B.get_cscalar(cache_B));
       Z->check_value(LOC);
       return Token(TOK_APL_VALUE1, Z);
     }

uRank max_rank = A.get_rank();
   if (max_rank < B.get_rank())  max_rank = B.get_rank();
   return Token(TOK_APL_VALUE1, catenate("A,B", A, max_rank-1, B));
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_COMMA::eval_B(cValue_R B) const
{
const Shape shape_Z(B.element_count());

   if (DO_RT_COMMA_B              &&
       B.get_owner_count() == 1  &&
       this == Workspace::SI_top()->get_prefix().get_monadic_fun())
      {
        Log(LOG_optimization)
           CERR << "optimizing ,B (len="
                << B.nz_element_count() << ")" << endl;

        OptmizationStatistics::count(OPTI_RT_COMMA_B);

        static_cast<Value *>(const_cast<cValue *>(&B))->set_shape(shape_Z);
        return Token(TOK_APL_VALUE1, CLONE(&B, LOC));
      }

   return ravel(shape_Z, B);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_COMMA1::eval_B(cValue_R B) const
{
   // turn B into a matrix
   //
ShapeItem c1 = 1;   // assume B is scalar
ShapeItem c2 = 1;   // assume B is scalar;

   if (B.get_rank() >= 1)
      {
        c1 = B.get_shape_item(0);
        for (uRank r = 1; r < B.get_rank(); ++r)   c2 *= B.get_shape_item(r);
      }

Shape shape_Z(c1, c2);
   if (DO_RT_COMMA1_B            &&
       B.get_owner_count() == 1 &&
       this == Workspace::SI_top()->get_prefix().get_monadic_fun())
      {
        Log(LOG_optimization) CERR << "optimizing ,B" << endl;

        OptmizationStatistics::count(OPTI_RT_COMMA1_B);

        static_cast<Value *>(const_cast<cValue *>(&B))->set_shape(shape_Z);
        return Token(TOK_APL_VALUE1, CLONE(&B, LOC));
      }
   return ravel(shape_Z, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_COMMA1::eval_AB(cValue_R A, cValue_R B) const
{
  if (A.is_scalar() && B.is_scalar())
     {
       Value_P Z(2, LOC);
       Cell cache_A, cache_B;
       Z->next_ravel_Cell(A.get_cfirst(cache_A));
       Z->next_ravel_Cell(B.get_cfirst(cache_B));
       Z->check_value(LOC);
       return Token(TOK_APL_VALUE1, Z);
     }

   return Token(TOK_APL_VALUE1, catenate("A⍪B", A, 0, B));
}
//════════════════════════════════════════════════════════════════════════════

