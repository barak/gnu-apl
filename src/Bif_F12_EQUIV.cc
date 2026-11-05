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

#include "Bif_F12_EQUIV.hh"
#include "ConstCell_P.hh"
#include "Value.hh"
#include "Workspace.hh"

// primitive function instances
//
Bif_F12_EQUIV     Bif_F12_EQUIV    ::fun;    // ≡
Bif_F12_NEQUIV    Bif_F12_NEQUIV   ::fun;    // ≢

//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_EQUIV::eval_B(cValue_R B) const
{
const APL_Integer depth = B.compute_depth();

Value_P Z = IntScalar(depth, LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
bool
Bif_F12_EQUIV::do_eval_AB(cValue_R A, cValue_R B)
{
   // match
   //
const double qct = Workspace::get_CT();

   if (!A.same_shape(B))   return false;   // shape mismatch

   for (ConstRavel_P a(A, true), b(B, true); +a; ++a, ++b)
       {
         if (!a->equal(*b, qct))   return false;   // no match
       }

   return true;   // match
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_NEQUIV::eval_AB(cValue_R A, cValue_R B) const
{
   // A ≢ B aka.  ∼A ≡ B
   //
const double qct = Workspace::get_CT();

   if (!A.same_shape(B))   // shape mismatch
      return Token(TOK_APL_VALUE1, IntScalar(1, LOC));

   for (ConstRavel_P a(A, true), b(B, true); +a; ++a, ++b)
       {
         if (!a->equal(*b, qct))
            {
              return Token(TOK_APL_VALUE1, IntScalar(1, LOC));   // no match
            }
       }

   return Token(TOK_APL_VALUE1, IntScalar(0, LOC));   // match
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_NEQUIV::eval_B(cValue_R B) const
{
   // Tally
   //
const ShapeItem len = B.is_scalar() ? 1 : B.get_shape().get_shape_item(0);

   return Token(TOK_APL_VALUE1, IntScalar(len, LOC));   // match
}
//════════════════════════════════════════════════════════════════════════════
