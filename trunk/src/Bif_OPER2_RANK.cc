/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

/*
    This file implements: (A) f ⍤ y B
                     and: (A) f ⍤[X] y B.

    IBM APL2 defines none of them,
    ISO 13751 defines the first, but not the axis variant, and
    NARS defines both.

   The implentation is roughly this:

   1. The N-dimensional case is reshaped to the case where A and B are vectors
   2. Function f is iterated over B (and, in the dyadic case A).
   3. The result of the iterations is reshaped and disclosed to the desired form.

   Since f can be a defined function (or, for that matter, a lambds), its
   computation is implemented as a macro (like for other operators with defined
   functrions. In theory built-in functions could be implemented entirely in
   C++, but we believe that ⍤ is so rarely used that this would not be worth
   the effort. The macros used are:

   For: f ⍤ y B (monadic):

   ∇Z←(LO Z__LO_RANK_X5_B) [X5] B;LB;rho_B;N_max;N;LZ;rho_Z
    (X5 LB rho_B LZ rho_Z)←X5 ◊ B←(LZ,LB)⍴B ◊ N_max←LZ+N←⎕IO ◊ Z←LZ⍴0
    Z[N]←⊂LO rho_B⍴B[N;] ◊ 0 → N_max>N←N+1
    →(X5≢¯1)⍴WITH_AXES ◊ Z←30 ⎕CR rho_Z⍴Z ◊ →0
    WITH_AXES:           Z←⊃[X5]rho_Z⍴Z
   ∇

   For: A f ⍤ y B (dyadic):

   ∇Z←A (LO Z__A_LO_RANK_X7_B)[X7] B;LA;rho_A;LB;rho_B;N_max;N;LZ;rho_Z
    (X7 LA rho_A LB rho_B LZ rho_Z)←X7 ◊ A←(LZ,LA)⍴A ◊ B←(LZ,LB)⍴B
    N_max←LZ+N←⎕IO ◊ Z←LZ⍴0
    Z[N]←⊂(rho_A⍴A[N;]) LO rho_B⍴B[N;] ◊ 0 → N_max>N←N+1
    →(X7≢¯1)⍴WITH_AXES ◊ Z←30 ⎕CR rho_Z⍴Z ◊ →0
    WITH_AXES:           Z←⊃[X7]rho_Z⍴Z
   ∇

   In both cases conforms 30 ⎕CR all items of the result to the same shape.

   Terminology (mostly from ISO): The shape of an argument (A or B) is ibeing
   divided into a higher part (called frame (-shape)) and a lower part (called
   chunk (-shape), For example:

         ⍴B  ←→  (⍴frame) , (⍴chunk)

   The point where the shapes are split into frame and chunk is defined by
   vector y (which is the right function (!) argument RO of the dyadic
   operator ⍤. y may have 1, 2, or 3 items which implies a 3-item vector Y:
   
         Y ←→ { ⌽3⍴⌽⍵ } y   i.e.

         y1        ←→  y1 y1 y1
         y1 y2     ←→  y2 y1 y2
         y1 y2 y3  ←→  y1 y2 y3
 */

#include "Bif_F12_TAKE_DROP.hh"
#include "Bif_OPER2_RANK.hh"
#include "IntCell.hh"
#include "Macro.hh"
#include "PointerCell.hh"
#include "Workspace.hh"

Bif_OPER2_RANK   Bif_OPER2_RANK::fun;

/* general comment: we use the term 'chunk' instead of 'p-rank' to avoid
   confusion with the rank of a value
 */

//----------------------------------------------------------------------------
Token
Bif_OPER2_RANK::eval_LRB(Token & LO, Token & y, Value_P B) const
{
   if (B->element_count() == 1 && B->get_cfirst().is_pointer_cell())
      B = B->get_cfirst().get_pointer_value();

const sRank rank_chunk_B = y123_to_chunk_B_rank(y.get_apl_val(), B->get_rank());
   return do_LyXB(LO, Value_P(), B, rank_chunk_B);
}
//----------------------------------------------------------------------------
Token
Bif_OPER2_RANK::eval_LRXB(Token & LO, Token & y, Value_P X, Value_P B) const
{
   if (B->element_count() == 1 && B->get_cfirst().is_pointer_cell())
      B = B->get_cfirst().get_pointer_value();

const sRank rank_chunk_B = y123_to_chunk_B_rank(y.get_apl_val(), B->get_rank());

   return do_LyXB(LO, X, B, rank_chunk_B);
}
//----------------------------------------------------------------------------
Token
Bif_OPER2_RANK::do_LyXB(Token & _LO, Value_P X, Value_P B, sRank rank_chunk_B)
{
cFunction_P LO = _LO.get_function();
   Assert(LO);
   if (!LO->has_result())
      {
        MORE_ERROR() << "f ⍤ y B: function f returns no result";
        DOMAIN_ERROR;
      }

   // split shape of B into high (=frame) and low (= chunk) shapes.
   //
const sRank frame_B_rank = B->get_rank() - rank_chunk_B;
const Shape shape_Z = B->get_shape().frame_shape(frame_B_rank);
   if (shape_Z.is_empty())
      {
        Value_P Fill_B = Bif_F12_TAKE::first(*B);
        Token tZ = LO->eval_fill_B(Fill_B);
        Value_P Z = tZ.get_apl_val();
        Z->set_shape(B->get_shape());
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (+X)   // ⍤ with axis
      {
        loop(x, X->element_count())
            {
              const APL_Integer axis = X->get_cravel(x).get_int_value();
              if (axis != B->get_rank())
                 {
                   MORE_ERROR() << "f ⍤[X] B: invalid axis " << axis
                                << " of B (with ⍴⍴B = " << B->get_rank()
                                << ") in X.";
                   RANK_ERROR;
                 }
            }
      }

const Shape shape_B = B->get_shape().chunk_shape(rank_chunk_B);

Value_P vsh_B(LOC, &shape_B);   // vsh_B ← ⍴B
Value_P vsh_Z(LOC, &shape_Z);   // vsh_Z ← ⍴Z

   /* X5 is the macro argument B of Macro::MAC_Z__LO_RANK_X5_B, which
      does the following:

      B ← (LZ,LB) ◊ Z ← LZ⍴0
      for every result item Z[N]: Z[N] ← ⊂ LO B[N;]
      Z←rho_Z⍴Z and disclose Z:
         either with: 30 ⎕CR          if LO ⍤ B  (no axis)
         or with:     ⊃[X5]           if LO ⍤ [X]B
  
      X5[0] the axis for the final disclose of rho_Z⍴Z if: f⍤[X] B
            and -1 if no final disclose needed, i.e.if:    f ⍤ B 
      X5[1] LB: the lower part of ⍴B
      X5[2] rho_B: the shape of one rank-y6 cell of B
      X5[3] LZ: the upper part of ⍴Z (= number of LO calls)
      X5[4] rho_Z: the final shape of Z (before 30 ⎕CR)
    */
Value_P X5(5, LOC);
   if (!X)   X5->next_ravel_Int(-1);            // no X:  f ⍤ y B
   else if (X->is_simple_scalar())   // X → ,X
      {
        Value_P X1(1, LOC);
        X1->next_ravel_Int(X->get_cfirst().get_int_value());
        X1->check_value(LOC);
        X5->next_ravel_Pointer(X1.get());   // with X: f ⍤[X] y B
      }
   else      X5->next_ravel_Pointer(X.get());   // with X: f ⍤[X] y B

   X5->next_ravel_Int(shape_B.get_volume());    // LB
   X5->next_ravel_Pointer(vsh_B.get());         // rho_B
   X5->next_ravel_Int(shape_Z.get_volume());    // LZ
   X5->next_ravel_Pointer(vsh_Z.get());         // rho_Z
   X5->check_value(LOC);

   return Macro::get_macro(Macro::MAC_Z__LO_RANK_X5_B)->eval_LXB(_LO, X5, B);
}
//----------------------------------------------------------------------------
Token
Bif_OPER2_RANK::eval_ALRB(Value_P A, Token & LO, Token & y, Value_P B) const
{
   if (B->element_count() == 1 && B->get_cfirst().is_pointer_cell())
      B = B->get_cfirst().get_pointer_value();

sRank rank_chunk_A = A->get_rank();
sRank rank_chunk_B = B->get_rank();
   y123_to_AB(y.get_apl_val(), rank_chunk_A, rank_chunk_B);

   return do_ALyXB(A, rank_chunk_A, LO, Value_P(), B, rank_chunk_B);
}
//----------------------------------------------------------------------------
Token
Bif_OPER2_RANK::eval_ALRXB(Value_P A, Token & LO, Token & y,
                           Value_P X, Value_P B) const
{
   if (B->element_count() == 1 && B->get_cfirst().is_pointer_cell())
      B = B->get_cfirst().get_pointer_value();

sRank rank_chunk_A = A->get_rank();
sRank rank_chunk_B = B->get_rank();

   y123_to_AB(y.get_apl_val(), rank_chunk_A, rank_chunk_B);

   return do_ALyXB(A, rank_chunk_A, LO, X, B, rank_chunk_B);
}
//----------------------------------------------------------------------------
Token
Bif_OPER2_RANK::do_ALyXB(Value_P A, sRank rank_chunk_A, Token & _LO,
                         Value_P X, Value_P B, sRank rank_chunk_B)
{
cFunction_P LO = _LO.get_function();
   Assert(LO);
   if (!LO->has_result())
      {
        MORE_ERROR() << "A f ⍤ y B: function f returns no result";
        DOMAIN_ERROR;
      }

sRank frame_A_rank = A->get_rank() - rank_chunk_A;   // frame_A_rank is y8
sRank frame_B_rank = B->get_rank() - rank_chunk_B;   // frame_B_rank is y9

   // if both high-ranks are 0 (i.e. high shapes are ⍬), then return A LO B.
   //
   if (frame_A_rank == 0 && frame_B_rank == 0)   return LO->eval_AB(A, B);

   /* Otherwise at least one of the frame ranks is > 0.
      split shapes of A1 and B1 into high (frame) and low (chunk) shapes.
      Even cwif A and B have the same shape, frame_A_rank and frame_B_rank
      could be different, leading to differently split shapes for A and B

      Non-scalar frame ranks must be equal and their shapes must match.
    */
const Shape shape_Z = frame_B_rank ? B->get_shape().frame_shape(frame_B_rank)
                                   : A->get_shape().frame_shape(frame_A_rank);

   if (+X)   // ⍤ with axis
      {
        loop(x, X->element_count())
            {
              const APL_Integer axis = X->get_cravel(x).get_int_value();
              if (axis != B->get_rank())
                 {
                   MORE_ERROR() << "A f ⍤[X] B: invalid axis " << axis
                                << " of B (with ⍴⍴B = " << B->get_rank()
                                << ") in X.";
                   RANK_ERROR;
                 }

              if (axis != A->get_rank())
                 {
                   MORE_ERROR() << "A f ⍤[X] B: invalid axis " << axis
                                << " of A (with ⍴⍴A = " << A->get_rank()
                                << ") in X.";
                   RANK_ERROR;
                 }
            }
      }

   if (frame_A_rank && frame_B_rank)   // A and B frames non-scalar
      {
        if (frame_A_rank != frame_B_rank)                          RANK_ERROR;
        if (shape_Z != A->get_shape().frame_shape(frame_A_rank))   LENGTH_ERROR;
      }

   if (shape_Z.is_empty())
      {
        Value_P Fill_A = Bif_F12_TAKE::first(*A);
        Value_P Fill_B = Bif_F12_TAKE::first(*B);
        Shape shape_Z;

        if (A->is_empty())          shape_Z = A->get_shape();
        else if (!A->is_scalar())   DOMAIN_ERROR;

        if (B->is_empty())          shape_Z = B->get_shape();
        else if (!B->is_scalar())   DOMAIN_ERROR;

        Value_P Z1 = LO->eval_fill_AB(Fill_A, Fill_B).get_apl_val();

        Value_P Z(shape_Z, LOC);
        Z->set_ravel_Value(0, Z1.get());
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

const Shape low_A = A->get_shape().chunk_shape(rank_chunk_A);
const Shape low_B = B->get_shape().chunk_shape(rank_chunk_B);

Value_P vsh_A(LOC, &low_A);
Value_P vsh_B(LOC, &low_B);
Value_P vsh_Z(LOC, &shape_Z);

   /* X7 is the macro argument B of Macro::MAC_Z__LO_RANK_X7_B, which
      does the following:

      A← (LZ,LA)⍴A ◊ B ←(LZ,LB)⍴B ◊ Z ← LZ⍴0
      for every result item Z[N]: Z[N] ← ⊂ A[N;] LO B[N;]
      Z←rho_Z⍴Z and disclose Z:
         either with: 30 ⎕CR          if LO ⍤ B  (no axis)
         or with:     ⊃[X7]           if LO ⍤ [X]B
  
      X7[0] the axis for the final disclose of rho_Z⍴Z if: f⍤[X] B
            and -1 if no final disclose needed, i.e.if:    f ⍤ B 
      X7[1] LA: the lower part of ⍴A
      X7[2] rho_A: the shape of one rank-y6 cell of A
      X7[3] LB: the lower part of ⍴B
      X7[4] rho_B: the shape of one rank-y6 cell of B
      X7[5] LZ: the upper part of ⍴Z (= number of LO calls)
      X7[6] rho_Z: the final shape of Z (before 30 ⎕CR)
    */
Value_P X7(7, LOC);
   if (!X)   X7->next_ravel_Int(-1);                 // no X:   A ⍤ y B
   else      X7->next_ravel_Value(X.get());          // with X: A ⍤[X] y B

   X7->next_ravel_Int(low_A.get_volume());           // LA
   X7->next_ravel_Value(vsh_A.get());                // rho_A
   X7->next_ravel_Int(low_B.get_volume());           // LB
   X7->next_ravel_Value(vsh_B.get());                // rho_B
   X7->next_ravel_Int(shape_Z.get_volume());         // LZ
   X7->next_ravel_Value(vsh_Z.get());
   X7->check_value(LOC);
   return Macro::get_macro(Macro::MAC_Z__A_LO_RANK_X7_B)
                           ->eval_ALXB(A, _LO, X7, B);
}
//----------------------------------------------------------------------------
sRank
Bif_OPER2_RANK::y123_to_chunk_B_rank(Value_P y123, sRank rank_B)
{
   /* y123_to_AB() splits the ranks of A and B into a (higher-dimensions)
      "frame" and a (lower-dimensions) "chunk" as specified by y123.

      Let Z ← A f ⍤ y123 B. Then y123 is the chunk ranks of Z, A, and B.

      See ISO p. 124, 125
    */

   if (!y123)
      {
        MORE_ERROR() << "(A) f ∘ y B without y ";
        VALUE_ERROR;
      }

   if ( y123->get_rank() > 1)
      {
        MORE_ERROR() << "(A) f ∘ y B with ⍴⍴y = " << y123->get_rank()
                     << " (expecting 1 ≥ ⍴⍴y 1).";
        RANK_ERROR;
      }

   // 2. the number of elements in y determines rank_B:
   //
   //                    -- monadic f⍤ --       -- dyadic f⍤ --
   //          	        rank_A     rank_B       rank_A   rank_B
   // ---------------------------------------------------------
   // y        :        N/A        y            y        y
   // yA yB    :        N/A        yB           yA       yB
   // yM yA yB :        N/A        yM           yA       yB
   // ---------------------------------------------------------

   /* Set y3 to ↑ ⌽3⍴⌽y1. We need only y3 but check that all items of y123
      are integers. Note that:

      ⌽3⍴⌽ 1      ←→  1 1 1
      ⌽3⍴⌽ 1 2    ←→  2 1 2
      ⌽3⍴⌽ 1 2 3  ←→  1 2 3
             │ │      │
             │ │      └──── take
             └─┴─────────── check
    */
sRank y3;

   switch(y123->element_count())
      {
        case 1:  y3 = y123->get_cfirst().get_near_int();    // take
                 break;

        case 2:       y123->get_cfirst().get_near_int();    // check
                 y3 = y123->get_cravel(1).get_near_int();   // take
                 break;

        case 3:  y3 = y123->get_cfirst().get_near_int();    // take
                      y123->get_cravel(1).get_near_int();   // check
                      y123->get_cravel(2).get_near_int();   // check
                      break;

        default: // ISO p. 124 (monadic) p. 125 (dyadic)
                 MORE_ERROR() << "(A) f ∘ y B with ⍴y = "
                              << y123->element_count() << " (not 1, 2, or 3)";
                 LENGTH_ERROR;
      }

const sRank y5 = y3 > rank_B ? rank_B : y3;
sRank y6 = y5;   if (y5 < 0)   y6 = y5 > 0 ? y5 : 0;
   return y6;
}
//----------------------------------------------------------------------------
void
Bif_OPER2_RANK::y123_to_AB(Value_P y123, sRank & rank_A, sRank & rank_B)
{
   // y123_to_AB() splits the ranks of A and B into a (higher-dimensions)
   // "frame" and a (lower-dimensions) "chunk" as specified by y123.

   // 1. on entry rank_A and rank_B are the ranks of A and B.
   //
   //    Remember the ranks of A and B to limit rank_A and rank_B
   //    if values in y123 should exceed them.
   //
const sRank rk_A = rank_A;
const sRank rk_B = rank_B;

   if (!y123)                   VALUE_ERROR;
   if ( y123->get_rank() > 1)   DOMAIN_ERROR;

   // 2. the number of elements in y determine how rank_A and rank_B
   // shall be computed:
   //
   //                    -- monadic f⍤ --       -- dyadic f⍤ --
   //          	        rank_A     rank_B       rank_A   rank_B
   // ---------------------------------------------------------
   // y        :        N/A        y            y        y
   // yA yB    :        N/A        yB           yA       yB
   // yM yA yB :        N/A        yM           yA       yB
   // ---------------------------------------------------------

   switch(y123->element_count())
      {
        case 1:  rank_A = y123->get_cfirst().get_near_int();
                 rank_B = rank_A;                            break;

        case 2:  rank_A = y123->get_cfirst().get_near_int();
                 rank_B = y123->get_cravel(1).get_near_int();  break;

        case 3:           y123->get_cfirst().get_near_int();
                 rank_A = y123->get_cravel(1).get_near_int();
                 rank_B = y123->get_cravel(2).get_near_int();  break;

        default: LENGTH_ERROR;
      }

   // 3. adjust rank_A and rank_B if they exceed their initial value or
   // if they are negative
   //
   if (rank_A > rk_A)   rank_A = rk_A;
   if (rank_A < 0)      rank_A += rk_A;
   if (rank_A < 0)      rank_A = 0;

   if (rank_B > rk_B)   rank_B = rk_B;
   if (rank_B < 0)      rank_B += rk_B;
   if (rank_B < 0)      rank_B = 0;
}
//----------------------------------------------------------------------------
void
Bif_OPER2_RANK::unstrand_y_B(Value_P y123_B, Value_P & y123, Value_P & B)
{
   /* The ISO standard and NARS define the reduction patterns for the RANK
      operator ⍤ as:
     
      Z ← A f ⍤ y B		    (ISO and NARS)
      Z ←   f ⍤ y B		    (ISO and NARS)
      Z ← A f ⍤ [X] y B		(NARS only)
      Z ←   f ⍤ [X] y B		(NARS only)

      We say y123 instead of y here to indicate that y has 1, 2, or 3 items.

      If y and B are integer literals then GNU APL will bind y to B at
      tokenization time.

      This function tries to "unbind" its argument y123_B into the original
      components y123 (= y in the ISO standard) and B. y123 can be an integer
      scalar IS or an integer vector IV (with 1-3 items). B can be a scalar
      BS or an nested scalar Bn of any type.

     Parser::fix_RANK_syntax() makes sure that leading integers literals
     are not stranded together with items of B. Although that avoids an
     incorrect mixing of y and B literals, it can not ensure that the
     first item of y123_B is a scalar (e.g. f ⍤ N B with scalar variable N
     or niladic function N returning a scalar). It does guarantee, however,
     that the first item of y123_B is really y123_B and that subsequent
     items of belong to B. The possible cases are therefore:

     case   y123_B      y123     B         Note
     ────────────────────────────────────────────────────────────────────────
      1.     nested,B    ↑y123_B  1↓y123_B  nested from fix_RANK_syntax()
      2.     simple,B    ↑y123_B  1↓y123_B  simple from variable or niladic N
     ────────────────────────────────────────────────────────────────────────
    */

   // in both cases is y123_B a vector
   //
   if (y123_B->get_rank() > 1)
      {
        MORE_ERROR() << "f⍤y B: ⍴⍴B = " << y123_B->get_rank()
                     << " (expecting ⍴⍴ y = 1)";
        RANK_ERROR;
      }

const ShapeItem length = y123_B->element_count();
   if (length == 0)
      {
        MORE_ERROR() << "f⍤y B: invalid empth y (⍴y = 0)";
        LENGTH_ERROR;
      }

   // check for case 1 (the only one with nested first element)
   //
   if (y123_B->get_cfirst().is_pointer_cell())   // case 1: (y123)
      {
         y123 = y123_B->get_cfirst().get_pointer_value();
         if (length == 1)        // scalar y123 and empty B
            {
            }
         else if (length == 2)   // skalar B
            {
              const Cell & B0 = y123_B->get_cravel(1);
              if (B0.is_pointer_cell())   // (B)
                 {
                   B = B0.get_pointer_value();
                 }
              else
                 {
                   B = Value_P(LOC);
                   B->next_ravel_Cell(B0);
                 }
            }
         else                    // vector B
            {
              B = Value_P(length - 1, LOC);
              loop(l, length - 1)
                  B->next_ravel_Cell(y123_B->get_cravel(l + 1));
            }
         y123->check_value(LOC);
         B->check_value(LOC);
         return;
      }

   // case 1. ruled out, so the first 1, 2, or 3 cells are j123.
   // see how many (at most)
   //
int y123_len = 0;
   loop(yy, 3)
      {
        if (yy >= length)   break;
        const Cell & cy = y123_B->get_cravel(yy);
        if (cy.is_near_int())   ++y123_len;
        else                                          break;
      }
   if (y123_len == 0)   LENGTH_ERROR;   // at least y1 is needed

   // cases 2.-4. start with integers of length 1, 2, or 3
   //
   if (length == y123_len)   // case 2: y123:⍬
      {
        y123 = y123_B;
        //
        // NOTE: B is NOT assigned so that Prefix::reduce_F_D_B_() can detect
        // that y123_ was only y123 !
        //
        return;
      }

   if (length == (y123_len + 1) &&
       y123_B->get_cravel(y123_len).is_pointer_cell())   // case 3. y123:⊂B
      {
        y123 = Value_P(y123_len, LOC);
        loop(yy, y123_len)   y123->next_ravel_Cell(y123_B->get_cravel(yy));
        B = y123_B->get_cravel(y123_len).get_pointer_value();
        y123->check_value(LOC);
        B->check_value(LOC);
        return;
      }

   // case 4: y123:B...
   //
   y123 = Value_P(y123_len, LOC);
   loop(yy, y123_len)   y123->next_ravel_Cell(y123_B->get_cravel(yy));

const ShapeItem B_len = length - y123_len;
   B = Value_P(B_len, LOC);
   loop(bb, B_len)   B->next_ravel_Cell(y123_B->get_cravel(bb + y123_len));
   B->check_value(LOC);
}
//----------------------------------------------------------------------------
