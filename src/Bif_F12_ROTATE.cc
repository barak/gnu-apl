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

#include "Bif_F12_ROTATE.hh"
#include "Shape.hh"
#include "Value.hh"

// primitive function instances
//
Bif_F12_ROTATE    Bif_F12_ROTATE   ::fun;    // ⌽
Bif_F12_ROTATE1   Bif_F12_ROTATE1  ::fun;    // ⊖

//════════════════════════════════════════════════════════════════════════════
Token
Bif_ROTATE::reverse(cValue_R B, sAxis axis)
{
   if (B.is_scalar())
      {
        // Bugs31 #19/#23 (Blake McBride): CLONE() aliases B rather than
        // copying it, so any lvalue pass-through marker on B (whole-
        // symbol, pick-slot, bare-member) would otherwise survive this
        // identity application unbroken -- see Value::clear_lval_markers().
        //
        Value_P Z = CLONE(&B, LOC);
        Z->clear_lval_markers();
        return Token(TOK_APL_VALUE1, Z);
      }

const Shape3 shape_B3(B.get_shape(), axis);

Value_P Z(B.get_shape(), LOC);

const ShapeItem ebytes = B.packed_bytes_per_item();
   if (ebytes > 0)   // packed non-bool: permute elements directly
      {
              uint8_t * pZ = reinterpret_cast<uint8_t *>(&Z->get_wfirst());
        const uint8_t * pB = reinterpret_cast<const uint8_t *>(B.cravel_packed());
        loop(h, shape_B3.h())
            {
              const ShapeItem plane = h * shape_B3.m() * shape_B3.l();
              loop(m, shape_B3.m())
                  {
                    const ShapeItem src = plane + (shape_B3.m() - m - 1) * shape_B3.l();
                    const ShapeItem dst = plane + m * shape_B3.l();
                    loop(l, shape_B3.l())
                        memcpy(pZ + (dst + l) * ebytes,
                               pB + (src + l) * ebytes, ebytes);
                  }
            }
        Z->commit_ravel_like(B, B.element_count());
      }
   else
      {
        loop(h, shape_B3.h())
            {
              const ShapeItem plane_h = h * shape_B3.m() * shape_B3.l();
              loop(m, shape_B3.m())
                  {
                    const ShapeItem col_m = plane_h
                                         + (shape_B3.l() * (shape_B3.m() - m - 1));
                    Cell cache;
                    loop(l, shape_B3.l())
                        Z->next_ravel_Cell(B.get_cravel(col_m + l, cache));
                  }
            }
        Z->pack_like(B);
      }

   Z->set_default(B, LOC);
   if (B.is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_ROTATE::rotate(cValue_R A, cValue_R B, sAxis axis)
{
// was int32_t: truncated a 64-bit rotate amount, and low-32-bits-zero
// (e.g. 4294967296 == 1<<32) even short-circuited the "nothing to do"
// case below. 4294967296⌽1 2 3 gave 1 2 3 unchanged instead of 2 3 1.
ShapeItem gsh = 0;   // global shift (scalar A); 0 means local shift (A) used.

const Shape3 shape_B3(B.get_shape(), axis);
const Shape shape_A2(shape_B3.h(), shape_B3.l());

   // LRM: a scalar or one-item vector L rotates all vectors along the
   // axis by L. GNU APL: any 1-element L (Bugs37 #66 scalar-extension rule).
   //
   if (A.is_scalar_extensible())
      {
        gsh = A.get_near_int(0);
        if (gsh == 0)   // nothing to do.
           {
             // Bugs31 #19/#23: see the scalar-B case above.
             //
             Value_P Z = CLONE(&B, LOC);
             Z->clear_lval_markers();
             Token result(TOK_APL_VALUE1, Z);
             return result;
           }
      }
   else   // otherwise shape A must be shape B with 'axis' removed.
      {
        A.get_shape().check_same(B.get_shape().without_axis(axis),
                                 E_RANK_ERROR, E_LENGTH_ERROR, LOC);

        // the items of A are otherwise only checked (by get_near_int())
        // in the loops below, which do not run if B is empty along the
        // axis (Bugs35 #29).
        //
        loop(a, A.element_count())   A.get_near_int(a);
      }


Value_P Z(B.get_shape(), LOC);

const ShapeItem ebytes = B.packed_bytes_per_item();
   if (ebytes > 0)   // packed non-bool: permute elements directly
      {
              uint8_t * pZ = reinterpret_cast<uint8_t *>(&Z->get_wfirst());
        const uint8_t * pB = reinterpret_cast<const uint8_t *>(B.cravel_packed());
        loop(h, shape_B3.h())
        loop(m, shape_B3.m())
        loop(l, shape_B3.l())
            {
              ShapeItem src = gsh;
              if (!src)   src = A.get_near_int(l + h*shape_B3.l());
              // reduce BEFORE adding m: src is the user's rotate amount
              // and may be any int64_t, so adding m first can overflow
              // (UB, wraps to a wrong offset) whenever src is within m
              // of INT64_MAX -- the same class of bug as the "+
              // shape_B3.m()" overflow fixed below for Bugs14 #8, just
              // one line earlier (Blake McBride, Bugs21 #2). Reducing
              // first keeps |src| < shape_B3.m() so "src += m" below
              // cannot overflow.
              src %= shape_B3.m();
              src += m;
              // normalize src into [0, shape_B3.m()) in O(1). A naive
              // while-loop here (adding/subtracting shape_B3.m() one
              // step at a time) hangs for huge rotate amounts (e.g.
              // A in the 10^11 range): O(|src|/m) iterations instead
              // of O(1). shape_B3.m() > 0 is guaranteed here since
              // this code only runs inside loop(m, shape_B3.m()).
              // The extra "+ shape_B3.m()" this normalization used to
              // add (needed by an older subtract-in-a-loop version) is
              // gone: it is redundant now that "% then fix up a
              // negative result" already normalizes correctly, and for
              // src near INT64_MAX it was exactly what overflowed
              // (signed overflow is UB and wrapped to a wrong rotation
              // amount -- Blake McBride, Bugs14 #8).
              src %= shape_B3.m();
              if (src < 0)   src += shape_B3.m();
              const ShapeItem dst = h * shape_B3.m() * shape_B3.l()
                                  + m * shape_B3.l() + l;
              memcpy(pZ + dst * ebytes,
                     pB + shape_B3.hml(h, src, l) * ebytes, ebytes);
            }
        Z->commit_ravel_like(B, B.element_count());
      }
   else
      {
        loop(h, shape_B3.h())
        loop(m, shape_B3.m())
        loop(l, shape_B3.l())
            {
              ShapeItem src = gsh;
              if (!src)   src = A.get_near_int(l + h*shape_B3.l());
              // reduce BEFORE adding m: src is the user's rotate amount
              // and may be any int64_t, so adding m first can overflow
              // (UB, wraps to a wrong offset) whenever src is within m
              // of INT64_MAX -- the same class of bug as the "+
              // shape_B3.m()" overflow fixed below for Bugs14 #8, just
              // one line earlier (Blake McBride, Bugs21 #2). Reducing
              // first keeps |src| < shape_B3.m() so "src += m" below
              // cannot overflow.
              src %= shape_B3.m();
              src += m;
              // normalize src into [0, shape_B3.m()) in O(1). A naive
              // while-loop here (adding/subtracting shape_B3.m() one
              // step at a time) hangs for huge rotate amounts (e.g.
              // A in the 10^11 range): O(|src|/m) iterations instead
              // of O(1). shape_B3.m() > 0 is guaranteed here since
              // this code only runs inside loop(m, shape_B3.m()).
              // The extra "+ shape_B3.m()" this normalization used to
              // add (needed by an older subtract-in-a-loop version) is
              // gone: it is redundant now that "% then fix up a
              // negative result" already normalizes correctly, and for
              // src near INT64_MAX it was exactly what overflowed
              // (signed overflow is UB and wrapped to a wrong rotation
              // amount -- Blake McBride, Bugs14 #8).
              src %= shape_B3.m();
              if (src < 0)   src += shape_B3.m();
              Cell cache;
              Z->next_ravel_Cell(B.get_cravel(shape_B3.hml(h, src, l), cache));
            }
        Z->pack_like(B);
      }

   Z->set_default(B, LOC);
   if (B.is_left_value())   Z->set_left_value();

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_ROTATE::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "A⌽[X]B");
   return rotate(A, B, axis);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_ROTATE::eval_XB(cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "⌽[X]B");
   return reverse(B, axis);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_ROTATE1::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "A⊖[X]B");
   return rotate(A, B, axis);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_ROTATE1::eval_XB(cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "⊖[X]B");
   return reverse(B, axis);
}
//════════════════════════════════════════════════════════════════════════════
