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

#include "Bif_F2_LEFT_RIGHT.hh"
#include "Value.hh"
#include "Workspace.hh"

// primitive function instances
//
Bif_F2_LEFT       Bif_F2_LEFT      ::fun;    // ⊣
Bif_F2_RIGHT      Bif_F2_RIGHT     ::fun;    // ⊢

//════════════════════════════════════════════════════════════════════════════
Token
Bif_F2_RIGHT::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   // Z ← A ⊢[X] B: select corresponding items of A or B according to X.
   //  A, B, and X must have matching shapes
   //
const int inc_A = A.is_scalar_extensible() ? 0 : 1;
const int inc_B = B.is_scalar_extensible() ? 0 : 1;
const int inc_X = X.is_scalar_extensible() ? 0 : 1;

   if (inc_X == 0)   // single item X: pick entire A or B according to X
      {
        const APL_Integer x0 = X.get_int_value(0);
        if (x0 == 0)   return Token(TOK_APL_VALUE1, CLONE(&A, LOC));
        if (x0 == 1)   return Token(TOK_APL_VALUE1, CLONE(&B, LOC));
        MORE_ERROR() << "A⊢[X]B: X = " << x0
                     << " is not Boolean (expecting 0 or 1)";
        DOMAIN_ERROR;
      }

   // X is non-scalar, so it must match any non-scalar A and B
   //
   if (inc_A && ! X.same_shape(A))
      {
        if (A.get_rank() != X.get_rank())
           {
             MORE_ERROR() << "A⊢[X]B: expecting ⍴⍴A = ⍴⍴X; ⍴⍴A is "
                          << A.get_rank() << ", ⍴⍴X is " << X.get_rank();
             RANK_ERROR;
           }
        else
           {
             MORE_ERROR() << "A⊢[X]B: expecting ⍴A = ⍴X; ⍴A is "
                          << A.get_shape() << ", ⍴X is " << X.get_shape();
             LENGTH_ERROR;
           }
      }

   if (inc_B && ! X.same_shape(B))
      {
        if (B.get_rank() != X.get_rank())
           {
             MORE_ERROR() << "A⊢[X]B: expecting ⍴⍴B = ⍴⍴X; ⍴⍴B is "
                          << B.get_rank() << ", ⍴⍴X is " << X.get_rank();
             RANK_ERROR;
           }
        else
           {
             MORE_ERROR() << "A⊢[X]B: expecting ⍴B = ⍴X; ⍴B is "
                          << B.get_shape() << ", ⍴X is " << X.get_shape();
             LENGTH_ERROR;
           }
      }

const Shape * shape_Z = &A.get_shape();   // last resort if X and B are scalar
   if (inc_X)        shape_Z = &X.get_shape();
   else if (inc_B)   shape_Z = &B.get_shape();

Value_P Z(*shape_Z, LOC);
Cell cache;
   loop(z, shape_Z->get_volume())
       {
        const APL_Integer xz = X.get_int_value(z*inc_X);   // X[z]
        if (xz == 0)        // take A[z]
           Z->next_ravel_Cell(A.get_cravel(z*inc_A, cache));
        else if (xz == 1)   // take B[z]
           Z->next_ravel_Cell(B.get_cravel(z*inc_B, cache));
        else
           {
             MORE_ERROR() << "A⊢[X]B: X[" << (z + Workspace::get_IO())
                          << "] = " << xz
                          << " is not Boolean (expecting 0 or 1)";
             DOMAIN_ERROR;
           }
       }

   Z->set_default(B, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
