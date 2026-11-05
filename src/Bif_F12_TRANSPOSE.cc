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
#include "ArrayIterator.hh"
#include "Bif_F12_TRANSPOSE.hh"
#include "Value.hh"
#include "Workspace.hh"

// primitive function instance
//
Bif_F12_TRANSPOSE Bif_F12_TRANSPOSE::fun;    // ⍉

//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_TRANSPOSE::eval_AB(cValue_R A, cValue_R B) const
{
   // A should be a scalar or vector.
   //
   ArgCheck::require_scalar_or_vector("A⍉B", "A", A);

const Shape shape_A(A, Workspace::get_IO());   // rank(shape_A) = length(A)
   if (shape_A.get_rank() != B.get_rank())
      {
        MORE_ERROR() << "A⍉B: expecting ⍴⍴B = " << shape_A.get_rank()
                     << " (⍴A); ⍴⍴B is " << B.get_rank();
        LENGTH_ERROR;
      }

   if (B.is_scalar())   // B is a scalar (so A should be empty)
      {
        Value_P Z(static_cast<Value *>(const_cast<cValue *>(&B)), LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // shape_A is normalized to ⎕IO←- and shall only contain valid axes of B.
const APL_Integer qio = Workspace::get_IO();
   loop(r, shape_A.get_rank())
      {
        const ShapeItem ar = shape_A.get_shape_item(r);
        if (ar < 0 || ar >= B.get_rank())
           {
             MORE_ERROR() << "A⍉B: A[" << (r + qio) << "] = " << (ar + qio)
                          << " is not a valid axis of B (expecting "
                          << qio << "≤A[" << (r + qio) << "]<"
                          << (B.get_rank() + qio) << ")"
                          << ArgCheck::index_io0_note(ar, B.get_rank());
             DOMAIN_ERROR;
           }
      }

Value_P Z = shape_A.get_rank() == B.get_rank() && shape_A.is_permutation()
          ? transpose(shape_A, B)
          : transpose_diag(shape_A, B);

   // Z is not always a freshly constructed (still default-prototyped)
   // Value here: transpose()'s rank<=1 and identity-matrix fast paths
   // return CLONE(&B, LOC), which already carries B's own (possibly
   // non-integer) prototype, and both transpose()'s and
   // transpose_diag()'s empty-result branches already call set_default()
   // themselves before returning. Calling set_default() unconditionally
   // on such an already-prototyped Z violates its "by constructor"
   // precondition (found via a real fuzzer crash on an empty B: the
   // assertion this->get_cproto().is_integer_cell() failed because Z,
   // a clone of an already char/nested-prototyped empty B, was no
   // longer in its fresh int-prototype state). Only call it when Z is
   // both empty and still genuinely unprototyped.
   if (Z->is_empty())
      {
        Cell cache;
        if (Z->get_cproto(cache).is_integer_cell())   Z->set_default(B, LOC);
      }

   if (B.is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_TRANSPOSE::do_eval_B(cValue_R B)
{
   // monadic transpose is A⍉B with A = ... 4 3 2 1 0
   //
Shape shape_A;
   loop(r, B.get_rank())   shape_A.add_shape_item(B.get_rank() - r - 1);

Value_P Z = transpose(shape_A, B);

   // see the matching comment in eval_AB() above: transpose()'s rank<=1
   // fast path returns CLONE(&B, LOC) -- not a copy -- so an unconditional
   // set_default() here would overwrite B's own already-live prototype
   // (Bugs18 #5).
   //
   if (Z->is_empty())
      {
        Cell cache;
        if (Z->get_cproto(cache).is_integer_cell())   Z->set_default(B, LOC);
      }

   if (B.is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_F12_TRANSPOSE::transpose(const Shape & sh_A, cValue_R B)
{
   // some frequent and simple to optimize cases beforehand...
   //
   if (sh_A.get_rank() <= 2)   // transpose matrix or vectors
      {
        if (sh_A.get_rank() <= 1)   // scalar or vector B:
           {
             // Bugs31 #19/#23 (Blake McBride): CLONE() aliases B rather
             // than copying it, so any lvalue pass-through marker on B
             // would otherwise survive this identity application
             // unbroken -- see Value::clear_lval_markers().
             //
             Value_P Z = CLONE(&B, LOC);
             Z->clear_lval_markers();
             return Z;
           }

        // 2-dimensional matrix (probably the most frequent case).
        //
        if (sh_A.get_shape_item(0) == 0 &&
            sh_A.get_shape_item(1) == 1)   // identity
           {
             Value_P Z = CLONE(&B, LOC);
             Z->clear_lval_markers();
             return Z;
           }

        const ShapeItem rows_B = B.get_shape_item(0);
        const ShapeItem cols_B = B.get_shape_item(1);
        const Shape shape_Z(cols_B, rows_B);
        Value_P Z(shape_Z, LOC);

        const ShapeItem bpi = B.packed_bytes_per_item();
        if (bpi > 0)   // packed non-bool: stride copy
           {
             const uint8_t * pB = static_cast<const uint8_t *>(B.cravel_packed());
             uint8_t * pZ = reinterpret_cast<uint8_t *>(&Z->get_wfirst());
             loop(rZ, cols_B)
             loop(cZ, rows_B)
                 memcpy(pZ + (rZ * rows_B + cZ) * bpi,
                        pB + (cZ * cols_B + rZ) * bpi, bpi);
             Z->commit_ravel_like(B, cols_B * rows_B);
           }
        else
           {
             loop(rZ, cols_B)   // the rows of B are columns of Z
             loop(cZ, rows_B)   // the columns of B are rows of Z
                 {
                   Cell cache;
                   Z->next_ravel_Cell(B.get_cravel(rZ + cZ*cols_B, cache));
                 }
           }
        Z->check_value(LOC);
        return Z;
      }

   /*
      Z←A ⍉ B could be reasonably implemented in 2 ways

      1. loop over (source) B and ArrayIterator for (destination) Z, or
      2. ArrayIterator for (source) B and loop over (destination) Z.

      sh_A specifies an axis permutation in the "forward" direction, i.e.

      B[i] → Z[A[i]]   for all indices i.

      which implies alternative 1. above. However, Z->check_value() below
      takes O(length(Z)) in alternative 1. above but only O(2) in
      alternative 2. above because then next_ravel_Cell() can be used.

      For alternative 2. we used the inverse mapping:

      Z[i] ← B[A⁻¹[i]]   for all indices i.

      which is achieved by using the inverse permutation for sh_A (which
      exchanges the source and the destination).

      The caller is supposed to normalized sh_A to to ⎕IO←0, therefore
      the shape items of sh_A are the axes 0, 1, ... (in some order).
    */

const Shape   shape_inv_A = inverse_permutation(sh_A);
const Shape & shape_B     = B.get_shape();
const Shape   shape_Z     = permute(shape_B, shape_inv_A);

Value_P Z(shape_Z, LOC);

   if (shape_Z.is_empty())
      {
         Z->set_default(B, LOC);
         Z->check_value(LOC);
         return Z;
      }

   for (ArrayIterator b(shape_Z, sh_A); b.has_more(); ++b)
       {
         Cell cache;
         Z->next_ravel_Cell(B.get_cravel(b.get_ravel_offset(), cache));
       }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Shape
Bif_F12_TRANSPOSE::inverse_permutation(const Shape & perm)
{
   // perm is a permutation of ⎕IO + 0, 1, ...
   // return the inverse permutaion of perm.

ShapeItem rho[MAX_RANK];

   // 1. set all items to -1.
   //
   loop(r, perm.get_rank())   rho[r] = -1;

   // 2. set all items to the shape items of perm
   //
   loop(a, perm.get_rank())
       {
         const ShapeItem ax = perm.get_shape_item(a);
         const APL_Integer qio = Workspace::get_IO();
         if (ax < 0 || ax >= perm.get_rank())
            {
              MORE_ERROR() << "A⍉B: A[" << (a + qio) << "] = " << (ax + qio)
                           << " is not a valid axis (expecting " << qio
                           << "≤A[" << (a + qio) << "]<"
                           << (perm.get_rank() + qio) << ")"
                           << ArgCheck::index_io0_note(ax, perm.get_rank());
              AXIS_ERROR;
            }
         if (rho[ax] != -1)
            {
              MORE_ERROR() << "A⍉B: A[" << (a + qio) << "] = " << (ax + qio)
                           << " is a duplicate axis";
              AXIS_ERROR;
            }

         // everything OK
         //
         rho[ax] = a;
       }

   return Shape(perm.get_rank(), rho);
}
//────────────────────────────────────────────────────────────────────────────
Shape
Bif_F12_TRANSPOSE::permute(const Shape & sh, const Shape & perm)
{
   /* permute sh according to perm.

      perm is a shape specifying a permutation like this:

      0 → perm[0]
      1 → perm[1]
      ...

      the result ret is the permuted shape sh:

     ret[0] = sh[perm[0]]
     ret[1] = sh[perm[1]]
     ...

    */
Shape ret;

   loop(r, perm.get_rank())
      {
        ret.add_shape_item(sh.get_shape_item(perm.get_shape_item(r)));
      }

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Bif_F12_TRANSPOSE::transpose_diag(const Shape & sh_A, cValue_R B)
{
   // A⍉B with repeated items in A. The caller has normalized sh_A to ⎕IO←0.

   // 1. compute rank_Z ← 1 + ⌈/sh_A.
   //
ShapeItem rank_Z = 0;
const Shape & sh_B = B.get_shape();
   loop(a, sh_A.get_rank())
       {
         const ShapeItem s_A = sh_A.get_shape_item(a);
         if (rank_Z < s_A)   rank_Z = s_A;
       }
   ++rank_Z;

   // compute the B-weights of the untransposed B axes
   //
const Shape weights_B = sh_B.get_weights();

   // compute the B-weights of the transposed B axes. transpose_diag() is
   // called rarely, and if so with small ranks. We can therefore afford
   // a rank_Z² algorithm. We also create shape_Z as we go.
   //
   // A unit step in Z[z] is a weight_Z[z] step in B, where weight_Z is the
   // sum of weights that map to z.
   //
Shape shape_Z;
ShapeItem * weight_Z = ALLOCA(ShapeItem, rank_Z);
   loop(z, rank_Z)
       {
         weight_Z[z] = 0;
         ShapeItem min_len_B = LARGE_INT;
         loop(a, sh_A.get_rank())
             {
               if (z == sh_A.get_shape_item(a))   // B-axis a maps to Z-axis z
                  {
                    const ShapeItem len_b = sh_B.get_shape_item(a);
                    if (min_len_B > len_b)   min_len_B = len_b;
                    weight_Z[z] += weights_B.get_shape_item(a);
                  }
             }

         if (weight_Z[z] == 0)
            {
              /* weight_Z[z] == 0 is rather unlikely and may have 2 reasons:

                 1. none of the A items is z (which raises DOMAIN ERROR), or
                 2. all B axes for z are 0.(which continues below).
               */
              bool z_in_A = false;
              loop(a, sh_A.get_rank())
                  {
                    if (sh_A.get_shape_item(a) == z)   // hence not case 1.
                       {
                         z_in_A = true;
                         break;   // no need to search further
                       }
                  }

              if (!z_in_A)   // z is missing in A
                 {
                   const APL_Integer qio = Workspace::get_IO();
                   MORE_ERROR() << "A⍉B: axis " << (qio + z)
                                << " is missing in A; "
                                   "A should contain only integers " << qio
                                << "..." << (qio + rank_Z) << "."
                                << (qio == 0 ? " Note: ⎕IO=0." : "");
                   DOMAIN_ERROR;
                 }
            }
         shape_Z.add_shape_item(min_len_B);
       }

Value_P Z(shape_Z, LOC);
   if (Z->is_empty())
      {
         Z->set_default(B, LOC);
        return Z;
      }

   for (ArrayIterator iZ(shape_Z); iZ.has_more(); ++iZ)
       {
         ShapeItem idxB = 0;
         loop(z, rank_Z)   idxB += iZ.get_shape_offset(z) * weight_Z[z];
         Cell cache;
         Z->next_ravel_Cell(B.get_cravel(idxB, cache));
       }

   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
