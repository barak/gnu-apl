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
#include "QuadFunction.hh"
#include "StateIndicator.hh"
#include "Workspace.hh"

Bif_OPER2_RANK   Bif_OPER2_RANK::fun;

/* general comment: we use the term 'chunk' instead of 'p-rank' to avoid
   confusion with the rank of a value
 */

//────────────────────────────────────────────────────────────────────────────
/// Genuinely evaluate LO on a synthetic chunk (a real value, but built from
/// B's -- or A's/B's -- own prototype cell rather than drawn from an actual
/// frame position, since the frame is empty): needed to determine the shape
/// an empty-frame LO⍤y B (or A LO⍤y B) result should have. There is
/// deliberately no primitive/defined distinction here: ISO 13751's own
/// Rank-operator definition (9.3.4/9.3.5, "apply f to the rank-y6 cells of
/// B") never makes one either, and neither does a real, widely used
/// Rank-operator implementation -- confirmed against Dyalog APL, e.g.
/// ⍴({⍴⍵}⍤1)0 3⍴0 is 0 1, matching ⍴(⍴⍤1)0 3⍴0, not the argument's own
/// chunk shape 0 3. Unlike Each/Outer Product, where APL2's own documented
/// fill function for "Defined Operations" IS identity (Figure 20), Rank
/// (not part of APL2 at all) has no such documented convention to justify
/// treating a defined LO more conservatively than a primitive one here.
///
/// A defined LO evaluated this way CAN have an observable effect (⎕←, a
/// global assignment, ...) if its body has one -- a known, accepted cost of
/// matching real Rank-operator behavior, not something this function hides.
///
/// \b pushed is the token LO->eval_rank_fill_B(...)/eval_rank_fill_AB(...)
/// just returned. A primitive's (including one like Domino that overrides
/// eval_rank_fill_B/AB to delegate to its own special, non-executing
/// eval_fill_B/AB formula instead) is already the real, synchronous
/// result. A defined function's is only ever Token(TOK_SI_PUSHED) --
/// eval_B()/eval_AB() (which the base Function::eval_rank_fill_B/AB()
/// default just calls) never compute synchronously, they push a new SI
/// frame and expect the normal driver loop to run it -- so that frame is
/// driven to completion here exactly the way Command::finish_context()
/// drives a top-level statement.
///
/// If LO's own evaluation fails with a real APL error, that same error is
/// re-thrown (not swallowed): the (F⍤k)B expression could not be completed
/// for a well-founded reason, and it is exactly the error LO itself would
/// raise if genuinely invoked -- which is what just happened. Anything else
/// abnormal (e.g. LO branching past its own frame) leaves the result shape
/// completely undetermined; per this codebase's own convention for a
/// bad/unknown shape (LENGTH_ERROR when some valid shape of the same rank
/// exists, RANK_ERROR when not even the rank is known -- never DOMAIN_ERROR,
/// which is reserved for bad ravel content, not bad shape), RANK_ERROR is
/// raised since no rank at all could be determined.
static Value_P
eval_LO_on_fill_chunk(Token pushed)
{
   if (pushed.get_Class() == TC_VALUE || pushed.get_tag() == TOK_VOID)
      return pushed.get_apl_val();   // LO was primitive: already synchronous

   Assert(pushed.get_tag() == TOK_SI_PUSHED);
StateIndicator * const caller_si = Workspace::SI_top()->get_parent();

   for (;;)
       {
         Token token = Workspace::SI_top()->get_executable()->execute_body();

         if (token.get_tag() == TOK_SI_PUSHED)   continue;

         // Bugs31 #17 (Blake McBride): LO (or a function called by LO) may
         // itself use ⎕EC (directly, or via ⎕EA/⎕EB). Its frames are driven
         // here rather than by Command::finish_context(), so the ⎕EC post-
         // processing that finish_context() does when a ⎕EC operand frame
         // finishes must be done here as well: wrap the operand's result
         // (or error) into ⎕EC's (rc ⎕ET value) result with Quad_EC::eoc().
         // Otherwise the raw operand value (e.g. 4 for ⎕EC '2+2') was
         // returned in place of ⎕EC's result, and an error in the operand
         // was not caught by ⎕EC at all.
         //
         if (token.get_tag() == TOK_ERROR)
            {
              // find a ⎕EC operand frame strictly above caller_si (one
              // further down belongs to a ⎕EC around the entire ⍤ and is
              // handled by finish_context() as usual).
              //
              StateIndicator * safe_si = 0;
              for (StateIndicator * si = Workspace::SI_top();
                   si && si != caller_si; si = si->get_parent())
                  {
                    if (si->is_safe_execution_start())   { safe_si = si;   break; }
                  }

              if (safe_si)
                 {
                   const Error err = StateIndicator::get_error(Workspace::SI_top());
                   while (Workspace::SI_top() != safe_si)   Workspace::pop_SI(LOC);
                   StateIndicator::get_error(Workspace::SI_top()) = err;
                   Quad_EC::eoc(token);   // TOK_ERROR → (0 ⎕ET ⎕EM)
                 }
            }
         else if (Workspace::SI_top()->is_safe_execution_start())
            {
              Quad_EC::eoc(token);
            }

         if (token.get_Class() == TC_VALUE || token.get_tag() == TOK_VOID)
            {
              Workspace::pop_SI(LOC);
              if (Workspace::SI_top() == caller_si)   return token.get_apl_val();

              // a deeper nested call just finished normally -- splice its
              // result into the parent's TOK_SI_PUSHED placeholder and
              // keep driving, exactly like Command::finish_context() does.
              Prefix & prefix = Workspace::SI_top()->get_prefix();
              Assert(prefix.at0().get_tag() == TOK_SI_PUSHED);
              prefix.replace_SI_PUSHED(token);
              continue;
            }

         if (token.get_tag() == TOK_ERROR)
            {
              while (Workspace::SI_top() != caller_si)   Workspace::pop_SI(LOC);
              throw_apl_error(token.get_ErrorCode(), LOC);
            }

         // branch, escape, or anything else abnormal: give up, the result
         // shape is completely undetermined.
         while (Workspace::SI_top() != caller_si)   Workspace::pop_SI(LOC);
         RANK_ERROR;
       }
}

//────────────────────────────────────────────────────────────────────────────
/// return the synthetic chunk that LO is applied to when the frame of
/// LO⍤y B (or A LO⍤y B) is empty: V itself if V's frame rank is 0 (V is
/// not iterated, i.e. it IS the single chunk that is used for every frame
/// position), and otherwise a chunk of V's chunk shape, filled with V's
/// prototype cell (mirroring how Value::set_default() fills an actually-
/// empty result).
static Value_P
make_fill_chunk(const Value & V, sRank frame_rank, sRank chunk_rank)
{
   if (frame_rank == 0)   return CLONE(&V, LOC);

const Shape chunk_shape = V.get_shape().chunk_shape(chunk_rank);
Value_P Fill(chunk_shape, LOC);
   {
     Cell proto_cache;
     const Cell & proto = V.get_cproto(proto_cache);
     loop(z, Fill->element_count())   Fill->next_ravel_Cell(proto);
   }
   Fill->check_value(LOC);
   return Fill;
}
//════════════════════════════════════════════════════════════════════════════
void
Bif_OPER2_RANK::unstrand_RO_B(const UCS_string & LO_name, Value_P y123_B,
                               Value_P & y123, Value_P & B)
{
   /* The ISO standard and NARS define the reduction patterns for the RANK
      operator ⍤ as:
     
      Z ← A f ⍤ y B             (ISO and NARS)
      Z ←   f ⍤ y B             (ISO and NARS)
      Z ← A f ⍤ [X] y B             (NARS only)
      Z ←   f ⍤ [X] y B             (NARS only)

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
        MORE_ERROR() << LO_name << "⍤y B: Bad rank " << y123_B->get_rank()
                     << " (expecting ⍴⍴y B ≤ 1)";
        RANK_ERROR;
      }

const ShapeItem length = y123_B->element_count();
   if (length == 0)
      {
        MORE_ERROR() << LO_name
                     << "⍤y B: Bad length 0 of y B (expecting ⍴y B > 0)";
        LENGTH_ERROR;
      }

   // check for case 1 (the only one with nested first element)
   //
   if (y123_B->is_pointer_cell(0))   // case 1: (y123)
      {
         y123 = y123_B->get_pointer_value(0);
         if (length == 1)        // scalar y123 and empty B
            {
              // NOTE: B is NOT assigned so that Prefix::reduce_F_D_B_() can
              // detect that y123_B was only y123 -- same as case 2 below.
              y123->check_value(LOC);
              return;
            }
         else if (length == 2)   // skalar B
            {
              Cell cache;
              const Cell & B0 = y123_B->get_cravel(1, cache);
              if (Value_P v = B0.try_pointer_value())   // (B)
                 {
                   B = v;
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
                  {
                    Cell cache;
                    B->next_ravel_Cell(y123_B->get_cravel(l + 1, cache));
                  }
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
        Cell cache;
        const Cell & cy = y123_B->get_cravel(yy, cache);
        if (cy.is_near_int())   ++y123_len;
        else                                          break;
      }
   if (y123_len == 0)
      {
        MORE_ERROR() << "f⍤y B: y must have at least 1 item (y1)";
        LENGTH_ERROR;   // at least y1 is needed
      }

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
       y123_B->is_pointer_cell(y123_len))   // case 3. y123:⊂B
      {
        y123 = Value_P(y123_len, LOC);
        loop(yy, y123_len)
            {
              Cell cache;
              y123->next_ravel_Cell(y123_B->get_cravel(yy, cache));
            }
        B = y123_B->get_pointer_value(y123_len);
        y123->check_value(LOC);
        B->check_value(LOC);
        return;
      }

   // case 4: y123:B...
   //
   y123 = Value_P(y123_len, LOC);
   loop(yy, y123_len)
       {
         Cell cache;
         y123->next_ravel_Cell(y123_B->get_cravel(yy, cache));
       }

const ShapeItem B_len = length - y123_len;
   B = Value_P(B_len, LOC);
   loop(bb, B_len)
       {
         Cell cache;
         B->next_ravel_Cell(y123_B->get_cravel(bb + y123_len, cache));
       }
   B->check_value(LOC);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_OPER2_RANK::eval_ALRB(cValue_R A, Token & LO, Token & y, cValue_R B) const
{
// B_hold_ is function-scoped (not block-scoped) so it actually
   // outlives pB, matching Quad_MAP.cc's version of this same pattern.
   // The block-scoped version was safe only because B (a cValue_R
   // parameter) itself outlives the call regardless -- the "hold" did
   // nothing (Bugs18 #15).
   //
Value_P B_hold_;
const cValue * pB = &B;
   // B is not disclosed here: an f ⍤ y (B) stranded by the parser is
   // unstranded (and B disclosed) by unstrand_RO_B(); any other 1-item
   // nested B, e.g. ,⊂1 2, is a genuine argument (Bugs35 #38)

sRank rank_chunk_A = A.get_rank();
sRank rank_chunk_B = pB->get_rank();
   y123_to_AB(y.get_apl_val().get(), rank_chunk_A, rank_chunk_B);

   return do_ALyXB(CLONE(&A, LOC), rank_chunk_A, LO, Value_P(), CLONE(pB, LOC), rank_chunk_B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_OPER2_RANK::eval_ALRXB(cValue_R A, Token & LO, Token & y,
                           cValue_R X, cValue_R B) const
{
// see eval_ALRB() above for why B_hold_ is function- not block-scoped.
   //
Value_P B_hold_;
const cValue * pB = &B;
   // B is not disclosed here: an f ⍤ y (B) stranded by the parser is
   // unstranded (and B disclosed) by unstrand_RO_B(); any other 1-item
   // nested B, e.g. ,⊂1 2, is a genuine argument (Bugs35 #38)

sRank rank_chunk_A = A.get_rank();
sRank rank_chunk_B = pB->get_rank();

   y123_to_AB(y.get_apl_val().get(), rank_chunk_A, rank_chunk_B);

   return do_ALyXB(CLONE(&A, LOC), rank_chunk_A, LO, CLONE(&X, LOC), CLONE(pB, LOC), rank_chunk_B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_OPER2_RANK::eval_LRB(Token & LO, Token & y, cValue_R B) const
{
// see eval_ALRB() above for why B_hold_ is function- not block-scoped.
   //
Value_P B_hold_;
const cValue * pB = &B;
   // B is not disclosed here: an f ⍤ y (B) stranded by the parser is
   // unstranded (and B disclosed) by unstrand_RO_B(); any other 1-item
   // nested B, e.g. ,⊂1 2, is a genuine argument (Bugs35 #38)

const sRank rank_chunk_B = y123_to_chunk_B_rank(LO.get_function()->get_name(),
                                                 y.get_apl_val().get(),
                                                 pB->get_rank());
   return do_LyXB(LO, Value_P(), CLONE(pB, LOC), rank_chunk_B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_OPER2_RANK::eval_LRXB(Token & LO, Token & y, cValue_R X, cValue_R B) const
{
// see eval_ALRB() above for why B_hold_ is function- not block-scoped.
   //
Value_P B_hold_;
const cValue * pB = &B;
   // B is not disclosed here: an f ⍤ y (B) stranded by the parser is
   // unstranded (and B disclosed) by unstrand_RO_B(); any other 1-item
   // nested B, e.g. ,⊂1 2, is a genuine argument (Bugs35 #38)

const sRank rank_chunk_B = y123_to_chunk_B_rank(LO.get_function()->get_name(),
                                                 y.get_apl_val().get(),
                                                 pB->get_rank());

   return do_LyXB(LO, CLONE(&X, LOC), CLONE(pB, LOC), rank_chunk_B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_OPER2_RANK::do_ALyXB(Value_P A, sRank rank_chunk_A, Token & _LO,
                         Value_P X, Value_P B, sRank rank_chunk_B)
{
cFunction_P LO = _LO.get_function();
   Assert(LO);
   if (!LO->has_result())
      {
        MORE_ERROR() << "A " << LO->get_name() << " ⍤ y B: Bad function "
                     << LO->get_name() << " (expecting a result)";
        DOMAIN_ERROR;
      }

sRank frame_A_rank = A->get_rank() - rank_chunk_A;   // frame_A_rank is y8
sRank frame_B_rank = B->get_rank() - rank_chunk_B;   // frame_B_rank is y9

   // if both high-ranks are 0 (i.e. high shapes are ⍬), then return A LO B.
   //
   if (frame_A_rank == 0 && frame_B_rank == 0)   return LO->eval_AB(*A, *B);

   /* Otherwise at least one of the frame ranks is > 0.
      split shapes of A1 and B1 into high (frame) and low (chunk) shapes.
      Even cwif A and B have the same shape, frame_A_rank and frame_B_rank
      could be different, leading to differently split shapes for A and B

      Non-scalar frame ranks must be equal and their shapes must match.
    */
const Shape shape_Z = frame_B_rank ? B->get_shape().frame_shape(frame_B_rank)
                                   : A->get_shape().frame_shape(frame_A_rank);

   // X (if any) is the axis of the final disclose ⊃[X] of the LO results
   // (the NARS variant of ⍤). Whether X is valid depends on the rank of
   // the LO results, so ⊃[X] checks it (Bugs35 D15).

   if (frame_A_rank && frame_B_rank)   // A and B frames non-scalar
      {
        if (frame_A_rank != frame_B_rank)
           {
             MORE_ERROR() << "A f⍤y B: expecting the frame ranks of A and"
                             " B to match; frame rank of A is "
                          << frame_A_rank << ", of B is " << frame_B_rank;
             RANK_ERROR;
           }
        if (shape_Z != A->get_shape().frame_shape(frame_A_rank))
           {
             MORE_ERROR() << "A f⍤y B: expecting the frames of A and B to"
                             " have the same shape; frame of A is "
                          << A->get_shape().frame_shape(frame_A_rank)
                          << ", of B is " << shape_Z;
             LENGTH_ERROR;
           }
      }

   if (shape_Z.is_empty())
      {
        // The frame is empty: LO is applied to one synthetic chunk pair
        // (A and B themselves if their frame rank is 0, and otherwise a
        // chunk built from their prototypes), only to determine the shape
        // and prototype of the (empty) result. A non-empty A (or B) with a
        // non-zero frame rank cannot occur here: its frame is shape_Z
        // (checked above), so it would be empty. See Bugs27 #28.
        //
        // Bugs31 #16 (Blake McBride): the chunks that LO is applied to
        // must have the real chunk shapes (not just ↑A resp. ↑B), and the
        // shape of Z is the (empty) frame shape_Z followed by the shape
        // of LO's result Z1 on such a chunk -- exactly as in the monadic
        // case (see do_LyXB()), e.g. 1 (,⍤1) 0 3⍴0 has shape 0 4 like
        // 1 (,⍤1) 1 3⍴0 has shape 1 4. Previously Z got the entire shape
        // of A or B (0 3), ignoring LO's result shape.
        //
        // A (resp. B) is a single chunk that is used for every frame
        // position if its frame rank is 0; otherwise its frame is the
        // (empty) frame shape_Z and a chunk is synthesized from its
        // prototype.
        //
        Value_P Fill_A = make_fill_chunk(*A, frame_A_rank, rank_chunk_A);
        Value_P Fill_B = make_fill_chunk(*B, frame_B_rank, rank_chunk_B);

        // LO is genuinely applied to the synthetic Fill_A/Fill_B chunks
        // (not routed through LO->eval_fill_AB(), which for a *defined*
        // LO would just return Fill_B itself unevaluated, per APL2's
        // Figure 20 "Defined Operations" identity rule -- a rule that
        // belongs to Each/Outer Product, not Rank; see
        // eval_LO_on_fill_chunk() above for the rationale).
        //
        Value_P Z1 = eval_LO_on_fill_chunk(LO->eval_rank_fill_AB(*Fill_A,
                                           *Fill_B));

        // Z's shape comes from what LO actually produced (Bugs30 #11/#12),
        // but its prototype is the TYPE of that result, like for every
        // other empty APL value and like the monadic branch below: e.g.
        // the prototype of (1 2 3)(+⍤1)0 3⍴0 is 0 (not 1 from 1 2 3+0 0 0),
        // as for 1∘.,0 3⍴0 or 1,¨0 3⍴0 (Blake McBride, Bugs32 #18).
        //
        Value_P Z(shape_Z + Z1->get_shape(), LOC);
        Z->set_default(*Z1, LOC);
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
                           ->eval_ALXB(*A, _LO, *X7, *B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_OPER2_RANK::do_LyXB(Token & _LO, Value_P X, Value_P B, sRank rank_chunk_B)
{
cFunction_P LO = _LO.get_function();
   Assert(LO);
   if (!LO->has_result())
      {
        MORE_ERROR() << LO->get_name() << " ⍤ y B: Bad function "
                     << LO->get_name() << " (expecting a result)";
        DOMAIN_ERROR;
      }

   // split shape of B into high (=frame) and low (= chunk) shapes.
   //
const sRank frame_B_rank = B->get_rank() - rank_chunk_B;

   // if the frame rank is 0 (i.e. the chunk rank already covers all of
   // B), there is exactly one chunk -- the whole of B -- so LO⍤y B is
   // simply LO B; mirrors the analogous (and already-existing)
   // frame_A_rank==0 && frame_B_rank==0 fast path a few dozen lines
   // below in the dyadic do_ALyXB() sibling of this function, which
   // this monadic do_LyXB() was missing. Without it, every f⍤k B --
   // including the trivial case where k already equals B's own rank,
   // e.g. (÷⍤0)0 -- always went through Macro::MAC_Z__LO_RANK_X5_B,
   // so any error LO raised suspended showing that internal macro's
   // own μ-variable pseudocode and left it stuck on )SI, instead of
   // reporting DOMAIN ERROR etc. against the user's own expression the
   // way (LO¨)B and (LO⍣N)B already do for the same trivial case
   // (Blake McBride, Bugs26 #7).
   if (frame_B_rank == 0)   return LO->eval_B(*B);

const Shape shape_Z = B->get_shape().frame_shape(frame_B_rank);
   if (shape_Z.is_empty())
      {
        // There is no real chunk to draw content from (the frame is
        // empty), so every cell of this placeholder chunk is filled with
        // B's own prototype cell, mirroring how Value::set_default()
        // already does the analogous thing for an actually-empty result.
        // See Bugs27 #28. LO is then genuinely applied to this synthetic
        // chunk (not routed through LO->eval_fill_B(), which for a
        // *defined* LO would just return Fill_B itself unevaluated, per
        // APL2's Figure 20 "Defined Operations" identity rule -- a rule
        // that belongs to Each/Outer Product, not Rank, which has no
        // fill-function convention of its own at all; see
        // eval_LO_on_fill_chunk() above for why genuine evaluation,
        // matching real Rank-operator implementations like Dyalog's, is
        // used here for primitive and defined LO alike).
        //
const Shape chunk_shape_B = B->get_shape().chunk_shape(rank_chunk_B);
        Value_P Fill_B(chunk_shape_B, LOC);
        {
          Cell proto_cache;
          const Cell & proto = B->get_cproto(proto_cache);
          loop(z, Fill_B->element_count())
              Fill_B->next_ravel_Cell(proto);
        }
        Fill_B->check_value(LOC);

        Value_P Z1 = eval_LO_on_fill_chunk(LO->eval_rank_fill_B(*Fill_B));

        // Z1's own shape is the result of genuinely applying LO to ONE
        // chunk (e.g. ⍴⍤1's chunk-result on a 3-item row is ,3, shape
        // (1)) -- the actual result shape is the (empty) frame shape
        // with that chunk-result shape appended, NOT B's entire original
        // shape discarding it, e.g. (⍴⍤1)0 3⍴0 is shape 0 1 (0 frame
        // positions, 1-item chunk-result shape), not 0 3 -- compare the
        // non-degenerate (⍴⍤1)1 3⍴0, whose analogous non-empty-frame path
        // already correctly gives shape 1 1 (1 frame position, the same
        // 1-item chunk-result shape). See Bugs27 #28.
        //
        // Z1 is always a fresh Value from eval_LO_on_fill_chunk(), never
        // Fill_B itself, so building Z's shape from it directly (rather
        // than reshaping Fill_B or Z1 in place) is safe regardless of
        // whether LO is primitive or defined -- deriving Z's type-correct
        // prototype from Z1 via set_default(), which only needs Z1's
        // TYPE, not its actual content.
        //
        Value_P Z(shape_Z + Z1->get_shape(), LOC);
        Z->set_default(*Z1, LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   // X is checked by the final ⊃[X], see do_ALyXB()

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
        X1->next_ravel_Int(X->get_near_int(0));
        X1->check_value(LOC);
        X5->next_ravel_Pointer(X1.get());   // with X: f ⍤[X] y B
      }
   else      X5->next_ravel_Pointer(X.get());   // with X: f ⍤[X] y B

   X5->next_ravel_Int(shape_B.get_volume());    // LB
   X5->next_ravel_Pointer(vsh_B.get());         // rho_B
   X5->next_ravel_Int(shape_Z.get_volume());    // LZ
   X5->next_ravel_Pointer(vsh_Z.get());         // rho_Z
   X5->check_value(LOC);

   return Macro::get_macro(Macro::MAC_Z__LO_RANK_X5_B)->eval_LXB(_LO, *X5, *B);
}
//────────────────────────────────────────────────────────────────────────────
sRank
Bif_OPER2_RANK::y123_to_chunk_B_rank(const UCS_string & LO_name,
                                      const cValue * y123, sRank rank_B)
{
   /* y123_to_AB() splits the ranks of A and B into a (higher-dimensions)
      "frame" and a (lower-dimensions) "chunk" as specified by y123.

      Let Z ← A f ⍤ y123 B. Then y123 is the chunk ranks of Z, A, and B.

      See ISO p. 124, 125
    */

   if (!y123)
      {
        MORE_ERROR() << "(A) " << LO_name << " ⍤ y B: Missing argument y";
        VALUE_ERROR;
      }

   if ( y123->get_rank() > 1)   // RANK ERROR, not ISO's domain-error
      {
        MORE_ERROR() << "f⍤y B: y must be a scalar or vector; ⍴⍴y is "
                     << y123->get_rank();
        RANK_ERROR;
      }

   // 2. the number of elements in y determines rank_B:
   //
   //                    -- monadic f⍤ --       -- dyadic f⍤ --
   //                   rank_A     rank_B       rank_A   rank_B
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
// APL_Integer (64-bit), not sRank (int16_t): y123's items come straight
// from the user, and narrowing a huge value (e.g. 65536) directly into
// sRank at the point of assignment wraps it into a small (even
// negative) chunk rank *before* the "y3 > rank_B ? rank_B : y3" clamp
// below ever sees the true magnitude (⌽⍤65536⊢M silently used chunk
// rank 0 -- 65536 wrapped to 0 -- instead of behaving like the
// in-range ⌽⍤99⊢M). Clamp in 64 bits first, narrow only at the end.
APL_Integer y3;

   switch(y123->element_count())
      {
        case 1:  y3 = y123->get_near_int(0);    // take
                 break;

        case 2:       y123->get_near_int(0);    // check
                 y3 = y123->get_near_int(1);   // take
                 break;

        case 3:  y3 = y123->get_near_int(0);    // take
                      y123->get_near_int(1);   // check
                      y123->get_near_int(2);   // check
                      break;

        default: // ISO p. 124 (monadic) p. 125 (dyadic)
                 MORE_ERROR() << "(A) " << LO_name << " ⍤ y B: Bad length "
                              << y123->element_count()
                              << " of argument y (expecting 1, 2, or 3)";
                 LENGTH_ERROR;
      }

   // symmetric lower-bound clamp before narrowing: y6 below clamps to 0
   // for any y3 <= -rank_B anyway (y6 += rank_B stays <= 0), so this
   // preserves the exact same result while keeping y3 within sRank's
   // representable range for the narrowing cast.
   if (y3 < -APL_Integer(rank_B))   y3 = -APL_Integer(rank_B);
const sRank y5 = y3 > rank_B ? rank_B : sRank(y3);
sRank y6 = y5;
   if (y6 < 0)   y6 += rank_B;   // e.g. y123_to_AB's rank_A += rk_A
   if (y6 < 0)   y6 = 0;
   return y6;
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_OPER2_RANK::y123_to_AB(const cValue * y123, sRank & rank_A, sRank & rank_B)
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
   if ( y123->get_rank() > 1)   // RANK ERROR, not ISO's domain-error
      {
        MORE_ERROR() << "f⍤y B: y must be a scalar or vector; ⍴⍴y is "
                     << y123->get_rank();
        RANK_ERROR;
      }

   // 2. the number of elements in y determine how rank_A and rank_B
   // shall be computed:
   //
   //                    -- monadic f⍤ --       -- dyadic f⍤ --
   //                   rank_A     rank_B       rank_A   rank_B
   // ---------------------------------------------------------
   // y        :        N/A        y            y        y
   // yA yB    :        N/A        yB           yA       yB
   // yM yA yB :        N/A        yM           yA       yB
   // ---------------------------------------------------------

   // wide (APL_Integer) temporaries, not sRank (int16_t) directly: y123
   // comes straight from the user, and narrowing at the point of
   // assignment (the old code stored straight into rank_A/rank_B, both
   // sRank references) wrapped a huge value into a small/negative rank
   // *before* the clamp below ever saw its true magnitude -- the same
   // bug as get_rank_A_or_B() above, same fix.
APL_Integer wide_A, wide_B;
   switch(y123->element_count())
      {
        case 1:  wide_A = y123->get_near_int(0);
                 wide_B = wide_A;                            break;

        case 2:  wide_A = y123->get_near_int(0);
                 wide_B = y123->get_near_int(1);  break;

        case 3:           y123->get_near_int(0);
                 wide_A = y123->get_near_int(1);
                 wide_B = y123->get_near_int(2);  break;

        default:
             MORE_ERROR() << "f⍤y B: expecting ⍴y ≤ 3; ⍴y is "
                          << y123->element_count();
             LENGTH_ERROR;
      }

   // 3. adjust rank_A and rank_B if they exceed their initial value or
   // if they are negative -- clamped in 64 bits, narrowed to sRank only
   // once guaranteed to fit (|rk_A|/|rk_B| are always small).
   //
   if (wide_A > rk_A)   wide_A = rk_A;
   if (wide_A < 0)       wide_A += rk_A;
   if (wide_A < 0)       wide_A = 0;
   rank_A = sRank(wide_A);

   if (wide_B > rk_B)   wide_B = rk_B;
   if (wide_B < 0)       wide_B += rk_B;
   if (wide_B < 0)       wide_B = 0;
   rank_B = sRank(wide_B);
}
//════════════════════════════════════════════════════════════════════════════
