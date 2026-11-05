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

#include "ArgCheck.hh"
#include "Bif_F2_INDEX.hh"
#include "IndexExpr.hh"
#include "Value.hh"
#include "Workspace.hh"

// primitive function instance
//
Bif_F2_INDEX      Bif_F2_INDEX     ::fun;    // ⌷

//════════════════════════════════════════════════════════════════════════════
Token
Bif_F2_INDEX::eval_AB(cValue_R A, cValue_R B) const
{
   ArgCheck::require_scalar_or_vector("A⌷B", "A", A);

const ShapeItem ec_A = A.element_count();
   if (ec_A != B.get_rank())
      {
        MORE_ERROR() << "A⌷B: expecting ⍴A = ⍴⍴B; ⍴A is " << ec_A
                     << ", ⍴⍴B is " << B.get_rank();
        RANK_ERROR;
      }

   // index_expr is in reverse order!
   //
IndexExpr index_expr(ASS_none, LOC);
   loop(a, ec_A)
      {
        const ShapeItem idx = ec_A - a - 1;
         if (Value_P val0 = A.try_pointer_value(idx))
            {
              Value_P val = CLONE_P(val0, LOC);
              if (val->compute_depth() > 1)
                 {
                   MORE_ERROR() << "A⌷B: A[" << (idx + Workspace::get_IO())
                                << "] is nested too deeply (expecting a"
                                   " simple index or a simple vector of"
                                   " indices)";
                   DOMAIN_ERROR;
                 }
              index_expr.add_index(val);
            }
        else
            {
              const APL_Integer I = A.get_near_int(idx);
              if (I < 0)
                 {
                   MORE_ERROR() << "A⌷B: A["
                                << (idx + Workspace::get_IO())
                                << "] = " << I << " is negative";
                   DOMAIN_ERROR;
                 }
              index_expr.add_index(IntScalar(I, LOC));
            }
      }

   // the index() functions do set_default() and check_value(),
   // so we return immediately without doint it again.
   //
   index_expr.quad_io = Workspace::get_IO();

   if (index_expr.is_axis())   // [ ] or [ axis ]
      {
        Value_P single_index = index_expr.extract_axis();
        Value_P Z = B.index(*single_index);
        return Token(TOK_APL_VALUE1, Z);
      }
   else
      {
        Value_P Z = B.index(index_expr);
        return Token(TOK_APL_VALUE1, Z);
      }
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F2_INDEX::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   ArgCheck::require_scalar_or_vector("A⌷[X]B", "A", A);

const AxesBitmap axes_X = X.to_bitmap("⌷[X]B", B.get_rank());

const ShapeItem ec_A = A.element_count();
   if (ec_A != X.element_count())
      {
        MORE_ERROR() << "A⌷[X]B: expecting ⍴A = ⍴X; ⍴A is " << ec_A
                     << ", ⍴X is " << X.element_count();
        RANK_ERROR;
      }
   if (ec_A > B.get_rank())
      {
        MORE_ERROR() << "A⌷[X]B: expecting ⍴A ≤ ⍴⍴B; ⍴A is " << ec_A
                     << ", ⍴⍴B is " << B.get_rank();
        RANK_ERROR;
      }

   // axis_to_pos[axis] = the position in X (and, since ⍴A = ⍴X, in A
   // too) that names this 0-based axis of B -- axes_X above only
   // records WHICH axes are present (a bitmap), discarding the ORDER
   // they were written in, so the main loop below used to just consume
   // A's items sequentially in ascending-axis-number order regardless
   // of how X was actually written: A[1] paired with axis X's SMALLEST
   // value, never necessarily X[1]'s own axis. See Bugs27 #34 (LRM
   // p.163: "L[i] selects along axis X[i]").
   //
const int qio = Workspace::get_IO();
vector<ShapeItem> axis_to_pos(B.get_rank(), -1);
   loop(e, X.element_count())
       {
         Cell cache;
         const APL_Integer axis = X.get_cravel(e, cache).get_near_int() - qio;
         axis_to_pos[axis] = e;
       }

   // construct an IndexExpr in index (= parse-) order (i.e. the index_expr[0]
   // corresponds to the lasr axis ¯1↑⍴B of B). We therefore move backwards
   // from the end of A resp. X.
   //
IndexExpr index_expr(ASS_none, LOC);   // start with an empty IndexExpr
   index_expr.quad_io = Workspace::get_IO();

   for (sAxis b = B.get_rank() - 1; b >= 0; --b)
       {
         if (!(axes_X & 1 << b))   // Axis  b was not in X: elided idx
            {
              index_expr.add_index(Value_P());   // add elided index
              continue;
            }

         const ShapeItem a = axis_to_pos[b];
          if (Value_P val0 = A.try_pointer_value(a))
             {
               Value_P val = CLONE_P(val0, LOC);
               if (val->compute_depth() > 1)
                  {
                    MORE_ERROR() << "A⌷[X]B: A[" << (a + Workspace::get_IO())
                                 << "] is nested too deeply (expecting a"
                                    " simple index or a simple vector of"
                                    " indices)";
                    DOMAIN_ERROR;
                  }
              index_expr.add_index(val);
             }
         else   // single index
             {
               const APL_Integer I = A.get_near_int(a);
               if (I < 0)
                  {
                    MORE_ERROR() << "A⌷[X]B: A[" << (a + Workspace::get_IO())
                                 << "] = " << I << " is negative";
                    DOMAIN_ERROR;
                  }
               Value_P val = IntScalar(I, LOC);
              index_expr.add_index(val);
             }
       }

   if (index_expr.is_axis())   // Z←B[x] or Z←B[]
      {
        Value_P single_index = index_expr.extract_axis();

        // single_index is null when the sole index was elided (A and X
        // both empty, e.g. ⍬⌷[⍬]B) -- B.index(Value_P) requires a real
        // Value, so *single_index on the null Value_P used to dereference
        // a null pointer (Bugs27 #12). Elided means "this axis unchanged",
        // i.e. the whole of B, same as any other elided-index B[] result.
        //
        Value_P Z = single_index ? B.index(*single_index)
                                  : CLONE(&B, LOC);

        // the clone must not carry B's whole-symbol (or member) marker:
        // (⍬⌷[⍬]V)←C is selective, exactly like (V[])←C (Blake McBride,
        // Bugs32 #7), see Value::clear_lval_markers().
        //
        if (!single_index)   Z->clear_lval_markers();

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }
   else                        // Z←B[x;...]
      {
        Value_P Z = B.index(index_expr);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }
}
//════════════════════════════════════════════════════════════════════════════
